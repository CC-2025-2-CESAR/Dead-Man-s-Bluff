CC = cc
CPPFLAGS += -Isrc/poker
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
POKER_SRC = src/poker/poker.c src/poker/apostas.c
POKER_HEADERS = src/poker/poker.h src/poker/apostas.h
JOGO_SRC = src/main.c src/core/jogo.c src/ui/interface.c
JOGO_HEADERS = src/core/jogo.h src/ui/interface.h
TORNEIO_SRC = src/core/torneio.c
TORNEIO_HEADERS = src/core/torneio.h
DECK_SRC = src/cards/deck.c
DECK_HEADERS = src/cards/deck.h
DECK_TESTS = build/test_deck build/test_edge_cases build/test_integration
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

.PHONY: all test console-demo raylib-demo jogo clean
all: test console-demo

build:
	mkdir -p $@

build/test_poker: tests/test_poker.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_poker.c $(POKER_SRC) $(LDLIBS) -o $@

build/test_torneio: tests/test_torneio.c $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) tests/test_torneio.c $(TORNEIO_SRC) $(POKER_SRC) $(LDLIBS) -o $@

$(DECK_TESTS): build/%: tests/%.c $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/$*.c $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -o $@

test: build/test_poker build/test_torneio $(DECK_TESTS)
	./build/test_poker
	./build/test_torneio
	./build/test_deck
	./build/test_edge_cases
	./build/test_integration

build/console_demo: examples/console_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) examples/console_demo.c $(POKER_SRC) $(LDLIBS) -o $@

console-demo: build/console_demo

build/raylib_demo: examples/raylib_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) examples/raylib_demo.c $(POKER_SRC) $(RAYLIB_LIBS) $(LDLIBS) -o $@

raylib-demo: build/raylib_demo

build/dead_mans_bluff: $(JOGO_SRC) $(JOGO_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) $(JOGO_SRC) $(RAYLIB_LIBS) $(LDLIBS) -o $@

jogo: build/dead_mans_bluff

clean:
	rm -rf build
