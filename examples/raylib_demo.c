#include "poker.h"
#include "raylib.h"

#include <stdio.h>

static const PokerCard hole[2][2] = {
    {{14, POKER_SPADES}, {14, POKER_HEARTS}},
    {{13, POKER_SPADES}, {13, POKER_HEARTS}}
};

static const PokerCard board[5] = {
    {2, POKER_CLUBS},
    {7, POKER_DIAMONDS},
    {9, POKER_HEARTS},
    {11, POKER_CLUBS},
    {3, POKER_SPADES}
};

int main(void)
{
    PokerTable table;
    PokerResult result = {0};
    const PokerChips stacks[2] = {2000, 2000};

    PokerStatus status =
        poker_start_hand(&table, stacks, 0, 50, 100);

    if (status != POKER_OK) {
        fprintf(stderr, "%s\n", poker_status_name(status));
        return 1;
    }

    InitWindow(900, 500, "Dead Man's Bluff - teste de integracao");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        /*
         * Controlamos os dois jogadores para testar as regras.
         * Ainda nao existe IA nesta demonstracao.
         */
        if (table.phase == POKER_ACTING) {
            PokerLegalActions legal;

            status = poker_legal_actions(
                &table, table.actor, &legal
            );

            if (status == POKER_OK) {
                PokerAction action = {POKER_CHECK, 0};
                bool requested = false;

                if (IsKeyPressed(KEY_C)) {
                    if (legal.check || legal.call) {
                        action.type =
                            legal.check ? POKER_CHECK : POKER_CALL;
                        requested = true;
                    }
                } else if (IsKeyPressed(KEY_B)) {
                    if (legal.bet || legal.raise) {
                        action.type =
                            legal.bet ? POKER_BET : POKER_RAISE;
                        action.total = legal.min_total;
                        requested = true;
                    }
                } else if (IsKeyPressed(KEY_A) && legal.all_in) {
                    action.type = POKER_ALL_IN;
                    requested = true;
                } else if (IsKeyPressed(KEY_F) && legal.fold) {
                    action.type = POKER_FOLD;
                    requested = true;
                }

                if (requested) {
                    status = poker_act(
                        &table, table.actor, action
                    );
                }
            }
        } else if (IsKeyPressed(KEY_N)) {
            if (table.phase == POKER_STREET_FINISHED) {
                status = poker_next_street(&table);
            } else if (table.phase == POKER_SHOWDOWN ||
                       table.phase == POKER_UNCONTESTED) {
                status = poker_settle(
                    &table, hole[0], hole[1], board, &result
                );
            }
        }

        BeginDrawing();
        ClearBackground((Color){20, 60, 45, 255});

        DrawText("TESTE DO BACKEND + RAYLIB", 25, 25, 28, GOLD);

        DrawText(
            TextFormat("Jogador 0: %lld fichas",
                       (long long)table.player[0].chips),
            25, 85, 22, RAYWHITE
        );

        DrawText(
            TextFormat("Jogador 1: %lld fichas",
                       (long long)table.player[1].chips),
            25, 120, 22, RAYWHITE
        );

        DrawText(
            TextFormat("Pote: %lld",
                       (long long)poker_pot(&table)),
            25, 155, 22, RAYWHITE
        );

        const char *streets[] = {
            "Pre-flop", "Flop", "Turn", "River"
        };

        DrawText(streets[table.street], 25, 195, 22, GOLD);

        if (table.phase == POKER_ACTING) {
            DrawText(
                TextFormat("Vez do jogador %d", table.actor),
                25, 240, 24, RAYWHITE
            );

            DrawText("C: passar/pagar | B: apostar/aumentar",
                     25, 285, 20, RAYWHITE);
            DrawText("A: all-in | F: desistir",
                     25, 315, 20, RAYWHITE);

        } else if (table.phase == POKER_STREET_FINISHED) {
            DrawText("N: avancar para a proxima etapa",
                     25, 240, 22, RAYWHITE);

        } else if (table.phase == POKER_SHOWDOWN ||
                   table.phase == POKER_UNCONTESTED) {
            DrawText("N: calcular resultado e pagar pote",
                     25, 240, 22, RAYWHITE);

        } else if (table.phase == POKER_SETTLED) {
            const char *winner =
                result.winners == 3 ? "Empate" :
                result.winners == 1 ? "Jogador 0 venceu" :
                                      "Jogador 1 venceu";

            DrawText(winner, 25, 240, 26, GOLD);
            DrawText("Mao encerrada. ESC: sair.",
                     25, 285, 20, RAYWHITE);
        }

        if (status != POKER_OK) {
            DrawText(poker_status_name(status),
                     25, 390, 20, RED);
        }

        DrawText("Cartas fixas: jogador 0 = AA; jogador 1 = KK.",
                 25, 435, 18, LIGHTGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
