#include "../src/cards/deck.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *rank_name(int rank)
{
    static const char *const names[] = {
        "?", "?", "2", "3", "4", "5", "6", "7", "8", "9",
        "10", "J", "Q", "K", "A"
    };
    assert(rank >= 2 && rank <= 14);
    return names[rank];
}

static char suit_name(PokerSuit suit)
{
    const char *const names = "CDHS";
    assert((unsigned)suit <= POKER_SPADES);
    return names[suit];
}

static void print_card(PokerCard c)
{
    printf("%s%c", rank_name(c.rank), suit_name(c.suit));
}

static int card_id(PokerCard c)
{
    assert(c.rank >= 2 && c.rank <= 14);
    assert((unsigned)c.suit <= POKER_SPADES);
    return (int)c.suit * 13 + c.rank - 2;
}

static void check_unique(PokerCard hole[2][2], const PokerCard board[5])
{
    int used[52] = {0};
    for (int player = 0; player < 2; ++player)
        for (int i = 0; i < 2; ++i)
            assert(++used[card_id(hole[player][i])] == 1);
    for (int i = 0; i < 5; ++i)
        assert(++used[card_id(board[i])] == 1);
}

static void complete_betting_round(PokerTable *table)
{
    int moves = 0;
    while (table->phase == POKER_ACTING) {
        assert(++moves <= 4);
        int acting = table->actor;
        PokerLegalActions legal;
        assert(poker_legal_actions(table, acting, &legal) == POKER_OK);
        PokerAction action = {legal.check ? POKER_CHECK : POKER_CALL, 0};
        assert(legal.check || legal.call);
        assert(poker_act(table, acting, action) == POKER_OK);
    }
    assert(table->phase == POKER_STREET_FINISHED ||
           table->phase == POKER_SHOWDOWN);
}

static void run_hand(int dealer, int print_first)
{
    Deck deck = {0};
    PokerCard hole[2][2] = {{{0}}};
    PokerCard board[5] = {{0}};
    size_t board_count = 0;
    PokerTable table;
    PokerResult result = {0};
    PokerHandValue direct[2];
    const PokerChips stacks[2] = {1000, 1000};

    assert(deck_create(&deck));
    assert(deck.count == 52);
    deck_shuffle(&deck);
    assert(poker_start_hand(&table, stacks, dealer, 5, 10) == POKER_OK);
    assert(deck_deal(&deck, dealer, hole));
    assert(deck.count == 48);

    complete_betting_round(&table);
    assert(poker_next_street(&table) == POKER_OK);
    assert(table.street == POKER_FLOP);
    assert(deck_deal_board(&deck, table.street, board, &board_count));
    assert(deck.count == 44 && board_count == 3);
    for (int i = 0; i < 2; ++i)
        assert(poker_evaluate_holdem(hole[i], board, board_count, &direct[i]) == POKER_OK);

    complete_betting_round(&table);
    assert(poker_next_street(&table) == POKER_OK);
    assert(table.street == POKER_TURN);
    assert(deck_deal_board(&deck, table.street, board, &board_count));
    assert(deck.count == 42 && board_count == 4);
    for (int i = 0; i < 2; ++i)
        assert(poker_evaluate_holdem(hole[i], board, board_count, &direct[i]) == POKER_OK);

    complete_betting_round(&table);
    assert(poker_next_street(&table) == POKER_OK);
    assert(table.street == POKER_RIVER);
    assert(deck_deal_board(&deck, table.street, board, &board_count));
    assert(deck.count == 40 && board_count == 5);
    for (int i = 0; i < 2; ++i)
        assert(poker_evaluate_holdem(hole[i], board, board_count, &direct[i]) == POKER_OK);
    check_unique(hole, board);
    int comparison = poker_compare(&direct[0], &direct[1]);
    unsigned expected_winners = comparison > 0 ? 1u : comparison < 0 ? 2u : 3u;

    complete_betting_round(&table);
    assert(table.phase == POKER_SHOWDOWN);
    assert(poker_settle(&table, (const PokerCard (*)[2])hole, board, &result) == POKER_OK);
    assert(result.winners == expected_winners);
    assert(result.showdown);
    assert(poker_compare(&result.value[0], &direct[0]) == 0);
    assert(poker_compare(&result.value[1], &direct[1]) == 0);
    assert(result.pot == 20);
    assert(result.award[0] + result.award[1] == result.contested);
    assert(table.phase == POKER_SETTLED);
    assert(table.player[0].chips + table.player[1].chips == 2000);

    if (print_first) {
        printf("SIMULACAO COMPLETA: dealer %d\n", dealer);
        for (int i = 0; i < 2; ++i) {
            printf("  Jogador %d: ", i);
            print_card(hole[i][0]); putchar(' '); print_card(hole[i][1]);
            printf(" | %s | desempate [%d,%d,%d,%d,%d]\n",
                poker_category_name(result.value[i].category),
                result.value[i].tiebreak[0], result.value[i].tiebreak[1],
                result.value[i].tiebreak[2], result.value[i].tiebreak[3],
                result.value[i].tiebreak[4]);
        }
        printf("  Mesa: ");
        for (int i = 0; i < 5; ++i) {
            print_card(board[i]); putchar(i == 4 ? '\n' : ' ');
        }
        printf("  Resultado: %s | pote: %lld | saldos finais: %lld / %lld\n",
            result.winners == 1 ? "jogador 0" :
            result.winners == 2 ? "jogador 1" : "empate",
            (long long)result.pot,
            (long long)table.player[0].chips,
            (long long)table.player[1].chips);
    }
    deck_free(&deck);
    assert(deck.head == NULL && deck.tail == NULL && deck.count == 0);
}

static PokerCard card(int rank, PokerSuit suit)
{
    PokerCard result = {rank, suit};
    return result;
}

static void targeted_tests(void)
{
    PokerHandValue a, b;
    /* Par de ases com K versus par de ases com Q (kicker) */
    PokerCard board[5] = {
        card(14, POKER_CLUBS), card(14, POKER_DIAMONDS),
        card(2, POKER_HEARTS), card(4, POKER_SPADES), card(7, POKER_CLUBS)
    };
    PokerCard better[2] = {card(13, POKER_SPADES), card(9, POKER_DIAMONDS)};
    PokerCard worse[2] = {card(12, POKER_HEARTS), card(9, POKER_CLUBS)};
    assert(poker_evaluate_holdem(better, board, 5, &a) == POKER_OK);
    assert(poker_evaluate_holdem(worse, board, 5, &b) == POKER_OK);
    assert(a.category == POKER_ONE_PAIR && b.category == POKER_ONE_PAIR);
    assert(poker_compare(&a, &b) > 0);

    /* Mesa faz sequencia maxima para ambos: empate */
    PokerCard shared[5] = {
        card(10, POKER_CLUBS), card(11, POKER_DIAMONDS),
        card(12, POKER_HEARTS), card(13, POKER_SPADES), card(14, POKER_CLUBS)
    };
    PokerCard h0[2] = {card(2, POKER_CLUBS), card(3, POKER_DIAMONDS)};
    PokerCard h1[2] = {card(4, POKER_HEARTS), card(5, POKER_SPADES)};
    assert(poker_evaluate_holdem(h0, shared, 5, &a) == POKER_OK);
    assert(poker_evaluate_holdem(h1, shared, 5, &b) == POKER_OK);
    assert(a.category == POKER_STRAIGHT && b.category == POKER_STRAIGHT);
    assert(poker_compare(&a, &b) == 0);

    /* Carta repetida: avaliador recusa. */
    PokerCard invalid[2] = {card(10, POKER_CLUBS), card(2, POKER_HEARTS)};
    assert(poker_evaluate_holdem(invalid, shared, 5, &a) == POKER_ERR_DUPLICATE_CARD);

    /* Flush pode ser melhorado com uma sexta carta do mesmo naipe. */
    PokerCard diamonds[5] = {
        card(2, POKER_DIAMONDS), card(5, POKER_DIAMONDS),
        card(7, POKER_DIAMONDS), card(9, POKER_DIAMONDS), card(13, POKER_DIAMONDS)
    };
    PokerCard flush_hand[2] = {card(3, POKER_DIAMONDS), card(14, POKER_SPADES)};
    PokerCard pair_hand[2] = {card(13, POKER_SPADES), card(13, POKER_HEARTS)};
    assert(poker_evaluate_holdem(flush_hand, diamonds, 5, &a) == POKER_OK);
    assert(poker_evaluate_holdem(pair_hand, diamonds, 5, &b) == POKER_OK);
    /* O 3 de ouros melhora o flush ao substituir o 2 do board. */
    assert(poker_compare(&a, &b) > 0);
}

int main(void)
{
    srand(20261107);
    targeted_tests();
    run_hand(0, 1);
    run_hand(1, 1);
    for (int i = 0; i < 1000; ++i)
        run_hand(i % 2, 0);
    puts("OK: 1.002 partidas heads-up completas + cenarios direcionados.");
    return 0;
}
