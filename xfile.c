#include "xfile.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
xsetvbuf(XFILE *file, char *buf, int mode, size_t bufsiz)
{
  /* FIXME: free old buf */
  assert(mode != _IONBF);       /* FIXME: support IONBF */

  file->mode = mode;
  if (buf) {
    file->buf = buf;
    file->bufsiz = bufsiz;
  }
  else {
    if (mode == _IONBF) {
      file->buf = NULL;
      file->bufsiz = 0;
    }
    else {
      assert(bufsiz == 0);
      file->buf = (char *)malloc(BUFSIZ);
      file->bufsiz = BUFSIZ;
    }
  }
  file->s = file->c = file->buf;
  file->e = file->buf + file->bufsiz;
  return 0;
}

int
xfflush(XFILE *file)
{
  int r;

  r = file->vtable.write(file->vtable.cookie, file->s, file->c - file->s);
  /* TODO: error handling (r == -1 or r < file->c - file->s)*/
  file->c -= r;
  return r;
}

int
xffill(XFILE *file)
{
  int r;

  r = file->vtable.read(file->vtable.cookie, file->c, file->e - file->c);
  /* TODO: error handling (r == -1) */
  if (r < file->e - file->c) {
    file->flags |= XFILE_EOF;
  }
  file->c += r;
  return r;
}

static char xfile_atexit_registered__;
static XFILE *xfile_chain_ptr__;

static void
xfile_atexit()
{
  while (xfile_chain_ptr__) {
    xfclose(xfile_chain_ptr__);
  }
}

XFILE *
xfunopen(void *cookie,
         int (*read)(void *, char *, int),
         int (*write)(void *, const char *, int),
         long (*seek)(void *, long, int),
         int (*close)(void *))
{
  XFILE *file;

  file = (XFILE *)malloc(sizeof(XFILE));
  if (! file) {
    return NULL;
  }
  file->flags = 0;
  /* no buffering at the beginning */
  file->buf = NULL;
  file->mode = _IONBF;
  file->bufsiz = 0;
  file->us = 3;
  file->ur = 0;
  /* set vtable */
  file->vtable.cookie = cookie;
  file->vtable.read = read;
  file->vtable.write = write;
  file->vtable.seek = seek;
  file->vtable.close = close;
  /* chain */
  file->next = xfile_chain_ptr__;
  xfile_chain_ptr__ = file;

  xsetvbuf(file, (char *)NULL, _IOFBF, 0);

  if (! xfile_atexit_registered__) {
    xfile_atexit_registered__ = 1;
    atexit(xfile_atexit);
  }

  return file;
}

static int
file_read(void *cookie, char *ptr, int size)
{
  return fread(ptr, 1, size, (FILE *)cookie);
}

static int
file_write(void *cookie, const char *ptr, int size)
{
  return fwrite(ptr, 1, size, (FILE *)cookie);
}

static long
file_seek(void *cookie, long pos, int whence)
{
  return fseek((FILE *)cookie, pos, whence);
}

static int
file_close(void *cookie)
{
  return fclose((FILE *)cookie);
}

XFILE *
xfopen(const char *filename, const char *mode)
{
  FILE *fp;

  fp = fopen(filename, mode);
  if (! fp) {
    return NULL;
  }
  return xfunopen(fp, file_read, file_write, file_seek, file_close);
}

static XFILE *xfile_stdinp__;
static XFILE *xfile_stdoutp__;
static XFILE *xfile_stderrp__;

XFILE *
xstdin_()
{
  return xfile_stdinp__
    ? xfile_stdinp__
    : (xfile_stdinp__ = xfunopen(stdin, file_read, file_write, file_seek, file_close));
}

XFILE *
xstdout_()
{
  return xfile_stdoutp__
    ? xfile_stdoutp__
    : (xfile_stdoutp__ = xfunopen(stdout, file_read, file_write, file_seek, file_close));
}

XFILE *
xstderr_()
{
  return xfile_stderrp__
    ? xfile_stderrp__
    : (xfile_stderrp__ = xfunopen(stderr, file_read, file_write, file_seek, file_close));
}

int
xfclose(XFILE *file)
{
  XFILE *chain;
  int r;

  xfflush(file);

  r = file->vtable.close(file->vtable.cookie);
  if (! r) {
    return -1;
  }

  /* unchain */
  if (xfile_chain_ptr__ == file) {
    xfile_chain_ptr__ = xfile_chain_ptr__->next;
  }
  else {
    for (chain = xfile_chain_ptr__; ; chain = chain->next) {
      if (chain->next == file) {
        chain->next = file->next;
        break;
      }
    }
  }
  free(file);
  return 0;
}

size_t
xfread(void *ptr, size_t block, size_t nitems, XFILE *file)
{
  int size, avail;
  char *dst = (char *)ptr;

  size = block * nitems;        /* TODO: optimize block read */

  /* take care of ungetc buf */
  while (file->ur > 0) {
    *dst++ = file->ub[--file->ur];
    if (--size == 0)
      return block * nitems;
  }

  while (1) {
    avail = file->c - file->s;
    if (size <= avail) {
      memcpy(dst, file->s, size);
      memmove(file->s, file->s + size, avail - size);
      file->c -= size;
      return block * nitems;
    }
    else {
      memcpy(dst, file->s, avail);
      file->c = file->s;
      size -= avail;
      dst += avail;
      if ((file->flags & XFILE_EOF) != 0)
        break;
      xffill(file);
    }
  }
  /* handle end-of-file */
  *dst = EOF;
  return block * nitems - size;
}

size_t
xfwrite(const void *ptr, size_t block, size_t nitems, XFILE *file)
{
  int size, room;
  char *dst = (char *)ptr;

  size = block * nitems;               /* TODO: optimize block write */
  while (1) {
    room = file->e - file->c;
    if (room < size) {
      memcpy(file->c, dst, room);
      file->c = file->e;
      size -= room;
      dst += room;
      xfflush(file);
    }
    else {
      memcpy(file->c, dst, size);
      file->c += size;
      return block * nitems;
    }
  }
}

int
xfgetc(XFILE *file)
{
  char buf[1];

  xfread(buf, 1, 1, file);

  return buf[0];
}

int
xungetc(int c, XFILE *file)
{
  return file->ub[file->ur++] = (char)c;
}

int
xfputc(int c, XFILE *file)
{
  char buf[1];

  buf[0] = c;
  xfwrite(buf, 1, 1, file);

  return buf[0];
}

int
xfputs(const char *str, XFILE *file)
{
  int len;

  len = strlen(str);
  xfwrite(str, len, 1, file);

  return 0;
}

int
xprintf(const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = xvfprintf(xstdout, fmt, ap);
  va_end(ap);
  return n;
}

int
xfprintf(XFILE *stream, const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = xvfprintf(stream, fmt, ap);
  va_end(ap);
  return n;
}

int
xvfprintf(XFILE *stream, const char *fmt, va_list ap)
{
  static char buf[1024 + 1];
  int n = 0, k;
  char c, *str;

  while ((c = *fmt++)) {
    if (c == '%') {
      if (! (c = *fmt++))
        break;
      switch (c) {
      case '%':
        xfputs("%", stream);
        n++;
        break;
      case 'd':
        n += snprintf(buf, 1024, "%d", va_arg(ap, int));
        xfputs(buf, stream);
        break;
      case 'f':
        n += snprintf(buf, 1024, "%f", va_arg(ap, double));
        xfputs(buf, stream);
        break;
      case 's':
        str = va_arg(ap, char *);
        do {
          k = snprintf(buf, 1024, "%s", str);
          xfputs(buf, stream);
          n += k;
        } while (k >= 1024);
        break;
      }
    }
    else {
      xfputc(c, stream);
      n++;
    }
  }
  return n;
}
