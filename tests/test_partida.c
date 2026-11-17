#include "core/partida.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 40
#define LIMITE_DE_PASSOS 200000

typedef PokerAction (*Politica)(const Partida *partida, unsigned *sorteio);

typedef struct {
    int maos;
    int potes_pagos;
    int confrontos_vencidos;
    int falencias;
    int roletas;
    int recuperacoes;
    int mortes;
    int falidos;
    int campeoes;
    int passos;
    PokerChips fichas_jogador;
    PokerChips fichas_adversario;
    int confronto;
} Resultado;

static const PokerAction DESISTIR = {POKER_FOLD, 0};
static const PokerAction PASSAR = {POKER_CHECK, 0};
static const PokerAction PAGAR = {POKER_CALL, 0};
static const PokerAction ALL_IN = {POKER_ALL_IN, 0};

static unsigned proximo_aleatorio(unsigned *semente)
{
    *semente = *semente * 1664525u + 1013904223u;
    return *semente >> 8;
}

/**
 * O estilo que a IA deve ter numa mão: o de base do confronto, trocado pelo de
 * quando está perdendo ou ganhando. Escrito à parte da implementação, com uma
 * tabela, para que o teste não repita o que ela faz.
 */
static PokerAIStyle estilo_esperado(int confronto, PokerChips fichas_adversario)
{
    static const PokerChips INICIAL[TORNEIO_CONFRONTOS] = {2000, 4000, 8000};
    static const PokerAIStyle BASE[TORNEIO_CONFRONTOS] = {
        POKER_AI_AGGRESSIVE, POKER_AI_CAUTIOUS, POKER_AI_BALANCED
    };
    static const PokerAIStyle PERDENDO[TORNEIO_CONFRONTOS] = {
        POKER_AI_CAUTIOUS, POKER_AI_CAUTIOUS, POKER_AI_CAUTIOUS
    };
    static const PokerAIStyle GANHANDO[TORNEIO_CONFRONTOS] = {
        POKER_AI_AGGRESSIVE, POKER_AI_AGGRESSIVE, POKER_AI_AGGRESSIVE
    };
    PokerChips inicial = INICIAL[confronto];

    if (fichas_adversario * 100 <= inicial * PARTIDA_ESTILO_ATRAS_PCT) {
        return PERDENDO[confronto];
    }

    if (fichas_adversario * 100 >= inicial * PARTIDA_ESTILO_FRENTE_PCT) {
        return GANHANDO[confronto];
    }

    return BASE[confronto];
}

static PokerAIStyle estilo_com(int confronto, PokerChips fichas_adversario)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    torneio.confronto = confronto;
    torneio.fichas_adversario = fichas_adversario;
    return partida_estilo_do_adversario(&torneio);
}

static PokerAction politica_passiva(const Partida *partida, unsigned *sorteio)
{
    PokerLegalActions legais;

    (void)sorteio;
    CHECK(partida_acoes_legais(partida, &legais) == POKER_OK);
    return legais.check ? PASSAR : PAGAR;
}

static PokerAction politica_all_in(const Partida *partida, unsigned *sorteio)
{
    PokerLegalActions legais;

    (void)sorteio;
    CHECK(partida_acoes_legais(partida, &legais) == POKER_OK);

    if (legais.all_in) {
        return ALL_IN;
    }

    return legais.check ? PASSAR : PAGAR;
}

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

static void drenar_eventos(Partida *partida, Resultado *resultado)
{
    EventoJogo evento;

    while (fila_eventos_retirar(&partida->eventos, &evento)) {
        if (evento.tipo == EVENTO_POTE_PAGO) {
            resultado->potes_pagos++;
        }
    }

    CHECK(partida->eventos.perdidos == 0);
}

static Resultado jogar_ate_o_fim(unsigned semente, Politica politica, bool embriagado)
{
    Partida partida = {0};
    Resultado resultado = {0};
    unsigned sorteio = semente * 7919u + 13u;
    PokerChips total;
    bool terminou = false;
    bool mao_ja_contada = false;

    CHECK(partida_iniciar(&partida, semente));
    partida.adversario_embriagado = embriagado;
    partida.cheats.maos_embriagado = embriagado ? PARTIDA_USOS_MAXIMOS : 0;
    total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;

    while (!terminou) {
        PokerAIDecisionInfo info;

        CHECK(++resultado.passos < LIMITE_DE_PASSOS);
        drenar_eventos(&partida, &resultado);
        CHECK(partida.ia.style == estilo_esperado(partida.torneio_da_mao.confronto,
                                                  partida.torneio_da_mao.fichas_adversario));

        switch (partida.situacao) {
            case PARTIDA_VEZ_DO_JOGADOR:
                CHECK(partida.mao.situacao == MAO_EM_ANDAMENTO);
                CHECK(partida_agir_jogador(&partida, politica(&partida, &sorteio)) == POKER_OK);
                break;

            case PARTIDA_VEZ_DO_ADVERSARIO:
                CHECK(partida.mao.situacao == MAO_EM_ANDAMENTO);
                CHECK(partida_agir_adversario(&partida, &info) == POKER_OK);
                CHECK(info.profile == partida.ia.style);
                break;

            case PARTIDA_MAO_ENCERRADA:
                if (!mao_ja_contada) {
                    resultado.maos++;
                }

                mao_ja_contada = false;
                CHECK(partida.mao.situacao == MAO_REGISTRADA);
                CHECK(partida.torneio.fichas_jogador + partida.torneio.fichas_adversario == total);
                CHECK(partida_continuar(&partida));
                break;

            case PARTIDA_CONFRONTO_VENCIDO:
            {
                int confronto = partida.torneio.confronto;

                resultado.maos++;
                resultado.confrontos_vencidos++;
                CHECK(partida.torneio.fichas_adversario == 0);
                CHECK(partida_continuar(&partida));
                CHECK(partida.torneio.confronto == confronto + 1);
                total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;
                break;
            }

            case PARTIDA_ROLETA:
            {
                int confronto = partida.torneio.confronto;
                int recuperacoes = partida.torneio.recuperacoes;
                int balas = partida.risco.balas_roleta;

                resultado.maos++;
                resultado.falencias++;
                resultado.roletas++;
                CHECK(partida.roleta_motivo == ROLETA_FALENCIA);
                CHECK(partida.torneio.fichas_jogador == 0);
                CHECK(partida.torneio.recuperacoes < PARTIDA_RECUPERACOES_MAXIMAS);
                CHECK(balas >= 1 && balas <= 6);
                CHECK(partida_puxar_gatilho(&partida));
                CHECK(partida.roleta_motivo == ROLETA_NENHUMA);

                if (partida.situacao == PARTIDA_JOGADOR_MORTO) {
                    CHECK(partida.risco.balas_roleta == balas);
                    break;
                }

                CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
                CHECK(balas < 6);
                CHECK(partida.risco.balas_roleta == balas + 1);
                CHECK(partida.torneio.recuperacoes == recuperacoes + 1);
                CHECK(partida.torneio.fichas_jogador ==
                      torneio_fichas_iniciais_adversario(confronto) / 2);
                total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;
                resultado.recuperacoes++;
                mao_ja_contada = true;
                break;
            }

            case PARTIDA_JOGADOR_MORTO:
                resultado.mortes++;
                terminou = true;
                break;

            case PARTIDA_JOGADOR_FALIDO:
                resultado.maos++;
                resultado.falencias++;
                resultado.falidos++;
                CHECK(partida.torneio.fichas_jogador == 0);
                CHECK(partida.torneio.recuperacoes == PARTIDA_RECUPERACOES_MAXIMAS);
                terminou = true;
                break;

            case PARTIDA_CAMPEAO:
                resultado.maos++;
                resultado.campeoes++;
                CHECK(partida.torneio.confronto == TORNEIO_CONFRONTOS - 1);
                terminou = true;
                break;

            case PARTIDA_VAZIA:
                CHECK(false);
        }
    }

    drenar_eventos(&partida, &resultado);
    resultado.fichas_jogador = partida.torneio.fichas_jogador;
    resultado.fichas_adversario = partida.torneio.fichas_adversario;
    resultado.confronto = partida.torneio.confronto;

    CHECK(!partida_continuar(&partida));
    CHECK(!partida_puxar_gatilho(&partida));
    CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_ERR_STATE);
    CHECK(partida_agir_adversario(&partida, NULL) == POKER_ERR_STATE);
    partida_encerrar(&partida);
    return resultado;
}

static void test_primeira_mao_comeca_na_vez_do_jogador(void)
{
    Partida partida = {0};
    PokerLegalActions legais;

    CHECK(partida_iniciar(&partida, 1));
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(partida.torneio.confronto == 0);
    CHECK(partida.ia.style == POKER_AI_AGGRESSIVE);
    CHECK(partida.ia.action_rng == &partida.rng_acao);
    CHECK(partida.ia.equity_rng == &partida.rng_equidade);
    CHECK(!partida.adversario_embriagado);
    CHECK(partida.mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(partida.mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    CHECK(fila_eventos_tamanho(&partida.eventos) == 3);

    CHECK(partida_acoes_legais(&partida, &legais) == POKER_OK);
    CHECK(legais.fold && legais.call && !legais.check);

    partida_encerrar(&partida);
    CHECK(partida.situacao == PARTIDA_VAZIA);
    partida_encerrar(&partida);
    partida_encerrar(NULL);
}

static void test_nomes_dos_adversarios(void)
{
    CHECK(strcmp(partida_nome_do_adversario(0), "Agressivo") == 0);
    CHECK(strcmp(partida_nome_do_adversario(1), "Medroso") == 0);
    CHECK(strcmp(partida_nome_do_adversario(2), "Desconfiado") == 0);
    CHECK(strcmp(partida_nome_do_adversario(-1), "?") == 0);
    CHECK(strcmp(partida_nome_do_adversario(TORNEIO_CONFRONTOS), "?") == 0);
}

static void test_recusas_de_turno_e_de_estado(void)
{
    Partida vazia = {0};
    Partida partida = {0};
    PokerLegalActions legais;

    CHECK(!partida_iniciar(NULL, 1));
    CHECK(partida_agir_jogador(NULL, PAGAR) == POKER_ERR_ARGUMENT);
    CHECK(partida_agir_adversario(NULL, NULL) == POKER_ERR_ARGUMENT);
    CHECK(!partida_continuar(NULL));
    CHECK(partida_acoes_legais(NULL, &legais) == POKER_ERR_ARGUMENT);
    CHECK(partida_acoes_legais(&vazia, NULL) == POKER_ERR_ARGUMENT);

    CHECK(partida_agir_jogador(&vazia, PAGAR) == POKER_ERR_STATE);
    CHECK(partida_agir_adversario(&vazia, NULL) == POKER_ERR_STATE);
    CHECK(!partida_continuar(&vazia));
    CHECK(partida_acoes_legais(&vazia, &legais) == POKER_ERR_STATE);

    CHECK(partida_iniciar(&partida, 2));
    CHECK(partida_agir_adversario(&partida, NULL) == POKER_ERR_TURN);
    CHECK(!partida_continuar(&partida));
    CHECK(partida_agir_jogador(&partida, PASSAR) == POKER_ERR_ACTION);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(partida.mao.mesa.player[0].chips == 1990);

    CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_OK);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO);
    CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_ERR_TURN);
    CHECK(partida_acoes_legais(&partida, &legais) == POKER_ERR_TURN);

    partida_encerrar(&partida);
}

static void test_desistencia_registra_no_torneio_e_continua(void)
{
    Partida partida = {0};
    PokerAIDecisionInfo info;

    CHECK(partida_iniciar(&partida, 3));
    CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);

    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(partida.mao.situacao == MAO_REGISTRADA);
    CHECK(partida.torneio.maos_jogadas == 1);
    CHECK(partida.torneio.fichas_jogador == 1990);
    CHECK(partida.torneio.fichas_adversario == 2010);
    CHECK(partida.torneio.dealer == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_ERR_STATE);

    CHECK(partida_continuar(&partida));
    CHECK(partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO);
    CHECK(partida.mao.mesa.dealer == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(partida.mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(!partida_continuar(&partida));

    CHECK(partida_agir_adversario(&partida, &info) == POKER_OK);
    CHECK(info.profile == POKER_AI_AGGRESSIVE);
    CHECK(partida.situacao != PARTIDA_VEZ_DO_ADVERSARIO ||
          partida.mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO);

    partida_encerrar(&partida);
}

static void test_stack_curto_resolve_a_mao_ao_iniciar(void)
{
    Partida partida = {0};
    EventoJogo evento;
    bool pote_pago = false;

    CHECK(partida_iniciar(&partida, 4));
    CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);

    partida.torneio.fichas_jogador = 10;
    partida.torneio.fichas_adversario = 1990;
    fila_eventos_iniciar(&partida.eventos);

    CHECK(partida_continuar(&partida));
    CHECK(partida.mao.situacao == MAO_REGISTRADA);
    CHECK(partida.mao.cartas_no_board == MAO_CARTAS_NO_BOARD);
    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA ||
          partida.situacao == PARTIDA_ROLETA ||
          partida.situacao == PARTIDA_CONFRONTO_VENCIDO);
    CHECK(partida.torneio.fichas_jogador + partida.torneio.fichas_adversario == 2000);

    while (fila_eventos_retirar(&partida.eventos, &evento)) {
        if (evento.tipo == EVENTO_POTE_PAGO) {
            pote_pago = true;
        }
    }

    CHECK(pote_pago);
    partida_encerrar(&partida);
}

static void test_comandos_e_ajuste_de_aposta(void)
{
    Partida partida = {0};
    PokerLegalActions legais;

    CHECK(partida_iniciar(&partida, 11));
    CHECK(partida_comando_jogador(NULL, COMANDO_DESISTIR, 0) == POKER_ERR_ARGUMENT);
    CHECK(partida_comando_jogador(&partida, COMANDO_NENHUM, 0) == POKER_ERR_ARGUMENT);
    CHECK(partida_ajustar_aposta(NULL, 0, 0) == 0);

    CHECK(partida_acoes_legais(&partida, &legais) == POKER_OK);
    CHECK(legais.raise && legais.min_total == 40 && legais.max_total == 2000);

    CHECK(partida_ajustar_aposta(&partida, 0, 0) == 40);
    CHECK(partida_ajustar_aposta(&partida, 0, 5) == 40);
    CHECK(partida_ajustar_aposta(&partida, 40, 0) == 40);
    CHECK(partida_ajustar_aposta(&partida, 40, 1) == 60);
    CHECK(partida_ajustar_aposta(&partida, 100, 3) == 160);
    CHECK(partida_ajustar_aposta(&partida, 100, -1) == 80);
    CHECK(partida_ajustar_aposta(&partida, 100, -50) == 40);
    CHECK(partida_ajustar_aposta(&partida, 100, 5000) == 2000);
    CHECK(partida_ajustar_aposta(&partida, 3000, 0) == 2000);

    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 25) == POKER_ERR_ACTION);
    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 2001) == POKER_ERR_ACTION);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(partida.mao.mesa.player[0].chips == 1990);

    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 60) == POKER_OK);
    CHECK(partida.mao.mesa.player[0].street_bet == 60);
    CHECK(partida.mao.mesa.player[0].chips == 1940);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO);

    CHECK(partida_ajustar_aposta(&partida, 60, 1) == 0);
    CHECK(partida_comando_jogador(&partida, COMANDO_PASSAR_OU_PAGAR, 0) == POKER_ERR_TURN);
    partida_encerrar(&partida);
    CHECK(partida_comando_jogador(&partida, COMANDO_ALL_IN, 0) == POKER_ERR_STATE);

    CHECK(partida_iniciar(&partida, 12));
    CHECK(partida_comando_jogador(&partida, COMANDO_ALL_IN, 0) == POKER_OK);
    CHECK(partida.mao.mesa.player[0].chips == 0);
    partida_encerrar(&partida);

    CHECK(partida_iniciar(&partida, 13));
    CHECK(partida_comando_jogador(&partida, COMANDO_DESISTIR, 0) == POKER_OK);
    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(partida.torneio.fichas_jogador == 1990);
    partida_encerrar(&partida);
}

static void test_passar_pagar_e_apostar_sem_aposta_anterior(void)
{
    Partida partida = {0};
    PokerChips stacks[2] = {2000, 2000};
    EventoJogo evento;
    bool viu_check = false;

    CHECK(partida_iniciar(&partida, 14));
    CHECK(poker_start_hand(&partida.mao.mesa, stacks, TORNEIO_ASSENTO_ADVERSARIO, 10, 20) == POKER_OK);
    CHECK(poker_act(&partida.mao.mesa, TORNEIO_ASSENTO_ADVERSARIO, PAGAR) == POKER_OK);
    partida.situacao = PARTIDA_VEZ_DO_JOGADOR;
    fila_eventos_iniciar(&partida.eventos);

    CHECK(partida.mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    CHECK(partida_comando_jogador(&partida, COMANDO_PASSAR_OU_PAGAR, 0) == POKER_OK);
    CHECK(partida.mao.cartas_no_board == 3);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);

    while (fila_eventos_retirar(&partida.eventos, &evento)) {
        if (evento.tipo == EVENTO_ACAO && evento.acao.acao == POKER_CHECK) {
            viu_check = true;
        }
    }

    CHECK(viu_check);
    CHECK(partida_ajustar_aposta(&partida, 0, 0) == 20);
    CHECK(partida_ajustar_aposta(&partida, 20, 2) == 60);
    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 19) == POKER_ERR_ACTION);
    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 40) == POKER_OK);
    CHECK(partida.mao.mesa.player[0].street_bet == 40);
    partida_encerrar(&partida);
}

static void test_adversario_all_in_impede_aumentar(void)
{
    Partida partida = {0};
    PokerChips stacks[2] = {2000, 15};

    CHECK(partida_iniciar(&partida, 15));
    CHECK(poker_start_hand(&partida.mao.mesa, stacks, TORNEIO_ASSENTO_JOGADOR, 10, 20) == POKER_OK);
    CHECK(partida.mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    partida.situacao = PARTIDA_VEZ_DO_JOGADOR;

    CHECK(partida_ajustar_aposta(&partida, 0, 0) == 0);
    CHECK(partida_ajustar_aposta(&partida, 100, 1) == 0);
    CHECK(partida_comando_jogador(&partida, COMANDO_APOSTAR, 100) == POKER_ERR_ACTION);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);

    CHECK(partida_comando_jogador(&partida, COMANDO_PASSAR_OU_PAGAR, 0) == POKER_OK);
    CHECK(partida.mao.mesa.phase == POKER_SETTLED);
    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA ||
          partida.situacao == PARTIDA_ROLETA ||
          partida.situacao == PARTIDA_CONFRONTO_VENCIDO);
    partida_encerrar(&partida);
}

static void test_partidas_inteiras(void)
{
    struct {
        const char *nome;
        Politica politica;
    } politicas[] = {
        {"passiva", politica_passiva},
        {"all-in", politica_all_in},
        {"aleatoria", politica_aleatoria}
    };

    int campeoes_em_todas = 0;
    int roletas_em_todas = 0;
    int recuperacoes_em_todas = 0;
    int mortes_em_todas = 0;
    int falidos_em_todas = 0;

    for (size_t p = 0; p < sizeof politicas / sizeof politicas[0]; p++) {
        int confrontos_vencidos = 0;
        int derrotas = 0;
        int campeoes = 0;

        for (int semente = 1; semente <= SEMENTES; semente++) {
            Resultado resultado = jogar_ate_o_fim((unsigned)semente, politicas[p].politica, false);

            CHECK(resultado.maos > 0);
            CHECK(resultado.potes_pagos == resultado.maos);
            CHECK(resultado.mortes + resultado.falidos + resultado.campeoes == 1);
            CHECK(resultado.falencias == resultado.roletas + resultado.falidos);
            CHECK(resultado.recuperacoes <= PARTIDA_RECUPERACOES_MAXIMAS);
            CHECK(resultado.roletas == resultado.recuperacoes + resultado.mortes);
            CHECK(resultado.confrontos_vencidos <= TORNEIO_CONFRONTOS - 1);
            confrontos_vencidos += resultado.confrontos_vencidos;
            derrotas += resultado.mortes + resultado.falidos;
            campeoes += resultado.campeoes;
            roletas_em_todas += resultado.roletas;
            recuperacoes_em_todas += resultado.recuperacoes;
            mortes_em_todas += resultado.mortes;
            falidos_em_todas += resultado.falidos;
        }

        CHECK(derrotas + campeoes == SEMENTES);
        CHECK(confrontos_vencidos > 0);
        CHECK(derrotas > 0);
        campeoes_em_todas += campeoes;
    }

    CHECK(campeoes_em_todas > 0);
    CHECK(roletas_em_todas > 0);
    CHECK(recuperacoes_em_todas > 0);
    CHECK(mortes_em_todas > 0);
    CHECK(falidos_em_todas > 0);
}

static void test_mesma_semente_reproduz_a_partida(void)
{
    for (int semente = 1; semente <= 10; semente++) {
        Resultado a = jogar_ate_o_fim((unsigned)semente, politica_aleatoria, false);
        Resultado b = jogar_ate_o_fim((unsigned)semente, politica_aleatoria, false);

        CHECK(a.maos == b.maos);
        CHECK(a.passos == b.passos);
        CHECK(a.confronto == b.confronto);
        CHECK(a.fichas_jogador == b.fichas_jogador);
        CHECK(a.fichas_adversario == b.fichas_adversario);
        CHECK(a.confrontos_vencidos == b.confrontos_vencidos);
        CHECK(a.roletas == b.roletas);
        CHECK(a.recuperacoes == b.recuperacoes);
        CHECK(a.mortes == b.mortes);
    }
}

static void test_sementes_diferentes_dao_partidas_diferentes(void)
{
    int diferentes = 0;
    Resultado base = jogar_ate_o_fim(1, politica_aleatoria, false);

    for (int semente = 2; semente <= 12; semente++) {
        Resultado outra = jogar_ate_o_fim((unsigned)semente, politica_aleatoria, false);

        if (outra.maos != base.maos || outra.passos != base.passos ||
            outra.confronto != base.confronto) {
            diferentes++;
        }
    }

    CHECK(diferentes > 0);
}

static void test_embriaguez_muda_o_comportamento_da_ia(void)
{
    int diferentes = 0;

    for (int semente = 1; semente <= 20; semente++) {
        Resultado sobrio = jogar_ate_o_fim((unsigned)semente, politica_passiva, false);
        Resultado bebado = jogar_ate_o_fim((unsigned)semente, politica_passiva, true);

        CHECK(sobrio.mortes + sobrio.falidos + sobrio.campeoes == 1);
        CHECK(bebado.mortes + bebado.falidos + bebado.campeoes == 1);

        if (sobrio.passos != bebado.passos || sobrio.maos != bebado.maos ||
            sobrio.fichas_jogador != bebado.fichas_jogador) {
            diferentes++;
        }
    }

    CHECK(diferentes > 0);
}

/**
 * Joga a partida com all-in a cada vez do jogador, até que ela chegue a uma
 * das situações que o chamador procura. Devolve a situação em que parou, que
 * pode ser outra (por exemplo, um confronto vencido quando se procurava a
 * falência).
 */
static PartidaSituacao jogar_all_in_ate(Partida *partida, PartidaSituacao procurada,
                                        PartidaSituacao alternativa)
{
    int passos = 0;

    while (++passos < LIMITE_DE_PASSOS) {
        switch (partida->situacao) {
            case PARTIDA_VEZ_DO_JOGADOR:
                CHECK(partida_agir_jogador(partida, ALL_IN) == POKER_OK);
                break;

            case PARTIDA_VEZ_DO_ADVERSARIO:
                CHECK(partida_agir_adversario(partida, NULL) == POKER_OK);
                break;

            case PARTIDA_MAO_ENCERRADA:
                CHECK(partida_continuar(partida));
                break;

            default:
                return partida->situacao;
        }

        if (partida->situacao == procurada || partida->situacao == alternativa) {
            return partida->situacao;
        }
    }

    CHECK(false);
    return PARTIDA_VAZIA;
}

/** Leva uma partida nova até a roleta de falência; false se a semente não faliu o jogador. */
static bool levar_a_roleta(Partida *partida, unsigned semente, int balas, int recuperacoes)
{
    CHECK(partida_iniciar(partida, semente));
    partida->risco.balas_roleta = balas;
    partida->risco_da_mao.balas_roleta = balas;
    partida->torneio.recuperacoes = recuperacoes;
    partida->torneio_da_mao.recuperacoes = recuperacoes;

    return jogar_all_in_ate(partida, PARTIDA_ROLETA, PARTIDA_JOGADOR_FALIDO) == PARTIDA_ROLETA;
}

static bool achar_evento_de_roleta(Partida *partida, EventoJogo *achado)
{
    EventoJogo evento;
    bool achou = false;

    while (fila_eventos_retirar(&partida->eventos, &evento)) {
        if (evento.tipo == EVENTO_ROLETA) {
            *achado = evento;
            achou = true;
        }
    }

    return achou;
}

static void test_gatilho_so_vale_na_roleta(void)
{
    Partida partida = {0};
    Partida vazia = {0};

    CHECK(!partida_puxar_gatilho(NULL));
    CHECK(!partida_puxar_gatilho(&vazia));

    CHECK(partida_iniciar(&partida, 3));
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    fila_eventos_iniciar(&partida.eventos);
    CHECK(!partida_puxar_gatilho(&partida));
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(partida.quantidade_no_registro == 0);
    CHECK(fila_eventos_tamanho(&partida.eventos) == 0);
    partida_encerrar(&partida);
}

static void test_o_risco_comeca_com_uma_bala(void)
{
    Partida partida = {0};

    CHECK(partida_iniciar(&partida, 9));
    CHECK(partida.risco.balas_roleta == 1);

    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        CHECK(partida.risco.usos_trapaca[i] == 0);
    }

    CHECK(partida.roleta_motivo == ROLETA_NENHUMA);
    CHECK(partida.risco_da_mao.balas_roleta == 1);
    CHECK(partida.estado_do_risco_da_mao == partida.estado_do_risco);
    partida_encerrar(&partida);
}

static void test_falencia_leva_a_roleta_e_seis_balas_matam(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 60 && !achou; semente++) {
        Partida partida = {0};
        EventoJogo evento;

        if (!levar_a_roleta(&partida, semente, 6, 0)) {
            partida_encerrar(&partida);
            continue;
        }

        achou = true;
        CHECK(partida.situacao == PARTIDA_ROLETA);
        CHECK(partida.roleta_motivo == ROLETA_FALENCIA);
        CHECK(partida.torneio.situacao == TORNEIO_JOGADOR_FALIDO);
        CHECK(partida.quantidade_no_registro > 0);
        CHECK(!partida_continuar(&partida));
        CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_ERR_STATE);
        CHECK(partida_agir_adversario(&partida, NULL) == POKER_ERR_STATE);

        CHECK(partida_puxar_gatilho(&partida));
        CHECK(partida.situacao == PARTIDA_JOGADOR_MORTO);
        CHECK(partida.risco.balas_roleta == 6);
        CHECK(achar_evento_de_roleta(&partida, &evento));
        CHECK(evento.roleta.por_falencia && evento.roleta.morreu);
        CHECK(evento.roleta.balas_antes == 6 && evento.roleta.balas_depois == 6);
        CHECK(!partida_puxar_gatilho(&partida));
        CHECK(!partida_continuar(&partida));
        partida_encerrar(&partida);
    }

    CHECK(achou);
}

static void test_sobreviver_a_roleta_devolve_o_jogador_ao_confronto(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 60 && !achou; semente++) {
        for (uint64_t estado = 1; estado <= 40 && !achou; estado++) {
            Partida partida = {0};
            EventoJogo evento;

            if (!levar_a_roleta(&partida, semente, 1, 0)) {
                partida_encerrar(&partida);
                break;
            }

            partida.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] = 5;
            partida.estado_do_risco = estado;
            fila_eventos_iniciar(&partida.eventos);
            CHECK(partida_puxar_gatilho(&partida));

            if (partida.situacao == PARTIDA_JOGADOR_MORTO) {
                CHECK(partida.risco.balas_roleta == 1);
                CHECK(partida.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] == 5);
                partida_encerrar(&partida);
                continue;
            }

            achou = true;
            CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
            CHECK(partida.risco.balas_roleta == 2);
            CHECK(partida.risco.usos_trapaca[PROB_TRAPACA_CUMPLICE] == 0);
            CHECK(partida.torneio.situacao == TORNEIO_EM_ANDAMENTO);
            CHECK(partida.torneio.recuperacoes == 1);
            CHECK(partida.torneio.fichas_jogador == torneio_fichas_iniciais_adversario(0) / 2);
            CHECK(achar_evento_de_roleta(&partida, &evento));
            CHECK(evento.roleta.por_falencia && !evento.roleta.morreu);
            CHECK(evento.roleta.balas_antes == 1 && evento.roleta.balas_depois == 2);

            CHECK(partida_continuar(&partida));
            CHECK(partida.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips +
                  partida.mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].committed ==
                  torneio_fichas_iniciais_adversario(0) / 2);
            CHECK(partida.risco_da_mao.balas_roleta == 2);
            partida_encerrar(&partida);
        }
    }

    CHECK(achou);
}

static void test_depois_do_limite_de_recuperacoes_a_falencia_acaba_a_partida(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 60 && !achou; semente++) {
        Partida partida = {0};

        CHECK(partida_iniciar(&partida, semente));
        partida.torneio.recuperacoes = PARTIDA_RECUPERACOES_MAXIMAS;

        if (jogar_all_in_ate(&partida, PARTIDA_JOGADOR_FALIDO, PARTIDA_CONFRONTO_VENCIDO) ==
            PARTIDA_JOGADOR_FALIDO) {
            achou = true;
            CHECK(partida.roleta_motivo == ROLETA_NENHUMA);
            CHECK(!partida_puxar_gatilho(&partida));
            CHECK(!partida_continuar(&partida));
        }

        partida_encerrar(&partida);
    }

    CHECK(achou);
}

static void test_o_resultado_da_roleta_so_depende_do_gerador_da_partida(void)
{
    int mortes = 0;
    int sobrevivencias = 0;
    unsigned semente = 0;

    /* O rand da biblioteca muda de um sistema para outro: procura uma semente que leve à roleta. */
    for (unsigned candidata = 1; candidata <= 300 && semente == 0; candidata++) {
        Partida teste = {0};

        if (levar_a_roleta(&teste, candidata, 3, 0)) {
            semente = candidata;
        }

        partida_encerrar(&teste);
    }

    CHECK(semente != 0);

    for (uint64_t estado = 1; estado <= 60; estado++) {
        Partida a = {0};
        Partida b = {0};

        CHECK(levar_a_roleta(&a, semente, 3, 0));
        CHECK(levar_a_roleta(&b, semente, 3, 0));
        a.estado_do_risco = estado;
        b.estado_do_risco = estado;

        srand(1);
        CHECK(partida_puxar_gatilho(&a));
        srand(123456);
        CHECK(partida_puxar_gatilho(&b));

        CHECK(a.situacao == b.situacao);
        CHECK(a.risco.balas_roleta == b.risco.balas_roleta);
        CHECK(a.estado_do_risco == b.estado_do_risco);
        CHECK(a.estado_do_risco != estado);

        if (a.situacao == PARTIDA_JOGADOR_MORTO) {
            mortes++;
        } else {
            sobrevivencias++;
        }

        partida_encerrar(&a);
        partida_encerrar(&b);
    }

    CHECK(mortes > 0 && sobrevivencias > 0);
}

static void test_vencer_um_confronto_tira_uma_bala_e_recomeca_a_suspeita(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 80 && !achou; semente++) {
        Partida partida = {0};

        CHECK(partida_iniciar(&partida, semente));
        partida.risco.balas_roleta = 4;
        partida.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] = 3;
        partida.risco.usos_trapaca[PROB_TRAPACA_CARTA_MANGA] = 2;

        if (jogar_all_in_ate(&partida, PARTIDA_CONFRONTO_VENCIDO, PARTIDA_ROLETA) ==
            PARTIDA_CONFRONTO_VENCIDO) {
            achou = true;
            CHECK(partida.risco.balas_roleta == 4);
            CHECK(partida.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] == 3);
            CHECK(partida_continuar(&partida));
            CHECK(partida.torneio.confronto == 1);
            CHECK(partida.risco.balas_roleta == 3);
            CHECK(partida.risco.usos_trapaca[PROB_TRAPACA_SPIKE_DRINK] == 0);
            CHECK(partida.risco.usos_trapaca[PROB_TRAPACA_CARTA_MANGA] == 0);
            CHECK(partida.risco_da_mao.balas_roleta == 3);
        }

        partida_encerrar(&partida);
    }

    CHECK(achou);
}

static void test_a_ultima_bala_nao_sai_do_tambor(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 80 && !achou; semente++) {
        Partida partida = {0};

        CHECK(partida_iniciar(&partida, semente));
        CHECK(partida.risco.balas_roleta == 1);

        if (jogar_all_in_ate(&partida, PARTIDA_CONFRONTO_VENCIDO, PARTIDA_ROLETA) ==
            PARTIDA_CONFRONTO_VENCIDO) {
            achou = true;
            CHECK(partida_continuar(&partida));
            CHECK(partida.risco.balas_roleta == 1);
        }

        partida_encerrar(&partida);
    }

    CHECK(achou);
}

static void test_o_registro_guarda_acoes_e_gatilho_na_ordem(void)
{
    bool achou = false;

    for (unsigned semente = 1; semente <= 60 && !achou; semente++) {
        Partida partida = {0};
        size_t acoes;

        if (!levar_a_roleta(&partida, semente, 1, 0)) {
            partida_encerrar(&partida);
            continue;
        }

        achou = true;
        acoes = partida.quantidade_no_registro;
        CHECK(acoes > 0 && !partida.registro_perdido);

        for (size_t i = 0; i < acoes; i++) {
            CHECK(partida.registro[i].tipo == REGISTRO_ACAO);
            CHECK(i >= partida.mao.quantidade_de_acoes ||
                  partida.registro[i].acao.type == partida.mao.acoes[i].type);
        }

        CHECK(acoes == partida.mao.quantidade_de_acoes);
        CHECK(partida_puxar_gatilho(&partida));
        CHECK(partida.quantidade_no_registro == acoes + 1);
        CHECK(partida.registro[acoes].tipo == REGISTRO_GATILHO);
        partida_encerrar(&partida);
    }

    CHECK(achou);
}

static void test_reiniciar_partida_libera_a_anterior(void)
{
    Partida partida = {0};

    CHECK(partida_iniciar(&partida, 7));
    CHECK(partida_agir_jogador(&partida, PAGAR) == POKER_OK);
    partida.adversario_embriagado = true;
    CHECK(partida_iniciar(&partida, 8));
    CHECK(!partida.adversario_embriagado);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_JOGADOR);
    CHECK(partida.torneio.maos_jogadas == 0);
    CHECK(fila_eventos_tamanho(&partida.eventos) == 3);
    CHECK(partida.mao.baralho.count == 47);

    partida_encerrar(&partida);
    CHECK(partida.mao.baralho.count == 0 && partida.mao.baralho.head == NULL);
}

static void test_o_estilo_do_adversario_depende_das_fichas(void)
{
    /* Agressivo (2000): só muda de estilo quando perde, e então joga medroso. */
    PokerChips atras0 = 2000 * PARTIDA_ESTILO_ATRAS_PCT / 100;
    /* Medroso (4000): só muda de estilo quando ganha, e então joga agressivo. */
    PokerChips frente1 = 4000 * PARTIDA_ESTILO_FRENTE_PCT / 100;
    /* Desconfiado (8000): equilibrado, medroso se perde e agressivo se ganha. */
    PokerChips atras2 = 8000 * PARTIDA_ESTILO_ATRAS_PCT / 100;
    PokerChips frente2 = 8000 * PARTIDA_ESTILO_FRENTE_PCT / 100;

    CHECK(estilo_com(0, 2000) == POKER_AI_AGGRESSIVE);
    CHECK(estilo_com(0, atras0 + 1) == POKER_AI_AGGRESSIVE);
    CHECK(estilo_com(0, atras0) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(0, 1) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(0, 6000) == POKER_AI_AGGRESSIVE);

    CHECK(estilo_com(1, 4000) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(1, 1) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(1, frente1 - 1) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(1, frente1) == POKER_AI_AGGRESSIVE);

    CHECK(estilo_com(2, 8000) == POKER_AI_BALANCED);
    CHECK(estilo_com(2, atras2 + 1) == POKER_AI_BALANCED);
    CHECK(estilo_com(2, atras2) == POKER_AI_CAUTIOUS);
    CHECK(estilo_com(2, frente2 - 1) == POKER_AI_BALANCED);
    CHECK(estilo_com(2, frente2) == POKER_AI_AGGRESSIVE);

    CHECK(estilo_com(-1, 2000) == POKER_AI_BALANCED);
    CHECK(estilo_com(TORNEIO_CONFRONTOS, 2000) == POKER_AI_BALANCED);

    for (int confronto = 0; confronto < TORNEIO_CONFRONTOS; confronto++) {
        PokerChips inicial = torneio_fichas_iniciais_adversario(confronto);

        for (PokerChips fichas = 0; fichas <= inicial * 2; fichas += 37) {
            CHECK(estilo_com(confronto, fichas) == estilo_esperado(confronto, fichas));
        }
    }
}

static void test_o_estilo_muda_entre_maos_e_a_ia_joga_com_ele(void)
{
    Partida partida = {0};
    PokerAIDecisionInfo info;
    PokerChips total;

    CHECK(partida_iniciar(&partida, 6));
    CHECK(partida.ia.style == POKER_AI_AGGRESSIVE);
    CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);
    total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;

    /* Perdendo feio, o Agressivo passa a jogar medroso na mão seguinte. */
    partida.torneio.fichas_adversario = 1000;
    partida.torneio.fichas_jogador = total - 1000;
    CHECK(partida_continuar(&partida));
    CHECK(partida.ia.style == POKER_AI_CAUTIOUS);
    CHECK(partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO);
    CHECK(partida_agir_adversario(&partida, &info) == POKER_OK);
    CHECK(info.profile == POKER_AI_CAUTIOUS);

    for (int passos = 0; passos < 50 && (partida.situacao == PARTIDA_VEZ_DO_JOGADOR ||
                                         partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO); passos++) {
        if (partida.situacao == PARTIDA_VEZ_DO_JOGADOR) {
            CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);
        } else {
            CHECK(partida_agir_adversario(&partida, &info) == POKER_OK);
            CHECK(info.profile == POKER_AI_CAUTIOUS);
        }
    }

    /* O estilo vale pela mão inteira, mesmo depois de as fichas mudarem. */
    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(partida.ia.style == POKER_AI_CAUTIOUS);

    /* Com as fichas de volta ao ponto de partida, a mão seguinte é agressiva de novo. */
    total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;
    partida.torneio.fichas_adversario = total / 2;
    partida.torneio.fichas_jogador = total - total / 2;
    CHECK(partida_continuar(&partida));
    CHECK(partida.ia.style == POKER_AI_AGGRESSIVE);

    partida_encerrar(&partida);
}

static void test_o_estilo_dinamico_se_refaz_ao_continuar_o_salvamento(void)
{
    static PartidaSalva salva;
    Partida partida = {0};
    Partida refeita = {0};
    PokerAIDecisionInfo info;
    PokerAIDecisionInfo info_refeita;
    PokerChips total;

    CHECK(partida_iniciar(&partida, 9));
    CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);
    total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;
    partida.torneio.fichas_adversario = 1000;
    partida.torneio.fichas_jogador = total - 1000;
    CHECK(partida_continuar(&partida));
    CHECK(partida.ia.style == POKER_AI_CAUTIOUS);

    /* Guardada no começo da mão, a partida volta com o mesmo estilo e decide igual. */
    CHECK(partida_exportar(&partida, &salva));
    CHECK(partida_restaurar(&refeita, &salva));
    CHECK(refeita.ia.style == POKER_AI_CAUTIOUS);
    CHECK(refeita.situacao == PARTIDA_VEZ_DO_ADVERSARIO);
    CHECK(partida_agir_adversario(&partida, &info) == POKER_OK);
    CHECK(partida_agir_adversario(&refeita, &info_refeita) == POKER_OK);
    CHECK(info.profile == POKER_AI_CAUTIOUS && info_refeita.profile == POKER_AI_CAUTIOUS);
    CHECK(partida.mao.quantidade_de_acoes == refeita.mao.quantidade_de_acoes);
    CHECK(partida.mao.acoes[partida.mao.quantidade_de_acoes - 1].type ==
          refeita.mao.acoes[refeita.mao.quantidade_de_acoes - 1].type);

    /* Guardada depois de a mão acabar, volta com o estilo da mão, não o da seguinte. */
    for (int passos = 0; passos < 50 && (partida.situacao == PARTIDA_VEZ_DO_JOGADOR ||
                                         partida.situacao == PARTIDA_VEZ_DO_ADVERSARIO); passos++) {
        if (partida.situacao == PARTIDA_VEZ_DO_JOGADOR) {
            CHECK(partida_agir_jogador(&partida, DESISTIR) == POKER_OK);
        } else {
            CHECK(partida_agir_adversario(&partida, NULL) == POKER_OK);
        }
    }

    CHECK(partida.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(partida_exportar(&partida, &salva));
    CHECK(partida_restaurar(&refeita, &salva));
    CHECK(refeita.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(refeita.ia.style == partida.ia.style);
    CHECK(refeita.torneio.fichas_adversario == partida.torneio.fichas_adversario);

    partida_encerrar(&partida);
    partida_encerrar(&refeita);
}

int main(void)
{
    test_primeira_mao_comeca_na_vez_do_jogador();
    test_o_estilo_do_adversario_depende_das_fichas();
    test_o_estilo_muda_entre_maos_e_a_ia_joga_com_ele();
    test_o_estilo_dinamico_se_refaz_ao_continuar_o_salvamento();
    test_nomes_dos_adversarios();
    test_recusas_de_turno_e_de_estado();
    test_desistencia_registra_no_torneio_e_continua();
    test_stack_curto_resolve_a_mao_ao_iniciar();
    test_comandos_e_ajuste_de_aposta();
    test_passar_pagar_e_apostar_sem_aposta_anterior();
    test_adversario_all_in_impede_aumentar();
    test_partidas_inteiras();
    test_mesma_semente_reproduz_a_partida();
    test_sementes_diferentes_dao_partidas_diferentes();
    test_embriaguez_muda_o_comportamento_da_ia();
    test_reiniciar_partida_libera_a_anterior();
    test_gatilho_so_vale_na_roleta();
    test_o_risco_comeca_com_uma_bala();
    test_falencia_leva_a_roleta_e_seis_balas_matam();
    test_sobreviver_a_roleta_devolve_o_jogador_ao_confronto();
    test_depois_do_limite_de_recuperacoes_a_falencia_acaba_a_partida();
    test_o_resultado_da_roleta_so_depende_do_gerador_da_partida();
    test_vencer_um_confronto_tira_uma_bala_e_recomeca_a_suspeita();
    test_a_ultima_bala_nao_sai_do_tambor();
    test_o_registro_guarda_acoes_e_gatilho_na_ordem();
    puts("OK: partida com torneio, mao, fila de eventos, IA e roleta (turnos, confrontos, perfis, falencia e partidas inteiras).");
    return EXIT_SUCCESS;
}
