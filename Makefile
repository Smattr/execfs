FUSE_ARGS=$(shell pkg-config fuse --cflags --libs)

default: execfs

ifeq (${V},1)
Q:=
else
Q:=@
endif

ifeq (${DEBUG},1)
WERROR:=
else
WERROR:=-Werror
endif

execfs: main.o config.o
	@echo " [LD] $@"
	${Q}gcc -Wall ${WERROR} -o $@ $^ ${FUSE_ARGS}

main.o: entry.h config.h
config.o: entry.h config.h

%.o: %.c
	@echo " [CC] $@"
	${Q}gcc -Wall ${WERROR} -c -o $@ $< ${FUSE_ARGS}

clean:
	@echo " [CLEAN] execfs *.o"
	${Q}rm execfs *.o

.PHONY: default clean
