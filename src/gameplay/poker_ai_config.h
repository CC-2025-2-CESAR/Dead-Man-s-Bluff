#ifndef POKER_AI_CONFIG_H
#define POKER_AI_CONFIG_H

/*
 * Arquivo de TUNING da IA. Esta e a unica peca que um designer de jogo
 * deveria precisar editar para mudar o comportamento dos adversarios.
 *
 * O que fica aqui dentro:
 *   1. poker_ai_scenario_table  -- a matriz [PERFIL][STREET][FORCA][PRESSAO]
 *      com os 4 pesos {FOLD, PASSIVE, RAISE, RAISE_BIG} de cada cenario.
 *   2. poker_ai_pressure_thresholds -- onde comeca "aposta media", "aposta
 *      grande", etc, em fracao do pote.
 *   3. poker_ai_bet_ranges      -- faixas de tamanho de aposta (fracao do
 *      pote de referencia) por perfil e por tipo (normal/grande).
 *   4. poker_ai_equity_thresholds -- onde a equidade de Monte Carlo corta
 *      entre fraca/media/forte/premium no pos-flop.
 *   5. poker_ai_monte_carlo_iterations -- quantas simulacoes por decisao
 *      pos-flop.
 *   6. poker_ai_cheap_all_in_odds_max -- call muito barato com stack curto.
 *
 * NADA neste arquivo contem "if" de logica de jogo. So tipos e dados.
 * A logica que LE esses dados esta em poker_ai.c e nao deveria precisar
 * mudar so porque voce editou um numero aqui.
 */

#include "poker_ai.h"

/* ---------------------------------------------------------------------
 * Dimensoes da tabela.
 * --------------------------------------------------------------------- */

/* PokerAIStyle tem 3 valores (CAUTIOUS/BALANCED/AGGRESSIVE) -- ver poker_ai.h */
#define POKER_AI_PROFILE_COUNT 3

/* PokerStreet (poker.h) tem 4 valores: PREFLOP, FLOP, TURN, RIVER */
#define POKER_AI_STREET_COUNT 4

/* PokerAIHandStrength (poker_ai.h) tem 4 valores: WEAK..PREMIUM */
#define POKER_AI_STRENGTH_COUNT 4

/*
 * Pressao da aposta enfrentada, relativa ao POTE ANTES dessa aposta (nao
 * poker_pot(table) direto, que ja inclui o incremento do adversario que
 * ainda nao foi pago por nos). Ver poker_ai_classify_pressure().
 *
 *   FREE     -> legal.to_call == 0 (check gratuito; nao existe "pressao")
 *   SMALL    -> 0 < to_call/pote_antes <= small_max
 *   MEDIUM   -> small_max < razao <= medium_max
 *   LARGE    -> medium_max < razao <= large_max
 *   OVERBET  -> razao > large_max
 */
typedef enum {
    POKER_AI_PRESSURE_FREE = 0,
    POKER_AI_PRESSURE_SMALL,
    POKER_AI_PRESSURE_MEDIUM,
    POKER_AI_PRESSURE_LARGE,
    POKER_AI_PRESSURE_OVERBET
} PokerAIPressure;
#define POKER_AI_PRESSURE_COUNT 5

/* ---------------------------------------------------------------------
 * A matriz de cenarios.
 *
 * weight[POKER_AI_ACTION_FOLD], weight[POKER_AI_ACTION_PASSIVE],
 * weight[POKER_AI_ACTION_RAISE], weight[POKER_AI_ACTION_RAISE_BIG]
 * (enum em poker_ai.h). Cada celula entregue soma 100 e FREE exige
 * FOLD=0. Se uma acao nao for legal, o motor a remove e normaliza
 * SOMENTE as acoes restantes. Nao transferimos fold para check em FREE.
 * --------------------------------------------------------------------- */
typedef struct {
    double weight[POKER_AI_ACTION_SLOT_COUNT];
} PokerAIScenario;

/* Formato de tabela recebido por parametro no contexto da IA. */
typedef PokerAIScenario PokerAIScenarioTable
    [POKER_AI_PROFILE_COUNT][POKER_AI_STREET_COUNT]
    [POKER_AI_STRENGTH_COUNT][POKER_AI_PRESSURE_COUNT];

extern const PokerAIScenarioTable poker_ai_scenario_table;

/* O chamador escolhe o perfil, a tabela e mantem os RNGs entre turnos.
 * Em FLOP/TURN/RIVER, equity_rng e obrigatorio e independente do
 * action_rng; isso permite balancear os pesos sem mudar os sorteios
 * quando a quantidade de Monte Carlo for alterada. */
struct PokerAIContext {
    PokerAIStyle style;
    const PokerAIScenarioTable *table; /* NULL usa tabela padrao */
    PokerAIRng *action_rng;
    PokerAIRng *equity_rng;
};

/*
 * Classifica a pressao de uma aposta enfrentada, relativa ao pote ANTES
 * dela (ver comentario do enum PokerAIPressure acima). `pot_now` deve
 * ser poker_pot(table) no momento da decisao. Protegida contra
 * pote-antes <= 0 (classificada como OVERBET, a categoria de maior
 * pressao -- nao existe divisao por zero nem comportamento indefinido).
 */
PokerAIPressure poker_ai_classify_pressure(
    const PokerLegalActions *legal,
    PokerChips pot_now
);

/* ---------------------------------------------------------------------
 * Limiares de pressao (fracao do pote ANTES da aposta atual).
 * --------------------------------------------------------------------- */
typedef struct {
    double small_max;  /* ate aqui: SMALL */
    double medium_max; /* ate aqui: MEDIUM */
    double large_max;  /* ate aqui: LARGE; acima disso: OVERBET */
} PokerAIPressureThresholds;

extern const PokerAIPressureThresholds poker_ai_pressure_thresholds;

/* ---------------------------------------------------------------------
 * Faixas de tamanho de aposta, como fracao de um "pote de referencia"
 * (ver poker_ai.c: para BET/abertura e poker_pot(table); para RAISE e o
 * pote DEPOIS do call, poker_pot(table) + legal.to_call -- exatamente
 * como pedido: "valor extra acima do call como percentual do pote
 * depois do call"). Indexado por [perfil][tipo].
 * --------------------------------------------------------------------- */
typedef enum {
    POKER_AI_BET_NORMAL = 0,
    POKER_AI_BET_BIG
} PokerAIBetKind;
#define POKER_AI_BET_KIND_COUNT 2

typedef struct {
    double min_fraction;
    double max_fraction;
} PokerAIBetRange;

extern const PokerAIBetRange
    poker_ai_bet_ranges[POKER_AI_PROFILE_COUNT][POKER_AI_BET_KIND_COUNT];

/* ---------------------------------------------------------------------
 * Limiares de equidade (Monte Carlo) -> forca da mao, usados so no
 * pos-flop (pre-flop usa poker_ai_evaluate_preflop, que e heuristica de
 * range, nao equidade numerica).
 * --------------------------------------------------------------------- */
typedef struct {
    double weak_max;   /* equity <= isto: WEAK */
    double medium_max; /* equity <= isto: MEDIUM */
    double strong_max; /* equity <= isto: STRONG; acima: PREMIUM */
} PokerAIEquityThresholds;

extern const PokerAIEquityThresholds poker_ai_equity_thresholds;

/* ---------------------------------------------------------------------
 * Quantas iteracoes de Monte Carlo rodar por decisao pos-flop. Mais
 * iteracoes = equidade mais precisa, porem mais lento. 300-500 e um bom
 * equilibrio para uma decisao por turno de jogo; os testes estatisticos
 * usam a sua PROPRIA contagem (100000), que e sobre o sorteio ponderado
 * de acao, nao sobre isto.
 * --------------------------------------------------------------------- */
extern const int poker_ai_monte_carlo_iterations;

/* Excecao TRANSPARENTE de short stack: se call capado custar no maximo
 * esta fracao do pote efetivamente disputavel, transfere o peso FOLD
 * para CALL. No pos-flop so se a equity estimada >= pot odds.
 * Coloque 0.0 para desligar. Nao cria uma nova dimensao na tabela. */
extern const double poker_ai_cheap_all_in_odds_max;

#endif
