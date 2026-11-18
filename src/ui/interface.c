#include "ui/interface.h"
#include "ui/animacoes.h"
#include "ui/cartas.h"
#include "ui/painel.h"
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TITULO_JANELA "Dead Man's Bluff"
#define EVENTOS_NO_REGISTRO 6
#define TAMANHO_LINHA 160

static char registro_de_eventos[EVENTOS_NO_REGISTRO][TAMANHO_LINHA];
static int eventos_registrados;
static Texture2D textura_menu = { 0 };
static Texture2D textura_vitoria = { 0 };
static Texture2D textura_derrota = { 0 };

static Texture2D textura_mesa = { 0 };
static Texture2D textura_protagonista = { 0 };
static Texture2D textura_numeros[10];
static Texture2D textura_confirmacao_novo_jogo = { 0 };
static Texture2D textura_configuracoes = { 0 };
static Texture2D textura_dealer = { 0 };

/* O dealer muda de cara com a suspeita: calmo (dealer.png), desconfiado e
 * bravo. O revólver apontado para o jogador é o bravo armado: é ele quem
 * cobra a trapaça descoberta na roleta russa. dealer_revolver.png foi feita a
 * partir de dealer_bravo.png. */
static Texture2D textura_dealer_desconfiado = { 0 };
static Texture2D textura_dealer_bravo = { 0 };
static Texture2D textura_dealer_revolver = { 0 };

/* As três expressões do oponente do confronto em retratos_do_confronto, por
 * estilo da IA (POKER_AI_CAUTIOUS, BALANCED e AGGRESSIVE). Carregadas só para
 * o confronto em jogo, para não ocupar memória com as nove. */
static Texture2D retratos[3];
static int retratos_do_confronto = -1;

static const Color COR_FUNDO = { 12, 46, 31, 255 };
static const Color COR_OURO = { 212, 175, 55, 255 };
static const Color COR_TEXTO = { 235, 235, 225, 255 };
static const Color COR_TEXTO_SUAVE = { 150, 170, 155, 255 };
static const Color COR_FUNDO_CLARO = { 52, 88, 70, 255 };

static const char *ROTULOS_MENU[MENU_TOTAL] = { "CONTINUAR", "NOVO JOGO", "CONFIGURAÇÕES", "SAIR" };

/** Espaço que a barra de título e a barra de tarefas tiram da altura do monitor. */
#define MARGEM_VERTICAL_DA_JANELA 80

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

/** Ajusta um PNG às dimensões lógicas da tela, preservando sua transparência. */
static void desenhar_textura_tela(Texture2D textura)
{
    if (textura.id == 0)
        return;

    DrawTexturePro(textura,
                   (Rectangle){ 0, 0, (float)textura.width, (float)textura.height },
                   (Rectangle){ 0, 0, (float)LARGURA_TELA, (float)ALTURA_TELA },
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
}

/**
 * Monta a câmera que leva o desenho de LARGURA_TELA por ALTURA_TELA para a
 * janela real: escala igual nos dois eixos e centralizada, deixando faixas
 * nas bordas se a proporção da janela for outra.
 */
static Camera2D camera_da_janela(void)
{
    float largura = (float)GetScreenWidth();
    float altura = (float)GetScreenHeight();
    float escala_x = largura / LARGURA_TELA;
    float escala_y = altura / ALTURA_TELA;
    float escala = escala_x < escala_y ? escala_x : escala_y;
    Camera2D camera = { 0 };

    camera.zoom = escala;
    camera.offset = (Vector2){ (largura - LARGURA_TELA * escala) / 2.0f,
                               (altura - ALTURA_TELA * escala) / 2.0f };
    return camera;
}

#define BOTOES_MAXIMOS 96
#define BOTAO_NENHUM (-1)
#define BOTAO_RESOLUCAO 100
#define BOTAO_VOLTAR_CONFIGURACOES 200
#define BOTAO_SIM 210
#define BOTAO_NAO 211
#define BOTAO_DESISTIR 300
#define BOTAO_PASSAR_OU_PAGAR 301
#define BOTAO_APOSTAR 302
#define BOTAO_ALL_IN 303
#define BOTAO_AUMENTAR 304
#define BOTAO_DIMINUIR 305
#define BOTAO_CONTINUAR 306
#define BOTAO_VOLTAR_DA_MESA 307

/**
 * Área clicável de uma tela. Cada tela registra os seus botões ao desenhar, e
 * a leitura do mouse no quadro seguinte consulta o que foi registrado. Os
 * valores de id: 0 a MENU_TOTAL - 1 são as opções do menu, BOTAO_RESOLUCAO
 * mais a posição são as resoluções, e os demais estão acima.
 */
typedef struct {
    Rectangle area;
    int id;
} Botao;

static Botao botoes[BOTOES_MAXIMOS];
static int total_de_botoes;
static bool cursor_sobre_botao;
static bool cursor_em_mao;

/** Posição do mouse nas coordenadas de LARGURA_TELA por ALTURA_TELA. */
static Vector2 posicao_do_mouse(void)
{
    return GetScreenToWorld2D(GetMousePosition(), camera_da_janela());
}

static bool mouse_sobre(Rectangle area)
{
    return CheckCollisionPointRec(posicao_do_mouse(), area);
}

/** Registra uma área clicável do quadro e, se o mouse está sobre ela, pede o cursor de mão. */
static void registrar_botao(Rectangle area, int id)
{
    if (total_de_botoes < BOTOES_MAXIMOS)
        botoes[total_de_botoes++] = (Botao){ area, id };

    if (mouse_sobre(area))
        cursor_sobre_botao = true;
}

/** Devolve o id do botão sob o mouse, ou BOTAO_NENHUM. */
static int botao_sob_o_mouse(void)
{
    Vector2 mouse = posicao_do_mouse();

    for (int i = total_de_botoes - 1; i >= 0; i--) {
        if (CheckCollisionPointRec(mouse, botoes[i].area))
            return botoes[i].id;
    }

    return BOTAO_NENHUM;
}

/** Devolve o id do botão em que o jogador clicou neste quadro, ou BOTAO_NENHUM. */
static int botao_clicado(void)
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? botao_sob_o_mouse() : BOTAO_NENHUM;
}

/** Indica se o mouse mudou de lugar neste quadro. */
static bool mouse_se_moveu(void)
{
    Vector2 delta = GetMouseDelta();

    return delta.x != 0.0f || delta.y != 0.0f;
}

/**
 * Desenha um texto que também é um botão: ele muda de cor e ganha um
 * sublinhado quando o mouse passa por cima.
 *
 * @return a largura do texto, para quem quiser colocar outro ao lado.
 */
static int botao_de_texto(const char *texto, int x, int y, int tamanho, Color cor, int id)
{
    int largura = MeasureText(texto, tamanho);
    Rectangle area = { (float)(x - 8), (float)(y - 4), (float)(largura + 16), (float)(tamanho + 8) };
    bool sobre = mouse_sobre(area);

    DrawText(texto, x, y, tamanho, sobre ? COR_TEXTO : cor);

    if (sobre)
        DrawRectangle(x, y + tamanho + 2, largura, 2, COR_OURO);

    registrar_botao(area, id);
    return largura;
}

/** Como botao_de_texto, centralizado na largura da tela. */
static void botao_centralizado(const char *texto, int y, int tamanho, Color cor, int id)
{
    botao_de_texto(texto, (LARGURA_TELA - MeasureText(texto, tamanho)) / 2, y, tamanho, cor, id);
}

/** Começa um quadro: limpa a janela e passa a desenhar nas coordenadas de LARGURA_TELA por ALTURA_TELA. */
static void comecar_desenho(void)
{
    total_de_botoes = 0;
    cursor_sobre_botao = false;

    BeginDrawing();
    ClearBackground(COR_FUNDO);
    BeginMode2D(camera_da_janela());
}

/** Termina o quadro começado por comecar_desenho e troca o cursor se o mouse entrou ou saiu de um botão. */
static void terminar_desenho(void)
{
    EndMode2D();
    EndDrawing();

    if (cursor_sobre_botao != cursor_em_mao) {
        SetMouseCursor(cursor_sobre_botao ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
        cursor_em_mao = cursor_sobre_botao;
    }
}

void interface_iniciar(void)
{
    InitWindow(LARGURA_TELA, ALTURA_TELA, TITULO_JANELA);
    textura_menu = LoadTexture("assets/images/menu.png");
    textura_vitoria = LoadTexture("assets/images/vitoria.png");
    textura_derrota = LoadTexture("assets/images/derrota.png");
    textura_mesa = LoadTexture("assets/images/mesa.png");
    textura_protagonista = LoadTexture("assets/images/protagonista.png");
    for (int i = 0; i < 10; i++)
        textura_numeros[i] = LoadTexture(TextFormat("assets/images/numeros/N%d.png", i));
    textura_confirmacao_novo_jogo = LoadTexture("assets/images/confirmacao_novo_jogo.png");
    textura_configuracoes = LoadTexture("assets/images/configuracoes.png");
    textura_dealer = LoadTexture("assets/images/dealer.png");
    textura_dealer_desconfiado = LoadTexture("assets/images/dealer_desconfiado.png");
    textura_dealer_bravo = LoadTexture("assets/images/dealer_bravo.png");
    textura_dealer_revolver = LoadTexture("assets/images/dealer_revolver.png");
    cartas_iniciar();
    animacoes_iniciar();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
}

void interface_encerrar(void)
{
    if (textura_menu.id != 0)
        UnloadTexture(textura_menu);

    if (textura_vitoria.id != 0)
        UnloadTexture(textura_vitoria);

    if (textura_derrota.id != 0)
        UnloadTexture(textura_derrota);

    if (textura_mesa.id != 0)
        UnloadTexture(textura_mesa);

    if (textura_protagonista.id != 0)
        UnloadTexture(textura_protagonista);

    for (int i = 0; i < 10; i++) {
        if (textura_numeros[i].id != 0)
            UnloadTexture(textura_numeros[i]);
    }

    if (textura_confirmacao_novo_jogo.id != 0)
        UnloadTexture(textura_confirmacao_novo_jogo);

    if (textura_configuracoes.id != 0)
        UnloadTexture(textura_configuracoes);

    if (textura_dealer.id != 0)
        UnloadTexture(textura_dealer);

    if (textura_dealer_desconfiado.id != 0)
        UnloadTexture(textura_dealer_desconfiado);

    if (textura_dealer_bravo.id != 0)
        UnloadTexture(textura_dealer_bravo);

    if (textura_dealer_revolver.id != 0)
        UnloadTexture(textura_dealer_revolver);

    for (int i = 0; i < 3; i++) {
        if (retratos[i].id != 0)
            UnloadTexture(retratos[i]);
        retratos[i] = (Texture2D){ 0 };
    }
    retratos_do_confronto = -1;

    cartas_encerrar();
    animacoes_encerrar();

    CloseWindow();
}

bool interface_fechar_solicitado(void)
{
    return WindowShouldClose();
}

/** Anda uma posição no menu, pulando Continuar quando não há jogo salvo. */
static OpcaoMenu mover_no_menu(OpcaoMenu atual, int sentido, bool continuar_disponivel)
{
    int destino = ((int)atual + sentido + MENU_TOTAL) % MENU_TOTAL;

    if (destino == MENU_CONTINUAR && !continuar_disponivel)
        destino = (destino + sentido + MENU_TOTAL) % MENU_TOTAL;

    return (OpcaoMenu)destino;
}

bool interface_menu_atualizar(OpcaoMenu *selecionada, bool continuar_disponivel)
{
    if (*selecionada == MENU_CONTINUAR && !continuar_disponivel)
        *selecionada = MENU_NOVO_JOGO;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        *selecionada = mover_no_menu(*selecionada, 1, continuar_disponivel);

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        *selecionada = mover_no_menu(*selecionada, -1, continuar_disponivel);

    int apontada = botao_sob_o_mouse();
    int clicada = botao_clicado();
    bool clicou = clicada >= 0 && clicada < MENU_TOTAL;

    if (mouse_se_moveu() && apontada >= 0 && apontada < MENU_TOTAL)
        *selecionada = (OpcaoMenu)apontada;

    if (clicou)
        *selecionada = (OpcaoMenu)clicada;

    return clicou || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

void interface_menu_desenhar(OpcaoMenu selecionada, bool continuar_disponivel, const char *aviso)
{
    comecar_desenho();

    if (textura_menu.id != 0) {
        DrawTexturePro(
            textura_menu,
            (Rectangle){ 0, 0, (float)textura_menu.width, (float)textura_menu.height },
            (Rectangle){ 0, 0, (float)LARGURA_TELA, (float)ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    }

    for (int i = 0; i < MENU_TOTAL; i++) {
        int y = 345 + i * 62;
        bool apagada = i == MENU_CONTINUAR && !continuar_disponivel;
        int largura = MeasureText(ROTULOS_MENU[i], 44);

        if (i == (int)selecionada && !apagada)
            desenhar_texto_centralizado(TextFormat("> %s <", ROTULOS_MENU[i]), y, 44, COR_OURO);
        else
            desenhar_texto_centralizado(ROTULOS_MENU[i], y, 44, apagada ? COR_FUNDO_CLARO : COR_TEXTO_SUAVE);

        if (!apagada)
            registrar_botao((Rectangle){ (float)((LARGURA_TELA - largura) / 2 - 40), (float)(y - 6),
                                         (float)(largura + 80), 56.0f }, i);
    }

    if (aviso != NULL)
        desenhar_texto_centralizado(aviso, 600, 24, COR_OURO);

    desenhar_texto_centralizado("Clique ou use W/S e ENTER para escolher", ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);

    terminar_desenho();
}

RespostaConfirmacao interface_confirmacao_ler_resposta(void)
{
    int clicada = botao_clicado();

    if (IsKeyPressed(KEY_ESCAPE) || clicada == BOTAO_NAO)
        return RESPOSTA_NAO;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || clicada == BOTAO_SIM)
        return RESPOSTA_SIM;

    return RESPOSTA_NENHUMA;
}

void interface_confirmacao_desenhar(const char *pergunta, const char *detalhe)
{
    comecar_desenho();

    if (textura_confirmacao_novo_jogo.id != 0) {
        desenhar_textura_tela(textura_confirmacao_novo_jogo);
    } else {
        desenhar_texto_centralizado(pergunta, 240, 56, COR_OURO);
        DrawRectangle((LARGURA_TELA - 320) / 2, 320, 320, 3, COR_OURO);
        desenhar_texto_centralizado(detalhe, 360, 28, COR_TEXTO);
        desenhar_texto_centralizado("Clique, ou ENTER para sim e ESC para não",
                                    ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);
    }

    botao_de_texto("SIM", LARGURA_TELA / 2 - 150 - MeasureText("SIM", 44) / 2, 450, 44, COR_OURO, BOTAO_SIM);
    botao_de_texto("NÃO", LARGURA_TELA / 2 + 150 - MeasureText("NÃO", 44) / 2, 450, 44, COR_TEXTO_SUAVE, BOTAO_NAO);

    terminar_desenho();
}

#define BOTAO_VOLUME 120

static int volume_arrastado = -1;

/** Barra de volume no espaço livre à esquerda da lista de resoluções. */
static Rectangle area_do_volume(int canal)
{
    return (Rectangle){ 50.0f, 323.0f + canal * 84.0f, 160.0f, 8.0f };
}

static bool tecla_de_volume(int tecla)
{
    return IsKeyPressed(tecla) || IsKeyPressedRepeat(tecla);
}

EntradaConfiguracoes interface_configuracoes_ler_entrada(int selecionada)
{
    EntradaConfiguracoes entrada = {0, false, false, -1, -1, -1, 0};
    int apontada = botao_sob_o_mouse();
    int clicada = botao_clicado();
    int total = RESOLUCOES_TOTAL + VOLUMES_TOTAL;
    int foco;

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        entrada.movimento = 1;
    } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        entrada.movimento = -1;
    }

    entrada.confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    entrada.voltar = IsKeyPressed(KEY_ESCAPE);

    if (mouse_se_moveu() && apontada >= BOTAO_RESOLUCAO && apontada < BOTAO_RESOLUCAO + RESOLUCOES_TOTAL)
        entrada.apontada = apontada - BOTAO_RESOLUCAO;
    else if (mouse_se_moveu() && apontada >= BOTAO_VOLUME && apontada < BOTAO_VOLUME + VOLUMES_TOTAL)
        entrada.apontada = RESOLUCOES_TOTAL + apontada - BOTAO_VOLUME;

    if (clicada >= BOTAO_RESOLUCAO && clicada < BOTAO_RESOLUCAO + RESOLUCOES_TOTAL) {
        entrada.apontada = clicada - BOTAO_RESOLUCAO;
        entrada.confirmar = true;
    } else if (clicada == BOTAO_VOLTAR_CONFIGURACOES) {
        entrada.voltar = true;
    } else if (clicada >= BOTAO_VOLUME && clicada < BOTAO_VOLUME + VOLUMES_TOTAL) {
        volume_arrastado = clicada - BOTAO_VOLUME;
    }

    if (entrada.voltar || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        volume_arrastado = -1;
    }
    if (volume_arrastado >= 0) {
        Rectangle barra = area_do_volume(volume_arrastado);
        float proporcao = (posicao_do_mouse().x - barra.x) / barra.width;

        if (proporcao < 0.0f) proporcao = 0.0f;
        if (proporcao > 1.0f) proporcao = 1.0f;
        entrada.apontada = RESOLUCOES_TOTAL + volume_arrastado;
        entrada.canal_volume = volume_arrastado;
        entrada.volume = (int)(proporcao * 100.0f + 0.5f);
        entrada.movimento = 0;
        entrada.ajuste_volume = 0;
        entrada.confirmar = false;
    }

    foco = entrada.apontada >= 0 ? entrada.apontada : selecionada;
    foco = (foco + entrada.movimento + total) % total;
    if (foco >= RESOLUCOES_TOTAL) {
        entrada.confirmar = false;
        if (volume_arrastado < 0) {
            if (tecla_de_volume(KEY_LEFT) || tecla_de_volume(KEY_MINUS) || tecla_de_volume(KEY_KP_SUBTRACT)) {
                entrada.ajuste_volume = -5;
            } else if (tecla_de_volume(KEY_RIGHT) || tecla_de_volume(KEY_EQUAL) || tecla_de_volume(KEY_KP_ADD)) {
                entrada.ajuste_volume = 5;
            }
            if (entrada.ajuste_volume != 0)
                entrada.canal_volume = foco - RESOLUCOES_TOTAL;
        }
    }

    return entrada;
}

/** Escreve o nome de uma resolução, como "1280 x 720" ou "TELA CHEIA". */
static void rotulo_da_resolucao(Resolucao resolucao, char *saida, size_t tamanho)
{
    if (resolucao.tela_cheia) {
        snprintf(saida, tamanho, "TELA CHEIA");
    } else {
        snprintf(saida, tamanho, "%d x %d", resolucao.largura, resolucao.altura);
    }
}

static void desenhar_volumes(const Configuracoes *configuracoes, int selecionada)
{
    static const char *rotulos[VOLUMES_TOTAL] = { "GERAL", "SFX", "MÚSICA" };

    DrawText("SOM", 50, 250, 28, COR_OURO);
    for (int canal = 0; canal < VOLUMES_TOTAL; canal++) {
        Rectangle barra = area_do_volume(canal);
        int volume = configuracoes_volume(configuracoes, (CanalVolume)canal);
        bool destacada = selecionada == RESOLUCOES_TOTAL + canal;
        Color cor = destacada ? COR_OURO : COR_TEXTO;
        float preenchido = barra.width * volume / 100.0f;
        const char *valor = TextFormat("%d", volume);

        DrawText(rotulos[canal], (int)barra.x, (int)barra.y - 31, 22, cor);
        DrawText(valor, (int)(barra.x + barra.width) - MeasureText(valor, 22),
                 (int)barra.y - 31, 22, cor);
        DrawRectangleRec(barra, COR_FUNDO_CLARO);
        DrawRectangleRec((Rectangle){barra.x, barra.y, preenchido, barra.height}, COR_OURO);
        DrawCircleV((Vector2){barra.x + preenchido, barra.y + barra.height / 2.0f},
                    destacada ? 9.0f : 7.0f, cor);
        DrawText("0", (int)barra.x, (int)barra.y + 15, 16, COR_TEXTO_SUAVE);
        DrawText("100", (int)(barra.x + barra.width) - MeasureText("100", 16),
                 (int)barra.y + 15, 16, COR_TEXTO_SUAVE);
        registrar_botao((Rectangle){barra.x - 10.0f, barra.y - 35.0f,
                                     barra.width + 20.0f, 64.0f}, BOTAO_VOLUME + canal);
    }
    DrawText("Arraste ou use <- / -> e +/-", 50, 544, 16, COR_TEXTO_SUAVE);
}

void interface_configuracoes_desenhar(const Configuracoes *configuracoes, int selecionada)
{
    comecar_desenho();

    if (textura_configuracoes.id != 0) {
        desenhar_textura_tela(textura_configuracoes);
    } else {
        desenhar_texto_centralizado("CONFIGURAÇÕES", 90, 64, COR_OURO);
        DrawRectangle((LARGURA_TELA - 320) / 2, 175, 320, 3, COR_OURO);
        desenhar_texto_centralizado("RESOLUÇÃO", 215, 32, COR_TEXTO);
        desenhar_texto_centralizado("Clique em uma opção para aplicar, ou use W/S, ENTER e ESC",
                                    ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);
    }

    for (int i = 0; i < RESOLUCOES_TOTAL; i++) {
        Resolucao resolucao = configuracoes_resolucao(i);
        bool cabe = interface_resolucao_cabe(resolucao);
        bool atual = configuracoes != NULL && i == configuracoes->resolucao;
        char rotulo[32];
        char linha[96];
        int y = textura_configuracoes.id != 0 ? 270 + i * 54 : 290 + i * 56;

        rotulo_da_resolucao(resolucao, rotulo, sizeof rotulo);
        snprintf(linha, sizeof linha, "%s%s%s", rotulo,
                 atual ? "  (atual)" : "",
                 cabe ? "" : "  (não cabe no monitor)");

        if (i == selecionada) {
            desenhar_texto_centralizado(TextFormat("> %s <", linha), y, 36,
                                        cabe ? COR_OURO : COR_TEXTO_SUAVE);
        } else {
            desenhar_texto_centralizado(linha, y, 36, cabe ? COR_TEXTO : COR_TEXTO_SUAVE);
        }

        registrar_botao((Rectangle){ (float)((LARGURA_TELA - MeasureText(linha, 36)) / 2 - 40), (float)(y - 6),
                                     (float)(MeasureText(linha, 36) + 80), 48.0f }, BOTAO_RESOLUCAO + i);
    }

    botao_centralizado("VOLTAR", textura_configuracoes.id != 0 ? 555 : 585,
                       30, COR_TEXTO_SUAVE, BOTAO_VOLTAR_CONFIGURACOES);

    desenhar_volumes(configuracoes, selecionada);

    terminar_desenho();
}

bool interface_resolucao_cabe(Resolucao resolucao)
{
    int monitor;

    if (resolucao.tela_cheia) {
        return true;
    }

    monitor = GetCurrentMonitor();
    return resolucao.largura <= GetMonitorWidth(monitor) &&
           resolucao.altura + MARGEM_VERTICAL_DA_JANELA <= GetMonitorHeight(monitor);
}

/** Põe a janela no meio do monitor onde ela está. */
static void centralizar_janela(Resolucao resolucao)
{
    int monitor = GetCurrentMonitor();
    Vector2 origem = GetMonitorPosition(monitor);
    int x = (int)origem.x + (GetMonitorWidth(monitor) - resolucao.largura) / 2;
    int y = (int)origem.y + (GetMonitorHeight(monitor) - resolucao.altura) / 2;

    SetWindowPosition(x, y);
}

bool interface_aplicar_resolucao(Resolucao resolucao)
{
    bool em_tela_cheia = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);

    if (!interface_resolucao_cabe(resolucao)) {
        return false;
    }

    if (resolucao.tela_cheia) {
        if (!em_tela_cheia) {
            ToggleBorderlessWindowed();
        }
        return true;
    }

    if (em_tela_cheia) {
        ToggleBorderlessWindowed();
    }

    SetWindowSize(resolucao.largura, resolucao.altura);
    centralizar_janela(resolucao);
    return true;
}

double interface_tempo_do_quadro(void)
{
    return (double)GetFrameTime();
}

#define BOTAO_TRAPACAS 308
#define BOTAO_TRAPACA 400            /* + grupo, de 0 a GRUPOS_DE_TRAPACA - 1 */
#define BOTAO_USAR 410
#define BOTAO_OPCAO_DO_SUBORNO 411   /* + opção, de 0 a 2 */
#define BOTAO_CARTA_DA_MAO 420       /* + carta, 0 ou 1 */
#define BOTAO_VOLTAR_DAS_TRAPACAS 435
#define BOTAO_GRADE 440              /* + posição da carta na grade, de 0 a 51 */
#define BOTAO_CONTINUAR_TRAPACA 500
#define BOTAO_RECUAR 501
#define BOTAO_FECHAR_AVISO 510

/** As cinco trapaças do menu, na ordem em que aparecem. O suborno se abre em três serviços. */
typedef enum {
    GRUPO_BEBIDA = 0,
    GRUPO_CUMPLICE,
    GRUPO_MANGA,
    GRUPO_SUBORNO,
    GRUPO_MARCAR,
    GRUPOS_DE_TRAPACA
} GrupoDeTrapaca;

static const TipoDePedido PEDIDO_DO_GRUPO[GRUPOS_DE_TRAPACA] = {
    PEDIDO_BEBIDA, PEDIDO_CUMPLICE, PEDIDO_MANGA, PEDIDO_SUBORNO_MESA, PEDIDO_MARCAR_CARTA
};

static const TipoDePedido SERVICOS_DO_SUBORNO[3] = {
    PEDIDO_SUBORNO_MESA, PEDIDO_SUBORNO_MAO, PEDIDO_SUBORNO_VER
};

/**
 * Telas das trapaças, que aparecem por cima da mesa na vez do jogador:
 * a lista com o detalhe da escolhida, os serviços do suborno, a carta da mão
 * (marcar ou manga), a grade com as 52 cartas (suborno de carta) e, antes de
 * qualquer trapaça, a tela de risco com CONTINUAR e RECUAR.
 */
typedef enum {
    TRAPACAS_FECHADAS = 0,
    TRAPACAS_MENU,
    TRAPACAS_SUBORNO,
    TRAPACAS_CARTA_DA_MAO,
    TRAPACAS_ESCOLHER_CARTA,
    TRAPACAS_RISCO
} TelaDeTrapacas;

/** O que aparece por cima da mesa depois de uma trapaça ou da roleta, até o jogador fechar. */
typedef enum {
    AVISO_NENHUM = 0,
    AVISO_CUMPLICE,
    AVISO_CARTAS_VISTAS,
    AVISO_BEBIDA,
    AVISO_DESCOBERTO,
    AVISO_ROLETA
} TipoDeAviso;

static TelaDeTrapacas tela_de_trapacas;
static TelaDeTrapacas tela_antes_do_risco;
static TipoDePedido trapaca_em_andamento;
static int grupo_destacado;
static int servico_destacado;
static int carta_destacada;
static int valor_do_seletor = 14;
static int naipe_do_seletor = POKER_SPADES;
static const char *aviso_das_trapacas;
static PedidoTrapaca pedido_em_espera;
static double inicio_da_tela;
static double inicio_das_trapacas;

static TipoDeAviso aviso_aberto;
static double inicio_do_aviso;
static bool aviso_tem_categoria;
static PokerCategory aviso_categoria;
static PokerCard aviso_cartas[2];
static bool aviso_morreu;
static int aviso_balas;

/* Recado curto no alto da mesa (uma trapaça que não pede tela própria). */
static char texto_do_recado[TAMANHO_LINHA];
static Icone icone_do_recado;
static double inicio_do_recado = -100.0;

/* O que o desenho do quadro anterior viu da partida, para a leitura de entrada consultar. */
static bool vez_do_jogador_vista;
static PokerStatus trapaca_status[PEDIDO_TOTAL];
static bool trapaca_vale_para_a_carta[PEDIDO_TOTAL][2];
static PokerCard cartas_a_vista[10];
static int total_de_cartas_a_vista;

/* O evento de distribuição fixa as cartas do adversário e seus dorsos nesta mão. */
static PokerCard cartas_do_adversario[2];
static bool marcou_carta_do_adversario[2];
static bool adversario_recebeu_cartas;
static int carta_marcada_ampliada = -1;
static bool cartas_do_adversario_visiveis;
static double inicio_da_revelacao = -1.0;
static bool revelacao_em_andamento;

static const Rectangle AREAS_DO_ADVERSARIO[2] = {
    { 766.0f, 412.0f, 94.0f, 132.0f },
    { 872.0f, 412.0f, 94.0f, 132.0f }
};

static bool carta_esta_a_vista(PokerCard carta)
{
    for (int i = 0; i < total_de_cartas_a_vista; i++) {
        if (cartas_a_vista[i].rank == carta.rank && cartas_a_vista[i].suit == carta.suit)
            return true;
    }

    return false;
}

static PedidoTrapaca montar_pedido(TipoDePedido tipo, int carta_da_mao, PokerCard carta)
{
    PedidoTrapaca pedido = { tipo, carta_da_mao, carta };

    return pedido;
}

/** Diz por que uma trapaça não pode ser escolhida agora. */
static const char *motivo_da_trapaca(PokerStatus status)
{
    switch (status) {
        case POKER_ERR_CHIPS:
            return "Você não tem fichas para pagar isso.";

        case POKER_ERR_ACTION:
            return "Isso não vale agora.";

        default:
            break;
    }

    return "Indisponível agora.";
}

/** Troca a tela das trapaças e marca quando ela abriu, para as animações. */
static void trocar_tela(TelaDeTrapacas nova)
{
    tela_de_trapacas = nova;
    inicio_da_tela = GetTime();
    aviso_das_trapacas = NULL;
}

static void fechar_trapacas(void)
{
    tela_de_trapacas = TRAPACAS_FECHADAS;
    aviso_das_trapacas = NULL;
}

/** Abre o menu das trapaças, se for a vez do jogador. */
static void abrir_trapacas(void)
{
    if (!vez_do_jogador_vista)
        return;

    trocar_tela(TRAPACAS_MENU);
    inicio_das_trapacas = inicio_da_tela;
    grupo_destacado = 0;
}

/** Se o grupo pode ser usado agora; o suborno vale se algum dos três serviços vale. */
static PokerStatus situacao_do_grupo(int grupo)
{
    PokerStatus primeiro = POKER_OK;

    if (grupo < 0 || grupo >= GRUPOS_DE_TRAPACA)
        return POKER_ERR_ARGUMENT;

    if (grupo != GRUPO_SUBORNO)
        return trapaca_status[PEDIDO_DO_GRUPO[grupo]];

    for (int i = 0; i < 3; i++) {
        PokerStatus status = trapaca_status[SERVICOS_DO_SUBORNO[i]];

        if (status == POKER_OK)
            return POKER_OK;
        if (i == 0)
            primeiro = status;
    }

    return primeiro;
}

/** Guarda o pedido pronto e mostra o risco antes de tentar. */
static void mostrar_risco(PedidoTrapaca pedido)
{
    pedido_em_espera = pedido;
    tela_antes_do_risco = tela_de_trapacas;
    trocar_tela(TRAPACAS_RISCO);
}

/** Põe o cursor da grade numa carta que o jogador não vê, se a atual estiver à vista. */
static void ajustar_cursor_da_grade(void)
{
    for (int tentativa = 0; tentativa < 52; tentativa++) {
        PokerCard carta = { valor_do_seletor, (PokerSuit)naipe_do_seletor };

        if (!carta_esta_a_vista(carta))
            return;

        valor_do_seletor = valor_do_seletor >= 14 ? 2 : valor_do_seletor + 1;
        if (valor_do_seletor == 2)
            naipe_do_seletor = (naipe_do_seletor + 3) % 4;
    }
}

/** Escolhe uma das cinco trapaças: abre o passo seguinte ou o risco. */
static void escolher_grupo(int grupo)
{
    PokerStatus status = situacao_do_grupo(grupo);
    PokerCard nenhuma = { 0, POKER_CLUBS };

    if (status != POKER_OK) {
        aviso_das_trapacas = motivo_da_trapaca(status);
        return;
    }

    grupo_destacado = grupo;

    switch ((GrupoDeTrapaca)grupo) {
        case GRUPO_BEBIDA:
        case GRUPO_CUMPLICE:
            mostrar_risco(montar_pedido(PEDIDO_DO_GRUPO[grupo], 0, nenhuma));
            break;

        case GRUPO_MANGA:
        case GRUPO_MARCAR:
            trapaca_em_andamento = PEDIDO_DO_GRUPO[grupo];
            carta_destacada = trapaca_vale_para_a_carta[trapaca_em_andamento][0] ? 0 : 1;
            trocar_tela(TRAPACAS_CARTA_DA_MAO);
            break;

        case GRUPO_SUBORNO:
            servico_destacado = 0;
            for (int i = 0; i < 3; i++) {
                if (trapaca_status[SERVICOS_DO_SUBORNO[i]] == POKER_OK) {
                    servico_destacado = i;
                    break;
                }
            }
            trocar_tela(TRAPACAS_SUBORNO);
            break;

        case GRUPOS_DE_TRAPACA:
            break;
    }
}

/** Escolhe um dos três serviços do suborno. */
static void escolher_servico(int servico)
{
    TipoDePedido tipo;

    if (servico < 0 || servico > 2)
        return;

    servico_destacado = servico;
    tipo = SERVICOS_DO_SUBORNO[servico];

    if (trapaca_status[tipo] != POKER_OK) {
        aviso_das_trapacas = motivo_da_trapaca(trapaca_status[tipo]);
        return;
    }

    if (tipo == PEDIDO_SUBORNO_VER) {
        mostrar_risco(montar_pedido(tipo, 0, (PokerCard){ 0, POKER_CLUBS }));
        return;
    }

    trapaca_em_andamento = tipo;
    ajustar_cursor_da_grade();
    trocar_tela(TRAPACAS_ESCOLHER_CARTA);
}

/** Escolhe a carta da mão (0 ou 1) para marcar ou trocar pela da manga. */
static void escolher_carta_da_mao(int carta)
{
    if (carta < 0 || carta > 1)
        return;

    carta_destacada = carta;

    if (!trapaca_vale_para_a_carta[trapaca_em_andamento][carta]) {
        aviso_das_trapacas = trapaca_em_andamento == PEDIDO_MARCAR_CARTA
            ? "Essa carta já está marcada." : "Essa carta não pode ir para a manga.";
        return;
    }

    mostrar_risco(montar_pedido(trapaca_em_andamento, carta, (PokerCard){ 0, POKER_CLUBS }));
}

/** Pede ao dealer a carta do cursor da grade, se o jogador ainda não a vê. */
static void escolher_carta_da_grade(void)
{
    PokerCard carta = { valor_do_seletor, (PokerSuit)naipe_do_seletor };

    if (carta_esta_a_vista(carta)) {
        aviso_das_trapacas = "Você já vê essa carta. Peça outra.";
        return;
    }

    mostrar_risco(montar_pedido(trapaca_em_andamento, 0, carta));
}

/** Põe o cursor da grade na carta da posição (linha de espadas em cima, paus embaixo). */
static void cursor_na_posicao(int posicao)
{
    naipe_do_seletor = 3 - posicao / 13;
    valor_do_seletor = posicao % 13 + 2;
}

/** Lê o teclado e o mouse enquanto as telas das trapaças estão abertas. */
static void ler_entrada_das_trapacas(EntradaMesa *entrada)
{
    int botao = botao_clicado();
    int sob = mouse_se_moveu() ? botao_sob_o_mouse() : BOTAO_NENHUM;
    bool voltar = IsKeyPressed(KEY_ESCAPE) || botao == BOTAO_VOLTAR_DAS_TRAPACAS;
    bool confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    bool acima = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    bool abaixo = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);

    switch (tela_de_trapacas) {
        case TRAPACAS_MENU:
            if (voltar || IsKeyPressed(KEY_T)) {
                fechar_trapacas();
                return;
            }

            for (int i = 0; i < GRUPOS_DE_TRAPACA; i++) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    escolher_grupo(i);
                    return;
                }
            }

            if (acima)
                grupo_destacado = (grupo_destacado + GRUPOS_DE_TRAPACA - 1) % GRUPOS_DE_TRAPACA;
            else if (abaixo)
                grupo_destacado = (grupo_destacado + 1) % GRUPOS_DE_TRAPACA;

            if (acima || abaixo)
                aviso_das_trapacas = NULL;

            if (sob >= BOTAO_TRAPACA && sob < BOTAO_TRAPACA + GRUPOS_DE_TRAPACA)
                grupo_destacado = sob - BOTAO_TRAPACA;

            if (botao >= BOTAO_TRAPACA && botao < BOTAO_TRAPACA + GRUPOS_DE_TRAPACA) {
                grupo_destacado = botao - BOTAO_TRAPACA;
                aviso_das_trapacas = NULL;
            }

            if (confirmar || botao == BOTAO_USAR)
                escolher_grupo(grupo_destacado);
            return;

        case TRAPACAS_SUBORNO:
            if (voltar) {
                trocar_tela(TRAPACAS_MENU);
                return;
            }

            for (int i = 0; i < 3; i++) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    escolher_servico(i);
                    return;
                }
            }

            if (acima)
                servico_destacado = (servico_destacado + 2) % 3;
            else if (abaixo)
                servico_destacado = (servico_destacado + 1) % 3;

            if (sob >= BOTAO_OPCAO_DO_SUBORNO && sob < BOTAO_OPCAO_DO_SUBORNO + 3)
                servico_destacado = sob - BOTAO_OPCAO_DO_SUBORNO;

            if (botao >= BOTAO_OPCAO_DO_SUBORNO && botao < BOTAO_OPCAO_DO_SUBORNO + 3)
                escolher_servico(botao - BOTAO_OPCAO_DO_SUBORNO);
            else if (confirmar || botao == BOTAO_USAR)
                escolher_servico(servico_destacado);
            return;

        case TRAPACAS_CARTA_DA_MAO:
            if (voltar) {
                trocar_tela(TRAPACAS_MENU);
                return;
            }

            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))
                carta_destacada = 1 - carta_destacada;

            for (int i = 0; i < 2; i++) {
                if (IsKeyPressed(KEY_ONE + i) || botao == BOTAO_CARTA_DA_MAO + i) {
                    escolher_carta_da_mao(i);
                    return;
                }
            }

            if (sob == BOTAO_CARTA_DA_MAO || sob == BOTAO_CARTA_DA_MAO + 1)
                carta_destacada = sob - BOTAO_CARTA_DA_MAO;

            if (confirmar || botao == BOTAO_USAR)
                escolher_carta_da_mao(carta_destacada);
            return;

        case TRAPACAS_ESCOLHER_CARTA:
            if (voltar) {
                trocar_tela(TRAPACAS_SUBORNO);
                return;
            }

            if (IsKeyPressed(KEY_RIGHT))
                valor_do_seletor = valor_do_seletor >= 14 ? 2 : valor_do_seletor + 1;
            else if (IsKeyPressed(KEY_LEFT))
                valor_do_seletor = valor_do_seletor <= 2 ? 14 : valor_do_seletor - 1;

            /* As linhas da grade vão de espadas, em cima, a paus. */
            if (acima)
                naipe_do_seletor = (naipe_do_seletor + 1) % 4;
            else if (abaixo)
                naipe_do_seletor = (naipe_do_seletor + 3) % 4;

            if (sob >= BOTAO_GRADE && sob < BOTAO_GRADE + 52)
                cursor_na_posicao(sob - BOTAO_GRADE);

            if (botao >= BOTAO_GRADE && botao < BOTAO_GRADE + 52) {
                cursor_na_posicao(botao - BOTAO_GRADE);
                escolher_carta_da_grade();
            } else if (confirmar || botao == BOTAO_USAR) {
                escolher_carta_da_grade();
            }
            return;

        case TRAPACAS_RISCO:
            if (voltar || botao == BOTAO_RECUAR) {
                trocar_tela(tela_antes_do_risco);
                return;
            }

            if (confirmar || botao == BOTAO_CONTINUAR_TRAPACA) {
                entrada->pedido = pedido_em_espera;
                fechar_trapacas();
            }
            return;

        case TRAPACAS_FECHADAS:
            return;
    }
}

EntradaMesa interface_mesa_ler_entrada(void)
{
    EntradaMesa entrada = { COMANDO_NENHUM, 0, false, false, { PEDIDO_NENHUM, 0, { 0, POKER_CLUBS } }, false };

    if (carta_marcada_ampliada >= 0) {
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            carta_marcada_ampliada = -1;
        return entrada;
    }

    if (aviso_aberto != AVISO_NENHUM) {
        /* Um instante de espera evita fechar o aviso com a mesma tecla que o abriu; na
         * roleta, a espera cobre o giro do tambor, para o resultado sempre aparecer. */
        bool pode_fechar = painel_progresso(inicio_do_aviso, aviso_aberto == AVISO_ROLETA ? 1.2 : 0.4) >= 1.0f;

        if (pode_fechar && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                            IsKeyPressed(KEY_ESCAPE) || botao_clicado() == BOTAO_FECHAR_AVISO))
            aviso_aberto = AVISO_NENHUM;
        return entrada;
    }

    if (animacoes_ativas()) {
        entrada.voltar = IsKeyPressed(KEY_ESCAPE);
        return entrada;
    }

    if (revelacao_em_andamento) {
        entrada.voltar = IsKeyPressed(KEY_ESCAPE) || botao_clicado() == BOTAO_VOLTAR_DA_MESA;
        return entrada;
    }

    if (cartas_do_adversario_visiveis && tela_de_trapacas == TRAPACAS_FECHADAS &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < 2; i++) {
            if (marcou_carta_do_adversario[i] &&
                CheckCollisionPointRec(posicao_do_mouse(), AREAS_DO_ADVERSARIO[i])) {
                carta_marcada_ampliada = i;
                return entrada;
            }
        }
    }

    if (tela_de_trapacas != TRAPACAS_FECHADAS) {
        ler_entrada_das_trapacas(&entrada);
        return entrada;
    }

    if (IsKeyPressed(KEY_T))
        abrir_trapacas();

    if (IsKeyPressed(KEY_F)) {
        entrada.comando = COMANDO_DESISTIR;
    } else if (IsKeyPressed(KEY_C)) {
        entrada.comando = COMANDO_PASSAR_OU_PAGAR;
    } else if (IsKeyPressed(KEY_R)) {
        entrada.comando = COMANDO_APOSTAR;
    } else if (IsKeyPressed(KEY_A)) {
        entrada.comando = COMANDO_ALL_IN;
    }

    if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
        entrada.ajuste_aposta = 1;
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
        entrada.ajuste_aposta = -1;
    }

    entrada.continuar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    entrada.voltar = IsKeyPressed(KEY_ESCAPE);

    switch (botao_clicado()) {
        case BOTAO_DESISTIR:
            entrada.comando = COMANDO_DESISTIR;
            break;

        case BOTAO_PASSAR_OU_PAGAR:
            entrada.comando = COMANDO_PASSAR_OU_PAGAR;
            break;

        case BOTAO_APOSTAR:
            entrada.comando = COMANDO_APOSTAR;
            break;

        case BOTAO_ALL_IN:
            entrada.comando = COMANDO_ALL_IN;
            break;

        case BOTAO_AUMENTAR:
            entrada.ajuste_aposta = 1;
            break;

        case BOTAO_DIMINUIR:
            entrada.ajuste_aposta = -1;
            break;

        case BOTAO_CONTINUAR:
            entrada.continuar = true;
            break;

        case BOTAO_VOLTAR_DA_MESA:
            entrada.voltar = true;
            break;

        case BOTAO_TRAPACAS:
            abrir_trapacas();
            break;

        default:
            break;
    }

    entrada.ajuste_rapido = entrada.ajuste_aposta != 0 &&
                            (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    return entrada;
}

bool interface_mesa_modal_aberto(void)
{
    return carta_marcada_ampliada >= 0 || aviso_aberto != AVISO_NENHUM;
}

/**
 * Escreve o texto curto de uma carta: valor e inicial do naipe (P paus,
 * O ouros, C copas, E espadas).
 */
static void texto_da_carta(PokerCard carta, char *saida, size_t tamanho)
{
    static const char *VALORES[] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
    };
    static const char NAIPES[] = {'P', 'O', 'C', 'E'};

    snprintf(saida, tamanho, "%s%c", VALORES[carta.rank - 2], NAIPES[carta.suit]);
}

/** Escreve o texto de uma lista de cartas separadas por espaço. */
static void texto_das_cartas(const PokerCard *cartas, int quantidade, char *saida, size_t tamanho)
{
    saida[0] = '\0';

    for (int i = 0; i < quantidade; i++) {
        char carta[8];
        size_t usado = strlen(saida);

        texto_da_carta(cartas[i], carta, sizeof carta);
        snprintf(saida + usado, tamanho - usado, "%s%s", i > 0 ? " " : "", carta);
    }
}

/** Guarda uma linha no registro dos últimos eventos, descartando a mais antiga. */
static void guardar_linha(const char *linha)
{
    if (eventos_registrados == EVENTOS_NO_REGISTRO) {
        memmove(registro_de_eventos[0], registro_de_eventos[1],
                (EVENTOS_NO_REGISTRO - 1) * TAMANHO_LINHA);
        eventos_registrados--;
    }

    snprintf(registro_de_eventos[eventos_registrados++], TAMANHO_LINHA, "%s", linha);
}

static const char *nome_do_assento(int assento)
{
    return assento == TORNEIO_ASSENTO_JOGADOR ? "Você" : "Adversário";
}

static const char *nome_da_acao(PokerActionType acao)
{
    switch (acao) {
        case POKER_FOLD:
            return "desistiu";

        case POKER_CHECK:
            return "passou";

        case POKER_CALL:
            return "pagou";

        case POKER_BET:
            return "apostou";

        case POKER_RAISE:
            return "aumentou";

        case POKER_ALL_IN:
            return "foi all-in";
    }

    return "?";
}

void interface_mesa_limpar_eventos(void)
{
    eventos_registrados = 0;
    inicio_da_revelacao = -1.0;
    revelacao_em_andamento = false;
    adversario_recebeu_cartas = false;
    marcou_carta_do_adversario[0] = false;
    marcou_carta_do_adversario[1] = false;
    carta_marcada_ampliada = -1;
    cartas_do_adversario_visiveis = false;
    aviso_aberto = AVISO_NENHUM;
    inicio_do_recado = -100.0;
    fechar_trapacas();
    animacoes_cancelar();
}

static const char *nome_da_trapaca(TipoDePedido pedido)
{
    switch (pedido) {
        case PEDIDO_BEBIDA:
            return "bebida batizada";

        case PEDIDO_CUMPLICE:
            return "cúmplice";

        case PEDIDO_MARCAR_CARTA:
            return "carta marcada";

        case PEDIDO_SUBORNO_MESA:
        case PEDIDO_SUBORNO_MAO:
        case PEDIDO_SUBORNO_VER:
            return "suborno do dealer";

        case PEDIDO_MANGA:
            return "ás na manga";

        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    return "trapaça";
}

/** Escreve a linha do registro que conta o que uma trapaça fez. */
static void linha_da_trapaca(const EventoTrapaca *trapaca, char *linha, size_t tamanho)
{
    char c0[8] = "";
    char c1[8] = "";
    const char *nome = nome_da_trapaca(trapaca->pedido);
    long long custo = (long long)trapaca->custo;

    if (trapaca->quantidade_de_cartas > 0) {
        texto_da_carta(trapaca->cartas[0], c0, sizeof c0);
    }

    if (trapaca->quantidade_de_cartas > 1) {
        texto_da_carta(trapaca->cartas[1], c1, sizeof c1);
    }

    if (trapaca->resultado == TRAPACA_DESCOBERTA) {
        snprintf(linha, tamanho, "Descoberto! (%s)", nome);
        return;
    }

    if (trapaca->resultado == TRAPACA_RECUSADA) {
        if (trapaca->na_distribuicao) {
            snprintf(linha, tamanho, "O dealer não achou a carta %s", c0);
        } else {
            snprintf(linha, tamanho, "O dealer pegou o dinheiro (-%lld) e não fez nada", custo);
        }

        return;
    }

    switch (trapaca->pedido) {
        case PEDIDO_BEBIDA:
            snprintf(linha, tamanho, "Bebida batizada: o adversário está embriagado");
            break;

        case PEDIDO_CUMPLICE:
            snprintf(linha, tamanho, "Cúmplice (-%lld): o adversário tem %s", custo,
                     trapaca->tem_categoria ? poker_category_name(trapaca->categoria) : "?");
            break;

        case PEDIDO_MARCAR_CARTA:
            if (trapaca->na_distribuicao) {
                snprintf(linha, tamanho, "Sua carta marcada %s está na mão do adversário", c0);
            } else {
                snprintf(linha, tamanho, "Carta marcada: %s", c0);
            }

            break;

        case PEDIDO_SUBORNO_MESA:
            if (trapaca->na_distribuicao) {
                snprintf(linha, tamanho, "O dealer pôs %s na mesa", c0);
            } else {
                snprintf(linha, tamanho, "Dealer subornado (-%lld): %s será a próxima carta da mesa", custo, c0);
            }

            break;

        case PEDIDO_SUBORNO_MAO:
            if (trapaca->na_distribuicao) {
                snprintf(linha, tamanho, "O dealer entregou %s a você", c0);
            } else {
                snprintf(linha, tamanho, "Dealer subornado (-%lld): você receberá %s na próxima mão", custo, c0);
            }

            break;

        case PEDIDO_SUBORNO_VER:
            snprintf(linha, tamanho, "Dealer subornado (-%lld): o adversário tem %s %s", custo, c0, c1);
            break;

        case PEDIDO_MANGA:
            snprintf(linha, tamanho, "Ás na manga: você pegou %s e guardou %s", c0, c1);
            break;

        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            snprintf(linha, tamanho, "Trapaça");
            break;
    }
}

static Icone icone_do_pedido(TipoDePedido pedido)
{
    switch (pedido) {
        case PEDIDO_BEBIDA: return ICONE_GARRAFA;
        case PEDIDO_CUMPLICE: return ICONE_BINOCULO;
        case PEDIDO_MARCAR_CARTA: return ICONE_MARCA;
        case PEDIDO_SUBORNO_MESA: return ICONE_CARTA_MESA;
        case PEDIDO_SUBORNO_MAO: return ICONE_CARTA_MAO;
        case PEDIDO_SUBORNO_VER: return ICONE_OLHO;
        case PEDIDO_MANGA: return ICONE_AS;
        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    return ICONE_MASCARA;
}

static void abrir_aviso(TipoDeAviso tipo)
{
    aviso_aberto = tipo;
    inicio_do_aviso = GetTime();
}

static void mostrar_recado(const char *texto, Icone icone)
{
    snprintf(texto_do_recado, sizeof texto_do_recado, "%s", texto);
    icone_do_recado = icone;
    inicio_do_recado = GetTime();
}

/** Decide como mostrar o que uma trapaça fez: uma tela própria ou um recado na mesa. */
static void mostrar_resultado_da_trapaca(const EventoTrapaca *trapaca, const char *linha)
{
    if (trapaca->resultado == TRAPACA_DESCOBERTA) {
        abrir_aviso(AVISO_DESCOBERTO);
        return;
    }

    if (trapaca->resultado == TRAPACA_SUCESSO && !trapaca->na_distribuicao) {
        switch (trapaca->pedido) {
            case PEDIDO_CUMPLICE:
                aviso_tem_categoria = trapaca->tem_categoria;
                aviso_categoria = trapaca->categoria;
                abrir_aviso(AVISO_CUMPLICE);
                return;

            case PEDIDO_SUBORNO_VER:
                if (trapaca->quantidade_de_cartas == 2) {
                    aviso_cartas[0] = trapaca->cartas[0];
                    aviso_cartas[1] = trapaca->cartas[1];
                    abrir_aviso(AVISO_CARTAS_VISTAS);
                    return;
                }
                break;

            case PEDIDO_BEBIDA:
                abrir_aviso(AVISO_BEBIDA);
                return;

            default:
                break;
        }
    }

    mostrar_recado(linha, icone_do_pedido(trapaca->pedido));
}

void interface_mesa_registrar_evento(const EventoJogo *evento)
{
    char linha[TAMANHO_LINHA];
    char cartas[64];
    static const char *RUAS[] = {"Pré-flop", "Flop", "Turn", "River"};

    switch (evento->tipo) {
        case EVENTO_MAO_INICIADA:
            inicio_da_revelacao = -1.0;
            revelacao_em_andamento = false;
            adversario_recebeu_cartas = false;
            marcou_carta_do_adversario[0] = false;
            marcou_carta_do_adversario[1] = false;
            carta_marcada_ampliada = -1;
            cartas_do_adversario_visiveis = false;
            animacoes_comecar_embaralhamento();
            snprintf(linha, sizeof linha, "Nova mão: dealer %s, blinds %lld/%lld",
                     nome_do_assento(evento->mao_iniciada.dealer),
                     (long long)evento->mao_iniciada.pequeno_blind,
                     (long long)evento->mao_iniciada.grande_blind);
            guardar_linha(linha);
            break;

        case EVENTO_ACAO:
            snprintf(linha, sizeof linha, "%s %s (colocou %lld, pote %lld)",
                     nome_do_assento(evento->acao.assento),
                     nome_da_acao(evento->acao.acao),
                     (long long)evento->acao.fichas_colocadas,
                     (long long)evento->acao.pote);
            guardar_linha(linha);
            break;

        case EVENTO_BOARD_REVELADO:
            animacoes_board_revelado(evento->board.quantidade);
            texto_das_cartas(evento->board.cartas, evento->board.quantidade, cartas, sizeof cartas);
            snprintf(linha, sizeof linha, "%s: %s", RUAS[evento->board.rua], cartas);
            guardar_linha(linha);
            break;

        case EVENTO_SHOWDOWN:
            snprintf(linha, sizeof linha, "Showdown: você com %s, adversário com %s",
                     poker_category_name(evento->showdown.valor[TORNEIO_ASSENTO_JOGADOR].category),
                     poker_category_name(evento->showdown.valor[TORNEIO_ASSENTO_ADVERSARIO].category));
            guardar_linha(linha);
            break;

        case EVENTO_POTE_PAGO:
            snprintf(linha, sizeof linha, "Pote de %lld: %s",
                     (long long)evento->pote.pote,
                     evento->pote.vencedores == 1u ? "você ganhou"
                     : evento->pote.vencedores == 2u ? "o adversário ganhou"
                     : "empate");
            guardar_linha(linha);
            break;

        case EVENTO_TRAPACA:
            if (evento->trapaca.pedido == PEDIDO_MARCAR_CARTA &&
                evento->trapaca.resultado == TRAPACA_SUCESSO &&
                evento->trapaca.na_distribuicao &&
                evento->trapaca.quantidade_de_cartas == 1 && adversario_recebeu_cartas) {
                PokerCard marcada = evento->trapaca.cartas[0];

                for (int i = 0; i < 2; i++) {
                    if (cartas_do_adversario[i].rank == marcada.rank &&
                        cartas_do_adversario[i].suit == marcada.suit)
                        marcou_carta_do_adversario[i] = true;
                }
            }

            linha_da_trapaca(&evento->trapaca, linha, sizeof linha);
            guardar_linha(linha);
            mostrar_resultado_da_trapaca(&evento->trapaca, linha);
            break;

        case EVENTO_ROLETA:
            if (evento->roleta.morreu) {
                snprintf(linha, sizeof linha, "Roleta russa (%s): BANG! Você morreu",
                         evento->roleta.por_falencia ? "falência" : "trapaça descoberta");
            } else {
                snprintf(linha, sizeof linha, "Roleta russa (%s): click, você sobreviveu (agora %d balas)",
                         evento->roleta.por_falencia ? "falência" : "trapaça descoberta",
                         evento->roleta.balas_depois);
            }

            guardar_linha(linha);
            aviso_morreu = evento->roleta.morreu;
            aviso_balas = evento->roleta.balas_depois;
            abrir_aviso(AVISO_ROLETA);
            break;

        case EVENTO_SEGUNDA_VIDA:
            snprintf(linha, sizeof linha, "%s caiu, mas se levanta de novo com %lld fichas!",
                     partida_nome_do_adversario(evento->segunda_vida.confronto),
                     (long long)evento->segunda_vida.fichas);
            guardar_linha(linha);
            break;

        case EVENTO_CARTAS_DISTRIBUIDAS:
            if (evento->cartas_distribuidas.assento == TORNEIO_ASSENTO_ADVERSARIO) {
                cartas_do_adversario[0] = evento->cartas_distribuidas.cartas[0];
                cartas_do_adversario[1] = evento->cartas_distribuidas.cartas[1];
                adversario_recebeu_cartas = true;
            }
            break;
    }
}

/* Os botões da vez ficam no canto de baixo à direita, sobre a mesa, como nos mockups. */
#define BOTOES_DA_MESA_Y 640.0f
#define BOTOES_DA_MESA_ALTURA 50.0f
#define BOTOES_DA_MESA_DIREITA 1250.0f

/** Botão da mesa, no mesmo estilo das telas das trapaças. Só registra o clique se estiver ativo. */
static void botao_da_mesa(Rectangle area, const char *rotulo, bool principal, bool ativo, int id)
{
    painel_botao(area, rotulo, 19, principal, mouse_sobre(area), ativo);

    if (ativo)
        registrar_botao(area, id);
}

/** Botão único no canto da mesa (próxima mão, recomeçar...), em vermelho. */
static void botao_unico_da_mesa(const char *rotulo, int id)
{
    float largura = (float)MeasureText(rotulo, 19) + 100.0f;

    botao_da_mesa((Rectangle){ BOTOES_DA_MESA_DIREITA - largura, BOTOES_DA_MESA_Y, largura, BOTOES_DA_MESA_ALTURA },
                  rotulo, true, true, id);
}

/**
 * Ações da vez do jogador, alinhadas à direita: DESISTIR, PASSAR ou IGUALAR
 * (com o valor), APOSTAR ou AUMENTAR (vermelho), ALL-IN e o botão da máscara,
 * que abre as trapaças. A que não vale agora aparece apagada. O valor da
 * aposta fica numa barrinha com menos e mais em cima do APOSTAR/AUMENTAR.
 */
static void desenhar_acoes_do_jogador(const Partida *partida, PokerChips aposta_escolhida)
{
    PokerLegalActions legal;
    bool apostar = aposta_escolhida > 0;
    char pagar[32];
    char valor[24];
    float y = BOTOES_DA_MESA_Y;
    float h = BOTOES_DA_MESA_ALTURA;
    float vao = 12.0f;
    Rectangle mascara = { BOTOES_DA_MESA_DIREITA - 64.0f, y, 64.0f, h };
    Rectangle all_in = { mascara.x - vao - 120.0f, y, 120.0f, h };
    Rectangle aumentar = { all_in.x - vao - 170.0f, y, 170.0f, h };
    Rectangle igualar = { aumentar.x - vao - 190.0f, y, 190.0f, h };
    Rectangle desistir = { igualar.x - vao - 150.0f, y, 150.0f, h };

    if (partida_acoes_legais(partida, &legal) != POKER_OK)
        memset(&legal, 0, sizeof legal);

    if (legal.check)
        snprintf(pagar, sizeof pagar, "PASSAR");
    else
        snprintf(pagar, sizeof pagar, "IGUALAR %lld", (long long)legal.call_cost);

    botao_da_mesa(desistir, "DESISTIR", false, legal.fold, BOTAO_DESISTIR);
    botao_da_mesa(igualar, pagar, false, legal.check || legal.call, BOTAO_PASSAR_OU_PAGAR);
    botao_da_mesa(aumentar, partida->mao.mesa.current_bet > 0 ? "AUMENTAR" : "APOSTAR", true, apostar, BOTAO_APOSTAR);
    botao_da_mesa(all_in, "ALL-IN", false, legal.all_in, BOTAO_ALL_IN);

    painel_botao(mascara, "", 19, false, mouse_sobre(mascara), true);
    painel_icone(ICONE_MASCARA, (Rectangle){ mascara.x + 4.0f, mascara.y - 4.0f, mascara.width - 8.0f, h + 8.0f },
                 PAINEL_OURO_VIVO);
    registrar_botao(mascara, BOTAO_TRAPACAS);

    if (apostar) {
        Rectangle barra = { aumentar.x, y - 40.0f, aumentar.width, 32.0f };
        Rectangle menos = { barra.x, barra.y, 36.0f, barra.height };
        Rectangle mais = { barra.x + barra.width - 36.0f, barra.y, 36.0f, barra.height };
        int largura;

        snprintf(valor, sizeof valor, "%lld", (long long)aposta_escolhida);
        largura = MeasureText(valor, 18);
        DrawRectangleRec(barra, Fade(PAINEL_FUNDO, 0.92f));
        DrawRectangleLinesEx(barra, 1.0f, Fade(PAINEL_OURO, 0.7f));
        botao_da_mesa(menos, "-", false, true, BOTAO_DIMINUIR);
        botao_da_mesa(mais, "+", false, true, BOTAO_AUMENTAR);
        DrawText(valor, (int)(barra.x + (barra.width - (float)largura) / 2.0f), (int)(barra.y + 7.0f), 18, PAINEL_TEXTO);
    }
}

/** Escreve o nome de uma carta por extenso, como "Ás de espadas". */
static void nome_da_carta(PokerCard carta, char *saida, size_t tamanho)
{
    static const char *VALORES[] = {
        "Dois", "Três", "Quatro", "Cinco", "Seis", "Sete", "Oito", "Nove", "Dez",
        "Valete", "Dama", "Rei", "Ás"
    };
    static const char *NAIPES[] = { "paus", "ouros", "copas", "espadas" };

    if (carta.rank < 2 || carta.rank > 14 || (int)carta.suit < 0 || (int)carta.suit > 3) {
        snprintf(saida, tamanho, "?");
        return;
    }

    snprintf(saida, tamanho, "%s de %s", VALORES[carta.rank - 2], NAIPES[carta.suit]);
}

/** Anota o que o menu das trapaças precisa saber da partida, e o fecha se a vez não é do jogador. */
static void registrar_disponibilidade(const Partida *partida)
{
    const Mao *mao = &partida->mao;
    PokerCard livre = { 0, POKER_CLUBS };

    vez_do_jogador_vista = partida->situacao == PARTIDA_VEZ_DO_JOGADOR;
    total_de_cartas_a_vista = 0;

    if (mao->situacao != MAO_VAZIA) {
        cartas_a_vista[total_de_cartas_a_vista++] = mao->cartas[TORNEIO_ASSENTO_JOGADOR][0];
        cartas_a_vista[total_de_cartas_a_vista++] = mao->cartas[TORNEIO_ASSENTO_JOGADOR][1];

        for (size_t i = 0; i < mao->cartas_no_board && total_de_cartas_a_vista < 9; i++)
            cartas_a_vista[total_de_cartas_a_vista++] = mao->board[i];

        cartas_a_vista[total_de_cartas_a_vista++] = partida->cheats.carta_na_manga;
    }

    for (int valor = 2; valor <= 14 && livre.rank == 0; valor++) {
        PokerCard candidata = { valor, POKER_CLUBS };

        if (!carta_esta_a_vista(candidata))
            livre = candidata;
    }

    for (int tipo = PEDIDO_BEBIDA; tipo < PEDIDO_TOTAL; tipo++) {
        PedidoTrapaca pedido = montar_pedido((TipoDePedido)tipo, 0, livre);
        PokerStatus primeiro_erro = POKER_OK;

        trapaca_status[tipo] = partida_pode_trapacear(partida, pedido);
        primeiro_erro = trapaca_status[tipo];

        for (int carta = 0; carta < 2; carta++) {
            pedido.carta_da_mao = carta;
            trapaca_vale_para_a_carta[tipo][carta] = partida_pode_trapacear(partida, pedido) == POKER_OK;
        }

        /* Marcar e a manga valem se servir pelo menos uma das duas cartas. */
        if (tipo == PEDIDO_MARCAR_CARTA || tipo == PEDIDO_MANGA) {
            bool alguma = trapaca_vale_para_a_carta[tipo][0] || trapaca_vale_para_a_carta[tipo][1];

            trapaca_status[tipo] = alguma ? POKER_OK : primeiro_erro;
        }
    }

    if (!vez_do_jogador_vista)
        fechar_trapacas();
}

/* ------------------------------------------------------------------------
 * Telas das trapaças, avisos de resultado, roleta e o que a mesa mostra do
 * oponente (expressão e suspeita). Desenhadas no estilo noir de painel.h,
 * por cima da mesa, com as expressões do oponente e o dealer do Matheus.
 * ------------------------------------------------------------------------ */

static const char *NOME_DO_GRUPO[GRUPOS_DE_TRAPACA] = {
    "BATIZAR A BEBIDA", "CÚMPLICE", "ÁS NA MANGA", "SUBORNO DO DEALER", "MARCAR CARTA"
};

static const char *RESUMO_DO_GRUPO[GRUPOS_DE_TRAPACA] = {
    "Embriaga o adversário por 3 mãos.",
    "Conta que mão o adversário tem.",
    "Troca uma carta pela da manga.",
    "Informação ou cartas, por um preço.",
    "Reconheça sua carta na mão dele."
};

static const char *DESCRICAO_DO_GRUPO[GRUPOS_DE_TRAPACA] = {
    "Adultere a bebida do adversário. Se ninguém perceber, ele fica embriagado por 3 mãos: joga pior e desconfia menos de você.",
    "Um cúmplice na plateia espia as cartas do adversário e faz um sinal: você fica sabendo que mão ele tem agora. O preço sobe a cada uso no mesmo nível de blind.",
    "Troque uma carta da sua mão pela carta escondida na manga. A carta que sai fica guardada para a próxima vez.",
    "O dealer trabalha para quem paga: põe uma carta na mesa, te entrega uma carta ou mostra as cartas do adversário.",
    "Marque uma carta da sua mão. Por 5 mãos, se ela cair com o adversário, você reconhece o verso."
};

static const Icone ICONE_DO_GRUPO[GRUPOS_DE_TRAPACA] = {
    ICONE_GARRAFA, ICONE_BINOCULO, ICONE_AS, ICONE_SUBORNO, ICONE_MARCA
};

static const char *NOME_DO_SERVICO[3] = {
    "Pôr uma carta na mesa", "Receber uma carta", "Ver as cartas dele"
};

static const Icone ICONE_DO_SERVICO[3] = { ICONE_CARTA_MESA, ICONE_CARTA_MAO, ICONE_OLHO };

/* Índices por PokerAIStyle: CAUTIOUS, BALANCED, AGGRESSIVE. */
static const char *ARQUIVOS_DOS_ESTILOS[3] = { "medroso", "desconfiado", "agressivo" };

/* Recortes das imagens, em frações da largura e da altura. */
static const Rectangle RETRATO_INTEIRO = { 0.0f, 0.0f, 1.0f, 1.0f };
static const Rectangle DEALER_RECORTE = { 0.25f, 0.0f, 0.5f, 1.0f };
static const Rectangle DEALER_BUSTO = { 0.18f, 0.0f, 0.64f, 1.0f };

/* Boca do cano em dealer_revolver.png, em frações da largura e da altura. */
static const Vector2 BOCA_DO_CANO = { 622.0f / 1754.0f, 564.0f / 897.0f };

static int grupo_no_detalhe = -1;
static double inicio_do_detalhe;
static float risco_mostrado;
static float brilho_das_linhas[GRUPOS_DE_TRAPACA];

static float limitar(float valor, float minimo, float maximo)
{
    return valor < minimo ? minimo : (valor > maximo ? maximo : valor);
}

/** Passo de uma aproximação suave, com a velocidade pedida por segundo. */
static float passo_do_quadro(float velocidade)
{
    return limitar(GetFrameTime() * velocidade, 0.0f, 1.0f);
}

static int indice_do_estilo(PokerAIStyle estilo)
{
    int indice = (int)estilo;

    return indice >= 0 && indice < 3 ? indice : (int)POKER_AI_BALANCED;
}

/** Carrega as três expressões do oponente do confronto, se ainda não estão carregadas. */
static void carregar_retratos(int confronto)
{
    if (confronto == retratos_do_confronto || confronto < 0 || confronto >= TORNEIO_CONFRONTOS)
        return;

    for (int i = 0; i < 3; i++) {
        if (retratos[i].id != 0)
            UnloadTexture(retratos[i]);

        retratos[i] = LoadTexture(TextFormat("assets/images/adversario_%d_%s.png",
                                             confronto + 1, ARQUIVOS_DOS_ESTILOS[i]));
    }

    retratos_do_confronto = confronto;
}

static Texture2D retrato_do_oponente(const Partida *partida, PokerAIStyle estilo)
{
    carregar_retratos(partida->torneio.confronto);
    return retratos[indice_do_estilo(estilo)];
}

/** Largura que um recorte ocupa com a altura pedida, mantendo a proporção. */
static float largura_do_recorte(Texture2D textura, Rectangle janela, float altura)
{
    if (textura.id == 0 || janela.height <= 0.0f)
        return 0.0f;

    return altura * (janela.width * (float)textura.width) / (janela.height * (float)textura.height);
}

/**
 * Desenha um recorte de uma imagem com a altura pedida, mantendo a
 * proporção. A posição é o canto de cima à esquerda; com rotação, a imagem
 * gira em torno do centro de baixo (como alguém balançando na cadeira).
 */
static void desenhar_recorte(Texture2D textura, Rectangle janela, Vector2 posicao, float altura,
                             float rotacao, Color tinta)
{
    Rectangle fonte;
    float largura = largura_do_recorte(textura, janela, altura);
    Vector2 origem = { largura / 2.0f, altura };

    if (largura <= 0.0f)
        return;

    fonte = (Rectangle){ janela.x * (float)textura.width, janela.y * (float)textura.height,
                         janela.width * (float)textura.width, janela.height * (float)textura.height };
    DrawTexturePro(textura, fonte,
                   (Rectangle){ posicao.x + origem.x, posicao.y + origem.y, largura, altura },
                   origem, rotacao, tinta);
}

/** Escurece a mesa por trás das telas por cima dela e bloqueia os cliques na mesa. */
static void escurecer_mesa(float opacidade)
{
    total_de_botoes = 0;
    cursor_sobre_botao = false;
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.66f * opacidade));
    painel_vinheta(LARGURA_TELA, ALTURA_TELA, BLACK, 0.8f * opacidade);
}

/** Botão com moldura que também registra o clique. */
static void botao_de_painel(Rectangle area, const char *rotulo, int tamanho, bool principal, bool ativo, int id)
{
    painel_botao(area, rotulo, tamanho, principal, mouse_sobre(area), ativo);

    if (ativo)
        registrar_botao(area, id);
}

/** Índice de SistemaRisco.usos_trapaca de um pedido, ou -1. */
static int indice_de_risco(TipoDePedido tipo)
{
    switch (tipo) {
        case PEDIDO_BEBIDA: return PROB_TRAPACA_SPIKE_DRINK;
        case PEDIDO_CUMPLICE: return PROB_TRAPACA_CUMPLICE;
        case PEDIDO_MARCAR_CARTA: return PROB_TRAPACA_MARCAR_CARTA;
        case PEDIDO_SUBORNO_MESA:
        case PEDIDO_SUBORNO_MAO:
        case PEDIDO_SUBORNO_VER: return PROB_TRAPACA_SUBORNO_DEALER;
        case PEDIDO_MANGA: return PROB_TRAPACA_CARTA_MANGA;
        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    return -1;
}

static int usos_do_grupo(const Partida *partida, int grupo)
{
    int indice = indice_de_risco(PEDIDO_DO_GRUPO[grupo]);

    return indice >= 0 ? partida->risco.usos_trapaca[indice] : 0;
}

/** Cor de uma chance de ser descoberto: dourada, laranja ou vermelha. */
static Color cor_do_risco(int chance)
{
    if (chance < 20)
        return PAINEL_OURO_VIVO;
    if (chance < 45)
        return (Color){ 222, 126, 54, 255 };
    return PAINEL_VERMELHO_VIVO;
}

/** Bolinhas de uso de uma trapaça neste confronto (até três aparecem). */
static void desenhar_usos(int usos, float x, float y, float opacidade)
{
    for (int i = 0; i < 3; i++) {
        Vector2 centro = { x + (float)i * 14.0f, y };

        if (i < usos)
            DrawCircleV(centro, 4.5f, Fade(PAINEL_OURO_VIVO, opacidade));
        else
            DrawCircleLines((int)centro.x, (int)centro.y, 4.5f, Fade(PAINEL_OURO, 0.7f * opacidade));
    }
}

/** Custo alinhado à direita, com uma ficha na frente; "grátis" se não custa nada. */
static void desenhar_custo(PokerChips custo, float direita, float y, int tamanho, float opacidade)
{
    char texto[32];
    int largura;

    if (custo > 0)
        snprintf(texto, sizeof texto, "%lld", (long long)custo);
    else
        snprintf(texto, sizeof texto, "grátis");

    largura = MeasureText(texto, tamanho);
    DrawText(texto, (int)(direita - (float)largura), (int)y, tamanho, Fade(PAINEL_TEXTO, opacidade));

    if (custo > 0)
        painel_icone(ICONE_FICHA, (Rectangle){ direita - (float)largura - (float)tamanho - 6.0f, y - 1.0f,
                                               (float)tamanho, (float)tamanho },
                     Fade(PAINEL_VERMELHO_VIVO, opacidade));
}

/** Um ponto da imagem (em frações) na tela, onde o recorte foi desenhado. */
static Vector2 ponto_no_recorte(Texture2D textura, Rectangle janela, Vector2 posicao, float altura, Vector2 fracao)
{
    float largura = largura_do_recorte(textura, janela, altura);

    return (Vector2){ posicao.x + (fracao.x - janela.x) / janela.width * largura,
                      posicao.y + (fracao.y - janela.y) / janela.height * altura };
}

/** Texto centralizado na largura de uma área. */
static void texto_no_meio(const char *texto, Rectangle area, float y, int tamanho, Color cor)
{
    DrawText(texto, (int)(area.x + (area.width - (float)MeasureText(texto, tamanho)) / 2.0f), (int)y, tamanho, cor);
}

static int suspeita_media(const Partida *partida);

/**
 * O dealer conforme a chance de ser pego, com os mesmos cortes das cores do
 * risco: calmo abaixo de 20%, desconfiado até 45% e bravo dali para cima.
 */
static Texture2D dealer_pela_suspeita(int chance)
{
    if (chance >= 45 && textura_dealer_bravo.id != 0)
        return textura_dealer_bravo;
    if (chance >= 20 && textura_dealer_desconfiado.id != 0)
        return textura_dealer_desconfiado;
    return textura_dealer;
}

/** O dealer grande à direita, por trás dos painéis das trapaças: vigia a mesa com a cara da suspeita. */
static void desenhar_dealer_ao_fundo(const Partida *partida, float abertura, Color tinta)
{
    Vector2 posicao = { 700.0f + (1.0f - painel_suave(abertura)) * 60.0f, 110.0f };

    desenhar_recorte(dealer_pela_suspeita(suspeita_media(partida)), DEALER_BUSTO, posicao, 610.0f, 0.0f,
                     Fade(tinta, abertura));
    DrawRectangleGradientH(700, 110, 320, 610, Fade(BLACK, 0.8f * abertura), Fade(BLACK, 0.0f));
}

/** Lista das cinco trapaças, à esquerda. */
static void desenhar_lista_de_trapacas(const Partida *partida, float abertura, bool interativa)
{
    float deslize = (1.0f - painel_suave(abertura)) * -50.0f;
    Rectangle painel = { 32.0f + deslize, 80.0f, 476.0f, 584.0f };
    float passo = passo_do_quadro(12.0f);

    painel_desenhar(painel, abertura);
    painel_icone(ICONE_MASCARA, (Rectangle){ painel.x + 22.0f, painel.y + 18.0f, 44.0f, 34.0f },
                 Fade(PAINEL_OURO_VIVO, abertura));
    DrawText("TRAPACEAR", (int)(painel.x + 78.0f), (int)(painel.y + 20.0f), 32, Fade(PAINEL_OURO_VIVO, abertura));
    DrawLineEx((Vector2){ painel.x + 22.0f, painel.y + 64.0f },
               (Vector2){ painel.x + painel.width - 22.0f, painel.y + 64.0f }, 1.0f, Fade(PAINEL_OURO, 0.6f * abertura));

    for (int g = 0; g < GRUPOS_DE_TRAPACA; g++) {
        Rectangle linha = { painel.x + 16.0f, painel.y + 80.0f + (float)g * 92.0f, painel.width - 32.0f, 82.0f };
        Rectangle caixa;
        bool destacado = g == grupo_destacado;
        bool vale = situacao_do_grupo(g) == POKER_OK;
        TipoDePedido pedido = PEDIDO_DO_GRUPO[g];
        char numero[12];

        brilho_das_linhas[g] += ((destacado ? 1.0f : 0.0f) - brilho_das_linhas[g]) * passo;
        linha.x += 6.0f * brilho_das_linhas[g];

        DrawRectangleRec(linha, Fade(PAINEL_FUNDO_CLARO, 0.45f * abertura));
        DrawRectangleRec(linha, Fade(PAINEL_VERMELHO, brilho_das_linhas[g] * abertura));
        DrawRectangleLinesEx(linha, 1.0f, Fade(destacado ? PAINEL_OURO_VIVO : PAINEL_OURO,
                                               (0.35f + 0.55f * brilho_das_linhas[g]) * abertura));

        caixa = (Rectangle){ linha.x + 10.0f, linha.y + 12.0f, 58.0f, 58.0f };
        DrawRectangleLinesEx(caixa, 1.0f, Fade(PAINEL_OURO, 0.5f * abertura));
        painel_icone(ICONE_DO_GRUPO[g], (Rectangle){ caixa.x + 9.0f, caixa.y + 9.0f, 40.0f, 40.0f },
                     Fade(vale ? PAINEL_OURO_VIVO : PAINEL_TEXTO_SUAVE, abertura));

        snprintf(numero, sizeof numero, "%d", g + 1);
        DrawText(numero, (int)(caixa.x + 3.0f), (int)(caixa.y + 2.0f), 12, Fade(PAINEL_TEXTO_SUAVE, abertura));
        DrawText(NOME_DO_GRUPO[g], (int)(linha.x + 82.0f), (int)(linha.y + 16.0f), 21,
                 Fade(vale ? PAINEL_TEXTO : PAINEL_TEXTO_SUAVE, abertura));
        DrawText(RESUMO_DO_GRUPO[g], (int)(linha.x + 82.0f), (int)(linha.y + 48.0f), 15,
                 Fade(PAINEL_TEXTO_SUAVE, abertura));

        desenhar_custo(partida_custo_da_trapaca(partida, pedido), linha.x + linha.width - 14.0f,
                       linha.y + 14.0f, 20, abertura);
        desenhar_usos(usos_do_grupo(partida, g), linha.x + linha.width - 42.0f, linha.y + 60.0f, abertura);

        if (!vale)
            DrawRectangleRec(linha, Fade(BLACK, 0.35f * abertura));

        if (interativa)
            registrar_botao(linha, BOTAO_TRAPACA + g);
    }

    DrawText("1 a 5 ou setas escolhem    ENTER usa    ESC fecha", (int)(painel.x + 22.0f),
             (int)(painel.y + painel.height - 32.0f), 15, Fade(PAINEL_TEXTO_SUAVE, abertura));
}

/** Rótulo e valor de uma medida do detalhe (custo ou risco). */
static void desenhar_medida(const char *rotulo, const char *valor, float x, float y, Color cor, float opacidade)
{
    DrawText(rotulo, (int)x, (int)y, 15, Fade(PAINEL_TEXTO_SUAVE, opacidade));
    DrawText(valor, (int)x, (int)(y + 22.0f), 30, Fade(cor, opacidade));
}

/** Painel do meio: o que a trapaça destacada faz, quanto custa e o risco. */
static void desenhar_detalhe(const Partida *partida, float abertura, bool servicos_interativos)
{
    float deslize = (1.0f - painel_suave(abertura)) * 50.0f;
    Rectangle painel = { 528.0f + deslize, 80.0f, 430.0f, 584.0f };
    int g = grupo_destacado;
    bool suborno = g == GRUPO_SUBORNO;
    TipoDePedido tipo = suborno ? SERVICOS_DO_SUBORNO[servico_destacado] : PEDIDO_DO_GRUPO[g];
    PokerStatus status = suborno && servicos_interativos ? trapaca_status[tipo] : situacao_do_grupo(g);
    int chance = partida_chance_de_descoberta(partida, tipo);
    float x = painel.x + 24.0f;
    float largura = painel.width - 48.0f;
    float y;
    float conteudo;
    Rectangle caixa = { x, painel.y + 22.0f, 70.0f, 70.0f };
    char texto[TAMANHO_LINHA];
    const char *motivo;

    if (g != grupo_no_detalhe) {
        grupo_no_detalhe = g;
        inicio_do_detalhe = GetTime();
    }

    conteudo = painel_suave(painel_progresso(inicio_do_detalhe, 0.2)) * abertura;
    risco_mostrado += ((float)chance - risco_mostrado) * passo_do_quadro(7.0f);

    painel_desenhar(painel, abertura);
    painel_moldura(caixa, PAINEL_OURO, conteudo);
    painel_icone(ICONE_DO_GRUPO[g], (Rectangle){ caixa.x + 10.0f, caixa.y + 10.0f, 50.0f, 50.0f },
                 Fade(PAINEL_OURO_VIVO, conteudo));
    DrawText(NOME_DO_GRUPO[g], (int)(x + 88.0f), (int)(painel.y + 32.0f), 23, Fade(PAINEL_TEXTO, conteudo));
    DrawText(RESUMO_DO_GRUPO[g], (int)(x + 88.0f), (int)(painel.y + 66.0f), 15, Fade(PAINEL_TEXTO_SUAVE, conteudo));

    y = painel.y + 114.0f;
    y += (float)painel_texto_quebrado(DESCRICAO_DO_GRUPO[g], (Rectangle){ x, y, largura, 120.0f }, 17,
                                      Fade(PAINEL_TEXTO, conteudo));
    y += 8.0f;
    DrawLineEx((Vector2){ x, y }, (Vector2){ x + largura, y }, 1.0f, Fade(PAINEL_OURO, 0.5f * conteudo));
    y += 14.0f;

    if (suborno) {
        for (int i = 0; i < 3; i++) {
            Rectangle linha = { x, y + (float)i * 52.0f, largura, 46.0f };
            TipoDePedido servico = SERVICOS_DO_SUBORNO[i];
            bool vale = trapaca_status[servico] == POKER_OK;
            bool destacado = servicos_interativos && i == servico_destacado;
            char rotulo[48];

            if (destacado)
                DrawRectangleRec(linha, Fade(PAINEL_VERMELHO, conteudo));

            DrawRectangleLinesEx(linha, 1.0f, Fade(destacado ? PAINEL_OURO_VIVO : PAINEL_OURO,
                                                   (destacado ? 0.9f : 0.35f) * conteudo));
            painel_icone(ICONE_DO_SERVICO[i], (Rectangle){ linha.x + 8.0f, linha.y + 7.0f, 32.0f, 32.0f },
                         Fade(vale ? PAINEL_OURO_VIVO : PAINEL_TEXTO_SUAVE, conteudo));
            snprintf(rotulo, sizeof rotulo, "%d  %s", i + 1, NOME_DO_SERVICO[i]);
            DrawText(rotulo, (int)(linha.x + 50.0f), (int)(linha.y + 15.0f), 17,
                     Fade(vale ? PAINEL_TEXTO : PAINEL_TEXTO_SUAVE, conteudo));
            desenhar_custo(partida_custo_da_trapaca(partida, servico), linha.x + linha.width - 10.0f,
                           linha.y + 14.0f, 18, conteudo);

            if (servicos_interativos && vale)
                registrar_botao(linha, BOTAO_OPCAO_DO_SUBORNO + i);
        }

        y += 3.0f * 52.0f + 2.0f;
        snprintf(texto, sizeof texto, "Risco de o dealer não aceitar: %d%%", PARTIDA_CHANCE_DO_DEALER_RECUSAR);
        DrawText(texto, (int)x, (int)y, 16, Fade(PAINEL_TEXTO_SUAVE, conteudo));
        y += 26.0f;
        snprintf(texto, sizeof texto, "RISCO DE SER DESCOBERTO: %d%%", chance);
        DrawText(texto, (int)x, (int)y, 15, Fade(cor_do_risco(chance), conteudo));
        y += 22.0f;
    } else {
        PokerChips custo = partida_custo_da_trapaca(partida, tipo);

        if (custo > 0)
            snprintf(texto, sizeof texto, "%lld", (long long)custo);
        else
            snprintf(texto, sizeof texto, "grátis");

        desenhar_medida("CUSTO", texto, x, y, PAINEL_TEXTO, conteudo);
        snprintf(texto, sizeof texto, "%d%%", chance);
        desenhar_medida("RISCO DE SER DESCOBERTO", texto, x + largura * 0.42f, y, cor_do_risco(chance), conteudo);
        y += 62.0f;
    }

    painel_barra((Rectangle){ x, y, largura, 12.0f }, risco_mostrado / 100.0f, 0, cor_do_risco(chance));
    y += 26.0f;

    texto[0] = '\0';
    switch ((GrupoDeTrapaca)g) {
        case GRUPO_BEBIDA:
            if (partida->cheats.maos_embriagado > 0)
                snprintf(texto, sizeof texto, "Ele já está embriagado por mais %d mão(s).",
                         partida->cheats.maos_embriagado);
            else
                snprintf(texto, sizeof texto, "Ele está sóbrio e atento.");
            break;

        case GRUPO_CUMPLICE:
            snprintf(texto, sizeof texto, "Usos neste confronto: %d.", usos_do_grupo(partida, g));
            break;

        case GRUPO_MANGA: {
            char carta[32];

            nome_da_carta(partida->cheats.carta_na_manga, carta, sizeof carta);
            snprintf(texto, sizeof texto, "Na manga agora: %s.", carta);
            cartas_desenhar_frente(partida->cheats.carta_na_manga, false,
                                   (Rectangle){ x + largura - 70.0f, y - 6.0f, 70.0f, 98.0f });
            break;
        }

        case GRUPO_MARCAR: {
            int marcas = 0;

            for (int i = 0; i < 5; i++)
                marcas += partida->cheats.turnos_marcacao[i] > 0 ? 1 : 0;

            snprintf(texto, sizeof texto, "Cartas marcadas valendo: %d.", marcas);
            cartas_desenhar_verso(true, (Rectangle){ x + largura - 70.0f, y - 6.0f, 70.0f, 98.0f });
            break;
        }

        case GRUPO_SUBORNO:
        case GRUPOS_DE_TRAPACA:
            break;
    }

    if (texto[0] != '\0')
        painel_texto_quebrado(texto, (Rectangle){ x, y, largura - 90.0f, 60.0f }, 16, Fade(PAINEL_TEXTO_SUAVE, conteudo));

    motivo = aviso_das_trapacas != NULL ? aviso_das_trapacas
           : (status != POKER_OK ? motivo_da_trapaca(status) : NULL);

    if (motivo != NULL)
        DrawText(motivo, (int)x, (int)(painel.y + painel.height - 108.0f), 17, PAINEL_VERMELHO_VIVO);

    botao_de_painel((Rectangle){ x, painel.y + painel.height - 78.0f, largura, 54.0f },
                    suborno ? (servicos_interativos ? "PEDIR ESTE SERVIÇO" : "ESCOLHER SERVIÇO") : "USAR",
                    22, true, status == POKER_OK, BOTAO_USAR);
}

/** Tela de escolha da carta da mão, para marcar ou trocar pela da manga. */
static void desenhar_carta_da_mao(const Partida *partida, float abertura)
{
    Rectangle painel = { 220.0f, 90.0f, 840.0f, 548.0f };
    bool manga = trapaca_em_andamento == PEDIDO_MANGA;
    float pulso = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);

    painel_desenhar(painel, abertura);
    painel_icone(manga ? ICONE_AS : ICONE_MARCA, (Rectangle){ painel.x + 26.0f, painel.y + 22.0f, 54.0f, 54.0f },
                 Fade(PAINEL_OURO_VIVO, abertura));
    DrawText(manga ? "ÁS NA MANGA" : "MARCAR CARTA", (int)(painel.x + 96.0f), (int)(painel.y + 26.0f), 30,
             Fade(PAINEL_OURO_VIVO, abertura));
    DrawText(manga ? "Qual carta da sua mão vai para a manga?" : "Qual carta da sua mão você marca?",
             (int)(painel.x + 96.0f), (int)(painel.y + 62.0f), 18, Fade(PAINEL_TEXTO, abertura));

    for (int i = 0; i < 2; i++) {
        PokerCard carta = partida->mao.cartas[TORNEIO_ASSENTO_JOGADOR][i];
        bool vale = trapaca_vale_para_a_carta[trapaca_em_andamento][i];
        bool destacada = i == carta_destacada;
        Rectangle area = { (manga ? 310.0f : 400.0f) + (float)i * 230.0f, destacada ? 190.0f : 202.0f, 170.0f, 238.0f };
        Rectangle clique = { area.x - 14.0f, 180.0f, area.width + 28.0f, area.height + 72.0f };
        char rotulo[48];

        if (destacada && vale)
            DrawRectangleLinesEx((Rectangle){ area.x - 7.0f, area.y - 7.0f, area.width + 14.0f, area.height + 14.0f },
                                 3.0f, Fade(PAINEL_OURO_VIVO, (0.55f + 0.45f * pulso) * abertura));

        cartas_desenhar_frente(carta, is_card_marked(&partida->cheats, carta), area);

        if (!vale)
            DrawRectangleRec(area, Fade(BLACK, 0.55f));

        snprintf(rotulo, sizeof rotulo, "%d%s", i + 1, vale ? "" : "  (já marcada)");
        DrawText(rotulo, (int)(area.x + (area.width - (float)MeasureText(rotulo, 22)) / 2.0f),
                 (int)(area.y + area.height + 16.0f), 22, vale ? PAINEL_OURO_VIVO : PAINEL_TEXTO_SUAVE);
        registrar_botao(clique, BOTAO_CARTA_DA_MAO + i);
    }

    if (manga) {
        Rectangle na_manga = { 830.0f, 202.0f, 170.0f, 238.0f };
        float meio = 812.0f;

        DrawLineEx((Vector2){ meio - 46.0f, 320.0f }, (Vector2){ meio - 6.0f, 320.0f }, 3.0f, PAINEL_OURO_VIVO);
        DrawTriangle((Vector2){ meio, 320.0f }, (Vector2){ meio - 12.0f, 312.0f }, (Vector2){ meio - 12.0f, 328.0f },
                     PAINEL_OURO_VIVO);
        cartas_desenhar_frente(partida->cheats.carta_na_manga, false, na_manga);
        DrawText("NA MANGA", (int)(na_manga.x + (na_manga.width - (float)MeasureText("NA MANGA", 22)) / 2.0f),
                 (int)(na_manga.y + na_manga.height + 16.0f), 22, PAINEL_OURO);
    }

    if (aviso_das_trapacas != NULL)
        DrawText(aviso_das_trapacas, (int)(painel.x + 30.0f), (int)(painel.y + painel.height - 124.0f), 18,
                 PAINEL_VERMELHO_VIVO);

    DrawText("1, 2 ou setas e ENTER    ESC volta", (int)(painel.x + 30.0f),
             (int)(painel.y + painel.height - 66.0f), 16, PAINEL_TEXTO_SUAVE);
    botao_de_painel((Rectangle){ painel.x + painel.width - 470.0f, painel.y + painel.height - 84.0f, 200.0f, 52.0f },
                    "VOLTAR", 20, false, true, BOTAO_VOLTAR_DAS_TRAPACAS);
    botao_de_painel((Rectangle){ painel.x + painel.width - 256.0f, painel.y + painel.height - 84.0f, 230.0f, 52.0f },
                    "ESCOLHER", 22, true, trapaca_vale_para_a_carta[trapaca_em_andamento][carta_destacada],
                    BOTAO_USAR);
}

/** Grade com as 52 cartas, para pedir uma ao dealer. */
static void desenhar_grade(float abertura)
{
    Rectangle painel = { 120.0f, 56.0f, 1040.0f, 616.0f };
    bool na_mesa = trapaca_em_andamento == PEDIDO_SUBORNO_MESA;
    float x0 = painel.x + (painel.width - (13.0f * 62.0f + 12.0f * 8.0f)) / 2.0f;
    float y0 = painel.y + 116.0f;
    PokerCard cursor = { valor_do_seletor, (PokerSuit)naipe_do_seletor };
    char nome[48];
    char linha[TAMANHO_LINHA];

    painel_desenhar(painel, abertura);
    painel_icone(na_mesa ? ICONE_CARTA_MESA : ICONE_CARTA_MAO,
                 (Rectangle){ painel.x + 26.0f, painel.y + 20.0f, 50.0f, 50.0f }, Fade(PAINEL_OURO_VIVO, abertura));
    DrawText(na_mesa ? "SUBORNO: CARTA NA MESA" : "SUBORNO: CARTA NA SUA MÃO", (int)(painel.x + 92.0f),
             (int)(painel.y + 22.0f), 28, Fade(PAINEL_OURO_VIVO, abertura));
    DrawText(na_mesa ? "O dealer põe a carta escolhida como a primeira da próxima rua."
                     : "O dealer te entrega a carta escolhida na próxima mão.",
             (int)(painel.x + 92.0f), (int)(painel.y + 58.0f), 17, Fade(PAINEL_TEXTO, abertura));

    for (int linha_da_grade = 0; linha_da_grade < 4; linha_da_grade++) {
        int naipe = 3 - linha_da_grade;

        for (int valor = 2; valor <= 14; valor++) {
            PokerCard carta = { valor, (PokerSuit)naipe };
            int posicao = linha_da_grade * 13 + (valor - 2);
            bool no_cursor = valor == valor_do_seletor && naipe == naipe_do_seletor;
            bool visivel = carta_esta_a_vista(carta);
            Rectangle area = { x0 + (float)(valor - 2) * 70.0f, y0 + (float)linha_da_grade * 97.0f, 62.0f, 87.0f };

            if (no_cursor) {
                area.y -= 6.0f;
                DrawRectangleRec((Rectangle){ area.x - 4.0f, area.y - 4.0f, area.width + 8.0f, area.height + 8.0f },
                                 visivel ? PAINEL_TEXTO_SUAVE : PAINEL_OURO_VIVO);
            }

            cartas_desenhar_frente(carta, false, area);

            if (visivel)
                DrawRectangleRec(area, Fade(BLACK, 0.62f));
            else
                registrar_botao(area, BOTAO_GRADE + posicao);
        }
    }

    nome_da_carta(cursor, nome, sizeof nome);
    snprintf(linha, sizeof linha, "%s%s", nome, carta_esta_a_vista(cursor) ? "  (você já vê esta carta)" : "");
    DrawText(linha, (int)x0, (int)(y0 + 4.0f * 97.0f + 6.0f), 24,
             carta_esta_a_vista(cursor) ? PAINEL_TEXTO_SUAVE : PAINEL_TEXTO);

    if (aviso_das_trapacas != NULL)
        DrawText(aviso_das_trapacas, (int)x0, (int)(y0 + 4.0f * 97.0f + 40.0f), 17, PAINEL_VERMELHO_VIVO);

    DrawText("Setas ou mouse escolhem    ENTER pede    ESC volta", (int)x0,
             (int)(painel.y + painel.height - 46.0f), 16, PAINEL_TEXTO_SUAVE);
    botao_de_painel((Rectangle){ painel.x + painel.width - 470.0f, painel.y + painel.height - 70.0f, 200.0f, 50.0f },
                    "VOLTAR", 20, false, true, BOTAO_VOLTAR_DAS_TRAPACAS);
    botao_de_painel((Rectangle){ painel.x + painel.width - 256.0f, painel.y + painel.height - 70.0f, 230.0f, 50.0f },
                    "PEDIR ESTA CARTA", 20, true, !carta_esta_a_vista(cursor), BOTAO_USAR);
}

/** Descreve o pedido que está esperando o CONTINUAR. */
static void descrever_pedido(const Partida *partida, char *saida, size_t tamanho)
{
    char carta[32];
    char outra[32];
    PokerCard da_mao = partida->mao.cartas[TORNEIO_ASSENTO_JOGADOR][pedido_em_espera.carta_da_mao == 1 ? 1 : 0];

    switch (pedido_em_espera.tipo) {
        case PEDIDO_BEBIDA:
            snprintf(saida, tamanho, "Batizar a bebida do adversário.");
            return;

        case PEDIDO_CUMPLICE:
            snprintf(saida, tamanho, "Pedir ao cúmplice que espie a mão dele.");
            return;

        case PEDIDO_MARCAR_CARTA:
            nome_da_carta(da_mao, carta, sizeof carta);
            snprintf(saida, tamanho, "Marcar carta: %s.", carta);
            return;

        case PEDIDO_MANGA:
            nome_da_carta(da_mao, carta, sizeof carta);
            nome_da_carta(partida->cheats.carta_na_manga, outra, sizeof outra);
            snprintf(saida, tamanho, "Manga: trocar %s por %s.", carta, outra);
            return;

        case PEDIDO_SUBORNO_MESA:
            nome_da_carta(pedido_em_espera.carta, carta, sizeof carta);
            snprintf(saida, tamanho, "Dealer: pôr %s na mesa.", carta);
            return;

        case PEDIDO_SUBORNO_MAO:
            nome_da_carta(pedido_em_espera.carta, carta, sizeof carta);
            snprintf(saida, tamanho, "Dealer: te entregar %s na próxima mão.", carta);
            return;

        case PEDIDO_SUBORNO_VER:
            snprintf(saida, tamanho, "Pagar o dealer para mostrar as cartas do adversário.");
            return;

        case PEDIDO_NENHUM:
        case PEDIDO_TOTAL:
            break;
    }

    snprintf(saida, tamanho, "Trapaça.");
}

/** Frase da tela de risco, conforme a chance de ser descoberto. */
static const char *frase_do_risco(int chance)
{
    if (chance < 15)
        return "Ninguém está olhando. Por enquanto.";
    if (chance < 30)
        return "O oponente percebeu uma movimentação estranha.";
    if (chance < 50)
        return "Ele está de olho nas suas mãos.";
    return "Ele já desconfia de você. Pense bem.";
}

/** Tela de SUSPEITA antes de cada trapaça: a chance de ser pego, CONTINUAR e RECUAR. */
static void desenhar_risco(const Partida *partida, float abertura)
{
    Rectangle painel = { 70.0f, 120.0f, 660.0f, 480.0f };
    TipoDePedido tipo = pedido_em_espera.tipo;
    int chance = partida_chance_de_descoberta(partida, tipo);
    float enchimento = painel_suave(painel_progresso(inicio_da_tela, 0.9)) * (float)chance;
    float pulso = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f);
    PokerChips custo = partida_custo_da_trapaca(partida, tipo);
    float x = painel.x + 30.0f;
    float y = painel.y + 26.0f;
    char texto[TAMANHO_LINHA];
    int balas = partida->risco.balas_roleta;

    painel_vinheta(LARGURA_TELA, ALTURA_TELA, (Color){ 130, 0, 0, 255 },
                   (0.45f + (chance >= 45 ? 0.3f * pulso : 0.0f)) * abertura);
    desenhar_recorte(dealer_pela_suspeita(chance), DEALER_BUSTO, (Vector2){ 640.0f, 90.0f }, 630.0f, 0.0f,
                     Fade((Color){ 255, 92, 92, 255 }, abertura));
    DrawRectangleGradientH(640, 90, 260, 630, Fade(BLACK, 0.7f * abertura), Fade(BLACK, 0.0f));

    painel_desenhar(painel, abertura);
    painel_icone(ICONE_AVISO, (Rectangle){ x, y, 60.0f, 60.0f }, Fade(PAINEL_VERMELHO_VIVO, abertura));
    DrawText("SUSPEITA", (int)(x + 80.0f), (int)(y + 8.0f), 44, Fade(PAINEL_VERMELHO_VIVO, abertura));
    y += 84.0f;

    y += (float)painel_texto_quebrado(frase_do_risco(chance), (Rectangle){ x, y, painel.width - 60.0f, 60.0f },
                                      22, Fade(PAINEL_TEXTO, abertura));
    descrever_pedido(partida, texto, sizeof texto);
    y += (float)painel_texto_quebrado(texto, (Rectangle){ x, y + 4.0f, painel.width - 60.0f, 50.0f }, 17,
                                      Fade(PAINEL_TEXTO_SUAVE, abertura));
    y += 22.0f;

    DrawText("CHANCE DE SER DESCOBERTO", (int)x, (int)y, 16, Fade(PAINEL_TEXTO_SUAVE, abertura));
    y += 26.0f;
    painel_barra((Rectangle){ x, y, painel.width - 190.0f, 28.0f }, enchimento / 100.0f, 0, cor_do_risco(chance));
    snprintf(texto, sizeof texto, "%d%%", (int)(enchimento + 0.5f));
    DrawText(texto, (int)(painel.x + painel.width - 140.0f), (int)(y - 4.0f), 36, cor_do_risco(chance));
    y += 48.0f;

    if (tipo == PEDIDO_SUBORNO_MESA || tipo == PEDIDO_SUBORNO_MAO || tipo == PEDIDO_SUBORNO_VER) {
        snprintf(texto, sizeof texto, "Chance de o dealer não aceitar: %d%% (ele fica com o dinheiro).",
                 PARTIDA_CHANCE_DO_DEALER_RECUSAR);
        DrawText(texto, (int)x, (int)y, 16, Fade(PAINEL_TEXTO_SUAVE, abertura));
        y += 24.0f;
    }

    snprintf(texto, sizeof texto, "Se for pego: roleta russa com %d bala%s no tambor.", balas, balas == 1 ? "" : "s");
    DrawText(texto, (int)x, (int)y, 16, Fade(PAINEL_TEXTO_SUAVE, abertura));
    y += 24.0f;

    if (custo > 0) {
        snprintf(texto, sizeof texto, "Custo: %lld fichas, pagas na hora.", (long long)custo);
        DrawText(texto, (int)x, (int)y, 16, Fade(PAINEL_TEXTO_SUAVE, abertura));
    }

    DrawText("ENTER continua    ESC recua", (int)x, (int)(painel.y + painel.height - 108.0f), 16, PAINEL_TEXTO_SUAVE);
    botao_de_painel((Rectangle){ x, painel.y + painel.height - 80.0f, 280.0f, 56.0f }, "CONTINUAR", 24, false, true,
                    BOTAO_CONTINUAR_TRAPACA);
    botao_de_painel((Rectangle){ painel.x + painel.width - 310.0f, painel.y + painel.height - 80.0f, 280.0f, 56.0f },
                    "RECUAR", 24, true, true, BOTAO_RECUAR);
}

static void desenhar_trapacas(const Partida *partida)
{
    float abertura;
    float tela;
    Color tinta = { 196, 186, 176, 255 };

    if (tela_de_trapacas == TRAPACAS_FECHADAS)
        return;

    abertura = painel_progresso(inicio_das_trapacas, 0.3);
    tela = painel_suave(painel_progresso(inicio_da_tela, 0.25));
    escurecer_mesa(abertura);

    switch (tela_de_trapacas) {
        case TRAPACAS_MENU:
            desenhar_dealer_ao_fundo(partida, abertura, tinta);
            desenhar_lista_de_trapacas(partida, abertura, true);
            desenhar_detalhe(partida, abertura, false);
            break;

        case TRAPACAS_SUBORNO:
            desenhar_dealer_ao_fundo(partida, 1.0f, tinta);
            desenhar_lista_de_trapacas(partida, 1.0f, false);
            DrawRectangle(32, 80, 476, 584, Fade(BLACK, 0.4f));
            desenhar_detalhe(partida, 1.0f, true);
            DrawText("1 a 3 ou setas escolhem o serviço    ESC volta", 560, 676, 15, PAINEL_TEXTO_SUAVE);
            break;

        case TRAPACAS_CARTA_DA_MAO:
            desenhar_carta_da_mao(partida, tela);
            break;

        case TRAPACAS_ESCOLHER_CARTA:
            desenhar_grade(tela);
            break;

        case TRAPACAS_RISCO:
            desenhar_risco(partida, tela);
            break;

        case TRAPACAS_FECHADAS:
            break;
    }
}

/** Botão de fechar dos avisos, centralizado no painel. */
static void botao_do_aviso(Rectangle painel, const char *rotulo, bool principal)
{
    float largura = (float)MeasureText(rotulo, 22) + 80.0f;

    botao_de_painel((Rectangle){ painel.x + (painel.width - largura) / 2.0f, painel.y + painel.height - 78.0f,
                                 largura, 54.0f }, rotulo, 22, principal, true, BOTAO_FECHAR_AVISO);
}

static void desenhar_aviso_do_cumplice(float abertura)
{
    Rectangle painel = { 300.0f, 96.0f, 680.0f, 520.0f };
    float subida = (1.0f - painel_suave(painel_progresso(inicio_do_aviso + 0.25, 0.5))) * 50.0f;
    char texto[TAMANHO_LINHA];
    int largura_titulo = MeasureText("INFORMAÇÃO DO CÚMPLICE", 28);

    painel_desenhar(painel, abertura);
    painel_icone(ICONE_BINOCULO, (Rectangle){ painel.x + (painel.width - (float)largura_titulo) / 2.0f - 58.0f,
                                              painel.y + 22.0f, 44.0f, 40.0f }, Fade(PAINEL_OURO_VIVO, abertura));
    DrawText("INFORMAÇÃO DO CÚMPLICE", (int)(painel.x + (painel.width - (float)largura_titulo) / 2.0f),
             (int)(painel.y + 28.0f), 28, Fade(PAINEL_OURO_VIVO, abertura));

    for (int i = 0; i < 2; i++)
        cartas_desenhar_verso(marcou_carta_do_adversario[i],
                              (Rectangle){ 512.0f + (float)i * 136.0f, 186.0f, 120.0f, 168.0f });

    if (aviso_tem_categoria)
        snprintf(texto, sizeof texto, "O cúmplice fez o sinal combinado. Com as cartas que estão na mesa, ele tem: %s.",
                 poker_category_name(aviso_categoria));
    else
        snprintf(texto, sizeof texto, "O cúmplice não conseguiu ver a mão dele.");

    painel_papel((Rectangle){ 380.0f, 390.0f + subida, 520.0f, 104.0f }, texto, 19);
    botao_do_aviso(painel, "CONTINUAR", false);
}

static void desenhar_aviso_das_cartas(float abertura)
{
    Rectangle painel = { 200.0f, 92.0f, 880.0f, 528.0f };
    char texto[TAMANHO_LINHA];
    char carta0[32];
    char carta1[32];

    painel_desenhar(painel, abertura);
    desenhar_recorte(textura_dealer, DEALER_RECORTE, (Vector2){ painel.x + 20.0f, painel.y + 70.0f }, 340.0f, 0.0f,
                     Fade(WHITE, abertura));
    DrawText("O DEALER MOSTROU AS CARTAS", (int)(painel.x + 360.0f), (int)(painel.y + 30.0f), 26,
             Fade(PAINEL_OURO_VIVO, abertura));

    for (int i = 0; i < 2; i++) {
        float t = painel_progresso(inicio_do_aviso + 0.3 + 0.35 * (double)i, 0.45);
        float escala = fabsf(cosf(3.14159265f * t));
        Rectangle area = { painel.x + 380.0f + (float)i * 190.0f, painel.y + 96.0f, 150.0f, 210.0f };

        if (escala < 0.03f)
            escala = 0.03f;

        area.x += area.width * (1.0f - escala) / 2.0f;
        area.width *= escala;

        if (t >= 0.5f)
            cartas_desenhar_frente(aviso_cartas[i], false, area);
        else
            cartas_desenhar_verso(false, area);
    }

    nome_da_carta(aviso_cartas[0], carta0, sizeof carta0);
    nome_da_carta(aviso_cartas[1], carta1, sizeof carta1);
    snprintf(texto, sizeof texto, "O adversário tem %s e %s.", carta0, carta1);
    painel_papel((Rectangle){ painel.x + 380.0f, painel.y + 336.0f, 440.0f, 70.0f }, texto, 19);
    botao_do_aviso(painel, "CONTINUAR", false);
}

static void desenhar_aviso_da_bebida(const Partida *partida, float abertura)
{
    Rectangle painel = { 640.0f, 160.0f, 560.0f, 400.0f };
    float balanco = sinf((float)GetTime() * 1.7f) * 4.0f;

    desenhar_recorte(retrato_do_oponente(partida, partida->ia.style), RETRATO_INTEIRO,
                     (Vector2){ 90.0f, 70.0f }, 640.0f, balanco, Fade((Color){ 255, 222, 196, 255 }, abertura));
    painel_desenhar(painel, abertura);
    painel_icone(ICONE_GARRAFA, (Rectangle){ painel.x + 28.0f, painel.y + 24.0f, 56.0f, 56.0f },
                 Fade(PAINEL_OURO_VIVO, abertura));
    DrawText("BEBIDA BATIZADA", (int)(painel.x + 100.0f), (int)(painel.y + 36.0f), 32, Fade(PAINEL_OURO_VIVO, abertura));
    painel_papel((Rectangle){ painel.x + 30.0f, painel.y + 116.0f, painel.width - 60.0f, 120.0f },
                 "Ele bebeu sem perceber. Pelas próximas 3 mãos ele joga pior e desconfia menos de você.", 20);
    botao_do_aviso(painel, "CONTINUAR", false);
}

static void desenhar_aviso_descoberto(const Partida *partida, float abertura)
{
    float t = painel_progresso(inicio_do_aviso, 0.7);
    float tremor = (1.0f - t) * 12.0f * sinf((float)GetTime() * 70.0f);
    Rectangle painel = { 70.0f + tremor, 170.0f, 560.0f, 380.0f };

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade((Color){ 190, 0, 0, 255 }, 0.55f * (1.0f - t)));
    painel_vinheta(LARGURA_TELA, ALTURA_TELA, (Color){ 140, 0, 0, 255 }, 0.8f * abertura);
    desenhar_recorte(textura_dealer_revolver, DEALER_BUSTO, (Vector2){ 600.0f + tremor, 60.0f }, 660.0f, 0.0f,
                     Fade((Color){ 255, 96, 96, 255 }, abertura));
    DrawRectangleGradientH(600, 60, 200, 660, Fade(BLACK, 0.6f * abertura), Fade(BLACK, 0.0f));

    painel_desenhar(painel, abertura);
    DrawText("DESCOBERTO!", (int)(painel.x + 30.0f), (int)(painel.y + 34.0f), 58, PAINEL_VERMELHO_VIVO);
    DrawText("O dealer viu tudo.", (int)(painel.x + 32.0f), (int)(painel.y + 120.0f), 28, PAINEL_TEXTO);
    DrawText("Agora quem decide é o tambor.", (int)(painel.x + 32.0f), (int)(painel.y + 162.0f), 22, PAINEL_TEXTO_SUAVE);
    painel_tambor((Vector2){ painel.x + painel.width - 90.0f, painel.y + 190.0f }, 52.0f, partida->risco.balas_roleta,
                  (float)GetTime() * 0.6f);
    botao_do_aviso(painel, "ENCARAR A ROLETA", true);
}

/** O dealer com o revólver apontado, à direita: é ele quem atira. Devolve onde fica a boca do cano. */
static Vector2 desenhar_dealer_armado(float deslocamento, Color tinta)
{
    Vector2 posicao = { 600.0f + deslocamento, 60.0f };
    float altura = 660.0f;

    desenhar_recorte(textura_dealer_revolver, DEALER_BUSTO, posicao, altura, 0.0f, tinta);
    DrawRectangleGradientH(600, 60, 220, 660, Fade(BLACK, 0.7f * (float)tinta.a / 255.0f), Fade(BLACK, 0.0f));
    return ponto_no_recorte(textura_dealer_revolver, DEALER_BUSTO, posicao, altura, BOCA_DO_CANO);
}

static void desenhar_aviso_da_roleta(float abertura)
{
    Rectangle painel = { 70.0f, 140.0f, 560.0f, 460.0f };
    float t = painel_progresso(inicio_do_aviso, 1.1);
    float giro = (1.0f - painel_suave(t)) * 14.0f;
    float depois = painel_progresso(inicio_do_aviso + 1.1, 0.5);
    bool revelado = t >= 1.0f;
    float tremor = revelado && aviso_morreu ? (1.0f - depois) * 16.0f * sinf((float)GetTime() * 80.0f) : 0.0f;
    Vector2 boca = desenhar_dealer_armado(tremor, Fade(WHITE, abertura));
    char texto[TAMANHO_LINHA];

    if (revelado && aviso_morreu)
        painel_clarao(boca, 80.0f + 60.0f * depois, 1.0f - depois);

    if (revelado && !aviso_morreu) {
        float subida = painel_suave(depois) * 30.0f;
        Color tinta = Fade(PAINEL_TEXTO, 1.0f - 0.6f * depois);

        DrawText("click", (int)(boca.x + 50.0f), (int)(boca.y - 64.0f - subida), 34, tinta);
        DrawLineEx((Vector2){ boca.x + 30.0f, boca.y - 30.0f }, (Vector2){ boca.x + 46.0f, boca.y - 44.0f }, 3.0f, tinta);
        DrawLineEx((Vector2){ boca.x + 36.0f, boca.y - 12.0f }, (Vector2){ boca.x + 56.0f, boca.y - 16.0f }, 3.0f, tinta);
        DrawLineEx((Vector2){ boca.x + 22.0f, boca.y - 40.0f }, (Vector2){ boca.x + 28.0f, boca.y - 60.0f }, 3.0f, tinta);
    }

    painel_desenhar(painel, abertura);
    painel_tambor((Vector2){ painel.x + painel.width / 2.0f, painel.y + 150.0f }, 100.0f, aviso_balas, giro);

    if (!revelado) {
        texto_no_meio("O dealer puxa o gatilho...", painel, painel.y + 290.0f, 24, PAINEL_TEXTO_SUAVE);
        return;
    }

    if (aviso_morreu) {
        texto_no_meio("BANG.", painel, painel.y + 276.0f, 64, PAINEL_VERMELHO_VIVO);
        texto_no_meio("Você morreu.", painel, painel.y + 348.0f, 26, PAINEL_TEXTO);
    } else {
        texto_no_meio("CLICK.", painel, painel.y + 276.0f, 64, PAINEL_OURO_VIVO);
        snprintf(texto, sizeof texto, "Você sobreviveu. Agora são %d balas.", aviso_balas);
        texto_no_meio(texto, painel, painel.y + 348.0f, 20, PAINEL_TEXTO);
    }

    botao_do_aviso(painel, "CONTINUAR", aviso_morreu);

    /* O tiro clareia a tela inteira por um instante. */
    if (aviso_morreu)
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade((Color){ 255, 236, 214, 255 }, 0.75f * (1.0f - depois)));
}

/** O aviso aberto por uma trapaça ou pela roleta, por cima de tudo. */
static void desenhar_aviso(const Partida *partida)
{
    float abertura;

    if (aviso_aberto == AVISO_NENHUM)
        return;

    abertura = painel_suave(painel_progresso(inicio_do_aviso, 0.35));
    escurecer_mesa(abertura);

    switch (aviso_aberto) {
        case AVISO_CUMPLICE:
            desenhar_aviso_do_cumplice(abertura);
            break;

        case AVISO_CARTAS_VISTAS:
            desenhar_aviso_das_cartas(abertura);
            break;

        case AVISO_BEBIDA:
            desenhar_aviso_da_bebida(partida, abertura);
            break;

        case AVISO_DESCOBERTO:
            desenhar_aviso_descoberto(partida, abertura);
            break;

        case AVISO_ROLETA:
            desenhar_aviso_da_roleta(abertura);
            break;

        case AVISO_NENHUM:
            break;
    }
}

/** Painel da roleta russa, esperando o gatilho. */
static void desenhar_roleta(const Partida *partida)
{
    Rectangle painel = { 70.0f, 170.0f, 560.0f, 400.0f };
    int balas = partida->risco.balas_roleta;
    float x = painel.x + 250.0f;
    char texto[TAMANHO_LINHA];

    escurecer_mesa(1.0f);
    desenhar_dealer_armado(0.0f, (Color){ 236, 224, 214, 255 });

    painel_desenhar(painel, 1.0f);
    DrawText("ROLETA RUSSA", (int)(painel.x + 28.0f), (int)(painel.y + 24.0f), 32, PAINEL_VERMELHO_VIVO);
    painel_tambor((Vector2){ painel.x + 125.0f, painel.y + 196.0f }, 92.0f, balas, 0.0f);

    DrawText(partida->roleta_motivo == ROLETA_DESCOBERTO ? "O dealer te pegou." : "Você faliu.",
             (int)x, (int)(painel.y + 92.0f), 22, PAINEL_TEXTO);
    snprintf(texto, sizeof texto, "%d bala%s em 6 câmaras.", balas, balas == 1 ? "" : "s");
    DrawText(texto, (int)x, (int)(painel.y + 128.0f), 20, PAINEL_TEXTO_SUAVE);
    snprintf(texto, sizeof texto, "Chance de morrer: %d%%", balas * 100 / 6);
    DrawText(texto, (int)x, (int)(painel.y + 158.0f), 20, cor_do_risco(balas * 100 / 6));

    if (partida->roleta_motivo == ROLETA_FALENCIA) {
        snprintf(texto, sizeof texto, "Se sobreviver, volta com %lld fichas.",
                 (long long)(torneio_fichas_iniciais_adversario(partida->torneio.confronto) / 2));
        DrawText(texto, (int)x, (int)(painel.y + 188.0f), 18, PAINEL_TEXTO_SUAVE);
    }

    DrawText("O dealer aponta. ENTER e ele atira.", (int)x, (int)(painel.y + 222.0f), 17, PAINEL_TEXTO_SUAVE);
    botao_de_painel((Rectangle){ x - 30.0f, painel.y + painel.height - 84.0f, 300.0f, 56.0f }, "ENCARAR O CANO", 22,
                    true, true, BOTAO_CONTINUAR);
}

/** Retratos nas laterais da mesa; o adversário acompanha o confronto e o estilo da IA. */
static void desenhar_personagens_na_mesa(const Partida *partida)
{
    desenhar_recorte(textura_protagonista, RETRATO_INTEIRO,
                     (Vector2){ -70.0f, 280.0f }, 330.0f, 0.0f, WHITE);
    desenhar_recorte(retrato_do_oponente(partida, partida->ia.style), RETRATO_INTEIRO,
                     (Vector2){ 975.0f, 280.0f }, 330.0f, 0.0f, WHITE);
}

/** Desenha o valor com N0.png a N9.png, centralizado e ajustado à área. */
static void desenhar_valor_em_png(PokerChips valor, Rectangle area)
{
    char digitos[32];
    int quantidade;
    float altura = area.height;
    float largura_digito;
    float espacamento = 2.0f;
    float largura_total;
    float x;

    snprintf(digitos, sizeof digitos, "%lld", (long long)valor);
    quantidade = (int)strlen(digitos);

    for (int i = 0; i < quantidade; i++) {
        int indice = digitos[i] - '0';
        if (indice < 0 || indice > 9 || textura_numeros[indice].id == 0) {
            texto_no_meio(digitos, area, area.y, 28, PAINEL_OURO_VIVO);
            return;
        }
    }

    largura_digito = altura * (float)textura_numeros[0].width / (float)textura_numeros[0].height;
    largura_total = (float)quantidade * largura_digito + (float)(quantidade - 1) * espacamento;
    if (largura_total > area.width) {
        float escala = area.width / largura_total;
        largura_digito *= escala;
        altura *= escala;
        espacamento *= escala;
        largura_total = area.width;
    }

    x = area.x + (area.width - largura_total) / 2.0f;
    for (int i = 0; i < quantidade; i++) {
        Texture2D numero = textura_numeros[digitos[i] - '0'];
        DrawTexturePro(numero, (Rectangle){ 0.0f, 0.0f, (float)numero.width, (float)numero.height },
                       (Rectangle){ x, area.y + (area.height - altura) / 2.0f, largura_digito, altura },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        x += largura_digito + espacamento;
    }
}

static void desenhar_contador(Rectangle area, const char *rotulo, PokerChips valor)
{
    painel_desenhar(area, 0.95f);
    texto_no_meio(rotulo, area, area.y + 5.0f, 15, PAINEL_TEXTO_SUAVE);
    desenhar_valor_em_png(valor, (Rectangle){ area.x + 10.0f, area.y + 24.0f,
                                              area.width - 20.0f, area.height - 28.0f });
}

/** Média das chances de ser descoberto agora, nas cinco trapaças. */
static int suspeita_media(const Partida *partida)
{
    int soma = 0;

    for (int g = 0; g < GRUPOS_DE_TRAPACA; g++) {
        int chance = partida_chance_de_descoberta(partida, PEDIDO_DO_GRUPO[g]);

        soma += chance > 0 ? chance : 0;
    }

    return soma / GRUPOS_DE_TRAPACA;
}

/** Suspeita no canto de baixo da mesa: só o rótulo e a barra, como nos mockups. */
static void desenhar_suspeita_na_mesa(const Partida *partida)
{
    DrawText("SUSPEITA", 32, 642, 16, PAINEL_OURO);
    painel_barra((Rectangle){ 32.0f, 664.0f, 200.0f, 12.0f }, (float)suspeita_media(partida) / 100.0f, 5,
                 PAINEL_VERMELHO_VIVO);
}

/** Recado curto no alto da mesa, que some sozinho. */
static void desenhar_recado(void)
{
    float idade = (float)(GetTime() - inicio_do_recado);
    float opacidade;
    float largura;
    Rectangle area;

    if (idade < 0.0f || idade > 3.4f)
        return;

    opacidade = idade < 0.25f ? idade / 0.25f : (idade > 2.8f ? (3.4f - idade) / 0.6f : 1.0f);
    largura = (float)MeasureText(texto_do_recado, 18) + 90.0f;
    area = (Rectangle){ ((float)LARGURA_TELA - largura) / 2.0f, 552.0f, largura, 46.0f };

    painel_desenhar(area, opacidade);
    painel_icone(icone_do_recado, (Rectangle){ area.x + 14.0f, area.y + 8.0f, 30.0f, 30.0f },
                 Fade(PAINEL_OURO_VIVO, opacidade));
    DrawText(texto_do_recado, (int)(area.x + 58.0f), (int)(area.y + 14.0f), 18, Fade(PAINEL_TEXTO, opacidade));
}

/** Revela a mão do adversário somente após um showdown e as animações da mesa. */
static bool adversario_mostra_as_cartas(const Mao *mao)
{
    bool terminou = mao->situacao == MAO_ENCERRADA || mao->situacao == MAO_REGISTRADA;
    return terminou && mao->resultado.showdown && !animacoes_ativas();
}

/** As imagens das cartas ocupam a mesa; o adversário revela as suas no showdown. */
static void desenhar_cartas_da_mesa(const Partida *partida)
{
    const Mao *mao = &partida->mao;
    bool revelar_adversario = adversario_mostra_as_cartas(mao);
    int frentes = animacoes_board_frentes();
    int versos = animacoes_board_versos();
    float virada = animacoes_board_virada();

    if (mao->situacao == MAO_VAZIA)
        return;

    for (int i = 0; i < frentes + versos && (size_t)i < mao->cartas_no_board; i++) {
        PokerCard carta = mao->board[i];
        Rectangle area = { 375.0f + (float)i * 108.0f, 249.0f, 94.0f, 132.0f };
        bool mostrar_frente = i < frentes || virada >= 0.5f;

        if (i >= frentes && virada >= 0.0f) {
            float escala = virada < 0.5f ? 1.0f - 2.0f * virada : 2.0f * virada - 1.0f;
            if (escala < 0.02f)
                escala = 0.02f;
            area.x += area.width * (1.0f - escala) / 2.0f;
            area.width *= escala;
        }

        if (mostrar_frente)
            cartas_desenhar_frente(carta, is_card_marked(&partida->cheats, carta), area);
        else
            cartas_desenhar_verso(false, area);
    }

    for (int i = 0; i < 2; i++) {
        PokerCard carta = mao->cartas[TORNEIO_ASSENTO_JOGADOR][i];
        Rectangle area = { 314.0f + (float)i * 106.0f, 412.0f, 94.0f, 132.0f };

        cartas_desenhar_frente(carta, is_card_marked(&partida->cheats, carta), area);

        if (revelar_adversario) {
            cartas_desenhar_frente(mao->cartas[TORNEIO_ASSENTO_ADVERSARIO][i],
                                   marcou_carta_do_adversario[i], AREAS_DO_ADVERSARIO[i]);
        } else if (adversario_recebeu_cartas) {
            cartas_desenhar_verso(marcou_carta_do_adversario[i], AREAS_DO_ADVERSARIO[i]);

            if (marcou_carta_do_adversario[i] && tela_de_trapacas == TRAPACAS_FECHADAS &&
                mouse_sobre(AREAS_DO_ADVERSARIO[i]))
                cursor_sobre_botao = true;
        }
    }
}

static void desenhar_carta_ampliada(void)
{
    if (carta_marcada_ampliada < 0)
        return;

    /* O clique no fundo fecha a ampliação sem acionar os botões da mesa. */
    total_de_botoes = 0;
    cursor_sobre_botao = false;
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, (Color){ 0, 0, 0, 226 });
    desenhar_texto_centralizado("CARTA MARCADA", 112, 30, COR_OURO);
    cartas_desenhar_frente(cartas_do_adversario[carta_marcada_ampliada], true,
                          (Rectangle){ 530.0f, 192.0f, 220.0f, 309.0f });
    desenhar_texto_centralizado("Clique ou pressione ESC para fechar", 535, 20, COR_TEXTO);
}

void interface_mesa_desenhar(const Partida *partida, PokerChips aposta_escolhida)
{
    const Mao *mao = &partida->mao;
    const PokerTable *mesa = &mao->mesa;
    bool terminou = mao->situacao == MAO_ENCERRADA || mao->situacao == MAO_REGISTRADA;
    bool resultado_cobre_mesa = partida->situacao == PARTIDA_CONFRONTO_VENCIDO ||
                               partida->situacao == PARTIDA_JOGADOR_FALIDO ||
                               partida->situacao == PARTIDA_ROLETA;
    bool showdown_antes_do_resultado = terminou && mao->resultado.showdown && resultado_cobre_mesa;

    if (showdown_antes_do_resultado && !animacoes_ativas() && inicio_da_revelacao < 0.0)
        inicio_da_revelacao = GetTime();
    revelacao_em_andamento = showdown_antes_do_resultado &&
                             (animacoes_ativas() || GetTime() - inicio_da_revelacao < 1.5);

    registrar_disponibilidade(partida);
    comecar_desenho();
    cartas_do_adversario_visiveis = false;

    if (partida->situacao == PARTIDA_CONFRONTO_VENCIDO && !revelacao_em_andamento && textura_vitoria.id != 0) {
        DrawTexturePro(
            textura_vitoria,
            (Rectangle){ 0, 0, (float)textura_vitoria.width, (float)textura_vitoria.height },
            (Rectangle){ 0, 0, (float)LARGURA_TELA, (float)ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

        botao_de_texto("CONTINUAR", 100, 540, 28, COR_OURO, BOTAO_CONTINUAR);
        botao_de_texto("MENU PRINCIPAL", 100, 590, 24, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DA_MESA);
        terminar_desenho();
        return;
    } else if ((partida->situacao == PARTIDA_JOGADOR_FALIDO ||
                (partida->situacao == PARTIDA_JOGADOR_MORTO && aviso_aberto == AVISO_NENHUM)) &&
               !revelacao_em_andamento && textura_derrota.id != 0) {
        /* A morte na roleta também acaba aqui, depois do aviso do BANG. */
        DrawTexturePro(
            textura_derrota,
            (Rectangle){ 0, 0, (float)textura_derrota.width, (float)textura_derrota.height },
            (Rectangle){ 0, 0, (float)LARGURA_TELA, (float)ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

        botao_de_texto("TENTAR NOVAMENTE", 100, 540, 28, COR_OURO, BOTAO_CONTINUAR);
        botao_de_texto("MENU PRINCIPAL", 100, 590, 24, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DA_MESA);
        terminar_desenho();
        return;
    }

    if (textura_mesa.id != 0) {
        DrawTexturePro(
            textura_mesa,
            (Rectangle){ 0, 0, (float)textura_mesa.width, (float)textura_mesa.height },
            (Rectangle){ 0, 0, (float)LARGURA_TELA, (float)ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    }

    desenhar_personagens_na_mesa(partida);

    if (animacoes_iniciais_ativas()) {
        animacoes_desenhar();
        terminar_desenho();
        return;
    }

    cartas_do_adversario_visiveis = adversario_recebeu_cartas && mao->situacao != MAO_VAZIA &&
                                    !adversario_mostra_as_cartas(mao);
    desenhar_contador((Rectangle){ 554.0f, 184.0f, 172.0f, 60.0f }, "POTE", poker_pot(mesa));
    desenhar_contador((Rectangle){ 30.0f, 526.0f, 200.0f, 62.0f }, "FICHAS",
                      terminou ? partida->torneio.fichas_jogador : mesa->player[TORNEIO_ASSENTO_JOGADOR].chips);
    desenhar_contador((Rectangle){ 1050.0f, 526.0f, 200.0f, 62.0f }, "FICHAS",
                      terminou ? partida->torneio.fichas_adversario : mesa->player[TORNEIO_ASSENTO_ADVERSARIO].chips);

    desenhar_cartas_da_mesa(partida);
    animacoes_desenhar();

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            if (tela_de_trapacas == TRAPACAS_FECHADAS)
                desenhar_acoes_do_jogador(partida, aposta_escolhida);
            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            DrawText("VEZ DO ADVERSÁRIO...",
                     (int)(BOTOES_DA_MESA_DIREITA - (float)MeasureText("VEZ DO ADVERSÁRIO...", 22)),
                     (int)BOTOES_DA_MESA_Y + 14, 22, PAINEL_OURO);
            break;

        case PARTIDA_MAO_ENCERRADA:
            botao_unico_da_mesa("PRÓXIMA MÃO", BOTAO_CONTINUAR);
            break;

        case PARTIDA_CONFRONTO_VENCIDO:
            if (!revelacao_em_andamento)
                botao_unico_da_mesa("PRÓXIMO ADVERSÁRIO", BOTAO_CONTINUAR);
            break;

        case PARTIDA_JOGADOR_MORTO:
        case PARTIDA_JOGADOR_FALIDO:
            if (!revelacao_em_andamento)
                botao_unico_da_mesa("RECOMEÇAR", BOTAO_CONTINUAR);
            break;

        case PARTIDA_CAMPEAO:
            botao_unico_da_mesa("CAMPEÃO! JOGAR DE NOVO", BOTAO_CONTINUAR);
            break;

        case PARTIDA_ROLETA:
        case PARTIDA_VAZIA:
            break;
    }

    /* No canto de baixo à esquerda: o último acontecimento e a suspeita. */
    if (eventos_registrados > 0)
        DrawText(registro_de_eventos[eventos_registrados - 1], 32, 608, 16, COR_TEXTO_SUAVE);

    if (partida->situacao != PARTIDA_ROLETA)
        desenhar_suspeita_na_mesa(partida);

    /* A volta ao menu fica sozinha no alto da tela. */
    if (tela_de_trapacas == TRAPACAS_FECHADAS)
        botao_da_mesa((Rectangle){ 1160.0f, 20.0f, 100.0f, 34.0f }, "MENU", false, true, BOTAO_VOLTAR_DA_MESA);

    if (partida->situacao == PARTIDA_ROLETA && aviso_aberto == AVISO_NENHUM && !revelacao_em_andamento)
        desenhar_roleta(partida);

    desenhar_trapacas(partida);
    desenhar_recado();
    desenhar_carta_ampliada();
    desenhar_aviso(partida);
    terminar_desenho();
}
