#include <stdio.h>
#include "poker_ai_config.h"

int main(void)
{
    int checked = 0;
    int free_checked = 0;
    for (int profile = 0; profile < POKER_AI_PROFILE_COUNT; ++profile)
    for (int street = 0; street < POKER_AI_STREET_COUNT; ++street)
    for (int strength = 0; strength < POKER_AI_STRENGTH_COUNT; ++strength)
    for (int pressure = 0; pressure < POKER_AI_PRESSURE_COUNT; ++pressure) {
        const PokerAIScenario *s =
            &poker_ai_scenario_table[profile][street][strength][pressure];
        double sum = 0;
        for (int slot = 0; slot < POKER_AI_ACTION_SLOT_COUNT; ++slot) {
            if (s->weight[slot] < 0 || s->weight[slot] > 100) {
                fprintf(stderr, "Peso fora do limite: %d/%d/%d/%d/%d\n",
                        profile, street, strength, pressure, slot);
                return 1;
            }
            sum += s->weight[slot];
        }
        if (sum != 100.0) {
            fprintf(stderr, "Soma diferente de 100: %d/%d/%d/%d\n",
                    profile, street, strength, pressure);
            return 1;
        }
        if (pressure == POKER_AI_PRESSURE_FREE) {
            ++free_checked;
            if (s->weight[POKER_AI_ACTION_FOLD] != 0) {
                fprintf(stderr, "Fold nao zerado no FREE: %d/%d/%d\n",
                        profile, street, strength);
                return 1;
            }
        }
        ++checked;
    }
    if (checked != 240 || free_checked != 48) {
        fprintf(stderr, "Contagem incorreta: %d e %d\n",
                checked, free_checked);
        return 1;
    }
    puts("240 cenarios validos; 48 FREE com FOLD=0.");
    return 0;
}
