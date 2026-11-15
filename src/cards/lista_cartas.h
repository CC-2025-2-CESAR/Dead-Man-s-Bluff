#ifndef LISTA_CARTAS_H
#define LISTA_CARTAS_H

#include <stdbool.h>
#include <stddef.h>

#include "poker.h"

typedef struct NoCarta NoCarta;

/**
 * Lista simplesmente encadeada de cartas, usada para guardar e ordenar uma
 * mão (as cartas do jogador mais as do board). O baralho tem a sua própria
 * lista em deck.h; esta serve para as cartas que já saíram dele.
 *
 * Inicialize com ListaCartas lista = {0}; e libere com lista_cartas_liberar().
 * Só entram cartas válidas: valor de 2 a 14 e naipe de POKER_CLUBS a
 * POKER_SPADES. A mesma carta pode aparecer mais de uma vez.
 */
typedef struct {
    NoCarta *inicio;
    NoCarta *fim;
    size_t quantidade;
} ListaCartas;

/** Sentido da ordenação: do valor menor para o maior, ou o contrário. */
typedef enum {
    ORDEM_CRESCENTE = 0,
    ORDEM_DECRESCENTE
} OrdemCartas;

/**
 * Função chamada por lista_cartas_percorrer() para cada carta, da primeira
 * para a última. A carta é só para leitura.
 */
typedef void (*VisitanteCarta)(const PokerCard *carta, void *contexto);

/**
 * Compara duas cartas pelo valor e, se o valor for igual, pelo naipe, na
 * ordem de PokerSuit (paus, ouros, copas, espadas). O ás vale 14.
 *
 * @return negativo se a vem antes de b, positivo se vem depois, 0 se são
 * a mesma carta.
 */
int lista_cartas_comparar(PokerCard a, PokerCard b);

/**
 * Coloca a carta no início da lista.
 *
 * @return false se a lista for NULL, a carta for inválida ou faltar memória.
 * A lista não muda nesses casos.
 */
bool lista_cartas_inserir_inicio(ListaCartas *lista, PokerCard carta);

/**
 * Coloca a carta no fim da lista, em tempo constante.
 *
 * @return false se a lista for NULL, a carta for inválida ou faltar memória.
 * A lista não muda nesses casos.
 */
bool lista_cartas_inserir_fim(ListaCartas *lista, PokerCard carta);

/**
 * Copia as cartas do vetor para o fim da lista, na mesma ordem. Ou entram
 * todas ou nenhuma: se alguma carta for inválida ou faltar memória, a lista
 * volta ao que era antes da chamada.
 *
 * @return true se todas entraram; um vetor vazio também devolve true.
 */
bool lista_cartas_inserir_vetor(ListaCartas *lista,
                                const PokerCard *cartas,
                                size_t quantidade);

/**
 * Tira da lista a primeira ocorrência da carta, e só ela.
 *
 * @return true se encontrou e removeu a carta.
 */
bool lista_cartas_remover(ListaCartas *lista, PokerCard carta);

/** @return true se a carta aparece na lista. */
bool lista_cartas_contem(const ListaCartas *lista, PokerCard carta);

/**
 * Chama o visitante para cada carta, da primeira para a última. O visitante
 * não pode inserir nem remover cartas da lista durante o percurso.
 */
void lista_cartas_percorrer(const ListaCartas *lista,
                            VisitanteCarta visitante,
                            void *contexto);

/**
 * Copia as cartas, na ordem da lista, para o vetor de saída.
 *
 * @param capacidade quantas cartas cabem em saida; o que passar disso não é
 * copiado.
 * @return quantas cartas foram copiadas.
 */
size_t lista_cartas_copiar(const ListaCartas *lista,
                           PokerCard *saida,
                           size_t capacidade);

/**
 * Ordena a lista com Insertion Sort, religando os nós existentes: não aloca
 * memória, não pode falhar e não copia cartas. O custo é quadrático no pior
 * caso, o que basta para uma mão de até sete cartas.
 *
 * @param ordem sentido da ordenação, conforme lista_cartas_comparar().
 */
void lista_cartas_ordenar(ListaCartas *lista, OrdemCartas ordem);

/** Libera todos os nós e deixa a lista vazia, pronta para ser reutilizada. */
void lista_cartas_liberar(ListaCartas *lista);

#endif
