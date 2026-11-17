#include "ui/cartas.h"

#include <stdio.h>

#define VALORES 13
#define NAIPES 4

static Texture2D frentes[2][NAIPES][VALORES];
static Texture2D versos[2];

void cartas_iniciar(void)
{
    static const char *VALOR[VALORES] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
    };
    static const char NAIPE[NAIPES] = { 'C', 'D', 'H', 'S' };
    char caminho[128];

    for (int marcada = 0; marcada < 2; marcada++) {
        const char *pasta = marcada ? "assets/images/cartas_marcadas" : "assets/images/cartas";

        for (int naipe = 0; naipe < NAIPES; naipe++) {
            for (int valor = 0; valor < VALORES; valor++) {
                snprintf(caminho, sizeof caminho, "%s/%s%c%s.png", pasta, VALOR[valor], NAIPE[naipe], marcada ? "D" : "");
                frentes[marcada][naipe][valor] = LoadTexture(caminho);
            }
        }
    }

    versos[0] = LoadTexture("assets/images/cartas/BACK_RED.png");
    versos[1] = LoadTexture("assets/images/cartas_marcadas/BACK_MARKEDD.png");
}

void cartas_encerrar(void)
{
    for (int marcada = 0; marcada < 2; marcada++) {
        for (int naipe = 0; naipe < NAIPES; naipe++) {
            for (int valor = 0; valor < VALORES; valor++) {
                if (frentes[marcada][naipe][valor].id != 0)
                    UnloadTexture(frentes[marcada][naipe][valor]);
            }
        }

        if (versos[marcada].id != 0)
            UnloadTexture(versos[marcada]);
    }
}

static void desenhar_textura(Texture2D textura, Rectangle destino)
{
    if (textura.id != 0) {
        DrawTexturePro(textura, (Rectangle){ 0, 0, (float)textura.width, (float)textura.height }, destino, (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
}

void cartas_desenhar_frente(PokerCard carta, bool marcada, Rectangle destino)
{
    if (carta.rank < 2 || carta.rank > 14 || carta.suit < POKER_CLUBS || carta.suit > POKER_SPADES)
        return;

    desenhar_textura(frentes[marcada ? 1 : 0][carta.suit][carta.rank - 2], destino);
}

void cartas_desenhar_verso(bool marcado, Rectangle destino)
{
    desenhar_textura(versos[marcado ? 1 : 0], destino);
}
