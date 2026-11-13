#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>

#include "core/eventos.h"
#include "core/partida.h"

#define LARGURA_TELA 1280
#define ALTURA_TELA 720

/** Opções disponíveis na tela inicial. */
typedef enum {
    MENU_JOGAR = 0,
    MENU_SAIR,
    MENU_TOTAL
} OpcaoMenu;

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
 * ENTER ou ESPAÇO confirmam.
 *
 * @param selecionada opção destacada, atualizada quando o jogador navega.
 * @return true no quadro em que o jogador confirma a opção destacada.
 */
bool interface_menu_atualizar(OpcaoMenu *selecionada);

/**
 * Desenha a tela inicial com o título do jogo e o menu.
 *
 * @param selecionada opção que aparece destacada.
 */
void interface_menu_desenhar(OpcaoMenu selecionada);

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
