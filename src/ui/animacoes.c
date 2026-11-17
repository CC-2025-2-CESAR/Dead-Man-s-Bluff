#include "ui/animacoes.h"
#include "raylib.h"

#include <stddef.h>

static Image gif = { 0 };
static Texture2D textura = { 0 };
static int total_quadros;
static int quadro_atual;
static double tempo_no_quadro;
static bool embaralhando;
static bool ignorar_proximo_inicio;

void animacoes_iniciar(void)
{
    gif = LoadImageAnim("assets/gifs/embaralhamento.gif", &total_quadros);

    if (gif.data != NULL && total_quadros > 0)
        textura = LoadTextureFromImage(gif);
}

void animacoes_encerrar(void)
{
    if (textura.id != 0)
        UnloadTexture(textura);

    if (gif.data != NULL)
        UnloadImage(gif);
}

void animacoes_cancelar(void)
{
    embaralhando = false;
    ignorar_proximo_inicio = false;
    quadro_atual = 0;
    tempo_no_quadro = 0.0;
}

void animacoes_ignorar_proximo_inicio(void)
{
    ignorar_proximo_inicio = true;
}

void animacoes_comecar_embaralhamento(void)
{
    if (ignorar_proximo_inicio) {
        ignorar_proximo_inicio = false;
        return;
    }

    if (textura.id == 0 || total_quadros <= 0)
        return;

    embaralhando = true;
    quadro_atual = 0;
    tempo_no_quadro = 0.0;
    UpdateTexture(textura, gif.data);
}

bool animacoes_embaralhando(void)
{
    return embaralhando;
}

static double duracao_do_quadro(void)
{
    if (quadro_atual == 0)
        return 0.42;

    if (quadro_atual == total_quadros - 1)
        return 0.34;

    return 0.08;
}

void animacoes_atualizar(double segundos)
{
    if (!embaralhando)
        return;

    tempo_no_quadro += segundos;

    while (embaralhando && tempo_no_quadro >= duracao_do_quadro()) {
        tempo_no_quadro -= duracao_do_quadro();
        quadro_atual++;

        if (quadro_atual >= total_quadros) {
            embaralhando = false;
            break;
        }

        size_t deslocamento =
            (size_t)quadro_atual * (size_t)gif.width * (size_t)gif.height * 4u;

        UpdateTexture(textura, (unsigned char *)gif.data + deslocamento);
    }
}

void animacoes_desenhar(void)
{
    if (embaralhando)
        DrawTexture(textura, 0, 0, WHITE);
}