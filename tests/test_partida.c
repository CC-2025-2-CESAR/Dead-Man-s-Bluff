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

static PokerAIStyle estilo_esperado(int confronto)
{
    return confronto == 0 ? POKER_AI_AGGRESSIVE
         : confronto == 1 ? POKER_AI_CAUTIOUS
         : POKER_AI_BALANCED;
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

    CHECK(partida_iniciar(&partida, semente));
    partida.adversario_embriagado = embriagado;
    total = partida.torneio.fichas_jogador + partida.torneio.fichas_adversario;

    while (!terminou) {
        PokerAIDecisionInfo info;

        CHECK(++resultado.passos < LIMITE_DE_PASSOS);
        drenar_eventos(&partida, &resultado);
        CHECK(partida.ia.style == estilo_esperado(partida.torneio.confronto));

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
                resultado.maos++;
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

            case PARTIDA_JOGADOR_FALIDO:
                resultado.maos++;
                resultado.falencias++;
                CHECK(partida.torneio.fichas_jogador == 0);
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
          partida.situacao == PARTIDA_JOGADOR_FALIDO ||
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

    for (size_t p = 0; p < sizeof politicas / sizeof politicas[0]; p++) {
        int confrontos_vencidos = 0;
        int falencias = 0;
        int campeoes = 0;

        for (int semente = 1; semente <= SEMENTES; semente++) {
            Resultado resultado = jogar_ate_o_fim((unsigned)semente, politicas[p].politica, false);

            CHECK(resultado.maos > 0);
            CHECK(resultado.potes_pagos == resultado.maos);
            CHECK(resultado.falencias + resultado.campeoes == 1);
            CHECK(resultado.confrontos_vencidos <= TORNEIO_CONFRONTOS - 1);
            confrontos_vencidos += resultado.confrontos_vencidos;
            falencias += resultado.falencias;
            campeoes += resultado.campeoes;
        }

        CHECK(falencias + campeoes == SEMENTES);
        CHECK(confrontos_vencidos > 0);
        CHECK(falencias > 0);
        campeoes_em_todas += campeoes;
    }

    CHECK(campeoes_em_todas > 0);
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

        CHECK(sobrio.falencias + sobrio.campeoes == 1);
        CHECK(bebado.falencias + bebado.campeoes == 1);

        if (sobrio.passos != bebado.passos || sobrio.maos != bebado.maos ||
            sobrio.fichas_jogador != bebado.fichas_jogador) {
            diferentes++;
        }
    }

    CHECK(diferentes > 0);
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
    CHECK(partida.mao.baralho.count == 48);

    partida_encerrar(&partida);
    CHECK(partida.mao.baralho.count == 0 && partida.mao.baralho.head == NULL);
}

int main(void)
{
    test_primeira_mao_comeca_na_vez_do_jogador();
    test_nomes_dos_adversarios();
    test_recusas_de_turno_e_de_estado();
    test_desistencia_registra_no_torneio_e_continua();
    test_stack_curto_resolve_a_mao_ao_iniciar();
    test_partidas_inteiras();
    test_mesma_semente_reproduz_a_partida();
    test_sementes_diferentes_dao_partidas_diferentes();
    test_embriaguez_muda_o_comportamento_da_ia();
    test_reiniciar_partida_libera_a_anterior();
    puts("OK: partida com torneio, mao, fila de eventos e IA (turnos, confrontos, perfis e partidas inteiras).");
    return EXIT_SUCCESS;
}
