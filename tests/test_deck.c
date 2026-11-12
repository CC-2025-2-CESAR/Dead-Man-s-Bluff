#include "../src/cards/deck.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int card_id(PokerCard card)
{
    assert(card.rank >= 2 && card.rank <= 14);
    assert(card.suit >= POKER_CLUBS && card.suit <= POKER_SPADES);
    return (int)card.suit * 13 + card.rank - 2;
}

static void check_no_duplicates(PokerCard cards[], int n)
{
    int seen[52] = {0};
    for (int i = 0; i < n; i++)
        assert(++seen[card_id(cards[i])] == 1);
}

static void test_all_52(void)
{
    Deck deck = {0};
    PokerCard cards[52];
    assert(deck_create(&deck));
    assert(deck.count == 52);
    assert(deck.head != NULL && deck.tail != NULL);
    assert(!deck_create(&deck));
    deck_shuffle(&deck);
    for (int i = 0; i < 52; i++) {
        assert(deck_draw(&deck, &cards[i]));
        assert(deck.count == (size_t)(51 - i));
    }
    check_no_duplicates(cards, 52);
    assert(deck.head == NULL && deck.tail == NULL && deck.count == 0);
    assert(!deck_draw(&deck, &cards[0]));
    deck_free(&deck);
    assert(deck_create(&deck));
    deck_free(&deck);
}

static void test_dealer_order(int dealer)
{
    Deck deck = {0};
    PokerCard hole[2][2];
    PokerCard first[4];

    assert(deck_create(&deck));
    for (int i = 0; i < 4; i++)
        assert(deck_draw(&deck, &first[i]));
    deck_free(&deck);
    assert(deck_create(&deck));
    assert(deck_deal(&deck, dealer, hole));
    assert(deck.count == 48);
    int other = 1 - dealer;
    assert(card_id(hole[dealer][0]) == card_id(first[0]));
    assert(card_id(hole[other][0]) == card_id(first[1]));
    assert(card_id(hole[dealer][1]) == card_id(first[2]));
    assert(card_id(hole[other][1]) == card_id(first[3]));
    assert(!deck_deal(&deck, 2, hole));
    assert(deck.count == 48);
    deck_free(&deck);
}

static void test_board_streets(void)
{
    Deck deck = {0};
    PokerCard hole[2][2];
    PokerCard board[5];
    PokerCard all[9];
    size_t count = 0;
    assert(deck_create(&deck));
    deck_shuffle(&deck);
    assert(deck_deal(&deck, 0, hole));
    assert(!deck_deal_board(&deck, POKER_TURN, board, &count));
    assert(count == 0 && deck.count == 48);
    assert(deck_deal_board(&deck, POKER_FLOP, board, &count));
    assert(count == 3 && deck.count == 44);
    assert(!deck_deal_board(&deck, POKER_FLOP, board, &count));
    assert(count == 3 && deck.count == 44);
    assert(deck_deal_board(&deck, POKER_TURN, board, &count));
    assert(count == 4 && deck.count == 42);
    assert(deck_deal_board(&deck, POKER_RIVER, board, &count));
    assert(count == 5 && deck.count == 40);
    assert(!deck_deal_board(&deck, POKER_RIVER, board, &count));
    assert(deck.count == 40);
    int n = 0;
    for (int p = 0; p < 2; p++)
        for (int j = 0; j < 2; j++)
            all[n++] = hole[p][j];
    for (int i = 0; i < 5; i++)
        all[n++] = board[i];
    check_no_duplicates(all, 9);
    deck_free(&deck);
}

static void test_insufficient_cards(void)
{
    Deck deck = {0};
    PokerCard card;
    PokerCard hole[2][2];
    PokerCard board[5];
    size_t count = 0;
    assert(deck_create(&deck));
    for (int i = 0; i < 49; i++)
        assert(deck_draw(&deck, &card));
    assert(deck.count == 3);
    assert(!deck_deal(&deck, 0, hole));
    assert(!deck_deal_board(&deck, POKER_FLOP, board, &count));
    assert(deck.count == 3 && count == 0);
    deck_free(&deck);
}

int main(void)
{
    srand(12345);
    test_all_52();
    test_dealer_order(0);
    test_dealer_order(1);
    test_insufficient_cards();
    for (int i = 0; i < 500; i++)
        test_board_streets();
    puts("OK: 52 cartas unicas, dealer 0/1, streets, queimadas e validacoes (500 rodadas).");
    return 0;
}
