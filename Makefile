# Campus Navigation System (shortest paths on graphs) -- C11
# ASCII-only on purpose: builds on Windows (mingw32-make) and Linux (CI).

CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic \
           -finput-charset=UTF-8 -fexec-charset=UTF-8 -Iinclude
LDLIBS  ?= -lm

CORE = src/campus.c src/graph.c src/algo.c src/app.c src/ui.c \
       src/platform.c src/crc32.c src/bench.c src/default_campus.c
HDRS = include/nav.h include/campus.h include/graph.h include/algo.h \
       include/app.h include/ui.h include/platform.h include/crc32.h \
       include/bench.h include/err.h

.PHONY: all test e2e clean

all: build/nav

build/nav: $(CORE) src/main.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -Werror $(CORE) src/main.c -o $@ $(LDLIBS)

build/tests: $(CORE) tests/test_main.c $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) -Werror $(CORE) tests/test_main.c -o $@ $(LDLIBS)

test: build/tests
	./build/tests

e2e: build/nav
	bash scripts/e2e.sh

clean:
	rm -rf build
