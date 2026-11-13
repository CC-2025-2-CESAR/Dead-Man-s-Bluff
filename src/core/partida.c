#include "core/partida.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Escolhe o perfil da IA de um confronto: o primeiro adversário é o
 * Agressivo, o segundo o Medroso e o último o Desconfiado.
 */
static PokerAIStyle estilo_do_confronto(int confronto)
{
    switch (confronto) {
        case 0:
            return POKER_AI_AGGRESSIVE;

        case 1:
            return POKER_AI_CAUTIOUS;

        default:
            return POKER_AI_BALANCED;
    }
}

/**
 * Ajusta a situação da partida ao que aconteceu na mão. Se a mão acabou,
 * registra o resultado no torneio antes de olhar para ele.
 *
 * @return false se o torneio recusou o resultado da mão.
 */
static bool atualizar_situacao(Partida *partida)
{
    Mao *mao = &partida->mao;

    if (mao->situacao == MAO_ENCERRADA &&
        !mao_registrar_no_torneio(mao, &partida->torneio)) {
        return false;
    }

    if (mao->situacao == MAO_EM_ANDAMENTO) {
        partida->situacao = mao->mesa.actor == TORNEIO_ASSENTO_JOGADOR
            ? PARTIDA_VEZ_DO_JOGADOR
            : PARTIDA_VEZ_DO_ADVERSARIO;
        return true;
    }

    switch (partida->torneio.situacao) {
        case TORNEIO_CONFRONTO_VENCIDO:
            partida->situacao = PARTIDA_CONFRONTO_VENCIDO;
            break;

        case TORNEIO_JOGADOR_FALIDO:
            partida->situacao = PARTIDA_JOGADOR_FALIDO;
            break;

        case TORNEIO_CAMPEAO:
            partida->situacao = PARTIDA_CAMPEAO;
            break;

        case TORNEIO_EM_ANDAMENTO:
            partida->situacao = PARTIDA_MAO_ENCERRADA;
            break;
    }

    return true;
}

/**
 * Inicia a mão seguinte do torneio, com a fila de eventos da partida.
 */
static bool iniciar_mao(Partida *partida)
{
    return mao_iniciar_com_eventos(
               &partida->mao,
               &partida->torneio,
               &partida->eventos
           ) && atualizar_situacao(partida);
}

bool partida_iniciar(Partida *partida, unsigned semente)
{
    if (partida == NULL) {
        return false;
    }

    mao_liberar(&partida->mao);
    memset(partida, 0, sizeof *partida);

    srand(semente);
    torneio_iniciar(&partida->torneio);
    fila_eventos_iniciar(&partida->eventos);
    poker_ai_rng_seed(&partida->rng_acao, (uint64_t)semente * 2u + 1u);
    poker_ai_rng_seed(&partida->rng_equidade, (uint64_t)semente * 3u + 7u);

    partida->ia.style = estilo_do_confronto(partida->torneio.confronto);
    partida->ia.table = NULL;
    partida->ia.action_rng = &partida->rng_acao;
    partida->ia.equity_rng = &partida->rng_equidade;

    if (iniciar_mao(partida)) {
        return true;
    }

    partida_encerrar(partida);
    return false;
}

void partida_encerrar(Partida *partida)
{
    if (partida == NULL) {
        return;
    }

    mao_liberar(&partida->mao);
    partida->situacao = PARTIDA_VAZIA;
}

PokerStatus partida_agir_jogador(Partida *partida, PokerAction acao)
{
    PokerStatus status;

    if (partida == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_JOGADOR) {
        return partida->situacao == PARTIDA_VEZ_DO_ADVERSARIO
            ? POKER_ERR_TURN
            : POKER_ERR_STATE;
    }

    status = mao_agir(&partida->mao, acao);

    if (status != POKER_OK) {
        return status;
    }

    return atualizar_situacao(partida) ? POKER_OK : POKER_ERR_STATE;
}

PokerStatus partida_comando_jogador(Partida *partida, ComandoJogador comando, PokerChips total)
{
    PokerLegalActions legais;
    PokerStatus status = partida_acoes_legais(partida, &legais);
    PokerAction acao;

    if (status != POKER_OK) {
        return status;
    }

    switch (comando) {
        case COMANDO_DESISTIR:
            acao = (PokerAction){POKER_FOLD, 0};
            break;

        case COMANDO_PASSAR_OU_PAGAR:
            acao = (PokerAction){legais.check ? POKER_CHECK : POKER_CALL, 0};
            break;

        case COMANDO_APOSTAR:
            if (!legais.bet && !legais.raise) {
                return POKER_ERR_ACTION;
            }

            acao = (PokerAction){legais.bet ? POKER_BET : POKER_RAISE, total};
            break;

        case COMANDO_ALL_IN:
            acao = (PokerAction){POKER_ALL_IN, 0};
            break;

        default:
            return POKER_ERR_ARGUMENT;
    }

    return partida_agir_jogador(partida, acao);
}

PokerChips partida_ajustar_aposta(const Partida *partida, PokerChips atual, int passos)
{
    PokerLegalActions legais;
    PokerChips total;

    if (partida_acoes_legais(partida, &legais) != POKER_OK ||
        (!legais.bet && !legais.raise)) {
        return 0;
    }

    if (atual < legais.min_total) {
        return legais.min_total;
    }

    total = atual + (PokerChips)passos * partida->mao.mesa.big_blind;

    if (total < legais.min_total) {
        return legais.min_total;
    }

    return total > legais.max_total ? legais.max_total : total;
}

PokerStatus partida_agir_adversario(Partida *partida, PokerAIDecisionInfo *info_out)
{
    PokerStatus status;

    if (partida == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_ADVERSARIO) {
        return partida->situacao == PARTIDA_VEZ_DO_JOGADOR
            ? POKER_ERR_TURN
            : POKER_ERR_STATE;
    }

    status = mao_ia_agir(
        &partida->mao,
        &partida->ia,
        partida->adversario_embriagado,
        info_out
    );

    if (status != POKER_OK) {
        return status;
    }

    return atualizar_situacao(partida) ? POKER_OK : POKER_ERR_STATE;
}

bool partida_continuar(Partida *partida)
{
    if (partida == NULL) {
        return false;
    }

    if (partida->situacao == PARTIDA_CONFRONTO_VENCIDO) {
        if (!torneio_avancar_confronto(&partida->torneio)) {
            return false;
        }

        partida->ia.style = estilo_do_confronto(partida->torneio.confronto);
        return iniciar_mao(partida);
    }

    if (partida->situacao == PARTIDA_MAO_ENCERRADA) {
        return iniciar_mao(partida);
    }

    return false;
}

PokerStatus partida_acoes_legais(const Partida *partida, PokerLegalActions *out)
{
    if (partida == NULL || out == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_JOGADOR) {
        return partida->situacao == PARTIDA_VEZ_DO_ADVERSARIO
            ? POKER_ERR_TURN
            : POKER_ERR_STATE;
    }

    return poker_legal_actions(
        &partida->mao.mesa,
        TORNEIO_ASSENTO_JOGADOR,
        out
    );
}

const char *partida_nome_do_adversario(int confronto)
{
    switch (confronto) {
        case 0:
            return "Agressivo";

        case 1:
            return "Medroso";

        case 2:
            return "Desconfiado";

        default:
            return "?";
    }
}
