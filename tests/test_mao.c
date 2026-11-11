#include "core/mao.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 120
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

static bool registrar_carta(bool vistas[52], PokerCard carta)
{
    int indice = (int)carta.suit * 13 + carta.rank - 2;

    if (carta.rank < 2 || carta.rank > 14 || indice < 0 || indice >= 52 ||
        vistas[indice]) {
        return false;
    }

    vistas[indice] = true;
    return true;
}

static bool cartas_distintas(const Mao *mao)
{
    bool vistas[52] = {false};

    for (int assento = 0; assento < 2; assento++) {
        for (int i = 0; i < 2; i++) {
            if (!registrar_carta(vistas, mao->cartas[assento][i])) {
                return false;
            }
        }
    }

    for (size_t i = 0; i < mao->cartas_no_board; i++) {
        if (!registrar_carta(vistas, mao->board[i])) {
            return false;
        }
    }

    return true;
}

static unsigned vencedores_esperados(const Mao *mao)
{
    PokerHandValue valor[2];

    CHECK(mao->cartas_no_board == MAO_CARTAS_NO_BOARD);

    for (int assento = 0; assento < 2; assento++) {
        CHECK(poker_evaluate_holdem(
                  mao->cartas[assento],
                  mao->board,
                  MAO_CARTAS_NO_BOARD,
                  &valor[assento]
              ) == POKER_OK);
    }

    int comparacao = poker_compare(&valor[0], &valor[1]);

    return comparacao > 0 ? 1u : comparacao < 0 ? 2u : 3u;
}

static PokerChips soma_das_fichas(const Mao *mao)
{
    return mao->mesa.player[0].chips + mao->mesa.player[1].chips;
}

static void conferir_showdown_completo(
    const Mao *mao,
    PokerChips total,
    PokerStreet rua_do_all_in
)
{
    CHECK(mao->situacao == MAO_ENCERRADA);
    CHECK(mao->mesa.phase == POKER_SETTLED);
    CHECK(mao->resultado.showdown);
    CHECK(mao->cartas_no_board == MAO_CARTAS_NO_BOARD);
    CHECK(mao->mesa.street == rua_do_all_in);
    CHECK(mao->baralho.head == NULL && mao->baralho.count == 0);
    CHECK(cartas_distintas(mao));
    CHECK(mao->resultado.winners == vencedores_esperados(mao));
    CHECK(soma_das_fichas(mao) == total);
}

static void test_iniciar_posta_blinds_e_da_a_vez_ao_dealer(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(1);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(mao.mesa.phase == POKER_ACTING);
    CHECK(mao.mesa.dealer == torneio.dealer);
    CHECK(mao.mesa.actor == torneio.dealer);
    CHECK(mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 2000 - 10);
    CHECK(mao.mesa.player[TORNEIO_ASSENTO_ADVERSARIO].chips == 2000 - 20);
    CHECK(mao.cartas_no_board == 0);
    CHECK(mao.baralho.count == 48);
    CHECK(cartas_distintas(&mao));

    mao_liberar(&mao);
}

static void test_iniciar_recusa_pedidos_invalidos(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);

    CHECK(!mao_iniciar(NULL, &torneio));
    CHECK(!mao_iniciar(&mao, NULL));

    torneio.situacao = TORNEIO_JOGADOR_FALIDO;
    CHECK(!mao_iniciar(&mao, &torneio));
    torneio.situacao = TORNEIO_CONFRONTO_VENCIDO;
    CHECK(!mao_iniciar(&mao, &torneio));
    torneio.situacao = TORNEIO_CAMPEAO;
    CHECK(!mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_VAZIA);

    torneio.situacao = TORNEIO_EM_ANDAMENTO;
    torneio.fichas_jogador = 0;
    CHECK(!mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_VAZIA);
    CHECK(mao.baralho.head == NULL && mao.baralho.count == 0);
}

static void test_desistencia_no_preflop_e_registro(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(2);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(mao.situacao == MAO_ENCERRADA);
    CHECK(mao.mesa.phase == POKER_SETTLED);
    CHECK(!mao.resultado.showdown);
    CHECK(mao.resultado.winners == 2u);
    CHECK(mao.cartas_no_board == 0);
    CHECK(mao.baralho.head == NULL && mao.baralho.count == 0);
    CHECK(mao.mesa.player[0].chips == 1990);
    CHECK(mao.mesa.player[1].chips == 2010);

    CHECK(mao_registrar_no_torneio(&mao, &torneio));
    CHECK(mao.situacao == MAO_REGISTRADA);
    CHECK(!mao_registrar_no_torneio(&mao, &torneio));
    CHECK(torneio.fichas_jogador == 1990 && torneio.fichas_adversario == 2010);
    CHECK(torneio.maos_jogadas == 1);
    CHECK(torneio.dealer == TORNEIO_ASSENTO_ADVERSARIO);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(mao.mesa.dealer == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO);
    CHECK(mao.mesa.player[TORNEIO_ASSENTO_ADVERSARIO].chips == 2010 - 10);
    CHECK(mao.mesa.player[TORNEIO_ASSENTO_JOGADOR].chips == 1990 - 20);
    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(mao.resultado.winners == 1u);
    CHECK(mao_registrar_no_torneio(&mao, &torneio));
    CHECK(torneio.fichas_jogador == 2000 && torneio.fichas_adversario == 2000);
    CHECK(torneio.maos_jogadas == 2);
    CHECK(torneio.dealer == TORNEIO_ASSENTO_JOGADOR);
}

static void test_registro_so_depois_de_encerrar(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(3);

    CHECK(!mao_registrar_no_torneio(&mao, &torneio));
    CHECK(!mao_registrar_no_torneio(NULL, &torneio));
    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(!mao_registrar_no_torneio(&mao, &torneio));
    CHECK(!mao_registrar_no_torneio(&mao, NULL));
    CHECK(torneio.maos_jogadas == 0);

    mao_liberar(&mao);
}

static void test_acao_recusada_nao_altera_a_mao(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(4);
    CHECK(mao_iniciar(&mao, &torneio));

    CHECK(mao_agir(&mao, PASSAR) == POKER_ERR_ACTION);
    CHECK(mao_agir(&mao, (PokerAction){POKER_RAISE, 21}) == POKER_ERR_ACTION);
    CHECK(mao_agir(&mao, (PokerAction){POKER_CALL, 5}) == POKER_ERR_ACTION);

    CHECK(mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(mao.mesa.phase == POKER_ACTING);
    CHECK(mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    CHECK(mao.mesa.player[0].chips == 1990 && mao.mesa.player[1].chips == 1980);
    CHECK(mao.mesa.current_bet == 20);
    CHECK(mao.cartas_no_board == 0 && mao.baralho.count == 48);

    CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
    CHECK(mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO);

    mao_liberar(&mao);
}

static void test_agir_sem_mao_em_andamento(void)
{
    Torneio torneio;
    Mao vazia = {0};
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(5);

    CHECK(mao_agir(NULL, PAGAR) == POKER_ERR_ARGUMENT);
    CHECK(mao_agir(&vazia, PAGAR) == POKER_ERR_STATE);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao_agir(&mao, DESISTIR) == POKER_OK);
    CHECK(mao_agir(&mao, PAGAR) == POKER_ERR_STATE);
    CHECK(mao_registrar_no_torneio(&mao, &torneio));
    CHECK(mao_agir(&mao, PAGAR) == POKER_ERR_STATE);
}

static void test_mao_ate_o_river_sem_aumentos(void)
{
    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        PokerChips esperado[2];
        unsigned vencedores;

        torneio_iniciar(&torneio);
        srand((unsigned)semente);
        CHECK(mao_iniciar(&mao, &torneio));

        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        CHECK(mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);

        CHECK(mao.mesa.street == POKER_FLOP);
        CHECK(mao.cartas_no_board == 3 && mao.baralho.count == 44);
        CHECK(mao.mesa.actor == 1 - torneio.dealer);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);

        CHECK(mao.mesa.street == POKER_TURN);
        CHECK(mao.cartas_no_board == 4 && mao.baralho.count == 42);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);

        CHECK(mao.mesa.street == POKER_RIVER);
        CHECK(mao.cartas_no_board == 5 && mao.baralho.count == 40);
        CHECK(mao.situacao == MAO_EM_ANDAMENTO);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao.situacao == MAO_EM_ANDAMENTO);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);

        conferir_showdown_completo(&mao, 4000, POKER_RIVER);
        CHECK(mao.resultado.pot == 40);

        vencedores = mao.resultado.winners;
        esperado[0] = vencedores == 1u ? 2020 : vencedores == 2u ? 1980 : 2000;
        esperado[1] = 4000 - esperado[0];
        CHECK(mao.mesa.player[0].chips == esperado[0]);
        CHECK(mao.mesa.player[1].chips == esperado[1]);

        CHECK(mao_registrar_no_torneio(&mao, &torneio));
        CHECK(torneio.fichas_jogador == esperado[0]);
        CHECK(torneio.fichas_adversario == esperado[1]);
        CHECK(torneio.situacao == TORNEIO_EM_ANDAMENTO);
    }
}

static void conferir_all_in_e_registro(
    Mao *mao,
    Torneio *torneio,
    PokerStreet rua,
    int vitorias[2]
)
{
    unsigned vencedores;

    conferir_showdown_completo(mao, 4000, rua);
    CHECK(mao->resultado.pot == 4000);

    vencedores = mao->resultado.winners;
    CHECK(mao->mesa.player[0].chips ==
          (vencedores == 1u ? 4000 : vencedores == 2u ? 0 : 2000));

    if (vencedores == 1u) {
        vitorias[0]++;
    } else if (vencedores == 2u) {
        vitorias[1]++;
    }

    CHECK(mao_registrar_no_torneio(mao, torneio));
    CHECK(torneio->situacao ==
          (vencedores == 1u ? TORNEIO_CONFRONTO_VENCIDO :
           vencedores == 2u ? TORNEIO_JOGADOR_FALIDO :
           TORNEIO_EM_ANDAMENTO));
}

static void test_all_in_antes_do_river_completa_o_board(void)
{
    int vitorias[2] = {0, 0};

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};

        torneio_iniciar(&torneio);
        srand((unsigned)semente);
        CHECK(mao_iniciar(&mao, &torneio));
        CHECK(mao_agir(&mao, ALL_IN) == POKER_OK);
        CHECK(mao.situacao == MAO_EM_ANDAMENTO);
        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        conferir_all_in_e_registro(&mao, &torneio, POKER_PREFLOP, vitorias);
    }

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};

        torneio_iniciar(&torneio);
        srand((unsigned)semente);
        CHECK(mao_iniciar(&mao, &torneio));
        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao.cartas_no_board == 3);
        CHECK(mao_agir(&mao, ALL_IN) == POKER_OK);
        CHECK(mao.situacao == MAO_EM_ANDAMENTO);
        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        conferir_all_in_e_registro(&mao, &torneio, POKER_FLOP, vitorias);
    }

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};

        torneio_iniciar(&torneio);
        srand((unsigned)semente);
        CHECK(mao_iniciar(&mao, &torneio));
        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao_agir(&mao, PASSAR) == POKER_OK);
        CHECK(mao.cartas_no_board == 4);
        CHECK(mao_agir(&mao, ALL_IN) == POKER_OK);
        CHECK(mao.situacao == MAO_EM_ANDAMENTO);
        CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
        conferir_all_in_e_registro(&mao, &torneio, POKER_TURN, vitorias);
    }

    CHECK(vitorias[0] > 0 && vitorias[1] > 0);
}

static void test_stack_menor_que_o_blind_resolve_ao_iniciar(void)
{
    PokerChips stacks_curtos[] = {10, 5, 1};

    for (size_t i = 0; i < sizeof stacks_curtos / sizeof stacks_curtos[0]; i++) {
        for (int assento = 0; assento < 2; assento++) {
            Torneio torneio;
            Mao mao = {0};
            PokerChips total;

            torneio_iniciar(&torneio);
            if (assento == TORNEIO_ASSENTO_JOGADOR) {
                torneio.fichas_jogador = stacks_curtos[i];
            } else {
                torneio.fichas_adversario = stacks_curtos[i];
            }
            total = torneio.fichas_jogador + torneio.fichas_adversario;
            srand((unsigned)(10 + i));

            CHECK(mao_iniciar(&mao, &torneio));
            conferir_showdown_completo(&mao, total, POKER_PREFLOP);
            CHECK(mao_registrar_no_torneio(&mao, &torneio));
            CHECK(torneio.maos_jogadas == 1);
        }
    }
}

static void test_stack_entre_small_e_big_blind_exige_resposta(void)
{
    Torneio torneio;
    Mao mao = {0};
    PokerLegalActions legais;

    torneio_iniciar(&torneio);
    torneio.fichas_adversario = 15;
    srand(7);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    CHECK(mao.mesa.player[TORNEIO_ASSENTO_ADVERSARIO].chips == 0);
    CHECK(poker_legal_actions(&mao.mesa, TORNEIO_ASSENTO_JOGADOR, &legais) == POKER_OK);
    CHECK(legais.call && legais.call_cost == 5);

    CHECK(mao_agir(&mao, PAGAR) == POKER_OK);
    conferir_showdown_completo(&mao, 2015, POKER_PREFLOP);
    CHECK(mao.resultado.pot == 30);
    CHECK(mao_registrar_no_torneio(&mao, &torneio));
}

static void test_liberar_mao_abandonada(void)
{
    Torneio torneio;
    Mao mao = {0};

    torneio_iniciar(&torneio);
    srand(8);

    mao_liberar(NULL);
    mao_liberar(&mao);
    CHECK(mao.situacao == MAO_VAZIA);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.baralho.count == 48);
    mao_liberar(&mao);
    mao_liberar(&mao);
    CHECK(mao.baralho.head == NULL && mao.baralho.count == 0);
    CHECK(mao.situacao == MAO_VAZIA);
    CHECK(mao_agir(&mao, PAGAR) == POKER_ERR_STATE);

    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao_iniciar(&mao, &torneio));
    CHECK(mao.situacao == MAO_EM_ANDAMENTO);
    CHECK(mao.baralho.count == 48);
    mao_liberar(&mao);
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

static void jogar_mao_aleatoria(Mao *mao, unsigned *semente)
{
    for (int acoes = 0; acoes < LIMITE_DE_ACOES; acoes++) {
        if (mao->situacao != MAO_EM_ANDAMENTO) {
            return;
        }

        CHECK(mao_agir(mao, escolher_acao_aleatoria(mao, semente)) == POKER_OK);
    }

    CHECK(mao->situacao != MAO_EM_ANDAMENTO);
}

static void test_torneio_completo_com_acoes_aleatorias(void)
{
    int confrontos_vencidos = 0;
    int falencias = 0;
    int campeoes = 0;

    for (int semente = 1; semente <= SEMENTES; semente++) {
        Torneio torneio;
        Mao mao = {0};
        unsigned sorteio = (unsigned)semente * 7919u + 13u;
        int maos = 0;
        bool terminou = false;

        torneio_iniciar(&torneio);
        srand((unsigned)semente);

        while (!terminou) {
            PokerChips antes = torneio.fichas_jogador + torneio.fichas_adversario;

            CHECK(++maos <= LIMITE_DE_MAOS);
            CHECK(mao_iniciar(&mao, &torneio));
            jogar_mao_aleatoria(&mao, &sorteio);
            CHECK(mao.situacao == MAO_ENCERRADA);
            CHECK(soma_das_fichas(&mao) == antes);
            CHECK(mao_registrar_no_torneio(&mao, &torneio));
            CHECK(torneio.fichas_jogador + torneio.fichas_adversario == antes);

            switch (torneio.situacao) {
                case TORNEIO_EM_ANDAMENTO:
                    break;

                case TORNEIO_CONFRONTO_VENCIDO:
                    confrontos_vencidos++;
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

    CHECK(confrontos_vencidos > 0);
    CHECK(falencias > 0);
    CHECK(campeoes > 0);
}

int main(void)
{
    test_iniciar_posta_blinds_e_da_a_vez_ao_dealer();
    test_iniciar_recusa_pedidos_invalidos();
    test_desistencia_no_preflop_e_registro();
    test_registro_so_depois_de_encerrar();
    test_acao_recusada_nao_altera_a_mao();
    test_agir_sem_mao_em_andamento();
    test_mao_ate_o_river_sem_aumentos();
    test_all_in_antes_do_river_completa_o_board();
    test_stack_menor_que_o_blind_resolve_ao_iniciar();
    test_stack_entre_small_e_big_blind_exige_resposta();
    test_liberar_mao_abandonada();
    test_torneio_completo_com_acoes_aleatorias();
    puts("OK: mao completa com baralho, motor e torneio (desistencia, river, all-in em cada rua, stacks curtos e torneios inteiros).");
    return EXIT_SUCCESS;
}
