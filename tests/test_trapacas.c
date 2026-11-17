#include "core/partida.h"
#include "core/salvamento.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 70
#define LIMITE_DE_PASSOS 100000

static const PokerAction DESISTIR = {POKER_FOLD, 0};
static const PokerAction PASSAR = {POKER_CHECK, 0};
static const PokerAction PAGAR = {POKER_CALL, 0};
static const PokerAction ALL_IN = {POKER_ALL_IN, 0};
static const PokerCard SEM_CARTA = {0, POKER_CLUBS};
static const PokerCard DOIS_DE_PAUS = {2, POKER_CLUBS};

static unsigned proximo_aleatorio(unsigned *semente)
{
    *semente = *semente * 1664525u + 1013904223u;
    return *semente >> 8;
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static PedidoTrapaca pedir(TipoDePedido tipo, int carta_da_mao, PokerCard carta)
{
    PedidoTrapaca pedido = {tipo, carta_da_mao, carta};

    return pedido;
}

/** Junta todas as cartas à vista do jogador mais a manga, e confere que não há repetida. */
static void conferir_cartas_unicas(const Partida *p)
{
    PokerCard todas[10];
    int n = 0;

    if (p->mao.situacao == MAO_VAZIA) {
        return;
    }

    for (int assento = 0; assento < 2; assento++) {
        for (int i = 0; i < 2; i++) {
            todas[n++] = p->mao.cartas[assento][i];
        }
    }

    for (size_t i = 0; i < p->mao.cartas_no_board; i++) {
        todas[n++] = p->mao.board[i];
    }

    todas[n++] = p->cheats.carta_na_manga;

    for (int i = 0; i < n; i++) {
        CHECK(todas[i].rank >= 2 && todas[i].rank <= 14);

        for (int j = i + 1; j < n; j++) {
            CHECK(!mesma_carta(todas[i], todas[j]));
        }
    }
}

/** Uma carta que o jogador ainda não vê: fora da mão dele, do board e da manga. */
static PokerCard carta_livre(const Partida *p, int pulo)
{
    for (int suit = POKER_CLUBS; suit <= POKER_SPADES; suit++) {
        for (int rank = 2; rank <= 14; rank++) {
            PokerCard carta = {rank, (PokerSuit)suit};
            bool vista = mesma_carta(carta, p->cheats.carta_na_manga);

            for (int i = 0; i < 2; i++) {
                vista = vista || mesma_carta(carta, p->mao.cartas[TORNEIO_ASSENTO_JOGADOR][i]);
            }

            for (size_t i = 0; i < p->mao.cartas_no_board; i++) {
                vista = vista || mesma_carta(carta, p->mao.board[i]);
            }

            if (!vista && pulo-- == 0) {
                return carta;
            }
        }
    }

    CHECK(false);
    return SEM_CARTA;
}

/** Esvazia a fila e devolve os eventos de trapaça que havia, na ordem. */
static int eventos_de_trapaca(Partida *p, EventoTrapaca *lista, int capacidade)
{
    EventoJogo evento;
    int n = 0;

    while (fila_eventos_retirar(&p->eventos, &evento)) {
        if (evento.tipo == EVENTO_TRAPACA && n < capacidade) {
            lista[n++] = evento.trapaca;
        }
    }

    return n;
}

static bool ha_evento(const EventoTrapaca *lista, int n, TipoDePedido pedido, CheatResult resultado,
                      bool na_distribuicao)
{
    for (int i = 0; i < n; i++) {
        if (lista[i].pedido == pedido && lista[i].resultado == resultado &&
            lista[i].na_distribuicao == na_distribuicao) {
            return true;
        }
    }

    return false;
}

/** Joga até o fim da mão: a IA age e o jogador desiste, se tiver a vez. */
static void encerrar_mao(Partida *p)
{
    for (int passos = 0; passos < 200; passos++) {
        if (p->situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
            CHECK(partida_agir_adversario(p, NULL) == POKER_OK);
        } else if (p->situacao == PARTIDA_VEZ_DO_JOGADOR) {
            CHECK(partida_agir_jogador(p, DESISTIR) == POKER_OK);
        } else {
            return;
        }
    }

    CHECK(false);
}

/** Leva a mão ao flop, na vez do jogador; false se a mão acabou antes ou a vez é da IA. */
static bool ir_ao_flop(Partida *p)
{
    for (int passos = 0; passos < 40; passos++) {
        PokerLegalActions legais;

        if (p->situacao == PARTIDA_VEZ_DO_JOGADOR && p->mao.cartas_no_board >= 3) {
            return true;
        }

        if (p->situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
            CHECK(partida_agir_adversario(p, NULL) == POKER_OK);
        } else if (p->situacao == PARTIDA_VEZ_DO_JOGADOR) {
            CHECK(partida_acoes_legais(p, &legais) == POKER_OK);
            CHECK(partida_agir_jogador(p, legais.check ? PASSAR : PAGAR) == POKER_OK);
        } else {
            return false;
        }
    }

    return false;
}

/**
 * Começa uma partida e tenta o pedido com cada estado do gerador do risco, do
 * 1 em diante, até a tentativa dar o resultado desejado. A partida fica logo
 * depois da trapaça, com os sorteios daquele estado. A vontade de estar no
 * flop faz a trapaça valer no flop, na vez do jogador.
 */
static bool tentar(Partida *p, unsigned semente, PedidoTrapaca pedido, CheatResult desejado, bool no_flop)
{
    for (uint64_t estado = 1; estado <= 3000; estado++) {
        CheatResult resultado;

        CHECK(partida_iniciar(p, semente));

        if (no_flop && !ir_ao_flop(p)) {
            partida_encerrar(p);
            return false;
        }

        p->estado_do_risco = estado;
        p->estado_do_risco_da_mao = estado;

        if (partida_trapacear(p, pedido, &resultado) == POKER_OK && resultado == desejado) {
            return true;
        }

        partida_encerrar(p);
    }

    return false;
}

static void test_custo_e_chance_de_cada_trapaca(void)
{
    static Partida p;
    Partida vazia = {0};

    CHECK(partida_iniciar(&p, 1));
    CHECK(p.torneio.confronto == 0 && p.mao.mesa.big_blind == 20);

    /* Fichas iniciais do adversário (2.000) por 20: a base do suborno é 100. */
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_BEBIDA) == 0);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_MARCAR_CARTA) == 0);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_MANGA) == 0);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_CUMPLICE) == 20);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_SUBORNO_MESA) == 100);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_SUBORNO_MAO) == 200);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_SUBORNO_VER) == 300);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_NENHUM) == 0);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_TOTAL) == 0);
    CHECK(partida_custo_da_trapaca(&p, (TipoDePedido)99) == 0);
    CHECK(partida_custo_da_trapaca(NULL, PEDIDO_CUMPLICE) == 0);

    p.cheats.usos_cumplice_blind = 2;
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_CUMPLICE) == 60);
    p.torneio.confronto = 2;
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_SUBORNO_VER) == 1200);
    p.torneio.confronto = 1;
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_SUBORNO_MESA) == 200);
    p.torneio.confronto = 0;
    p.cheats.usos_cumplice_blind = 0;

    /* Agressivo sóbrio: 1,2 vezes a chance-base. */
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 12);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_CUMPLICE) == 18);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_MARCAR_CARTA) == 6);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_SUBORNO_MESA) == 24);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_SUBORNO_MAO) == 24);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_SUBORNO_VER) == 24);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_MANGA) == 30);

    /* A desconfiança é do oponente do confronto e sobe de um para o outro... */
    p.torneio.confronto = 1;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 15);
    p.torneio.confronto = 2;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 25);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_MANGA) == 62);

    /* ... e não segue o estilo de jogo, que muda durante o confronto. */
    p.torneio.confronto = 0;
    p.ia.style = POKER_AI_CAUTIOUS;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 12);
    p.ia.style = POKER_AI_AGGRESSIVE;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 12);
    p.ia.style = POKER_AI_BALANCED;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 12);

    /* Embriagado, a desconfiança cai. */
    p.cheats.maos_embriagado = 1;
    p.torneio.confronto = 2;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 13);
    p.torneio.confronto = 1;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 12);
    p.torneio.confronto = 0;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 10);
    p.cheats.maos_embriagado = 0;

    /* Cada uso repetido soma o incremento da trapaça. */
    p.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] = 2;
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 31);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_CUMPLICE) == 18);

    CHECK(partida_chance_de_descoberta(&p, PEDIDO_NENHUM) == -1);
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_TOTAL) == -1);
    CHECK(partida_chance_de_descoberta(&p, (TipoDePedido)99) == -1);
    CHECK(partida_chance_de_descoberta(NULL, PEDIDO_BEBIDA) == -1);
    CHECK(partida_custo_da_trapaca(&vazia, PEDIDO_CUMPLICE) == 0);
    partida_encerrar(&p);
}

static void test_pedidos_invalidos_e_fora_de_hora_sao_recusados(void)
{
    static Partida p;
    Partida vazia = {0};
    PokerCard livre;
    int registros;

    CHECK(partida_pode_trapacear(NULL, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_trapacear(NULL, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA), NULL) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&vazia, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_STATE);

    CHECK(partida_iniciar(&p, 4));
    livre = carta_livre(&p, 3);
    registros = (int)p.quantidade_no_registro;

    /* O que dá certo na vez do jogador, uma a uma. */
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 1, SEM_CARTA)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, livre)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, livre)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MANGA, 0, SEM_CARTA)) == POKER_OK);

    /* Argumentos inválidos. */
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_NENHUM, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_TOTAL, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir((TipoDePedido)-1, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 2, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, -1, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MANGA, 2, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, (PokerCard){15, POKER_HEARTS})) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, (PokerCard){5, (PokerSuit)4})) == POKER_ERR_ARGUMENT);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, (PokerCard){5, (PokerSuit)-1})) == POKER_ERR_ARGUMENT);
    /* A carta da mão só importa para marcar e para a manga. */
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 7, SEM_CARTA)) == POKER_OK);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 7, SEM_CARTA)) == POKER_OK);

    /* Cartas já à vista: da própria mão, do board e a da manga. */
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][0])) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][1])) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, DOIS_DE_PAUS)) == POKER_ERR_ACTION);
    p.mao.board[0] = livre;
    p.mao.cartas_no_board = 1;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, livre)) == POKER_ERR_ACTION);
    p.mao.cartas_no_board = 0;

    /* Regras de cada trapaça. */
    p.mao.mesa.street = POKER_RIVER;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, livre)) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, livre)) == POKER_OK);
    p.mao.mesa.street = POKER_PREFLOP;
    p.mao.carta_forcada_pendente = true;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, livre)) == POKER_ERR_ACTION);
    p.mao.carta_forcada_pendente = false;
    p.cheats.carta_comprada_pendente = true;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, livre)) == POKER_ERR_ACTION);
    p.cheats.carta_comprada_pendente = false;
    p.cheats.suborno_usado_na_mao = true;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, livre)) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, livre)) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA)) == POKER_OK);
    p.cheats.suborno_usado_na_mao = false;

    p.cheats.carta_na_manga.rank = 0;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MANGA, 0, SEM_CARTA)) == POKER_ERR_ACTION);
    p.cheats.carta_na_manga = DOIS_DE_PAUS;

    p.cheats.cartas_marcadas[1] = p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][1];
    p.cheats.turnos_marcacao[1] = 3;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 1, SEM_CARTA)) == POKER_ERR_ACTION);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)) == POKER_OK);
    p.cheats.cartas_marcadas[1].rank = 0;
    p.cheats.turnos_marcacao[1] = 0;

    for (int i = 0; i < 5; i++) {
        p.cheats.cartas_marcadas[i] = carta_livre(&p, i);
        p.cheats.turnos_marcacao[i] = 2;
    }

    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)) == POKER_ERR_ACTION);

    for (int i = 0; i < 5; i++) {
        p.cheats.cartas_marcadas[i].rank = 0;
        p.cheats.turnos_marcacao[i] = 0;
    }

    p.registro[p.quantidade_no_registro++] = (RegistroDaMao){.tipo = REGISTRO_TRAPACA,
                                                              .pedido = pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)};
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)) == POKER_ERR_ACTION);
    p.quantidade_no_registro--;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)) == POKER_OK);

    /* Fichas: pagar nunca deixa o jogador sem nada. */
    p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips = 300;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_ERR_CHIPS);
    p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips = 301;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_OK);
    p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips = 20;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA)) == POKER_ERR_CHIPS);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_OK);
    p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips = 21;
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA)) == POKER_OK);
    p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips = 1990;

    /* Nada disso mexeu em registros nem em fichas. */
    CHECK((int)p.quantidade_no_registro == registros);

    /* Fora da vez do jogador. */
    CHECK(partida_agir_jogador(&p, PAGAR) == POKER_OK);
    CHECK(p.situacao == PARTIDA_VEZ_DO_ADVERSARIO);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_TURN);
    CHECK(partida_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA), NULL) == POKER_ERR_TURN);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_NENHUM, 0, SEM_CARTA)) == POKER_ERR_ARGUMENT);
    encerrar_mao(&p);
    CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_STATE);
    partida_encerrar(&p);
}

static void test_recusa_nao_gasta_sorteio_nem_registra(void)
{
    static Partida p;
    uint64_t estado;
    size_t registros;

    CHECK(partida_iniciar(&p, 5));
    estado = p.estado_do_risco;
    registros = p.quantidade_no_registro;
    CHECK(partida_trapacear(&p, pedir(PEDIDO_NENHUM, 0, SEM_CARTA), NULL) == POKER_ERR_ARGUMENT);
    CHECK(partida_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 5, SEM_CARTA), NULL) == POKER_ERR_ARGUMENT);
    CHECK(p.estado_do_risco == estado);
    CHECK(p.quantidade_no_registro == registros);
    CHECK(fila_eventos_tamanho(&p.eventos) == 3);
    partida_encerrar(&p);
}

static void test_bebida_embriaga_por_tres_maos(void)
{
    static Partida p;
    EventoTrapaca eventos[8];
    int n;

    CHECK(tentar(&p, 1, pedir(PEDIDO_BEBIDA, 1, (PokerCard){9, POKER_HEARTS}), TRAPACA_SUCESSO, false));
    CHECK(p.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(p.cheats.maos_embriagado == 3 && p.adversario_embriagado);
    /* O registro guarda só o que o tipo de pedido usa. */
    CHECK(p.registro[0].pedido.carta_da_mao == 0 && p.registro[0].pedido.carta.rank == 0);
    CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] == 1);
    CHECK(p.quantidade_no_registro == 1 && p.registro[0].tipo == REGISTRO_TRAPACA);
    CHECK(p.registro[0].pedido.tipo == PEDIDO_BEBIDA);
    CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990);

    n = eventos_de_trapaca(&p, eventos, 8);
    CHECK(n == 1);
    CHECK(eventos[0].pedido == PEDIDO_BEBIDA && eventos[0].resultado == TRAPACA_SUCESSO);
    CHECK(eventos[0].custo == 0 && !eventos[0].na_distribuicao);

    /* O uso aumenta a chance da próxima: 10 + 8 = 18 pontos, vezes 1,0 embriagado = 18. */
    CHECK(partida_chance_de_descoberta(&p, PEDIDO_BEBIDA) == 18);

    /* A bebedeira passa a cada mão que termina. */
    encerrar_mao(&p);
    CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(p.cheats.maos_embriagado == 2 && p.adversario_embriagado);
    CHECK(partida_continuar(&p));
    CHECK(p.adversario_embriagado);
    encerrar_mao(&p);
    CHECK(p.cheats.maos_embriagado == 1 && p.adversario_embriagado);
    CHECK(partida_continuar(&p));
    encerrar_mao(&p);
    CHECK(p.cheats.maos_embriagado == 0 && !p.adversario_embriagado);
    CHECK(partida_continuar(&p));
    CHECK(!p.adversario_embriagado);
    partida_encerrar(&p);
}

static void test_descoberta_leva_a_roleta_e_a_mao_segue_se_sobreviver(void)
{
    static Partida p;
    EventoTrapaca eventos[8];
    bool sobreviveu = false;
    bool morreu = false;

    for (uint64_t gatilho = 1; gatilho <= 80 && !(sobreviveu && morreu); gatilho++) {
        CHECK(tentar(&p, 1, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA), TRAPACA_DESCOBERTA, false));
        CHECK(p.situacao == PARTIDA_ROLETA && p.roleta_motivo == ROLETA_DESCOBERTO);
        CHECK(p.cheats.maos_embriagado == 0 && !p.adversario_embriagado);
        CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] == 0);
        CHECK(p.quantidade_no_registro == 1 && p.registro[0].tipo == REGISTRO_TRAPACA);
        CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
        CHECK(eventos[0].resultado == TRAPACA_DESCOBERTA);

        /* Na roleta, nada mais vale. */
        CHECK(partida_agir_jogador(&p, PAGAR) == POKER_ERR_STATE);
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_STATE);
        CHECK(!partida_continuar(&p));

        /* O gatilho só depende do gerador da partida. */
        p.estado_do_risco = gatilho;
        p.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] = 4;
        CHECK(partida_puxar_gatilho(&p));

        if (p.situacao == PARTIDA_JOGADOR_MORTO) {
            morreu = true;
            CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_ERR_STATE);
        } else {
            sobreviveu = true;
            CHECK(p.situacao == PARTIDA_VEZ_DO_JOGADOR && p.roleta_motivo == ROLETA_NENHUMA);
            CHECK(p.risco.balas_roleta == 2);
            CHECK(p.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] == 0);
            CHECK(p.cheats.maos_embriagado == 0);
            CHECK(p.quantidade_no_registro == 2 && p.registro[1].tipo == REGISTRO_GATILHO);

            /* A mão segue na vez dele, e ele ainda pode agir. */
            CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_BEBIDA, 0, SEM_CARTA)) == POKER_OK);
            CHECK(partida_agir_jogador(&p, DESISTIR) == POKER_OK);
            CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
            CHECK(p.quantidade_no_registro == 3 && p.registro[2].tipo == REGISTRO_ACAO);
        }

        partida_encerrar(&p);
    }

    CHECK(sobreviveu && morreu);
}

static void test_cumplice_cobra_do_stack_e_revela_a_categoria(void)
{
    static Partida p;
    EventoTrapaca eventos[8];
    PokerCard *dele;
    bool no_flop = false;

    CHECK(tentar(&p, 2, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA), TRAPACA_SUCESSO, false));
    CHECK(p.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 20);
    CHECK(p.mao.mesa.initial_total == 4000 - 20);
    CHECK(p.mao.mesa.player[0].chips + p.mao.mesa.player[1].chips + poker_pot(&p.mao.mesa) ==
          p.mao.mesa.initial_total);
    CHECK(p.cheats.usos_cumplice_blind == 1);
    CHECK(p.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] == 1);
    CHECK(partida_custo_da_trapaca(&p, PEDIDO_CUMPLICE) == 40);

    CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
    CHECK(eventos[0].custo == 20 && eventos[0].tem_categoria);
    dele = p.mao.cartas[TORNEIO_ASSENTO_ADVERSARIO];
    CHECK(eventos[0].categoria == (dele[0].rank == dele[1].rank ? POKER_ONE_PAIR : POKER_HIGH_CARD));

    /* As fichas pagas saem de circulação: o torneio registra o que sobrou. */
    encerrar_mao(&p);
    CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(p.torneio.fichas_jogador + p.torneio.fichas_adversario == 4000 - 20);
    partida_encerrar(&p);

    /* No flop, a categoria vem da melhor mão dele com o board. */
    for (unsigned semente = 1; semente <= 80 && !no_flop; semente++) {
        PokerHandValue valor;

        if (!tentar(&p, semente, pedir(PEDIDO_CUMPLICE, 0, SEM_CARTA), TRAPACA_SUCESSO, true)) {
            continue;
        }

        no_flop = true;
        CHECK(p.mao.cartas_no_board >= 3);
        CHECK(poker_evaluate_holdem(p.mao.cartas[TORNEIO_ASSENTO_ADVERSARIO], p.mao.board,
                                    p.mao.cartas_no_board, &valor) == POKER_OK);
        CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
        CHECK(eventos[0].tem_categoria && eventos[0].categoria == valor.category);
        partida_encerrar(&p);
    }

    CHECK(no_flop);
}

static void test_marcar_carta_dura_cinco_maos_e_avisa_quando_cai_com_o_adversario(void)
{
    static Partida p;
    EventoTrapaca eventos[16];
    bool avisou = false;

    for (unsigned semente = 1; semente <= 300; semente++) {
        PokerCard marcada;

        CHECK(tentar(&p, semente, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA), TRAPACA_SUCESSO, false));
        marcada = p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][0];
        CHECK(is_card_marked(&p.cheats, marcada));
        CHECK(p.cheats.turnos_marcacao[0] == PARTIDA_MARCA_DURACAO);
        CHECK(p.risco.usos_trapaca[PROB_TRAPACA_MARCAR_CARTA] == 1);
        CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990);

        /* Uma carta por mão. */
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 0, SEM_CARTA)) == POKER_ERR_ACTION);
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_MARCAR_CARTA, 1, SEM_CARTA)) == POKER_ERR_ACTION);
        CHECK(eventos_de_trapaca(&p, eventos, 16) == 1);
        CHECK(mesma_carta(eventos[0].cartas[0], marcada) && eventos[0].quantidade_de_cartas == 1);

        for (int mao = 1; mao <= PARTIDA_MARCA_DURACAO; mao++) {
            const PokerCard *dele;
            bool com_ele;
            int n;

            encerrar_mao(&p);
            CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
            CHECK(p.cheats.turnos_marcacao[0] == PARTIDA_MARCA_DURACAO - mao);

            if (mao == PARTIDA_MARCA_DURACAO) {
                CHECK(p.cheats.cartas_marcadas[0].rank == 0 && !is_card_marked(&p.cheats, marcada));
                break;
            }

            CHECK(partida_continuar(&p));
            dele = p.mao.cartas[TORNEIO_ASSENTO_ADVERSARIO];
            com_ele = mesma_carta(dele[0], marcada) || mesma_carta(dele[1], marcada);
            n = eventos_de_trapaca(&p, eventos, 16);
            CHECK(ha_evento(eventos, n, PEDIDO_MARCAR_CARTA, TRAPACA_SUCESSO, true) == com_ele);
            avisou = avisou || com_ele;
        }

        partida_encerrar(&p);
    }

    CHECK(avisou);
}

static void test_suborno_ver_mostra_as_cartas_do_adversario(void)
{
    static Partida p;
    EventoTrapaca eventos[8];

    CHECK(tentar(&p, 3, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA), TRAPACA_SUCESSO, false));
    CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 300);
    CHECK(p.mao.mesa.initial_total == 4000 - 300);
    CHECK(p.mao.mesa.player[0].chips + p.mao.mesa.player[1].chips + poker_pot(&p.mao.mesa) ==
          p.mao.mesa.initial_total);
    CHECK(p.cheats.suborno_usado_na_mao);
    CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SUBORNO_DEALER] == 1);
    CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
    CHECK(eventos[0].quantidade_de_cartas == 2 && eventos[0].custo == 300);
    CHECK(mesma_carta(eventos[0].cartas[0], p.mao.cartas[TORNEIO_ASSENTO_ADVERSARIO][0]));
    CHECK(mesma_carta(eventos[0].cartas[1], p.mao.cartas[TORNEIO_ASSENTO_ADVERSARIO][1]));

    /* Um suborno por mão; a próxima mão libera de novo. */
    CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_ERR_ACTION);
    encerrar_mao(&p);
    CHECK(!p.cheats.suborno_usado_na_mao);
    CHECK(partida_continuar(&p));

    /* Na mão seguinte o dealer é o adversário, que age primeiro. */
    while (p.situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
        CHECK(partida_agir_adversario(&p, NULL) == POKER_OK);
    }

    if (p.situacao == PARTIDA_VEZ_DO_JOGADOR) {
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA)) == POKER_OK);
    }

    CHECK(!p.cheats.suborno_usado_na_mao);
    partida_encerrar(&p);
}

static void test_dealer_recusa_trinta_por_cento_e_fica_com_o_dinheiro(void)
{
    static Partida p;
    EventoTrapaca eventos[8];
    int recusadas = 0;
    int descobertas = 0;
    int sucessos = 0;

    for (uint64_t estado = 1; estado <= 800; estado++) {
        CheatResult resultado;

        CHECK(partida_iniciar(&p, 3));
        p.estado_do_risco = estado;
        CHECK(partida_trapacear(&p, pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA), &resultado) == POKER_OK);

        /* O dealer embolsa o dinheiro nos três casos. */
        CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 300);
        CHECK(p.mao.mesa.initial_total == 4000 - 300);
        CHECK(p.cheats.suborno_usado_na_mao);
        CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
        CHECK(eventos[0].resultado == resultado && eventos[0].custo == 300);

        if (resultado == TRAPACA_RECUSADA) {
            recusadas++;
            CHECK(p.situacao == PARTIDA_VEZ_DO_JOGADOR);
            CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SUBORNO_DEALER] == 0);
            CHECK(eventos[0].quantidade_de_cartas == 0);
        } else if (resultado == TRAPACA_DESCOBERTA) {
            descobertas++;
            CHECK(p.situacao == PARTIDA_ROLETA && p.roleta_motivo == ROLETA_DESCOBERTO);
            CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SUBORNO_DEALER] == 0);
        } else {
            sucessos++;
            CHECK(p.risco.usos_trapaca[PROB_TRAPACA_SUBORNO_DEALER] == 1);
        }

        partida_encerrar(&p);
    }

    /* 30% de recusa, e 24% de descoberta sobre os 70% restantes (sobrio, Agressivo). */
    CHECK(recusadas >= 192 && recusadas <= 288);
    CHECK(descobertas >= 96 && descobertas <= 176);
    CHECK(sucessos > 300);
}

/** Acha um estado do gerador em que o pedido dá certo, e o devolve; 0 se não achar. */
static uint64_t estado_que_da_certo(unsigned semente, PedidoTrapaca pedido)
{
    static Partida p;
    uint64_t estado = 0;

    if (tentar(&p, semente, pedido, TRAPACA_SUCESSO, false)) {
        estado = p.estado_do_risco_da_mao;
        partida_encerrar(&p);
    }

    return estado;
}

static void test_suborno_poe_a_carta_na_mesa_ou_o_dealer_nao_acha(void)
{
    static Partida p;
    EventoTrapaca eventos[16];
    uint64_t estado;
    int aplicadas = 0;
    int falhas = 0;

    CHECK(partida_iniciar(&p, 6));
    estado = estado_que_da_certo(6, pedir(PEDIDO_SUBORNO_MESA, 0, carta_livre(&p, 0)));
    CHECK(estado != 0);

    for (int pulo = 0; pulo < 45; pulo++) {
        CheatResult resultado;
        PokerCard carta;
        int n;

        CHECK(partida_iniciar(&p, 6));
        carta = carta_livre(&p, pulo);
        p.estado_do_risco = estado;
        p.estado_do_risco_da_mao = estado;
        CHECK(partida_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, carta), &resultado) == POKER_OK);
        CHECK(resultado == TRAPACA_SUCESSO);
        CHECK(p.mao.carta_forcada_pendente && mesma_carta(p.mao.carta_forcada, carta));
        CHECK(!p.cheats.carta_comprada_pendente && p.cheats.destino_carta_comprada == -1);
        CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 100);
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, carta)) == POKER_ERR_ACTION);
        CHECK(eventos_de_trapaca(&p, eventos, 16) == 1);

        CHECK(ir_ao_flop(&p) || p.situacao != PARTIDA_VEZ_DO_JOGADOR || p.mao.cartas_no_board < 3);

        if (p.mao.cartas_no_board < 3) {
            partida_encerrar(&p);
            continue;
        }

        n = eventos_de_trapaca(&p, eventos, 16);
        CHECK(n == 1 && eventos[0].pedido == PEDIDO_SUBORNO_MESA && eventos[0].na_distribuicao);
        CHECK(!p.mao.carta_forcada_pendente);
        conferir_cartas_unicas(&p);

        if (eventos[0].resultado == TRAPACA_SUCESSO) {
            aplicadas++;
            CHECK(mesma_carta(p.mao.board[0], carta) && mesma_carta(eventos[0].cartas[0], carta));
        } else {
            falhas++;
            CHECK(eventos[0].resultado == TRAPACA_RECUSADA);
            CHECK(!mesma_carta(p.mao.board[0], carta));
        }

        partida_encerrar(&p);
    }

    CHECK(aplicadas >= 10 && falhas >= 1);
}

/* Mesmo gerador que partida.c usa para os sorteios do risco (splitmix64). */
static unsigned sortear(uint64_t *estado)
{
    uint64_t z;

    *estado += 0x9E3779B97F4A7C15ull;
    z = *estado;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (unsigned)(z >> 32);
}

/** Acha um estado do gerador cujos dois primeiros sorteios, módulo 100, valem os pedidos (-1 não importa). */
static uint64_t estado_com_sorteios(int primeiro, int segundo)
{
    for (uint64_t estado = 1; estado < 200000; estado++) {
        uint64_t e = estado;
        int a = (int)(sortear(&e) % 100u);
        int b = (int)(sortear(&e) % 100u);

        if ((primeiro < 0 || a == primeiro) && (segundo < 0 || b == segundo)) {
            return estado;
        }
    }

    CHECK(false);
    return 0;
}

static CheatResult tentar_com_estado(Partida *p, uint64_t estado, PedidoTrapaca pedido)
{
    CheatResult resultado = TRAPACA_SUCESSO;

    CHECK(partida_iniciar(p, 1));
    p->estado_do_risco = estado;
    p->estado_do_risco_da_mao = estado;
    CHECK(partida_trapacear(p, pedido, &resultado) == POKER_OK);
    partida_encerrar(p);
    return resultado;
}

static void test_limites_dos_sorteios_de_descoberta_e_recusa(void)
{
    static Partida p;
    PedidoTrapaca bebida = pedir(PEDIDO_BEBIDA, 0, SEM_CARTA);
    PedidoTrapaca ver = pedir(PEDIDO_SUBORNO_VER, 0, SEM_CARTA);

    /* Bebida contra o Agressivo sóbrio: 12%. O sorteio 11 (mais 1) ainda é pego; o 12, não. */
    CHECK(tentar_com_estado(&p, estado_com_sorteios(11, -1), bebida) == TRAPACA_DESCOBERTA);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(12, -1), bebida) == TRAPACA_SUCESSO);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(0, -1), bebida) == TRAPACA_DESCOBERTA);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(99, -1), bebida) == TRAPACA_SUCESSO);

    /* O dealer recusa quando o primeiro sorteio é menor que 30 (e a recusa vale antes da descoberta). */
    CHECK(tentar_com_estado(&p, estado_com_sorteios(29, 0), ver) == TRAPACA_RECUSADA);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(0, 99), ver) == TRAPACA_RECUSADA);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(30, 99), ver) == TRAPACA_SUCESSO);

    /* Sem recusa, a descoberta usa o segundo sorteio: 24% no suborno. */
    CHECK(tentar_com_estado(&p, estado_com_sorteios(30, 23), ver) == TRAPACA_DESCOBERTA);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(30, 24), ver) == TRAPACA_SUCESSO);
    CHECK(tentar_com_estado(&p, estado_com_sorteios(99, 0), ver) == TRAPACA_DESCOBERTA);
}

static void test_aviso_da_carta_forcada_sai_na_hora_nos_dois_caminhos(void)
{
    static Partida p;
    EventoTrapaca eventos[16];
    uint64_t estado;
    int pelo_jogador = 0;
    int pela_ia = 0;

    CHECK(partida_iniciar(&p, 6));
    estado = estado_que_da_certo(6, pedir(PEDIDO_SUBORNO_MESA, 0, carta_livre(&p, 0)));
    CHECK(estado != 0);

    for (unsigned semente = 6; semente <= 120; semente++) {
        for (int maos = 0; maos < 2; maos++) {
            CheatResult resultado;

            CHECK(partida_iniciar(&p, semente));

            if (maos == 1) {
                encerrar_mao(&p);
                CHECK(partida_continuar(&p));
            }

            while (p.situacao == PARTIDA_VEZ_DO_ADVERSARIO && p.mao.cartas_no_board == 0) {
                CHECK(partida_agir_adversario(&p, NULL) == POKER_OK);

                if (p.mao.cartas_no_board > 0) {
                    break;
                }
            }

            if (p.situacao != PARTIDA_VEZ_DO_JOGADOR || p.mao.cartas_no_board != 0) {
                partida_encerrar(&p);
                continue;
            }

            p.estado_do_risco = estado;
            p.estado_do_risco_da_mao = estado;

            if (partida_trapacear(&p, pedir(PEDIDO_SUBORNO_MESA, 0, carta_livre(&p, 5)), &resultado) !=
                    POKER_OK || resultado != TRAPACA_SUCESSO) {
                partida_encerrar(&p);
                continue;
            }

            CHECK(eventos_de_trapaca(&p, eventos, 16) == 1);

            /* O aviso sai na própria ação que distribui o flop, de quem for. */
            for (int passos = 0; passos < 20 && p.mao.situacao == MAO_EM_ANDAMENTO; passos++) {
                bool era_o_jogador = p.situacao == PARTIDA_VEZ_DO_JOGADOR;
                size_t antes = p.mao.cartas_no_board;

                if (era_o_jogador) {
                    PokerLegalActions legais;

                    CHECK(partida_acoes_legais(&p, &legais) == POKER_OK);
                    CHECK(partida_agir_jogador(&p, legais.check ? PASSAR : PAGAR) == POKER_OK);
                } else {
                    CHECK(partida_agir_adversario(&p, NULL) == POKER_OK);
                }

                if (antes == 0 && p.mao.cartas_no_board >= 3) {
                    CHECK(eventos_de_trapaca(&p, eventos, 16) == 1);
                    CHECK(eventos[0].pedido == PEDIDO_SUBORNO_MESA && eventos[0].na_distribuicao);
                    pelo_jogador += era_o_jogador ? 1 : 0;
                    pela_ia += era_o_jogador ? 0 : 1;
                    break;
                }
            }

            partida_encerrar(&p);
        }
    }

    CHECK(pelo_jogador > 0 && pela_ia > 0);
}

static void test_suborno_poe_a_carta_na_mao_da_proxima_mao(void)
{
    static Partida p;
    EventoTrapaca eventos[16];
    uint64_t estado;
    int aplicadas = 0;
    int falhas = 0;

    CHECK(partida_iniciar(&p, 6));
    estado = estado_que_da_certo(6, pedir(PEDIDO_SUBORNO_MAO, 0, carta_livre(&p, 0)));
    CHECK(estado != 0);

    for (int volta = 0; volta < 160; volta++) {
        unsigned semente = 6u + (unsigned)(volta / 40);
        CheatResult resultado;
        PokerCard carta;
        int n;

        CHECK(partida_iniciar(&p, semente));
        carta = carta_livre(&p, volta % 45);
        p.estado_do_risco = estado;
        p.estado_do_risco_da_mao = estado;
        CHECK(partida_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, carta), &resultado) == POKER_OK);

        if (resultado != TRAPACA_SUCESSO) {
            /* Outra semente muda a chance só pelo perfil: o estado serve, mas confira. */
            partida_encerrar(&p);
            continue;
        }

        CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 200);
        CHECK(p.cheats.carta_comprada_pendente && p.cheats.destino_carta_comprada == 1);
        CHECK(mesma_carta(p.cheats.carta_comprada, carta));
        CHECK(partida_pode_trapacear(&p, pedir(PEDIDO_SUBORNO_MAO, 0, carta)) == POKER_ERR_ACTION);
        CHECK(eventos_de_trapaca(&p, eventos, 16) == 1);

        /* A mão atual não muda; a carta chega na próxima. */
        encerrar_mao(&p);
        CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
        CHECK(p.cheats.carta_comprada_pendente);
        CHECK(partida_continuar(&p));
        CHECK(p.cheats_da_mao.carta_comprada_pendente);
        CHECK(!p.cheats.carta_comprada_pendente && p.cheats.destino_carta_comprada == -1);
        CHECK(p.mao.baralho.count == 47);
        conferir_cartas_unicas(&p);

        n = eventos_de_trapaca(&p, eventos, 16);
        CHECK(n == 1 && eventos[0].pedido == PEDIDO_SUBORNO_MAO && eventos[0].na_distribuicao);

        if (eventos[0].resultado == TRAPACA_SUCESSO) {
            aplicadas++;
            CHECK(p.mao.carta_do_jogador == MAO_AJUSTE_APLICADO);
            CHECK(mesma_carta(p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][0], carta));
            CHECK(mesma_carta(eventos[0].cartas[0], carta));
        } else {
            falhas++;
            CHECK(eventos[0].resultado == TRAPACA_RECUSADA);
            CHECK(p.mao.carta_do_jogador == MAO_AJUSTE_FALHOU);
            CHECK(!mesma_carta(p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][0], carta));
        }

        partida_encerrar(&p);
    }

    CHECK(aplicadas >= 40 && falhas >= 1);
}

static void test_manga_fica_fora_do_baralho_e_troca_com_a_mao(void)
{
    static Partida p;
    EventoTrapaca eventos[8];
    PokerCard manga;
    PokerCard na_mao;

    /* A manga começa com o 2 de paus, que nunca sai do baralho para a mesa. */
    for (unsigned semente = 1; semente <= 40; semente++) {
        CHECK(partida_iniciar(&p, semente));
        CHECK(mesma_carta(p.cheats.carta_na_manga, DOIS_DE_PAUS));
        CHECK(!deck_contains(&p.mao.baralho, DOIS_DE_PAUS));
        CHECK(p.mao.baralho.count == 47);

        for (int mao = 0; mao < 6; mao++) {
            conferir_cartas_unicas(&p);

            for (int i = 0; i < 2; i++) {
                CHECK(!mesma_carta(p.mao.cartas[0][i], DOIS_DE_PAUS));
                CHECK(!mesma_carta(p.mao.cartas[1][i], DOIS_DE_PAUS));
            }

            encerrar_mao(&p);
            CHECK(partida_continuar(&p));
        }

        partida_encerrar(&p);
    }

    /* Trocar: a carta da mão vai para a manga e a manga entra na mão. */
    CHECK(tentar(&p, 7, pedir(PEDIDO_MANGA, 1, SEM_CARTA), TRAPACA_SUCESSO, false));
    na_mao = p.cheats.carta_na_manga;
    CHECK(mesma_carta(p.mao.cartas[TORNEIO_ASSENTO_JOGADOR][1], DOIS_DE_PAUS));
    CHECK(!mesma_carta(na_mao, DOIS_DE_PAUS));
    CHECK(p.cheats.as_na_manga_usado);
    CHECK(p.risco.usos_trapaca[PROB_TRAPACA_CARTA_MANGA] == 1);
    conferir_cartas_unicas(&p);
    CHECK(eventos_de_trapaca(&p, eventos, 8) == 1);
    CHECK(eventos[0].quantidade_de_cartas == 2);
    CHECK(mesma_carta(eventos[0].cartas[0], DOIS_DE_PAUS) && mesma_carta(eventos[0].cartas[1], na_mao));
    CHECK(p.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990);

    /* A carta guardada vale para a mão seguinte e fica fora do baralho dela. */
    manga = p.cheats.carta_na_manga;
    encerrar_mao(&p);
    CHECK(partida_continuar(&p));
    CHECK(mesma_carta(p.cheats.carta_na_manga, manga));
    CHECK(!deck_contains(&p.mao.baralho, manga));
    CHECK(p.mao.baralho.count == 47);
    conferir_cartas_unicas(&p);
    partida_encerrar(&p);
}

static void test_novo_confronto_limpa_a_mesa_e_guarda_a_manga(void)
{
    static Partida p;
    bool achou = false;

    for (unsigned semente = 1; semente <= 80 && !achou; semente++) {
        EventoTrapaca eventos[16];
        int passos = 0;

        CHECK(partida_iniciar(&p, semente));
        p.risco.balas_roleta = 3;
        p.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] = 3;

        while (passos++ < LIMITE_DE_PASSOS && p.situacao != PARTIDA_CONFRONTO_VENCIDO &&
               p.situacao != PARTIDA_ROLETA && p.situacao != PARTIDA_JOGADOR_FALIDO &&
               p.situacao != PARTIDA_CAMPEAO) {
            if (p.situacao == PARTIDA_VEZ_DO_JOGADOR) {
                PokerLegalActions legais;

                CHECK(partida_acoes_legais(&p, &legais) == POKER_OK);
                CHECK(partida_agir_jogador(&p, legais.all_in ? ALL_IN : (legais.check ? PASSAR : PAGAR)) ==
                      POKER_OK);
            } else if (p.situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
                CHECK(partida_agir_adversario(&p, NULL) == POKER_OK);
            } else {
                CHECK(partida_continuar(&p));
            }

            /* Estado de uma mesa em plena atividade. */
            if (p.situacao == PARTIDA_VEZ_DO_JOGADOR || p.situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
                p.cheats.maos_embriagado = 3;
                p.cheats.usos_cumplice_blind = 4;
                p.cheats.suborno_usado_na_mao = true;
                p.cheats.cartas_marcadas[2] = (PokerCard){9, POKER_HEARTS};
                p.cheats.turnos_marcacao[2] = 4;
                p.adversario_embriagado = true;
            }
        }

        if (p.situacao != PARTIDA_CONFRONTO_VENCIDO) {
            partida_encerrar(&p);
            continue;
        }

        achou = true;

        /* O suborno da mão que ainda não chegou atravessa o confronto. */
        p.cheats.carta_comprada_pendente = true;
        p.cheats.destino_carta_comprada = 1;
        p.cheats.carta_comprada = (PokerCard){9, POKER_DIAMONDS};
        fila_eventos_iniciar(&p.eventos);

        CHECK(partida_continuar(&p));
        CHECK(p.torneio.confronto == 1 && p.ia.style == POKER_AI_BALANCED);
        CHECK(p.cheats.maos_embriagado == 0 && !p.adversario_embriagado);
        CHECK(p.cheats.usos_cumplice_blind == 0 && !p.cheats.suborno_usado_na_mao);

        for (int i = 0; i < 5; i++) {
            CHECK(p.cheats.cartas_marcadas[i].rank == 0 && p.cheats.turnos_marcacao[i] == 0);
        }

        CHECK(mesma_carta(p.cheats.carta_na_manga, DOIS_DE_PAUS));
        CHECK(p.risco.balas_roleta == 2);

        for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
            CHECK(p.risco.usos_trapaca[i] == 0);
        }

        CHECK(p.cheats_da_mao.carta_comprada_pendente);
        CHECK(!p.cheats.carta_comprada_pendente);
        CHECK(ha_evento(eventos, eventos_de_trapaca(&p, eventos, 16), PEDIDO_SUBORNO_MAO, TRAPACA_SUCESSO, true) ||
              ha_evento(eventos, 1, PEDIDO_SUBORNO_MAO, TRAPACA_RECUSADA, true));
        partida_encerrar(&p);
    }

    CHECK(achou);
}

static void test_usos_do_cumplice_recomecam_a_cada_nivel_de_blind(void)
{
    static Partida p;
    PokerChips blind_antes;

    CHECK(partida_iniciar(&p, 8));
    CHECK(p.mao.mesa.big_blind == 20);
    encerrar_mao(&p);
    p.cheats.usos_cumplice_blind = 3;

    for (int mao = 1; mao <= 11; mao++) {
        CHECK(partida_continuar(&p));
        blind_antes = p.mao.mesa.big_blind;

        if (p.torneio.maos_jogadas % TORNEIO_MAOS_POR_NIVEL == 0) {
            CHECK(p.cheats.usos_cumplice_blind == 0);
            CHECK(p.cheats_da_mao.usos_cumplice_blind == 0);
            CHECK(partida_custo_da_trapaca(&p, PEDIDO_CUMPLICE) == blind_antes);
            p.cheats.usos_cumplice_blind = 3;
        } else {
            CHECK(p.cheats.usos_cumplice_blind == 3);
            CHECK(partida_custo_da_trapaca(&p, PEDIDO_CUMPLICE) == blind_antes * 4);
        }

        encerrar_mao(&p);
        CHECK(p.situacao == PARTIDA_MAO_ENCERRADA);
    }

    CHECK(p.torneio.maos_jogadas == 12);
    partida_encerrar(&p);
}

typedef struct {
    int sucessos[PEDIDO_TOTAL];
    int descobertas;
    int recusadas;
    int pedidos_recusados_pela_regra;
    int marcadas_avisadas;
    int suborno_mao_aplicado;
    int suborno_mesa_aplicado;
    int roletas;
    int mortes;
    int restauracoes;
    int partidas;
} Estatisticas;

static PokerAction politica_aleatoria(const Partida *partida, unsigned *sorteio)
{
    PokerLegalActions legais;
    unsigned dado = proximo_aleatorio(sorteio) % 100;

    CHECK(partida_acoes_legais(partida, &legais) == POKER_OK);

    if (dado < 8) {
        return DESISTIR;
    }

    if (dado < 16 && legais.all_in) {
        return ALL_IN;
    }

    if (dado < 40 && (legais.bet || legais.raise)) {
        PokerChips faixa = legais.max_total - legais.min_total + 1;
        PokerChips total = legais.min_total +
            (PokerChips)(proximo_aleatorio(sorteio) % (unsigned)faixa);

        return (PokerAction){legais.bet ? POKER_BET : POKER_RAISE, total};
    }

    return legais.check ? PASSAR : PAGAR;
}

static PedidoTrapaca pedido_aleatorio(unsigned *sorteio)
{
    PedidoTrapaca pedido;

    pedido.tipo = (TipoDePedido)(1 + proximo_aleatorio(sorteio) % 7);
    pedido.carta_da_mao = (int)(proximo_aleatorio(sorteio) % 2);
    pedido.carta = (PokerCard){2 + (int)(proximo_aleatorio(sorteio) % 13),
                               (PokerSuit)(proximo_aleatorio(sorteio) % 4)};

    if (proximo_aleatorio(sorteio) % 25 == 0) {
        pedido.carta_da_mao = 5;
    }

    return pedido;
}

static void conferir_cheats_iguais(const CheatState *a, const CheatState *b)
{
    CHECK(a->maos_embriagado == b->maos_embriagado);
    CHECK(a->usos_cumplice_blind == b->usos_cumplice_blind);
    CHECK(a->suborno_usado_na_mao == b->suborno_usado_na_mao);
    CHECK(a->as_na_manga_usado == b->as_na_manga_usado);
    CHECK(mesma_carta(a->carta_na_manga, b->carta_na_manga));
    CHECK(a->carta_comprada_pendente == b->carta_comprada_pendente);
    CHECK(a->destino_carta_comprada == b->destino_carta_comprada);
    CHECK(a->carta_comprada.rank == b->carta_comprada.rank);

    for (int i = 0; i < 5; i++) {
        CHECK(a->cartas_marcadas[i].rank == b->cartas_marcadas[i].rank);
        CHECK(a->turnos_marcacao[i] == b->turnos_marcacao[i]);
    }
}

static void conferir_partidas_iguais(const Partida *a, const Partida *b)
{
    CHECK(a->situacao == b->situacao);
    CHECK(a->roleta_motivo == b->roleta_motivo);
    CHECK(a->torneio.confronto == b->torneio.confronto);
    CHECK(a->torneio.fichas_jogador == b->torneio.fichas_jogador);
    CHECK(a->torneio.fichas_adversario == b->torneio.fichas_adversario);
    CHECK(a->torneio.maos_jogadas == b->torneio.maos_jogadas);
    CHECK(a->torneio.recuperacoes == b->torneio.recuperacoes);
    CHECK(a->semente_da_mao == b->semente_da_mao);
    CHECK(a->estado_das_sementes == b->estado_das_sementes);
    CHECK(a->estado_do_risco == b->estado_do_risco);
    CHECK(a->estado_do_risco_da_mao == b->estado_do_risco_da_mao);
    CHECK(a->rng_acao.state == b->rng_acao.state);
    CHECK(a->rng_equidade.state == b->rng_equidade.state);
    CHECK(a->adversario_embriagado == b->adversario_embriagado);
    CHECK(a->risco.balas_roleta == b->risco.balas_roleta);
    CHECK(a->risco_da_mao.balas_roleta == b->risco_da_mao.balas_roleta);

    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        CHECK(a->risco.usos_trapaca[i] == b->risco.usos_trapaca[i]);
        CHECK(a->risco_da_mao.usos_trapaca[i] == b->risco_da_mao.usos_trapaca[i]);
    }

    conferir_cheats_iguais(&a->cheats, &b->cheats);
    conferir_cheats_iguais(&a->cheats_da_mao, &b->cheats_da_mao);
    CHECK(a->quantidade_no_registro == b->quantidade_no_registro);

    for (size_t i = 0; i < a->quantidade_no_registro; i++) {
        CHECK(a->registro[i].tipo == b->registro[i].tipo);

        if (a->registro[i].tipo == REGISTRO_ACAO) {
            CHECK(a->registro[i].acao.type == b->registro[i].acao.type);
            CHECK(a->registro[i].acao.total == b->registro[i].acao.total);
        } else if (a->registro[i].tipo == REGISTRO_TRAPACA) {
            CHECK(a->registro[i].pedido.tipo == b->registro[i].pedido.tipo);
            CHECK(a->registro[i].pedido.carta_da_mao == b->registro[i].pedido.carta_da_mao);
        }
    }

    CHECK(a->mao.situacao == b->mao.situacao);
    CHECK(a->mao.cartas_no_board == b->mao.cartas_no_board);
    CHECK(a->mao.carta_forcada_pendente == b->mao.carta_forcada_pendente);
    CHECK(a->mao.baralho.count == b->mao.baralho.count);
    CHECK(a->mao.mesa.initial_total == b->mao.mesa.initial_total);
    CHECK(a->mao.mesa.actor == b->mao.mesa.actor);
    CHECK(a->mao.mesa.street == b->mao.mesa.street);

    for (int assento = 0; assento < 2; assento++) {
        CHECK(a->mao.mesa.player[assento].chips == b->mao.mesa.player[assento].chips);
        CHECK(a->mao.mesa.player[assento].committed == b->mao.mesa.player[assento].committed);

        for (int i = 0; i < 2; i++) {
            CHECK(mesma_carta(a->mao.cartas[assento][i], b->mao.cartas[assento][i]));
        }
    }

    for (size_t i = 0; i < a->mao.cartas_no_board; i++) {
        CHECK(mesma_carta(a->mao.board[i], b->mao.board[i]));
    }
}

static void contar_eventos(Partida *p, Estatisticas *e)
{
    EventoJogo evento;

    while (fila_eventos_retirar(&p->eventos, &evento)) {
        if (evento.tipo == EVENTO_TRAPACA) {
            const EventoTrapaca *t = &evento.trapaca;

            if (t->na_distribuicao && t->resultado == TRAPACA_SUCESSO) {
                e->marcadas_avisadas += t->pedido == PEDIDO_MARCAR_CARTA ? 1 : 0;
                e->suborno_mao_aplicado += t->pedido == PEDIDO_SUBORNO_MAO ? 1 : 0;
                e->suborno_mesa_aplicado += t->pedido == PEDIDO_SUBORNO_MESA ? 1 : 0;
            }
        }
    }
}

static void conferir_invariantes(const Partida *p)
{
    conferir_cartas_unicas(p);

    if (p->mao.situacao == MAO_EM_ANDAMENTO) {
        CHECK(p->mao.mesa.player[0].chips + p->mao.mesa.player[1].chips + poker_pot(&p->mao.mesa) ==
              p->mao.mesa.initial_total);
        CHECK(p->mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips >= 0);
    }

    CHECK(p->adversario_embriagado == (p->cheats.maos_embriagado > 0));
}

/** Guarda a partida por texto e a refaz em outra, que passa a ser igual à primeira. */
static void passar_por_texto(const Partida *origem, Partida *destino)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    PartidaSalva lida;
    size_t tamanho;

    CHECK(partida_exportar(origem, &guardada));
    tamanho = salvamento_formatar(&guardada, texto, sizeof texto);
    CHECK(tamanho > 0);
    CHECK(salvamento_interpretar(texto, tamanho, &lida));
    CHECK(partida_restaurar(destino, &lida));
    conferir_partidas_iguais(origem, destino);
}

/** Faz as duas partidas darem o mesmo passo; false quando a partida acabou. */
static bool passo_juntas(Partida *a, Partida *b, unsigned *sorteio, Estatisticas *e)
{
    PokerChips soma_antes = a->torneio.fichas_jogador + a->torneio.fichas_adversario;
    int confronto_antes = a->torneio.confronto;
    int recuperacoes_antes = a->torneio.recuperacoes;

    switch (a->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            if (proximo_aleatorio(sorteio) % 100 < 40) {
                PedidoTrapaca pedido = pedido_aleatorio(sorteio);
                CheatResult ra = TRAPACA_SUCESSO;
                CheatResult rb = TRAPACA_SUCESSO;
                PokerStatus sa = partida_trapacear(a, pedido, &ra);
                PokerStatus sb = partida_trapacear(b, pedido, &rb);

                CHECK(sa == sb);

                if (sa != POKER_OK) {
                    e->pedidos_recusados_pela_regra++;
                    break;
                }

                CHECK(ra == rb);

                if (ra == TRAPACA_SUCESSO) {
                    e->sucessos[pedido.tipo]++;
                } else if (ra == TRAPACA_DESCOBERTA) {
                    e->descobertas++;
                } else {
                    e->recusadas++;
                }
            } else {
                PokerAction acao = politica_aleatoria(a, sorteio);

                CHECK(partida_agir_jogador(a, acao) == POKER_OK);
                CHECK(partida_agir_jogador(b, acao) == POKER_OK);
            }

            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            CHECK(partida_agir_adversario(a, NULL) == POKER_OK);
            CHECK(partida_agir_adversario(b, NULL) == POKER_OK);
            break;

        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
            CHECK(partida_continuar(a));
            CHECK(partida_continuar(b));
            break;

        case PARTIDA_ROLETA:
            e->roletas++;
            CHECK(partida_puxar_gatilho(a));
            CHECK(partida_puxar_gatilho(b));
            e->mortes += a->situacao == PARTIDA_JOGADOR_MORTO ? 1 : 0;
            break;

        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_JOGADOR_MORTO:
        case PARTIDA_CAMPEAO:
            return false;

        case PARTIDA_VAZIA:
            CHECK(false);
    }

    contar_eventos(a, e);
    contar_eventos(b, e);
    e->mortes += 0;
    conferir_partidas_iguais(a, b);
    conferir_invariantes(a);

    /* As fichas só saem de circulação, a não ser quando a roleta devolve o jogador ou o confronto muda. */
    if (a->torneio.confronto == confronto_antes && a->torneio.recuperacoes == recuperacoes_antes &&
        a->torneio.situacao == TORNEIO_EM_ANDAMENTO) {
        CHECK(a->torneio.fichas_jogador + a->torneio.fichas_adversario <= soma_antes);
    }

    return true;
}

static void test_tudo_se_refaz_pelo_registro_e_mantem_as_invariantes(void)
{
    static Partida a;
    static Partida b;
    Estatisticas e = {{0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        unsigned sorteio = semente * 7919u + 13u;
        int passos = 0;

        CHECK(partida_iniciar(&a, semente));
        passar_por_texto(&a, &b);
        e.restauracoes++;

        while (passo_juntas(&a, &b, &sorteio, &e)) {
            CHECK(++passos < LIMITE_DE_PASSOS);

            /* De vez em quando, a segunda partida é trocada por uma refeita do texto da primeira. */
            if (passos % 4 == 0 && a.situacao != PARTIDA_JOGADOR_FALIDO &&
                a.situacao != PARTIDA_JOGADOR_MORTO && a.situacao != PARTIDA_CAMPEAO) {
                PartidaSalva tentativa;

                if (partida_exportar(&a, &tentativa)) {
                    passar_por_texto(&a, &b);
                    e.restauracoes++;
                }
            }
        }

        e.partidas++;
        partida_encerrar(&a);
        partida_encerrar(&b);
    }

    for (int t = PEDIDO_BEBIDA; t < PEDIDO_TOTAL; t++) {
        CHECK(e.sucessos[t] > 0);
    }

    CHECK(e.descobertas > 0 && e.recusadas > 0 && e.pedidos_recusados_pela_regra > 0);
    CHECK(e.roletas > 0 && e.mortes > 0);
    CHECK(e.suborno_mao_aplicado > 0 && e.suborno_mesa_aplicado > 0 && e.marcadas_avisadas > 0);
    CHECK(e.restauracoes > 200 && e.partidas == SEMENTES);
}

int main(void)
{
    test_custo_e_chance_de_cada_trapaca();
    test_pedidos_invalidos_e_fora_de_hora_sao_recusados();
    test_recusa_nao_gasta_sorteio_nem_registra();
    test_bebida_embriaga_por_tres_maos();
    test_descoberta_leva_a_roleta_e_a_mao_segue_se_sobreviver();
    test_cumplice_cobra_do_stack_e_revela_a_categoria();
    test_marcar_carta_dura_cinco_maos_e_avisa_quando_cai_com_o_adversario();
    test_suborno_ver_mostra_as_cartas_do_adversario();
    test_dealer_recusa_trinta_por_cento_e_fica_com_o_dinheiro();
    test_suborno_poe_a_carta_na_mesa_ou_o_dealer_nao_acha();
    test_aviso_da_carta_forcada_sai_na_hora_nos_dois_caminhos();
    test_limites_dos_sorteios_de_descoberta_e_recusa();
    test_suborno_poe_a_carta_na_mao_da_proxima_mao();
    test_manga_fica_fora_do_baralho_e_troca_com_a_mao();
    test_novo_confronto_limpa_a_mesa_e_guarda_a_manga();
    test_usos_do_cumplice_recomecam_a_cada_nivel_de_blind();
    test_tudo_se_refaz_pelo_registro_e_mantem_as_invariantes();
    puts("OK: trapacas na partida (custos, chances, regras, efeitos, descoberta e roleta, e partidas inteiras refeitas pelo registro).");
    return EXIT_SUCCESS;
}
