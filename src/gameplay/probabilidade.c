#include "probabilidade.h"
#include <stdlib.h> 

// ------------------------------------------------------------------
// Configurações de Balanceamento (Fáceis de alterar para testes)
// ------------------------------------------------------------------

// Chance base inicial de ser pego em cada trapaça (em %)
static const int CHANCE_BASE[PROB_TRAPACA_TOTAL] = {
    10, // SPIKE_DRINK: Baixo risco
    15, // CUMPLICE: Risco moderado
    5,  // MARCAR_CARTA: Muito baixo risco inicial, mas exige marcar várias
    20, // SUBORNO_DEALER: Alto risco, envolve envolver terceiros
    25  // CARTA_MANGA: Risco altíssimo, manipulação física de cartas em jogo
};

// Quanto a chance base aumenta a CADA USO repetido na mesma mesa (em %)
static const int INCREMENTO_POR_USO[PROB_TRAPACA_TOTAL] = {
    10, // SPIKE_DRINK 
    10, // CUMPLICE
    2.75,  // MARCAR_CARTA 
    10, // SUBORNO_DEALER
    5  // CARTA_MANGA
};

// ------------------------------------------------------------------
// Implementação
// ------------------------------------------------------------------

void prob_inicializar(SistemaRisco *risco) {
    if (!risco) return;
    
    risco->balas_roleta = 1; // Começa sempre com 1 bala
    prob_resetar_usos(risco);
}

void prob_avancar_mesa(SistemaRisco *risco) {
    if (!risco) return;
    
    // Diminui 1 bala como recompensa por vencer, mantendo o mínimo de 1
    if (risco->balas_roleta > 1) {
        risco->balas_roleta--;
    }
    
    // Zera a suspeita/usos das trapaças para o novo adversário
    prob_resetar_usos(risco);
}

void prob_resetar_usos(SistemaRisco *risco) {
    if (!risco) return;
    
    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        risco->usos_trapaca[i] = 0;
    }
}

void prob_registrar_uso_trapaca(SistemaRisco *risco, TipoTrapacaProb tipo) {
    if (!risco || tipo < 0 || tipo >= PROB_TRAPACA_TOTAL) return;
    
    risco->usos_trapaca[tipo]++;
}

int prob_calcular_chance_descoberta(const SistemaRisco *risco, TipoTrapacaProb tipo, ProbPerfilIA perfil) {
    if (!risco || tipo < 0 || tipo >= PROB_TRAPACA_TOTAL) return 100;
    
    // 1. Calcula a chance acumulada (Base + (Usos * Incremento))
    int chance_acumulada = CHANCE_BASE[tipo] + (risco->usos_trapaca[tipo] * INCREMENTO_POR_USO[tipo]);
    
    // 2. Define o multiplicador da IA
    double multiplicador = 1.0;
    switch (perfil) {
        case PROB_IA_AGRESSIVO:
            multiplicador = 1.2;
            break;
        case PROB_IA_MEDROSO:
            multiplicador = 1.5;
            break;
        case PROB_IA_DESCONFIADO:
            multiplicador = 2.5;
            break;
    }
    
    // 3. Aplica o multiplicador
    int chance_final = (int)(chance_acumulada * multiplicador);
    
    // Garante que não ultrapasse 100%
    if (chance_final > 100) {
        chance_final = 100;
    }
    
    return chance_final;
}

bool prob_jogar_roleta_russa(SistemaRisco *risco) {
    if (!risco) return false;
    
    // O tambor do revólver tem 6 câmaras. Sorteia um número de 1 a 6.
    int camara_sorteada = (rand() % 6) + 1;
    
    // Se a câmara sorteada for menor ou igual ao número de balas, a arma dispara.
    if (camara_sorteada <= risco->balas_roleta) {
        return true; // Morreu
    }
    
    // Se sobreviveu:
    // 1. Adiciona uma bala (limite de 6, o que significa morte certa na próxima)
    if (risco->balas_roleta < 6) {
        risco->balas_roleta++;
    }
    
    // 2. Reseta a desconfiança da mesa (probabilidades voltam ao padrão inicial)
    prob_resetar_usos(risco);
    
    return false; // Sobreviveu
}