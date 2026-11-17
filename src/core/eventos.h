#ifndef EVENTOS_H
#define EVENTOS_H

#include <stdbool.h>
#include <stddef.h>

#include "core/pedidos.h"
#include "poker.h"

#define FILA_EVENTOS_CAPACIDADE 256

/**
 * Tipos de evento que o core registra durante uma mão para a interface
 * animar. A interface não precisa ler o estado do motor para descobrir o
 * que mudou: basta consumir os eventos na ordem em que foram registrados.
 *
 * EVENTO_MAO_INICIADA: nova mão com blinds postados.
 * EVENTO_CARTAS_DISTRIBUIDAS: cartas de um assento (uma por assento).
 * EVENTO_ACAO: um assento apostou, pagou, passou, aumentou ou desistiu.
 * EVENTO_BOARD_REVELADO: nova rua do board (flop, turn ou river).
 * EVENTO_SHOWDOWN: as mãos são reveladas; só ocorre se ninguém desistiu.
 * EVENTO_POTE_PAGO: o pote foi pago e a mão terminou.
 * EVENTO_ROLETA: o jogador puxou o gatilho da roleta russa.
 * EVENTO_TRAPACA: uma trapaça foi tentada ou fez efeito.
 */
typedef enum {
    EVENTO_MAO_INICIADA = 0,
    EVENTO_CARTAS_DISTRIBUIDAS,
    EVENTO_ACAO,
    EVENTO_BOARD_REVELADO,
    EVENTO_SHOWDOWN,
    EVENTO_POTE_PAGO,
    EVENTO_ROLETA,
    EVENTO_TRAPACA
} TipoEvento;

/**
 * Dados de EVENTO_MAO_INICIADA.
 *
 * dealer: assento do dealer, que também é o small blind.
 * pequeno_blind e grande_blind: valores nominais dos blinds da mão.
 * fichas_postadas: o que cada assento realmente colocou como blind (um
 * stack curto coloca menos que o blind).
 * fichas_restantes: fichas de cada assento depois de postar o blind.
 */
typedef struct {
    int dealer;
    PokerChips pequeno_blind;
    PokerChips grande_blind;
    PokerChips fichas_postadas[2];
    PokerChips fichas_restantes[2];
} EventoMaoIniciada;

/**
 * Dados de EVENTO_CARTAS_DISTRIBUIDAS.
 *
 * assento: dono das cartas.
 * cartas: as duas cartas do assento.
 */
typedef struct {
    int assento;
    PokerCard cartas[2];
} EventoCartasDistribuidas;

/**
 * Dados de EVENTO_ACAO.
 *
 * assento: quem agiu.
 * acao: tipo da ação (desistir, passar, pagar, apostar, aumentar, all-in).
 * fichas_colocadas: fichas que saíram do stack nesta ação.
 * total_na_rua: tudo o que o assento colocou na rua depois da ação.
 * fichas_restantes: stack do assento depois da ação.
 * pote: pote total depois da ação.
 */
typedef struct {
    int assento;
    PokerActionType acao;
    PokerChips fichas_colocadas;
    PokerChips total_na_rua;
    PokerChips fichas_restantes;
    PokerChips pote;
} EventoAcao;

/**
 * Dados de EVENTO_BOARD_REVELADO.
 *
 * rua: rua revelada (flop, turn ou river).
 * cartas: cartas novas, nas primeiras posições.
 * quantidade: 3 no flop, 1 no turn e 1 no river.
 */
typedef struct {
    PokerStreet rua;
    PokerCard cartas[3];
    int quantidade;
} EventoBoardRevelado;

/**
 * Dados de EVENTO_SHOWDOWN.
 *
 * cartas: as duas cartas de cada assento, indexadas pelo assento.
 * valor: melhor mão de cada assento, com a categoria e as cinco cartas.
 */
typedef struct {
    PokerCard cartas[2][2];
    PokerHandValue valor[2];
} EventoShowdown;

/**
 * Dados de EVENTO_POTE_PAGO.
 *
 * vencedores: 1 se o assento 0 venceu, 2 se o assento 1 venceu, 3 se houve
 * empate (mesma convenção de PokerResult).
 * showdown: verdadeiro se a mão foi a showdown, falso se alguém desistiu.
 * pote: pote total da mão.
 * premio: o que cada assento ganhou do pote disputado.
 * devolucao: aposta não coberta devolvida a cada assento.
 * fichas_finais: stack de cada assento depois do pagamento.
 */
typedef struct {
    unsigned vencedores;
    bool showdown;
    PokerChips pote;
    PokerChips premio[2];
    PokerChips devolucao[2];
    PokerChips fichas_finais[2];
} EventoPotePago;

/**
 * Dados de EVENTO_ROLETA.
 *
 * por_falencia: true se a roleta veio de uma falência; false se veio de uma
 * trapaça descoberta.
 * morreu: true se a arma disparou e o jogador morreu.
 * balas_antes: balas no tambor antes do gatilho.
 * balas_depois: balas no tambor depois; sobreviver acrescenta uma.
 */
typedef struct {
    bool por_falencia;
    bool morreu;
    int balas_antes;
    int balas_depois;
} EventoRoleta;

/**
 * Dados de EVENTO_TRAPACA.
 *
 * pedido: qual trapaça.
 * resultado: TRAPACA_SUCESSO se fez efeito, TRAPACA_DESCOBERTA se o jogador
 * foi pego (a roleta vem em seguida) e TRAPACA_RECUSADA se o dealer pegou o
 * dinheiro e não fez o serviço.
 * na_distribuicao: true se o efeito veio quando as cartas foram dadas, e não
 * na hora do pedido: a carta do suborno entrando na mesa ou na mão, ou uma
 * carta marcada reconhecida na mão do adversário.
 * custo: fichas que o pedido tirou do jogador.
 * cartas e quantidade_de_cartas: as cartas envolvidas: a marcada, a recebida
 * do dealer, as duas do adversário (PEDIDO_SUBORNO_VER) ou a nova carta da
 * mão (PEDIDO_MANGA).
 * tem_categoria e categoria: no cúmplice, a categoria da melhor mão do
 * adversário com as cartas à vista.
 */
typedef struct {
    TipoDePedido pedido;
    CheatResult resultado;
    bool na_distribuicao;
    PokerChips custo;
    PokerCard cartas[2];
    int quantidade_de_cartas;
    bool tem_categoria;
    PokerCategory categoria;
} EventoTrapaca;

/**
 * Um evento do jogo. O campo tipo diz qual membro da união vale.
 */
typedef struct {
    TipoEvento tipo;
    union {
        EventoMaoIniciada mao_iniciada;
        EventoCartasDistribuidas cartas_distribuidas;
        EventoAcao acao;
        EventoBoardRevelado board;
        EventoShowdown showdown;
        EventoPotePago pote;
        EventoRoleta roleta;
        EventoTrapaca trapaca;
    };
} EventoJogo;

/**
 * Fila circular de eventos, do mais antigo para o mais novo.
 *
 * itens, inicio e quantidade: armazenamento interno; use as funções abaixo.
 * perdidos: quantos eventos foram descartados porque a fila estava cheia.
 * Se for maior que zero, a interface deve redesenhar a mesa a partir do
 * estado. Consumir a fila a cada quadro evita que isso aconteça.
 */
typedef struct {
    EventoJogo itens[FILA_EVENTOS_CAPACIDADE];
    size_t inicio;
    size_t quantidade;
    size_t perdidos;
} FilaEventos;

/**
 * Esvazia a fila e zera o contador de eventos perdidos.
 *
 * @param fila fila a iniciar.
 */
void fila_eventos_iniciar(FilaEventos *fila);

/**
 * Coloca um evento no fim da fila. Se a fila estiver cheia, o evento é
 * descartado e FilaEventos.perdidos aumenta.
 *
 * @param fila fila que recebe o evento.
 * @param evento evento a copiar para a fila.
 * @return true se o evento entrou na fila.
 */
bool fila_eventos_adicionar(FilaEventos *fila, const EventoJogo *evento);

/**
 * Tira da fila o evento mais antigo.
 *
 * @param fila fila a consumir.
 * @param saida recebe o evento retirado.
 * @return true se havia um evento; false se a fila estava vazia.
 */
bool fila_eventos_retirar(FilaEventos *fila, EventoJogo *saida);

/**
 * Informa quantos eventos esperam na fila.
 *
 * @param fila fila a consultar.
 * @return quantidade de eventos pendentes.
 */
size_t fila_eventos_tamanho(const FilaEventos *fila);

#endif
