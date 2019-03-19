CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -Wall -Wextra -O2 -s
LOADLIBES = -ltinfo

xcd: xcd.o
xcd.o: xcd.c

clean:
	rm -f xcd xcd.o
