#ifndef TORNEIO_H
#define TORNEIO_H

#include <stdbool.h>

#include "poker.h"

#define TORNEIO_CONFRONTOS 3
#define TORNEIO_FICHAS_INICIAIS_JOGADOR 2000
#define TORNEIO_MAOS_POR_NIVEL 5
#define TORNEIO_FATOR_BLIND 2
#define TORNEIO_BLIND_MAXIMO 1000000
#define TORNEIO_ASSENTO_JOGADOR 0
#define TORNEIO_ASSENTO_ADVERSARIO 1

/**
 * Situação do torneio depois de cada mão.
 *
 * TORNEIO_EM_ANDAMENTO: o confronto continua e a próxima mão pode começar.
 * TORNEIO_CONFRONTO_VENCIDO: o adversário ficou sem fichas e há um próximo
 * confronto; é preciso chamar torneio_avancar_confronto.
 * TORNEIO_JOGADOR_FALIDO: o jogador ficou sem fichas; quem trata a roleta
 * decide se ele continua, chamando torneio_recuperar_jogador.
 * TORNEIO_CAMPEAO: o último adversário foi derrotado e o torneio acabou.
 */
typedef enum {
    TORNEIO_EM_ANDAMENTO = 0,
    TORNEIO_CONFRONTO_VENCIDO,
    TORNEIO_JOGADOR_FALIDO,
    TORNEIO_CAMPEAO
} TorneioSituacao;

/**
 * Estado do torneio de três confrontos em formato heads-up.
 *
 * confronto: índice do confronto atual, de 0 a TORNEIO_CONFRONTOS - 1.
 * fichas_jogador e fichas_adversario: fichas de cada um entre as mãos.
 * maos_jogadas: mãos já registradas no confronto atual; define o nível dos
 * blinds e volta a zero a cada novo confronto.
 * dealer: assento que dá as cartas na próxima mão (também é o small blind).
 * recuperacoes: quantas vezes o jogador voltou de uma falência.
 * situacao: situação atual do torneio.
 */
typedef struct {
    int confronto;
    PokerChips fichas_jogador;
    PokerChips fichas_adversario;
    int maos_jogadas;
    int dealer;
    int recuperacoes;
    TorneioSituacao situacao;
} Torneio;

/**
 * Prepara um torneio novo: primeiro confronto, jogador com
 * TORNEIO_FICHAS_INICIAIS_JOGADOR e o adversário com as fichas do confronto.
 *
 * @param torneio estado a inicializar.
 */
void torneio_iniciar(Torneio *torneio);

/**
 * Informa com quantas fichas o adversário de um confronto começa.
 *
 * @param confronto índice do confronto, de 0 a TORNEIO_CONFRONTOS - 1.
 * @return fichas iniciais do adversário, ou 0 se o índice for inválido.
 */
PokerChips torneio_fichas_iniciais_adversario(int confronto);

/**
 * Calcula o big blind da próxima mão. Ele começa num valor próprio de cada
 * confronto e é multiplicado por TORNEIO_FATOR_BLIND a cada
 * TORNEIO_MAOS_POR_NIVEL mãos, sem passar de TORNEIO_BLIND_MAXIMO.
 *
 * @param torneio estado do torneio.
 * @return big blind da próxima mão.
 */
PokerChips torneio_big_blind(const Torneio *torneio);

/**
 * Calcula o small blind da próxima mão, que é metade do big blind.
 *
 * @param torneio estado do torneio.
 * @return small blind da próxima mão.
 */
PokerChips torneio_small_blind(const Torneio *torneio);

/**
 * Entrega os stacks no formato que o motor de pôquer espera, indexados pelo
 * assento: posição TORNEIO_ASSENTO_JOGADOR e TORNEIO_ASSENTO_ADVERSARIO.
 *
 * @param torneio estado do torneio.
 * @param stacks vetor de dois elementos que recebe as fichas de cada assento.
 */
void torneio_stacks(const Torneio *torneio, PokerChips stacks[2]);

/**
 * Registra o resultado de uma mão já liquidada e atualiza a situação do
 * torneio. Passa o dealer para o outro assento e conta a mão para os blinds.
 * Recusa o registro, sem alterar nada, se o torneio não estiver em andamento,
 * se algum valor for negativo, se os dois ficarem sem fichas ou se a soma das
 * fichas aumentar: fichas só podem sair de circulação (por exemplo,
 * pagamentos de trapaça), nunca surgir.
 *
 * @param torneio estado do torneio.
 * @param fichas_jogador fichas do jogador ao fim da mão.
 * @param fichas_adversario fichas do adversário ao fim da mão.
 * @return true se o resultado foi aceito.
 */
bool torneio_registrar_mao(
    Torneio *torneio,
    PokerChips fichas_jogador,
    PokerChips fichas_adversario
);

/**
 * Passa para o próximo confronto depois de uma vitória. O jogador leva o
 * saldo acumulado, o novo adversário entra com as fichas do seu confronto e
 * os blinds reiniciam.
 *
 * @param torneio estado do torneio.
 * @return true se avançou; false se o confronto atual não foi vencido.
 */
bool torneio_avancar_confronto(Torneio *torneio);

/**
 * Devolve o jogador ao confronto depois de uma falência em que ele
 * sobreviveu à roleta. Ele volta com metade das fichas iniciais do
 * adversário do confronto atual; o adversário mantém suas fichas e os blinds
 * continuam no nível em que estavam. O limite de recuperações fica a cargo
 * de quem chama, que pode consultar o campo recuperacoes.
 *
 * @param torneio estado do torneio.
 * @return true se o jogador voltou; false se ele não estava falido.
 */
bool torneio_recuperar_jogador(Torneio *torneio);

#endif
