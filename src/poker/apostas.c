#include "poker.h"

#include <string.h>

static bool valid_seat(int seat)
{
    return seat == 0 || seat == 1;
}

static PokerChips minimum(PokerChips a, PokerChips b)
{
    return a < b ? a : b;
}

PokerChips poker_pot(const PokerTable *table)
{
    if (!table)
        return 0;

    return table->player[0].committed +
           table->player[1].committed;
}

static void commit(
    PokerTable *table,
    int seat,
    PokerChips amount
)
{
    table->player[seat].chips -= amount;
    table->player[seat].street_bet += amount;
    table->player[seat].committed += amount;
}

/* Verifica se ainda existe alguma decisão de apostas. */
static void refresh(PokerTable *table, int preferred_actor)
{
    table->actor = POKER_NO_PLAYER;

    if (table->player[0].folded || table->player[1].folded) {
        table->phase = POKER_UNCONTESTED;
        return;
    }

    bool all_in_0 = table->player[0].chips == 0;
    bool all_in_1 = table->player[1].chips == 0;

    if (all_in_0 || all_in_1) {
        int live = all_in_0 ? 1 : 0;

        if (!(all_in_0 && all_in_1) &&
            table->player[live].street_bet < table->current_bet) {
            table->phase = POKER_ACTING;
            table->actor = live;
        } else {
            table->phase = POKER_SHOWDOWN;
        }

        return;
    }

    bool both_acted =
        table->player[0].acted &&
        table->player[1].acted;

    bool both_matched =
        table->player[0].street_bet == table->current_bet &&
        table->player[1].street_bet == table->current_bet;

    if (both_acted && both_matched) {
        table->phase =
            table->street == POKER_RIVER
            ? POKER_SHOWDOWN
            : POKER_STREET_FINISHED;

        return;
    }

    table->phase = POKER_ACTING;
    table->actor = preferred_actor;
}

PokerStatus poker_start_hand(
    PokerTable *table,
    const PokerChips stacks[2],
    int dealer,
    PokerChips small_blind,
    PokerChips big_blind
)
{
    if (!table || !stacks || !valid_seat(dealer))
        return POKER_ERR_ARGUMENT;

    if (stacks[0] <= 0 ||
        stacks[1] <= 0 ||
        stacks[0] > POKER_MAX_STACK ||
        stacks[1] > POKER_MAX_STACK ||
        small_blind <= 0 ||
        big_blind < small_blind ||
        big_blind > POKER_MAX_STACK) {
        return POKER_ERR_CHIPS;
    }

    PokerTable next = {0};

    next.dealer = dealer;
    next.street = POKER_PREFLOP;
    next.big_blind = big_blind;
    next.min_raise = big_blind;
    next.initial_total = stacks[0] + stacks[1];

    next.player[0].chips = stacks[0];
    next.player[1].chips = stacks[1];

    commit(
        &next,
        dealer,
        minimum(small_blind, stacks[dealer])
    );

    commit(
        &next,
        1 - dealer,
        minimum(big_blind, stacks[1 - dealer])
    );

    next.current_bet =
        next.player[0].street_bet > next.player[1].street_bet
        ? next.player[0].street_bet
        : next.player[1].street_bet;

    /* No heads-up, o dealer/SB começa o pré-flop. */
    refresh(&next, dealer);

    *table = next;
    return POKER_OK;
}

PokerStatus poker_legal_actions(
    const PokerTable *table,
    int seat,
    PokerLegalActions *out
)
{
    if (!table || !out || !valid_seat(seat))
        return POKER_ERR_ARGUMENT;

    if (table->phase != POKER_ACTING)
        return POKER_ERR_STATE;

    if (table->actor != seat)
        return POKER_ERR_TURN;

    const PokerPlayer *player = &table->player[seat];

    bool opponent_can_bet =
        table->player[1 - seat].chips > 0;

    PokerLegalActions legal = {0};

    legal.fold = true;

    legal.to_call =
        table->current_bet - player->street_bet;

    legal.call_cost =
        minimum(legal.to_call, player->chips);

    legal.max_total =
        player->street_bet + player->chips;

    legal.min_total =
        table->current_bet == 0
        ? table->big_blind
        : table->current_bet + table->min_raise;

    legal.check = legal.to_call == 0;
    legal.call = legal.to_call > 0;

    legal.bet =
        table->current_bet == 0 &&
        opponent_can_bet &&
        legal.max_total >= legal.min_total;

    legal.raise =
        table->current_bet > 0 &&
        opponent_can_bet &&
        legal.max_total >= legal.min_total;

    legal.all_in =
        player->chips > 0 &&
        (player->chips <= legal.to_call || opponent_can_bet);

    *out = legal;
    return POKER_OK;
}

PokerStatus poker_act(
    PokerTable *table,
    int seat,
    PokerAction action
)
{
    PokerLegalActions legal;

    PokerStatus status =
        poker_legal_actions(table, seat, &legal);

    if (status != POKER_OK)
        return status;

    if (action.type != POKER_BET &&
        action.type != POKER_RAISE &&
        action.total != 0) {
        return POKER_ERR_ACTION;
    }

    PokerChips target = table->player[seat].street_bet;
    bool fold = false;

    switch (action.type) {
        case POKER_FOLD:
            if (!legal.fold)
                return POKER_ERR_ACTION;

            fold = true;
            break;

        case POKER_CHECK:
            if (!legal.check)
                return POKER_ERR_ACTION;

            break;

        case POKER_CALL:
            if (!legal.call)
                return POKER_ERR_ACTION;

            target += legal.call_cost;
            break;

        case POKER_BET:
            if (!legal.bet ||
                action.total < legal.min_total ||
                action.total > legal.max_total) {
                return POKER_ERR_ACTION;
            }

            target = action.total;
            break;

        case POKER_RAISE:
            if (!legal.raise ||
                action.total < legal.min_total ||
                action.total > legal.max_total) {
                return POKER_ERR_ACTION;
            }

            target = action.total;
            break;

        case POKER_ALL_IN:
            if (!legal.all_in)
                return POKER_ERR_ACTION;

            target = legal.max_total;
            break;

        default:
            return POKER_ERR_ACTION;
    }

    /*
     * Só altera a mesa depois que toda a ação foi validada.
     * Portanto, uma ação ilegal não desconta fichas.
     */
    PokerTable next = *table;

    next.player[seat].folded = fold;
    next.player[seat].acted = true;

    commit(
        &next,
        seat,
        target - next.player[seat].street_bet
    );

    if (target > next.current_bet) {
        PokerChips increment = target - next.current_bet;

        /* All-in curto não redefine o aumento mínimo. */
        if (increment >= next.min_raise)
            next.min_raise = increment;

        next.current_bet = target;
        next.player[1 - seat].acted = false;
    }

    refresh(&next, 1 - seat);

    *table = next;
    return POKER_OK;
}

PokerStatus poker_next_street(PokerTable *table)
{
    if (!table)
        return POKER_ERR_ARGUMENT;

    if (table->phase != POKER_STREET_FINISHED ||
        table->street >= POKER_RIVER) {
        return POKER_ERR_STATE;
    }

    PokerTable next = *table;

    next.street = (PokerStreet)(next.street + 1);
    next.current_bet = 0;
    next.min_raise = next.big_blind;

    for (int i = 0; i < 2; ++i) {
        next.player[i].street_bet = 0;
        next.player[i].acted = false;
    }

    /* Depois do pré-flop, quem começa é o BB. */
    refresh(&next, 1 - next.dealer);

    *table = next;
    return POKER_OK;
}

PokerStatus poker_divide_pot(
    PokerChips pot,
    unsigned winners,
    int dealer,
    PokerChips awards[2]
)
{
    if (!awards ||
        !valid_seat(dealer) ||
        winners < 1 ||
        winners > 3) {
        return POKER_ERR_ARGUMENT;
    }

    if (pot < 0)
        return POKER_ERR_CHIPS;

    PokerChips result[2] = {0, 0};

    if (winners == 3) {
        result[0] = pot / 2;
        result[1] = pot / 2;

        /* Ficha ímpar para o vencedor à esquerda do button. */
        result[1 - dealer] += pot % 2;
    } else {
        result[winners == 1 ? 0 : 1] = pot;
    }

    memcpy(awards, result, sizeof result);
    return POKER_OK;
}

static PokerStatus validate_showdown(
    const PokerCard hole0[2],
    const PokerCard hole1[2],
    const PokerCard board[5]
)
{
    if (!hole0 || !hole1 || !board)
        return POKER_ERR_ARGUMENT;

    uint64_t seen = 0;

    /* Valida as nove cartas físicas, inclusive entre jogadores. */
    for (int i = 0; i < 9; ++i) {
        PokerCard card =
            i < 4
            ? (i < 2 ? hole0[i] : hole1[i - 2])
            : board[i - 4];

        if (card.rank < 2 ||
            card.rank > 14 ||
            (unsigned)card.suit > POKER_SPADES) {
            return POKER_ERR_CARD;
        }

        unsigned id =
            (unsigned)card.suit * 13u +
            (unsigned)(card.rank - 2);

        uint64_t bit = UINT64_C(1) << id;

        if (seen & bit)
            return POKER_ERR_DUPLICATE_CARD;

        seen |= bit;
    }

    return POKER_OK;
}

PokerStatus poker_settle(
    PokerTable *table,
    const PokerCard hole0[2],
    const PokerCard hole1[2],
    const PokerCard board[5],
    PokerResult *out
)
{
    if (!table || !out)
        return POKER_ERR_ARGUMENT;

    if (table->phase != POKER_SHOWDOWN &&
        table->phase != POKER_UNCONTESTED) {
        return POKER_ERR_STATE;
    }

    PokerResult result = {0};

    result.pot = poker_pot(table);

    PokerChips matched = minimum(
        table->player[0].committed,
        table->player[1].committed
    );

    result.contested = 2 * matched;

    for (int i = 0; i < 2; ++i) {
        result.refund[i] =
            table->player[i].committed - matched;
    }

    if (table->phase == POKER_UNCONTESTED) {
        result.winners =
            table->player[0].folded ? 2u : 1u;
    } else {
        PokerStatus status =
            validate_showdown(hole0, hole1, board);

        if (status != POKER_OK)
            return status;

        for (int i = 0; i < 2; ++i) {
            status = poker_evaluate_holdem(
                i == 0 ? hole0 : hole1,
                board,
                5,
                &result.value[i]
            );

            if (status != POKER_OK)
                return status;
        }

        int comparison = poker_compare(
            &result.value[0],
            &result.value[1]
        );

        result.winners =
            comparison > 0 ? 1u :
            comparison < 0 ? 2u : 3u;

        result.showdown = true;
    }

    PokerStatus status = poker_divide_pot(
        result.contested,
        result.winners,
        table->dealer,
        result.award
    );

    if (status != POKER_OK)
        return status;

    PokerTable next = *table;

    for (int i = 0; i < 2; ++i) {
        next.player[i].chips +=
            result.refund[i] + result.award[i];

        next.player[i].committed = 0;
        next.player[i].street_bet = 0;
    }

    next.current_bet = 0;
    next.actor = POKER_NO_PLAYER;
    next.phase = POKER_SETTLED;

    *table = next;
    *out = result;

    return POKER_OK;
}
