CC = cc
CPPFLAGS += -Isrc/poker
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
POKER_SRC = src/poker/poker.c src/poker/apostas.c
POKER_HEADERS = src/poker/poker.h src/poker/apostas.h
JOGO_SRC = src/main.c src/core/jogo.c src/ui/interface.c $(PARTIDA_SRC) $(MAO_IA_SRC) $(MAO_SRC) $(AI_SRC) $(TORNEIO_SRC) $(DECK_SRC) $(POKER_SRC)
JOGO_HEADERS = src/core/jogo.h src/ui/interface.h $(PARTIDA_HEADERS) $(MAO_IA_HEADERS) $(MAO_HEADERS) $(AI_HEADERS) $(TORNEIO_HEADERS) $(DECK_HEADERS) $(POKER_HEADERS)
TORNEIO_SRC = src/core/torneio.c
TORNEIO_HEADERS = src/core/torneio.h
EVENTOS_SRC = src/core/eventos.c
EVENTOS_HEADERS = src/core/eventos.h
LISTA_CARTAS_SRC = src/cards/lista_cartas.c
LISTA_CARTAS_HEADERS = src/cards/lista_cartas.h
MAO_SRC = src/core/mao.c $(EVENTOS_SRC) $(LISTA_CARTAS_SRC)
MAO_HEADERS = src/core/mao.h $(EVENTOS_HEADERS) $(LISTA_CARTAS_HEADERS)
MAO_IA_SRC = src/core/mao_ia.c
MAO_IA_HEADERS = src/core/mao_ia.h
PARTIDA_SRC = src/core/partida.c
PARTIDA_HEADERS = src/core/partida.h
AI_SRC = src/gameplay/poker_ai.c src/gameplay/poker_ai_config.c
AI_HEADERS = src/gameplay/poker_ai.h src/gameplay/poker_ai_config.h
AI_CFLAGS = $(CFLAGS) -O2
DECK_SRC = src/cards/deck.c
DECK_HEADERS = src/cards/deck.h
DECK_TESTS = build/test_deck build/test_edge_cases build/test_integration
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

.PHONY: all test test-ai console-demo raylib-demo jogo clean
all: test console-demo

build:
	mkdir -p $@

build/test_poker: tests/test_poker.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/test_poker.c $(POKER_SRC) $(LDLIBS) -o $@

build/test_torneio: tests/test_torneio.c $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) tests/test_torneio.c $(TORNEIO_SRC) $(POKER_SRC) $(LDLIBS) -o $@

$(DECK_TESTS): build/%: tests/%.c $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) tests/$*.c $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -o $@

build/test_lista_cartas: tests/test_lista_cartas.c $(LISTA_CARTAS_SRC) $(LISTA_CARTAS_HEADERS) $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) tests/test_lista_cartas.c $(LISTA_CARTAS_SRC) $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -o $@

build/test_mao: tests/test_mao.c $(MAO_SRC) $(MAO_HEADERS) $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) tests/test_mao.c $(MAO_SRC) $(TORNEIO_SRC) $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -o $@

build/test_eventos: tests/test_eventos.c $(MAO_SRC) $(MAO_HEADERS) $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) $(LDFLAGS) tests/test_eventos.c $(MAO_SRC) $(TORNEIO_SRC) $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -o $@

test: build/test_poker build/test_torneio build/test_mao build/test_eventos $(DECK_TESTS) build/test_lista_cartas build/test_poker_ai build/test_ai_integration build/test_tabelas_config build/test_ai_match_flow build/test_ai_embriaguez build/test_mao_ia build/test_partida
	./build/test_poker
	./build/test_torneio
	./build/test_mao
	./build/test_eventos
	./build/test_deck
	./build/test_edge_cases
	./build/test_integration
	./build/test_lista_cartas
	./build/test_poker_ai
	./build/test_ai_integration
	./build/test_tabelas_config
	./build/test_ai_match_flow
	./build/test_ai_embriaguez
	./build/test_mao_ia
	./build/test_partida

test-ai: build/test_poker_ai build/test_ai_integration build/test_tabelas_config build/test_ai_match_flow build/test_ai_embriaguez build/test_mao_ia
	./build/test_poker_ai
	./build/test_ai_integration
	./build/test_tabelas_config
	./build/test_ai_match_flow
	./build/test_ai_embriaguez
	./build/test_mao_ia

build/test_mao_ia: tests/test_mao_ia.c $(MAO_IA_SRC) $(MAO_IA_HEADERS) $(MAO_SRC) $(MAO_HEADERS) $(AI_SRC) $(AI_HEADERS) $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_mao_ia.c $(MAO_IA_SRC) $(MAO_SRC) $(AI_SRC) $(TORNEIO_SRC) $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/test_partida: tests/test_partida.c $(PARTIDA_SRC) $(PARTIDA_HEADERS) $(MAO_IA_SRC) $(MAO_IA_HEADERS) $(MAO_SRC) $(MAO_HEADERS) $(AI_SRC) $(AI_HEADERS) $(TORNEIO_SRC) $(TORNEIO_HEADERS) $(DECK_SRC) $(DECK_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_partida.c $(PARTIDA_SRC) $(MAO_IA_SRC) $(MAO_SRC) $(AI_SRC) $(TORNEIO_SRC) $(DECK_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/test_poker_ai: tests/test_poker_ai.c $(AI_SRC) $(AI_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_poker_ai.c $(AI_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/test_ai_integration: tests/test_ai_integration.c $(AI_SRC) $(AI_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_ai_integration.c $(AI_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/test_tabelas_config: tests/test_tabelas_config.c src/gameplay/poker_ai_config.c $(AI_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_tabelas_config.c src/gameplay/poker_ai_config.c $(LDLIBS) -o $@

build/test_ai_embriaguez: tests/test_ai_embriaguez.c $(AI_SRC) $(AI_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_ai_embriaguez.c $(AI_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/test_ai_match_flow: tests/test_ai_match_flow.c $(AI_SRC) $(AI_HEADERS) $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc/gameplay $(AI_CFLAGS) $(LDFLAGS) tests/test_ai_match_flow.c $(AI_SRC) $(POKER_SRC) $(LDLIBS) -lm -o $@

build/console_demo: examples/console_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) examples/console_demo.c $(POKER_SRC) $(LDLIBS) -o $@

console-demo: build/console_demo

build/raylib_demo: examples/raylib_demo.c $(POKER_SRC) $(POKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) examples/raylib_demo.c $(POKER_SRC) $(RAYLIB_LIBS) $(LDLIBS) -o $@

raylib-demo: build/raylib_demo

build/dead_mans_bluff: $(JOGO_SRC) $(JOGO_HEADERS) | build
	$(CC) $(CPPFLAGS) -Isrc -Isrc/gameplay $(AI_CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) $(JOGO_SRC) $(RAYLIB_LIBS) $(LDLIBS) -lm -o $@

jogo: build/dead_mans_bluff

clean:
	rm -rf build
