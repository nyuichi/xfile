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

typedef struct XFILE {
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
  struct XFILE *next;
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
#if XFILE_FOPEN_TYPE != 0
XFILE *xfopen(const char *, const char *);
#endif
#if XFILE_ENABLE_CSTDIO
XFILE *xfpopen(FILE *);
#endif
#if XFILE_ENABLE_POSIX
XFILE *xfdopen(int);
#endif
XFILE *xmopen();
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
char *xfgets(char *, int, XFILE *);
int xfputc(int, XFILE *);
int xfputs(const char *, XFILE *);
char xgetc(XFILE *);
char xgetchar(void);
/* char *xgets(char *); */
int xputc(int, XFILE *);
int xputchar(int);
int xputs(char *);
int xungetc(int, XFILE *);

/* formatted I/O */
int xprintf(const char *, ...);
int xfprintf(XFILE *, const char *, ...);
int xvfprintf(XFILE *, const char *, va_list);

#if XFILE_STDX_TYPE != 0

/* standard I/O */
XFILE *xstdin_();
XFILE *xstdout_();
XFILE *xstderr_();
#define xstdin	(xstdin_())
#define xstdout	(xstdout_())
#define xstderr	(xstderr_())

#endif

#if defined(__cplusplus)
}
#endif

#endif
