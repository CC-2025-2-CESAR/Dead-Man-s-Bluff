#include "trapacas.h"
#include <stdlib.h> // Para rand()

void cheat_init_state(CheatState *state) {
    if (!state) return;
    state->suspeita_acumulada = 0;
    state->maos_embriagado = 0;
    state->usos_cumplice_blind = 0;
    state->suborno_usado_na_mao = false;
    state->as_na_manga_usado = false;
    
    state->carta_na_manga.rank = 2; // Começa com um 2 de paus
    state->carta_na_manga.suit = POKER_CLUBS; 
    
    for (int i = 0; i < 5; i++) {
        state->cartas_marcadas[i].rank = 0; // 0 significa slot vazio
        state->turnos_marcacao[i] = 0;
    }
}

CheatResult cheat_spike_drink(CheatState *state, int base_risk) {
    if (!state) return TRAPACA_DESCOBERTA;

    // Calcula a chance de ser pego (Risco da ação + Suspeita atual)
    int chance_descoberta = base_risk + state->suspeita_acumulada;
    
    // RNG de 1 a 100
    int roll = (rand() % 100) + 1;

    // Foi pego:

    if (roll <= chance_descoberta) {
        // A suspeita deve cair consideravelmente após a punição (Roleta)
        state->suspeita_acumulada = state->suspeita_acumulada / 2; 
        return TRAPACA_DESCOBERTA;
    }

    // Sucesso:

    // Define a duração da embriaguez (ex: 3 mãos)
    state->maos_embriagado = 3; 
    
    // Aumenta a suspeita para as próximas tentativas (ex: +15%)
    state->suspeita_acumulada += 15; 
    
    // Limita a suspeita a 100%
    if (state->suspeita_acumulada > 100) {
        state->suspeita_acumulada = 100;
    }

    return TRAPACA_SUCESSO;
}