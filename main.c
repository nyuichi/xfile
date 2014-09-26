#include "xfile.h"
#include <stdio.h>
#include <assert.h>

void
test_memio(void)
{
  xFILE *mem;
  int c;

  mem = xmopen();

  /* output */
  xfprintf(mem, "%d %s %c\n", 42, "hello", 'A');

  xrewind(mem);

  /* input */
  while ((c = xfgetc(mem)) != -1) {
    xputchar(c);
  }
  puts(" equals ?: \n42 hello A\n");

  assert(xfclose(mem) == 0);
}

int
main()
{
  xprintf("%d %d %d\n", 1, 42, -100);

  test_memio();
}
