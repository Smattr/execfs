FUSE_ARGS=$(shell pkg-config fuse --cflags --libs 2>/dev/null)
ifeq (${FUSE_ARGS},)
$(error libfuse-dev not found)
endif

default: execfs

INIPARSER=iniparser/src

CFLAGS:=-Wall -I${INIPARSER}

# Version info. Set this here or via the command line for a release. Otherwise
# you just get the git commit ID.
ifndef VERSION
VERSION="git-$(shell git rev-parse HEAD)"
endif
CFLAGS+=-DVERSION=\"${VERSION}\"

ifeq (${V},1)
Q:=
else
Q:=@
endif

# By default, build an executable for debugging.
ifndef DEBUG
DEBUG=1
endif
ifeq (${DEBUG},1)
CFLAGS+=-g -ggdb
else
CFLAGS+=-Werror -DNDEBUG
endif

### EXECFS TARGETS ###

execfs: main.o config.o fileops.o impl.o log.o pipes.o ${INIPARSER}/iniparser.o ${INIPARSER}/dictionary.o
	@echo " [LD] $@"
	${Q}gcc ${CFLAGS} -o $@ $^ ${FUSE_ARGS}
	$(if $(filter 0,${DEBUG}),@echo " [STRIP] $@",)
	$(if $(filter 0,${DEBUG}),${Q}strip $@,)

main.o: entry.h config.h fileops.h log.h globals.h
config.o: entry.h config.h macros.h
fileops.o: assert.h entry.h fileops.h globals.h impl.h log.h macros.h
impl.o: entry.h fuse.h pipes.h
log.o: globals.h log.h
pipes.o: pipes.h

%.o: %.c
	@echo " [CC] $@"
	${Q}gcc ${CFLAGS} -c -o $@ $< ${FUSE_ARGS}

### INI parser dependencies ###
config.o: ${INIPARSER}/iniparser.h ${INIPARSER}/dictionary.h
${INIPARSER}/%.o: ${INIPARSER}/%.c ${INIPARSER}/%.h

${INIPARSER}/%.c ${INIPARSER}/%.h:
	${Q}which git >/dev/null
	${Q}git submodule init
	${Q}git submodule update

### TOOLS TARGETS ###

open: tools/open.o
	@echo " [LD] $@"
	${Q}gcc ${CFLAGS} -o $@ $^

block: tools/block.o
	@echo " [LD] $@"
	${Q}gcc ${CFLAGS} -o $@ $^

### TEST TARGETS ###

.PHONY: tests
TESTS=$(patsubst tests/%.config,%,$(wildcard tests/test-*.config))
tests: ${TESTS}

test-%: tests/test-%.sh tests/test-%.config execfs tests/test.sh
	@echo " [TEST] $@"
	${Q}PATH=.:${PATH} ./tests/test.sh $< $(word 2,$^)

.PHONY: default clean
clean:
	@echo " [CLEAN] execfs open block *.o"
	${Q}rm -f execfs open block *.o
