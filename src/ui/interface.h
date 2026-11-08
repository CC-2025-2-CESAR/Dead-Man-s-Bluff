#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>

#define LARGURA_TELA 1280
#define ALTURA_TELA 720

/** Opções disponíveis na tela inicial. */
typedef enum {
    MENU_JOGAR = 0,
    MENU_SAIR,
    MENU_TOTAL
} OpcaoMenu;

/**
 * Abre a janela do jogo e define a taxa de quadros. A tecla ESC não fecha
 * a janela: quem decide a saída é o jogo.
 */
void interface_iniciar(void);

/** Fecha a janela do jogo. */
void interface_encerrar(void);

/** Indica se o jogador pediu para fechar a janela (botão de fechar). */
bool interface_fechar_solicitado(void);

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

/** Indica se o jogador apertou o botão de voltar (ESC) neste quadro. */
bool interface_voltar_pressionado(void);

/** Desenha a tela provisória da mesa, enquanto a partida não é exibida. */
void interface_mesa_desenhar(void);

#endif
