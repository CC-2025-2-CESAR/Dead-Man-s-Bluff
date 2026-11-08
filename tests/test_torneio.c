#include "core/torneio.h"
#include "poker.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static const PokerCard forte[2] = { {14, POKER_SPADES}, {14, POKER_HEARTS} };
static const PokerCard fraca[2] = { {13, POKER_SPADES}, {13, POKER_HEARTS} };
static const PokerCard board[5] = {
    {2, POKER_CLUBS}, {7, POKER_DIAMONDS}, {9, POKER_HEARTS},
    {11, POKER_CLUBS}, {3, POKER_SPADES}
};

static void jogar_all_in(Torneio *torneio, bool jogador_vence)
{
    PokerTable table;
    PokerResult result;
    PokerChips stacks[2];
    const PokerCard *mao_jogador = jogador_vence ? forte : fraca;
    const PokerCard *mao_adversario = jogador_vence ? fraca : forte;

    torneio_stacks(torneio, stacks);
    CHECK(poker_start_hand(&table, stacks, torneio->dealer,
                           torneio_small_blind(torneio), torneio_big_blind(torneio)) == POKER_OK);
    CHECK(poker_act(&table, table.actor, (PokerAction){POKER_ALL_IN, 0}) == POKER_OK);
    CHECK(poker_act(&table, table.actor, (PokerAction){POKER_ALL_IN, 0}) == POKER_OK);
    CHECK(table.phase == POKER_SHOWDOWN);
    CHECK(poker_settle(&table, mao_jogador, mao_adversario, board, &result) == POKER_OK);
    CHECK(torneio_registrar_mao(torneio,
                                table.player[TORNEIO_ASSENTO_JOGADOR].chips,
                                table.player[TORNEIO_ASSENTO_ADVERSARIO].chips));
}

static void test_iniciar(void)
{
    Torneio torneio;
    PokerChips stacks[2];

    torneio_iniciar(&torneio);
    CHECK(torneio.confronto == 0);
    CHECK(torneio.fichas_jogador == 2000 && torneio.fichas_adversario == 2000);
    CHECK(torneio.maos_jogadas == 0 && torneio.recuperacoes == 0);
    CHECK(torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    torneio_stacks(&torneio, stacks);
    CHECK(stacks[TORNEIO_ASSENTO_JOGADOR] == 2000);
    CHECK(stacks[TORNEIO_ASSENTO_ADVERSARIO] == 2000);
}

static void test_fichas_iniciais_adversario(void)
{
    CHECK(torneio_fichas_iniciais_adversario(0) == 2000);
    CHECK(torneio_fichas_iniciais_adversario(1) == 4000);
    CHECK(torneio_fichas_iniciais_adversario(2) == 8000);
    CHECK(torneio_fichas_iniciais_adversario(-1) == 0);
    CHECK(torneio_fichas_iniciais_adversario(TORNEIO_CONFRONTOS) == 0);
}

static void test_blinds_sobem_por_nivel(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    for (int mao = 0; mao < 3 * TORNEIO_MAOS_POR_NIVEL; mao++) {
        PokerChips esperado = 20;
        for (int nivel = 0; nivel < mao / TORNEIO_MAOS_POR_NIVEL; nivel++)
            esperado *= TORNEIO_FATOR_BLIND;
        CHECK(torneio_big_blind(&torneio) == esperado);
        CHECK(torneio_small_blind(&torneio) == esperado / 2);
        CHECK(torneio_registrar_mao(&torneio, 2000, 2000));
    }
    CHECK(torneio_big_blind(&torneio) == 20 * 8);
}

static void test_blinds_tem_limite(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    for (int mao = 0; mao < 400; mao++) {
        CHECK(torneio_registrar_mao(&torneio, 2000, 2000));
        CHECK(torneio_big_blind(&torneio) > 0);
        CHECK(torneio_big_blind(&torneio) <= TORNEIO_BLIND_MAXIMO);
    }
    CHECK(torneio_big_blind(&torneio) == TORNEIO_BLIND_MAXIMO);
}

static void test_dealer_alterna(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    for (int mao = 0; mao < 6; mao++) {
        CHECK(torneio.dealer == (mao % 2 == 0 ? TORNEIO_ASSENTO_JOGADOR : TORNEIO_ASSENTO_ADVERSARIO));
        CHECK(torneio_registrar_mao(&torneio, 2000, 2000));
    }
}

static void test_registro_invalido(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    CHECK(!torneio_registrar_mao(&torneio, -1, 4001));
    CHECK(!torneio_registrar_mao(&torneio, 4001, 0));
    CHECK(!torneio_registrar_mao(&torneio, 2001, 2000));
    CHECK(!torneio_registrar_mao(&torneio, 0, 0));
    CHECK(torneio.fichas_jogador == 2000 && torneio.fichas_adversario == 2000);
    CHECK(torneio.maos_jogadas == 0 && torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
    CHECK(torneio_registrar_mao(&torneio, 1700, 2000));
    CHECK(torneio.fichas_jogador == 1700 && torneio.fichas_adversario == 2000);
    CHECK(torneio.maos_jogadas == 1 && torneio.dealer == TORNEIO_ASSENTO_ADVERSARIO);
}

static void test_campanha_vencedora(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    CHECK(!torneio_avancar_confronto(&torneio));

    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO);
    CHECK(torneio.fichas_jogador == 4000 && torneio.fichas_adversario == 0);
    CHECK(!torneio_registrar_mao(&torneio, 4000, 0));
    CHECK(!torneio_recuperar_jogador(&torneio));
    CHECK(torneio.dealer == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
    CHECK(torneio.confronto == 1 && torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.fichas_jogador == 4000 && torneio.fichas_adversario == 4000);

    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.confronto == 2);
    CHECK(torneio.fichas_jogador == 8000 && torneio.fichas_adversario == 8000);

    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_CAMPEAO);
    CHECK(torneio.fichas_jogador == 16000 && torneio.fichas_adversario == 0);
    CHECK(!torneio_avancar_confronto(&torneio));
    CHECK(!torneio_registrar_mao(&torneio, 16000, 0));
}

static void test_saldo_com_gasto_em_trapacas(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    CHECK(torneio_registrar_mao(&torneio, 1400, 2000));
    CHECK(torneio_registrar_mao(&torneio, 3400, 0));
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.fichas_jogador == 3400 && torneio.fichas_adversario == 4000);
}

static void test_blinds_reiniciam_no_novo_confronto(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    for (int mao = 0; mao < 3 * TORNEIO_MAOS_POR_NIVEL; mao++)
        CHECK(torneio_registrar_mao(&torneio, 2000, 2000));
    CHECK(torneio_big_blind(&torneio) == 20 * 8);
    CHECK(torneio_registrar_mao(&torneio, 4000, 0));
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.maos_jogadas == 0);
    CHECK(torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
    CHECK(torneio_big_blind(&torneio) == 40);
    CHECK(torneio_small_blind(&torneio) == 20);
}

static void test_falencia_e_recuperacao(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    CHECK(!torneio_recuperar_jogador(&torneio));

    jogar_all_in(&torneio, false);
    CHECK(torneio.situacao == TORNEIO_JOGADOR_FALIDO);
    CHECK(torneio.fichas_jogador == 0 && torneio.fichas_adversario == 4000);
    CHECK(!torneio_registrar_mao(&torneio, 0, 4000));
    CHECK(!torneio_avancar_confronto(&torneio));
    CHECK(torneio_recuperar_jogador(&torneio));
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.fichas_jogador == 1000 && torneio.fichas_adversario == 4000);
    CHECK(torneio.confronto == 0 && torneio.recuperacoes == 1);
    CHECK(!torneio_recuperar_jogador(&torneio));
}

static void test_recuperacao_e_metade_da_etapa(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    jogar_all_in(&torneio, true);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.confronto == 1);

    jogar_all_in(&torneio, false);
    CHECK(torneio.situacao == TORNEIO_JOGADOR_FALIDO);
    CHECK(torneio.fichas_adversario == 8000);
    CHECK(torneio_recuperar_jogador(&torneio));
    CHECK(torneio.fichas_jogador == 2000);
    CHECK(torneio.recuperacoes == 1);
}

static void test_recuperacao_mantem_nivel_dos_blinds(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    for (int mao = 0; mao < 2 * TORNEIO_MAOS_POR_NIVEL; mao++)
        CHECK(torneio_registrar_mao(&torneio, 2000, 2000));
    CHECK(torneio_registrar_mao(&torneio, 0, 2000));
    CHECK(torneio.situacao == TORNEIO_JOGADOR_FALIDO);
    CHECK(torneio_recuperar_jogador(&torneio));
    CHECK(torneio_big_blind(&torneio) == 20 * 4);
}

int main(void)
{
    test_iniciar();
    test_fichas_iniciais_adversario();
    test_blinds_sobem_por_nivel();
    test_blinds_tem_limite();
    test_dealer_alterna();
    test_registro_invalido();
    test_campanha_vencedora();
    test_saldo_com_gasto_em_trapacas();
    test_blinds_reiniciam_no_novo_confronto();
    test_falencia_e_recuperacao();
    test_recuperacao_e_metade_da_etapa();
    test_recuperacao_mantem_nivel_dos_blinds();
    puts("OK: torneio com 3 confrontos, saldo, blinds, falencia e recuperacao.");
    return EXIT_SUCCESS;
}
