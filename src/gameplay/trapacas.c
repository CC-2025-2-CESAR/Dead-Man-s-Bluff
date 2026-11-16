#include "trapacas.h"
#include <stdlib.h> 

void cheat_init_state(CheatState *state) {
    if (!state) return;

    state->suspeita_acumulada = 0;
    // Nota: state->balas_roleta não é inicializada aqui para não zerar entre confrontos

    state->maos_embriagado = 0;
    state->usos_cumplice_blind = 0;
    state->suborno_usado_na_mao = false;
    state->as_na_manga_usado = false;

    // Na primeira mesa, o jogador deve ter sua carta padrão se ela ainda não existir
    if (state->carta_na_manga.rank == 0) {
        state->carta_na_manga.rank = 2; // Começa com um 2 de paus
        state->carta_na_manga.suit = POKER_CLUBS; 
    }

    for (int i = 0; i < 5; i++) {
        state->cartas_marcadas[i].rank = 0; // 0 significa slot vazio
        state->turnos_marcacao[i] = 0;
    }

    state->carta_comprada_pendente = false;
    state->destino_carta_comprada = -1;
    state->carta_comprada.rank = 0;
}

void cheat_update_end_of_hand(CheatState *state) {
    if (!state) return;

    if (state->maos_embriagado > 0) state->maos_embriagado--;

    state->suborno_usado_na_mao = false;
    // state->as_na_manga_usado = false; // Removido ou resetado no loop só APÓS embaralhar

    for (int i = 0; i < 5; i++) {
        if (state->turnos_marcacao[i] > 0) {
            state->turnos_marcacao[i]--;
            if (state->turnos_marcacao[i] == 0) {
                state->cartas_marcadas[i].rank = 0; 
            }
        }
    }
}

bool is_card_marked(const CheatState *state, PokerCard card) {
    if (!state) return false;

    for (int i = 0; i < 5; i++) {
        if (state->cartas_marcadas[i].rank == card.rank && 
            state->cartas_marcadas[i].suit == card.suit &&
            state->turnos_marcacao[i] > 0) {
            return true;
        }
    }
    return false;
}

CheatResult cheat_spike_drink(CheatState *state, int base_risk) {
    if (!state) return TRAPACA_DESCOBERTA;

    int chance_descoberta = base_risk + state->suspeita_acumulada;
    int roll = (rand() % 100) + 1;

    if (roll <= chance_descoberta) {
        state->suspeita_acumulada = state->suspeita_acumulada / 2; 
        return TRAPACA_DESCOBERTA;
    }

    state->maos_embriagado = 3; 
    state->suspeita_acumulada += 15; 
    if (state->suspeita_acumulada > 100) state->suspeita_acumulada = 100;

    return TRAPACA_SUCESSO;
}

CheatResult cheat_signal_accomplice(CheatState *state, PokerTable *table, int seat, int base_risk) {
    if (!state || !table || (seat != 0 && seat != 1)) return TRAPACA_DESCOBERTA;

    int multiplicador = state->usos_cumplice_blind + 1;
    PokerChips custo = table->big_blind * multiplicador;

    if (table->player[seat].chips < custo) return TRAPACA_FUNDO_INSUFICIENTE;

    int chance_descoberta = base_risk + state->suspeita_acumulada;
    int roll = (rand() % 100) + 1;

    if (roll <= chance_descoberta) {
        state->suspeita_acumulada = state->suspeita_acumulada / 2;
        return TRAPACA_DESCOBERTA;
    }

    table->player[seat].chips -= custo;
    state->usos_cumplice_blind++;
    state->suspeita_acumulada += 10;
    if (state->suspeita_acumulada > 100) state->suspeita_acumulada = 100;

    return TRAPACA_SUCESSO;
}

CheatResult cheat_mark_card(CheatState *state, PokerCard card, int base_risk, int duration) {
    if (!state) return TRAPACA_DESCOBERTA;

    int slot = -1;
    for (int i = 0; i < 5; i++) {
        if (state->cartas_marcadas[i].rank == 0) {
            slot = i;
            break;
        }
    }

    if (slot == -1) return TRAPACA_RECUSADA; 

    int chance_descoberta = base_risk + state->suspeita_acumulada;
    int roll = (rand() % 100) + 1;

    if (roll <= chance_descoberta) {
        state->suspeita_acumulada = state->suspeita_acumulada / 2;
        return TRAPACA_DESCOBERTA;
    }

    state->cartas_marcadas[slot] = card;
    state->turnos_marcacao[slot] = duration;

    state->suspeita_acumulada += 5; 
    if (state->suspeita_acumulada > 100) state->suspeita_acumulada = 100;

    return TRAPACA_SUCESSO;
}

CheatResult cheat_bribe_dealer(CheatState *state, PokerTable *table, int seat, TipoSuborno tipo, PokerChips custo_base_mesa, int base_risk, PokerCard carta_desejada) {
    if (!state || !table || (seat != 0 && seat != 1)) return TRAPACA_DESCOBERTA;

    if (state->suborno_usado_na_mao) return TRAPACA_RECUSADA; 

    PokerChips custo = 0;
    switch(tipo) {
        case SUBORNO_CARTA_MESA: custo = custo_base_mesa; break;
        case SUBORNO_CARTA_MAO_FUTURA: custo = custo_base_mesa * 2; break;
        case SUBORNO_VER_CARTA_OPONENTE: custo = custo_base_mesa * 3; break;
    }

    if (table->player[seat].chips < custo) return TRAPACA_FUNDO_INSUFICIENTE;

    // Dedução antes do resultado do suborno
    table->player[seat].chips -= custo;
    state->suborno_usado_na_mao = true;

    int chance_recusa = 30; 
    int roll_recusa = (rand() % 100) + 1;
    if (roll_recusa <= chance_recusa) return TRAPACA_RECUSADA;

    int chance_descoberta = base_risk + state->suspeita_acumulada;
    int roll_descoberta = (rand() % 100) + 1;
    if (roll_descoberta <= chance_descoberta) {
        state->suspeita_acumulada = state->suspeita_acumulada / 2; 
        return TRAPACA_DESCOBERTA;
    }

    if (tipo == SUBORNO_CARTA_MESA) {
        state->carta_comprada = carta_desejada;
        state->destino_carta_comprada = 0;
        state->carta_comprada_pendente = true;
    } else if (tipo == SUBORNO_CARTA_MAO_FUTURA) {
        state->carta_comprada = carta_desejada;
        state->destino_carta_comprada = 1;
        state->carta_comprada_pendente = true;
    }

    state->suspeita_acumulada += 15;
    if (state->suspeita_acumulada > 100) state->suspeita_acumulada = 100;

    return TRAPACA_SUCESSO;
}

CheatResult cheat_swap_sleeve_card(CheatState *state, PokerCard *hand_card, int base_risk) {
    if (!state || !hand_card) return TRAPACA_DESCOBERTA;

    int chance_descoberta = base_risk + state->suspeita_acumulada;
    int roll = (rand() % 100) + 1;

    if (roll <= chance_descoberta) {
        state->suspeita_acumulada = state->suspeita_acumulada / 2; 
        return TRAPACA_DESCOBERTA;
    }

    PokerCard temp = *hand_card;
    *hand_card = state->carta_na_manga;
    state->carta_na_manga = temp;

    state->as_na_manga_usado = true;

    state->suspeita_acumulada += 20;
    if (state->suspeita_acumulada > 100) state->suspeita_acumulada = 100;

    return TRAPACA_SUCESSO;
}