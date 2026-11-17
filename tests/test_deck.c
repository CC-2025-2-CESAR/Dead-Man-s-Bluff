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

static void test_remove_card(void)
{
    Deck deck = {0};
    PokerCard card;
    int ids[52];
    int n = 0;

    assert(deck_create(&deck));
    assert(deck_contains(&deck, (PokerCard){2, POKER_CLUBS}));
    assert(!deck_contains(NULL, (PokerCard){2, POKER_CLUBS}));
    assert(!deck_contains(&deck, (PokerCard){1, POKER_CLUBS}));
    assert(!deck_contains(&deck, (PokerCard){2, (PokerSuit)4}));

    /* Cabeca, cauda e meio do baralho em ordem de criacao. */
    assert(deck_remove_card(&deck, (PokerCard){2, POKER_CLUBS}));
    assert(deck.count == 51);
    assert(deck_remove_card(&deck, (PokerCard){14, POKER_SPADES}));
    assert(deck.count == 50);
    assert(deck_remove_card(&deck, (PokerCard){8, POKER_HEARTS}));
    assert(deck.count == 49);
    assert(deck.tail != NULL);
    assert(!deck_contains(&deck, (PokerCard){2, POKER_CLUBS}));
    assert(!deck_contains(&deck, (PokerCard){14, POKER_SPADES}));
    assert(!deck_contains(&deck, (PokerCard){8, POKER_HEARTS}));
    assert(deck_contains(&deck, (PokerCard){3, POKER_CLUBS}));

    /* Segunda remocao, carta invalida e ponteiro nulo nao alteram nada. */
    assert(!deck_remove_card(&deck, (PokerCard){8, POKER_HEARTS}));
    assert(!deck_remove_card(&deck, (PokerCard){15, POKER_HEARTS}));
    assert(!deck_remove_card(&deck, (PokerCard){5, (PokerSuit)-1}));
    assert(!deck_remove_card(NULL, (PokerCard){5, POKER_HEARTS}));
    assert(deck.count == 49);

    /* A ordem das demais cartas continua a de criacao. */
    while (deck_draw(&deck, &card)) {
        ids[n++] = card_id(card);
        assert(n == 1 || ids[n - 1] > ids[n - 2]);
    }

    assert(n == 49);
    assert(deck.head == NULL && deck.tail == NULL && deck.count == 0);

    /* Tirar a ultima carta deixa o baralho vazio e consistente. */
    assert(deck_create(&deck));

    for (int i = 0; i < 51; i++)
        assert(deck_draw(&deck, &card));

    assert(deck.count == 1 && deck.head == deck.tail);
    assert(deck_remove_card(&deck, (PokerCard){14, POKER_SPADES}));
    assert(deck.head == NULL && deck.tail == NULL && deck.count == 0);
    deck_free(&deck);

    /* Depois de tirar a cauda, o baralho ainda aceita comprar e embaralhar. */
    assert(deck_create(&deck));
    assert(deck_remove_card(&deck, (PokerCard){14, POKER_SPADES}));
    deck_shuffle(&deck);
    assert(deck.count == 51);

    for (int i = 0; i < 51; i++)
        assert(deck_draw(&deck, &card));

    assert(!deck_draw(&deck, &card));
    deck_free(&deck);
}

static void test_exchange(void)
{
    Deck deck = {0};
    PokerCard card;
    int ids[52];
    int n = 0;

    assert(deck_create(&deck));
    assert(deck_remove_card(&deck, (PokerCard){9, POKER_CLUBS}));

    /* O 5 de ouros sai e o 9 de paus ocupa o lugar dele, na mesma posicao. */
    assert(deck_exchange(&deck, (PokerCard){5, POKER_DIAMONDS}, (PokerCard){9, POKER_CLUBS}));
    assert(deck.count == 51);
    assert(!deck_contains(&deck, (PokerCard){5, POKER_DIAMONDS}));
    assert(deck_contains(&deck, (PokerCard){9, POKER_CLUBS}));

    /* Recusas: carta que nao esta, carta repetida, invalidas, nulo. */
    assert(!deck_exchange(&deck, (PokerCard){5, POKER_DIAMONDS}, (PokerCard){6, POKER_DIAMONDS}));
    assert(!deck_exchange(&deck, (PokerCard){2, POKER_CLUBS}, (PokerCard){3, POKER_CLUBS}));
    assert(!deck_exchange(&deck, (PokerCard){2, POKER_CLUBS}, (PokerCard){1, POKER_CLUBS}));
    assert(!deck_exchange(&deck, (PokerCard){2, POKER_CLUBS}, (PokerCard){15, POKER_CLUBS}));
    assert(!deck_exchange(&deck, (PokerCard){0, POKER_CLUBS}, (PokerCard){5, POKER_DIAMONDS}));
    assert(!deck_exchange(&deck, (PokerCard){2, POKER_CLUBS}, (PokerCard){5, (PokerSuit)4}));
    assert(!deck_exchange(NULL, (PokerCard){2, POKER_CLUBS}, (PokerCard){5, POKER_DIAMONDS}));
    assert(deck.count == 51);

    /* Trocar uma carta por ela mesma e permitido e nao muda nada. */
    assert(deck_exchange(&deck, (PokerCard){2, POKER_CLUBS}, (PokerCard){2, POKER_CLUBS}));
    assert(deck_contains(&deck, (PokerCard){2, POKER_CLUBS}));

    while (deck_draw(&deck, &card)) {
        ids[n++] = card_id(card);
    }

    assert(n == 51);

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++)
            assert(ids[i] != ids[j]);
    }

    /* Sem o 9 de paus (id 7) na lista original, o 5 de ouros (id 16) estava na posicao 15. */
    assert(ids[15] == card_id((PokerCard){9, POKER_CLUBS}));
    assert(ids[0] == 0 && ids[6] == 6 && ids[7] == 8 && ids[14] == 15 && ids[16] == 17);
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
    test_remove_card();
    test_exchange();
    puts("OK: 52 cartas unicas, dealer 0/1, streets, queimadas, remocao, troca e validacoes (500 rodadas).");
    return 0;
}
