#include "poker.h"

#include <string.h>

const char *poker_status_name(PokerStatus status)
{
    static const char *const names[] = {
        "OK",
        "Argumento invalido",
        "Carta invalida",
        "Carta repetida",
        "Quantidade de fichas invalida",
        "Estado invalido",
        "Nao e o turno deste jogador",
        "Acao ilegal"
    };

    if ((unsigned)status >= sizeof names / sizeof names[0])
        return "Erro desconhecido";

    return names[status];
}

const char *poker_category_name(PokerCategory category)
{
    static const char *const names[] = {
        "Carta alta",
        "Um par",
        "Dois pares",
        "Trinca",
        "Sequencia",
        "Flush",
        "Full house",
        "Quadra",
        "Straight flush"
    };

    if ((unsigned)category >= sizeof names / sizeof names[0])
        return "Categoria invalida";

    return names[category];
}

int poker_compare(
    const PokerHandValue *a,
    const PokerHandValue *b
)
{
    if (a->category != b->category)
        return a->category > b->category ? 1 : -1;

    for (int i = 0; i < 5; ++i) {
        if (a->tiebreak[i] != b->tiebreak[i])
            return a->tiebreak[i] > b->tiebreak[i] ? 1 : -1;
    }

    return 0;
}

/* Função interna: as cartas já foram validadas. */
static PokerHandValue evaluate_five(const PokerCard cards[5])
{
    PokerHandValue value = {0};

    int counts[15] = {0};
    int ranks[5];
    int groups[5];
    int group_count = 0;

    bool flush = true;

    memcpy(value.best, cards, sizeof value.best);

    for (int i = 0; i < 5; ++i) {
        ++counts[cards[i].rank];

        if (cards[i].suit != cards[0].suit)
            flush = false;
    }

    /* Ranks em ordem decrescente, incluindo repetições. */
    int index = 0;

    for (int rank = 14; rank >= 2; --rank) {
        for (int j = 0; j < counts[rank]; ++j)
            ranks[index++] = rank;
    }

    int straight_high = 0;

    for (int high = 14; high >= 6; --high) {
        if (counts[high] &&
            counts[high - 1] &&
            counts[high - 2] &&
            counts[high - 3] &&
            counts[high - 4]) {
            straight_high = high;
            break;
        }
    }

    /* Ás também pode ser baixo: A-2-3-4-5. */
    if (!straight_high &&
        counts[14] && counts[5] && counts[4] &&
        counts[3] && counts[2]) {
        straight_high = 5;
    }

    /*
     * Agrupa por quantidade e depois por rank.
     * Exemplo: A-A-K-K-8 -> grupos A, K, 8.
     */
    for (int copies = 4; copies >= 1; --copies) {
        for (int rank = 14; rank >= 2; --rank) {
            if (counts[rank] == copies)
                groups[group_count++] = rank;
        }
    }

    if (flush && straight_high) {
        value.category = POKER_STRAIGHT_FLUSH;
        value.tiebreak[0] = straight_high;

    } else if (counts[groups[0]] == 4) {
        value.category = POKER_FOUR_OF_A_KIND;
        value.tiebreak[0] = groups[0];
        value.tiebreak[1] = groups[1];

    } else if (counts[groups[0]] == 3 &&
               counts[groups[1]] == 2) {
        value.category = POKER_FULL_HOUSE;
        value.tiebreak[0] = groups[0];
        value.tiebreak[1] = groups[1];

    } else if (flush) {
        value.category = POKER_FLUSH;
        memcpy(value.tiebreak, ranks, sizeof ranks);

    } else if (straight_high) {
        value.category = POKER_STRAIGHT;
        value.tiebreak[0] = straight_high;

    } else if (counts[groups[0]] == 3) {
        value.category = POKER_THREE_OF_A_KIND;
        memcpy(value.tiebreak, groups, 3 * sizeof groups[0]);

    } else if (counts[groups[0]] == 2 &&
               counts[groups[1]] == 2) {
        value.category = POKER_TWO_PAIR;
        memcpy(value.tiebreak, groups, 3 * sizeof groups[0]);

    } else if (counts[groups[0]] == 2) {
        value.category = POKER_ONE_PAIR;
        memcpy(value.tiebreak, groups, 4 * sizeof groups[0]);

    } else {
        value.category = POKER_HIGH_CARD;
        memcpy(value.tiebreak, ranks, sizeof ranks);
    }

    return value;
}

PokerStatus poker_evaluate(
    const PokerCard *cards,
    size_t count,
    PokerHandValue *out
)
{
    if (!cards || !out || count < 5 || count > 7)
        return POKER_ERR_ARGUMENT;

    uint64_t seen = 0;

    for (size_t i = 0; i < count; ++i) {
        if (cards[i].rank < 2 ||
            cards[i].rank > 14 ||
            (unsigned)cards[i].suit > POKER_SPADES) {
            return POKER_ERR_CARD;
        }

        unsigned id =
            (unsigned)cards[i].suit * 13u +
            (unsigned)(cards[i].rank - 2);

        uint64_t bit = UINT64_C(1) << id;

        if (seen & bit)
            return POKER_ERR_DUPLICATE_CARD;

        seen |= bit;
    }

    PokerHandValue best = {0};
    bool has_best = false;

    for (size_t a = 0; a + 4 < count; ++a)
        for (size_t b = a + 1; b + 3 < count; ++b)
            for (size_t c = b + 1; c + 2 < count; ++c)
                for (size_t d = c + 1; d + 1 < count; ++d)
                    for (size_t e = d + 1; e < count; ++e) {
                        PokerCard five[5] = {
                            cards[a], cards[b], cards[c],
                            cards[d], cards[e]
                        };

                        PokerHandValue candidate =
                            evaluate_five(five);

                        if (!has_best ||
                            poker_compare(&candidate, &best) > 0) {
                            best = candidate;
                            has_best = true;
                        }
                    }

    *out = best;
    return POKER_OK;
}

PokerStatus poker_evaluate_holdem(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerHandValue *out
)
{
    if (!hole || !board || !out ||
        board_count < 3 || board_count > 5) {
        return POKER_ERR_ARGUMENT;
    }

    PokerCard cards[7];

    memcpy(cards, hole, 2 * sizeof cards[0]);
    memcpy(cards + 2, board, board_count * sizeof cards[0]);

    return poker_evaluate(cards, board_count + 2, out);
}