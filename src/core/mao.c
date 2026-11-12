#include "core/mao.h"

/**
 * Coloca um evento na fila da mão, se houver uma. A mão não depende de a
 * fila ter espaço: se ela estiver cheia, o evento se perde e a fila conta.
 */
static void registrar_evento(Mao *mao, const EventoJogo *evento)
{
    if (mao->eventos != NULL) {
        fila_eventos_adicionar(mao->eventos, evento);
    }
}

/**
 * Registra o início da mão com os blinds postados e as cartas de cada
 * assento, começando pelo dealer, que recebe a primeira carta.
 */
static void registrar_inicio(Mao *mao, const Torneio *torneio)
{
    EventoJogo evento = {.tipo = EVENTO_MAO_INICIADA};

    evento.mao_iniciada.dealer = torneio->dealer;
    evento.mao_iniciada.pequeno_blind = torneio_small_blind(torneio);
    evento.mao_iniciada.grande_blind = torneio_big_blind(torneio);

    for (int assento = 0; assento < 2; assento++) {
        evento.mao_iniciada.fichas_postadas[assento] =
            mao->mesa.player[assento].committed;
        evento.mao_iniciada.fichas_restantes[assento] =
            mao->mesa.player[assento].chips;
    }

    registrar_evento(mao, &evento);

    for (int i = 0; i < 2; i++) {
        int assento = i == 0 ? torneio->dealer : 1 - torneio->dealer;
        EventoJogo cartas = {.tipo = EVENTO_CARTAS_DISTRIBUIDAS};

        cartas.cartas_distribuidas.assento = assento;
        cartas.cartas_distribuidas.cartas[0] = mao->cartas[assento][0];
        cartas.cartas_distribuidas.cartas[1] = mao->cartas[assento][1];
        registrar_evento(mao, &cartas);
    }
}

/**
 * Registra a ação que acabou de ser aplicada pelo motor.
 *
 * @param assento quem agiu.
 * @param acao tipo da ação aplicada.
 * @param fichas_antes stack do assento antes da ação.
 */
static void registrar_acao(
    Mao *mao,
    int assento,
    PokerActionType acao,
    PokerChips fichas_antes
)
{
    EventoJogo evento = {.tipo = EVENTO_ACAO};
    const PokerPlayer *jogador = &mao->mesa.player[assento];

    evento.acao.assento = assento;
    evento.acao.acao = acao;
    evento.acao.fichas_colocadas = fichas_antes - jogador->chips;
    evento.acao.total_na_rua = jogador->street_bet;
    evento.acao.fichas_restantes = jogador->chips;
    evento.acao.pote = poker_pot(&mao->mesa);
    registrar_evento(mao, &evento);
}

/**
 * Registra a revelação das cartas do board que entraram depois de
 * cartas_antes cartas já reveladas.
 */
static void registrar_board(Mao *mao, PokerStreet rua, size_t cartas_antes)
{
    EventoJogo evento = {.tipo = EVENTO_BOARD_REVELADO};

    evento.board.rua = rua;
    evento.board.quantidade = (int)(mao->cartas_no_board - cartas_antes);

    for (int i = 0; i < evento.board.quantidade; i++) {
        evento.board.cartas[i] = mao->board[cartas_antes + (size_t)i];
    }

    registrar_evento(mao, &evento);
}

/**
 * Registra o showdown, se houve, e o pagamento do pote que fecha a mão.
 */
static void registrar_pagamento(Mao *mao)
{
    EventoJogo pote = {.tipo = EVENTO_POTE_PAGO};

    if (mao->resultado.showdown) {
        EventoJogo showdown = {.tipo = EVENTO_SHOWDOWN};

        for (int assento = 0; assento < 2; assento++) {
            showdown.showdown.cartas[assento][0] = mao->cartas[assento][0];
            showdown.showdown.cartas[assento][1] = mao->cartas[assento][1];
            showdown.showdown.valor[assento] = mao->resultado.value[assento];
        }

        registrar_evento(mao, &showdown);
    }

    pote.pote.vencedores = mao->resultado.winners;
    pote.pote.showdown = mao->resultado.showdown;
    pote.pote.pote = mao->resultado.pot;

    for (int assento = 0; assento < 2; assento++) {
        pote.pote.premio[assento] = mao->resultado.award[assento];
        pote.pote.devolucao[assento] = mao->resultado.refund[assento];
        pote.pote.fichas_finais[assento] = mao->mesa.player[assento].chips;
    }

    registrar_evento(mao, &pote);
}

/**
 * Distribui as cartas comunitárias da rua informada, queimando uma antes, e
 * registra a revelação. Falha se a rua não for a próxima do board ou se
 * faltarem cartas.
 */
static bool distribuir_rua(Mao *mao, PokerStreet rua)
{
    size_t cartas_antes = mao->cartas_no_board;

    if (!deck_deal_board(
            &mao->baralho,
            rua,
            mao->board,
            &mao->cartas_no_board
        )) {
        return false;
    }

    registrar_board(mao, rua, cartas_antes);
    return true;
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
    registrar_pagamento(mao);
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
    registrar_inicio(mao, torneio);
    return avancar(mao) == POKER_OK;
}

bool mao_iniciar(Mao *mao, const Torneio *torneio)
{
    return mao_iniciar_com_eventos(mao, torneio, NULL);
}

bool mao_iniciar_com_eventos(
    Mao *mao,
    const Torneio *torneio,
    FilaEventos *eventos
)
{
    if (mao == NULL || torneio == NULL ||
        torneio->situacao != TORNEIO_EM_ANDAMENTO) {
        return false;
    }

    mao_liberar(mao);
    *mao = (Mao){0};
    mao->eventos = eventos;

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
    int assento;
    PokerChips fichas_antes;

    if (mao == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (mao->situacao != MAO_EM_ANDAMENTO) {
        return POKER_ERR_STATE;
    }

    assento = mao->mesa.actor;

    if (assento != TORNEIO_ASSENTO_JOGADOR &&
        assento != TORNEIO_ASSENTO_ADVERSARIO) {
        return POKER_ERR_STATE;
    }

    fichas_antes = mao->mesa.player[assento].chips;
    status = poker_act(&mao->mesa, assento, acao);

    if (status != POKER_OK) {
        return status;
    }

    registrar_acao(mao, assento, acao.type, fichas_antes);
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
