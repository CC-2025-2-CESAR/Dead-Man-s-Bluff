CC = cc
CPPFLAGS += -Isrc/poker
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
POKER_SRC = src/poker/poker.c src/poker/apostas.c
POKER_HEADERS = src/poker/poker.h src/poker/apostas.h
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

.PHONY: all test console-demo raylib-demo clean
all: test console-demo

build:
	mkdir -p $@

build/test_poker: tests/test_poker.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_poker.c $(POKER_SRC) $(LDLIBS) -o $@

test: build/test_poker
	./build/test_poker

build/console_demo: examples/console_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) examples/console_demo.c $(POKER_SRC) $(LDLIBS) -o $@

console-demo: build/console_demo

build/raylib_demo: examples/raylib_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) examples/raylib_demo.c $(POKER_SRC) $(RAYLIB_LIBS) $(LDLIBS) -o $@

raylib-demo: build/raylib_demo

clean:
	rm -rf build
