#ifndef XFILE_H__
#define XFILE_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>

/* configurations */
#ifndef XFILE_ENABLE_CSTDIO
# define XFILE_ENABLE_CSTDIO 1
#endif
#ifndef XFILE_ENABLE_POSIX
# define XFILE_ENABLE_POSIX 0
#endif
#ifndef XFILE_STDX_TYPE
# define XFILE_STDX_TYPE 1      /* 0: no, 1: cstdio, 2: posix */
#endif
#ifndef XFILE_FOPEN_TYPE
# define XFILE_FOPEN_TYPE 1     /* 0: no, 1: cstdio, 2: posix */
#endif

#if XFILE_ENABLE_CSTDIO
# include <stdio.h>
#else
# if !defined(_IONBF)
#  define _IOFBF 0
#  define _IOLBF 1
#  define _IONBF 2
#  define BUFSIZ 1024
# endif
#endif

#ifndef EOF
# define EOF (-1)
#endif

typedef struct xFILE {
  /* buffered IO */
  char *buf;
  char ownbuf;
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
  struct xFILE *next;
} xFILE;

/* generic file constructor */
xFILE *xfunopen(void *cookie, int (*read)(void *, char *, int), int (*write)(void *, const char *, int), long (*seek)(void *, long, int), int (*close)(void *));

/* buffering */
int xsetvbuf(xFILE *, char *, int, size_t);
int xfflush(xFILE *);
int xffill(xFILE *);

/* resource aquisition */
#if XFILE_FOPEN_TYPE != 0
xFILE *xfopen(const char *, const char *);
#endif
#if XFILE_ENABLE_CSTDIO
xFILE *xfpopen(FILE *);
#endif
#if XFILE_ENABLE_POSIX
xFILE *xfdopen(int);
#endif
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
int xungetc(int, xFILE *);

/* formatted I/O */
int xprintf(const char *, ...);
int xfprintf(xFILE *, const char *, ...);
int xvfprintf(xFILE *, const char *, va_list);

#if XFILE_STDX_TYPE != 0

/* standard I/O */
xFILE *xstdin_();
xFILE *xstdout_();
xFILE *xstderr_();
#define xstdin	(xstdin_())
#define xstdout	(xstdout_())
#define xstderr	(xstderr_())

#endif

#if defined(__cplusplus)
}
#endif

#endif
