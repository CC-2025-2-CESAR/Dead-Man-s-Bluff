#ifndef MAO_H
#define MAO_H

#include <stdbool.h>
#include <stddef.h>

#include "cards/deck.h"
#include "core/eventos.h"
#include "core/torneio.h"
#include "poker.h"

#define MAO_CARTAS_NO_BOARD 5

/**
 * Situação de uma mão.
 *
 * MAO_VAZIA: nenhuma mão em andamento (estado de uma Mao recém-zerada).
 * MAO_EM_ANDAMENTO: há uma rodada de apostas; age o assento mesa.actor.
 * MAO_ENCERRADA: o pote foi pago e o resultado está em Mao.resultado, mas as
 * fichas ainda não foram passadas ao torneio.
 * MAO_REGISTRADA: o resultado já entrou no torneio.
 */
typedef enum {
    MAO_VAZIA = 0,
    MAO_EM_ANDAMENTO,
    MAO_ENCERRADA,
    MAO_REGISTRADA
} MaoSituacao;

/**
 * Estado de uma mão de pôquer, do embaralhamento até o pagamento do pote.
 *
 * baralho: cartas que ainda não saíram. É liberado quando a mão termina.
 * mesa: estado do motor de pôquer (fichas, apostas, fase e assento da vez).
 * cartas: cartas de cada assento, indexadas pelo assento.
 * board: cartas comunitárias já distribuídas.
 * cartas_no_board: quantas posições de board estão preenchidas.
 * resultado: resultado do pagamento; só vale depois de MAO_ENCERRADA.
 * situacao: situação atual da mão.
 * eventos: fila que recebe os eventos da mão, ou NULL se ninguém os
 * consome. A fila pertence a quem a criou, não à Mao.
 *
 * Inicialize com Mao mao = {0}; antes da primeira chamada a mao_iniciar.
 */
typedef struct {
    Deck baralho;
    PokerTable mesa;
    PokerCard cartas[2][2];
    PokerCard board[MAO_CARTAS_NO_BOARD];
    size_t cartas_no_board;
    PokerResult resultado;
    MaoSituacao situacao;
    FilaEventos *eventos;
} Mao;

/**
 * Começa uma mão: embaralha, distribui as cartas e inicia o motor com o
 * dealer, os stacks e os blinds do torneio. O embaralhamento usa rand(), e
 * quem chama srand é o jogo.
 *
 * Se algum stack for menor ou igual ao blind, não há ninguém para agir: a
 * mão completa o board e paga o pote ainda dentro desta chamada. Por isso,
 * depois dela, confira Mao.situacao.
 *
 * Libera o baralho de uma mão anterior que tenha ficado aberta. Em caso de
 * falha devolve false e deixa a Mao vazia.
 *
 * Não registra eventos. Para animar a mão, use mao_iniciar_com_eventos.
 *
 * @param mao mão a iniciar, zerada ou já usada.
 * @param torneio torneio em andamento; a mão não o altera.
 * @return true se a mão começou; false se o torneio não está em andamento
 * ou se o baralho ou o motor recusaram a mão.
 */
bool mao_iniciar(Mao *mao, const Torneio *torneio);

/**
 * Faz o mesmo que mao_iniciar e registra na fila os eventos da mão inteira,
 * a começar pelos que acontecem dentro desta chamada: EVENTO_MAO_INICIADA,
 * as cartas de cada assento e, se a mão já se resolver sozinha, o board, o
 * showdown e o pagamento do pote. Os eventos seguintes vêm de mao_agir.
 *
 * @param mao mão a iniciar, zerada ou já usada.
 * @param torneio torneio em andamento; a mão não o altera.
 * @param eventos fila que recebe os eventos, ou NULL para não registrar.
 * @return true se a mão começou.
 */
bool mao_iniciar_com_eventos(
    Mao *mao,
    const Torneio *torneio,
    FilaEventos *eventos
);

/**
 * Aplica a ação do assento da vez (Mao.mesa.actor) e avança a mão até a
 * próxima decisão. Distribui o board a cada rodada, completa o board quando
 * há all-in antes do river e paga o pote com poker_settle ao fim.
 *
 * Uma ação recusada pelo motor não altera a mão e pode ser substituída por
 * outra. Se a mão tem fila de eventos, registra EVENTO_ACAO e os eventos que
 * a ação provocar (board, showdown e pagamento do pote).
 *
 * @param mao mão em andamento.
 * @param acao ação do assento da vez.
 * @return POKER_OK se a ação foi aplicada; POKER_ERR_STATE se não há mão em
 * andamento; os demais códigos vêm do motor.
 */
PokerStatus mao_agir(Mao *mao, PokerAction acao);

/**
 * Passa ao torneio as fichas finais dos dois assentos. Só vale uma vez por
 * mão e só depois que ela foi encerrada.
 *
 * @param mao mão encerrada; vira MAO_REGISTRADA quando o torneio aceita.
 * @param torneio torneio que recebe o resultado.
 * @return true se o torneio aceitou o resultado.
 */
bool mao_registrar_no_torneio(Mao *mao, Torneio *torneio);

/**
 * Libera o baralho de uma mão abandonada no meio, que volta a ser
 * MAO_VAZIA. Pode ser chamada em qualquer situação e mais de uma vez.
 *
 * @param mao mão a liberar.
 */
void mao_liberar(Mao *mao);

#endif
