#ifndef TRAPACAS_H
#define TRAPACAS_H

#include <stdbool.h>
#include "../poker/poker.h"

// Tipos de Suborno
typedef enum {
    SUBORNO_CARTA_MESA = 0,      // Custo: 1x (Mais barato)
    SUBORNO_CARTA_MAO_FUTURA,    // Custo: 2x (Médio)
    SUBORNO_VER_CARTA_OPONENTE   // Custo: 3x (Mais caro)
} TipoSuborno;

// Resultados possíveis de uma tentativa de trapaça
typedef enum {
    TRAPACA_SUCESSO,
    TRAPACA_DESCOBERTA,
    TRAPACA_RECUSADA,      // Para quando o dealer não aceita suborno
    TRAPACA_FUNDO_INSUFICIENTE
} CheatResult;

// Estrutura que guarda o estado das trapaças do confronto atual
typedef struct {
    int suspeita_acumulada;     // De 0 a 100 (porcentagem)
    int balas_roleta;           // Persiste de 1 a 6 durante o torneio

    // Estados temporários da mão/confronto
    int maos_embriagado;        // Quantas mãos o adversário ainda fica bêbado
    int usos_cumplice_blind;    // Contador para escalar o preço do cúmplice
    bool suborno_usado_na_mao;  
    bool as_na_manga_usado;     // Flag para alertar duplicatas e contaminação

    PokerCard carta_na_manga;   
    PokerCard cartas_marcadas[5];
    int turnos_marcacao[5];     

    // Controle de compra de cartas (Suborno)
    bool carta_comprada_pendente; 
    PokerCard carta_comprada;
    int destino_carta_comprada; // 0 = mesa, 1 = mão do jogador
} CheatState;

// ==============================================================================
// FUNÇÕES DE GERENCIAMENTO
// ==============================================================================

// Inicializa ou reseta o estado (cuidado para preservar balas_roleta e carta_na_manga entre mesas)
void cheat_init_state(CheatState *state);

// Manutenção de estado: deve ser chamada no final de cada mão (POKER_SETTLED)
void cheat_update_end_of_hand(CheatState *state);

// Verifica se uma carta está marcada fisicamente
bool is_card_marked(const CheatState *state, PokerCard card);

// ==============================================================================
// FUNÇÕES DAS 5 TRAPAÇAS
// ==============================================================================

// 1. Spike His Drink
CheatResult cheat_spike_drink(CheatState *state, int base_risk);

// 2. Sinal do Cúmplice (Accomplice Signal)
CheatResult cheat_signal_accomplice(CheatState *state, PokerTable *table, int seat, int base_risk);

// 3. Marcar Cartas (Card Marking)
CheatResult cheat_mark_card(CheatState *state, PokerCard card, int base_risk, int duration);

// 4. Suborno ao Dealer
CheatResult cheat_bribe_dealer(CheatState *state, PokerTable *table, int seat, TipoSuborno tipo, PokerChips custo_base_mesa, int base_risk, PokerCard carta_desejada);

// 5. Carta na Manga (Ace Up the Sleeve)
CheatResult cheat_swap_sleeve_card(CheatState *state, PokerCard *hand_card, int base_risk);

#endif