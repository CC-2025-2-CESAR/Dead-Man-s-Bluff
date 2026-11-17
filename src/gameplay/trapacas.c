#include "trapacas.h"

// Inicializa o estado físico da mesa
// Chamado no início de uma campanha ou mesa
void cheat_init_state(CheatState *state) {
    if (!state) return;
    
    state->maos_embriagado = 0;
    state->usos_cumplice_blind = 0;
    state->suborno_usado_na_mao = false;
    state->as_na_manga_usado = false;

    // A carta na manga persiste entre confrontos 
    // Se não houver carta salva, o jogador começa com um 2 de Paus 
    if (state->carta_na_manga.rank == 0) {
        state->carta_na_manga.rank = 2; 
        state->carta_na_manga.suit = POKER_CLUBS; 
    }

    // Inicializa a matriz de cartas marcadas (0 = sem marcação)
    for (int i = 0; i < 5; i++) {
        state->cartas_marcadas[i].rank = 0; 
        state->turnos_marcacao[i] = 0;
    }

    // Reseta flags de entregas de cartas pelo dealer corrupto
    state->carta_comprada_pendente = false;
    state->destino_carta_comprada = -1;
    state->carta_comprada.rank = 0;
}

// Função de manutenção chamada pelo Core no final de CADA rodada/mão
void cheat_update_end_of_hand(CheatState *state) {
    if (!state) return;

    // O efeito da bebida passa aos poucos a cada mão jogada
    if (state->maos_embriagado > 0) state->maos_embriagado--;
    
    // Libera o suborno para a próxima mão
    state->suborno_usado_na_mao = false;

    // Desgasta as marcações físicas nas cartas
    for (int i = 0; i < 5; i++) {
        if (state->turnos_marcacao[i] > 0) {
            state->turnos_marcacao[i]--;
            // Se a duração da tinta/marcação acabou, esvazia o slot
            if (state->turnos_marcacao[i] == 0) {
                state->cartas_marcadas[i].rank = 0; 
            }
        }
    }
}

// Utilitário para o motor de poker saber se a carta avaliada está marcada
bool is_card_marked(const CheatState *state, PokerCard card) {
    if (!state) return false;
    
    for (int i = 0; i < 5; i++) {
        // Confere valor e naipe com exatidão
        if (state->cartas_marcadas[i].rank == card.rank && 
            state->cartas_marcadas[i].suit == card.suit &&
            state->turnos_marcacao[i] > 0) {
            return true;
        }
    }
    return false;
}

// O Core decide se foi descoberto usando a probabilidade e informa aqui
CheatResult cheat_spike_drink(CheatState *state, bool foi_descoberto) {
    if (!state) return TRAPACA_DESCOBERTA;

    if (foi_descoberto) {
        return TRAPACA_DESCOBERTA; // Core cuidará da punição (Roleta Russa)
    }

    // Sucesso: Altera a mesa embriagando o adversário por 3 rodadas
    state->maos_embriagado = 3; 
    return TRAPACA_SUCESSO;
}

// Desconta o dinheiro silenciosamente se não for pego
CheatResult cheat_signal_accomplice(CheatState *state, PokerTable *table, int seat, bool foi_descoberto) {
    if (!state || !table || (seat != 0 && seat != 1)) return TRAPACA_DESCOBERTA;

    // O preço sobe baseado na quantidade de vezes que o jogador já abusou dessa trapaça na blind
    int multiplicador = state->usos_cumplice_blind + 1;
    PokerChips custo = table->big_blind * multiplicador;

    // Se o jogador estiver sem fundos, a trapaça é barrada ANTES da rolagem de risco
    if (table->player[seat].chips < custo) return TRAPACA_FUNDO_INSUFICIENTE;

    if (foi_descoberto) {
        return TRAPACA_DESCOBERTA;
    }

    // Sucesso: Deduz o valor das fichas fora do pote
    table->player[seat].chips -= custo;
    state->usos_cumplice_blind++;

    return TRAPACA_SUCESSO;
}

// Guarda a carta num dos 5 slots do baralho
CheatResult cheat_mark_card(CheatState *state, PokerCard card, int duration, bool foi_descoberto) {
    if (!state) return TRAPACA_DESCOBERTA;

    int slot = -1;
    for (int i = 0; i < 5; i++) {
        if (state->cartas_marcadas[i].rank == 0) {
            slot = i;
            break; // Achou vaga
        }
    }

    // Não permite marcar se os 5 slots estiverem ocupados
    if (slot == -1) return TRAPACA_RECUSADA; 

    if (foi_descoberto) {
        return TRAPACA_DESCOBERTA;
    }

    // Sucesso: Registra a carta e sua durabilidade 
    state->cartas_marcadas[slot] = card;
    state->turnos_marcacao[slot] = duration;
    
    return TRAPACA_SUCESSO;
}

// O Core joga os dados duas vezes: 1 para recusa do dealer, 1 para ser pego
CheatResult cheat_bribe_dealer(CheatState *state, PokerTable *table, int seat, TipoSuborno tipo, PokerChips custo_base_mesa, bool dealer_recusou, bool foi_descoberto, PokerCard carta_desejada) {
    if (!state || !table || (seat != 0 && seat != 1)) return TRAPACA_DESCOBERTA;

    // O sistema só aguenta processar um suborno pendente por mão
    if (state->suborno_usado_na_mao) return TRAPACA_RECUSADA; 

    PokerChips custo = 0;
    switch(tipo) {
        case SUBORNO_CARTA_MESA: custo = custo_base_mesa; break;
        case SUBORNO_CARTA_MAO_FUTURA: custo = custo_base_mesa * 2; break;
        case SUBORNO_VER_CARTA_OPONENTE: custo = custo_base_mesa * 3; break;
    }

    if (table->player[seat].chips < custo) return TRAPACA_FUNDO_INSUFICIENTE;

    // DEDUÇÃO IMEDIATA: O dealer embolsa as fichas antes mesmo de agir
    table->player[seat].chips -= custo;
    state->suborno_usado_na_mao = true;

    // Dealer guardou o dinheiro mas se acovardou e não fez o serviço
    if (dealer_recusou) return TRAPACA_RECUSADA;

    if (foi_descoberto) {
        return TRAPACA_DESCOBERTA;
    }

    // Sucesso: Agenda para o core do motor forçar essa carta no próximo sorteio
    if (tipo == SUBORNO_CARTA_MESA) {
        state->carta_comprada = carta_desejada;
        state->destino_carta_comprada = 0; // Mesa
        state->carta_comprada_pendente = true;
    } else if (tipo == SUBORNO_CARTA_MAO_FUTURA) {
        state->carta_comprada = carta_desejada;
        state->destino_carta_comprada = 1; // Mão do jogador
        state->carta_comprada_pendente = true;
    }

    return TRAPACA_SUCESSO;
}

// Troca física das cartas preservando atributos (como marcadores)
CheatResult cheat_swap_sleeve_card(CheatState *state, PokerCard *hand_card, bool foi_descoberto) {
    if (!state || !hand_card) return TRAPACA_DESCOBERTA;

    if (foi_descoberto) {
        return TRAPACA_DESCOBERTA;
    }

    // Sucesso: Realiza a troca (Swap) dos valores na memória
    PokerCard temp = *hand_card;
    *hand_card = state->carta_na_manga;
    state->carta_na_manga = temp;

    // O Core usará essa flag para avaliar se haverá duplicação de cartas
    state->as_na_manga_usado = true;
    
    return TRAPACA_SUCESSO;
}