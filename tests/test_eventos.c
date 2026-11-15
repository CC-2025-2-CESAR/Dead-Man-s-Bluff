#include "core/mao.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 60
#define LIMITE_DE_ACOES 1000
#define LIMITE_DE_MAOS 2000

static const PokerAction DESISTIR = {POKER_FOLD, 0};
static const PokerAction PASSAR = {POKER_CHECK, 0};
static const PokerAction PAGAR = {POKER_CALL, 0};
static const PokerAction ALL_IN = {POKER_ALL_IN, 0};

static unsigned proximo_aleatorio(unsigned *semente)
{
    *semente = *semente * 1664525u + 1013904223u;
    return *semente >> 8;
}

static EventoJogo evento_de_teste(int marca)
{
    EventoJogo evento = {.tipo = (TipoEvento)(marca % 6)};

    evento.acao.assento = marca;
    return evento;
}

static EventoJogo retirar(FilaEventos *fila, TipoEvento esperado)
{
    EventoJogo evento;

    CHECK(fila_eventos_retirar(fila, &evento));
    CHECK(evento.tipo == esperado);
    return evento;
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static void conferir_acao(
    const EventoJogo *evento,
    int assento,
    PokerActionType acao,
    PokerChips colocadas,
    PokerChips total_na_rua,
    PokerChips restantes,
    PokerChips pote
)
{
    CHECK(evento->acao.assento == assento);
    CHECK(evento->acao.acao == acao);
    CHECK(evento->acao.fichas_colocadas == colocadas);
    CHECK(evento->acao.total_na_rua == total_na_rua);
    CHECK(evento->acao.fichas_restantes == restantes);
    CHECK(evento->acao.pote == pote);
}

static void conferir_inicio(
    FilaEventos *fila,
    const Mao *mao,
    int dealer,
    PokerChips postadas[2],
    PokerChips restantes[2]
)
{
    EventoJogo evento = retirar(fila, EVENTO_MAO_INICIADA);

    CHECK(evento.mao_iniciada.dealer == dealer);
    CHECK(evento.mao_iniciada.pequeno_blind == 10);
    CHECK(evento.mao_iniciada.grande_blind == 20);

    for (int assento = 0; assento < 2; assento++) {
        CHECK(evento.mao_iniciada.fichas_postadas[assento] == postadas[assento]);
        CHECK(evento.mao_iniciada.fichas_restantes[assento] == restantes[assento]);
    }

    for (int i = 0; i < 2; i++) {
        int assento = i == 0 ? dealer : 1 - dealer;

        evento = retirar(fila, EVENTO_CARTAS_DISTRIBUIDAS);
        CHECK(evento.cartas_distribuidas.assento == assento);
        CHECK(mesma_carta(evento.cartas_distribuidas.cartas[0], mao->cartas[assento][0]));
        CHECK(mesma_carta(evento.cartas_distribuidas.cartas[1], mao->cartas[assento][1]));
    }
}

static void conferir_board(
    FilaEventos *fila,
    const Mao *mao,
    PokerStreet rua,
    int quantidade,
    size_t primeira
)
{
    EventoJogo evento = retirar(fila, EVENTO_BOARD_REVELADO);

    CHECK(evento.board.rua == rua);
    CHECK(evento.board.quantidade == quantidade);

    for (int i = 0; i < quantidade; i++) {
        CHECK(mesma_carta(evento.board.cartas[i], mao->board[primeira + (size_t)i]));
    }
}

static void conferir_showdown_e_pote(
    FilaEventos *fila,
    const Mao *mao,
    PokerChips pote
)
{
    EventoJogo showdown = retirar(fila, EVENTO_SHOWDOWN);
    EventoJogo pago;

    for (int assento = 0; assento < 2; assento++) {
        PokerHandValue valor;

        CHECK(mesma_carta(showdown.showdown.cartas[assento][0], mao->cartas[assento][0]));
        CHECK(mesma_carta(showdown.showdown.cartas[assento][1], mao->cartas[assento][1]));
        CHECK(poker_evaluate_holdem(
                  mao->cartas[assento],
                  mao->board,
                  MAO_CARTAS_NO_BOARD,
                  &valor
              ) == POKER_OK);
        CHECK(showdown.showdown.valor[assento].category == valor.category);
        CHECK(poker_compare(&showdown.showdown.valor[assento], &valor) == 0);
    }

    pago = retirar(fila, EVENTO_POTE_PAGO);
    CHECK(pago.pote.showdown);
    CHECK(pago.pote.vencedores == mao->resultado.winners);
    CHECK(pago.pote.pote == pote);
    CHECK(pago.pote.premio[0] + pago.pote.premio[1] == pote);
    CHECK(pago.pote.fichas_finais[0] == mao->mesa.player[0].chips);
    CHECK(pago.pote.fichas_finais[1] == mao->mesa.player[1].chips);
    CHECK(fila_eventos_tamanho(fila) == 0);
}

static void test_fila_mantem_a_ordem_e_da_a_volta(void)
{
    FilaEventos fila;
    EventoJogo evento;
    int proxima_a_entrar = 0;
    int proxima_a_sair = 0;

    fila_eventos_iniciar(&fila);
    CHECK(fila_eventos_tamanho(&fila) == 0);
    CHECK(!fila_eventos_retirar(&fila, &evento));

    for (int volta = 0; volta < 4; volta++) {
        for (int i = 0; i < FILA_EVENTOS_CAPACIDADE - 10; i++) {
            evento = evento_de_teste(proxima_a_entrar);
            CHECK(fila_eventos_adicionar(&fila, &evento));
            proxima_a_entrar++;
        }

        CHECK(fila_eventos_tamanho(&fila) == FILA_EVENTOS_CAPACIDADE - 10);

        for (int i = 0; i < FILA_EVENTOS_CAPACIDADE - 10; i++) {
            CHECK(fila_eventos_retirar(&fila, &evento));
            CHECK(evento.acao.assento == proxima_a_sair);
            CHECK(evento.tipo == (TipoEvento)(proxima_a_sair % 6));
            proxima_a_sair++;
        }

        CHECK(fila_eventos_tamanho(&fila) == 0);
    }

    CHECK(fila.perdidos == 0);
}

static void test_fila_cheia_descarta_o_evento_novo(void)
{
    FilaEventos fila;
    EventoJogo evento;

    fila_eventos_iniciar(&fila);

    for (int i = 0; i < FILA_EVENTOS_CAPACIDADE; i++) {
        evento = evento_de_teste(i);
        CHECK(fila_eventos_adicionar(&fila, &evento));
    }

    evento = evento_de_teste(999);
    CHECK(!fila_eventos_adicionar(&fila, &evento));
    CHECK(!fila_eventos_adicionar(&fila, &evento));
    CHECK(fila.perdidos == 2);
    CHECK(fila_eventos_tamanho(&fila) == FILA_EVENTOS_CAPACIDADE);

    CHECK(fila_eventos_retirar(&fila, &evento));
    CHECK(evento.acao.assento == 0);
    evento = evento_de_teste(1000);
    CHECK(fila_eventos_adicionar(&fila, &evento));
    CHECK(fila.perdidos == 2);

    fila_eventos_iniciar(&fila);
    CHECK(fila.perdidos == 0 && fila_eventos_tamanho(&fila) == 0);
}

static void test_fila_recusa_argumentos_invalidos(void)
{
    FilaEventos fila;
    EventoJogo evento = evento_de_teste(1);

    fila_eventos_iniciar(NULL);
    fila_eventos_iniciar(&fila);

    CHECK(!fila_eventos_adicionar(NULL, &evento));
    CHECK(!fila_eventos_adicionar(&fila, NULL));
    CHECK(!fila_eventos_retirar(NULL, &evento));
    CHECK(fila_eventos_adicionar(&fila, &evento));
    CHECK(!fila_eventos_retirar(&fila, NULL));
    CHECK(fila_eventos_tamanho(&fila) == 1);
    CHECK(fila_eventos_tamanho(NULL) == 0);
    CHECK(fila.perdidos == 0);
}

static void test_eventos_da_desistencia(void)
{
    Torneio torneio;
    Mao mao = {0};
    FilaEventos fila;
    EventoJogo evento;
    PokerChips postadas[2] = {10, 20};
    PokerChips restantes[2] = {1990, 1980};

    torneio_iniciar(&torneio);
    fila_eventos_iniciar(&fila);
    srand(1);

    CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
    CHECK(fila_eventos_tamanho(&fila) == 3);
    conferir_inicio(&fila, &mao, 0, postadas, restantes);

    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(fila_eventos_tamanho(&fila) == 2);

    evento = retirar(&fila, EVENTO_ACAO);
    conferir_acao(&evento, 0, POKER_FOLD, 0, 10, 1990, 30);

    evento = retirar(&fila, EVENTO_POTE_PAGO);
    CHECK(!evento.pote.showdown);
    CHECK(evento.pote.vencedores == 2u);
    CHECK(evento.pote.pote == 30);
    CHECK(evento.pote.premio[0] == 0 && evento.pote.premio[1] == 20);
    CHECK(evento.pote.devolucao[0] == 0 && evento.pote.devolucao[1] == 10);
    CHECK(evento.pote.fichas_finais[0] == 1990);
    CHECK(evento.pote.fichas_finais[1] == 2010);
    CHECK(fila_eventos_tamanho(&fila) == 0);
    CHECK(fila.perdidos == 0);

    CHECK(mao_registrar_no_torneio(&mao, &torneio));
    CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
    postadas[0] = 20;
    postadas[1] = 10;
    restantes[0] = 1970;
    restantes[1] = 2000;
    conferir_inicio(&fila, &mao, TORNEIO_ASSENTO_ADVERSARIO, postadas, restantes);
    mao_liberar(&mao);
}

static void test_eventos_da_mao_ate_o_river(void)
{
    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        FilaEventos fila;
        EventoJogo evento;
        PokerChips postadas[2] = {10, 20};
        PokerChips restantes[2] = {1990, 1980};

        torneio_iniciar(&torneio);
        fila_eventos_iniciar(&fila);
        srand((unsigned)semente);
        CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
        conferir_inicio(&fila, &mao, 0, postadas, restantes);

        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 0, POKER_CALL, 10, 20, 1980, 40);
        CHECK(fila_eventos_tamanho(&fila) == 0);

        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 1, POKER_CHECK, 0, 20, 1980, 40);
        conferir_board(&fila, &mao, POKER_FLOP, 3, 0);
        CHECK(fila_eventos_tamanho(&fila) == 0);

        for (int rua = POKER_FLOP; rua <= POKER_RIVER; rua++) {
            CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
            evento = retirar(&fila, EVENTO_ACAO);
            conferir_acao(&evento, 1, POKER_CHECK, 0, 0, 1980, 40);
            CHECK(fila_eventos_tamanho(&fila) == 0);

            CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
            evento = retirar(&fila, EVENTO_ACAO);
            conferir_acao(&evento, 0, POKER_CHECK, 0, 0, 1980, 40);

            if (rua == POKER_FLOP) {
                conferir_board(&fila, &mao, POKER_TURN, 1, 3);
                CHECK(fila_eventos_tamanho(&fila) == 0);
            } else if (rua == POKER_TURN) {
                conferir_board(&fila, &mao, POKER_RIVER, 1, 4);
                CHECK(fila_eventos_tamanho(&fila) == 0);
            }
        }

        conferir_showdown_e_pote(&fila, &mao, 40);
        CHECK(fila.perdidos == 0);
    }
}

static void test_eventos_do_all_in_antes_do_river(void)
{
    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        FilaEventos fila;
        EventoJogo evento;
        PokerChips postadas[2] = {10, 20};
        PokerChips restantes[2] = {1990, 1980};

        torneio_iniciar(&torneio);
        fila_eventos_iniciar(&fila);
        srand((unsigned)semente);
        CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
        conferir_inicio(&fila, &mao, 0, postadas, restantes);

        CHECK(mao_agir(&mao, ALL_IN) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 0, POKER_ALL_IN, 1990, 2000, 0, 2020);
        CHECK(fila_eventos_tamanho(&fila) == 0);

        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 1, POKER_CALL, 1980, 2000, 0, 4000);
        conferir_board(&fila, &mao, POKER_FLOP, 3, 0);
        conferir_board(&fila, &mao, POKER_TURN, 1, 3);
        conferir_board(&fila, &mao, POKER_RIVER, 1, 4);
        conferir_showdown_e_pote(&fila, &mao, 4000);
        CHECK(mao.mesa.street == POKER_PREFLOP);
    }

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        FilaEventos fila;
        EventoJogo evento;
        PokerChips postadas[2] = {10, 20};
        PokerChips restantes[2] = {1990, 1980};

        torneio_iniciar(&torneio);
        fila_eventos_iniciar(&fila);
        srand((unsigned)semente);
        CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
        conferir_inicio(&fila, &mao, 0, postadas, restantes);

        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        retirar(&fila, EVENTO_ACAO);
        retirar(&fila, EVENTO_ACAO);
        conferir_board(&fila, &mao, POKER_FLOP, 3, 0);

        CHECK(mao_agir(&mao, ALL_IN) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 1, POKER_ALL_IN, 1980, 1980, 0, 2020);
        CHECK(fila_eventos_tamanho(&fila) == 0);

        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        evento = retirar(&fila, EVENTO_ACAO);
        conferir_acao(&evento, 0, POKER_CALL, 1980, 1980, 0, 4000);
        conferir_board(&fila, &mao, POKER_TURN, 1, 3);
        conferir_board(&fila, &mao, POKER_RIVER, 1, 4);
        conferir_showdown_e_pote(&fila, &mao, 4000);
        CHECK(mao.mesa.street == POKER_FLOP);
    }
}

static void test_eventos_de_stack_curto_saem_ao_iniciar(void)
{
    Torneio torneio;
    Mao mao = {0};
    FilaEventos fila;
    EventoJogo evento;
    PokerChips postadas[2] = {10, 20};
    PokerChips restantes[2] = {0, 1980};

    torneio_iniciar(&torneio);
    torneio.fichas_jogador = 10;
    fila_eventos_iniciar(&fila);
    srand(3);

    CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
    CHECK(mao.situacao == MAO_ENCERRADA);
    conferir_inicio(&fila, &mao, 0, postadas, restantes);
    conferir_board(&fila, &mao, POKER_FLOP, 3, 0);
    conferir_board(&fila, &mao, POKER_TURN, 1, 3);
    conferir_board(&fila, &mao, POKER_RIVER, 1, 4);

    evento = retirar(&fila, EVENTO_SHOWDOWN);
    CHECK(evento.showdown.valor[0].category >= POKER_HIGH_CARD);
    evento = retirar(&fila, EVENTO_POTE_PAGO);
    CHECK(evento.pote.showdown);
    CHECK(evento.pote.pote == 30);
    CHECK(evento.pote.devolucao[0] == 0 && evento.pote.devolucao[1] == 10);
    CHECK(evento.pote.premio[0] + evento.pote.premio[1] == 20);
    CHECK(evento.pote.fichas_finais[0] + evento.pote.fichas_finais[1] == 2010);
    CHECK(fila_eventos_tamanho(&fila) == 0);
}

static void test_mao_funciona_sem_fila_e_com_fila_cheia(void)
{
    Torneio torneio;
    Mao mao = {0};
    FilaEventos fila;
    EventoJogo evento = evento_de_teste(0);

    torneio_iniciar(&torneio);
    srand(4);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.eventos == NULL);
    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(mao.situacao == MAO_ENCERRADA);
    CHECK(mao_registrar_no_torneio(&mao, &torneio));

    fila_eventos_iniciar(&fila);

    for (int i = 0; i < FILA_EVENTOS_CAPACIDADE; i++) {
        CHECK(fila_eventos_adicionar(&fila, &evento));
    }

    CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));
    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(mao.situacao == MAO_ENCERRADA);
    CHECK(mao.mesa.player[0].chips + mao.mesa.player[1].chips == 4000);
    CHECK(fila.perdidos == 5);
    CHECK(fila_eventos_tamanho(&fila) == FILA_EVENTOS_CAPACIDADE);
    CHECK(mao_registrar_no_torneio(&mao, &torneio));
}

static PokerAction escolher_acao_aleatoria(const Mao *mao, unsigned *semente)
{
    PokerLegalActions legais;
    unsigned sorteio = proximo_aleatorio(semente) % 100;

    CHECK(poker_legal_actions(&mao->mesa, mao->mesa.actor, &legais) == POKER_OK);

    if (sorteio < 6) {
        return DESISTIR;
    }

    if (sorteio < 14 && legais.all_in) {
        return ALL_IN;
    }

    if (sorteio < 40 && (legais.bet || legais.raise)) {
        PokerChips faixa = legais.max_total - legais.min_total + 1;
        PokerChips total = legais.min_total +
            (PokerChips)(proximo_aleatorio(semente) % (unsigned)faixa);

        return (PokerAction){legais.bet ? POKER_BET : POKER_RAISE, total};
    }

    return legais.check ? PASSAR : PAGAR;
}

static void conferir_eventos_da_mao(
    const Mao *mao,
    const EventoJogo *eventos,
    size_t quantidade
)
{
    PokerChips colocadas = 0;
    size_t cartas_no_board = 0;
    int boards = 0;
    int showdowns = 0;
    int potes = 0;

    CHECK(quantidade >= 4);
    CHECK(eventos[0].tipo == EVENTO_MAO_INICIADA);
    CHECK(eventos[1].tipo == EVENTO_CARTAS_DISTRIBUIDAS);
    CHECK(eventos[2].tipo == EVENTO_CARTAS_DISTRIBUIDAS);
    CHECK(eventos[quantidade - 1].tipo == EVENTO_POTE_PAGO);

    colocadas = eventos[0].mao_iniciada.fichas_postadas[0] +
                eventos[0].mao_iniciada.fichas_postadas[1];

    for (size_t i = 3; i < quantidade; i++) {
        switch (eventos[i].tipo) {
            case EVENTO_ACAO:
                CHECK(potes == 0 && showdowns == 0);
                colocadas += eventos[i].acao.fichas_colocadas;
                CHECK(eventos[i].acao.pote == colocadas);
                break;

            case EVENTO_BOARD_REVELADO:
                CHECK(potes == 0 && showdowns == 0);
                CHECK(eventos[i].board.rua == (PokerStreet)(boards + 1));
                cartas_no_board += (size_t)eventos[i].board.quantidade;
                boards++;
                break;

            case EVENTO_SHOWDOWN:
                showdowns++;
                break;

            case EVENTO_POTE_PAGO:
                potes++;
                CHECK(eventos[i].pote.pote == colocadas);
                CHECK(eventos[i].pote.showdown == (showdowns == 1));
                CHECK(eventos[i].pote.fichas_finais[0] == mao->mesa.player[0].chips);
                CHECK(eventos[i].pote.fichas_finais[1] == mao->mesa.player[1].chips);
                break;

            default:
                CHECK(false);
        }
    }

    CHECK(potes == 1);
    CHECK(showdowns <= 1);
    CHECK(cartas_no_board == mao->cartas_no_board);
    CHECK(showdowns == 0 || cartas_no_board == MAO_CARTAS_NO_BOARD);
}

static void test_eventos_em_torneios_inteiros(void)
{
    int campeoes = 0;
    int falencias = 0;

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        FilaEventos fila;
        unsigned sorteio = (unsigned)semente * 7919u + 13u;
        int maos = 0;
        bool terminou = false;

        torneio_iniciar(&torneio);
        fila_eventos_iniciar(&fila);
        srand((unsigned)semente);

        while (!terminou) {
            EventoJogo eventos[FILA_EVENTOS_CAPACIDADE];
            size_t quantidade = 0;

            CHECK(++maos <= LIMITE_DE_MAOS);
            CHECK(mao_iniciar_com_eventos(&mao, &torneio, &fila));

            for (int acoes = 0; acoes < LIMITE_DE_ACOES; acoes++) {
                while (fila_eventos_retirar(&fila, &eventos[quantidade])) {
                    quantidade++;
                    CHECK(quantidade < FILA_EVENTOS_CAPACIDADE);
                }

                if (mao.situacao != MAO_EM_ANDAMENTO) {
                    break;
                }

                CHECK(mao_agir(&mao, escolher_acao_aleatoria(&mao, &sorteio)) == POKER_OK);
            }

            CHECK(mao.situacao == MAO_ENCERRADA);
            CHECK(fila.perdidos == 0);
            conferir_eventos_da_mao(&mao, eventos, quantidade);
            CHECK(mao_registrar_no_torneio(&mao, &torneio));

            switch (torneio.situacao) {
                case TORNEIO_EM_ANDAMENTO:
                    break;

                case TORNEIO_CONFRONTO_VENCIDO:
                    CHECK(torneio_avancar_confronto(&torneio));
                    break;

                case TORNEIO_JOGADOR_FALIDO:
                    falencias++;
                    if (torneio.recuperacoes < 2) {
                        CHECK(torneio_recuperar_jogador(&torneio));
                    } else {
                        terminou = true;
                    }
                    break;

                case TORNEIO_CAMPEAO:
                    campeoes++;
                    terminou = true;
                    break;
            }
        }
    }

    CHECK(falencias > 0);
    CHECK(campeoes > 0);
}

int main(void)
{
    test_fila_mantem_a_ordem_e_da_a_volta();
    test_fila_cheia_descarta_o_evento_novo();
    test_fila_recusa_argumentos_invalidos();
    test_eventos_da_desistencia();
    test_eventos_da_mao_ate_o_river();
    test_eventos_do_all_in_antes_do_river();
    test_eventos_de_stack_curto_saem_ao_iniciar();
    test_mao_funciona_sem_fila_e_com_fila_cheia();
    test_eventos_em_torneios_inteiros();
    puts("OK: fila de eventos e sequencia de eventos da mao (desistencia, river, all-in, stack curto e torneios inteiros).");
    return EXIT_SUCCESS;
}
