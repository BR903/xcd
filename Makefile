CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -Wall -Wextra -O2 -s
LOADLIBES = -ltinfo
PREFIX = /usr/local

xcd: xcd.o
xcd.o: xcd.c

clean:
	rm -f xcd xcd.o

install:
	cp xcd $(PREFIX)/bin/
	cp xcd.1 $(PREFIX)/share/man/man1/
