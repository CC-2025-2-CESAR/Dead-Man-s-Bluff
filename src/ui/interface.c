#include "ui/interface.h"
#include "raylib.h"

#define TITULO_JANELA "Dead Man's Bluff"

static const Color COR_FUNDO = { 12, 46, 31, 255 };
static const Color COR_OURO = { 212, 175, 55, 255 };
static const Color COR_TEXTO = { 235, 235, 225, 255 };
static const Color COR_TEXTO_SUAVE = { 150, 170, 155, 255 };

static const char *ROTULOS_MENU[MENU_TOTAL] = { "JOGAR", "SAIR" };

/**
 * Desenha um texto centralizado na largura da tela.
 *
 * @param texto texto a desenhar.
 * @param y posição vertical do topo do texto.
 * @param tamanho altura da fonte em pixels.
 * @param cor cor do texto.
 */
static void desenhar_texto_centralizado(const char *texto, int y, int tamanho, Color cor)
{
    int largura = MeasureText(texto, tamanho);

    DrawText(texto, (LARGURA_TELA - largura) / 2, y, tamanho, cor);
}

void interface_iniciar(void)
{
    InitWindow(LARGURA_TELA, ALTURA_TELA, TITULO_JANELA);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
}

void interface_encerrar(void)
{
    CloseWindow();
}

bool interface_fechar_solicitado(void)
{
    return WindowShouldClose();
}

bool interface_menu_atualizar(OpcaoMenu *selecionada)
{
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        *selecionada = (*selecionada + 1) % MENU_TOTAL;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        *selecionada = (*selecionada + MENU_TOTAL - 1) % MENU_TOTAL;

    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

void interface_menu_desenhar(OpcaoMenu selecionada)
{
    BeginDrawing();
    ClearBackground(COR_FUNDO);

    desenhar_texto_centralizado("DEAD MAN'S BLUFF", 150, 88, COR_OURO);
    desenhar_texto_centralizado("um roguelike de pôquer", 255, 28, COR_TEXTO);
    DrawRectangle((LARGURA_TELA - 320) / 2, 305, 320, 3, COR_OURO);

    for (int i = 0; i < MENU_TOTAL; i++) {
        int y = 390 + i * 70;

        if (i == (int)selecionada)
            desenhar_texto_centralizado(TextFormat("> %s <", ROTULOS_MENU[i]), y, 44, COR_OURO);
        else
            desenhar_texto_centralizado(ROTULOS_MENU[i], y, 44, COR_TEXTO_SUAVE);
    }

    desenhar_texto_centralizado("W/S ou setas para navegar  -  ENTER para confirmar", ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);

    EndDrawing();
}

bool interface_voltar_pressionado(void)
{
    return IsKeyPressed(KEY_ESCAPE);
}

void interface_mesa_desenhar(void)
{
    BeginDrawing();
    ClearBackground(COR_FUNDO);

    desenhar_texto_centralizado("MESA DE PÔQUER", 280, 64, COR_OURO);
    desenhar_texto_centralizado("em construção", 360, 32, COR_TEXTO);
    desenhar_texto_centralizado("ESC para voltar ao menu", ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);

    EndDrawing();
}
