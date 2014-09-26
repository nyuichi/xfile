all: test

test:
	clang -g -Weverything -Wno-vla -Wno-format-nonliteral -Wno-missing-prototypes -Werror main.c
	./a.out
