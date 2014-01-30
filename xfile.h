#ifndef XFILE_H__
#define XFILE_H__

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  short flags;
  /* buffered IO */
  char *buf;
  int mode;
  int bufsiz;
  char *s, *c, *e;
  /* ungetc buf */
  char ub[3];
  int us;
  int ur;
  /* operators */
  struct {
    void *cookie;
    int (*read)(void *, char *, int);
    int (*write)(void *, const char *, int);
    long (*seek)(void *, long, int);
    int (*close)(void *);
  } vtable;
} XFILE;

enum {
  XFILE_EOF = 1,
};

/* generic file constructor */
XFILE *xfunopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *));

/* buffering */
int xsetvbuf(XFILE *, char *, int, size_t);
int xfflush(XFILE *);
int xffill(XFILE *);

/* resource aquisition */
XFILE *xfopen(const char *, const char *);
XFILE *xmopen(const char *, size_t, const char *);
int xfclose(XFILE *);

/* direct IO with buffering */
size_t xfread(void *, size_t, size_t, XFILE *);
size_t xfwrite(const void *, size_t, size_t, XFILE *);

/* indicator positioning */
long xfseek(XFILE *, long offset, int whence);
long xftell(XFILE *);
void xrewind(XFILE *);

/* character IO */
int xfgetc(XFILE *);
int xungetc(int, XFILE *);
int xfputc(int, XFILE *);
int xfputs(const char *, XFILE *);

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

  xsetvbuf(file, (char *)NULL, _IOFBF, 0);

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

int
xfclose(XFILE *file)
{
  int r;

  r = file->vtable.close(file->vtable.cookie);
  if (! r) {
    return -1;
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

#endif
