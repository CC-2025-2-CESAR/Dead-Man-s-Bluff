#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>

#include "core/configuracoes.h"
#include "core/eventos.h"
#include "core/partida.h"

/**
 * Tamanho do desenho, em pixels, que não muda com a resolução da janela. Todas
 * as telas desenham como se a janela tivesse este tamanho, e a interface
 * escala o resultado para a janela real, com faixas nas bordas se a
 * proporção for diferente. Quem desenha telas novas usa estas coordenadas.
 */
#define LARGURA_TELA 1280
#define ALTURA_TELA 720

/** Opções disponíveis na tela inicial. */
typedef enum {
    MENU_CONTINUAR = 0,
    MENU_NOVO_JOGO,
    MENU_CONFIGURACOES,
    MENU_SAIR,
    MENU_TOTAL
} OpcaoMenu;

/** Resposta do jogador a uma pergunta de sim ou não. */
typedef enum {
    RESPOSTA_NENHUMA = 0,
    RESPOSTA_SIM,
    RESPOSTA_NAO
} RespostaConfirmacao;

/**
 * O que o jogador fez na tela de configurações em um quadro.
 *
 * movimento: -1 para subir na lista, 1 para descer, 0 se não mexeu.
 * confirmar: pediu para usar a opção destacada.
 * voltar: pediu para voltar ao menu.
 */
typedef struct {
    int movimento;
    bool confirmar;
    bool voltar;
} EntradaConfiguracoes;

/**
 * O que o jogador fez na mesa em um quadro. A interface só traduz teclas ou
 * cliques; quem decide o que cada comando significa é o core.
 *
 * comando: ação pedida na vez do jogador, ou COMANDO_NENHUM.
 * ajuste_aposta: -1 para diminuir e 1 para aumentar o valor da aposta, em
 * passos de um big blind; 0 se não mexeu.
 * continuar: pediu para seguir depois do fim de uma mão ou da partida.
 * voltar: pediu para sair da mesa e voltar ao menu.
 */
typedef struct {
    ComandoJogador comando;
    int ajuste_aposta;
    bool continuar;
    bool voltar;
} EntradaMesa;

/**
 * Abre a janela do jogo e define a taxa de quadros. A tecla ESC não fecha
 * a janela: quem decide a saída é o jogo.
 */
void interface_iniciar(void);

/** Fecha a janela do jogo. */
void interface_encerrar(void);

/** Indica se o jogador pediu para fechar a janela (botão de fechar). */
bool interface_fechar_solicitado(void);

/** Devolve quantos segundos durou o último quadro. */
double interface_tempo_do_quadro(void);

/**
 * Lê o teclado na tela inicial: W/S ou as setas movem a seleção e
 * ENTER ou ESPAÇO confirmam. Enquanto não há jogo salvo, a opção Continuar
 * não pode ser escolhida e a seleção pula por ela.
 *
 * @param selecionada opção destacada, atualizada quando o jogador navega.
 * @param continuar_disponivel true se há um jogo salvo para continuar.
 * @return true no quadro em que o jogador confirma a opção destacada.
 */
bool interface_menu_atualizar(OpcaoMenu *selecionada, bool continuar_disponivel);

/**
 * Desenha a tela inicial com o título do jogo e o menu.
 *
 * @param selecionada opção que aparece destacada.
 * @param continuar_disponivel true se há um jogo salvo; senão a opção
 * Continuar aparece apagada.
 * @param aviso frase curta que aparece abaixo do menu, ou NULL.
 */
void interface_menu_desenhar(OpcaoMenu selecionada, bool continuar_disponivel, const char *aviso);

/**
 * Lê a resposta do jogador a uma pergunta de sim ou não: ENTER ou ESPAÇO
 * respondem sim, ESC responde não.
 *
 * @return a resposta dada neste quadro, ou RESPOSTA_NENHUMA.
 */
RespostaConfirmacao interface_confirmacao_ler_resposta(void);

/**
 * Desenha uma pergunta de sim ou não, com uma frase de explicação.
 *
 * @param pergunta a pergunta, em destaque.
 * @param detalhe o que acontece se o jogador responder sim.
 */
void interface_confirmacao_desenhar(const char *pergunta, const char *detalhe);

/**
 * Lê o teclado na tela de configurações: W/S ou as setas movem a seleção,
 * ENTER ou ESPAÇO confirmam e ESC volta ao menu.
 *
 * @return os comandos do quadro.
 */
EntradaConfiguracoes interface_configuracoes_ler_entrada(void);

/**
 * Desenha a tela de configurações, com a lista de resoluções.
 *
 * @param configuracoes configurações em uso; a resolução atual aparece marcada.
 * @param selecionada posição destacada na lista de resoluções.
 */
void interface_configuracoes_desenhar(const Configuracoes *configuracoes, int selecionada);

/**
 * Indica se uma resolução cabe no monitor onde a janela está. A janela em
 * tamanho fixo precisa caber com a barra de título; a tela cheia sempre
 * cabe.
 *
 * @param resolucao resolução a conferir.
 * @return true se pode ser usada neste monitor.
 */
bool interface_resolucao_cabe(Resolucao resolucao);

/**
 * Passa a janela para a resolução pedida: muda o tamanho e a centraliza no
 * monitor, ou liga a tela cheia sem bordas. O desenho se ajusta sozinho ao
 * novo tamanho.
 *
 * @param resolucao resolução a usar.
 * @return true se a janela mudou ou já estava assim; false, sem alterar nada,
 * se a resolução não cabe no monitor.
 */
bool interface_aplicar_resolucao(Resolucao resolucao);

/**
 * Lê o que o jogador fez na mesa neste quadro. Hoje: F desiste, C passa ou
 * paga, R aposta ou aumenta, A vai all-in, as setas para cima e para baixo
 * ajustam a aposta, ENTER ou ESPAÇO continuam e ESC volta ao menu.
 *
 * @return os comandos do quadro.
 */
EntradaMesa interface_mesa_ler_entrada(void);

/**
 * Recebe um evento do jogo, na ordem em que aconteceu, para animar ou
 * registrar. A interface não precisa ler o estado do motor para descobrir o
 * que mudou: os eventos dizem isso (ver core/eventos.h).
 *
 * @param evento evento retirado da fila da partida.
 */
void interface_mesa_registrar_evento(const EventoJogo *evento);

/** Esquece os eventos já recebidos, ao começar uma partida nova. */
void interface_mesa_limpar_eventos(void);

/**
 * Desenha a mesa com o estado atual da partida. A interface só lê a partida
 * e nunca a altera.
 *
 * @param partida partida em andamento.
 * @param aposta_escolhida total de aposta que o jogador está ajustando, ou 0
 * se ele não pode apostar agora.
 */
void interface_mesa_desenhar(const Partida *partida, PokerChips aposta_escolhida);

#endif
