#ifndef POKER_AI_H
#define POKER_AI_H

/*
 * IA de personalidade para o Dead Man's Bluff.
 *
 * ATENCAO (leia antes de tocar neste arquivo):
 * - Esta IA so CALCULA uma PokerAction legal. Ela nunca muta PokerTable,
 *   nunca distribui cartas e nunca chama poker_act() -- isso e trabalho
 *   do core (src/core/).
 * - O "cerebro" probabilistico (pesos por cenario) mora em
 *   poker_ai_config.h/.c, nao aqui. Este arquivo so declara os tipos e
 *   as funcoes publicas; a tabela de numeros editaveis fica isolada.
 * - As cartas e regras sao as do motor da Julia em src/poker/poker.h.
 * - Configuracao e contexto de decisao ficam em poker_ai_config.h.
 */

#include "../poker/poker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------
 * Perfis de personalidade.
 *
 * Os tres perfis compartilham EXATAMENTE o mesmo algoritmo de decisao
 * (poker_ai_choose_action). O que muda entre "Agressivo", "Medroso" e
 * "Desconfiado" e so a linha que cada um le na tabela de configuracao
 * -- nunca um "if (style == ...)" dentro da logica de decisao.
 *
 *   POKER_AI_CAUTIOUS   <-> "Medroso"     (tight, evita pressao)
 *   POKER_AI_BALANCED   <-> "Desconfiado" (regular, equilibrado)
 *   POKER_AI_AGGRESSIVE <-> "Agressivo"   (loose-aggressive)
 * --------------------------------------------------------------------- */
typedef enum {
    POKER_AI_CAUTIOUS = 0,
    POKER_AI_BALANCED,
    POKER_AI_AGGRESSIVE
} PokerAIStyle;

/* ---------------------------------------------------------------------
 * Forca da mao. E a mesma classificacao de 4 niveis usada em todas as
 * streets:
 *   - No pre-flop vem de uma heuristica de ranges iniciais
 *     (poker_ai_evaluate_preflop), porque ainda nao existe board.
 *   - No flop/turn/river vem de uma equidade estimada por Monte Carlo
 *     (poker_ai_estimate_equity) cortada em faixas configuraveis
 *     (poker_ai_equity_thresholds, em poker_ai_config.h).
 *
 * As duas fontes sao heuristicas DIFERENTES alimentando o MESMO enum,
 * de propósito: a tabela de cenarios nao precisa saber como a forca foi
 * calculada, so em qual dos 4 baldes ela caiu.
 * --------------------------------------------------------------------- */
typedef enum {
    POKER_AI_HAND_WEAK = 0,
    POKER_AI_HAND_MEDIUM,
    POKER_AI_HAND_STRONG,
    POKER_AI_HAND_PREMIUM
} PokerAIHandStrength;

/* ---------------------------------------------------------------------
 * Informacao de draw (flop/turn). Mantida do rascunho original, com UM
 * bug corrigido (ver poker_ai.c): um flush draw so conta como "nosso"
 * se pelo menos uma hole card for do naipe que esta formando o draw.
 * Sem essa checagem, quatro cartas do mesmo naipe caidas inteiramente
 * no board (visiveis para os dois jogadores) seriam contadas como
 * vantagem exclusiva da IA, o que e falso.
 *
 * Esta struct e META-DADO INFORMATIVO (aparece em PokerAIDecisionInfo
 * para depuracao/telemetria). A decisao de apostar em si usa a
 * equidade de Monte Carlo (que já "sabe" sobre draws, pois simula as
 * cartas que faltam), e nao usa PokerAIDrawInfo como entrada numerica
 * direta -- isso evita contar o mesmo draw duas vezes com pesos
 * diferentes.
 * --------------------------------------------------------------------- */
typedef enum {
    POKER_AI_DRAW_NONE,
    POKER_AI_DRAW_BACKDOOR,
    POKER_AI_DRAW_GUTSHOT,
    POKER_AI_DRAW_OPEN_ENDED,
    POKER_AI_DRAW_FLUSH,
    POKER_AI_DRAW_COMBO
} PokerAIDrawStrength;

typedef struct {
    PokerAIDrawStrength strength;
    unsigned nominal_outs; /* Aproximacao: nao equivale a outs "limpos". */
    bool flush_draw;       /* Exige >=1 hole card do naipe (ver poker_ai.c). */
    bool open_ended;       /* Inclui draws duplos de sequencia com 8 outs. */
    bool gutshot;
    bool backdoor_flush;   /* Idem flush_draw: exige hole card do naipe. */
} PokerAIDrawInfo;

/* ---------------------------------------------------------------------
 * RNG proprio da IA.
 *
 * Requisito do projeto: a IA nao pode chamar rand()/srand() (isso e do
 * baralho, em src/cards/). O core cria UM PokerAIRng por jogador (ou um
 * so, compartilhado -- quem decide isso e o core) com uma seed fixa no
 * inicio do torneio e guarda esse estado entre as chamadas. Assim os
 * testes conseguem reproduzir exatamente a mesma sequencia de decisoes.
 *
 * Implementacao: xorshift64* de 64 bits. Nao e criptografico, nao
 * precisa ser -- so precisa ser rapido, determinístico e independente
 * do gerador do baralho.
 * --------------------------------------------------------------------- */
typedef struct {
    uint64_t state;
} PokerAIRng;

/* Inicializa o estado a partir de uma seed. Seed 0 e remapeada
 * internamente (xorshift fica preso em 0 se a seed for 0). */
void poker_ai_rng_seed(PokerAIRng *rng, uint64_t seed);

/* Um double uniforme em [0, 1). Usado tanto no sorteio ponderado de
 * acao quanto no tamanho da aposta e no Monte Carlo de equidade. */
double poker_ai_rng_uniform(PokerAIRng *rng);

/* ---------------------------------------------------------------------
 * Classificacao da mao inicial (pre-flop). Nao olha a mesa.
 * --------------------------------------------------------------------- */
PokerStatus poker_ai_evaluate_preflop(
    const PokerCard hole[2],
    PokerAIHandStrength *out
);

/* Identifica draws no flop/turn; no river nao existe compra futura
 * (board_count == 5 sempre devolve PokerAIDrawInfo zerado). */
PokerStatus poker_ai_evaluate_draws(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerAIDrawInfo *out
);

/* call_cost / (pote_atual + call_cost). Mantida por compatibilidade e
 * para depuracao; a decisao em si usa poker_ai_classify_pressure
 * (poker_ai_config.h), que e a classificacao EXIGIDA pelo projeto
 * (relativa ao pote ANTES da aposta, nao igual a esta funcao). */
PokerStatus poker_ai_pot_odds(
    const PokerTable *table,
    int seat,
    double *out
);

/* Monte Carlo: equidade do jogador contra UMA mao oculta amostrada
 * uniformemente do baralho restante, simulando tambem as cartas de
 * board que ainda vao sair (se board_count < 5).
 *
 * IMPORTANTE (documentado conforme exigido): isto e equidade contra
 * uma mao aleatoria, NAO contra o range real do adversario. Um
 * adversario "Medroso" raramente teria, na pratica, uma mao aleatoria
 * nas maos nessa altura da partida -- mas modelar o range real exigiria
 * conhecer a estrategia do oponente, o que este modulo nao faz. E uma
 * aproximacao deliberada, nao um erro.
 *
 * iterations tipico: poker_ai_monte_carlo_iterations (poker_ai_config.h).
 * Reprodutivel: usa poker_ai_rng_uniform(rng), nunca rand(). */
PokerStatus poker_ai_estimate_equity(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerAIRng *rng,
    int iterations,
    double *out_equity
);

/* ---------------------------------------------------------------------
 * Informacao de depuracao/balanceamento (opcional: pode passar NULL em
 * poker_ai_choose_action se nao precisar). Mostra o cenario que foi
 * selecionado na tabela, os pesos CONFIGURADOS (como estao no arquivo
 * de tuning) e os pesos EFETIVOS (depois de remover acoes ilegais e
 * aplicar a regra "nao foldar com check gratuito") -- a diferenca entre
 * os dois e exatamente o requisito "distinguir pesos configurados dos
 * efetivos".
 * --------------------------------------------------------------------- */
typedef enum {
    POKER_AI_ACTION_FOLD = 0,
    POKER_AI_ACTION_PASSIVE, /* CHECK se gratuito, senao CALL */
    POKER_AI_ACTION_RAISE,
    POKER_AI_ACTION_RAISE_BIG
} PokerAIActionSlot;
#define POKER_AI_ACTION_SLOT_COUNT 4

/* Declarado aqui (nao em poker_ai_config.h) por depender de
 * PokerAIActionSlot; o enum de pressao mora em poker_ai_config.h porque
 * e puramente um numero de configuracao. */
typedef struct {
    PokerAIStyle profile;
    PokerStreet street;
    PokerAIHandStrength strength;
    int pressure;           /* valor de PokerAIPressure (poker_ai_config.h) */
    double pressure_ratio;  /* razao USADA para selecionar o cenario; -1 se FREE */
    double nominal_pressure_ratio; /* aposta adversaria / pote antes (sem cap) */
    double effective_pot_odds; /* call_cost / pote contestado apos call */
    bool cheap_all_in_override; /* fold transferido explicitamente para call */
    double equity;          /* -1.0 no pre-flop (nao calculada) */
    double spr;             /* fichas_restantes / pote_antes; -1.0 se indefinido */
    bool capped_all_in_call;/* legal.call_cost < legal.to_call (call so cobre parte) */
    double configured_weight[POKER_AI_ACTION_SLOT_COUNT];
    double effective_weight[POKER_AI_ACTION_SLOT_COUNT];
    PokerAIActionSlot chosen_slot;
} PokerAIDecisionInfo;

/*
 * Decide a jogada da IA para `seat` na mesa `table`.
 *
 * - hole/board/board_count: cartas conhecidas pela IA (as suas proprias
 *   hole cards + o board publico). A IA nunca ve as hole cards do
 *   adversario.
 * - context: perfil/tabela e RNGs de acao + simulacao (ver config.h).
 * - info_out: pode ser NULL. Se nao for, e preenchido com o cenario
 *   usado e os pesos configurados/efetivos, para depuracao e balanceamento.
 * - out: a acao legal escolhida.
 *
 * Retorna POKER_OK, ou um PokerStatus de erro se os argumentos forem
 * invalidos ou poker_legal_actions falhar. Nunca muta `table`.
 */
/* Definicao completa de PokerAIContext em poker_ai_config.h.
 * style seleciona o PERFIL; table (NULL = tabela padrao) seleciona a
 * matriz. RNG de escolha e RNG de equity sao estados INDEPENDENTES. */
typedef struct PokerAIContext PokerAIContext;

PokerStatus poker_ai_choose_action(
    const PokerTable *table,
    int seat,
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    const PokerAIContext *context,
    PokerAIDecisionInfo *info_out,
    PokerAction *out
);

/* Embriaguez V1: mesma entrada/saida da API padrao, com flag adicional.
 * FALSE reproduz a IA sobria; TRUE habilita modificadores por perfil.
 * A integracao Raylib pode continuar usando poker_ai_choose_action(). */
PokerStatus poker_ai_choose_action_mode(
    const PokerTable *table, int seat, const PokerCard hole[2],
    const PokerCard *board, size_t board_count,
    const PokerAIContext *context, bool intoxicated,
    PokerAIDecisionInfo *info_out, PokerAction *out);

/* Hook para futura deteccao de trapaça:
 * chance = min(1, chance_base * fator_personalidade * fator_embriaguez).
 * Fatores iniciais: CAUTIOUS=1.0, BALANCED=1.7, AGGRESSIVE=0.8.
 * BALANCED embriagado usa fator adicional 0.55.
 * Ainda NAO executa deteccao nem aplica penalidade: so calcula chance. */
double poker_ai_detection_chance(PokerAIStyle style, bool intoxicated,
                                  double base_chance);

#endif
