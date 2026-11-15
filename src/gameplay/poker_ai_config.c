#include "poker_ai_config.h"

/* ===================================================================
 * Limiares de pressao da aposta (fracao do pote ANTES da aposta atual).
 *
 * small_max  = 0.35  -> ate 35% do pote-antes: PEQUENA
 * medium_max = 0.80  -> de 35% a 80% do pote-antes: MEDIA
 * large_max  = 1.50  -> de 80% a 150% do pote-antes: GRANDE
 *                        acima de 150%: OVERBET
 *
 * Esses tres numeros sao os UNICOS lugares onde "pequena/media/grande"
 * sao definidos. Mudar o comportamento de quando a IA considera uma
 * aposta "grande" e so editar large_max abaixo -- nenhuma outra parte
 * do codigo precisa mudar.
 */
const PokerAIPressureThresholds poker_ai_pressure_thresholds = {
    .small_max = 0.35,
    .medium_max = 0.80,
    .large_max = 1.50
};

/* ===================================================================
 * Faixas de tamanho de aposta (fracao do pote de referencia).
 *
 * Para BET (abertura, current_bet == 0): fracao aplicada sobre
 * poker_pot(table).
 * Para RAISE (current_bet > 0): fracao aplicada sobre o pote DEPOIS do
 * call (poker_pot(table) + legal.to_call), e o resultado e somado ao
 * valor necessario para igualar a aposta (current_bet) -- e exatamente
 * o "valor extra acima do call como percentual do pote depois do call"
 * pedido na especificacao. O total final e sempre travado em
 * [legal.min_total, legal.max_total] por poker_ai.c, entao nunca gera
 * uma PokerAction ilegal mesmo se a fracao sorteada for extrema.
 *
 * Valores iniciais (hipoteticos, para ajustar depois):
 *   - Agressivo aposta faixas maiores que Medroso/Desconfiado.
 *   - "BIG" e sempre maior que "NORMAL" para o mesmo perfil.
 * =================================================================== */
const PokerAIBetRange
    poker_ai_bet_ranges[POKER_AI_PROFILE_COUNT][POKER_AI_BET_KIND_COUNT] = {
    /* POKER_AI_CAUTIOUS */
    {
        /* NORMAL: 30% a 55% do pote de referencia */
        { .min_fraction = 0.30, .max_fraction = 0.55 },
        /* BIG: 70% a 110% do pote de referencia */
        { .min_fraction = 0.70, .max_fraction = 1.10 }
    },
    /* POKER_AI_BALANCED */
    {
        { .min_fraction = 0.35, .max_fraction = 0.75 },
        { .min_fraction = 1.00, .max_fraction = 1.40 }
    },
    /* POKER_AI_AGGRESSIVE */
    {
        { .min_fraction = 0.45, .max_fraction = 0.90 },
        { .min_fraction = 1.20, .max_fraction = 1.80 }
    }
};

/* ===================================================================
 * Limiares de equidade (Monte Carlo) -> forca da mao, usados so no
 * pos-flop. No heads-up, uma equidade de 50% contra uma mao aleatoria
 * ja e "razoavelmente boa" (o baseline de duas maos aleatorias e 50%
 * cada); por isso os cortes aqui NAO sao os mesmos que se usaria numa
 * mesa de 9 jogadores.
 * =================================================================== */
const PokerAIEquityThresholds poker_ai_equity_thresholds = {
    .weak_max = 0.35,   /* equity <= 35%: WEAK */
    .medium_max = 0.55, /* 35% a 55%: MEDIUM */
    .strong_max = 0.75  /* 55% a 75%: STRONG; acima de 75%: PREMIUM */
};

/* Iteracoes de Monte Carlo por decisao pos-flop (nao confundir com as
 * 100000 repeticoes do teste estatistico do sorteio ponderado). */
const int poker_ai_monte_carlo_iterations = 400;

/* Situacao excepcional: all-in capado extremamente barato (ex.: 5 BB
 * para disputar cerca de 200 BB). Edite 0.03 para ajustar ou 0 para
 * desligar. O override fica visivel em PokerAIDecisionInfo. */
const double poker_ai_cheap_all_in_odds_max = 0.03;

/* BIG vira shove se stack/pote <= 1.0; zero desliga conversao por SPR. */
const double poker_ai_offensive_all_in_spr_max = 1.0;

/* ===================================================================
 * Tabela de cenarios da IA -- VALORES INICIAIS EXPERIMENTAIS.
 *
 * Gerados uma unica vez por um script auxiliar (gen_table.py, fora do
 * repositorio) para povoar as 240 celulas com numeros plausiveis e
 * ja diferenciados por perfil/pressao/forca. Cenarios FREE revisados
 * manualmente para FOLD=0 e probabilidades EXATAS de CHECK/BET/BIG.
 * Dali em diante, SAO SO
 * NUMEROS: editar qualquer linha abaixo e trocar os quatro inteiros
 * e suficiente para mudar o comportamento daquele cenario. NAO HA
 * logica, multiplicador ou pos-processamento escondido em cima deles.
 *
 * Ordem fixa dos 4 pesos em cada celula (ver PokerAIActionSlot):
 *   { FOLD, PASSIVE(check/call), RAISE, RAISE_BIG }
 * Cada linha soma 100. Nas celulas FREE: {0, CHECK, BET, BIG_BET}.
 * Nas demais: {FOLD, CALL, RAISE, RAISE_BIG}.
 * A probabilidade nominal e exata se todas as acoes positivas forem
 * legalmente disponiveis; caso contrario, so as acoes legais competem.
 * Cada linha soma 100 (unidade arbitraria -- o motor normaliza pelo
 * total dos pesos ainda legais, entao a soma exata de 100 nao e uma
 * obrigacao do motor, e sim uma convencao para estes dados ficarem
 * faceis de ler como percentual).
 * =================================================================== */
const PokerAIScenario
    poker_ai_scenario_table[POKER_AI_PROFILE_COUNT][POKER_AI_STREET_COUNT]
                            [POKER_AI_STRENGTH_COUNT][POKER_AI_PRESSURE_COUNT] = {
    /* ===== PERFIL: POKER_AI_CAUTIOUS ===== */
    {
        /* -- street: POKER_PREFLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* CAUTIOUS, PREFLOP, WEAK, FREE */
                { { 0, 93, 6, 1 } },
                /* CAUTIOUS, PREFLOP, WEAK, SMALL */
                { { 41, 54, 4, 1 } },
                /* CAUTIOUS, PREFLOP, WEAK, MEDIUM */
                { { 75, 21, 3, 1 } },
                /* CAUTIOUS, PREFLOP, WEAK, LARGE */
                { { 92, 5, 2, 1 } },
                /* CAUTIOUS, PREFLOP, WEAK, OVERBET */
                { { 93, 6, 1, 0 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* CAUTIOUS, PREFLOP, MEDIUM, FREE */
                { { 0, 72, 23, 5 } },
                /* CAUTIOUS, PREFLOP, MEDIUM, SMALL */
                { { 16, 71, 10, 3 } },
                /* CAUTIOUS, PREFLOP, MEDIUM, MEDIUM */
                { { 30, 58, 9, 3 } },
                /* CAUTIOUS, PREFLOP, MEDIUM, LARGE */
                { { 46, 47, 5, 2 } },
                /* CAUTIOUS, PREFLOP, MEDIUM, OVERBET */
                { { 62, 34, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* CAUTIOUS, PREFLOP, STRONG, FREE */
                { { 0, 37, 54, 9 } },
                /* CAUTIOUS, PREFLOP, STRONG, SMALL */
                { { 4, 63, 24, 9 } },
                /* CAUTIOUS, PREFLOP, STRONG, MEDIUM */
                { { 8, 63, 21, 8 } },
                /* CAUTIOUS, PREFLOP, STRONG, LARGE */
                { { 13, 70, 12, 5 } },
                /* CAUTIOUS, PREFLOP, STRONG, OVERBET */
                { { 17, 73, 7, 3 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* CAUTIOUS, PREFLOP, PREMIUM, FREE */
                { { 0, 20, 60, 20 } },
                /* CAUTIOUS, PREFLOP, PREMIUM, SMALL */
                { { 1, 49, 27, 23 } },
                /* CAUTIOUS, PREFLOP, PREMIUM, MEDIUM */
                { { 3, 54, 23, 20 } },
                /* CAUTIOUS, PREFLOP, PREMIUM, LARGE */
                { { 4, 70, 14, 12 } },
                /* CAUTIOUS, PREFLOP, PREMIUM, OVERBET */
                { { 6, 79, 8, 7 } },
            },
        },
        /* -- street: POKER_FLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* CAUTIOUS, FLOP, WEAK, FREE */
                { { 0, 90, 8, 2 } },
                /* CAUTIOUS, FLOP, WEAK, SMALL */
                { { 41, 54, 4, 1 } },
                /* CAUTIOUS, FLOP, WEAK, MEDIUM */
                { { 75, 21, 3, 1 } },
                /* CAUTIOUS, FLOP, WEAK, LARGE */
                { { 92, 5, 2, 1 } },
                /* CAUTIOUS, FLOP, WEAK, OVERBET */
                { { 93, 6, 1, 0 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* CAUTIOUS, FLOP, MEDIUM, FREE */
                { { 0, 75, 20, 5 } },
                /* CAUTIOUS, FLOP, MEDIUM, SMALL */
                { { 16, 72, 9, 3 } },
                /* CAUTIOUS, FLOP, MEDIUM, MEDIUM */
                { { 30, 59, 8, 3 } },
                /* CAUTIOUS, FLOP, MEDIUM, LARGE */
                { { 46, 47, 5, 2 } },
                /* CAUTIOUS, FLOP, MEDIUM, OVERBET */
                { { 62, 34, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* CAUTIOUS, FLOP, STRONG, FREE */
                { { 0, 33, 56, 11 } },
                /* CAUTIOUS, FLOP, STRONG, SMALL */
                { { 4, 64, 23, 9 } },
                /* CAUTIOUS, FLOP, STRONG, MEDIUM */
                { { 8, 64, 20, 8 } },
                /* CAUTIOUS, FLOP, STRONG, LARGE */
                { { 13, 70, 12, 5 } },
                /* CAUTIOUS, FLOP, STRONG, OVERBET */
                { { 17, 73, 7, 3 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* CAUTIOUS, FLOP, PREMIUM, FREE */
                { { 0, 23, 58, 19 } },
                /* CAUTIOUS, FLOP, PREMIUM, SMALL */
                { { 1, 52, 25, 22 } },
                /* CAUTIOUS, FLOP, PREMIUM, MEDIUM */
                { { 3, 56, 22, 19 } },
                /* CAUTIOUS, FLOP, PREMIUM, LARGE */
                { { 4, 71, 13, 12 } },
                /* CAUTIOUS, FLOP, PREMIUM, OVERBET */
                { { 6, 79, 8, 7 } },
            },
        },
        /* -- street: POKER_TURN -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* CAUTIOUS, TURN, WEAK, FREE */
                { { 0, 91, 7, 2 } },
                /* CAUTIOUS, TURN, WEAK, SMALL */
                { { 41, 54, 4, 1 } },
                /* CAUTIOUS, TURN, WEAK, MEDIUM */
                { { 74, 22, 3, 1 } },
                /* CAUTIOUS, TURN, WEAK, LARGE */
                { { 92, 5, 2, 1 } },
                /* CAUTIOUS, TURN, WEAK, OVERBET */
                { { 92, 7, 1, 0 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* CAUTIOUS, TURN, MEDIUM, FREE */
                { { 0, 77, 18, 5 } },
                /* CAUTIOUS, TURN, MEDIUM, SMALL */
                { { 16, 72, 9, 3 } },
                /* CAUTIOUS, TURN, MEDIUM, MEDIUM */
                { { 30, 59, 8, 3 } },
                /* CAUTIOUS, TURN, MEDIUM, LARGE */
                { { 46, 47, 5, 2 } },
                /* CAUTIOUS, TURN, MEDIUM, OVERBET */
                { { 62, 34, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* CAUTIOUS, TURN, STRONG, FREE */
                { { 0, 36, 54, 10 } },
                /* CAUTIOUS, TURN, STRONG, SMALL */
                { { 4, 66, 22, 8 } },
                /* CAUTIOUS, TURN, STRONG, MEDIUM */
                { { 8, 66, 19, 7 } },
                /* CAUTIOUS, TURN, STRONG, LARGE */
                { { 13, 72, 11, 4 } },
                /* CAUTIOUS, TURN, STRONG, OVERBET */
                { { 17, 73, 7, 3 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* CAUTIOUS, TURN, PREMIUM, FREE */
                { { 0, 27, 56, 17 } },
                /* CAUTIOUS, TURN, PREMIUM, SMALL */
                { { 1, 54, 24, 21 } },
                /* CAUTIOUS, TURN, PREMIUM, MEDIUM */
                { { 3, 58, 21, 18 } },
                /* CAUTIOUS, TURN, PREMIUM, LARGE */
                { { 4, 72, 13, 11 } },
                /* CAUTIOUS, TURN, PREMIUM, OVERBET */
                { { 6, 81, 7, 6 } },
            },
        },
        /* -- street: POKER_RIVER -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* CAUTIOUS, RIVER, WEAK, FREE */
                { { 0, 94, 5, 1 } },
                /* CAUTIOUS, RIVER, WEAK, SMALL */
                { { 41, 55, 3, 1 } },
                /* CAUTIOUS, RIVER, WEAK, MEDIUM */
                { { 74, 22, 3, 1 } },
                /* CAUTIOUS, RIVER, WEAK, LARGE */
                { { 91, 6, 2, 1 } },
                /* CAUTIOUS, RIVER, WEAK, OVERBET */
                { { 92, 7, 1, 0 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* CAUTIOUS, RIVER, MEDIUM, FREE */
                { { 0, 79, 17, 4 } },
                /* CAUTIOUS, RIVER, MEDIUM, SMALL */
                { { 16, 72, 9, 3 } },
                /* CAUTIOUS, RIVER, MEDIUM, MEDIUM */
                { { 30, 61, 7, 2 } },
                /* CAUTIOUS, RIVER, MEDIUM, LARGE */
                { { 46, 49, 4, 1 } },
                /* CAUTIOUS, RIVER, MEDIUM, OVERBET */
                { { 62, 34, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* CAUTIOUS, RIVER, STRONG, FREE */
                { { 0, 39, 52, 9 } },
                /* CAUTIOUS, RIVER, STRONG, SMALL */
                { { 4, 68, 20, 8 } },
                /* CAUTIOUS, RIVER, STRONG, MEDIUM */
                { { 8, 67, 18, 7 } },
                /* CAUTIOUS, RIVER, STRONG, LARGE */
                { { 13, 72, 11, 4 } },
                /* CAUTIOUS, RIVER, STRONG, OVERBET */
                { { 17, 75, 6, 2 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* CAUTIOUS, RIVER, PREMIUM, FREE */
                { { 0, 30, 53, 17 } },
                /* CAUTIOUS, RIVER, PREMIUM, SMALL */
                { { 1, 56, 23, 20 } },
                /* CAUTIOUS, RIVER, PREMIUM, MEDIUM */
                { { 3, 60, 20, 17 } },
                /* CAUTIOUS, RIVER, PREMIUM, LARGE */
                { { 4, 74, 12, 10 } },
                /* CAUTIOUS, RIVER, PREMIUM, OVERBET */
                { { 6, 81, 7, 6 } },
            },
        },
    },
    /* ===== PERFIL: POKER_AI_BALANCED ===== */
    {
        /* -- street: POKER_PREFLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* BALANCED, PREFLOP, WEAK, FREE */
                { { 0, 79, 17, 4 } },
                /* BALANCED, PREFLOP, WEAK, SMALL */
                { { 30, 61, 7, 2 } },
                /* BALANCED, PREFLOP, WEAK, MEDIUM */
                { { 55, 37, 6, 2 } },
                /* BALANCED, PREFLOP, WEAK, LARGE */
                { { 85, 10, 4, 1 } },
                /* BALANCED, PREFLOP, WEAK, OVERBET */
                { { 92, 5, 2, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* BALANCED, PREFLOP, MEDIUM, FREE */
                { { 0, 53, 37, 10 } },
                /* BALANCED, PREFLOP, MEDIUM, SMALL */
                { { 12, 64, 18, 6 } },
                /* BALANCED, PREFLOP, MEDIUM, MEDIUM */
                { { 22, 57, 16, 5 } },
                /* BALANCED, PREFLOP, MEDIUM, LARGE */
                { { 34, 54, 9, 3 } },
                /* BALANCED, PREFLOP, MEDIUM, OVERBET */
                { { 46, 46, 6, 2 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* BALANCED, PREFLOP, STRONG, FREE */
                { { 0, 21, 60, 19 } },
                /* BALANCED, PREFLOP, STRONG, SMALL */
                { { 3, 36, 44, 17 } },
                /* BALANCED, PREFLOP, STRONG, MEDIUM */
                { { 6, 41, 38, 15 } },
                /* BALANCED, PREFLOP, STRONG, LARGE */
                { { 9, 59, 23, 9 } },
                /* BALANCED, PREFLOP, STRONG, OVERBET */
                { { 13, 69, 13, 5 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* BALANCED, PREFLOP, PREMIUM, FREE */
                { { 0, 10, 54, 36 } },
                /* BALANCED, PREFLOP, PREMIUM, SMALL */
                { { 1, 8, 49, 42 } },
                /* BALANCED, PREFLOP, PREMIUM, MEDIUM */
                { { 2, 19, 42, 37 } },
                /* BALANCED, PREFLOP, PREMIUM, LARGE */
                { { 3, 50, 25, 22 } },
                /* BALANCED, PREFLOP, PREMIUM, OVERBET */
                { { 4, 68, 15, 13 } },
            },
        },
        /* -- street: POKER_FLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* BALANCED, FLOP, WEAK, FREE */
                { { 0, 77, 19, 4 } },
                /* BALANCED, FLOP, WEAK, SMALL */
                { { 30, 61, 7, 2 } },
                /* BALANCED, FLOP, WEAK, MEDIUM */
                { { 55, 37, 6, 2 } },
                /* BALANCED, FLOP, WEAK, LARGE */
                { { 85, 10, 4, 1 } },
                /* BALANCED, FLOP, WEAK, OVERBET */
                { { 92, 5, 2, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* BALANCED, FLOP, MEDIUM, FREE */
                { { 0, 55, 35, 10 } },
                /* BALANCED, FLOP, MEDIUM, SMALL */
                { { 12, 65, 17, 6 } },
                /* BALANCED, FLOP, MEDIUM, MEDIUM */
                { { 22, 58, 15, 5 } },
                /* BALANCED, FLOP, MEDIUM, LARGE */
                { { 34, 54, 9, 3 } },
                /* BALANCED, FLOP, MEDIUM, OVERBET */
                { { 46, 47, 5, 2 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* BALANCED, FLOP, STRONG, FREE */
                { { 0, 24, 58, 18 } },
                /* BALANCED, FLOP, STRONG, SMALL */
                { { 3, 39, 42, 16 } },
                /* BALANCED, FLOP, STRONG, MEDIUM */
                { { 6, 44, 36, 14 } },
                /* BALANCED, FLOP, STRONG, LARGE */
                { { 9, 61, 22, 8 } },
                /* BALANCED, FLOP, STRONG, OVERBET */
                { { 13, 69, 13, 5 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* BALANCED, FLOP, PREMIUM, FREE */
                { { 0, 12, 53, 35 } },
                /* BALANCED, FLOP, PREMIUM, SMALL */
                { { 1, 13, 46, 40 } },
                /* BALANCED, FLOP, PREMIUM, MEDIUM */
                { { 2, 23, 40, 35 } },
                /* BALANCED, FLOP, PREMIUM, LARGE */
                { { 3, 52, 24, 21 } },
                /* BALANCED, FLOP, PREMIUM, OVERBET */
                { { 4, 70, 14, 12 } },
            },
        },
        /* -- street: POKER_TURN -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* BALANCED, TURN, WEAK, FREE */
                { { 0, 80, 16, 4 } },
                /* BALANCED, TURN, WEAK, SMALL */
                { { 30, 61, 7, 2 } },
                /* BALANCED, TURN, WEAK, MEDIUM */
                { { 55, 37, 6, 2 } },
                /* BALANCED, TURN, WEAK, LARGE */
                { { 86, 10, 3, 1 } },
                /* BALANCED, TURN, WEAK, OVERBET */
                { { 92, 5, 2, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* BALANCED, TURN, MEDIUM, FREE */
                { { 0, 59, 32, 9 } },
                /* BALANCED, TURN, MEDIUM, SMALL */
                { { 12, 67, 16, 5 } },
                /* BALANCED, TURN, MEDIUM, MEDIUM */
                { { 22, 59, 14, 5 } },
                /* BALANCED, TURN, MEDIUM, LARGE */
                { { 34, 54, 9, 3 } },
                /* BALANCED, TURN, MEDIUM, OVERBET */
                { { 46, 47, 5, 2 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* BALANCED, TURN, STRONG, FREE */
                { { 0, 27, 56, 17 } },
                /* BALANCED, TURN, STRONG, SMALL */
                { { 3, 43, 39, 15 } },
                /* BALANCED, TURN, STRONG, MEDIUM */
                { { 6, 47, 34, 13 } },
                /* BALANCED, TURN, STRONG, LARGE */
                { { 9, 62, 21, 8 } },
                /* BALANCED, TURN, STRONG, OVERBET */
                { { 13, 70, 12, 5 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* BALANCED, TURN, PREMIUM, FREE */
                { { 0, 14, 53, 33 } },
                /* BALANCED, TURN, PREMIUM, SMALL */
                { { 1, 17, 44, 38 } },
                /* BALANCED, TURN, PREMIUM, MEDIUM */
                { { 2, 27, 38, 33 } },
                /* BALANCED, TURN, PREMIUM, LARGE */
                { { 3, 54, 23, 20 } },
                /* BALANCED, TURN, PREMIUM, OVERBET */
                { { 4, 71, 13, 12 } },
            },
        },
        /* -- street: POKER_RIVER -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* BALANCED, RIVER, WEAK, FREE */
                { { 0, 84, 13, 3 } },
                /* BALANCED, RIVER, WEAK, SMALL */
                { { 30, 62, 6, 2 } },
                /* BALANCED, RIVER, WEAK, MEDIUM */
                { { 55, 38, 5, 2 } },
                /* BALANCED, RIVER, WEAK, LARGE */
                { { 86, 10, 3, 1 } },
                /* BALANCED, RIVER, WEAK, OVERBET */
                { { 92, 5, 2, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* BALANCED, RIVER, MEDIUM, FREE */
                { { 0, 62, 30, 8 } },
                /* BALANCED, RIVER, MEDIUM, SMALL */
                { { 12, 67, 16, 5 } },
                /* BALANCED, RIVER, MEDIUM, MEDIUM */
                { { 22, 60, 14, 4 } },
                /* BALANCED, RIVER, MEDIUM, LARGE */
                { { 34, 55, 8, 3 } },
                /* BALANCED, RIVER, MEDIUM, OVERBET */
                { { 46, 47, 5, 2 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* BALANCED, RIVER, STRONG, FREE */
                { { 0, 30, 54, 16 } },
                /* BALANCED, RIVER, STRONG, SMALL */
                { { 3, 46, 37, 14 } },
                /* BALANCED, RIVER, STRONG, MEDIUM */
                { { 6, 49, 32, 13 } },
                /* BALANCED, RIVER, STRONG, LARGE */
                { { 9, 64, 19, 8 } },
                /* BALANCED, RIVER, STRONG, OVERBET */
                { { 13, 72, 11, 4 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* BALANCED, RIVER, PREMIUM, FREE */
                { { 0, 16, 52, 32 } },
                /* BALANCED, RIVER, PREMIUM, SMALL */
                { { 1, 21, 42, 36 } },
                /* BALANCED, RIVER, PREMIUM, MEDIUM */
                { { 2, 30, 36, 32 } },
                /* BALANCED, RIVER, PREMIUM, LARGE */
                { { 3, 56, 22, 19 } },
                /* BALANCED, RIVER, PREMIUM, OVERBET */
                { { 4, 72, 13, 11 } },
            },
        },
    },
    /* ===== PERFIL: POKER_AI_AGGRESSIVE ===== */
    {
        /* -- street: POKER_PREFLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* AGGRESSIVE, PREFLOP, WEAK, FREE */
                { { 0, 55, 31, 14 } },
                /* AGGRESSIVE, PREFLOP, WEAK, SMALL */
                { { 20, 65, 11, 4 } },
                /* AGGRESSIVE, PREFLOP, WEAK, MEDIUM */
                { { 36, 51, 10, 3 } },
                /* AGGRESSIVE, PREFLOP, WEAK, LARGE */
                { { 55, 37, 6, 2 } },
                /* AGGRESSIVE, PREFLOP, WEAK, OVERBET */
                { { 76, 20, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* AGGRESSIVE, PREFLOP, MEDIUM, FREE */
                { { 0, 35, 45, 20 } },
                /* AGGRESSIVE, PREFLOP, MEDIUM, SMALL */
                { { 8, 55, 28, 9 } },
                /* AGGRESSIVE, PREFLOP, MEDIUM, MEDIUM */
                { { 14, 54, 24, 8 } },
                /* AGGRESSIVE, PREFLOP, MEDIUM, LARGE */
                { { 22, 58, 15, 5 } },
                /* AGGRESSIVE, PREFLOP, MEDIUM, OVERBET */
                { { 30, 58, 9, 3 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* AGGRESSIVE, PREFLOP, STRONG, FREE */
                { { 0, 13, 56, 31 } },
                /* AGGRESSIVE, PREFLOP, STRONG, SMALL */
                { { 2, 4, 68, 26 } },
                /* AGGRESSIVE, PREFLOP, STRONG, MEDIUM */
                { { 4, 15, 58, 23 } },
                /* AGGRESSIVE, PREFLOP, STRONG, LARGE */
                { { 6, 45, 35, 14 } },
                /* AGGRESSIVE, PREFLOP, STRONG, OVERBET */
                { { 8, 63, 21, 8 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* AGGRESSIVE, PREFLOP, PREMIUM, FREE */
                { { 0, 8, 48, 44 } },
                /* AGGRESSIVE, PREFLOP, PREMIUM, SMALL */
                { { 0, 3, 52, 45 } },
                /* AGGRESSIVE, PREFLOP, PREMIUM, MEDIUM */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, PREFLOP, PREMIUM, LARGE */
                { { 2, 25, 39, 34 } },
                /* AGGRESSIVE, PREFLOP, PREMIUM, OVERBET */
                { { 3, 54, 23, 20 } },
            },
        },
        /* -- street: POKER_FLOP -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* AGGRESSIVE, FLOP, WEAK, FREE */
                { { 0, 58, 30, 12 } },
                /* AGGRESSIVE, FLOP, WEAK, SMALL */
                { { 20, 65, 11, 4 } },
                /* AGGRESSIVE, FLOP, WEAK, MEDIUM */
                { { 36, 52, 9, 3 } },
                /* AGGRESSIVE, FLOP, WEAK, LARGE */
                { { 55, 37, 6, 2 } },
                /* AGGRESSIVE, FLOP, WEAK, OVERBET */
                { { 75, 21, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* AGGRESSIVE, FLOP, MEDIUM, FREE */
                { { 0, 38, 44, 18 } },
                /* AGGRESSIVE, FLOP, MEDIUM, SMALL */
                { { 8, 56, 27, 9 } },
                /* AGGRESSIVE, FLOP, MEDIUM, MEDIUM */
                { { 14, 55, 23, 8 } },
                /* AGGRESSIVE, FLOP, MEDIUM, LARGE */
                { { 22, 59, 14, 5 } },
                /* AGGRESSIVE, FLOP, MEDIUM, OVERBET */
                { { 30, 59, 8, 3 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* AGGRESSIVE, FLOP, STRONG, FREE */
                { { 0, 14, 55, 31 } },
                /* AGGRESSIVE, FLOP, STRONG, SMALL */
                { { 2, 9, 64, 25 } },
                /* AGGRESSIVE, FLOP, STRONG, MEDIUM */
                { { 4, 19, 55, 22 } },
                /* AGGRESSIVE, FLOP, STRONG, LARGE */
                { { 6, 48, 33, 13 } },
                /* AGGRESSIVE, FLOP, STRONG, OVERBET */
                { { 8, 64, 20, 8 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* AGGRESSIVE, FLOP, PREMIUM, FREE */
                { { 0, 10, 47, 43 } },
                /* AGGRESSIVE, FLOP, PREMIUM, SMALL */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, FLOP, PREMIUM, MEDIUM */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, FLOP, PREMIUM, LARGE */
                { { 2, 28, 37, 33 } },
                /* AGGRESSIVE, FLOP, PREMIUM, OVERBET */
                { { 3, 56, 22, 19 } },
            },
        },
        /* -- street: POKER_TURN -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* AGGRESSIVE, TURN, WEAK, FREE */
                { { 0, 62, 28, 10 } },
                /* AGGRESSIVE, TURN, WEAK, SMALL */
                { { 20, 67, 10, 3 } },
                /* AGGRESSIVE, TURN, WEAK, MEDIUM */
                { { 36, 52, 9, 3 } },
                /* AGGRESSIVE, TURN, WEAK, LARGE */
                { { 55, 38, 5, 2 } },
                /* AGGRESSIVE, TURN, WEAK, OVERBET */
                { { 75, 21, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* AGGRESSIVE, TURN, MEDIUM, FREE */
                { { 0, 40, 44, 16 } },
                /* AGGRESSIVE, TURN, MEDIUM, SMALL */
                { { 8, 59, 25, 8 } },
                /* AGGRESSIVE, TURN, MEDIUM, MEDIUM */
                { { 14, 57, 22, 7 } },
                /* AGGRESSIVE, TURN, MEDIUM, LARGE */
                { { 22, 61, 13, 4 } },
                /* AGGRESSIVE, TURN, MEDIUM, OVERBET */
                { { 30, 59, 8, 3 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* AGGRESSIVE, TURN, STRONG, FREE */
                { { 0, 16, 54, 30 } },
                /* AGGRESSIVE, TURN, STRONG, SMALL */
                { { 2, 13, 61, 24 } },
                /* AGGRESSIVE, TURN, STRONG, MEDIUM */
                { { 4, 22, 53, 21 } },
                /* AGGRESSIVE, TURN, STRONG, LARGE */
                { { 6, 50, 32, 12 } },
                /* AGGRESSIVE, TURN, STRONG, OVERBET */
                { { 8, 66, 19, 7 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* AGGRESSIVE, TURN, PREMIUM, FREE */
                { { 0, 12, 47, 41 } },
                /* AGGRESSIVE, TURN, PREMIUM, SMALL */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, TURN, PREMIUM, MEDIUM */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, TURN, PREMIUM, LARGE */
                { { 2, 32, 35, 31 } },
                /* AGGRESSIVE, TURN, PREMIUM, OVERBET */
                { { 3, 58, 21, 18 } },
            },
        },
        /* -- street: POKER_RIVER -- */
        {
            /* forca: POKER_AI_HAND_WEAK */
            {
                /* AGGRESSIVE, RIVER, WEAK, FREE */
                { { 0, 66, 25, 9 } },
                /* AGGRESSIVE, RIVER, WEAK, SMALL */
                { { 20, 67, 10, 3 } },
                /* AGGRESSIVE, RIVER, WEAK, MEDIUM */
                { { 36, 53, 8, 3 } },
                /* AGGRESSIVE, RIVER, WEAK, LARGE */
                { { 55, 38, 5, 2 } },
                /* AGGRESSIVE, RIVER, WEAK, OVERBET */
                { { 75, 21, 3, 1 } },
            },
            /* forca: POKER_AI_HAND_MEDIUM */
            {
                /* AGGRESSIVE, RIVER, MEDIUM, FREE */
                { { 0, 44, 42, 14 } },
                /* AGGRESSIVE, RIVER, MEDIUM, SMALL */
                { { 8, 60, 24, 8 } },
                /* AGGRESSIVE, RIVER, MEDIUM, MEDIUM */
                { { 14, 58, 21, 7 } },
                /* AGGRESSIVE, RIVER, MEDIUM, LARGE */
                { { 22, 61, 13, 4 } },
                /* AGGRESSIVE, RIVER, MEDIUM, OVERBET */
                { { 30, 61, 7, 2 } },
            },
            /* forca: POKER_AI_HAND_STRONG */
            {
                /* AGGRESSIVE, RIVER, STRONG, FREE */
                { { 0, 18, 53, 29 } },
                /* AGGRESSIVE, RIVER, STRONG, SMALL */
                { { 2, 18, 58, 22 } },
                /* AGGRESSIVE, RIVER, STRONG, MEDIUM */
                { { 4, 26, 50, 20 } },
                /* AGGRESSIVE, RIVER, STRONG, LARGE */
                { { 6, 52, 30, 12 } },
                /* AGGRESSIVE, RIVER, STRONG, OVERBET */
                { { 8, 67, 18, 7 } },
            },
            /* forca: POKER_AI_HAND_PREMIUM */
            {
                /* AGGRESSIVE, RIVER, PREMIUM, FREE */
                { { 0, 14, 47, 39 } },
                /* AGGRESSIVE, RIVER, PREMIUM, SMALL */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, RIVER, PREMIUM, MEDIUM */
                { { 1, 3, 51, 45 } },
                /* AGGRESSIVE, RIVER, PREMIUM, LARGE */
                { { 2, 36, 33, 29 } },
                /* AGGRESSIVE, RIVER, PREMIUM, OVERBET */
                { { 3, 60, 20, 17 } },
            },
        },
    },
};
