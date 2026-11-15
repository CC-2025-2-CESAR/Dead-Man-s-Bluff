#ifndef TRAPACAS_H
#define TRAPACAS_H

#include <stdbool.h>
#include "../poker/poker.h"

// Estrutura que guarda o estado das trapaças do confronto atual
typedef struct {
    int suspeita_acumulada;     // De 0 a 100 (porcentagem)
    int balas_roleta;           // Persiste de 1 a 6
    
    // Estados temporários da mão/confronto
    int maos_embriagado;        // Quantas mãos o adversário ainda fica bêbado
    int usos_cumplice_blind;    // Contador para escalar o preço do cúmplice
    bool suborno_usado_na_mao;  
    bool as_na_manga_usado;     // Flag para desativar duplicatas e recriar o deck
    
    PokerCard carta_na_manga;   
    PokerCard cartas_marcadas[5];
    int turnos_marcacao[5];     
} CheatState;

// Resultados possíveis de uma tentativa de trapaça
typedef enum {
    TRAPACA_SUCESSO,
    TRAPACA_DESCOBERTA,
    TRAPACA_RECUSADA,      // Para quando o dealer não aceita suborno
    TRAPACA_FUNDO_INSUFICIENTE
} CheatResult;

// Função de inicialização
void cheat_init_state(CheatState *state);

// 1. Spike His Drink
CheatResult cheat_spike_drink(CheatState *state, int base_risk);

#endif