#include "xfile.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define XF_EOF 1
#define XF_ERR 2

xFILE *
xfunopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *))
{
  xFILE *file;

  file = (xFILE *)malloc(sizeof(xFILE));
  if (! file) {
    return NULL;
  }
  file->flags = 0;
  /* set vtable */
  file->vtable.cookie = cookie;
  file->vtable.read = read;
  file->vtable.write = write;
  file->vtable.seek = seek;
  file->vtable.close = close;

  return file;
}

static int
file_read(void *cookie, char *ptr, int size)
{
  int r;

  r = fread(ptr, 1, size, (FILE *)cookie);
  if (r < size && ferror((FILE *)cookie)) {
    return -1;
  }
  if (r == 0 && feof((FILE *)cookie)) {
    clearerr((FILE *)cookie);
  }
  return r;
}

static int
file_write(void *cookie, const char *ptr, int size)
{
  int r;

  r = fwrite(ptr, 1, size, (FILE *)cookie);
  if (r < size) {
    return -1;
  }
  return r;
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

xFILE *
xfpopen(FILE *fp)
{
  xFILE *file;

  file = xfunopen(fp, file_read, file_write, file_seek, file_close);
  if (! file) {
    return NULL;
  }

  return file;
}

xFILE *
xfopen(const char *filename, const char *mode)
{
  FILE *fp;
  xFILE *file;

  fp = fopen(filename, mode);
  if (! fp) {
    return NULL;
  }

  file = xfpopen(fp);
  if (! file) {
    return NULL;
  }

  return file;
}

static xFILE *xfile_stdinp__;
static xFILE *xfile_stdoutp__;
static xFILE *xfile_stderrp__;

xFILE *
xstdin_()
{
  return xfile_stdinp__ ? xfile_stdinp__ : (xfile_stdinp__ = xfpopen(stdin));
}

xFILE *
xstdout_()
{
  return xfile_stdoutp__ ? xfile_stdoutp__ : (xfile_stdoutp__ = xfpopen(stdout));
}

xFILE *
xstderr_()
{
  return xfile_stderrp__ ? xfile_stderrp__ : (xfile_stderrp__ = xfpopen(stderr));
}

int
xfclose(xFILE *file)
{
  int r;

  r = file->vtable.close(file->vtable.cookie);
  if (r == EOF) {
    return -1;
  }

  free(file);
  return 0;
}

size_t
xfread(void *ptr, size_t block, size_t nitems, xFILE *file)
{
  char *dst = (char *)ptr;
  char buf[block];
  size_t i;
  int n;

  for (i = 0; i < nitems; ++i) {
    n = file->vtable.read(file->vtable.cookie, buf, block);
    if (n < 0) {
      file->flags |= XF_ERR;
      break;
    }
    if (n == 0) {
      file->flags |= XF_EOF;
      break;
    }
    memcpy(dst, buf, n);
    dst += n;
  }

  return i;
}

size_t
xfwrite(const void *ptr, size_t block, size_t nitems, xFILE *file)
{
  char *dst = (char *)ptr;
  size_t i;
  int n;

  for (i = 0; i < nitems; ++i) {
    n = file->vtable.write(file->vtable.cookie, dst, block);
    if (n < 0) {
      file->flags |= XF_ERR;
      break;
    }
    dst += n;
  }

  return i;
}

long
xfseek(xFILE *file, long offset, int whence)
{
  return file->vtable.seek(file->vtable.cookie, offset, whence);
}

long
xftell(xFILE *file)
{
  return file->vtable.seek(file->vtable.cookie, 0, SEEK_CUR);
}

void
xrewind(xFILE *file)
{
  file->vtable.seek(file->vtable.cookie, 0, SEEK_SET);
}

int
xfgetc(xFILE *file)
{
  char buf[1];

  xfread(buf, 1, 1, file);

  return buf[0];
}

int
xfputc(int c, xFILE *file)
{
  char buf[1];

  buf[0] = c;
  xfwrite(buf, 1, 1, file);

  return buf[0];
}

int
xfputs(const char *str, xFILE *file)
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
xfprintf(xFILE *stream, const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = xvfprintf(stream, fmt, ap);
  va_end(ap);
  return n;
}

int
xvfprintf(xFILE *stream, const char *fmt, va_list ap)
{
  char buf[vsnprintf(NULL, 0, fmt, ap) + 1];

  vsnprintf(buf, sizeof buf, fmt, ap);

  if (xfwrite(buf, sizeof buf, 1, stream) < 1) {
    return -1;
  }

  return sizeof buf;
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

xFILE *
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
