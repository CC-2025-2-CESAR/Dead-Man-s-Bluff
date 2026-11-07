#ifndef BARALHO_H
#define BARALHO_H

#include <stdbool.h>
#include <stddef.h>

/* Tipos de cartas definidos pelo modulo de poker da Julia. */
#include "../../poker.h"

/* O no fica privado ao baralho.c: o resto do jogo nao precisa acessa-lo. */
typedef struct NoCarta NoCarta;

typedef struct {
    NoCarta *inicio;
    size_t quantidade;
} Baralho;

/* Use com Baralho baralho = {0}; antes de chamar criar. */
bool baralho_criar(Baralho *baralho);
void baralho_liberar(Baralho *baralho);

/* Embaralha as cartas que ainda estao no baralho (Fisher-Yates). */
void baralho_embaralhar(Baralho *baralho);

/* Tira cartas do inicio da lista: nenhuma pode ser distribuida duas vezes. */
bool baralho_comprar(Baralho *baralho, PokerCard *carta);
bool baralho_distribuir(Baralho *baralho, PokerCard *destino, size_t total);

/* Distribui alternadamente duas cartas para cada um dos dois jogadores. */
bool baralho_distribuir_heads_up(Baralho *baralho, int dealer, PokerCard hole[2][2]);

/* Descarta uma carta antes do flop, turn ou river. */
bool baralho_queimar(Baralho *baralho);

#endif
