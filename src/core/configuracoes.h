#ifndef CONFIGURACOES_H
#define CONFIGURACOES_H

#include <stdbool.h>

#define RESOLUCOES_TOTAL 5

/** Canais independentes de volume, em porcentagem de 0 a 100. */
typedef enum {
    VOLUME_GERAL = 0,
    VOLUME_SFX,
    VOLUME_MUSICA,
    VOLUMES_TOTAL
} CanalVolume;

/**
 * Tamanho da janela do jogo.
 *
 * largura e altura: tamanho da janela em pixels, sempre na proporção 16:9.
 * tela_cheia: ocupa o monitor inteiro, sem bordas; nesse caso largura e
 * altura valem 0, porque o tamanho é o do monitor.
 */
typedef struct {
    int largura;
    int altura;
    bool tela_cheia;
} Resolucao;

/**
 * Escolhas do jogador que valem para o jogo todo.
 *
 * resolucao: índice da resolução em uso, de 0 a RESOLUCOES_TOTAL - 1.
 *
 * Inicialize com configuracoes_iniciar.
 */
typedef struct {
    int resolucao;
    int volume_geral;
    int volume_sfx;
    int volume_musica;
} Configuracoes;

/**
 * Põe as configurações no padrão: a janela de 1280x720, que é o tamanho com
 * que o jogo abre.
 *
 * @param configuracoes configurações a preencher.
 */
void configuracoes_iniciar(Configuracoes *configuracoes);

/** Consulta um volume; ponteiro ou canal inválido devolve 0. */
int configuracoes_volume(const Configuracoes *configuracoes, CanalVolume canal);

/** Define um volume, limitado a 0..100; false para ponteiro ou canal inválido. */
bool configuracoes_definir_volume(Configuracoes *configuracoes, CanalVolume canal, int valor);

/** Carrega apenas os volumes; arquivo ausente/inválido não altera as configurações. */
bool configuracoes_carregar_volumes(Configuracoes *configuracoes, const char *caminho);

/** Grava os volumes em texto versionado, substituindo o arquivo após concluir a escrita. */
bool configuracoes_salvar_volumes(const Configuracoes *configuracoes, const char *caminho);

/**
 * Dá uma das resoluções disponíveis, da menor para a maior, com a tela cheia
 * por último.
 *
 * @param indice posição na lista, de 0 a RESOLUCOES_TOTAL - 1.
 * @return a resolução; com um índice inválido, uma Resolucao zerada.
 */
Resolucao configuracoes_resolucao(int indice);

/**
 * Dá a resolução que está em uso.
 *
 * @param configuracoes configurações a consultar.
 * @return a resolução em uso; uma Resolucao zerada se o ponteiro for NULL.
 */
Resolucao configuracoes_resolucao_atual(const Configuracoes *configuracoes);

/**
 * Passa a usar uma das resoluções da lista.
 *
 * @param configuracoes configurações a alterar.
 * @param indice posição na lista, de 0 a RESOLUCOES_TOTAL - 1.
 * @return true se a escolha foi aceita; com um índice inválido não altera
 * nada.
 */
bool configuracoes_escolher_resolucao(Configuracoes *configuracoes, int indice);

/**
 * Move a seleção da lista de resoluções. Depois da última volta para a
 * primeira, e antes da primeira vai para a última.
 *
 * @param selecionada posição atual; um valor fora da lista conta como a
 * primeira.
 * @param passos quantas posições andar; negativo para voltar.
 * @return a nova posição, de 0 a RESOLUCOES_TOTAL - 1.
 */
int configuracoes_mover_selecao(int selecionada, int passos);

#endif
