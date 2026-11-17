#ifndef PARTIDA_H
#define PARTIDA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/eventos.h"
#include "core/mao.h"
#include "core/mao_ia.h"
#include "core/pedidos.h"
#include "core/torneio.h"
#include "gameplay/poker_ai_config.h"
#include "gameplay/probabilidade.h"
#include "gameplay/trapacas.h"
#include "poker.h"

/** Quantas vezes o jogador pode voltar de uma falência pela roleta. */
#define PARTIDA_RECUPERACOES_MAXIMAS 3

/** Quantas entradas cabem no registro de uma mão (ver RegistroDaMao). */
#define PARTIDA_REGISTRO_MAXIMO MAO_ACOES_MAXIMAS

/** Maior contagem de usos de uma trapaça que um salvamento pode trazer. */
#define PARTIDA_USOS_MAXIMOS 1000000

/** Quantas mãos uma carta marcada dura, contando a mão em que foi marcada. */
#define PARTIDA_MARCA_DURACAO 5

/**
 * Limites do estilo dinâmico do adversário, em % das fichas com que ele
 * começou o confronto. Com PARTIDA_ESTILO_ATRAS_PCT ou menos ele está
 * perdendo; com PARTIDA_ESTILO_FRENTE_PCT ou mais, ganhando.
 */
#define PARTIDA_ESTILO_ATRAS_PCT 60
#define PARTIDA_ESTILO_FRENTE_PCT 125

/** Chance, em %, de o dealer subornado pegar o dinheiro e não fazer o serviço. */
#define PARTIDA_CHANCE_DO_DEALER_RECUSAR 30

/**
 * O suborno custa as fichas iniciais do adversário do confronto divididas por
 * este valor (e vezes 1, 2 ou 3 conforme a ação). É "fixo por mesa e
 * crescente com o valor da mesa".
 */
#define PARTIDA_SUBORNO_DIVISOR 20

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
 * PARTIDA_JOGADOR_FALIDO: o jogador ficou sem fichas e já usou todas as
 * recuperações possíveis (PARTIDA_RECUPERACOES_MAXIMAS); a partida acabou.
 * PARTIDA_CAMPEAO: o último adversário foi derrotado e a partida acabou.
 * PARTIDA_ROLETA: a roleta russa espera o gatilho; use partida_puxar_gatilho.
 * Partida.roleta_motivo diz por que ela foi acionada.
 * PARTIDA_JOGADOR_MORTO: o jogador morreu na roleta e a partida acabou.
 */
typedef enum {
    PARTIDA_VAZIA = 0,
    PARTIDA_VEZ_DO_JOGADOR,
    PARTIDA_VEZ_DO_ADVERSARIO,
    PARTIDA_MAO_ENCERRADA,
    PARTIDA_CONFRONTO_VENCIDO,
    PARTIDA_JOGADOR_FALIDO,
    PARTIDA_CAMPEAO,
    PARTIDA_ROLETA,
    PARTIDA_JOGADOR_MORTO
} PartidaSituacao;

/**
 * Por que a roleta russa foi acionada.
 *
 * ROLETA_NENHUMA: a roleta não está em jogo.
 * ROLETA_FALENCIA: o jogador perdeu todas as fichas; se sobreviver, volta ao
 * confronto com fichas novas.
 * ROLETA_DESCOBERTO: uma trapaça foi descoberta; se sobreviver, a mão segue.
 */
typedef enum {
    ROLETA_NENHUMA = 0,
    ROLETA_FALENCIA,
    ROLETA_DESCOBERTO
} RoletaMotivo;

/**
 * O que a partida registra de uma mão, na ordem em que aconteceu, para poder
 * refazê-la depois (ver partida_exportar).
 *
 * REGISTRO_ACAO: uma ação do motor de pôquer, do jogador ou da IA.
 * REGISTRO_GATILHO: o jogador puxou o gatilho da roleta.
 * REGISTRO_TRAPACA: o jogador pediu uma trapaça (e ela foi tentada).
 */
typedef enum {
    REGISTRO_ACAO = 0,
    REGISTRO_GATILHO,
    REGISTRO_TRAPACA
} TipoDeRegistro;

/**
 * Uma entrada do registro da mão. REGISTRO_ACAO usa o campo acao e
 * REGISTRO_TRAPACA usa o campo pedido.
 */
typedef struct {
    TipoDeRegistro tipo;
    PokerAction acao;
    PedidoTrapaca pedido;
} RegistroDaMao;

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
 * ia: contexto da IA, com o estilo da mão atual (ver partida_estilo_do_adversario).
 * adversario_embriagado: modo embriagado da IA. Deriva de cheats: vale
 * enquanto restam mãos de bebedeira (cheats.maos_embriagado > 0).
 * situacao: situação atual da partida.
 * torneio_da_mao: o torneio como estava quando a mão atual começou. Com a
 * semente da mão e as ações da Mao, serve para refazer a mão (ver
 * partida_exportar).
 * semente_da_mao: semente do embaralhamento da mão atual.
 * estado_das_sementes: gerador que sorteia a semente de cada mão. É da
 * partida, e não o rand global, para duas partidas não se atrapalharem.
 * cheats: estado físico das trapaças (bebedeira, carta da manga, cartas
 * marcadas, suborno pendente; ver gameplay/trapacas.h). A carta da manga
 * vive fora do baralho e persiste entre as mãos e os confrontos.
 * cheats_da_mao: cheats como estavam quando a mão atual começou.
 * risco: balas da roleta e usos de cada trapaça (ver gameplay/probabilidade.h).
 * estado_do_risco: gerador que decide o gatilho da roleta, as descobertas e a
 * recusa do dealer. Também é da partida, pelo mesmo motivo.
 * roleta_motivo: por que a roleta está em jogo, enquanto a situação for
 * PARTIDA_ROLETA.
 * risco_da_mao e estado_do_risco_da_mao: risco e gerador como estavam quando a
 * mão atual começou; o que a mão muda neles se refaz pelo registro.
 * registro e quantidade_no_registro: o que aconteceu na mão atual, na ordem.
 * registro_perdido: true se a mão passou de PARTIDA_REGISTRO_MAXIMO entradas
 * e o registro ficou incompleto; nesse caso ela não pode mais ser guardada.
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
    Torneio torneio_da_mao;
    unsigned semente_da_mao;
    uint64_t estado_das_sementes;
    CheatState cheats;
    CheatState cheats_da_mao;
    SistemaRisco risco;
    uint64_t estado_do_risco;
    RoletaMotivo roleta_motivo;
    SistemaRisco risco_da_mao;
    uint64_t estado_do_risco_da_mao;
    RegistroDaMao registro[PARTIDA_REGISTRO_MAXIMO];
    size_t quantidade_no_registro;
    bool registro_perdido;
} Partida;

/**
 * O que basta guardar de uma partida para continuá-la depois: o torneio no
 * início da mão atual, a semente do baralho, o risco no início da mão, o
 * registro do que aconteceu nela e o estado da IA. As cartas, as fichas e a
 * situação servem para conferir, ao refazer a mão, que o resultado é o mesmo
 * de quando foi guardada.
 *
 * torneio: torneio no início da mão atual; sempre em andamento.
 * semente_da_mao: semente do embaralhamento da mão atual.
 * estado_das_sementes: estado do gerador das sementes das próximas mãos.
 * rng_acao e rng_equidade: estados dos geradores da IA.
 * risco e estado_do_risco: risco e gerador da roleta no início da mão.
 * cheats: estado das trapaças no início da mão (inclui a bebedeira e a carta
 * da manga).
 * situacao: situação da partida quando foi guardada.
 * cartas, board e cartas_no_board: cartas da mão quando foi guardada.
 * fichas: fichas de cada assento na mesa quando foi guardada.
 * registro e quantidade_no_registro: o que aconteceu na mão, na ordem.
 */
typedef struct {
    Torneio torneio;
    unsigned semente_da_mao;
    uint64_t estado_das_sementes;
    uint64_t rng_acao;
    uint64_t rng_equidade;
    SistemaRisco risco;
    uint64_t estado_do_risco;
    CheatState cheats;
    PartidaSituacao situacao;
    PokerCard cartas[2][2];
    PokerCard board[MAO_CARTAS_NO_BOARD];
    size_t cartas_no_board;
    PokerChips fichas[2];
    RegistroDaMao registro[PARTIDA_REGISTRO_MAXIMO];
    size_t quantidade_no_registro;
} PartidaSalva;

/**
 * Começa uma partida nova: prepara o torneio e a IA do primeiro adversário
 * e inicia a primeira mão. Cada mão sorteia a própria semente a partir da
 * semente da partida e chama srand com ela antes de embaralhar, então o
 * baralho de uma mão não depende do rand global. Depois da chamada, confira
 * Partida.situacao: se um stack for menor ou igual ao blind, a mão já se
 * resolve e a partida começa em PARTIDA_MAO_ENCERRADA.
 *
 * @param partida partida zerada ou já usada.
 * @param semente semente dos baralhos e da IA; a mesma semente com as mesmas
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
 * derrotado, antes disso avança o confronto, reinicia
 * os blinds e tira uma bala da roleta (a suspeita também recomeça).
 *
 * @param partida partida em PARTIDA_MAO_ENCERRADA ou PARTIDA_CONFRONTO_VENCIDO.
 * @return true se a mão seguinte começou; false se a partida não está
 * esperando por isso (inclusive depois de falência, morte ou título).
 */
bool partida_continuar(Partida *partida);

/**
 * Diz se o jogador pode pedir a trapaça agora, sem executá-la. A interface usa
 * isto para mostrar quais opções valem.
 *
 * @param partida partida a consultar.
 * @param pedido a trapaça e seus parâmetros.
 * @return POKER_OK se vale; POKER_ERR_ARGUMENT se o pedido é inválido (tipo,
 * carta da mão ou carta pedida); POKER_ERR_TURN se a vez é da IA;
 * POKER_ERR_STATE se não há mão em andamento; POKER_ERR_CHIPS se pagar
 * deixaria o jogador sem fichas; POKER_ERR_ACTION se a regra não deixa agora
 * (suborno já usado na mão, carta já marcada, sem espaço para marcar, carta
 * pedida já à vista ou na manga, ou nenhuma rua por vir).
 */
PokerStatus partida_pode_trapacear(const Partida *partida, PedidoTrapaca pedido);

/**
 * Quanto a trapaça custa agora, em fichas, tirado da pilha do jogador na hora
 * do pedido. O cúmplice custa o big blind vezes uma a mais que seus usos
 * naquele nível de blind; o suborno custa as fichas iniciais do adversário
 * divididas por PARTIDA_SUBORNO_DIVISOR, vezes 1 (carta na mesa), 2 (carta na
 * mão) ou 3 (ver as cartas do adversário); as demais não custam.
 *
 * @param partida partida a consultar.
 * @param tipo a trapaça.
 * @return o custo; 0 se não há custo ou o tipo é inválido.
 */
PokerChips partida_custo_da_trapaca(const Partida *partida, TipoDePedido tipo);

/**
 * Chance, de 0 a 100, de o jogador ser descoberto se tentar a trapaça agora:
 * risco-base, usos já feitos neste confronto, a desconfiança do oponente do
 * confronto (cresce de um oponente para o seguinte e não depende do estilo de
 * jogo dele) e o estado dele (sóbrio ou embriagado).
 *
 * @param partida partida a consultar.
 * @param tipo a trapaça.
 * @return a chance em %; -1 se o tipo é inválido.
 */
int partida_chance_de_descoberta(const Partida *partida, TipoDePedido tipo);

/**
 * Tenta a trapaça pedida na vez do jogador. Cobra o custo da pilha dele (e
 * tira o mesmo total do total da mão, porque essas fichas saem de
 * circulação), sorteia a recusa do dealer, se for suborno, e a descoberta
 * com os geradores da Partida, e aplica o efeito. O pedido entra no registro
 * da mão e a tentativa vira EVENTO_TRAPACA. Não gasta a vez do jogador.
 *
 * Se o jogador é descoberto, a partida vai para PARTIDA_ROLETA (motivo
 * ROLETA_DESCOBERTO) e a trapaça não faz efeito. Pedidos que partida_pode_trapacear
 * recusa não alteram nada e não gastam sorteio.
 *
 * @param partida partida em PARTIDA_VEZ_DO_JOGADOR.
 * @param pedido a trapaça e seus parâmetros.
 * @param resultado recebe TRAPACA_SUCESSO, TRAPACA_DESCOBERTA ou
 * TRAPACA_RECUSADA (o dealer pegou o dinheiro e não fez nada); pode ser NULL.
 * @return POKER_OK se a trapaça foi tentada; os mesmos códigos de erro de
 * partida_pode_trapacear se não foi.
 */
PokerStatus partida_trapacear(Partida *partida, PedidoTrapaca pedido, CheatResult *resultado);

/**
 * Puxa o gatilho da roleta russa. O resultado sai do gerador da partida, e
 * não do rand global, então refazer a mão (ver partida_restaurar) dá o mesmo
 * resultado. Registra EVENTO_ROLETA.
 *
 * Se o jogador morre, a partida vira PARTIDA_JOGADOR_MORTO. Se sobrevive,
 * uma bala entra no tambor e a suspeita das trapaças recomeça; depois, numa
 * roleta de falência o jogador volta com metade das fichas iniciais do
 * adversário (PARTIDA_MAO_ENCERRADA, e partida_continuar começa a próxima
 * mão), e numa de trapaça descoberta a mão segue na vez dele.
 *
 * @param partida partida em PARTIDA_ROLETA.
 * @return true se o gatilho foi puxado; false se a partida não está na roleta.
 */
bool partida_puxar_gatilho(Partida *partida);

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
 * Dá o nome (provisório) do adversário de um confronto: Oponente 1, 2 ou 3.
 * O nome não diz como ele joga, porque o comportamento muda durante o
 * confronto (ver partida_estilo_do_adversario).
 *
 * @param confronto índice do confronto, de 0 a TORNEIO_CONFRONTOS - 1.
 * @return o nome, ou "?" se o índice for inválido.
 */
const char *partida_nome_do_adversario(int confronto);

/**
 * Diz como o adversário joga a mão que começa com o torneio neste estado.
 * Todo oponente alterna entre os mesmos três comportamentos, conforme as
 * fichas que tem em relação às que trouxe para o confronto, e começa o
 * confronto desconfiado:
 *
 * - perdendo (PARTIDA_ESTILO_ATRAS_PCT ou menos): medroso (POKER_AI_CAUTIOUS);
 * - ganhando (PARTIDA_ESTILO_FRENTE_PCT ou mais): agressivo (POKER_AI_AGGRESSIVE);
 * - no meio: desconfiado (POKER_AI_BALANCED, o jogo equilibrado).
 *
 * Depende só do torneio, que o salvamento já guarda no começo da mão; por
 * isso o estilo se refaz ao continuar sem entrar no arquivo. O estilo vale
 * pela mão inteira.
 *
 * @param torneio estado do torneio no começo da mão.
 * @return o estilo da IA; o equilibrado se o confronto for inválido.
 */
PokerAIStyle partida_estilo_do_adversario(const Torneio *torneio);

/**
 * Junta o que é preciso para continuar a partida mais tarde, no ponto em
 * que ela está. Só vale enquanto a partida pode ser continuada: na vez do
 * jogador, na vez do adversário, na roleta, depois de uma mão ou depois de um
 * confronto vencido. Falência definitiva, morte e título encerram a partida,
 * e não há o que guardar.
 *
 * @param partida partida a guardar.
 * @param salva recebe os dados; fica intacta se a função devolver false.
 * @return true se os dados foram preenchidos; false se a partida não pode
 * ser continuada ou se a mão passou de PARTIDA_REGISTRO_MAXIMO entradas.
 */
bool partida_exportar(const Partida *partida, PartidaSalva *salva);

/**
 * Refaz uma partida guardada por partida_exportar: volta o torneio ao início
 * da mão, embaralha com a mesma semente e reaplica as ações, o que devolve a
 * mão exatamente ao ponto em que estava. Refaz também a fila de eventos da
 * mão, que a interface pode consumir para mostrar o que já aconteceu. Antes
 * de aceitar, confere se cartas, fichas e situação batem com os guardados.
 *
 * Chama srand com a semente da mão, como cada mão faz ao começar. Dados de
 * outro programa ou de outra máquina podem embaralhar de outro jeito, e
 * nesse caso a conferência recusa.
 *
 * @param partida partida zerada ou já usada; fica vazia se falhar.
 * @param salva dados guardados.
 * @return true se a partida voltou ao ponto guardado; false se os dados são
 * inválidos ou não reproduzem a mão guardada.
 */
bool partida_restaurar(Partida *partida, const PartidaSalva *salva);

#endif
