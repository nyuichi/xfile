#ifndef XFILE_H__
#define XFILE_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include <stddef.h>
#include <stdarg.h>

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
XFILE *xfopen(const char *, const char *);
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

/* standard I/O */
XFILE *xstdin_();
XFILE *xstdout_();
XFILE *xstderr_();

#define xstdin	(xstdin_())
#define xstdout	(xstdout_())
#define xstderr	(xstderr_())

#if defined(__cplusplus)
}
#endif

#endif
