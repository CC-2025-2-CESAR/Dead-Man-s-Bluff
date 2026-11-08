#include "poker.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void check_error(PokerStatus status)
{
    if (status != POKER_OK) {
        fprintf(stderr, "%s\n", poker_status_name(status));
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    PokerTable table;

    PokerChips stacks[2] = {2000, 2000};

    const PokerCard hole[2][2] = {
        {
            {14, POKER_SPADES},
            {14, POKER_HEARTS}
        },
        {
            {13, POKER_SPADES},
            {13, POKER_HEARTS}
        }
    };

    const PokerCard board[5] = {
        {2, POKER_CLUBS},
        {7, POKER_DIAMONDS},
        {9, POKER_HEARTS},
        {11, POKER_CLUBS},
        {3, POKER_SPADES}
    };

    check_error(
        poker_start_hand(&table, stacks, 0, 50, 100)
    );

    /* Jogador 0 aumenta para 300. */
    check_error(poker_act(
        &table, 0, (PokerAction){POKER_RAISE, 300}
    ));

    /* Jogador 1 paga. */
    check_error(poker_act(
        &table, 1, (PokerAction){POKER_CALL, 0}
    ));

    /* Flop, turn e river: ambos dão check. */
    while (table.phase == POKER_STREET_FINISHED) {
        /*
         * Na integração real, a engine revela aqui as cartas
         * correspondentes à próxima etapa.
         */
        check_error(poker_next_street(&table));

        check_error(poker_act(
            &table, 1, (PokerAction){POKER_CHECK, 0}
        ));

        check_error(poker_act(
            &table, 0, (PokerAction){POKER_CHECK, 0}
        ));
    }

    PokerResult result;

    check_error(
        poker_settle(&table, hole[0], hole[1], board, &result)
    );

    printf("Pote: %" PRId64 "\n", result.pot);

    printf(
        "Jogador 0: %s\n",
        poker_category_name(result.value[0].category)
    );

    printf(
        "Jogador 1: %s\n",
        poker_category_name(result.value[1].category)
    );

    printf(
        "Stacks finais: %" PRId64 " / %" PRId64 "\n",
        table.player[0].chips,
        table.player[1].chips
    );

    return 0;
}
