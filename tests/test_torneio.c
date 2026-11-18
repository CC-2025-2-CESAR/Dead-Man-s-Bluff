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
    /* Quem responde paga: com mais fichas, não há como aumentar sobre um all-in; com menos, paga o que tem. */
    CHECK(poker_act(&table, table.actor, (PokerAction){POKER_CALL, 0}) == POKER_OK);
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
    CHECK(!torneio.segunda_vida_usada);
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    torneio_stacks(&torneio, stacks);
    CHECK(stacks[TORNEIO_ASSENTO_JOGADOR] == 2000);
    CHECK(stacks[TORNEIO_ASSENTO_ADVERSARIO] == 2000);
}

static void test_fichas_iniciais_adversario(void)
{
    CHECK(torneio_fichas_iniciais_adversario(0) == 2000);
    CHECK(torneio_fichas_iniciais_adversario(1) == 6000);
    CHECK(torneio_fichas_iniciais_adversario(2) == 10000);
    CHECK(torneio_fichas_iniciais_adversario(-1) == 0);
    CHECK(torneio_fichas_iniciais_adversario(TORNEIO_CONFRONTOS) == 0);
}

static void test_so_o_chefe_tem_segunda_vida(void)
{
    CHECK(torneio_segunda_vida(0) == 0);
    CHECK(torneio_segunda_vida(1) == 0);
    CHECK(torneio_segunda_vida(2) == 8000);
    CHECK(torneio_segunda_vida(-1) == 0);
    CHECK(torneio_segunda_vida(TORNEIO_CONFRONTOS) == 0);
}

/** Leva um torneio novo até o chefe, como quem venceu os dois primeiros sem gastar fichas. */
static void chegar_ao_chefe(Torneio *torneio)
{
    torneio_iniciar(torneio);
    CHECK(torneio_registrar_mao(torneio, 4000, 0));
    CHECK(torneio_avancar_confronto(torneio));
    CHECK(torneio_registrar_mao(torneio, 10000, 0));
    CHECK(torneio_avancar_confronto(torneio));
    CHECK(torneio->confronto == 2 && torneio->situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio->fichas_jogador == 10000 && torneio->fichas_adversario == 10000);
    CHECK(!torneio->segunda_vida_usada);
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
    CHECK(!torneio.segunda_vida_usada);
    CHECK(!torneio_registrar_mao(&torneio, 4000, 0));
    CHECK(!torneio_recuperar_jogador(&torneio));
    CHECK(torneio.dealer == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
    CHECK(torneio.confronto == 1 && torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.fichas_jogador == 4000 && torneio.fichas_adversario == 6000);

    /* O segundo adversário traz mais fichas: um all-in vencido ainda não o quebra. */
    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.fichas_jogador == 8000 && torneio.fichas_adversario == 2000);
    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO);
    CHECK(!torneio.segunda_vida_usada);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.confronto == 2);
    CHECK(torneio.fichas_jogador == 10000 && torneio.fichas_adversario == 10000);

    /* O chefe cai uma vez e se levanta com a segunda vida; só a segunda queda dá o título. */
    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.segunda_vida_usada);
    CHECK(torneio.fichas_jogador == 20000 && torneio.fichas_adversario == 8000);
    CHECK(!torneio_avancar_confronto(&torneio));

    jogar_all_in(&torneio, true);
    CHECK(torneio.situacao == TORNEIO_CAMPEAO);
    CHECK(torneio.fichas_jogador == 28000 && torneio.fichas_adversario == 0);
    CHECK(!torneio_avancar_confronto(&torneio));
    CHECK(!torneio_registrar_mao(&torneio, 28000, 0));
}

static void test_a_segunda_vida_mantem_os_blinds_e_vale_uma_vez(void)
{
    Torneio torneio;
    PokerChips blind_antes;

    chegar_ao_chefe(&torneio);

    for (int mao = 0; mao < TORNEIO_MAOS_POR_NIVEL + 2; mao++)
        CHECK(torneio_registrar_mao(&torneio, 10000, 10000));

    blind_antes = torneio_big_blind(&torneio);
    CHECK(blind_antes == 80 * TORNEIO_FATOR_BLIND);

    /* Primeira queda: 8.000 fichas novas, a contagem de mãos segue e o dealer alterna. */
    CHECK(torneio_registrar_mao(&torneio, 20000, 0));
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    CHECK(torneio.segunda_vida_usada);
    CHECK(torneio.fichas_jogador == 20000 && torneio.fichas_adversario == 8000);
    CHECK(torneio.maos_jogadas == TORNEIO_MAOS_POR_NIVEL + 3);
    CHECK(torneio.dealer == (torneio.maos_jogadas % 2 == 0 ? TORNEIO_ASSENTO_JOGADOR
                                                           : TORNEIO_ASSENTO_ADVERSARIO));
    CHECK(torneio_big_blind(&torneio) == blind_antes);
    CHECK(torneio.recuperacoes == 0);

    /* As fichas novas entram uma vez só: depois, o registro volta a não deixar fichas surgirem. */
    CHECK(!torneio_registrar_mao(&torneio, 20001, 8000));
    CHECK(torneio_registrar_mao(&torneio, 19000, 9000));
    CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);

    /* A segunda queda vence o confronto e o torneio. */
    CHECK(torneio_registrar_mao(&torneio, 28000, 0));
    CHECK(torneio.situacao == TORNEIO_CAMPEAO);
    CHECK(torneio.fichas_adversario == 0);
    CHECK(!torneio_registrar_mao(&torneio, 28000, 0));
}

static void test_falencia_contra_a_segunda_vida_do_chefe(void)
{
    Torneio torneio;

    chegar_ao_chefe(&torneio);
    CHECK(torneio_registrar_mao(&torneio, 20000, 0));
    CHECK(torneio.segunda_vida_usada && torneio.fichas_adversario == 8000);

    /* Quebrar contra a segunda vida é falência comum: volta com metade das fichas iniciais do chefe. */
    CHECK(torneio_registrar_mao(&torneio, 0, 28000));
    CHECK(torneio.situacao == TORNEIO_JOGADOR_FALIDO);
    CHECK(torneio_recuperar_jogador(&torneio));
    CHECK(torneio.fichas_jogador == 5000 && torneio.fichas_adversario == 28000);
    CHECK(torneio.segunda_vida_usada);

    /* Ele já gastou a segunda vida: a próxima queda dá o título. */
    CHECK(torneio_registrar_mao(&torneio, 33000, 0));
    CHECK(torneio.situacao == TORNEIO_CAMPEAO);
}

static void test_a_segunda_vida_e_de_cada_adversario(void)
{
    Torneio torneio;

    /* Os dois primeiros não têm segunda vida: caem de uma vez. */
    torneio_iniciar(&torneio);
    CHECK(torneio_registrar_mao(&torneio, 4000, 0));
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO && !torneio.segunda_vida_usada);

    /* O campo é do adversário do confronto: o próximo chega com ele zerado. */
    torneio.segunda_vida_usada = true;
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(!torneio.segunda_vida_usada);
}

static void test_saldo_com_gasto_em_trapacas(void)
{
    Torneio torneio;

    torneio_iniciar(&torneio);
    CHECK(torneio_registrar_mao(&torneio, 1400, 2000));
    CHECK(torneio_registrar_mao(&torneio, 3400, 0));
    CHECK(torneio.situacao == TORNEIO_CONFRONTO_VENCIDO);
    CHECK(torneio_avancar_confronto(&torneio));
    CHECK(torneio.fichas_jogador == 3400 && torneio.fichas_adversario == 6000);
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
    CHECK(torneio.fichas_adversario == 10000);
    CHECK(torneio_recuperar_jogador(&torneio));
    CHECK(torneio.fichas_jogador == 3000);
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
    test_so_o_chefe_tem_segunda_vida();
    test_blinds_sobem_por_nivel();
    test_blinds_tem_limite();
    test_dealer_alterna();
    test_registro_invalido();
    test_campanha_vencedora();
    test_a_segunda_vida_mantem_os_blinds_e_vale_uma_vez();
    test_falencia_contra_a_segunda_vida_do_chefe();
    test_a_segunda_vida_e_de_cada_adversario();
    test_saldo_com_gasto_em_trapacas();
    test_blinds_reiniciam_no_novo_confronto();
    test_falencia_e_recuperacao();
    test_recuperacao_e_metade_da_etapa();
    test_recuperacao_mantem_nivel_dos_blinds();
    puts("OK: torneio com 3 confrontos, saldo, blinds, falencia, recuperacao e segunda vida do chefe.");
    return EXIT_SUCCESS;
}
