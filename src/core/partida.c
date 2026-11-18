#include "core/partida.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Limite do medroso de cada confronto, em %; o chefe resiste mais antes de ter medo. */
static const int LIMITE_DO_MEDROSO[TORNEIO_CONFRONTOS] = { 60, 60, 40 };

int partida_limite_do_medroso(int confronto)
{
    if (confronto < 0 || confronto >= TORNEIO_CONFRONTOS) {
        return 0;
    }

    return LIMITE_DO_MEDROSO[confronto];
}

PokerAIStyle partida_estilo_do_adversario(const Torneio *torneio)
{
    PokerChips inicial = torneio_fichas_iniciais_adversario(torneio->confronto);

    if (inicial <= 0) {
        return POKER_AI_BALANCED;
    }

    if (torneio->fichas_adversario * 100 <= inicial * partida_limite_do_medroso(torneio->confronto)) {
        return POKER_AI_CAUTIOUS;
    }

    if (torneio->fichas_adversario * 100 >= inicial * PARTIDA_ESTILO_FRENTE_PCT) {
        return POKER_AI_AGGRESSIVE;
    }

    return POKER_AI_BALANCED;
}

static bool carta_valida(PokerCard carta)
{
    return carta.rank >= 2 && carta.rank <= 14 &&
           carta.suit >= POKER_CLUBS && carta.suit <= POKER_SPADES;
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

/**
 * Perfil de risco do adversário do confronto atual: o quanto ele repara nas
 * trapaças. É de cada oponente e sobe de um confronto para o outro; não segue
 * o estilo de jogo, que muda durante o confronto.
 */
static ProbPerfilIA perfil_do_adversario(const Partida *partida)
{
    switch (partida->torneio.confronto) {
        case 0:
            return PROB_IA_AGRESSIVO;

        case 1:
            return PROB_IA_MEDROSO;

        default:
            return PROB_IA_DESCONFIADO;
    }
}

/** Tipo de risco de cada pedido; PROB_TRAPACA_TOTAL se o pedido não vale. */
static TipoTrapacaProb tipo_de_risco(TipoDePedido tipo)
{
    switch (tipo) {
        case PEDIDO_BEBIDA:
            return PROB_TRAPACA_SPIKE_DRINK;

        case PEDIDO_CUMPLICE:
            return PROB_TRAPACA_CUMPLICE;

        case PEDIDO_MARCAR_CARTA:
            return PROB_TRAPACA_MARCAR_CARTA;

        case PEDIDO_SUBORNO_MESA:
        case PEDIDO_SUBORNO_MAO:
        case PEDIDO_SUBORNO_VER:
            return PROB_TRAPACA_SUBORNO_DEALER;

        case PEDIDO_MANGA:
            return PROB_TRAPACA_CARTA_MANGA;

        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    return PROB_TRAPACA_TOTAL;
}

/** O adversário está embriagado enquanto restam mãos de bebedeira. */
static void sincronizar_embriaguez(Partida *partida)
{
    partida->adversario_embriagado = partida->cheats.maos_embriagado > 0;
}

/**
 * Prepara as trapaças para uma mesa nova: some a bebedeira, as marcas e o que
 * dependia do adversário anterior. A carta da manga e o suborno que ainda vai
 * chegar à mão do jogador ficam.
 */
static void normalizar_mesa(CheatState *cheats)
{
    cheats->maos_embriagado = 0;
    cheats->usos_cumplice_blind = 0;
    cheats->suborno_usado_na_mao = false;
    cheats->as_na_manga_usado = false;

    for (int i = 0; i < 5; i++) {
        cheats->cartas_marcadas[i].rank = 0;
        cheats->turnos_marcacao[i] = 0;
    }
}

/** Tira o suborno do estado, depois de ele ser entregue ou de virar um pedido à mão. */
static void limpar_suborno(CheatState *cheats)
{
    cheats->carta_comprada_pendente = false;
    cheats->destino_carta_comprada = -1;
    cheats->carta_comprada.rank = 0;
    cheats->carta_comprada.suit = POKER_CLUBS;
}

/** Coloca um EVENTO_TRAPACA na fila da partida. */
static void emitir_trapaca(Partida *partida, const EventoTrapaca *dados)
{
    EventoJogo evento = {.tipo = EVENTO_TRAPACA};

    evento.trapaca = *dados;
    fila_eventos_adicionar(&partida->eventos, &evento);
}

/** Coloca um EVENTO_SEGUNDA_VIDA na fila: o adversário acabou de voltar com as fichas novas. */
static void emitir_segunda_vida(Partida *partida)
{
    EventoJogo evento = {.tipo = EVENTO_SEGUNDA_VIDA};

    evento.segunda_vida.confronto = partida->torneio.confronto;
    evento.segunda_vida.fichas = partida->torneio.fichas_adversario;
    fila_eventos_adicionar(&partida->eventos, &evento);
}

/**
 * Ajusta a situação da partida ao que aconteceu na mão. Se a mão acabou,
 * registra o resultado no torneio antes de olhar para ele e deixa as
 * trapaças envelhecerem uma mão (bebedeira e marcas).
 *
 * @return false se o torneio recusou o resultado da mão.
 */
static bool atualizar_situacao(Partida *partida)
{
    Mao *mao = &partida->mao;

    if (mao->situacao == MAO_ENCERRADA) {
        bool segunda_vida_antes = partida->torneio.segunda_vida_usada;

        if (!mao_registrar_no_torneio(mao, &partida->torneio)) {
            return false;
        }

        /* O torneio devolveu o chefe à mesa com a segunda vida: a interface precisa saber. */
        if (!segunda_vida_antes && partida->torneio.segunda_vida_usada) {
            emitir_segunda_vida(partida);
        }

        cheat_update_end_of_hand(&partida->cheats);
        sincronizar_embriaguez(partida);
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
            if (partida->torneio.recuperacoes < PARTIDA_RECUPERACOES_MAXIMAS) {
                partida->situacao = PARTIDA_ROLETA;
                partida->roleta_motivo = ROLETA_FALENCIA;
            } else {
                partida->situacao = PARTIDA_JOGADOR_FALIDO;
            }
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

/** Avisa quais cartas marcadas pelo jogador estão na mão do adversário. */
static void avisar_marcadas_do_adversario(Partida *partida)
{
    for (int i = 0; i < 2; i++) {
        PokerCard carta = partida->mao.cartas[TORNEIO_ASSENTO_ADVERSARIO][i];

        if (is_card_marked(&partida->cheats, carta)) {
            EventoTrapaca dados = {
                .pedido = PEDIDO_MARCAR_CARTA,
                .resultado = TRAPACA_SUCESSO,
                .na_distribuicao = true,
                .quantidade_de_cartas = 1
            };

            dados.cartas[0] = carta;
            emitir_trapaca(partida, &dados);
        }
    }
}

/**
 * Abre a mão do torneio atual com a fila de eventos da partida e ajusta a
 * situação da partida a ela. O registro da mão recomeça vazio. A carta da
 * manga fica fora do baralho, o suborno que esperava esta mão entrega a carta
 * ao jogador e as cartas marcadas que caíram com o adversário são avisadas.
 */
static bool abrir_mao(Partida *partida)
{
    CheatState *cheats = &partida->cheats;
    MaoAjustes ajustes = {
        .reservar_carta = true,
        .carta_reservada = cheats->carta_na_manga
    };
    bool suborno = cheats->carta_comprada_pendente && cheats->destino_carta_comprada == 1;

    partida->roleta_motivo = ROLETA_NENHUMA;
    partida->quantidade_no_registro = 0;
    partida->registro_perdido = false;
    sincronizar_embriaguez(partida);

    if (suborno) {
        ajustes.dar_carta_ao_jogador = true;
        ajustes.carta_para_o_jogador = cheats->carta_comprada;
    }

    if (!mao_iniciar_com_ajustes(&partida->mao, &partida->torneio, &partida->eventos, &ajustes)) {
        return false;
    }

    if (suborno) {
        EventoTrapaca dados = {
            .pedido = PEDIDO_SUBORNO_MAO,
            .resultado = partida->mao.carta_do_jogador == MAO_AJUSTE_APLICADO
                ? TRAPACA_SUCESSO : TRAPACA_RECUSADA,
            .na_distribuicao = true,
            .quantidade_de_cartas = 1
        };

        dados.cartas[0] = cheats->carta_comprada;
        emitir_trapaca(partida, &dados);
        limpar_suborno(cheats);
    }

    avisar_marcadas_do_adversario(partida);
    return atualizar_situacao(partida);
}

/**
 * Sorteia o próximo número de um gerador da partida (splitmix64), que avança
 * o estado a cada sorteio. O rand global não serve: ele é compartilhado entre
 * partidas e dá só 15 bits no Windows.
 */
static unsigned proximo_sorteio(uint64_t *estado)
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
 * Inicia a mão seguinte do torneio. Guarda o torneio e o risco como estão e
 * sorteia a semente do baralho da mão, para que ela possa ser refeita depois.
 */
static bool iniciar_mao(Partida *partida)
{
    /* Os usos do cúmplice valem por nível de blind: um nível novo recomeça a conta. */
    if (partida->torneio.maos_jogadas % TORNEIO_MAOS_POR_NIVEL == 0) {
        partida->cheats.usos_cumplice_blind = 0;
    }

    partida->torneio_da_mao = partida->torneio;
    partida->ia.style = partida_estilo_do_adversario(&partida->torneio_da_mao);
    partida->cheats_da_mao = partida->cheats;
    partida->risco_da_mao = partida->risco;
    partida->estado_do_risco_da_mao = partida->estado_do_risco;
    partida->semente_da_mao = proximo_sorteio(&partida->estado_das_sementes);
    srand(partida->semente_da_mao);
    return abrir_mao(partida);
}

/** Acrescenta uma entrada ao registro da mão, ou marca o registro como perdido se cheio. */
static void registrar(Partida *partida, RegistroDaMao entrada)
{
    if (partida->quantidade_no_registro >= PARTIDA_REGISTRO_MAXIMO) {
        partida->registro_perdido = true;
        return;
    }

    partida->registro[partida->quantidade_no_registro++] = entrada;
}

/** Registra uma ação do motor que acabou de ser aplicada. */
static void registrar_acao(Partida *partida, PokerAction acao)
{
    registrar(partida, (RegistroDaMao){.tipo = REGISTRO_ACAO, .acao = acao});
}

/**
 * Avisa o que aconteceu com a carta que o suborno pôs na mesa, se a rua dela
 * acabou de ser distribuída.
 */
static void avisar_carta_forcada(Partida *partida)
{
    Mao *mao = &partida->mao;
    EventoTrapaca dados = {
        .pedido = PEDIDO_SUBORNO_MESA,
        .na_distribuicao = true,
        .quantidade_de_cartas = 1
    };

    if (mao->carta_forcada_resultado == MAO_AJUSTE_NENHUM) {
        return;
    }

    dados.resultado = mao->carta_forcada_resultado == MAO_AJUSTE_APLICADO
        ? TRAPACA_SUCESSO : TRAPACA_RECUSADA;
    dados.cartas[0] = mao->carta_forcada;
    emitir_trapaca(partida, &dados);
    mao->carta_forcada_resultado = MAO_AJUSTE_NENHUM;
}

/**
 * Aplica uma ação do motor na mão, registra e ajusta a situação da partida.
 * Serve ao jogador e a quem refaz a mão guardada.
 */
static PokerStatus aplicar_acao(Partida *partida, PokerAction acao)
{
    PokerStatus status = mao_agir(&partida->mao, acao);

    if (status != POKER_OK) {
        return status;
    }

    registrar_acao(partida, acao);
    avisar_carta_forcada(partida);
    return atualizar_situacao(partida) ? POKER_OK : POKER_ERR_STATE;
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
    partida->estado_do_risco = (uint64_t)semente * 6u + 11u;
    prob_inicializar(&partida->risco);
    cheat_init_state(&partida->cheats);
    torneio_iniciar(&partida->torneio);

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
    if (partida == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_JOGADOR) {
        return partida->situacao == PARTIDA_VEZ_DO_ADVERSARIO
            ? POKER_ERR_TURN
            : POKER_ERR_STATE;
    }

    return aplicar_acao(partida, acao);
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
    size_t acoes_antes;

    if (partida == NULL) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_ADVERSARIO) {
        return partida->situacao == PARTIDA_VEZ_DO_JOGADOR
            ? POKER_ERR_TURN
            : POKER_ERR_STATE;
    }

    acoes_antes = partida->mao.quantidade_de_acoes;
    status = mao_ia_agir(
        &partida->mao,
        &partida->ia,
        partida->adversario_embriagado,
        info_out
    );

    if (status != POKER_OK) {
        return status;
    }

    /* A mão guarda a ação que a IA escolheu; é de lá que o registro a copia. */
    if (partida->mao.quantidade_de_acoes > acoes_antes) {
        registrar_acao(partida, partida->mao.acoes[acoes_antes]);
    } else {
        partida->registro_perdido = true;
    }

    avisar_carta_forcada(partida);

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

        prob_avancar_mesa(&partida->risco);
        normalizar_mesa(&partida->cheats);
        return iniciar_mao(partida);
    }

    if (partida->situacao == PARTIDA_MAO_ENCERRADA) {
        return iniciar_mao(partida);
    }

    return false;
}

bool partida_puxar_gatilho(Partida *partida)
{
    EventoJogo evento = {.tipo = EVENTO_ROLETA};
    bool morreu;

    if (partida == NULL || partida->situacao != PARTIDA_ROLETA) {
        return false;
    }

    evento.roleta.por_falencia = partida->roleta_motivo == ROLETA_FALENCIA;
    evento.roleta.balas_antes = partida->risco.balas_roleta;

    /* A roleta sorteia pelo rand; semeá-lo com o gerador da partida a deixa reproduzível. */
    srand(proximo_sorteio(&partida->estado_do_risco));
    morreu = prob_jogar_roleta_russa(&partida->risco);

    evento.roleta.morreu = morreu;
    evento.roleta.balas_depois = partida->risco.balas_roleta;
    fila_eventos_adicionar(&partida->eventos, &evento);
    registrar(partida, (RegistroDaMao){.tipo = REGISTRO_GATILHO});

    if (morreu) {
        partida->situacao = PARTIDA_JOGADOR_MORTO;
    } else if (partida->roleta_motivo == ROLETA_FALENCIA) {
        torneio_recuperar_jogador(&partida->torneio);
        partida->situacao = PARTIDA_MAO_ENCERRADA;
    } else {
        partida->situacao = PARTIDA_VEZ_DO_JOGADOR;
    }

    partida->roleta_motivo = ROLETA_NENHUMA;
    return true;
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
            return "Oponente 1";

        case 1:
            return "Oponente 2";

        case 2:
            return "Oponente 3";

        default:
            return "?";
    }
}

PokerChips partida_custo_da_trapaca(const Partida *partida, TipoDePedido tipo)
{
    PokerChips base;

    if (partida == NULL) {
        return 0;
    }

    base = torneio_fichas_iniciais_adversario(partida->torneio.confronto) / PARTIDA_SUBORNO_DIVISOR;

    switch (tipo) {
        case PEDIDO_CUMPLICE:
            return partida->mao.mesa.big_blind * (partida->cheats.usos_cumplice_blind + 1);

        case PEDIDO_SUBORNO_MESA:
            return base;

        case PEDIDO_SUBORNO_MAO:
            return base * 2;

        case PEDIDO_SUBORNO_VER:
            return base * 3;

        case PEDIDO_NENHUM:
        case PEDIDO_BEBIDA:
        case PEDIDO_MARCAR_CARTA:
        case PEDIDO_MANGA:
        case PEDIDO_TOTAL:
            break;
    }

    return 0;
}

int partida_chance_de_descoberta(const Partida *partida, TipoDePedido tipo)
{
    TipoTrapacaProb risco = tipo_de_risco(tipo);

    if (partida == NULL || risco == PROB_TRAPACA_TOTAL) {
        return -1;
    }

    return prob_calcular_chance_descoberta(
        &partida->risco,
        risco,
        perfil_do_adversario(partida),
        partida->cheats.maos_embriagado > 0
    );
}

/** Quantos pedidos do tipo já foram feitos na mão atual. */
static int pedidos_na_mao(const Partida *partida, TipoDePedido tipo)
{
    int total = 0;

    for (size_t i = 0; i < partida->quantidade_no_registro; i++) {
        if (partida->registro[i].tipo == REGISTRO_TRAPACA &&
            partida->registro[i].pedido.tipo == tipo) {
            total++;
        }
    }

    return total;
}

/** Diz se uma carta já está à vista do jogador: na mão dele, no board ou na manga. */
static bool carta_a_vista(const Partida *partida, PokerCard carta)
{
    const Mao *mao = &partida->mao;

    if (mesma_carta(carta, partida->cheats.carta_na_manga)) {
        return true;
    }

    for (int i = 0; i < 2; i++) {
        if (mesma_carta(carta, mao->cartas[TORNEIO_ASSENTO_JOGADOR][i])) {
            return true;
        }
    }

    for (size_t i = 0; i < mao->cartas_no_board; i++) {
        if (mesma_carta(carta, mao->board[i])) {
            return true;
        }
    }

    return false;
}

PokerStatus partida_pode_trapacear(const Partida *partida, PedidoTrapaca pedido)
{
    const Mao *mao;
    PokerChips custo;
    bool suborno;

    if (partida == NULL || pedido.tipo <= PEDIDO_NENHUM || pedido.tipo >= PEDIDO_TOTAL) {
        return POKER_ERR_ARGUMENT;
    }

    if ((pedido.tipo == PEDIDO_MARCAR_CARTA || pedido.tipo == PEDIDO_MANGA) &&
        pedido.carta_da_mao != 0 && pedido.carta_da_mao != 1) {
        return POKER_ERR_ARGUMENT;
    }

    if ((pedido.tipo == PEDIDO_SUBORNO_MESA || pedido.tipo == PEDIDO_SUBORNO_MAO) &&
        !carta_valida(pedido.carta)) {
        return POKER_ERR_ARGUMENT;
    }

    if (partida->situacao != PARTIDA_VEZ_DO_JOGADOR) {
        return partida->situacao == PARTIDA_VEZ_DO_ADVERSARIO ? POKER_ERR_TURN : POKER_ERR_STATE;
    }

    mao = &partida->mao;
    custo = partida_custo_da_trapaca(partida, pedido.tipo);
    suborno = pedido.tipo == PEDIDO_SUBORNO_MESA || pedido.tipo == PEDIDO_SUBORNO_MAO ||
              pedido.tipo == PEDIDO_SUBORNO_VER;

    /* Pagar nunca pode deixar o jogador sem fichas: isso o poria all-in sem ter apostado. */
    if (custo > 0 && mao->mesa.player[TORNEIO_ASSENTO_JOGADOR].chips - custo < 1) {
        return POKER_ERR_CHIPS;
    }

    if (suborno && partida->cheats.suborno_usado_na_mao) {
        return POKER_ERR_ACTION;
    }

    switch (pedido.tipo) {
        case PEDIDO_MARCAR_CARTA:
        {
            int livres = 0;

            for (int i = 0; i < 5; i++) {
                livres += partida->cheats.cartas_marcadas[i].rank == 0 ? 1 : 0;
            }

            if (livres == 0 ||
                is_card_marked(&partida->cheats, mao->cartas[TORNEIO_ASSENTO_JOGADOR][pedido.carta_da_mao]) ||
                pedidos_na_mao(partida, PEDIDO_MARCAR_CARTA) > 0) {
                return POKER_ERR_ACTION;
            }

            break;
        }

        case PEDIDO_SUBORNO_MESA:
            if (mao->mesa.street >= POKER_RIVER || mao->cartas_no_board >= MAO_CARTAS_NO_BOARD ||
                mao->carta_forcada_pendente || carta_a_vista(partida, pedido.carta)) {
                return POKER_ERR_ACTION;
            }

            break;

        case PEDIDO_SUBORNO_MAO:
            if (partida->cheats.carta_comprada_pendente || carta_a_vista(partida, pedido.carta)) {
                return POKER_ERR_ACTION;
            }

            break;

        case PEDIDO_MANGA:
            if (!carta_valida(partida->cheats.carta_na_manga)) {
                return POKER_ERR_ACTION;
            }

            break;

        case PEDIDO_NENHUM:
        case PEDIDO_BEBIDA:
        case PEDIDO_CUMPLICE:
        case PEDIDO_SUBORNO_VER:
        case PEDIDO_TOTAL:
            break;
    }

    return POKER_OK;
}

/** Categoria da melhor mão do adversário com o que está à vista: o board, ou só as duas cartas. */
static PokerCategory categoria_do_adversario(const Mao *mao)
{
    const PokerCard *cartas = mao->cartas[TORNEIO_ASSENTO_ADVERSARIO];
    PokerHandValue valor;

    if (mao->cartas_no_board >= 3 &&
        poker_evaluate_holdem(cartas, mao->board, mao->cartas_no_board, &valor) == POKER_OK) {
        return valor.category;
    }

    return cartas[0].rank == cartas[1].rank ? POKER_ONE_PAIR : POKER_HIGH_CARD;
}

/**
 * Executa um pedido já validado: sorteia a recusa do dealer (só no suborno) e
 * a descoberta, chama a trapaça de gameplay/trapacas.h, acerta o total da mão
 * com o que foi pago e aplica o efeito. Os dois sorteios saem sempre, e nessa
 * ordem, para que refazer a mão gaste o gerador igual.
 */
static CheatResult executar_pedido(Partida *partida, PedidoTrapaca pedido)
{
    Mao *mao = &partida->mao;
    PokerTable *mesa = &mao->mesa;
    CheatState *cheats = &partida->cheats;
    int assento = TORNEIO_ASSENTO_JOGADOR;
    PokerChips base = torneio_fichas_iniciais_adversario(partida->torneio.confronto) /
                      PARTIDA_SUBORNO_DIVISOR;
    PokerChips fichas_antes[2] = {mesa->player[0].chips, mesa->player[1].chips};
    int chance = partida_chance_de_descoberta(partida, pedido.tipo);
    bool suborno = pedido.tipo == PEDIDO_SUBORNO_MESA || pedido.tipo == PEDIDO_SUBORNO_MAO ||
                   pedido.tipo == PEDIDO_SUBORNO_VER;
    bool recusou = false;
    bool pego;
    CheatResult resultado = TRAPACA_RECUSADA;
    EventoTrapaca dados = {.pedido = pedido.tipo};
    PokerCard sem_carta = {0, POKER_CLUBS};

    if (suborno) {
        recusou = proximo_sorteio(&partida->estado_do_risco) % 100u <
                  (unsigned)PARTIDA_CHANCE_DO_DEALER_RECUSAR;
    }

    pego = proximo_sorteio(&partida->estado_do_risco) % 100u + 1u <= (unsigned)chance;

    switch (pedido.tipo) {
        case PEDIDO_BEBIDA:
            resultado = cheat_spike_drink(cheats, pego);
            break;

        case PEDIDO_CUMPLICE:
            resultado = cheat_signal_accomplice(cheats, mesa, assento, pego);
            break;

        case PEDIDO_MARCAR_CARTA:
            resultado = cheat_mark_card(cheats, mao->cartas[assento][pedido.carta_da_mao],
                                        PARTIDA_MARCA_DURACAO, pego);
            break;

        case PEDIDO_SUBORNO_MESA:
            resultado = cheat_bribe_dealer(cheats, mesa, assento, SUBORNO_CARTA_MESA, base,
                                           recusou, pego, pedido.carta);
            break;

        case PEDIDO_SUBORNO_MAO:
            resultado = cheat_bribe_dealer(cheats, mesa, assento, SUBORNO_CARTA_MAO_FUTURA, base,
                                           recusou, pego, pedido.carta);
            break;

        case PEDIDO_SUBORNO_VER:
            resultado = cheat_bribe_dealer(cheats, mesa, assento, SUBORNO_VER_CARTA_OPONENTE, base,
                                           recusou, pego, sem_carta);
            break;

        case PEDIDO_MANGA:
            resultado = cheat_swap_sleeve_card(cheats, &mao->cartas[assento][pedido.carta_da_mao], pego);
            break;

        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    /* As fichas pagas saem de circulação: o total da mão desce junto com a pilha. */
    for (int i = 0; i < 2; i++) {
        PokerChips gasto = fichas_antes[i] - mesa->player[i].chips;

        if (gasto > 0) {
            mesa->initial_total -= gasto;
            dados.custo += gasto;
        }
    }

    dados.resultado = resultado;

    if (resultado == TRAPACA_DESCOBERTA) {
        partida->situacao = PARTIDA_ROLETA;
        partida->roleta_motivo = ROLETA_DESCOBERTO;
    } else if (resultado == TRAPACA_SUCESSO) {
        prob_registrar_uso_trapaca(&partida->risco, tipo_de_risco(pedido.tipo));

        switch (pedido.tipo) {
            case PEDIDO_BEBIDA:
                sincronizar_embriaguez(partida);
                break;

            case PEDIDO_CUMPLICE:
                dados.tem_categoria = true;
                dados.categoria = categoria_do_adversario(mao);
                break;

            case PEDIDO_MARCAR_CARTA:
                dados.cartas[0] = mao->cartas[assento][pedido.carta_da_mao];
                dados.quantidade_de_cartas = 1;
                break;

            case PEDIDO_SUBORNO_MESA:
                /* O suborno vira um pedido à mão, que o aplica quando a rua for distribuída. */
                (void)mao_forcar_proxima_carta_do_board(mao, pedido.carta);
                limpar_suborno(cheats);
                dados.cartas[0] = pedido.carta;
                dados.quantidade_de_cartas = 1;
                break;

            case PEDIDO_SUBORNO_MAO:
                dados.cartas[0] = pedido.carta;
                dados.quantidade_de_cartas = 1;
                break;

            case PEDIDO_SUBORNO_VER:
                dados.cartas[0] = mao->cartas[TORNEIO_ASSENTO_ADVERSARIO][0];
                dados.cartas[1] = mao->cartas[TORNEIO_ASSENTO_ADVERSARIO][1];
                dados.quantidade_de_cartas = 2;
                break;

            case PEDIDO_MANGA:
                dados.cartas[0] = mao->cartas[assento][pedido.carta_da_mao];
                dados.cartas[1] = cheats->carta_na_manga;
                dados.quantidade_de_cartas = 2;
                break;

            case PEDIDO_NENHUM:
            case PEDIDO_TOTAL:
                break;
        }
    }

    emitir_trapaca(partida, &dados);
    return resultado;
}

/** Zera o que o tipo de pedido não usa, para o registro guardar só valores que o salvamento aceita. */
static PedidoTrapaca normalizar_pedido(PedidoTrapaca pedido)
{
    if (pedido.tipo != PEDIDO_MARCAR_CARTA && pedido.tipo != PEDIDO_MANGA) {
        pedido.carta_da_mao = 0;
    }

    if (pedido.tipo != PEDIDO_SUBORNO_MESA && pedido.tipo != PEDIDO_SUBORNO_MAO) {
        pedido.carta = (PokerCard){0, POKER_CLUBS};
    }

    return pedido;
}

PokerStatus partida_trapacear(Partida *partida, PedidoTrapaca pedido, CheatResult *resultado)
{
    PokerStatus status = partida_pode_trapacear(partida, pedido);
    CheatResult obtido;

    if (status != POKER_OK) {
        return status;
    }

    pedido = normalizar_pedido(pedido);

    obtido = executar_pedido(partida, pedido);

    if (resultado != NULL) {
        *resultado = obtido;
    }

    registrar(partida, (RegistroDaMao){.tipo = REGISTRO_TRAPACA, .pedido = pedido});
    return POKER_OK;
}

/** Diz se a partida está num ponto em que pode ser guardada e continuada. */
static bool situacao_continuavel(PartidaSituacao situacao)
{
    switch (situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
        case PARTIDA_VEZ_DO_ADVERSARIO:
        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
        case PARTIDA_ROLETA:
            return true;

        case PARTIDA_VAZIA:
        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_CAMPEAO:
        case PARTIDA_JOGADOR_MORTO:
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
        partida->registro_perdido) {
        return false;
    }

    mao = &partida->mao;
    copia.torneio = partida->torneio_da_mao;
    copia.semente_da_mao = partida->semente_da_mao;
    copia.estado_das_sementes = partida->estado_das_sementes;
    copia.rng_acao = partida->rng_acao.state;
    copia.rng_equidade = partida->rng_equidade.state;
    copia.risco = partida->risco_da_mao;
    copia.estado_do_risco = partida->estado_do_risco_da_mao;
    copia.cheats = partida->cheats_da_mao;
    copia.situacao = partida->situacao;
    memcpy(copia.cartas, mao->cartas, sizeof copia.cartas);
    memcpy(copia.board, mao->board, sizeof copia.board);
    copia.cartas_no_board = mao->cartas_no_board;
    copia.fichas[0] = mao->mesa.player[0].chips;
    copia.fichas[1] = mao->mesa.player[1].chips;
    copia.quantidade_no_registro = partida->quantidade_no_registro;
    memcpy(copia.registro, partida->registro,
           partida->quantidade_no_registro * sizeof partida->registro[0]);

    *salva = copia;
    return true;
}

/** Confere se o risco guardado está dentro do que o jogo pode produzir. */
static bool risco_valido(const SistemaRisco *risco)
{
    if (risco->balas_roleta < 1 || risco->balas_roleta > 6) {
        return false;
    }

    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        if (risco->usos_trapaca[i] < 0 || risco->usos_trapaca[i] > PARTIDA_USOS_MAXIMOS) {
            return false;
        }
    }

    return true;
}

/** Confere se o estado das trapaças guardado está dentro do que o jogo pode produzir. */
static bool cheats_validos(const CheatState *cheats)
{
    if (cheats->maos_embriagado < 0 || cheats->maos_embriagado > PARTIDA_USOS_MAXIMOS ||
        cheats->usos_cumplice_blind < 0 || cheats->usos_cumplice_blind > PARTIDA_USOS_MAXIMOS ||
        !carta_valida(cheats->carta_na_manga) ||
        cheats->destino_carta_comprada < -1 || cheats->destino_carta_comprada > 1) {
        return false;
    }

    /* Só o suborno da mão espera entre mãos; sem ele, não sobra carta comprada. */
    if (cheats->carta_comprada_pendente) {
        if (cheats->destino_carta_comprada != 1 || !carta_valida(cheats->carta_comprada)) {
            return false;
        }
    } else if (cheats->destino_carta_comprada != -1 || cheats->carta_comprada.rank != 0) {
        return false;
    }

    for (int i = 0; i < 5; i++) {
        int turnos = cheats->turnos_marcacao[i];

        if (cheats->cartas_marcadas[i].rank == 0) {
            if (turnos != 0) {
                return false;
            }
        } else if (!carta_valida(cheats->cartas_marcadas[i]) ||
                   turnos < 1 || turnos > PARTIDA_MARCA_DURACAO) {
            return false;
        }
    }

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
           (!torneio->segunda_vida_usada || torneio_segunda_vida(torneio->confronto) > 0) &&
           salva->rng_acao != 0 && salva->rng_equidade != 0 &&
           risco_valido(&salva->risco) &&
           cheats_validos(&salva->cheats) &&
           situacao_continuavel(salva->situacao) &&
           salva->quantidade_no_registro <= PARTIDA_REGISTRO_MAXIMO &&
           salva->cartas_no_board <= MAO_CARTAS_NO_BOARD;
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
    partida->risco = salva->risco;
    partida->estado_do_risco = salva->estado_do_risco;
    partida->risco_da_mao = salva->risco;
    partida->estado_do_risco_da_mao = salva->estado_do_risco;
    partida->cheats = salva->cheats;
    partida->cheats_da_mao = salva->cheats;
    partida->ia.style = partida_estilo_do_adversario(&partida->torneio_da_mao);

    srand(salva->semente_da_mao);
    refeita = abrir_mao(partida);

    for (size_t i = 0; refeita && i < salva->quantidade_no_registro; i++) {
        const RegistroDaMao *entrada = &salva->registro[i];

        switch (entrada->tipo) {
            case REGISTRO_ACAO:
                refeita = partida->mao.situacao == MAO_EM_ANDAMENTO &&
                          aplicar_acao(partida, entrada->acao) == POKER_OK;
                break;

            case REGISTRO_GATILHO:
                refeita = partida_puxar_gatilho(partida);
                break;

            case REGISTRO_TRAPACA:
                refeita = partida_trapacear(partida, entrada->pedido, NULL) == POKER_OK;
                break;

            default:
                refeita = false;
                break;
        }
    }

    if (refeita && mao_igual_a_guardada(partida, salva)) {
        return true;
    }

    partida_encerrar(partida);
    return false;
}
