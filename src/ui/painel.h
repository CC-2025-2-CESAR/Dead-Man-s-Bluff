#ifndef PAINEL_H
#define PAINEL_H

#include <stdbool.h>

#include "raylib.h"

/*
 * Peças de desenho das telas no estilo noir (painéis escuros com borda
 * dourada, linha escolhida em vermelho, bilhete de papel). Só desenham: quem
 * registra cliques e guarda estado é a interface.
 */

extern const Color PAINEL_FUNDO;
extern const Color PAINEL_FUNDO_CLARO;
extern const Color PAINEL_OURO;
extern const Color PAINEL_OURO_VIVO;
extern const Color PAINEL_VERMELHO;
extern const Color PAINEL_VERMELHO_VIVO;
extern const Color PAINEL_TEXTO;
extern const Color PAINEL_TEXTO_SUAVE;
extern const Color PAINEL_PAPEL;
extern const Color PAINEL_TINTA;

/** Ícones desenhados com formas simples, sem imagem. */
typedef enum {
    ICONE_GARRAFA = 0,
    ICONE_BINOCULO,
    ICONE_AS,
    ICONE_SUBORNO,
    ICONE_MARCA,
    ICONE_MASCARA,
    ICONE_AVISO,
    ICONE_FICHA,
    ICONE_OLHO,
    ICONE_CARTA_MESA,
    ICONE_CARTA_MAO
} Icone;

/** Curva de saída suave (cúbica): 0 vira 0, 1 vira 1, e o fim desacelera. */
float painel_suave(float t);

/**
 * Quanto de uma animação já passou, de 0 a 1.
 *
 * @param inicio instante em que ela começou (GetTime()).
 * @param duracao duração em segundos.
 */
float painel_progresso(double inicio, double duracao);

/** Painel escuro com borda dupla dourada e cantos em losango. */
void painel_desenhar(Rectangle area, float opacidade);

/** Só as linhas da borda de painel_desenhar, na cor pedida. */
void painel_moldura(Rectangle area, Color cor, float opacidade);

/** Desenha um ícone dentro da área (usa o maior quadrado que cabe nela). */
void painel_icone(Icone icone, Rectangle area, Color cor);

/**
 * Barra de medida, cheia até a fração pedida.
 *
 * @param segmentos 0 ou 1 para barra contínua; mais que isso divide em blocos.
 */
void painel_barra(Rectangle area, float fracao, int segmentos, Color cor);

/**
 * Escreve um texto quebrando as linhas na largura da área.
 *
 * @return a altura usada, em pixels.
 */
int painel_texto_quebrado(const char *texto, Rectangle area, int tamanho, Color cor);

/** Escurece as bordas da tela inteira, do tamanho dado, com a cor pedida. */
void painel_vinheta(int largura, int altura, Color cor, float intensidade);

/**
 * Botão com moldura. Quem chama decide se o mouse está sobre ele e registra
 * a área para o clique.
 *
 * @param principal true para o botão vermelho de ação.
 * @param ativo false desenha o botão apagado.
 */
void painel_botao(Rectangle area, const char *rotulo, int tamanho, bool principal, bool sobre, bool ativo);

/** Bilhete de papel com o texto quebrado em tinta escura. */
void painel_papel(Rectangle area, const char *texto, int tamanho);

/**
 * Tambor de revólver com seis câmaras, visto de frente.
 *
 * @param balas quantas câmaras estão carregadas.
 * @param giro rotação em radianos, para animar o giro.
 */
void painel_tambor(Vector2 centro, float raio, int balas, float giro);

/**
 * Clarão de tiro na boca do cano: estrela de pontas com brilho em volta.
 *
 * @param intensidade de 0 (apagado) a 1 (no auge).
 */
void painel_clarao(Vector2 centro, float raio, float intensidade);

#endif
