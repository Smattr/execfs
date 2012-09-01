FUSE_ARGS=$(shell pkg-config fuse --cflags --libs)

default: execfs

execfs: main.o config.o
	gcc -Wall -o $@ $^ ${FUSE_ARGS}

main.o: entry.h config.h
config.o: entry.h config.h

%.o: %.c
	gcc -Wall -c -o $@ $< ${FUSE_ARGS}

clean:
	rm execfs
	rm *.o

.PHONY: default clean
