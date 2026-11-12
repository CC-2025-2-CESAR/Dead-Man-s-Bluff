/* Testes de regressao dos modificadores de embriaguez V1. */
#include "poker_ai_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static PokerTable setup(void)
{
    PokerTable t = {0};
    t.actor = 1;
    t.dealer = 0;
    t.street = POKER_PREFLOP;
    t.phase = POKER_ACTING;
    t.big_blind = 10;
    t.min_raise = 10;
    t.player[0].committed = 120;
    t.player[1].committed = 100;
    t.player[0].street_bet = 20;
    t.player[1].street_bet = 0;
    t.player[0].chips = 500;
    t.player[1].chips = 500;
    t.current_bet = 20;
    t.initial_total = 1220;
    return t;
}

static void check_style(PokerAIStyle style)
{
    PokerTable t = setup();
    PokerCard hole[2] = {{7, POKER_CLUBS}, {2, POKER_DIAMONDS}};
    PokerAIRng rng1, rng2;
    poker_ai_rng_seed(&rng1, 101);
    poker_ai_rng_seed(&rng2, 202);
    PokerAIContext ctx = {style, NULL, &rng1, &rng2};
    PokerAIDecisionInfo info;
    PokerAction action;
    for (int i = 0; i < 300; ++i) {
        assert(poker_ai_choose_action_mode(&t, 1, hole, NULL, 0, &ctx,
                                           true, &info, &action) == POKER_OK);
        PokerTable copy = t;
        assert(poker_act(&copy, 1, action) == POKER_OK);
        if (style == POKER_AI_CAUTIOUS || style == POKER_AI_BALANCED)
            assert(info.effective_weight[POKER_AI_ACTION_FOLD] <=
                   info.configured_weight[POKER_AI_ACTION_FOLD]);
    }
}

static void check_sober_compatibility(void)
{
    PokerTable t = setup();
    PokerCard hole[2] = {{7, POKER_CLUBS}, {2, POKER_DIAMONDS}};
    PokerAIRng a, b, e;
    poker_ai_rng_seed(&a, 12);
    poker_ai_rng_seed(&b, 12);
    poker_ai_rng_seed(&e, 34);
    PokerAIContext ca = {POKER_AI_CAUTIOUS, NULL, &a, &e};
    PokerAIContext cb = {POKER_AI_CAUTIOUS, NULL, &b, &e};
    PokerAction old_action, mode_action;
    assert(poker_ai_choose_action(&t, 1, hole, NULL, 0, &ca, NULL,
                                  &old_action) == POKER_OK);
    assert(poker_ai_choose_action_mode(&t, 1, hole, NULL, 0, &cb, false,
                                       NULL, &mode_action) == POKER_OK);
    assert(old_action.type == mode_action.type);
    assert(old_action.total == mode_action.total);
    assert(a.state == b.state);
}

int main(void)
{
    check_sober_compatibility();
    check_style(POKER_AI_CAUTIOUS);
    check_style(POKER_AI_BALANCED);
    check_style(POKER_AI_AGGRESSIVE);
    assert(fabs(poker_ai_detection_chance(POKER_AI_BALANCED, true, .60) -
                .33) < 1e-12);
    assert(fabs(poker_ai_detection_chance(POKER_AI_BALANCED, false, .60) -
                .60) < 1e-12);
    assert(isnan(poker_ai_detection_chance(POKER_AI_BALANCED, true, -1.0)));
    puts("embriaguez v1: testes aprovados");
    return 0;
}
