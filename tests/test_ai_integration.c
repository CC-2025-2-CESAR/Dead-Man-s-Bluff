/* Regressao de integracao: short stack, preco real, FREE, RNG e erros. */
#include "poker_ai_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static PokerCard card(int rank, PokerSuit suit)
{
    PokerCard c = {rank, suit};
    return c;
}

static PokerTable face_bet(PokerChips baseline, PokerChips bet,
                           PokerChips actor_stack, PokerStreet street)
{
    PokerTable t = {0};
    t.actor = 1;
    t.dealer = 0;
    t.street = street;
    t.phase = POKER_ACTING;
    t.big_blind = 10;
    t.min_raise = 10;
    t.player[0].committed = baseline + bet;
    t.player[1].committed = baseline;
    t.player[0].street_bet = bet;
    t.player[1].street_bet = 0;
    t.player[0].chips = 1000;
    t.player[1].chips = actor_stack;
    t.current_bet = bet;
    t.initial_total = t.player[0].committed + t.player[1].committed +
                      t.player[0].chips + t.player[1].chips;
    return t;
}

static void check_capped_cheap_call(void)
{
    PokerTable t = face_bet(100, 100, 5, POKER_PREFLOP);
    PokerAIRng action, equity;
    poker_ai_rng_seed(&action, 1234);
    poker_ai_rng_seed(&equity, 5678);
    PokerAIContext ctx = {POKER_AI_CAUTIOUS, NULL, &action, &equity};
    PokerCard hole[2] = {card(7, POKER_CLUBS), card(2, POKER_DIAMONDS)};
    PokerAIDecisionInfo info;
    PokerAction act;
    double odds;
    assert(poker_ai_pot_odds(&t, 1, &odds) == POKER_OK);
    assert(fabs(odds - 5.0/210.0) < 1e-12);
    for (int i = 0; i < 1000; ++i) {
        assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                     &ctx, &info, &act) == POKER_OK);
        assert(info.capped_all_in_call);
        assert(info.cheap_all_in_override);
        assert(info.pressure == POKER_AI_PRESSURE_SMALL);
        assert(fabs(info.nominal_pressure_ratio - .5) < 1e-12);
        assert(fabs(info.pressure_ratio - .025) < 1e-12);
        assert(fabs(info.effective_pot_odds - odds) < 1e-12);
        assert(act.type == POKER_CALL);
        PokerTable copy = t;
        assert(poker_act(&copy, 1, act) == POKER_OK);
    }
    /* Ainda e capado, mas pagar 30 de 260 disputaveis custa 11.5%. */
    t.player[1].chips = 30;
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                 &ctx, &info, &act) == POKER_OK);
    assert(!info.cheap_all_in_override);
}

static void check_uncapped_and_free(void)
{
    PokerTable t = face_bet(100, 100, 1000, POKER_PREFLOP);
    PokerAIRng action, equity;
    poker_ai_rng_seed(&action, 5);
    poker_ai_rng_seed(&equity, 7);
    PokerAIContext ctx = {POKER_AI_CAUTIOUS, NULL, &action, &equity};
    PokerCard hole[2] = {card(7, POKER_CLUBS), card(2, POKER_DIAMONDS)};
    PokerAIDecisionInfo info;
    PokerAction act;
    double odds;
    assert(poker_ai_pot_odds(&t, 1, &odds) == POKER_OK);
    assert(fabs(odds - .25) < 1e-12);
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                 &ctx, &info, &act) == POKER_OK);
    assert(!info.capped_all_in_call);
    assert(!info.cheap_all_in_override);
    assert(info.pressure == POKER_AI_PRESSURE_MEDIUM);

    t.current_bet = 0;
    t.player[0].street_bet = 0;
    t.player[0].committed = 100;
    PokerAIScenarioTable custom;
    memcpy(custom, poker_ai_scenario_table, sizeof custom);
    PokerAIScenario *free_cell = &custom[0][POKER_PREFLOP][POKER_AI_HAND_WEAK][POKER_AI_PRESSURE_FREE];
    ctx.table = (const PokerAIScenarioTable *)&custom;
    for (int i = 0; i < 1000; ++i) {
        assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                     &ctx, &info, &act) == POKER_OK);
        assert(info.configured_weight[0] == 0.0);
        assert(info.effective_weight[0] == 0.0);
        assert(info.effective_weight[1] == free_cell->weight[1]);
        assert(info.effective_weight[2] == free_cell->weight[2]);
        assert(info.effective_weight[3] == free_cell->weight[3]);
        assert(act.type != POKER_FOLD);
    }
    free_cell->weight[POKER_AI_ACTION_FOLD] = 1;
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                 &ctx, &info, &act) == POKER_ERR_ARGUMENT);
}

static void check_cards_and_rng(void)
{
    PokerAIRng action, equity;
    poker_ai_rng_seed(&action, 11);
    poker_ai_rng_seed(&equity, 22);
    PokerCard hole[2] = {card(7, POKER_CLUBS), card(2, POKER_DIAMONDS)};
    PokerCard board[3] = {card(3, POKER_SPADES), card(6, POKER_HEARTS), card(15, POKER_CLUBS)};
    PokerAIDrawInfo draw;
    double value;
    assert(poker_ai_evaluate_draws(hole, board, 3, &draw) == POKER_ERR_CARD);
    assert(poker_ai_estimate_equity(hole, board, 3, &equity, 10, &value) == POKER_ERR_CARD);
    board[2] = hole[0];
    assert(poker_ai_evaluate_draws(hole, board, 3, &draw) == POKER_ERR_DUPLICATE_CARD);
    assert(poker_ai_estimate_equity(hole, board, 3, &equity, 10, &value) == POKER_ERR_DUPLICATE_CARD);
    board[2] = card(9, POKER_CLUBS);
    PokerTable t = face_bet(100, 100, 200, POKER_FLOP);
    PokerAIContext ctx = {POKER_AI_BALANCED, NULL, &action, &equity};
    PokerAction act;
    PokerAIDecisionInfo info;
    action.state = 0;
    assert(poker_ai_choose_action(&t, 1, hole, board, 3, &ctx, &info, &act) == POKER_ERR_ARGUMENT);
    poker_ai_rng_seed(&action, 11);
    equity.state = 0;
    assert(poker_ai_choose_action(&t, 1, hole, board, 3, &ctx, &info, &act) == POKER_ERR_ARGUMENT);
    poker_ai_rng_seed(&equity, 22);
    ctx.equity_rng = ctx.action_rng;
    assert(poker_ai_choose_action(&t, 1, hole, board, 3, &ctx, &info, &act) == POKER_ERR_ARGUMENT);
    ctx.equity_rng = &equity;
    assert(poker_ai_choose_action(&t, 1, hole, board, 3, &ctx, &info, &act) == POKER_OK);
    PokerTable copy = t;
    assert(poker_act(&copy, 1, act) == POKER_OK);
    /* O RNG de acao nao deve consumir os 400 sorteios do Monte Carlo. */
    PokerAIRng expected;
    poker_ai_rng_seed(&expected, 11);
    (void)poker_ai_rng_uniform(&expected);
    if (act.type != POKER_BET && act.type != POKER_RAISE)
        assert(action.state == expected.state);
}


/* Forca BIG em cada cenario para testar all-in sem alterar a tabela base. */
static void check_offensive_all_in(void)
{
    PokerAIScenarioTable forced;
    memcpy(forced, poker_ai_scenario_table, sizeof forced);
    for (int p = 0; p < POKER_AI_PROFILE_COUNT; ++p)
        for (int s = 0; s < POKER_AI_STREET_COUNT; ++s)
            for (int h = 0; h < POKER_AI_STRENGTH_COUNT; ++h)
                for (int q = 0; q < POKER_AI_PRESSURE_COUNT; ++q) {
                    PokerAIScenario *cell = &forced[p][s][h][q];
                    cell->weight[0] = 0.0;
                    cell->weight[1] = 0.0;
                    cell->weight[2] = 0.0;
                    cell->weight[3] = 100.0;
                }

    PokerAIRng action_rng, equity_rng;
    poker_ai_rng_seed(&action_rng, 301);
    poker_ai_rng_seed(&equity_rng, 302);
    PokerAIContext ctx = {POKER_AI_AGGRESSIVE,
                          (const PokerAIScenarioTable *)&forced,
                          &action_rng, &equity_rng};
    PokerCard hole[2] = {card(7, POKER_CLUBS), card(2, POKER_DIAMONDS)};
    PokerAction act;
    PokerAIDecisionInfo info;
    PokerLegalActions legal;

    /* Stack 55; call 50; raise minimo 60: all-in curto. */
    PokerTable t = face_bet(100, 50, 55, POKER_PREFLOP);
    assert(poker_legal_actions(&t, 1, &legal) == POKER_OK);
    assert(!legal.raise && legal.all_in && legal.max_total > t.current_bet);
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                  &ctx, &info, &act) == POKER_OK);
    assert(info.chosen_slot == POKER_AI_ACTION_RAISE_BIG);
    assert(act.type == POKER_ALL_IN && act.total == 0);
    assert(poker_act(&t, 1, act) == POKER_OK);
    assert(t.player[1].chips == 0 && t.current_bet == 55);

    /* Raise legal, mas SPR baixo: BIG vira all-in deliberado. */
    t = face_bet(100, 50, 80, POKER_PREFLOP);
    assert(poker_legal_actions(&t, 1, &legal) == POKER_OK && legal.raise);
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                  &ctx, &info, &act) == POKER_OK);
    assert(act.type == POKER_ALL_IN && act.total == 0);
    assert(poker_act(&t, 1, act) == POKER_OK);
    assert(t.player[1].chips == 0 && t.current_bet == 80);

    /* Stack profundo: BIG continua sendo RAISE convencional. */
    t = face_bet(100, 50, 1000, POKER_PREFLOP);
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                  &ctx, &info, &act) == POKER_OK);
    assert(act.type == POKER_RAISE);
    assert(poker_act(&t, 1, act) == POKER_OK);

    /* Stack insuficiente para call: continua CALL, nunca shove ofensivo. */
    t = face_bet(100, 100, 5, POKER_PREFLOP);
    assert(poker_legal_actions(&t, 1, &legal) == POKER_OK);
    assert(legal.all_in && legal.max_total <= t.current_bet);
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0,
                                  &ctx, &info, &act) == POKER_OK);
    assert(act.type == POKER_CALL);
    assert(poker_act(&t, 1, act) == POKER_OK);

    /* Abrir aposta com menos do que o minimo: so ALL_IN. */
    t = face_bet(100, 0, 5, POKER_FLOP);
    PokerCard board[3] = {card(3, POKER_HEARTS),
                          card(9, POKER_SPADES),
                          card(13, POKER_DIAMONDS)};
    assert(poker_legal_actions(&t, 1, &legal) == POKER_OK);
    assert(!legal.bet && legal.all_in);
    assert(poker_ai_choose_action(&t, 1, hole, board, 3,
                                  &ctx, &info, &act) == POKER_OK);
    assert(act.type == POKER_ALL_IN && act.total == 0);
    assert(poker_act(&t, 1, act) == POKER_OK);
}

int main(void)
{
    check_offensive_all_in();
    check_capped_cheap_call();
    check_uncapped_and_free();
    check_cards_and_rng();
    puts("Regressoes: all-in ofensivo, short stack, pot odds, FREE, RNG e cartas OK.");
    return 0;
}
