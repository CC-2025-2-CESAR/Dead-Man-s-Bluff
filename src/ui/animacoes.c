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
    ANIMACAO_DISTRIBUIR,
    ANIMACAO_CARTA_MESA,
    ANIMACAO_VIRAR_BOARD
} FaseAnimacao;

static AnimacaoGif embaralhamento;
static AnimacaoGif distribuicao;
static AnimacaoGif carta_mesa;
static FaseAnimacao fase;
static int quadro_atual;
static double tempo_no_quadro;
static bool ignorar_proximo_inicio;
static bool restaurando_sem_animacoes;
static int lotes_board[5];
static int total_lotes_board;
static int lote_atual;
static int entregues_no_lote;
static int board_frentes;
static int board_versos;
static int board_cartas_agendadas;
static double tempo_virada;

static AnimacaoGif *animacao_atual(void)
{
    if (fase == ANIMACAO_EMBARALHAR)
        return &embaralhamento;
    if (fase == ANIMACAO_DISTRIBUIR)
        return &distribuicao;
    if (fase == ANIMACAO_CARTA_MESA)
        return &carta_mesa;
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

static void comecar_proximo_lote(void)
{
    if (lote_atual < total_lotes_board && carregada(&carta_mesa))
        comecar(ANIMACAO_CARTA_MESA);
    else
        fase = ANIMACAO_PARADA;
}

void animacoes_iniciar(void)
{
    carregar(&embaralhamento, "assets/gifs/embaralhamento.gif");
    carregar(&distribuicao, "assets/gifs/distribuicao.gif");
    carregar(&carta_mesa, "assets/gifs/carta_para_mesa.gif");
}

void animacoes_encerrar(void)
{
    animacoes_cancelar();
    descarregar(&embaralhamento);
    descarregar(&distribuicao);
    descarregar(&carta_mesa);
}

void animacoes_cancelar(void)
{
    fase = ANIMACAO_PARADA;
    ignorar_proximo_inicio = false;
    quadro_atual = 0;
    tempo_no_quadro = 0.0;
    restaurando_sem_animacoes = false;
    total_lotes_board = 0;
    lote_atual = 0;
    entregues_no_lote = 0;
    board_frentes = 0;
    board_versos = 0;
    board_cartas_agendadas = 0;
    tempo_virada = 0.0;
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
    total_lotes_board = 0;
    lote_atual = 0;
    entregues_no_lote = 0;
    board_frentes = 0;
    board_versos = 0;
    board_cartas_agendadas = 0;
    tempo_virada = 0.0;
    restaurando_sem_animacoes = ignorar_proximo_inicio;

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

bool animacoes_iniciais_ativas(void)
{
    return fase == ANIMACAO_EMBARALHAR || fase == ANIMACAO_DISTRIBUIR;
}

/* Um evento vale por um lote: três cartas no flop, uma no turn ou no river. */
void animacoes_board_revelado(int quantidade)
{
    if (quantidade <= 0 || quantidade > 3 || board_cartas_agendadas + quantidade > 5)
        return;

    board_cartas_agendadas += quantidade;

    if (restaurando_sem_animacoes || !carregada(&carta_mesa)) {
        board_frentes += quantidade;
        return;
    }

    if (total_lotes_board >= 5)
        return;

    lotes_board[total_lotes_board++] = quantidade;
    if (fase == ANIMACAO_PARADA)
        comecar_proximo_lote();
}

int animacoes_board_frentes(void)
{
    return board_frentes;
}

int animacoes_board_versos(void)
{
    return board_versos;
}
float animacoes_board_virada(void)
{
    return fase == ANIMACAO_VIRAR_BOARD ? (float)(tempo_virada / 0.38) : -1.0f;
}

static double duracao_do_quadro(const AnimacaoGif *animacao)
{
    if (fase == ANIMACAO_DISTRIBUIR)
        return quadro_atual == animacao->total_quadros - 1 ? 0.85 : 0.09;

    if (fase == ANIMACAO_CARTA_MESA)
        return 0.04;

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

    if (fase == ANIMACAO_VIRAR_BOARD) {
        tempo_virada += segundos;
        if (tempo_virada >= 0.38) {
            board_frentes += board_versos;
            board_versos = 0;
            lote_atual++;
            entregues_no_lote = 0;
            comecar_proximo_lote();
        }
        return;
    }

    tempo_no_quadro += segundos;

    while (animacoes_ativas()) {
        AnimacaoGif *animacao = animacao_atual();
        if (animacao == NULL)
            break;
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

            if (fase == ANIMACAO_DISTRIBUIR) {
                comecar_proximo_lote();
                continue;
            }

            if (fase == ANIMACAO_CARTA_MESA) {
                board_versos++;
                entregues_no_lote++;
                if (entregues_no_lote < lotes_board[lote_atual]) {
                    comecar(ANIMACAO_CARTA_MESA);
                    continue;
                }
                fase = ANIMACAO_VIRAR_BOARD;
                tempo_virada = 0.0;
                tempo_no_quadro = 0.0;
                break;
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
