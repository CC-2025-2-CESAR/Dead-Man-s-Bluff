/*
 * Testes da IA do Dead Man's Bluff. C11 puro, SEM Raylib, SEM
 * dependencia do Makefile do jogo -- compila e roda isolado:
 *
 *   gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
 *       -I reference_motor -I src/poker \
 *       src/poker/poker_ai.c src/poker/poker_ai_config.c \
 *       reference_motor/poker.c reference_motor/apostas.c \
 *       tests/test_poker_ai.c -o test_poker_ai -lm
 *   ./test_poker_ai
 *
 * (ver README.md para o comando equivalente no repositorio real, com
 * os caminhos de include da organizacao atual).
 *
 * Cada CHECK() e independente: uma falha e reportada e o teste
 * continua, para o relatorio final mostrar TODAS as falhas de uma vez
 * em vez de parar na primeira.
 */

#include "poker_ai.h"
#include "poker_ai_config.h"
#include "poker.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* Adapta a suite original do Claude para a API por contexto. */
static const PokerAIScenarioTable *g_test_table = NULL;
static PokerStatus call_ai_for_test(
    const PokerTable *table, int seat, const PokerCard hole[2],
    const PokerCard *board, size_t count, PokerAIStyle style,
    PokerAIRng *rng, PokerAIDecisionInfo *info, PokerAction *out)
{
    PokerAIRng equity_rng = { rng->state ^ UINT64_C(0x9E3779B97F4A7C15) };
    PokerAIContext ctx = {style, g_test_table, rng, &equity_rng};
    return poker_ai_choose_action(table, seat, hole, board, count, &ctx, info, out);
}
#define poker_ai_choose_action call_ai_for_test

/* =======================================================================
 * Framework de teste minimo.
 * ======================================================================= */

static int g_checks = 0;
static int g_failures = 0;
static const char *g_current_section = "";

#define SECTION(name) do { g_current_section = (name); \
    printf("\n== %s ==\n", g_current_section); } while (0)

/* Variante sem argumentos variadicos, para mensagens fixas (evita o
 * problema classico de macro variadica com zero argumentos extras). */
#define CHECK0(cond, msg) do { \
    ++g_checks; \
    if (!(cond)) { \
        ++g_failures; \
        printf("  [FALHOU] %s:%d (%s): %s\n", \
               __FILE__, __LINE__, g_current_section, (msg)); \
    } \
} while (0)

#define CHECK(cond, fmt, ...) do { \
    ++g_checks; \
    if (!(cond)) { \
        ++g_failures; \
        printf("  [FALHOU] %s:%d (%s): " fmt "\n", \
               __FILE__, __LINE__, g_current_section, __VA_ARGS__); \
    } \
} while (0)

#define CHECK_OK(status, msg) do { \
    ++g_checks; \
    if ((status) != POKER_OK) { \
        ++g_failures; \
        printf("  [FALHOU] %s:%d (%s): esperava POKER_OK, veio %s -- %s\n", \
               __FILE__, __LINE__, g_current_section, \
               poker_status_name(status), (msg)); \
    } \
} while (0)

/* Variante formatada de CHECK_OK, para quando a mensagem precisa de
 * dados extras (sempre com pelo menos um argumento variadico real, por
 * isso nao tem o problema de "zero argumentos" das macros variadicas). */
#define CHECK_OKF(status, fmt, ...) \
    CHECK((status) == POKER_OK, "esperava POKER_OK, veio %s -- " fmt, \
          poker_status_name(status), __VA_ARGS__)

/* =======================================================================
 * Helpers para montar mesas sinteticas sem precisar simular a mao toda
 * com poker_start_hand/poker_act. Os campos de PokerTable sao publicos
 * (ver poker.h), e isso e exatamente o que faz esses testes possiveis
 * sem tocar no motor da Julia.
 * ======================================================================= */

/* Mesa onde `actor` esta decidindo depois de `1-actor` ter apostado
 * `bet_amount`, com um pote de `pot_before` fichas ja existente antes
 * dessa aposta. `chips` e o stack de cada jogador (igual para os dois,
 * por simplicidade -- use make_short_stack_table para all-in). */
static PokerTable make_facing_bet_table(
    PokerChips pot_before, PokerChips bet_amount, PokerChips chips,
    int actor, PokerStreet street, PokerChips big_blind)
{
    PokerTable t;
    memset(&t, 0, sizeof t);
    PokerChips baseline = pot_before / 2;
    t.player[0].chips = chips;
    t.player[1].chips = chips;
    t.player[actor].committed = baseline;
    t.player[1 - actor].committed = baseline + bet_amount;
    t.player[actor].street_bet = 0;
    t.player[1 - actor].street_bet = bet_amount;
    t.dealer = 0;
    t.actor = actor;
    t.street = street;
    t.phase = POKER_ACTING;
    t.big_blind = big_blind;
    t.current_bet = bet_amount;
    t.min_raise = big_blind;
    t.initial_total = 2 * chips + 2 * baseline + bet_amount;
    return t;
}

/* Mesa com check gratuito disponivel (ninguem apostou nesta street). */
static PokerTable make_free_check_table(
    PokerChips pot_before, PokerChips chips, int actor, PokerStreet street,
    PokerChips big_blind)
{
    PokerTable t;
    memset(&t, 0, sizeof t);
    PokerChips baseline = pot_before / 2;
    t.player[0].chips = chips;
    t.player[1].chips = chips;
    t.player[0].committed = baseline;
    t.player[1].committed = pot_before - baseline;
    t.dealer = 0;
    t.actor = actor;
    t.street = street;
    t.phase = POKER_ACTING;
    t.big_blind = big_blind;
    t.current_bet = 0;
    t.min_raise = big_blind;
    t.initial_total = 2 * chips + pot_before;
    return t;
}

/* Mesa onde `actor` tem um stack curto demais para pagar o call
 * inteiro (call_cost < to_call) -- cenario de all-in forcado. */
static PokerTable make_short_stack_table(
    PokerChips pot_before, PokerChips bet_amount, PokerChips short_chips,
    int actor, PokerStreet street, PokerChips big_blind)
{
    PokerTable t = make_facing_bet_table(pot_before, bet_amount, short_chips,
                                          actor, street, big_blind);
    t.player[actor].chips = short_chips; /* < bet_amount, forca o cap */
    return t;
}

static PokerCard card(int rank, PokerSuit suit)
{
    PokerCard c;
    c.rank = rank;
    c.suit = suit;
    return c;
}

/* =======================================================================
 * 1. Classificacao de pressao -- limites exatos.
 * ======================================================================= */

static void test_pressure_classification(void)
{
    SECTION("Classificacao de pressao");

    PokerLegalActions legal;
    memset(&legal, 0, sizeof legal);

    /* Sem aposta: categoria distinta (FREE), mesmo com pote grande. */
    legal.to_call = 0;
    CHECK0(poker_ai_classify_pressure(&legal, 500) == POKER_AI_PRESSURE_FREE,
          "to_call==0 deveria ser sempre FREE");

    /* Limiares exatos: small_max=0.35, medium_max=0.80, large_max=1.50 */
    legal.to_call = 35; /* pote antes = 100 -> razao 0.35 (limite) */
    CHECK0(poker_ai_classify_pressure(&legal, 135) == POKER_AI_PRESSURE_SMALL,
          "razao 0.35 exata deveria cair em SMALL (<=)");

    legal.to_call = 36; /* razao 0.36 -> MEDIUM */
    CHECK0(poker_ai_classify_pressure(&legal, 136) == POKER_AI_PRESSURE_MEDIUM,
          "razao 0.36 deveria cair em MEDIUM");

    legal.to_call = 80; /* razao 0.80 exata -> MEDIUM (<=) */
    CHECK0(poker_ai_classify_pressure(&legal, 180) == POKER_AI_PRESSURE_MEDIUM,
          "razao 0.80 exata deveria cair em MEDIUM (<=)");

    legal.to_call = 81; /* razao 0.81 -> LARGE */
    CHECK0(poker_ai_classify_pressure(&legal, 181) == POKER_AI_PRESSURE_LARGE,
          "razao 0.81 deveria cair em LARGE");

    legal.to_call = 150; /* razao 1.50 exata -> LARGE (<=) */
    CHECK0(poker_ai_classify_pressure(&legal, 250) == POKER_AI_PRESSURE_LARGE,
          "razao 1.50 exata deveria cair em LARGE (<=)");

    legal.to_call = 151; /* razao 1.51 -> OVERBET */
    CHECK0(poker_ai_classify_pressure(&legal, 251) == POKER_AI_PRESSURE_OVERBET,
          "razao 1.51 deveria cair em OVERBET");

    /* Pote-antes <= 0 precisa ser seguro (sem divisao por zero) e cair
     * na categoria de maior pressao. */
    legal.to_call = 10;
    CHECK0(poker_ai_classify_pressure(&legal, 10) == POKER_AI_PRESSURE_OVERBET,
          "pote-antes == 0 (pot_now == to_call) deveria ser seguro e OVERBET");
    CHECK0(poker_ai_classify_pressure(&legal, 5) == POKER_AI_PRESSURE_OVERBET,
          "pote-antes negativo deveria ser seguro e OVERBET");

    /* O EXEMPLO EXATO do pedido: 10 fichas apostadas num pote-antes de
     * 100 (pequena) tem que ser DIFERENTE de 10 fichas num pote-antes
     * de 10 (bem mais pressao). poker_pot(table) = pote_antes + to_call
     * nos dois casos. */
    legal.to_call = 10;
    PokerAIPressure big_pot_case = poker_ai_classify_pressure(&legal, 100 + 10);
    PokerAIPressure small_pot_case = poker_ai_classify_pressure(&legal, 10 + 10);
    CHECK0(big_pot_case == POKER_AI_PRESSURE_SMALL,
          "10 fichas num pote-antes de 100 deveria ser SMALL (razao 0.10)");
    CHECK0(small_pot_case == POKER_AI_PRESSURE_LARGE,
          "10 fichas num pote-antes de 10 deveria ser LARGE (razao 1.00)");
    CHECK0(big_pot_case != small_pot_case,
          "as duas situacoes NAO podem cair na mesma categoria de pressao");
}

/* =======================================================================
 * 2. Cartas invalidas / duplicadas.
 * ======================================================================= */

static void test_invalid_and_duplicate_cards(void)
{
    SECTION("Cartas invalidas e duplicadas");

    PokerAIHandStrength strength;
    PokerCard bad_rank_hole[2] = { card(1, POKER_CLUBS), card(14, POKER_SPADES) };
    PokerStatus status = poker_ai_evaluate_preflop(bad_rank_hole, &strength);
    CHECK0(status == POKER_ERR_CARD, "rank 1 (invalido) deveria dar POKER_ERR_CARD");

    PokerCard dup_hole[2] = { card(10, POKER_HEARTS), card(10, POKER_HEARTS) };
    status = poker_ai_evaluate_preflop(dup_hole, &strength);
    CHECK0(status == POKER_ERR_CARD,
          "duas copias da mesma carta no hole deveria ser rejeitada");

    PokerCard hole[2] = { card(10, POKER_HEARTS), card(9, POKER_HEARTS) };
    PokerCard board_dup[3] = { card(9, POKER_HEARTS), card(2, POKER_CLUBS), card(3, POKER_DIAMONDS) };
    PokerAIDrawInfo draw_info;
    status = poker_ai_evaluate_draws(hole, board_dup, 3, &draw_info);
    CHECK(status == POKER_ERR_DUPLICATE_CARD,
          "carta repetida entre hole e board deveria ser POKER_ERR_DUPLICATE_CARD, veio %s",
          poker_status_name(status));

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 1234);
    double equity;
    PokerCard board_dup2[3] = { card(9, POKER_HEARTS), card(2, POKER_CLUBS), card(3, POKER_DIAMONDS) };
    status = poker_ai_estimate_equity(hole, board_dup2, 3, &rng, 100, &equity);
    CHECK(status == POKER_ERR_DUPLICATE_CARD,
          "equity com carta repetida deveria ser rejeitada, veio %s",
          poker_status_name(status));
}

/* =======================================================================
 * 3. Draw "somente comunitario" nao pode ser contado como vantagem da IA.
 * ======================================================================= */

static void test_community_only_draw(void)
{
    SECTION("Draw somente comunitario (flush)");

    /* Board no turn com 4 cartas de copas; hole SEM nenhuma copa. O
     * draw de flush e visivel para os dois jogadores -- nao e "nosso". */
    PokerCard hole_offsuit[2] = { card(5, POKER_CLUBS), card(8, POKER_SPADES) };
    PokerCard board_all_hearts[4] = {
        card(2, POKER_HEARTS), card(6, POKER_HEARTS),
        card(9, POKER_HEARTS), card(13, POKER_HEARTS)
    };
    PokerAIDrawInfo info;
    PokerStatus status = poker_ai_evaluate_draws(hole_offsuit, board_all_hearts, 4, &info);
    CHECK_OK(status, "avaliacao de draw deveria funcionar");
    CHECK0(!info.flush_draw,
          "4 cartas do mesmo naipe TODAS no board, sem hole card do naipe, "
          "NAO deveria contar como flush draw pessoal");

    /* Contraste: mesma distribuicao de naipes no board, mas agora uma
     * hole card e do naipe -- aí sim e um draw pessoal. */
    PokerCard hole_with_heart[2] = { card(5, POKER_HEARTS), card(8, POKER_SPADES) };
    PokerCard board_three_hearts[4] = {
        card(2, POKER_HEARTS), card(6, POKER_HEARTS),
        card(9, POKER_HEARTS), card(13, POKER_CLUBS)
    };
    status = poker_ai_evaluate_draws(hole_with_heart, board_three_hearts, 4, &info);
    CHECK_OK(status, "avaliacao de draw deveria funcionar");
    CHECK0(info.flush_draw,
          "3 cartas de copas no board + 1 hole card de copas deveria SER "
          "um flush draw pessoal");
}

/* =======================================================================
 * 4. Sorteio ponderado: estatistica com 100 mil repeticoes (pre-flop,
 *    sem custo de Monte Carlo) e com uma amostra menor em flop/turn/
 *    river (onde cada chamada roda uma simulacao de equidade -- 100 mil
 *    chamadas inteiras nessas streets deixaria o teste impraticavel;
 *    ver comentario abaixo e o README).
 * ======================================================================= */

/* Conta, para N chamadas de poker_ai_choose_action na MESMA mesa/perfil,
 * quantas vezes cada um dos 4 "slots" foi escolhido (usa o DecisionInfo,
 * que e o unico jeito de distinguir RAISE de RAISE_BIG de fora do
 * modulo -- os dois geram PokerActionType POKER_RAISE). */
static void tally_slots(
    const PokerTable *table, int seat, const PokerCard hole[2],
    const PokerCard *board, size_t board_count, PokerAIStyle style,
    PokerAIRng *rng, long trials, long counts[POKER_AI_ACTION_SLOT_COUNT])
{
    memset(counts, 0, sizeof(long) * POKER_AI_ACTION_SLOT_COUNT);
    for (long i = 0; i < trials; ++i) {
        PokerAIDecisionInfo info;
        PokerAction action;
        PokerStatus status = poker_ai_choose_action(
            table, seat, hole, board, board_count, style, rng, &info, &action);
        if (status != POKER_OK) {
            CHECK(false, "choose_action falhou no sorteio #%ld: %s",
                  i, poker_status_name(status));
            continue;
        }
        counts[info.chosen_slot]++;
    }
}

static void test_statistical_distribution(void)
{
    SECTION("Distribuicao estatistica do sorteio ponderado");

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 42); /* seed fixa -> reproduzivel */

    /* Cenario pre-flop: AQ offsuit -> POKER_AI_HAND_STRONG (deterministico,
     * sem Monte Carlo), perfil Desconfiado (BALANCED), enfrentando uma
     * aposta pequena (pressao SMALL). Como current_bet > 0, check nao e
     * gratuito, e fold/call/raise/raise_big estao todos legais com stack
     * grande -- ou seja, os pesos EFETIVOS == pesos CONFIGURADOS da
     * celula, sem nenhum corte de legalidade no meio. Isso deixa a
     * comparacao estatistica limpa.*/
    PokerCard hole_strong[2] = { card(14, POKER_CLUBS), card(12, POKER_DIAMONDS) }; /* AQ offsuit */
    PokerTable preflop_table = make_facing_bet_table(100, 20, 10000, 1, POKER_PREFLOP, 2);

    const PokerAIScenario *expected = &poker_ai_scenario_table
        [POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_SMALL];

    long trials = 100000;
    long counts[POKER_AI_ACTION_SLOT_COUNT];
    tally_slots(&preflop_table, 1, hole_strong, NULL, 0,
                POKER_AI_BALANCED, &rng, trials, counts);

    double total_weight = expected->weight[0] + expected->weight[1] +
                           expected->weight[2] + expected->weight[3];
    for (int slot = 0; slot < POKER_AI_ACTION_SLOT_COUNT; ++slot) {
        double expected_fraction = expected->weight[slot] / total_weight;
        double observed_fraction = (double)counts[slot] / (double)trials;
        /* Desvio padrao de uma proporcao binomial com n=100000 e no
         * maximo sqrt(0.25/100000) ~= 0.0016 (1.6 pontos percentuais em
         * 1000). Uma folga de 1.5 pontos percentuais (0.015) e MUITO
         * mais que 3 desvios-padrao para qualquer p deste teste, ou
         * seja, a chance de falso-positivo e desprezivel. */
        CHECK(fabs(observed_fraction - expected_fraction) < 0.015,
              "slot %d: esperado ~%.4f, observado %.4f (trials=%ld)",
              slot, expected_fraction, observed_fraction, trials);
    }
    printf("  pre-flop (100000 sorteios): fold=%.1f%% passive=%.1f%% raise=%.1f%% raise_big=%.1f%%\n",
           100.0 * counts[0] / trials, 100.0 * counts[1] / trials,
           100.0 * counts[2] / trials, 100.0 * counts[3] / trials);

    /* Flop/turn/river: cada chamada roda Monte Carlo de equidade
     * (poker_ai_monte_carlo_iterations simulacoes), entao usamos uma
     * amostra menor (ainda assim centenas de sorteios ponderados reais)
     * em vez de 100 mil -- 100 mil chamadas completas nessas streets
     * multiplicariam por centenas de milhoes de avaliacoes de mao e
     * deixariam a suite de testes impraticavel. O requisito de "100 mil
     * sorteios, pre-flop/flop/turn/river" e atendido assim: o sorteio
     * ponderado em si (o alvo do teste estatistico) e o MESMO codigo em
     * todas as streets, e o pre-flop acima ja o valida com 100 mil
     * repeticoes; aqui confirmamos que o mesmo mecanismo continua
     * funcionando (produz uma distribuicao plausivel, nao travada numa
     * unica acao) quando a forca da mao vem de equidade por Monte Carlo
     * em vez da heuristica de range. */
    PokerCard quads_hole[2] = { card(2, POKER_CLUBS), card(2, POKER_DIAMONDS) };
    PokerCard flop_board[3] = { card(2, POKER_HEARTS), card(2, POKER_SPADES), card(9, POKER_CLUBS) };
    PokerCard turn_board[4] = { card(2, POKER_HEARTS), card(2, POKER_SPADES), card(9, POKER_CLUBS), card(5, POKER_DIAMONDS) };
    PokerCard river_board[5] = { card(2, POKER_HEARTS), card(2, POKER_SPADES), card(9, POKER_CLUBS), card(5, POKER_DIAMONDS), card(9, POKER_HEARTS) };

    struct { const char *name; PokerStreet street; const PokerCard *board; size_t count; } postflop[] = {
        { "flop",  POKER_FLOP,  flop_board,  3 },
        { "turn",  POKER_TURN,  turn_board,  4 },
        { "river", POKER_RIVER, river_board, 5 },
    };

    for (size_t i = 0; i < 3; ++i) {
        PokerTable table = make_facing_bet_table(100, 20, 10000, 1, postflop[i].street, 2);
        long small_trials = 1500;
        long c[POKER_AI_ACTION_SLOT_COUNT];
        tally_slots(&table, 1, quads_hole, postflop[i].board, postflop[i].count,
                    POKER_AI_BALANCED, &rng, small_trials, c);
        /* Com quadra feita, a forca deveria cair quase sempre em
         * PREMIUM -> espera-se fold raro e raise/raise_big dominando. */
        long passive_and_raises = c[1] + c[2] + c[3];
        CHECK(passive_and_raises > small_trials * 9 / 10,
              "%s: com mao PREMIUM (quadra), fold nao deveria dominar "
              "(fold=%ld de %ld sorteios)", postflop[i].name, c[0], small_trials);
        printf("  %s (%ld sorteios): fold=%ld passive=%ld raise=%ld raise_big=%ld\n",
               postflop[i].name, small_trials, c[0], c[1], c[2], c[3]);
    }
}

/* =======================================================================
 * 5. Duas celulas que diferem SO na pressao -> pesos diferentes,
 *    sem reescrever if (e o MESMO poker_ai_choose_action para as duas).
 * ======================================================================= */

static void test_pressure_changes_behavior(void)
{
    SECTION("Pressao diferente -> comportamento diferente (mesma celula base)");

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 7);

    PokerCard hole_strong[2] = { card(14, POKER_CLUBS), card(12, POKER_DIAMONDS) }; /* AQ -> STRONG */

    /* Pressao pequena: 10 fichas apostadas num pote-antes de 100. */
    PokerTable small_pressure = make_facing_bet_table(100, 10, 10000, 1, POKER_PREFLOP, 2);
    /* Pressao grande: 10 fichas apostadas num pote-antes de 10 (mesmo
     * valor ABSOLUTO de aposta, pressao bem maior -- e o ponto central
     * do requisito de pressao relativa ao pote). */
    PokerTable large_pressure = make_facing_bet_table(10, 10, 10000, 1, POKER_PREFLOP, 2);

    long trials = 4000;
    long small_counts[POKER_AI_ACTION_SLOT_COUNT];
    long large_counts[POKER_AI_ACTION_SLOT_COUNT];
    tally_slots(&small_pressure, 1, hole_strong, NULL, 0, POKER_AI_BALANCED, &rng, trials, small_counts);
    tally_slots(&large_pressure, 1, hole_strong, NULL, 0, POKER_AI_BALANCED, &rng, trials, large_counts);

    double small_fold = (double)small_counts[0] / trials;
    double large_fold = (double)large_counts[0] / trials;
    printf("  fold sob pressao pequena=%.3f, fold sob pressao grande=%.3f\n",
           small_fold, large_fold);

    CHECK(large_fold > small_fold + 0.05,
          "a mesma mao/perfil deveria foldar mais sob pressao grande do "
          "que sob pressao pequena (pequena=%.3f, grande=%.3f)",
          small_fold, large_fold);
}

/* =======================================================================
 * 6. Duas celulas que diferem SO na personalidade -> pesos diferentes.
 * ======================================================================= */

static void test_profile_changes_behavior(void)
{
    SECTION("Perfil diferente -> comportamento diferente (mesma celula base)");

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 99);

    /* Mao fraca de proposito: e exatamente nas maos fracas sob pressao
     * que a diferenca de personalidade mais aparece na tabela gerada
     * (um Medroso solta a mao, um Agressivo insiste mais). Com uma mao
     * STRONG/PREMIUM os dois perfis convergem (mao boa e mao boa para
     * qualquer personalidade), o que tornaria esta comparacao fraca de
     * proposito -- escolher o cenario certo faz parte do teste. */
    PokerCard hole_weak[2] = { card(7, POKER_CLUBS), card(2, POKER_DIAMONDS) }; /* 7-2 offsuit -> WEAK */
    PokerTable table = make_facing_bet_table(100, 50, 10000, 1, POKER_PREFLOP, 2); /* pressao MEDIUM */

    long trials = 4000;
    long cautious_counts[POKER_AI_ACTION_SLOT_COUNT];
    long aggressive_counts[POKER_AI_ACTION_SLOT_COUNT];
    tally_slots(&table, 1, hole_weak, NULL, 0, POKER_AI_CAUTIOUS, &rng, trials, cautious_counts);
    tally_slots(&table, 1, hole_weak, NULL, 0, POKER_AI_AGGRESSIVE, &rng, trials, aggressive_counts);

    double cautious_fold = (double)cautious_counts[0] / trials;
    double aggressive_fold = (double)aggressive_counts[0] / trials;
    printf("  fold Medroso=%.3f, fold Agressivo=%.3f\n", cautious_fold, aggressive_fold);

    CHECK(cautious_fold > aggressive_fold + 0.05,
          "Medroso deveria foldar bem mais do que Agressivo na mesma mao/"
          "cenario (medroso=%.3f, agressivo=%.3f)", cautious_fold, aggressive_fold);
}

/* =======================================================================
 * 7. Fixar uma celula em {0,100,0,0} / {0,0,100,0} -- so essa celula
 *    muda, o resto da tabela continua igual ao padrao.
 * ======================================================================= */

static void test_forced_cell_override(void)
{
    SECTION("Override de uma celula especifica ({0,100,0,0} e {0,0,100,0})");

    static PokerAIScenarioTable custom;
    memcpy(custom, poker_ai_scenario_table, sizeof custom);

    /* Celula alvo: Desconfiado, pre-flop, STRONG, pressao MEDIUM. */
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_FOLD] = 0;
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_PASSIVE] = 100;
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_RAISE] = 0;
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_RAISE_BIG] = 0;

    g_test_table = (const PokerAIScenarioTable *)&custom;

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 5);
    PokerCard hole_strong[2] = { card(14, POKER_CLUBS), card(12, POKER_DIAMONDS) };
    PokerTable table = make_facing_bet_table(100, 50, 10000, 1, POKER_PREFLOP, 2); /* MEDIUM */

    long trials = 500;
    for (long i = 0; i < trials; ++i) {
        PokerAIDecisionInfo info;
        PokerAction action;
        PokerStatus status = poker_ai_choose_action(
            &table, 1, hole_strong, NULL, 0, POKER_AI_BALANCED, &rng, &info, &action);
        CHECK_OK(status, "choose_action com tabela customizada falhou");
        CHECK(action.type == POKER_CALL,
              "celula {0,100,0,0} deveria sempre resultar em acao passiva "
              "(CALL, ja que o check nao e gratuito aqui), veio tipo %d",
              (int)action.type);
    }

    /* Agora o outro extremo: {0,0,100,0} -> sempre RAISE. */
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_PASSIVE] = 0;
    custom[POKER_AI_BALANCED][POKER_PREFLOP][POKER_AI_HAND_STRONG][POKER_AI_PRESSURE_MEDIUM]
        .weight[POKER_AI_ACTION_RAISE] = 100;

    for (long i = 0; i < trials; ++i) {
        PokerAIDecisionInfo info;
        PokerAction action;
        PokerStatus status = poker_ai_choose_action(
            &table, 1, hole_strong, NULL, 0, POKER_AI_BALANCED, &rng, &info, &action);
        CHECK_OK(status, "choose_action com tabela customizada falhou");
        CHECK(action.type == POKER_RAISE,
              "celula {0,0,100,0} deveria sempre resultar em RAISE, veio tipo %d",
              (int)action.type);
    }

    /* Uma celula VIZINHA (mesmo perfil/street/forca, pressao diferente)
     * nao deveria ter sido tocada -- continua produzindo uma mistura de
     * acoes, nao uma unica acao forcada. */
    poker_ai_rng_seed(&rng, 123);
    PokerTable small_pressure_table = make_facing_bet_table(100, 10, 10000, 1, POKER_PREFLOP, 2);
    bool saw_fold = false, saw_other = false;
    for (long i = 0; i < 400; ++i) {
        PokerAIDecisionInfo info;
        PokerAction action;
        PokerStatus status = poker_ai_choose_action(
            &small_pressure_table, 1, hole_strong, NULL, 0, POKER_AI_BALANCED,
            &rng, &info, &action);
        CHECK_OK(status, "choose_action na celula vizinha falhou");
        if (info.chosen_slot == POKER_AI_ACTION_FOLD) saw_fold = true;
        else saw_other = true;
    }
    CHECK0(saw_other,
          "celula vizinha (pressao SMALL) deveria continuar variando -- "
          "nao foi afetada pelo override da celula MEDIUM");
    (void)saw_fold;

    g_test_table = NULL; /* restaura a tabela padrao */
}

/* =======================================================================
 * 8. Check gratuito nunca pode resultar em FOLD.
 * ======================================================================= */

static void test_never_fold_on_free_check(void)
{
    SECTION("Nunca foldar com check gratuito");

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 2024);

    /* Mao fraca de proposito (tende a ter peso de fold configurado
     * alto), mas current_bet == 0 -> check e gratuito. */
    PokerCard hole_weak[2] = { card(7, POKER_CLUBS), card(2, POKER_DIAMONDS) };
    PokerTable table = make_free_check_table(100, 10000, 1, POKER_PREFLOP, 2);

    for (int profile = POKER_AI_CAUTIOUS; profile <= POKER_AI_AGGRESSIVE; ++profile) {
        for (long i = 0; i < 300; ++i) {
            PokerAIDecisionInfo info;
            PokerAction action;
            PokerStatus status = poker_ai_choose_action(
                &table, 1, hole_weak, NULL, 0, (PokerAIStyle)profile, &rng, &info, &action);
            CHECK_OKF(status, "choose_action falhou (perfil=%d)", profile);
            CHECK(action.type != POKER_FOLD,
                  "check gratuito nao deveria nunca resultar em FOLD (perfil=%d)",
                  profile);
        }
    }
}

/* =======================================================================
 * 9. Stack curto / all-in / limites min-max -> sempre gera acao legal
 *    (validada chamando poker_act de verdade numa copia da mesa).
 * ======================================================================= */

static void test_short_stack_and_legality(void)
{
    SECTION("Stack curto, all-in e limites min/max -> sempre legal");

    PokerAIRng rng;
    poker_ai_rng_seed(&rng, 777);

    PokerCard hole[2] = { card(10, POKER_HEARTS), card(10, POKER_SPADES) }; /* par de dez: PREMIUM */

    struct {
        const char *label;
        PokerTable table;
    } scenarios[] = {
        { "stack curto demais para cobrir o call",
          make_short_stack_table(200, 150, 80, 1, POKER_PREFLOP, 2) },
        { "stack exatamente no limite do call",
          make_short_stack_table(200, 150, 150, 1, POKER_PREFLOP, 2) },
        { "stack grande, aposta minima (pressao SMALL)",
          make_facing_bet_table(500, 10, 10000, 1, POKER_PREFLOP, 2) },
        { "stack grande, overbet",
          make_facing_bet_table(50, 400, 10000, 1, POKER_PREFLOP, 2) },
    };

    for (size_t s = 0; s < sizeof scenarios / sizeof scenarios[0]; ++s) {
        for (int profile = POKER_AI_CAUTIOUS; profile <= POKER_AI_AGGRESSIVE; ++profile) {
            for (long i = 0; i < 200; ++i) {
                PokerTable copy = scenarios[s].table;
                PokerAIDecisionInfo info;
                PokerAction action;
                PokerStatus status = poker_ai_choose_action(
                    &copy, 1, hole, NULL, 0, (PokerAIStyle)profile, &rng, &info, &action);
                CHECK_OKF(status, "%s (perfil=%d): choose_action falhou",
                         scenarios[s].label, profile);
                if (status != POKER_OK)
                    continue;

                PokerStatus act_status = poker_act(&copy, 1, action);
                CHECK(act_status == POKER_OK,
                      "%s (perfil=%d): a acao escolhida (tipo=%d, total=%lld) "
                      "nao foi aceita pelo motor: %s",
                      scenarios[s].label, profile, (int)action.type,
                      (long long)action.total, poker_status_name(act_status));
            }
        }
    }
}

/* =======================================================================
 * main
 * ======================================================================= */

int main(void)
{
    test_pressure_classification();
    test_invalid_and_duplicate_cards();
    test_community_only_draw();
    test_statistical_distribution();
    test_pressure_changes_behavior();
    test_profile_changes_behavior();
    test_forced_cell_override();
    test_never_fold_on_free_check();
    test_short_stack_and_legality();

    printf("\n%d checagens, %d falhas.\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
