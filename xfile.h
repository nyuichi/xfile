#include <stddef.h>

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
} pic_file;

/* generic file constructor */
pic_file *pic_funopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *));

/* buffering */
int pic_setvbuf(pic_file *, char *, int, size_t);
int pic_fflush(pic_file *);
int pic_ffill(pic_file *);

/* resource aquisition */
pic_file *pic_fopen(const char *, const char *);
pic_file *pic_mopen(const char *, size_t, const char *);
int pic_fclose(pic_file *);

/* direct IO with buffering */
size_t pic_fread(void *, size_t, size_t, pic_file *);
size_t pic_fwrite(const void *, size_t, size_t, pic_file *);

/* indicator positioning */
long pic_fseek(pic_file *, long offset, int whence);
long pic_ftell(pic_file *);
void pic_rewind(pic_file *);

/* character IO */
int pic_fgetc(pic_file *);
int pic_ungetc(int, pic_file *);
int pic_fputc(int, pic_file *);
int pic_fputs(const char *, pic_file *);

int
pic_setvbuf(pic_file *file, char *buf, int mode, size_t bufsiz)
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
pic_fflush(pic_file *file)
{
  int r;

  r = file->vtable.write(file->vtable.cookie, file->s, file->c - file->s);
  /* TODO: error handling (r == -1 or r < file->c - file->s)*/
  file->c -= r;
  return r;
}

int
pic_ffill(pic_file *file)
{
  int r;

  r = file->vtable.read(file->vtable.cookie, file->c, file->e - file->c);
  /* TODO: error handling (r == -1) */
  if (r < file->e - file->c) {
    file->flags |= PIC_FILE_EOF;
  }
  file->c += r;
  return r;
}

pic_file *
pic_funopen(void *cookie,
            int (*read)(void *, char *, int),
            int (*write)(void *, const char *, int),
            long (*seek)(void *, long, int),
            int (*close)(void *))
{
  pic_file *file;

  file = (pic_file *)malloc(sizeof(pic_file));
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

  pic_setvbuf(file, (char *)NULL, _IOFBF, 0);

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

pic_file *
pic_fopen(const char *filename, const char *mode)
{
  FILE *fp;

  fp = fopen(filename, mode);
  if (! fp) {
    return NULL;
  }
  return pic_funopen(fp, file_read, file_write, file_seek, file_close);
}

int
pic_fclose(pic_file *file)
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
pic_fread(void *ptr, size_t block, size_t nitems, pic_file *file)
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
      if ((file->flags & PIC_FILE_EOF) != 0)
        break;
      pic_ffill(file);
    }
  }
  /* handle end-of-file */
  *dst = EOF;
  return block * nitems - size;
}

size_t
pic_fwrite(const void *ptr, size_t block, size_t nitems, pic_file *file)
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
      pic_fflush(file);
    }
    else {
      memcpy(file->c, dst, size);
      file->c += size;
      return block * nitems;
    }
  }
}

int
pic_fgetc(pic_file *file)
{
  char buf[1];

  pic_fread(buf, 1, 1, file);

  return buf[0];
}

int
pic_ungetc(int c, pic_file *file)
{
  return file->ub[file->ur++] = (char)c;
}

int
pic_fputc(int c, pic_file *file)
{
  char buf[1];

  buf[0] = c;
  pic_fwrite(buf, 1, 1, file);

  return buf[0];
}

int
pic_fputs(const char *str, pic_file *file)
{
  int len;

  len = strlen(str);
  pic_fwrite(str, len, 1, file);

  return 0;
}
