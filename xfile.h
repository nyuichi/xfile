#ifndef XFILE_H__
#define XFILE_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct xFILE {
  /* buffered IO */
  char *buf;
  char ownbuf;
  int mode;
  int bufsiz;
  char *s, *c, *e;
  /* operators */
  struct {
    void *cookie;
    int (*read)(void *, char *, int);
    int (*write)(void *, const char *, int);
    long (*seek)(void *, long, int);
    int (*close)(void *);
  } vtable;
} xFILE;

/* generic file constructor */
xFILE *xfunopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *));

/* buffering */
int xsetvbuf(xFILE *, char *, int, size_t);
int xfflush(xFILE *);
int xffill(xFILE *);

/* resource aquisition */
xFILE *xfopen(const char *, const char *);
xFILE *xfpopen(FILE *);       /* disables xfile's buffer management */
xFILE *xmopen();
int xfclose(xFILE *);

/* direct IO with buffering */
size_t xfread(void *, size_t, size_t, xFILE *);
size_t xfwrite(const void *, size_t, size_t, xFILE *);

/* indicator positioning */
long xfseek(xFILE *, long offset, int whence);
long xftell(xFILE *);
void xrewind(xFILE *);

/* character IO */
int xfgetc(xFILE *);
char *xfgets(char *, int, xFILE *);
int xfputc(int, xFILE *);
int xfputs(const char *, xFILE *);
char xgetc(xFILE *);
char xgetchar(void);
/* char *xgets(char *); */
int xputc(int, xFILE *);
int xputchar(int);
int xputs(char *);

/* formatted I/O */
int xprintf(const char *, ...);
int xfprintf(xFILE *, const char *, ...);
int xvfprintf(xFILE *, const char *, va_list);

/* standard I/O */
xFILE *xstdin_();
xFILE *xstdout_();
xFILE *xstderr_();
#define xstdin	(xstdin_())
#define xstdout	(xstdout_())
#define xstderr	(xstderr_())

#if defined(__cplusplus)
}
#endif

#endif
