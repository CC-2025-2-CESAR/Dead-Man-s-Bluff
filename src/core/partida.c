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
 * Abre a mão do torneio atual com a fila de eventos da partida e ajusta a
 * situação da partida a ela.
 */
static bool abrir_mao(Partida *partida)
{
    return mao_iniciar_com_eventos(
               &partida->mao,
               &partida->torneio,
               &partida->eventos
           ) && atualizar_situacao(partida);
}

/**
 * Sorteia a semente do embaralhamento de uma mão com o gerador da partida
 * (splitmix64), que avança o estado a cada sorteio. O rand global não serve:
 * ele é compartilhado entre partidas e dá só 15 bits no Windows.
 */
static unsigned sortear_semente_da_mao(uint64_t *estado)
{
    uint64_t z;

    *estado += 0x9E3779B97F4A7C15ull;
    z = *estado;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (unsigned)(z >> 32);
}

/**
 * Inicia a mão seguinte do torneio. Guarda o torneio como está e sorteia a
 * semente do baralho da mão, para que ela possa ser refeita depois.
 */
static bool iniciar_mao(Partida *partida)
{
    partida->torneio_da_mao = partida->torneio;
    partida->semente_da_mao = sortear_semente_da_mao(&partida->estado_das_sementes);
    srand(partida->semente_da_mao);
    return abrir_mao(partida);
}

/**
 * Zera a partida e deixa pronta a parte que não depende do torneio: a fila de
 * eventos e a IA, ligada aos geradores que a partida guarda.
 */
static void preparar_partida(Partida *partida, uint64_t estado_acao, uint64_t estado_equidade)
{
    mao_liberar(&partida->mao);
    memset(partida, 0, sizeof *partida);

    fila_eventos_iniciar(&partida->eventos);
    partida->rng_acao.state = estado_acao;
    partida->rng_equidade.state = estado_equidade;

    partida->ia.table = NULL;
    partida->ia.action_rng = &partida->rng_acao;
    partida->ia.equity_rng = &partida->rng_equidade;
}

bool partida_iniciar(Partida *partida, unsigned semente)
{
    PokerAIRng rng_acao;
    PokerAIRng rng_equidade;

    if (partida == NULL) {
        return false;
    }

    poker_ai_rng_seed(&rng_acao, (uint64_t)semente * 2u + 1u);
    poker_ai_rng_seed(&rng_equidade, (uint64_t)semente * 3u + 7u);

    preparar_partida(partida, rng_acao.state, rng_equidade.state);
    partida->estado_das_sementes = (uint64_t)semente * 4u + 5u;
    torneio_iniciar(&partida->torneio);
    partida->ia.style = estilo_do_confronto(partida->torneio.confronto);

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

/** Diz se a partida está num ponto em que pode ser guardada e continuada. */
static bool situacao_continuavel(PartidaSituacao situacao)
{
    switch (situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
        case PARTIDA_VEZ_DO_ADVERSARIO:
        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
            return true;

        case PARTIDA_VAZIA:
        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_CAMPEAO:
            break;
    }

    return false;
}

bool partida_exportar(const Partida *partida, PartidaSalva *salva)
{
    const Mao *mao;
    PartidaSalva copia = {0};

    if (partida == NULL || salva == NULL ||
        !situacao_continuavel(partida->situacao) ||
        partida->mao.acoes_perdidas) {
        return false;
    }

    mao = &partida->mao;
    copia.torneio = partida->torneio_da_mao;
    copia.semente_da_mao = partida->semente_da_mao;
    copia.estado_das_sementes = partida->estado_das_sementes;
    copia.rng_acao = partida->rng_acao.state;
    copia.rng_equidade = partida->rng_equidade.state;
    copia.adversario_embriagado = partida->adversario_embriagado;
    copia.situacao = partida->situacao;
    memcpy(copia.cartas, mao->cartas, sizeof copia.cartas);
    memcpy(copia.board, mao->board, sizeof copia.board);
    copia.cartas_no_board = mao->cartas_no_board;
    copia.fichas[0] = mao->mesa.player[0].chips;
    copia.fichas[1] = mao->mesa.player[1].chips;
    copia.quantidade_de_acoes = mao->quantidade_de_acoes;
    memcpy(copia.acoes, mao->acoes, mao->quantidade_de_acoes * sizeof mao->acoes[0]);

    *salva = copia;
    return true;
}

/** Confere se os dados guardados descrevem um torneio que pode recomeçar uma mão. */
static bool dados_validos(const PartidaSalva *salva)
{
    const Torneio *torneio = &salva->torneio;

    return torneio->situacao == TORNEIO_EM_ANDAMENTO &&
           torneio->confronto >= 0 && torneio->confronto < TORNEIO_CONFRONTOS &&
           torneio->fichas_jogador >= 0 && torneio->fichas_jogador <= POKER_MAX_STACK &&
           torneio->fichas_adversario >= 0 && torneio->fichas_adversario <= POKER_MAX_STACK &&
           torneio->maos_jogadas >= 0 &&
           (torneio->dealer == TORNEIO_ASSENTO_JOGADOR ||
            torneio->dealer == TORNEIO_ASSENTO_ADVERSARIO) &&
           torneio->recuperacoes >= 0 &&
           salva->rng_acao != 0 && salva->rng_equidade != 0 &&
           situacao_continuavel(salva->situacao) &&
           salva->quantidade_de_acoes <= MAO_ACOES_MAXIMAS &&
           salva->cartas_no_board <= MAO_CARTAS_NO_BOARD;
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

/** Confere se a mão refeita é igual à que foi guardada. */
static bool mao_igual_a_guardada(const Partida *partida, const PartidaSalva *salva)
{
    const Mao *mao = &partida->mao;

    if (partida->situacao != salva->situacao ||
        mao->cartas_no_board != salva->cartas_no_board ||
        mao->mesa.player[0].chips != salva->fichas[0] ||
        mao->mesa.player[1].chips != salva->fichas[1]) {
        return false;
    }

    for (int assento = 0; assento < 2; assento++) {
        for (int i = 0; i < 2; i++) {
            if (!mesma_carta(mao->cartas[assento][i], salva->cartas[assento][i])) {
                return false;
            }
        }
    }

    for (size_t i = 0; i < salva->cartas_no_board; i++) {
        if (!mesma_carta(mao->board[i], salva->board[i])) {
            return false;
        }
    }

    return true;
}

bool partida_restaurar(Partida *partida, const PartidaSalva *salva)
{
    bool refeita;

    if (partida == NULL) {
        return false;
    }

    if (salva == NULL || !dados_validos(salva)) {
        partida_encerrar(partida);
        return false;
    }

    preparar_partida(partida, salva->rng_acao, salva->rng_equidade);
    partida->torneio = salva->torneio;
    partida->torneio_da_mao = salva->torneio;
    partida->semente_da_mao = salva->semente_da_mao;
    partida->estado_das_sementes = salva->estado_das_sementes;
    partida->adversario_embriagado = salva->adversario_embriagado;
    partida->ia.style = estilo_do_confronto(partida->torneio.confronto);

    srand(salva->semente_da_mao);
    refeita = abrir_mao(partida);

    for (size_t i = 0; refeita && i < salva->quantidade_de_acoes; i++) {
        refeita = partida->mao.situacao == MAO_EM_ANDAMENTO &&
                  mao_agir(&partida->mao, salva->acoes[i]) == POKER_OK &&
                  atualizar_situacao(partida);
    }

    if (refeita && mao_igual_a_guardada(partida, salva)) {
        return true;
    }

    partida_encerrar(partida);
    return false;
}
