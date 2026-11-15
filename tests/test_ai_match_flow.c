/* Teste de integracao: duas IAs jogam maos completas usando o motor de poker.
 * Sem Raylib, sem dependencia de outras areas do jogo. */
#include "poker_ai_config.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t game_seed = UINT64_C(0x2B992DDFA23249D6);
static uint64_t next_u64(void)
{
    uint64_t x = game_seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    game_seed = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}

static void deal_nine(PokerCard hole[2][2], PokerCard board[5])
{
    PokerCard cards[52];
    int n = 0;
    for (int s = POKER_CLUBS; s <= POKER_SPADES; ++s)
        for (int r = 2; r <= 14; ++r)
            cards[n++] = (PokerCard){r, (PokerSuit)s};
    for (int i = 0; i < 9; ++i) {
        int swap = i + (int)(next_u64() % (uint64_t)(52 - i));
        PokerCard tmp = cards[i];
        cards[i] = cards[swap];
        cards[swap] = tmp;
    }
    hole[0][0] = cards[0]; hole[0][1] = cards[1];
    hole[1][0] = cards[2]; hole[1][1] = cards[3];
    for (int i = 0; i < 5; ++i)
        board[i] = cards[i + 4];
}

static int stats[4][6]; /* street x fold/check/call/bet/raise/all-in */
static int showdowns, uncontested, calls_capped, action_count;

static void play_hand(int number, PokerAIStyle first, PokerAIStyle second,
                      const PokerAIScenarioTable *custom_table,
                      PokerChips stack0, PokerChips stack1)
{
    PokerCard hole[2][2], board[5];
    deal_nine(hole, board);
    PokerChips stack[2] = {stack0, stack1};
    PokerChips initial_total = stack0 + stack1;
    PokerTable table;
    PokerStatus status = poker_start_hand(&table, stack, number % 2, 10, 20);
    assert(status == POKER_OK);

    PokerAIRng actions[2], equity[2];
    PokerAIContext contexts[2] = {
        {first, custom_table, &actions[0], &equity[0]},
        {second, custom_table, &actions[1], &equity[1]}
    };
    for (int s = 0; s < 2; ++s) {
        poker_ai_rng_seed(&actions[s], next_u64());
        poker_ai_rng_seed(&equity[s], next_u64());
    }

    int turns = 0;
    while (table.phase == POKER_ACTING || table.phase == POKER_STREET_FINISHED) {
        assert(turns++ < 200);
        if (table.phase == POKER_STREET_FINISHED) {
            assert(table.street != POKER_RIVER);
            assert(poker_next_street(&table) == POKER_OK);
            continue;
        }
        int seat = table.actor;
        assert(seat == 0 || seat == 1);
        size_t board_count = table.street == POKER_PREFLOP ? 0 :
            table.street == POKER_FLOP ? 3 :
            table.street == POKER_TURN ? 4 : 5;
        PokerTable before = table;
        PokerAIDecisionInfo info;
        PokerAction action;
        status = poker_ai_choose_action(&table, seat, hole[seat],
                  board_count ? board : NULL, board_count,
                  &contexts[seat], &info, &action);
        if (status != POKER_OK) {
            fprintf(stderr, "Falha IA hand=%d turns=%d seat=%d street=%d: %s\n",
                    number, turns, seat, (int)table.street,
                    poker_status_name(status));
            assert(status == POKER_OK);
        }
        assert(table.actor == before.actor);
        assert(table.street == before.street);
        assert(table.phase == before.phase);
        assert(table.current_bet == before.current_bet);
        assert(table.min_raise == before.min_raise);
        assert(table.big_blind == before.big_blind);
        for (int idx = 0; idx < 2; ++idx) {
            assert(table.player[idx].chips == before.player[idx].chips);
            assert(table.player[idx].committed == before.player[idx].committed);
            assert(table.player[idx].street_bet == before.player[idx].street_bet);
            assert(table.player[idx].folded == before.player[idx].folded);
            assert(table.player[idx].acted == before.player[idx].acted);
        }
        assert(action.type >= POKER_FOLD && action.type <= POKER_ALL_IN);
        assert(info.street == table.street);
        assert(info.profile == contexts[seat].style);
        if (info.capped_all_in_call) ++calls_capped;
        ++stats[table.street][action.type];
        ++action_count;
        status = poker_act(&table, seat, action);
        if (status != POKER_OK) {
            fprintf(stderr, "Acao ilegal hand=%d turns=%d seat=%d street=%d type=%d total=%lld: %s\n",
                    number, turns, seat, (int)before.street, (int)action.type,
                    (long long)action.total, poker_status_name(status));
            assert(status == POKER_OK);
        }
        assert(table.player[0].chips >= 0 && table.player[1].chips >= 0);
        assert(table.player[0].chips + table.player[1].chips + poker_pot(&table) == initial_total);
    }

    assert(table.phase == POKER_SHOWDOWN || table.phase == POKER_UNCONTESTED);
    if (table.phase == POKER_SHOWDOWN) ++showdowns;
    else ++uncontested;
    PokerResult result;
    status = poker_settle(&table, hole[0], hole[1], board, &result);
    assert(status == POKER_OK);
    assert(table.phase == POKER_SETTLED);
    assert(table.player[0].chips + table.player[1].chips == initial_total);
    assert(poker_pot(&table) == 0);
    assert(result.winners == 1u || result.winners == 2u || result.winners == 3u);
    assert(poker_settle(&table, hole[0], hole[1], board, &result) == POKER_ERR_STATE);
}

int main(void)
{
    int serial = 0;
    for (int p = POKER_AI_CAUTIOUS; p <= POKER_AI_AGGRESSIVE; ++p)
        for (int opponent = POKER_AI_CAUTIOUS; opponent <= POKER_AI_AGGRESSIVE; ++opponent)
            for (int i = 0; i < 90; ++i)
                play_hand(serial++, (PokerAIStyle)p, (PokerAIStyle)opponent, NULL, 2000, 2000);

    /* Uma tabela somente CHECK/CALL obriga maos ate o river;
     * continua submetida a legalidade do motor. */
    static PokerAIScenarioTable passive;
    for (int p = 0; p < POKER_AI_PROFILE_COUNT; ++p)
        for (int st = 0; st < POKER_AI_STREET_COUNT; ++st)
            for (int h = 0; h < POKER_AI_STRENGTH_COUNT; ++h)
                for (int pr = 0; pr < POKER_AI_PRESSURE_COUNT; ++pr)
                    passive[p][st][h][pr].weight[POKER_AI_ACTION_PASSIVE] = 100;
    for (int i = 0; i < 90; ++i)
        play_hand(serial++, POKER_AI_CAUTIOUS, POKER_AI_AGGRESSIVE,
                  (const PokerAIScenarioTable *)(const void *)&passive, 2000, 2000);

    /* Stacks desiguais e curtos: forcam calls capados, devolucoes
     * de apostas nao cobertas e termino antecipado da partida. */
    for (int i = 0; i < 450; ++i) {
        PokerChips first_stack = i % 2 ? 40 : 150;
        play_hand(serial++, (PokerAIStyle)(i % 3),
                  (PokerAIStyle)((i + 1) % 3), NULL, first_stack, 1200);
    }

    assert(stats[POKER_PREFLOP][POKER_CALL] > 0);
    assert(stats[POKER_FLOP][POKER_CHECK] > 0);
    assert(stats[POKER_TURN][POKER_CHECK] > 0);
    assert(stats[POKER_RIVER][POKER_CHECK] > 0);
    assert(showdowns >= 90);
    assert(calls_capped > 0);
    printf("Integracao completa: %d maos, %d acoes, %d showdowns, %d folds finais, %d calls capados.\n",
           serial, action_count, showdowns, uncontested, calls_capped);
    for (int st = 0; st < 4; ++st)
        printf("Street %d: fold=%d check=%d call=%d bet=%d raise=%d all-in=%d\n",
            st, stats[st][POKER_FOLD], stats[st][POKER_CHECK], stats[st][POKER_CALL],
            stats[st][POKER_BET], stats[st][POKER_RAISE], stats[st][POKER_ALL_IN]);
    return 0;
}
