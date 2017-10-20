CC=gcc
CFLAGS=-Wall -Werror
LDFLAGS=-lconfig

ifdef DEBUG
CFLAGS += -g -O0
endif

mqclient: mqclient.c

test: mqclient
	./tests.sh

clean:
	rm -f mqclient
