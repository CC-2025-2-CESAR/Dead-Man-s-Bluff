#ifndef PEDIDOS_H
#define PEDIDOS_H

#include "gameplay/trapacas.h"
#include "poker.h"

/**
 * Trapaças que o jogador pode pedir na sua vez. A interface só traduz teclas
 * ou botões para um PedidoTrapaca; quem decide se vale, quanto custa e se o
 * jogador foi descoberto é o core (ver partida_trapacear).
 *
 * PEDIDO_NENHUM: o jogador não pediu nada.
 * PEDIDO_BEBIDA: batizar a bebida do adversário (Spike his drink).
 * PEDIDO_CUMPLICE: o cúmplice revela a força da mão do adversário.
 * PEDIDO_MARCAR_CARTA: marcar uma carta da própria mão (carta_da_mao).
 * PEDIDO_SUBORNO_MESA: o dealer põe uma carta específica (carta) como a
 * primeira da próxima rua do board.
 * PEDIDO_SUBORNO_MAO: o dealer põe uma carta específica (carta) na sua mão na
 * próxima mão.
 * PEDIDO_SUBORNO_VER: o dealer mostra as cartas do adversário.
 * PEDIDO_MANGA: trocar uma carta da mão (carta_da_mao) pela carta da manga.
 */
typedef enum {
    PEDIDO_NENHUM = 0,
    PEDIDO_BEBIDA,
    PEDIDO_CUMPLICE,
    PEDIDO_MARCAR_CARTA,
    PEDIDO_SUBORNO_MESA,
    PEDIDO_SUBORNO_MAO,
    PEDIDO_SUBORNO_VER,
    PEDIDO_MANGA,
    PEDIDO_TOTAL
} TipoDePedido;

/**
 * Um pedido de trapaça.
 *
 * tipo: qual trapaça.
 * carta_da_mao: 0 ou 1, qual das duas cartas da mão do jogador; só vale em
 * PEDIDO_MARCAR_CARTA e PEDIDO_MANGA.
 * carta: a carta pedida ao dealer; só vale em PEDIDO_SUBORNO_MESA e
 * PEDIDO_SUBORNO_MAO.
 */
typedef struct {
    TipoDePedido tipo;
    int carta_da_mao;
    PokerCard carta;
} PedidoTrapaca;

#endif
