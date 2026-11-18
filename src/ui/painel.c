#include "ui/painel.h"

#include <math.h>
#include <string.h>

const Color PAINEL_FUNDO = { 14, 11, 11, 255 };
const Color PAINEL_FUNDO_CLARO = { 34, 26, 24, 255 };
const Color PAINEL_OURO = { 196, 160, 98, 255 };
const Color PAINEL_OURO_VIVO = { 232, 196, 120, 255 };
const Color PAINEL_VERMELHO = { 122, 24, 30, 255 };
const Color PAINEL_VERMELHO_VIVO = { 196, 44, 48, 255 };
const Color PAINEL_TEXTO = { 236, 228, 214, 255 };
const Color PAINEL_TEXTO_SUAVE = { 156, 146, 132, 255 };
const Color PAINEL_PAPEL = { 226, 210, 176, 255 };
const Color PAINEL_TINTA = { 46, 34, 28, 255 };

#define PI_F 3.14159265f

float painel_suave(float t)
{
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;

    float resto = 1.0f - t;
    return 1.0f - resto * resto * resto;
}

float painel_progresso(double inicio, double duracao)
{
    if (duracao <= 0.0)
        return 1.0f;

    double t = (GetTime() - inicio) / duracao;

    if (t <= 0.0)
        return 0.0f;
    return t >= 1.0 ? 1.0f : (float)t;
}

/** Triângulo cheio em qualquer ordem de vértices (a raylib exige anti-horário). */
static void triangulo(Vector2 a, Vector2 b, Vector2 c, Color cor)
{
    float cruz = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    if (cruz > 0.0f) {
        Vector2 troca = b;

        b = c;
        c = troca;
    }

    DrawTriangle(a, b, c, cor);
}

static void losango(Vector2 centro, float raio, Color cor)
{
    Vector2 cima = { centro.x, centro.y - raio };
    Vector2 baixo = { centro.x, centro.y + raio };
    Vector2 esquerda = { centro.x - raio, centro.y };
    Vector2 direita = { centro.x + raio, centro.y };

    triangulo(cima, esquerda, direita, cor);
    triangulo(baixo, esquerda, direita, cor);
}

void painel_moldura(Rectangle area, Color cor, float opacidade)
{
    Rectangle dentro = { area.x + 5.0f, area.y + 5.0f, area.width - 10.0f, area.height - 10.0f };
    Vector2 cantos[4] = {
        { area.x, area.y },
        { area.x + area.width, area.y },
        { area.x, area.y + area.height },
        { area.x + area.width, area.y + area.height }
    };

    DrawRectangleLinesEx(area, 1.5f, Fade(cor, opacidade));

    if (dentro.width > 0.0f && dentro.height > 0.0f)
        DrawRectangleLinesEx(dentro, 1.0f, Fade(cor, 0.4f * opacidade));

    for (int i = 0; i < 4; i++)
        losango(cantos[i], 5.0f, Fade(cor, opacidade));
}

void painel_desenhar(Rectangle area, float opacidade)
{
    DrawRectangleRec(area, Fade(PAINEL_FUNDO, 0.95f * opacidade));
    DrawRectangleGradientV((int)area.x, (int)area.y, (int)area.width, (int)(area.height * 0.4f),
                           Fade(PAINEL_FUNDO_CLARO, 0.7f * opacidade), Fade(PAINEL_FUNDO, 0.0f));
    painel_moldura(area, PAINEL_OURO, opacidade);
}

/* Cada ícone usa o quadrado de lado s centrado em (cx, cy). */

static void icone_garrafa(float cx, float cy, float s, Color cor)
{
    Rectangle corpo = { cx - 0.17f * s, cy - 0.06f * s, 0.34f * s, 0.50f * s };
    Rectangle gargalo = { cx - 0.06f * s, cy - 0.40f * s, 0.12f * s, 0.24f * s };
    Rectangle tampa = { cx - 0.08f * s, cy - 0.46f * s, 0.16f * s, 0.07f * s };
    Rectangle rotulo = { cx - 0.17f * s, cy + 0.06f * s, 0.34f * s, 0.16f * s };

    DrawRectangleRounded(corpo, 0.3f, 6, cor);
    triangulo((Vector2){ cx - 0.06f * s, cy - 0.17f * s }, (Vector2){ cx - 0.17f * s, cy - 0.02f * s },
              (Vector2){ cx + 0.17f * s, cy - 0.02f * s }, cor);
    triangulo((Vector2){ cx - 0.06f * s, cy - 0.17f * s }, (Vector2){ cx + 0.17f * s, cy - 0.02f * s },
              (Vector2){ cx + 0.06f * s, cy - 0.17f * s }, cor);
    DrawRectangleRec(gargalo, cor);
    DrawRectangleRec(tampa, cor);
    DrawRectangleRec(rotulo, Fade(PAINEL_FUNDO, 0.75f));
}

static void icone_binoculo(float cx, float cy, float s, Color cor)
{
    float raio = 0.17f * s;
    Vector2 esquerda = { cx - 0.20f * s, cy + 0.10f * s };
    Vector2 direita = { cx + 0.20f * s, cy + 0.10f * s };

    DrawRectangleRec((Rectangle){ cx - 0.33f * s, cy - 0.30f * s, 0.20f * s, 0.30f * s }, cor);
    DrawRectangleRec((Rectangle){ cx + 0.13f * s, cy - 0.30f * s, 0.20f * s, 0.30f * s }, cor);
    DrawRectangleRec((Rectangle){ cx - 0.13f * s, cy - 0.16f * s, 0.26f * s, 0.10f * s }, cor);
    DrawCircleV(esquerda, raio, cor);
    DrawCircleV(direita, raio, cor);
    DrawCircleV(esquerda, raio * 0.55f, Fade(PAINEL_FUNDO, 0.85f));
    DrawCircleV(direita, raio * 0.55f, Fade(PAINEL_FUNDO, 0.85f));
}

static void contorno_de_carta(Rectangle carta, Color cor)
{
    DrawRectangleLinesEx(carta, 2.0f, cor);
}

static void espadas(float cx, float cy, float s, Color cor)
{
    triangulo((Vector2){ cx, cy - 0.5f * s }, (Vector2){ cx - 0.42f * s, cy + 0.05f * s },
              (Vector2){ cx + 0.42f * s, cy + 0.05f * s }, cor);
    DrawCircleV((Vector2){ cx - 0.22f * s, cy + 0.1f * s }, 0.22f * s, cor);
    DrawCircleV((Vector2){ cx + 0.22f * s, cy + 0.1f * s }, 0.22f * s, cor);
    triangulo((Vector2){ cx, cy + 0.05f * s }, (Vector2){ cx - 0.18f * s, cy + 0.5f * s },
              (Vector2){ cx + 0.18f * s, cy + 0.5f * s }, cor);
}

static void icone_as(float cx, float cy, float s, Color cor)
{
    Rectangle carta = { cx - 0.26f * s, cy - 0.38f * s, 0.52f * s, 0.76f * s };
    int tamanho = (int)(0.26f * s);

    if (tamanho < 8)
        tamanho = 8;

    contorno_de_carta(carta, cor);
    DrawText("A", (int)(carta.x + 0.08f * s), (int)(carta.y + 0.05f * s), tamanho, cor);
    espadas(cx + 0.04f * s, cy + 0.10f * s, 0.30f * s, cor);
}

static void icone_suborno(float cx, float cy, float s, Color cor)
{
    for (int i = 0; i < 3; i++) {
        float y = cy + 0.22f * s - (float)i * 0.15f * s;

        DrawEllipse((int)cx, (int)(y + 0.05f * s), 0.30f * s, 0.11f * s, Fade(cor, 0.55f));
        DrawEllipse((int)cx, (int)y, 0.30f * s, 0.11f * s, cor);
        DrawEllipseLines((int)cx, (int)y, 0.20f * s, 0.07f * s, Fade(PAINEL_FUNDO, 0.7f));
    }
}

static void icone_marca(float cx, float cy, float s, Color cor)
{
    Rectangle carta = { cx - 0.26f * s, cy - 0.38f * s, 0.52f * s, 0.76f * s };

    contorno_de_carta(carta, cor);
    DrawLineEx((Vector2){ carta.x + 0.10f * s, carta.y + carta.height - 0.12f * s },
               (Vector2){ carta.x + carta.width - 0.08f * s, carta.y + 0.14f * s }, 2.5f, cor);
    DrawCircleV((Vector2){ carta.x + carta.width - 0.10f * s, carta.y + 0.10f * s }, 0.05f * s, cor);
}

static void icone_mascara(float cx, float cy, float s, Color cor)
{
    DrawEllipse((int)cx, (int)cy, 0.44f * s, 0.20f * s, cor);
    triangulo((Vector2){ cx, cy + 0.02f * s }, (Vector2){ cx - 0.09f * s, cy + 0.22f * s },
              (Vector2){ cx + 0.09f * s, cy + 0.22f * s }, PAINEL_FUNDO);
    DrawEllipse((int)(cx - 0.20f * s), (int)(cy - 0.02f * s), 0.11f * s, 0.07f * s, PAINEL_FUNDO);
    DrawEllipse((int)(cx + 0.20f * s), (int)(cy - 0.02f * s), 0.11f * s, 0.07f * s, PAINEL_FUNDO);
}

static void icone_aviso(float cx, float cy, float s, Color cor)
{
    Vector2 cima = { cx, cy - 0.40f * s };
    Vector2 esquerda = { cx - 0.44f * s, cy + 0.36f * s };
    Vector2 direita = { cx + 0.44f * s, cy + 0.36f * s };
    int tamanho = (int)(0.42f * s);

    DrawLineEx(cima, esquerda, 3.0f, cor);
    DrawLineEx(esquerda, direita, 3.0f, cor);
    DrawLineEx(direita, cima, 3.0f, cor);
    DrawText("!", (int)(cx - (float)MeasureText("!", tamanho) / 2.0f), (int)(cy - 0.14f * s), tamanho, cor);
}

static void icone_ficha(float cx, float cy, float s, Color cor)
{
    float raio = 0.42f * s;
    Vector2 centro = { cx, cy };

    DrawCircleV(centro, raio, cor);

    for (int i = 0; i < 6; i++) {
        float angulo = (float)i * PI_F / 3.0f;
        Vector2 ponto = { cx + cosf(angulo) * raio * 0.82f, cy + sinf(angulo) * raio * 0.82f };

        DrawCircleV(ponto, raio * 0.13f, PAINEL_TEXTO);
    }

    DrawCircleV(centro, raio * 0.58f, Fade(PAINEL_FUNDO, 0.55f));
    DrawCircleLines((int)cx, (int)cy, raio * 0.58f, PAINEL_TEXTO);
}

static void icone_olho(float cx, float cy, float s, Color cor)
{
    DrawEllipse((int)cx, (int)cy, 0.42f * s, 0.20f * s, cor);
    DrawEllipse((int)cx, (int)cy, 0.36f * s, 0.15f * s, PAINEL_FUNDO);
    DrawCircleV((Vector2){ cx, cy }, 0.13f * s, cor);
    DrawCircleV((Vector2){ cx, cy }, 0.05f * s, PAINEL_FUNDO);
}

static void icone_carta_mesa(float cx, float cy, float s, Color cor)
{
    Rectangle carta = { cx - 0.20f * s, cy - 0.36f * s, 0.40f * s, 0.56f * s };

    contorno_de_carta(carta, cor);
    DrawRectangleRec((Rectangle){ cx - 0.42f * s, cy + 0.28f * s, 0.84f * s, 0.06f * s }, cor);
}

static void icone_carta_mao(float cx, float cy, float s, Color cor)
{
    contorno_de_carta((Rectangle){ cx - 0.32f * s, cy - 0.30f * s, 0.38f * s, 0.56f * s }, Fade(cor, 0.6f));
    contorno_de_carta((Rectangle){ cx - 0.08f * s, cy - 0.24f * s, 0.38f * s, 0.56f * s }, cor);
}

void painel_icone(Icone icone, Rectangle area, Color cor)
{
    float s = area.width < area.height ? area.width : area.height;
    float cx = area.x + area.width / 2.0f;
    float cy = area.y + area.height / 2.0f;

    switch (icone) {
        case ICONE_GARRAFA: icone_garrafa(cx, cy, s, cor); break;
        case ICONE_BINOCULO: icone_binoculo(cx, cy, s, cor); break;
        case ICONE_AS: icone_as(cx, cy, s, cor); break;
        case ICONE_SUBORNO: icone_suborno(cx, cy, s, cor); break;
        case ICONE_MARCA: icone_marca(cx, cy, s, cor); break;
        case ICONE_MASCARA: icone_mascara(cx, cy, s, cor); break;
        case ICONE_AVISO: icone_aviso(cx, cy, s, cor); break;
        case ICONE_FICHA: icone_ficha(cx, cy, s, cor); break;
        case ICONE_OLHO: icone_olho(cx, cy, s, cor); break;
        case ICONE_CARTA_MESA: icone_carta_mesa(cx, cy, s, cor); break;
        case ICONE_CARTA_MAO: icone_carta_mao(cx, cy, s, cor); break;
    }
}

void painel_barra(Rectangle area, float fracao, int segmentos, Color cor)
{
    Color vazio = { 38, 30, 28, 255 };

    if (fracao < 0.0f)
        fracao = 0.0f;
    if (fracao > 1.0f)
        fracao = 1.0f;

    if (segmentos <= 1) {
        DrawRectangleRec(area, vazio);
        DrawRectangleRec((Rectangle){ area.x, area.y, area.width * fracao, area.height }, cor);
        DrawRectangleGradientV((int)area.x, (int)area.y, (int)(area.width * fracao), (int)(area.height / 2.0f),
                               Fade(WHITE, 0.25f), Fade(WHITE, 0.0f));
        DrawRectangleLinesEx(area, 1.0f, Fade(PAINEL_OURO, 0.7f));
        return;
    }

    float vao = 4.0f;
    float largura = (area.width - vao * (float)(segmentos - 1)) / (float)segmentos;

    for (int i = 0; i < segmentos; i++) {
        Rectangle bloco = { area.x + (float)i * (largura + vao), area.y, largura, area.height };
        float cheio = fracao * (float)segmentos - (float)i;

        if (cheio < 0.0f)
            cheio = 0.0f;
        if (cheio > 1.0f)
            cheio = 1.0f;

        DrawRectangleRec(bloco, vazio);
        DrawRectangleRec((Rectangle){ bloco.x, bloco.y, bloco.width * cheio, bloco.height }, cor);
        DrawRectangleLinesEx(bloco, 1.0f, Fade(PAINEL_OURO, 0.55f));
    }
}

/** Desenha uma linha pronta e devolve a altura que ela ocupou. */
static int escrever_linha(const char *linha, Rectangle area, int y, int tamanho, Color cor)
{
    DrawText(linha, (int)area.x, y, tamanho, cor);
    return tamanho + tamanho / 3;
}

int painel_texto_quebrado(const char *texto, Rectangle area, int tamanho, Color cor)
{
    char linha[256];
    char tentativa[256];
    size_t usado = 0;
    int y = (int)area.y;
    const char *p = texto;

    linha[0] = '\0';

    while (p != NULL && *p != '\0') {
        const char *inicio = p;
        size_t n;
        size_t tamanho_tentativa;

        while (*p != '\0' && *p != ' ' && *p != '\n')
            p++;

        n = (size_t)(p - inicio);
        if (n > sizeof tentativa - 2)
            n = sizeof tentativa - 2;

        /* tentativa = linha + " " + palavra, sem passar do buffer */
        tamanho_tentativa = 0;
        if (usado > 0) {
            size_t copia = usado < sizeof tentativa - 2 - n ? usado : sizeof tentativa - 2 - n;

            memcpy(tentativa, linha, copia);
            tamanho_tentativa = copia;
            tentativa[tamanho_tentativa++] = ' ';
        }
        memcpy(tentativa + tamanho_tentativa, inicio, n);
        tamanho_tentativa += n;
        tentativa[tamanho_tentativa] = '\0';

        if (usado > 0 && MeasureText(tentativa, tamanho) > (int)area.width) {
            y += escrever_linha(linha, area, y, tamanho, cor);
            memcpy(linha, inicio, n);
            usado = n;
            linha[usado] = '\0';
        } else {
            memcpy(linha, tentativa, tamanho_tentativa + 1);
            usado = tamanho_tentativa;
        }

        if (*p == '\n') {
            y += escrever_linha(linha, area, y, tamanho, cor);
            usado = 0;
            linha[0] = '\0';
            p++;
            continue;
        }

        while (*p == ' ')
            p++;
    }

    if (usado > 0)
        y += escrever_linha(linha, area, y, tamanho, cor);

    return y - (int)area.y;
}

void painel_vinheta(int largura, int altura, Color cor, float intensidade)
{
    Color forte = Fade(cor, intensidade);
    Color nada = Fade(cor, 0.0f);
    int borda = altura / 4;

    DrawRectangleGradientV(0, 0, largura, borda, forte, nada);
    DrawRectangleGradientV(0, altura - borda, largura, borda, nada, forte);
    DrawRectangleGradientH(0, 0, borda, altura, forte, nada);
    DrawRectangleGradientH(largura - borda, 0, borda, altura, nada, forte);
}

void painel_botao(Rectangle area, const char *rotulo, int tamanho, bool principal, bool sobre, bool ativo)
{
    Color fundo = principal ? PAINEL_VERMELHO : PAINEL_FUNDO;
    Color borda = ativo ? PAINEL_OURO : PAINEL_TEXTO_SUAVE;
    int largura = MeasureText(rotulo, tamanho);

    if (!ativo)
        fundo = (Color){ 30, 27, 26, 255 };
    else if (sobre)
        fundo = principal ? PAINEL_VERMELHO_VIVO : PAINEL_FUNDO_CLARO;

    DrawRectangleRec(area, fundo);
    DrawRectangleLinesEx(area, sobre && ativo ? 2.0f : 1.5f, sobre && ativo ? PAINEL_OURO_VIVO : borda);
    DrawText(rotulo, (int)(area.x + (area.width - (float)largura) / 2.0f),
             (int)(area.y + (area.height - (float)tamanho) / 2.0f), tamanho,
             ativo ? PAINEL_TEXTO : PAINEL_TEXTO_SUAVE);
}

void painel_papel(Rectangle area, const char *texto, int tamanho)
{
    float dente = 6.0f;

    DrawRectangleRec((Rectangle){ area.x + 4.0f, area.y + 5.0f, area.width, area.height }, Fade(BLACK, 0.45f));
    DrawRectangleRec(area, PAINEL_PAPEL);
    DrawRectangleGradientV((int)area.x, (int)area.y, (int)area.width, (int)area.height,
                           Fade(WHITE, 0.15f), Fade((Color){ 120, 90, 50, 255 }, 0.18f));

    /* Borda de baixo rasgada: dentes alternados na cor do papel mais escura. */
    for (float x = area.x; x < area.x + area.width - dente; x += dente * 2.0f) {
        triangulo((Vector2){ x, area.y + area.height }, (Vector2){ x + dente, area.y + area.height + dente },
                  (Vector2){ x + dente * 2.0f, area.y + area.height }, PAINEL_PAPEL);
    }

    painel_texto_quebrado(texto, (Rectangle){ area.x + 18.0f, area.y + 14.0f, area.width - 36.0f, area.height - 20.0f },
                          tamanho, PAINEL_TINTA);
}

void painel_tambor(Vector2 centro, float raio, int balas, float giro)
{
    Color metal = { 58, 56, 60, 255 };
    Color metal_claro = { 104, 100, 106, 255 };
    Color latao = { 206, 160, 72, 255 };

    DrawCircleV(centro, raio, metal);
    DrawCircleLines((int)centro.x, (int)centro.y, raio, metal_claro);

    for (int i = 0; i < 6; i++) {
        float angulo = giro + (float)i * PI_F / 3.0f - PI_F / 2.0f;
        Vector2 camara = { centro.x + cosf(angulo) * raio * 0.58f, centro.y + sinf(angulo) * raio * 0.58f };
        float raio_camara = raio * 0.24f;

        DrawCircleV(camara, raio_camara, (Color){ 16, 14, 16, 255 });

        if (i < balas) {
            DrawCircleV(camara, raio_camara * 0.78f, latao);
            DrawCircleV(camara, raio_camara * 0.32f, (Color){ 150, 108, 40, 255 });
        }

        DrawCircleLines((int)camara.x, (int)camara.y, raio_camara, metal_claro);
    }

    DrawCircleV(centro, raio * 0.14f, metal_claro);
}

void painel_clarao(Vector2 centro, float raio, float intensidade)
{
    Color fogo = Fade((Color){ 255, 128, 34, 255 }, intensidade);
    Color luz = Fade((Color){ 255, 232, 160, 255 }, intensidade);

    if (intensidade <= 0.0f)
        return;

    DrawCircleGradient((int)centro.x, (int)centro.y, raio * 1.7f, Fade((Color){ 255, 196, 90, 255 }, 0.6f * intensidade),
                       Fade((Color){ 255, 120, 30, 255 }, 0.0f));

    for (int i = 0; i < 12; i++) {
        float angulo = (float)i * 2.0f * PI_F / 12.0f;
        float comprimento = raio * (i % 2 == 0 ? 1.0f : 0.6f);
        Vector2 ponta = { centro.x + cosf(angulo) * comprimento, centro.y + sinf(angulo) * comprimento };
        Vector2 lado1 = { centro.x + cosf(angulo + 0.26f) * raio * 0.3f, centro.y + sinf(angulo + 0.26f) * raio * 0.3f };
        Vector2 lado2 = { centro.x + cosf(angulo - 0.26f) * raio * 0.3f, centro.y + sinf(angulo - 0.26f) * raio * 0.3f };
        Vector2 ponta_clara = { centro.x + cosf(angulo) * comprimento * 0.65f,
                                centro.y + sinf(angulo) * comprimento * 0.65f };

        triangulo(ponta, lado1, lado2, fogo);
        triangulo(ponta_clara, lado1, lado2, luz);
    }

    DrawCircleV(centro, raio * 0.32f, luz);
    DrawCircleV(centro, raio * 0.18f, Fade(WHITE, intensidade));
}
