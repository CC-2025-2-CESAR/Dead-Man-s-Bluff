#ifndef DECK_H
#define DECK_H

#include <stdbool.h>
#include <stddef.h>

#include "../poker/poker.h"

typedef struct CardNode CardNode;

typedef struct {
    CardNode *head;
    CardNode *tail;
    size_t count;
} Deck;

/* Inicialize com Deck deck = {0}; antes de chamar deck_create(). */
bool deck_create(Deck *deck);
void deck_free(Deck *deck);

/* Usa Fisher-Yates para misturar as cartas que ainda estao na lista. */
void deck_shuffle(Deck *deck);

/* Remove a primeira carta e copia seu valor para o destino. */
bool deck_draw(Deck *deck, PokerCard *card);
bool deck_burn(Deck *deck);

/* Texas Hold'em heads-up: duas cartas por jogador, alternadamente. */
bool deck_deal(Deck *deck, int dealer, PokerCard hole[2][2]);

/*
 * Distribui cartas comunitarias seguindo a street informada:
 *  - POKER_FLOP: exige *board_count == 0, queima 1, coloca 3.
 *  - POKER_TURN: exige *board_count == 3, queima 1, coloca 1.
 *  - POKER_RIVER: exige *board_count == 4, queima 1, coloca 1.
 *
 * Nao distribui nada no preflop. Retorna false sem consumir cartas
 * quando os parametros, a sequencia ou a quantidade forem invalidos.
 * Inicialize o contador com size_t board_count = 0;
 */
bool deck_deal_board(Deck *deck,
                     PokerStreet street,
                     PokerCard board[5],
                     size_t *board_count);

#endif
