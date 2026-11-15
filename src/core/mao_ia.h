#ifndef MAO_IA_H
#define MAO_IA_H

#include "core/mao.h"
#include "gameplay/poker_ai_config.h"

/* Executa somente o turno do adversario, preservando o motor de poker.
 * Nao avanca quando a vez e do jogador ou a mao terminou.
 * O contexto e seus RNGs devem viver durante todo o confronto.
 * Modo embriagado opcional, com a API sobria existente preservada. */
PokerStatus mao_ia_agir(Mao *mao, PokerAIContext *ia, bool embriagado,
                       PokerAIDecisionInfo *info_out);

#endif
