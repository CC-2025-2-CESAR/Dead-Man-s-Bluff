#include "core/mao_ia.h"

PokerStatus mao_ia_agir(Mao *mao, PokerAIContext *ia, bool embriagado,
                       PokerAIDecisionInfo *info_out)
{
    if (!mao || !ia)
        return POKER_ERR_ARGUMENT;
    if (mao->situacao != MAO_EM_ANDAMENTO ||
        mao->mesa.actor != TORNEIO_ASSENTO_ADVERSARIO)
        return POKER_ERR_STATE;

    PokerAction acao;
    PokerStatus status = poker_ai_choose_action_mode(
        &mao->mesa,
        TORNEIO_ASSENTO_ADVERSARIO,
        mao->cartas[TORNEIO_ASSENTO_ADVERSARIO],
        mao->board,
        mao->cartas_no_board,
        ia,
        embriagado,
        info_out,
        &acao
    );
    if (status != POKER_OK)
        return status;
    return mao_agir(mao, acao);
}
