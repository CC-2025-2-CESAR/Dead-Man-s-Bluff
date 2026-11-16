#ifndef TRAPACAS_H
#define TRAPACAS_H

#include <stdbool.h>
#include "../poker/poker.h"

// Tipos de suborno possíveis
typedef enum {
    SUBORNO_CARTA_MESA = 0,
    SUBORNO_CARTA_MAO_FUTURA,
    SUBORNO_VER_CARTA_OPONENTE
} TipoSuborno;

// Estrutura que guarda o estado físico das trapaças na mesa
typedef struct {
    // Estados temporários da mão/confronto
    int maos_embriagado;
    int usos_cumplice_blind;
    bool suborno_usado_na_mao;  
    bool as_na_manga_usado;
    
    PokerCard carta_na_manga;   
    PokerCard cartas_marcadas[5];
    int turnos_marcacao[5];     
    
    // Propriedades do suborno
    bool carta_comprada_pendente; 
    PokerCard carta_comprada;
    int destino_carta_comprada; // 0 = mesa, 1 = mão
} CheatState;

// Resultados possíveis de uma tentativa de trapaça
typedef enum {
    TRAPACA_SUCESSO,
    TRAPACA_DESCOBERTA,
    TRAPACA_RECUSADA,
    TRAPACA_FUNDO_INSUFICIENTE
} CheatResult;

// Funções de inicialização e manutenção de estado
void cheat_init_state(CheatState *state);
void cheat_update_end_of_hand(CheatState *state);
bool is_card_marked(const CheatState *state, PokerCard card);

// Funções de trapaça 
CheatResult cheat_spike_drink(CheatState *state, bool foi_descoberto);
CheatResult cheat_signal_accomplice(CheatState *state, PokerTable *table, int seat, bool foi_descoberto);
CheatResult cheat_mark_card(CheatState *state, PokerCard card, int duration, bool foi_descoberto);
CheatResult cheat_bribe_dealer(CheatState *state, PokerTable *table, int seat, TipoSuborno tipo, PokerChips custo_base_mesa, bool dealer_recusou, bool foi_descoberto, PokerCard carta_desejada);
CheatResult cheat_swap_sleeve_card(CheatState *state, PokerCard *hand_card, bool foi_descoberto);

#endif