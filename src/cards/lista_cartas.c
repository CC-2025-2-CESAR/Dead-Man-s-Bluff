#include "lista_cartas.h"

#include <stdlib.h>

struct NoCarta {
    PokerCard carta;
    struct NoCarta *proximo;
};

static bool carta_valida(PokerCard carta)
{
    if (carta.rank < 2 || carta.rank > 14)
        return false;

    switch (carta.suit) {
        case POKER_CLUBS:
        case POKER_DIAMONDS:
        case POKER_HEARTS:
        case POKER_SPADES:
            return true;
    }

    return false;
}

static NoCarta *criar_no(PokerCard carta)
{
    if (!carta_valida(carta))
        return NULL;

    NoCarta *no = malloc(sizeof *no);
    if (no == NULL)
        return NULL;

    no->carta = carta;
    no->proximo = NULL;
    return no;
}

static void liberar_nos(NoCarta *no)
{
    while (no != NULL) {
        NoCarta *proximo = no->proximo;
        free(no);
        no = proximo;
    }
}

static void desfazer_insercoes(ListaCartas *lista,
                               NoCarta *fim_anterior,
                               size_t quantidade_anterior)
{
    if (fim_anterior != NULL) {
        liberar_nos(fim_anterior->proximo);
        fim_anterior->proximo = NULL;
    } else {
        liberar_nos(lista->inicio);
        lista->inicio = NULL;
    }

    lista->fim = fim_anterior;
    lista->quantidade = quantidade_anterior;
}

static bool fica_antes_ou_junto(PokerCard a, PokerCard b, OrdemCartas ordem)
{
    int comparacao = lista_cartas_comparar(a, b);

    if (ordem == ORDEM_DECRESCENTE)
        return comparacao >= 0;

    return comparacao <= 0;
}

int lista_cartas_comparar(PokerCard a, PokerCard b)
{
    if (a.rank != b.rank)
        return a.rank < b.rank ? -1 : 1;

    if (a.suit != b.suit)
        return a.suit < b.suit ? -1 : 1;

    return 0;
}

bool lista_cartas_inserir_inicio(ListaCartas *lista, PokerCard carta)
{
    if (lista == NULL)
        return false;

    NoCarta *no = criar_no(carta);
    if (no == NULL)
        return false;

    no->proximo = lista->inicio;
    lista->inicio = no;

    if (lista->fim == NULL)
        lista->fim = no;

    lista->quantidade++;
    return true;
}

bool lista_cartas_inserir_fim(ListaCartas *lista, PokerCard carta)
{
    if (lista == NULL)
        return false;

    NoCarta *no = criar_no(carta);
    if (no == NULL)
        return false;

    if (lista->fim == NULL)
        lista->inicio = no;
    else
        lista->fim->proximo = no;

    lista->fim = no;
    lista->quantidade++;
    return true;
}

bool lista_cartas_inserir_vetor(ListaCartas *lista,
                                const PokerCard *cartas,
                                size_t quantidade)
{
    if (lista == NULL || (cartas == NULL && quantidade > 0))
        return false;

    NoCarta *fim_anterior = lista->fim;
    size_t quantidade_anterior = lista->quantidade;

    for (size_t i = 0; i < quantidade; i++) {
        if (!lista_cartas_inserir_fim(lista, cartas[i])) {
            desfazer_insercoes(lista, fim_anterior, quantidade_anterior);
            return false;
        }
    }

    return true;
}

bool lista_cartas_remover(ListaCartas *lista, PokerCard carta)
{
    if (lista == NULL)
        return false;

    NoCarta *anterior = NULL;
    NoCarta *atual = lista->inicio;

    while (atual != NULL && lista_cartas_comparar(atual->carta, carta) != 0) {
        anterior = atual;
        atual = atual->proximo;
    }

    if (atual == NULL)
        return false;

    if (anterior == NULL)
        lista->inicio = atual->proximo;
    else
        anterior->proximo = atual->proximo;

    if (lista->fim == atual)
        lista->fim = anterior;

    lista->quantidade--;
    free(atual);
    return true;
}

bool lista_cartas_contem(const ListaCartas *lista, PokerCard carta)
{
    if (lista == NULL)
        return false;

    for (const NoCarta *no = lista->inicio; no != NULL; no = no->proximo) {
        if (lista_cartas_comparar(no->carta, carta) == 0)
            return true;
    }

    return false;
}

void lista_cartas_percorrer(const ListaCartas *lista,
                            VisitanteCarta visitante,
                            void *contexto)
{
    if (lista == NULL || visitante == NULL)
        return;

    for (const NoCarta *no = lista->inicio; no != NULL; no = no->proximo)
        visitante(&no->carta, contexto);
}

size_t lista_cartas_copiar(const ListaCartas *lista,
                           PokerCard *saida,
                           size_t capacidade)
{
    if (lista == NULL || saida == NULL)
        return 0;

    size_t copiadas = 0;

    for (const NoCarta *no = lista->inicio;
         no != NULL && copiadas < capacidade;
         no = no->proximo)
        saida[copiadas++] = no->carta;

    return copiadas;
}

void lista_cartas_ordenar(ListaCartas *lista, OrdemCartas ordem)
{
    if (lista == NULL || lista->inicio == NULL || lista->inicio->proximo == NULL)
        return;

    NoCarta *pendentes = lista->inicio;
    NoCarta *ordenada = NULL;
    NoCarta *fim = NULL;

    while (pendentes != NULL) {
        NoCarta *no = pendentes;
        pendentes = pendentes->proximo;

        NoCarta *anterior = NULL;
        NoCarta *atual = ordenada;
        while (atual != NULL && fica_antes_ou_junto(atual->carta, no->carta, ordem)) {
            anterior = atual;
            atual = atual->proximo;
        }

        no->proximo = atual;
        if (anterior == NULL)
            ordenada = no;
        else
            anterior->proximo = no;

        if (atual == NULL)
            fim = no;
    }

    lista->inicio = ordenada;
    lista->fim = fim;
}

void lista_cartas_liberar(ListaCartas *lista)
{
    if (lista == NULL)
        return;

    liberar_nos(lista->inicio);
    lista->inicio = NULL;
    lista->fim = NULL;
    lista->quantidade = 0;
}
