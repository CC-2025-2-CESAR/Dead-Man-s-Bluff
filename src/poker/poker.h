#ifndef POKER_H
#define POKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* J=11, Q=12, K=13, A=14. */
typedef enum {
    POKER_CLUBS = 0, /* paus */
    POKER_DIAMONDS,  /* ouros */
    POKER_HEARTS,    /* copas */
    POKER_SPADES     /* espadas */
} PokerSuit;

typedef struct {
    int rank; /* 2..14 */
    PokerSuit suit;
} PokerCard;

typedef int64_t PokerChips;

#define POKER_MAX_STACK (INT64_MAX / 4)
#define POKER_NO_PLAYER (-1)

typedef enum {
    POKER_OK = 0,
    POKER_ERR_ARGUMENT,
    POKER_ERR_CARD,
    POKER_ERR_DUPLICATE_CARD,
    POKER_ERR_CHIPS,
    POKER_ERR_STATE,
    POKER_ERR_TURN,
    POKER_ERR_ACTION
} PokerStatus;

/* Categorias em ordem crescente de força. */
typedef enum {
    POKER_HIGH_CARD = 0,
    POKER_ONE_PAIR,
    POKER_TWO_PAIR,
    POKER_THREE_OF_A_KIND,
    POKER_STRAIGHT,
    POKER_FLUSH,
    POKER_FULL_HOUSE,
    POKER_FOUR_OF_A_KIND,
    POKER_STRAIGHT_FLUSH
} PokerCategory;

typedef struct {
    PokerCategory category;

    /*
     * Valores usados para desempatar, em ordem de importância.
     * Exemplo: dois pares -> par maior, par menor, kicker.
     */
    int tiebreak[5];

    /* As cinco cartas escolhidas pelo avaliador. */
    PokerCard best[5];
} PokerHandValue;

typedef enum {
    POKER_PREFLOP = 0,
    POKER_FLOP,
    POKER_TURN,
    POKER_RIVER
} PokerStreet;

typedef enum {
    POKER_ACTING = 0,
    POKER_STREET_FINISHED,
    POKER_SHOWDOWN,
    POKER_UNCONTESTED,
    POKER_SETTLED
} PokerPhase;

typedef enum {
    POKER_FOLD = 0,
    POKER_CHECK,
    POKER_CALL,
    POKER_BET,
    POKER_RAISE,
    POKER_ALL_IN
} PokerActionType;

typedef struct {
    PokerActionType type;

    /*
     * BET/RAISE: total desejado na etapa atual.
     * Demais ações: zero.
     *
     * Se já colocou 100 e quer aumentar PARA 300:
     * total = 300; o módulo desconta somente mais 200.
     */
    PokerChips total;
} PokerAction;

typedef struct {
    PokerChips chips;      /* Fichas disponíveis. */
    PokerChips committed;  /* Investimento total nesta mão. */
    PokerChips street_bet; /* Investimento na etapa atual. */

    bool folded;
    bool acted;
} PokerPlayer;

typedef struct {
    PokerPlayer player[2];

    int dealer; /* Também é o small blind no heads-up. */
    int actor;  /* Jogador que deve agir; -1 quando não há ação. */

    PokerStreet street;
    PokerPhase phase;

    PokerChips big_blind;
    PokerChips current_bet;
    PokerChips min_raise;
    PokerChips initial_total;
} PokerTable;

typedef struct {
    bool fold;
    bool check;
    bool call;
    bool bet;
    bool raise;
    bool all_in;

    PokerChips to_call;
    PokerChips call_cost;
    PokerChips min_total;
    PokerChips max_total;
} PokerLegalActions;

typedef struct {
    /*
     * 1 = jogador 0 venceu
     * 2 = jogador 1 venceu
     * 3 = empate
     */
    unsigned winners;

    PokerChips pot;
    PokerChips contested;

    PokerChips refund[2]; /* Devolução de aposta não coberta. */
    PokerChips award[2];  /* Prêmio recebido do pote disputado. */

    bool showdown;
    PokerHandValue value[2];
} PokerResult;

/* Avaliação: aceita 5, 6 ou 7 cartas. */
PokerStatus poker_evaluate(
    const PokerCard *cards,
    size_t count,
    PokerHandValue *out
);

/* board_count: 3 no flop, 4 no turn ou 5 no river. */
PokerStatus poker_evaluate_holdem(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerHandValue *out
);

/* Retorna -1, 0 ou +1. Recebe avaliações válidas. */
int poker_compare(
    const PokerHandValue *a,
    const PokerHandValue *b
);

const char *poker_category_name(PokerCategory category);
const char *poker_status_name(PokerStatus status);

/* Inicializa a mão e desconta os blinds. */
PokerStatus poker_start_hand(
    PokerTable *table,
    const PokerChips stacks[2],
    int dealer,
    PokerChips small_blind,
    PokerChips big_blind
);

PokerStatus poker_legal_actions(
    const PokerTable *table,
    int seat,
    PokerLegalActions *out
);

PokerStatus poker_act(
    PokerTable *table,
    int seat,
    PokerAction action
);

/* Exige que a rodada de apostas anterior tenha terminado. */
PokerStatus poker_next_street(PokerTable *table);

PokerChips poker_pot(const PokerTable *table);

PokerStatus poker_divide_pot(
    PokerChips pot,
    unsigned winners,
    int dealer,
    PokerChips awards[2]
);

/*
 * Em showdown: exige duas cartas por jogador e board completo.
 * Em vitória por fold: hole0, hole1 e board podem ser NULL.
 *
 * Atualiza os stacks e impede pagamento duplicado.
 */
PokerStatus poker_settle(
    PokerTable *table,
    const PokerCard hole0[2],
    const PokerCard hole1[2],
    const PokerCard board[5],
    PokerResult *out
);

#endif
