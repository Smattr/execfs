FUSE_ARGS=$(shell pkg-config fuse --cflags --libs)

default: execfs

execfs: execfs.c
	gcc -Wall -Werror -o $@ $< ${FUSE_ARGS}

clean:
	rm execfs

.PHONY: default clean
