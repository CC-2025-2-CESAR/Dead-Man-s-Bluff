#include "baralho.h"

#include <stdlib.h>

struct NoCarta {
    PokerCard carta;
    struct NoCarta *proximo;
};

void baralho_liberar(Baralho *baralho)
{
    if (!baralho)
        return;

    NoCarta *atual = baralho->inicio;

    while (atual != NULL) {
        NoCarta *proximo = atual->proximo;
        free(atual);
        atual = proximo;
    }

    baralho->inicio = NULL;
    baralho->quantidade = 0;
}

bool baralho_criar(Baralho *baralho)
{
    if (!baralho || baralho->inicio != NULL || baralho->quantidade != 0)
        return false;

    NoCarta *ultimo = NULL;

    for (int naipe = POKER_CLUBS; naipe <= POKER_SPADES; ++naipe) {
        for (int valor = 2; valor <= 14; ++valor) {
            NoCarta *novo = malloc(sizeof *novo);

            if (novo == NULL) {
                baralho_liberar(baralho);
                return false;
            }

            novo->carta = (PokerCard){valor, (PokerSuit)naipe};
            novo->proximo = NULL;

            if (ultimo == NULL)
                baralho->inicio = novo;
            else
                ultimo->proximo = novo;

            ultimo = novo;
            ++baralho->quantidade;
        }
    }

    return true;
}

void baralho_embaralhar(Baralho *baralho)
{
    if (!baralho || baralho->quantidade < 2)
        return;

    /* Um vetor temporario permite acessar os nos por indice em O(1). */
    NoCarta *nos[52];
    size_t total = 0;

    for (NoCarta *atual = baralho->inicio;
         atual != NULL && total < 52;
         atual = atual->proximo) {
        nos[total++] = atual;
    }

    /* Fisher-Yates: cada indice so troca com um indice anterior ou ele mesmo. */
    for (size_t i = total - 1; i > 0; --i) {
        size_t j = (size_t)rand() % (i + 1);
        PokerCard temporaria = nos[i]->carta;
        nos[i]->carta = nos[j]->carta;
        nos[j]->carta = temporaria;
    }
}

bool baralho_comprar(Baralho *baralho, PokerCard *carta)
{
    if (!baralho || !carta || baralho->inicio == NULL)
        return false;

    NoCarta *primeiro = baralho->inicio;
    *carta = primeiro->carta;
    baralho->inicio = primeiro->proximo;
    --baralho->quantidade;
    free(primeiro);
    return true;
}

bool baralho_distribuir(Baralho *baralho, PokerCard *destino, size_t total)
{
    if (!baralho || total > baralho->quantidade ||
        (total > 0 && destino == NULL)) {
        return false;
    }

    for (size_t i = 0; i < total; ++i)
        baralho_comprar(baralho, &destino[i]);

    return true;
}

bool baralho_distribuir_heads_up(Baralho *baralho, int dealer, PokerCard hole[2][2])
{
    if (!baralho || !hole || (dealer != 0 && dealer != 1) ||
        baralho->quantidade < 4)
        return false;

    /* Em heads-up, o dealer e o small blind e recebe a primeira carta. */
    for (int rodada = 0; rodada < 2; ++rodada) {
        for (int ordem = 0; ordem < 2; ++ordem) {
            int jogador = (dealer + ordem) % 2;
            baralho_comprar(baralho, &hole[jogador][rodada]);
        }
    }

    return true;
}

bool baralho_queimar(Baralho *baralho)
{
    PokerCard descartada;
    return baralho_comprar(baralho, &descartada);
}
