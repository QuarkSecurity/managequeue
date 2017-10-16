CC=gcc
CFLAGS=-Werror
LDFLAGS=-lsystemd -lconfig

managequeue: managequeue.c

test: managequeue
	./tests.sh

clean:
	rm -f managequeue
