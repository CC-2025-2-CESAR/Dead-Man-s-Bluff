#include "../src/cards/deck.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int id(PokerCard c)
{
    assert(c.rank >= 2 && c.rank <= 14);
    assert((unsigned)c.suit <= POKER_SPADES);
    return (int)c.suit * 13 + c.rank - 2;
}

static void test_partial_shuffle(void)
{
    Deck deck = {0};
    PokerCard card;
    int seen[52] = {0};
    assert(deck_create(&deck));
    for (int i = 0; i < 11; ++i) {
        assert(deck_draw(&deck, &card));
        assert(++seen[id(card)] == 1);
    }
    assert(deck.count == 41);
    deck_shuffle(&deck);
    for (int i = 0; i < 41; ++i) {
        assert(deck_draw(&deck, &card));
        assert(++seen[id(card)] == 1);
    }
    assert(deck.count == 0 && deck.head == NULL && deck.tail == NULL);
    for (int i = 0; i < 52; ++i)
        assert(seen[i] == 1);
    deck_free(&deck);
}

static void test_invalid_input_does_not_consume(void)
{
    Deck deck = {0};
    PokerCard hole[2][2];
    PokerCard board[5] = {{0}};
    PokerCard drawn;
    size_t board_count = 0;
    assert(!deck_create(NULL));
    assert(!deck_draw(NULL, &drawn));
    assert(!deck_draw(&deck, &drawn));
    assert(!deck_burn(&deck));
    assert(deck_create(&deck));
    assert(!deck_draw(&deck, NULL));
    assert(!deck_deal(&deck, 0, NULL));
    assert(!deck_deal(&deck, 2, hole));
    assert(!deck_deal_board(&deck, POKER_PREFLOP, board, &board_count));
    assert(!deck_deal_board(&deck, POKER_RIVER, board, &board_count));
    assert(!deck_deal_board(&deck, POKER_FLOP, NULL, &board_count));
    assert(!deck_deal_board(&deck, POKER_FLOP, board, NULL));
    assert(!deck_deal_board(NULL, POKER_FLOP, board, &board_count));
    assert(deck.count == 52 && board_count == 0);
    assert(deck_deal(&deck, 0, hole));
    assert(deck.count == 48);
    deck_free(&deck);
    deck_free(&deck);
}

static void test_fold_without_board(void)
{
    PokerTable table;
    PokerResult result = {0};
    const PokerChips stacks[2] = {500, 500};
    assert(poker_start_hand(&table, stacks, 0, 5, 10) == POKER_OK);
    PokerAction fold = {POKER_FOLD, 0};
    assert(poker_act(&table, table.actor, fold) == POKER_OK);
    assert(table.phase == POKER_UNCONTESTED);
    assert(poker_settle(&table, NULL, NULL, NULL, &result) == POKER_OK);
    assert(result.winners == 2);
    assert(!result.showdown);
    assert(table.player[0].chips + table.player[1].chips == 1000);
}

static void test_all_in_runout(void)
{
    Deck deck = {0};
    PokerTable table;
    PokerResult result = {0};
    PokerCard hole[2][2];
    PokerCard board[5];
    size_t board_count = 0;
    const PokerChips stacks[2] = {10, 10};
    assert(deck_create(&deck));
    deck_shuffle(&deck);
    assert(poker_start_hand(&table, stacks, 0, 5, 10) == POKER_OK);
    assert(deck_deal(&deck, table.dealer, hole));
    assert(table.phase == POKER_ACTING && table.actor == 0);
    PokerAction call = {POKER_CALL, 0};
    assert(poker_act(&table, 0, call) == POKER_OK);
    assert(table.phase == POKER_SHOWDOWN);
    assert(table.street == POKER_PREFLOP);
    /* O controlador precisa abrir o board mesmo sem poker_next_street. */
    assert(deck_deal_board(&deck, POKER_FLOP, board, &board_count));
    assert(deck_deal_board(&deck, POKER_TURN, board, &board_count));
    assert(deck_deal_board(&deck, POKER_RIVER, board, &board_count));
    assert(board_count == 5 && deck.count == 40);
    assert(poker_settle(&table, hole[0], hole[1], board, &result) == POKER_OK);
    assert(result.pot == 20 && result.showdown);
    assert(table.player[0].chips + table.player[1].chips == 20);
    deck_free(&deck);
}

int main(void)
{
    srand(72026);
    test_invalid_input_does_not_consume();
    for (int i = 0; i < 1000; i++) test_partial_shuffle();
    test_fold_without_board();
    test_all_in_runout();
    puts("OK: entradas invalidas, 1.000 embaralhamentos parciais, fold, all-in preflop e runout.");
    return 0;
}
