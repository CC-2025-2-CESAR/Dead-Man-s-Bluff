#ifndef CARTAS_UI_H
#define CARTAS_UI_H

#include <stdbool.h>

#include "poker.h"
#include "raylib.h"

/* Carrega os desenhos das 52 cartas, das 52 versões marcadas e dos versos. */
void cartas_iniciar(void);
void cartas_encerrar(void);

void cartas_desenhar_frente(PokerCard carta, bool marcada, Rectangle destino);
void cartas_desenhar_verso(bool marcado, Rectangle destino);

#endif
