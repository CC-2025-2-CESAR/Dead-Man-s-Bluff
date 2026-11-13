#ifndef PARTIDA_H
#define PARTIDA_H

#include <stdbool.h>

#include "core/eventos.h"
#include "core/mao.h"
#include "core/mao_ia.h"
#include "core/torneio.h"
#include "gameplay/poker_ai_config.h"
#include "poker.h"

/**
 * Situação da partida, que une a mão em andamento e o torneio.
 *
 * PARTIDA_VAZIA: nenhuma partida iniciada (estado de uma Partida zerada).
 * PARTIDA_VEZ_DO_JOGADOR: o jogador precisa agir; use partida_agir_jogador.
 * PARTIDA_VEZ_DO_ADVERSARIO: a IA precisa agir; use partida_agir_adversario.
 * PARTIDA_MAO_ENCERRADA: a mão acabou e o torneio continua; use
 * partida_continuar para começar a próxima.
 * PARTIDA_CONFRONTO_VENCIDO: o adversário ficou sem fichas; use
 * partida_continuar para enfrentar o próximo.
 * PARTIDA_JOGADOR_FALIDO: o jogador ficou sem fichas. A roleta ainda não
 * existe, então a partida acaba aqui.
 * PARTIDA_CAMPEAO: o último adversário foi derrotado e a partida acabou.
 */
typedef enum {
    PARTIDA_VAZIA = 0,
    PARTIDA_VEZ_DO_JOGADOR,
    PARTIDA_VEZ_DO_ADVERSARIO,
    PARTIDA_MAO_ENCERRADA,
    PARTIDA_CONFRONTO_VENCIDO,
    PARTIDA_JOGADOR_FALIDO,
    PARTIDA_CAMPEAO
} PartidaSituacao;

/**
 * O que o jogador pede na sua vez. A interface só traduz teclas ou botões
 * para um destes comandos; quem decide o que ele significa é o core.
 *
 * COMANDO_NENHUM: o jogador não pediu nada.
 * COMANDO_DESISTIR: desistir da mão.
 * COMANDO_PASSAR_OU_PAGAR: passar se não há aposta a pagar, pagar se há.
 * COMANDO_APOSTAR: apostar ou aumentar até um total escolhido.
 * COMANDO_ALL_IN: apostar todas as fichas.
 */
typedef enum {
    COMANDO_NENHUM = 0,
    COMANDO_DESISTIR,
    COMANDO_PASSAR_OU_PAGAR,
    COMANDO_APOSTAR,
    COMANDO_ALL_IN
} ComandoJogador;

/**
 * Partida completa: torneio, mão atual, fila de eventos para a interface e
 * a IA do adversário.
 *
 * torneio: confronto, saldos, blinds e dealer.
 * mao: mão atual; depois do fim dela guarda o resultado até a próxima.
 * eventos: fila que a interface consome a cada quadro (ver eventos.h).
 * rng_acao e rng_equidade: geradores próprios da IA, que duram a partida.
 * ia: contexto da IA, com o perfil do confronto atual.
 * adversario_embriagado: modo embriagado da IA; fica para as trapaças
 * ligarem e hoje é sempre falso.
 * situacao: situação atual da partida.
 *
 * Não copie uma Partida: o contexto da IA aponta para os geradores que ela
 * guarda. Zere com Partida partida = {0}; antes de partida_iniciar.
 */
typedef struct {
    Torneio torneio;
    Mao mao;
    FilaEventos eventos;
    PokerAIRng rng_acao;
    PokerAIRng rng_equidade;
    PokerAIContext ia;
    bool adversario_embriagado;
    PartidaSituacao situacao;
} Partida;

/**
 * Começa uma partida nova: semeia o gerador do baralho com srand, prepara
 * o torneio e a IA do primeiro adversário e inicia a primeira mão. Depois
 * da chamada, confira Partida.situacao: se um stack for menor ou igual ao
 * blind, a mão já se resolve e a partida começa em PARTIDA_MAO_ENCERRADA.
 *
 * @param partida partida zerada ou já usada.
 * @param semente semente do baralho e da IA; a mesma semente com as mesmas
 * ações reproduz a mesma partida.
 * @return true se a partida começou.
 */
bool partida_iniciar(Partida *partida, unsigned semente);

/**
 * Libera o que a partida guarda, como o baralho de uma mão em andamento.
 * Pode ser chamada em qualquer situação e mais de uma vez.
 *
 * @param partida partida a encerrar.
 */
void partida_encerrar(Partida *partida);

/**
 * Aplica a ação do jogador. Quando a mão termina, registra o resultado no
 * torneio e atualiza a situação da partida.
 *
 * @param partida partida em PARTIDA_VEZ_DO_JOGADOR.
 * @param acao ação escolhida pelo jogador.
 * @return POKER_OK se a ação foi aplicada; POKER_ERR_TURN se a vez é da IA;
 * POKER_ERR_STATE se não há mão em andamento; os demais códigos vêm do
 * motor, e uma ação recusada não altera nada.
 */
PokerStatus partida_agir_jogador(Partida *partida, PokerAction acao);

/**
 * Traduz um comando do jogador para uma ação do motor e a aplica, como
 * partida_agir_jogador.
 *
 * @param partida partida em PARTIDA_VEZ_DO_JOGADOR.
 * @param comando o que o jogador pediu.
 * @param total para COMANDO_APOSTAR, o total que o jogador quer ter colocado
 * na rodada (use partida_ajustar_aposta para escolher um valor válido); nos
 * demais comandos é ignorado.
 * @return POKER_OK se a ação foi aplicada; POKER_ERR_ARGUMENT para
 * COMANDO_NENHUM; POKER_ERR_ACTION se o comando não vale agora (por exemplo,
 * apostar sem poder, ou um total fora dos limites); os demais códigos são
 * os de partida_agir_jogador.
 */
PokerStatus partida_comando_jogador(Partida *partida, ComandoJogador comando, PokerChips total);

/**
 * Move o total de uma aposta ou aumento em passos de um big blind, sempre
 * dentro dos limites legais. Um total que ainda não é válido (como zero)
 * vira o menor total permitido.
 *
 * @param partida partida em PARTIDA_VEZ_DO_JOGADOR.
 * @param atual total escolhido até agora.
 * @param passos quantos big blinds somar; negativo para diminuir.
 * @return o novo total, entre o mínimo e o máximo permitidos; 0 se o
 * jogador não pode apostar agora (não é a vez dele ou o adversário já está
 * all-in).
 */
PokerChips partida_ajustar_aposta(const Partida *partida, PokerChips atual, int passos);

/**
 * Pede a ação à IA do confronto atual e a aplica. A interface chama esta
 * função no ritmo que quiser, para o adversário não responder de imediato.
 *
 * @param partida partida em PARTIDA_VEZ_DO_ADVERSARIO.
 * @param info_out dados da decisão para depuração, ou NULL.
 * @return POKER_OK se a IA agiu; POKER_ERR_TURN se a vez é do jogador;
 * POKER_ERR_STATE se não há mão em andamento.
 */
PokerStatus partida_agir_adversario(Partida *partida, PokerAIDecisionInfo *info_out);

/**
 * Passa para a mão seguinte depois do fim de uma mão. Se o adversário foi
 * derrotado, antes disso avança o confronto, troca o perfil da IA e
 * reinicia os blinds.
 *
 * @param partida partida em PARTIDA_MAO_ENCERRADA ou PARTIDA_CONFRONTO_VENCIDO.
 * @return true se a mão seguinte começou; false se a partida não está
 * esperando por isso (inclusive depois de falência ou título).
 */
bool partida_continuar(Partida *partida);

/**
 * Informa o que o jogador pode fazer agora.
 *
 * @param partida partida a consultar.
 * @param out recebe as ações legais.
 * @return POKER_OK se é a vez do jogador; POKER_ERR_TURN se é a da IA;
 * POKER_ERR_STATE se não há mão em andamento.
 */
PokerStatus partida_acoes_legais(const Partida *partida, PokerLegalActions *out);

/**
 * Dá o nome do adversário de um confronto: Agressivo, Medroso ou
 * Desconfiado.
 *
 * @param confronto índice do confronto, de 0 a TORNEIO_CONFRONTOS - 1.
 * @return o nome, ou "?" se o índice for inválido.
 */
const char *partida_nome_do_adversario(int confronto);

#endif
