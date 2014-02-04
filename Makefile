all: test

test:
	cc -g main.c xfile.c
	./a.out
