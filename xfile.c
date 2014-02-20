#include "xfile.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if XFILE_ENABLE_POSIX
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

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
xfunopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *))
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

int
xsetvbuf(XFILE *file, char *buf, int mode, size_t bufsiz)
{
  /* FIXME: free old buf */
  assert(mode != _IONBF);       /* FIXME: support IONBF */
  assert(mode != _IOLBF);       /* FIXME: support IOLBF */

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
      assert(bufsiz == 0);      /* TODO allow custom buffer size? */
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

  while ((r = file->vtable.write(file->vtable.cookie, file->s, file->c - file->s)) > 0) {
    file->c -= r;
  }
  return r;                     /* 0 on success, -1 on error */
}

int
xffill(XFILE *file)
{
  int r;

  while ((r = file->vtable.read(file->vtable.cookie, file->c, file->e - file->c)) > 0) {
    file->c += r;
  }
  return r;                     /* 0 on success, -1 on error */
}

#if XFILE_ENABLE_CSTDIO

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

static XFILE *
file_open(FILE *fp)
{
  return xfunopen(fp, file_read, file_write, file_seek, file_close);
}

XFILE *
xfopen(const char *filename, const char *mode)
{
  FILE *fp;

  fp = fopen(filename, mode);
  if (! fp) {
    return NULL;
  }
  return file_open(fp);
}

#endif

#if XFILE_ENABLE_POSIX

static int
fd_read(void *cookie, char *ptr, int size)
{
  return read((int)cookie, ptr, size);
}

static int
fd_write(void *cookie, const char *ptr, int size)
{
  return write((int)cookie, ptr, size);
}

static long
fd_seek(void *cookie, long pos, int whence)
{
  return lseek((int)cookie, pos, whence);
}

static int
fd_close(void *cookie)
{
  return close((int)cookie);
}

static XFILE *
fd_open(int fd)
{
  return xfunopen((void *)(long)fd, fd_read, fd_write, fd_seek, fd_close);
}

XFILE *
xfopen(const char *filename, const char *mode)
{
  int fd, flags = 0;

  switch (*mode++) {
  case 'r':
    if (*mode == '+') {
      flags = O_RDWR;
    } else {
      flags = O_RDONLY;
    }
    break;
  case 'w':
    if (*mode == '+') {
      flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else {
      flags = O_RDWR | O_CREAT | O_TRUNC;
    }
    break;
  case 'a':
    if (*mode == '+') {
      flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
      flags = O_RDWR | O_CREAT | O_APPEND;
    }
    break;
  }

  fd = open(filename, flags);
  if (fd == -1) {
    return NULL;
  }
  return fd_open(fd);
}

#endif

#if XFILE_STDX_TYPE != 0

static XFILE *xfile_stdinp__;
static XFILE *xfile_stdoutp__;
static XFILE *xfile_stderrp__;

# if XFILE_STDX_TYPE == 1
XFILE *
xstdin_()
{
  return xfile_stdinp__ ? xfile_stdinp__ : (xfile_stdinp__ = file_open(stdin));
}

XFILE *
xstdout_()
{
  return xfile_stdoutp__ ? xfile_stdoutp__ : (xfile_stdoutp__ = file_open(stdout));
}

XFILE *
xstderr_()
{
  return xfile_stderrp__ ? xfile_stderrp__ : (xfile_stderrp__ = file_open(stderr));
}
# elif XFILE_STDX_TYPE == 2
XFILE *
xstdin_()
{
  return xfile_stdinp__ ? xfile_stdinp__ : (xfile_stdinp__ = fd_open(0));
}

XFILE *
xstdout_()
{
  return xfile_stdoutp__ ? xfile_stdoutp__ : (xfile_stdoutp__ = fd_open(1));
}

XFILE *
xstderr_()
{
  return xfile_stderrp__ ? xfile_stderrp__ : (xfile_stderrp__ = fd_open(2));
}
# endif

#endif

int
xfclose(XFILE *file)
{
  int r;

  xfflush(file);

  r = file->vtable.close(file->vtable.cookie);
  if (r == EOF) {
    return -1;
  }

  /* unchain */
  if (xfile_chain_ptr__ == file) {
    xfile_chain_ptr__ = xfile_chain_ptr__->next;
  }
  else {
    XFILE *chain;

    chain = xfile_chain_ptr__;
    while (chain->next != file) {
      chain = chain->next;
    }
    chain->next = file->next;
  }
  free(file);
  return 0;
}

size_t
xfread(void *ptr, size_t block, size_t nitems, XFILE *file)
{
  int size, avail, eof = 0;
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
      if (eof) {
        *dst = EOF;
        return block * nitems - size;
      }
      xffill(file);
      if (file->c - file->s == 0)
        eof = 1;
    }
  }
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
      xfflush(file);            /* TODO error handling */
    }
    else {
      memcpy(file->c, dst, size);
      file->c += size;
      return block * nitems;
    }
  }
}

long
xfseek(XFILE *file, long offset, int whence)
{
  return file->vtable.seek(file->vtable.cookie, offset, whence);
}

long
xftell(XFILE *file)
{
  return file->vtable.seek(file->vtable.cookie, 0, SEEK_CUR);
}

void
xrewind(XFILE *file)
{
  file->vtable.seek(file->vtable.cookie, 0, SEEK_SET);
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

#if XFILE_STDX_TYPE != 0
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
#endif

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

/* FIXME! */
#include <stdio.h>

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
      case 'c':
        n += snprintf(buf, 1024, "%c", va_arg(ap, int));
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
      case 'p':
        n += snprintf(buf, 1024, "%p", va_arg(ap, void *));
        xfputs(buf, stream);
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

struct membuf {
  char *buf;
  long pos, end, capa;
};

#define min(a,b) (((a)>(b))?(b):(a))
#define max(a,b) (((a)<(b))?(b):(a))

static int
mem_read(void *cookie, char *ptr, int size)
{
  struct membuf *mem;

  mem = (struct membuf *)cookie;

  size = min(size, mem->end - mem->pos);
  memcpy(ptr, mem->buf, size);
  mem->pos += size;
  return size;
}

static int
mem_write(void *cookie, const char *ptr, int size)
{
  struct membuf *mem;

  mem = (struct membuf *)cookie;

  if (mem->pos + size > mem->capa) {
    mem->capa = (mem->pos + size) * 2;
    mem->buf = realloc(mem->buf, mem->capa);
  }
  memcpy(mem->buf + mem->pos, ptr, size);
  mem->pos += size;
  mem->end = max(mem->pos, mem->end);
  return size;
}

static long
mem_seek(void *cookie, long pos, int whence)
{
  struct membuf *mem;

  mem = (struct membuf *)cookie;

  switch (whence) {
  case SEEK_SET:
    mem->pos = pos;
    break;
  case SEEK_CUR:
    mem->pos += pos;
    break;
  case SEEK_END:
    mem->pos = mem->end + pos;
    break;
  }

  return mem->pos;
}

static int
mem_close(void *cookie)
{
  struct membuf *mem;

  mem = (struct membuf *)cookie;
  free(mem->buf);
  free(mem);
  return 0;
}

XFILE *
xmopen()
{
  struct membuf *mem;

  mem = (struct membuf *)malloc(sizeof(struct membuf));
  mem->buf = (char *)malloc(BUFSIZ);
  mem->pos = 0;
  mem->end = 0;
  mem->capa = BUFSIZ;

  return xfunopen(mem, mem_read, mem_write, mem_seek, mem_close);
}
