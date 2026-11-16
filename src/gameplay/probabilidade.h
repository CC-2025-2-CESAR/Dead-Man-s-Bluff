#ifndef PROBABILIDADE_H
#define PROBABILIDADE_H

#include <stdbool.h>

// Enumeração para identificar qual trapaça está sendo calculada
typedef enum {
    PROB_TRAPACA_SPIKE_DRINK = 0,
    PROB_TRAPACA_CUMPLICE,
    PROB_TRAPACA_MARCAR_CARTA,
    PROB_TRAPACA_SUBORNO_DEALER,
    PROB_TRAPACA_CARTA_MANGA,
    PROB_TRAPACA_TOTAL // Guarda a quantidade total de trapaças
} TipoTrapacaProb;

// Personalidades da IA para aplicar o multiplicador de desconfiança
typedef enum {
    PROB_IA_AGRESSIVO = 0,  // Multiplicador: 1.2x
    PROB_IA_MEDROSO,        // Multiplicador: 1.5x
    PROB_IA_DESCONFIADO     // Multiplicador: 2.5x
} ProbPerfilIA;

// Estrutura que mantém o estado de risco e punições do jogador
typedef struct {
    int balas_roleta;                           // De 1 a 6
    int usos_trapaca[PROB_TRAPACA_TOTAL];       // Quantas vezes cada trapaça foi usada na mesa atual
} SistemaRisco;

// ------------------------------------------------------------------
// Funções de Inicialização e Manutenção
// ------------------------------------------------------------------

// Inicia o sistema para um novo jogo (1 bala, zera usos)
void prob_inicializar(SistemaRisco *risco);

// Chamada quando o jogador avança de mesa/boss. 
// Zera os usos das trapaças e remove 1 bala (mínimo de 1).
void prob_avancar_mesa(SistemaRisco *risco);

// Zera apenas o contador de usos das trapaças na mesa atual.
// É chamada automaticamente ao sobreviver à roleta russa.
void prob_resetar_usos(SistemaRisco *risco);

// ------------------------------------------------------------------
// Funções de Cálculo e Execução
// ------------------------------------------------------------------

// Registra que uma trapaça foi usada com sucesso, aumentando sua chance base futura
void prob_registrar_uso_trapaca(SistemaRisco *risco, TipoTrapacaProb tipo);

// Calcula a porcentagem final de chance do jogador ser descoberto (0 a 100)
// Leva em conta a chance base, os usos repetidos e a personalidade do oponente
int prob_calcular_chance_descoberta(const SistemaRisco *risco, TipoTrapacaProb tipo, ProbPerfilIA perfil);

// Roda o minigame mortal da roleta russa.
// Retorna 'true' se o jogador MORREU, ou 'false' se SOBREVIVEU.
// Caso sobreviva, adiciona 1 bala na câmara e reseta o uso das trapaças automaticamente.
bool prob_jogar_roleta_russa(SistemaRisco *risco);

#endif