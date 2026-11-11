#include "core/mao.h"

/**
 * Distribui as cartas comunitárias da rua informada, queimando uma antes.
 * Falha se a rua não for a próxima do board ou se faltarem cartas.
 */
static bool distribuir_rua(Mao *mao, PokerStreet rua)
{
    return deck_deal_board(
        &mao->baralho,
        rua,
        mao->board,
        &mao->cartas_no_board
    );
}

/**
 * Distribui as ruas que ainda faltam para o board ter as cinco cartas. Não
 * faz nada se o board já está completo.
 */
static bool completar_board(Mao *mao)
{
    for (int rua = mao->mesa.street + 1; rua <= POKER_RIVER; rua++) {
        if (!distribuir_rua(mao, (PokerStreet)rua)) {
            return false;
        }
    }

    return true;
}

/**
 * Paga o pote e fecha a mão. No showdown, completa o board antes, porque o
 * motor exige as cinco cartas mesmo quando o all-in veio antes do river. Se
 * alguém desistiu, o motor não precisa de cartas.
 */
static PokerStatus pagar_pote(Mao *mao)
{
    bool showdown = mao->mesa.phase == POKER_SHOWDOWN;
    PokerStatus status;

    if (showdown && !completar_board(mao)) {
        return POKER_ERR_STATE;
    }

    status = poker_settle(
        &mao->mesa,
        showdown ? mao->cartas[0] : NULL,
        showdown ? mao->cartas[1] : NULL,
        showdown ? mao->board : NULL,
        &mao->resultado
    );

    if (status != POKER_OK) {
        return status;
    }

    mao->situacao = MAO_ENCERRADA;
    deck_free(&mao->baralho);
    return POKER_OK;
}

/**
 * Avança a mão até a próxima decisão ou até o pote ser pago. Cada rodada
 * encerrada recebe as cartas da rua seguinte antes de o motor passar a vez.
 */
static PokerStatus avancar(Mao *mao)
{
    while (mao->mesa.phase == POKER_STREET_FINISHED) {
        PokerStreet proxima = (PokerStreet)(mao->mesa.street + 1);
        PokerStatus status;

        if (!distribuir_rua(mao, proxima)) {
            return POKER_ERR_STATE;
        }

        status = poker_next_street(&mao->mesa);

        if (status != POKER_OK) {
            return status;
        }
    }

    if (mao->mesa.phase == POKER_SHOWDOWN ||
        mao->mesa.phase == POKER_UNCONTESTED) {
        return pagar_pote(mao);
    }

    return POKER_OK;
}

/**
 * Embaralha, distribui as cartas e inicia o motor com os dados do torneio,
 * avançando a mão se ninguém tiver ação a tomar.
 */
static bool preparar_mao(Mao *mao, const Torneio *torneio)
{
    PokerChips stacks[2];

    torneio_stacks(torneio, stacks);

    if (!deck_create(&mao->baralho)) {
        return false;
    }

    deck_shuffle(&mao->baralho);

    if (!deck_deal(&mao->baralho, torneio->dealer, mao->cartas)) {
        return false;
    }

    if (poker_start_hand(
            &mao->mesa,
            stacks,
            torneio->dealer,
            torneio_small_blind(torneio),
            torneio_big_blind(torneio)
        ) != POKER_OK) {
        return false;
    }

    mao->situacao = MAO_EM_ANDAMENTO;
    return avancar(mao) == POKER_OK;
}

bool mao_iniciar(Mao *mao, const Torneio *torneio)
{
    if (mao == NULL || torneio == NULL ||
        torneio->situacao != TORNEIO_EM_ANDAMENTO) {
        return false;
    }

    mao_liberar(mao);
    *mao = (Mao){0};

    if (preparar_mao(mao, torneio)) {
        return true;
    }

    mao_liberar(mao);
    *mao = (Mao){0};
    return false;
}

PokerStatus mao_agir(Mao *mao, PokerAction acao)
{
    PokerStatus status;

    if (mao == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (mao->situacao != MAO_EM_ANDAMENTO) {
        return POKER_ERR_STATE;
    }

    status = poker_act(&mao->mesa, mao->mesa.actor, acao);

    if (status != POKER_OK) {
        return status;
    }

    return avancar(mao);
}

bool mao_registrar_no_torneio(Mao *mao, Torneio *torneio)
{
    if (mao == NULL || torneio == NULL || mao->situacao != MAO_ENCERRADA) {
        return false;
    }

    if (!torneio_registrar_mao(
            torneio,
            mao->mesa.player[TORNEIO_ASSENTO_JOGADOR].chips,
            mao->mesa.player[TORNEIO_ASSENTO_ADVERSARIO].chips
        )) {
        return false;
    }

    mao->situacao = MAO_REGISTRADA;
    return true;
}

void mao_liberar(Mao *mao)
{
    if (mao == NULL) {
        return;
    }

    deck_free(&mao->baralho);

    if (mao->situacao == MAO_EM_ANDAMENTO) {
        mao->situacao = MAO_VAZIA;
    }
}
