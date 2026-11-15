#include "apostas.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static const PokerCard pairs[2][2] = {
    {{14, POKER_SPADES}, {14, POKER_HEARTS}},
    {{13, POKER_SPADES}, {13, POKER_HEARTS}}
};
static const PokerCard board[5] = {
    {2, POKER_CLUBS}, {7, POKER_DIAMONDS}, {9, POKER_HEARTS},
    {11, POKER_CLUBS}, {3, POKER_SPADES}
};

static void check_conservation(const PokerTable *table)
{
    CHECK(table->player[0].chips + table->player[1].chips +
          poker_pot(table) == table->initial_total);
}

static void act(PokerTable *table, PokerActionType type, PokerChips total)
{
    CHECK(poker_act(table, table->actor, (PokerAction){type, total}) == POKER_OK);
    check_conservation(table);
}

static void check_to_showdown(PokerTable *table)
{
    while (table->phase != POKER_SHOWDOWN) {
        if (table->phase == POKER_STREET_FINISHED)
            CHECK(poker_next_street(table) == POKER_OK);
        CHECK(table->phase == POKER_ACTING);
        act(table, POKER_CHECK, 0);
    }
}

static void check_settled(const PokerTable *table)
{
    CHECK(table->phase == POKER_SETTLED);
    CHECK(table->actor == POKER_NO_PLAYER);
    CHECK(poker_pot(table) == 0);
    check_conservation(table);
}

static void test_higher_pair(void)
{
    PokerTable table;
    PokerResult result;
    const PokerChips stacks[2] = {2000, 2000};
    CHECK(poker_start_hand(&table, stacks, 0, 50, 100) == POKER_OK);
    act(&table, POKER_RAISE, 300);
    act(&table, POKER_CALL, 0);
    check_to_showdown(&table);
    CHECK(poker_settle(&table, pairs[0], pairs[1], board, &result) == POKER_OK);
    CHECK(result.showdown && result.winners == 1);
    CHECK(result.value[0].category == POKER_ONE_PAIR);
    CHECK(result.value[1].category == POKER_ONE_PAIR);
    CHECK(result.pot == 600 && result.contested == 600);
    CHECK(result.refund[0] == 0 && result.refund[1] == 0);
    CHECK(result.award[0] == 600 && result.award[1] == 0);
    CHECK(table.player[0].chips == 2300 && table.player[1].chips == 1700);
    check_settled(&table);
    CHECK(poker_settle(&table, pairs[0], pairs[1], board, &result) == POKER_ERR_STATE);
    check_settled(&table);
}

static void test_all_in(int short_seat, int short_wins)
{
    PokerTable table;
    PokerResult result;
    PokerChips stacks[2] = {2000, 2000};
    stacks[short_seat] = 500;
    const PokerCard *hole0 = pairs[short_wins ? short_seat : 1 - short_seat];
    const PokerCard *hole1 = pairs[short_wins ? 1 - short_seat : short_seat];
    CHECK(poker_start_hand(&table, stacks, 1 - short_seat, 50, 100) == POKER_OK);
    act(&table, POKER_ALL_IN, 0);
    act(&table, POKER_ALL_IN, 0);
    CHECK(table.phase == POKER_SHOWDOWN);
    CHECK(poker_settle(&table, hole0, hole1, board, &result) == POKER_OK);
    CHECK(result.pot == 2500 && result.contested == 1000);
    CHECK(result.refund[short_seat] == 0);
    CHECK(result.refund[1 - short_seat] == 1500);
    int winner = short_wins ? short_seat : 1 - short_seat;
    CHECK(result.winners == (1u << winner));
    CHECK(result.award[winner] == 1000 && result.award[1 - winner] == 0);
    CHECK(table.player[short_seat].chips == (short_wins ? 1000 : 0));
    CHECK(table.player[1 - short_seat].chips == (short_wins ? 1500 : 2500));
    check_settled(&table);
}

static void test_tie(void)
{
    PokerTable table;
    PokerResult result;
    const PokerChips stacks[2] = {2000, 2000};
    /* Matrizes mutáveis verificam a interface de poker_settle em C11. */
    PokerCard hole[2][2] = {
        {{2, POKER_SPADES}, {3, POKER_HEARTS}},
        {{4, POKER_SPADES}, {5, POKER_HEARTS}}
    };
    PokerCard shared[5] = {
        {10, POKER_CLUBS}, {11, POKER_CLUBS}, {12, POKER_CLUBS},
        {13, POKER_CLUBS}, {14, POKER_CLUBS}
    };
    CHECK(poker_start_hand(&table, stacks, 0, 50, 100) == POKER_OK);
    act(&table, POKER_CALL, 0);
    act(&table, POKER_CHECK, 0);
    check_to_showdown(&table);
    CHECK(poker_settle(&table, hole[0], hole[1], shared, &result) == POKER_OK);
    CHECK(result.showdown && result.winners == 3);
    CHECK(result.pot == 200 && result.contested == 200);
    CHECK(result.award[0] == 100 && result.award[1] == 100);
    CHECK(result.refund[0] == 0 && result.refund[1] == 0);
    CHECK(table.player[0].chips == 2000 && table.player[1].chips == 2000);
    check_settled(&table);
    for (int dealer = 0; dealer < 2; ++dealer) {
        PokerChips awards[2];
        CHECK(poker_divide_pot(201, 3, dealer, awards) == POKER_OK);
        CHECK(awards[dealer] == 100 && awards[1 - dealer] == 101);
    }
}

static void test_fold(void)
{
    PokerTable table;
    PokerResult result;
    const PokerChips stacks[2] = {2000, 2000};
    CHECK(poker_start_hand(&table, stacks, 0, 50, 100) == POKER_OK);
    act(&table, POKER_FOLD, 0);
    CHECK(poker_settle(&table, NULL, NULL, NULL, &result) == POKER_OK);
    CHECK(!result.showdown && result.winners == 2);
    CHECK(result.refund[1] == 50 && result.award[1] == 100);
    CHECK(table.player[0].chips == 1950 && table.player[1].chips == 2050);
    check_settled(&table);
}

int main(void)
{
    test_higher_pair();
    for (int seat = 0; seat < 2; ++seat)
        for (int wins = 0; wins < 2; ++wins)
            test_all_in(seat, wins);
    test_tie();
    test_fold();
    puts("OK: par superior, all-in/devolucao, empate, fold e pagamento unico.");
    return EXIT_SUCCESS;
}
