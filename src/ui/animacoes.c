#include "ui/animacoes.h"
#include "raylib.h"

#include <stddef.h>

typedef struct {
    Image imagem;
    Texture2D textura;
    int total_quadros;
} AnimacaoGif;

typedef enum {
    ANIMACAO_PARADA,
    ANIMACAO_EMBARALHAR,
    ANIMACAO_DISTRIBUIR
} FaseAnimacao;

static AnimacaoGif embaralhamento;
static AnimacaoGif distribuicao;
static FaseAnimacao fase;
static int quadro_atual;
static double tempo_no_quadro;
static bool ignorar_proximo_inicio;

static AnimacaoGif *animacao_atual(void)
{
    if (fase == ANIMACAO_EMBARALHAR)
        return &embaralhamento;
    if (fase == ANIMACAO_DISTRIBUIR)
        return &distribuicao;
    return NULL;
}

static bool carregada(const AnimacaoGif *animacao)
{
    return animacao->imagem.data != NULL && animacao->textura.id != 0 &&
           animacao->total_quadros > 0;
}

static void carregar(AnimacaoGif *animacao, const char *caminho)
{
    animacao->imagem = LoadImageAnim(caminho, &animacao->total_quadros);
    if (animacao->imagem.data != NULL && animacao->total_quadros > 0)
        animacao->textura = LoadTextureFromImage(animacao->imagem);
}

static void descarregar(AnimacaoGif *animacao)
{
    if (animacao->textura.id != 0)
        UnloadTexture(animacao->textura);
    if (animacao->imagem.data != NULL)
        UnloadImage(animacao->imagem);
    *animacao = (AnimacaoGif){ 0 };
}

static void mostrar_quadro(void)
{
    AnimacaoGif *animacao = animacao_atual();
    size_t deslocamento;

    if (animacao == NULL)
        return;

    deslocamento = (size_t)quadro_atual * (size_t)animacao->imagem.width *
                   (size_t)animacao->imagem.height * 4u;
    UpdateTexture(animacao->textura, (unsigned char *)animacao->imagem.data + deslocamento);
}

static void comecar(FaseAnimacao nova_fase)
{
    fase = nova_fase;
    quadro_atual = 0;
    mostrar_quadro();
}

void animacoes_iniciar(void)
{
    carregar(&embaralhamento, "assets/gifs/embaralhamento.gif");
    carregar(&distribuicao, "assets/gifs/distribuicao.gif");
}

void animacoes_encerrar(void)
{
    animacoes_cancelar();
    descarregar(&embaralhamento);
    descarregar(&distribuicao);
}

void animacoes_cancelar(void)
{
    fase = ANIMACAO_PARADA;
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
    fase = ANIMACAO_PARADA;
    quadro_atual = 0;
    tempo_no_quadro = 0.0;

    if (ignorar_proximo_inicio) {
        ignorar_proximo_inicio = false;
        return;
    }

    if (carregada(&embaralhamento))
        comecar(ANIMACAO_EMBARALHAR);
    else if (carregada(&distribuicao))
        comecar(ANIMACAO_DISTRIBUIR);
}

bool animacoes_embaralhando(void)
{
    return fase == ANIMACAO_EMBARALHAR;
}

bool animacoes_ativas(void)
{
    return fase != ANIMACAO_PARADA;
}

static double duracao_do_quadro(const AnimacaoGif *animacao)
{
    if (fase == ANIMACAO_DISTRIBUIR)
        return quadro_atual == animacao->total_quadros - 1 ? 0.85 : 0.09;

    if (quadro_atual == 0)
        return 0.42;

    if (quadro_atual == animacao->total_quadros - 1)
        return 0.34;

    return 0.08;
}

void animacoes_atualizar(double segundos)
{
    if (!animacoes_ativas())
        return;

    tempo_no_quadro += segundos;

    while (animacoes_ativas()) {
        AnimacaoGif *animacao = animacao_atual();
        double duracao = duracao_do_quadro(animacao);

        if (tempo_no_quadro < duracao)
            break;

        tempo_no_quadro -= duracao;
        quadro_atual++;

        if (quadro_atual >= animacao->total_quadros) {
            if (fase == ANIMACAO_EMBARALHAR && carregada(&distribuicao)) {
                comecar(ANIMACAO_DISTRIBUIR);
                continue;
            }

            fase = ANIMACAO_PARADA;
            tempo_no_quadro = 0.0;
            break;
        }

        mostrar_quadro();
    }
}

void animacoes_desenhar(void)
{
    AnimacaoGif *animacao = animacao_atual();

    if (animacao != NULL)
        DrawTexture(animacao->textura, 0, 0, WHITE);
}
