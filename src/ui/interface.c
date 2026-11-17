#include "ui/interface.h"
#include "raylib.h"

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

#define BOTOES_MAXIMOS 40
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

    desenhar_texto_centralizado(pergunta, 240, 56, COR_OURO);
    DrawRectangle((LARGURA_TELA - 320) / 2, 320, 320, 3, COR_OURO);
    desenhar_texto_centralizado(detalhe, 360, 28, COR_TEXTO);

    botao_de_texto("SIM", LARGURA_TELA / 2 - 150 - MeasureText("SIM", 44) / 2, 450, 44, COR_OURO, BOTAO_SIM);
    botao_de_texto("NÃO", LARGURA_TELA / 2 + 150 - MeasureText("NÃO", 44) / 2, 450, 44, COR_TEXTO_SUAVE, BOTAO_NAO);

    desenhar_texto_centralizado("Clique, ou ENTER para sim e ESC para não", ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);

    terminar_desenho();
}

EntradaConfiguracoes interface_configuracoes_ler_entrada(void)
{
    EntradaConfiguracoes entrada = {0, false, false, -1};
    int apontada = botao_sob_o_mouse();
    int clicada = botao_clicado();

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        entrada.movimento = 1;
    } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        entrada.movimento = -1;
    }

    entrada.confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    entrada.voltar = IsKeyPressed(KEY_ESCAPE);

    if (mouse_se_moveu() && apontada >= BOTAO_RESOLUCAO && apontada < BOTAO_RESOLUCAO + RESOLUCOES_TOTAL)
        entrada.apontada = apontada - BOTAO_RESOLUCAO;

    if (clicada >= BOTAO_RESOLUCAO && clicada < BOTAO_RESOLUCAO + RESOLUCOES_TOTAL) {
        entrada.apontada = clicada - BOTAO_RESOLUCAO;
        entrada.confirmar = true;
    } else if (clicada == BOTAO_VOLTAR_CONFIGURACOES) {
        entrada.voltar = true;
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

void interface_configuracoes_desenhar(const Configuracoes *configuracoes, int selecionada)
{
    comecar_desenho();

    desenhar_texto_centralizado("CONFIGURAÇÕES", 90, 64, COR_OURO);
    DrawRectangle((LARGURA_TELA - 320) / 2, 175, 320, 3, COR_OURO);
    desenhar_texto_centralizado("RESOLUÇÃO", 215, 32, COR_TEXTO);

    for (int i = 0; i < RESOLUCOES_TOTAL; i++) {
        Resolucao resolucao = configuracoes_resolucao(i);
        bool cabe = interface_resolucao_cabe(resolucao);
        bool atual = configuracoes != NULL && i == configuracoes->resolucao;
        char rotulo[32];
        char linha[96];
        int y = 290 + i * 56;

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

    botao_centralizado("VOLTAR", 585, 30, COR_TEXTO_SUAVE, BOTAO_VOLTAR_CONFIGURACOES);

    desenhar_texto_centralizado("Clique em uma opção para aplicar, ou use W/S, ENTER e ESC",
                                ALTURA_TELA - 60, 20, COR_TEXTO_SUAVE);

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
#define BOTAO_TRAPACA 400
#define BOTAO_CARTA_DA_MAO 420
#define BOTAO_VALOR_MENOS 430
#define BOTAO_VALOR_MAIS 431
#define BOTAO_NAIPE_ACIMA 432
#define BOTAO_NAIPE_ABAIXO 433
#define BOTAO_CONFIRMAR_CARTA 434
#define BOTAO_VOLTAR_DAS_TRAPACAS 435

/** Telas do menu das trapaças, que aparece por cima da mesa na vez do jogador. */
typedef enum {
    TRAPACAS_FECHADAS = 0,
    TRAPACAS_MENU,
    TRAPACAS_CARTA_DA_MAO,
    TRAPACAS_ESCOLHER_CARTA
} TelaDeTrapacas;

static TelaDeTrapacas tela_de_trapacas;
static TipoDePedido trapaca_em_andamento;
static int destaque_das_trapacas;
static int valor_do_seletor = 14;
static int naipe_do_seletor = POKER_SPADES;
static const char *aviso_das_trapacas;

/* O que o desenho do quadro anterior viu da partida, para a leitura de entrada consultar. */
static bool vez_do_jogador_vista;
static PokerStatus trapaca_status[PEDIDO_TOTAL];
static bool trapaca_vale_para_a_carta[PEDIDO_TOTAL][2];
static PokerCard cartas_a_vista[10];
static int total_de_cartas_a_vista;

/** Cartas do adversário que o suborno mostrou na mão atual. */
static PokerCard cartas_reveladas[2];
static bool ha_cartas_reveladas;

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

    tela_de_trapacas = TRAPACAS_MENU;
    destaque_das_trapacas = 0;
    aviso_das_trapacas = NULL;
}

/** Escolhe uma trapaça no menu: pede a carta que faltar, ou devolve o pedido pronto. */
static void escolher_trapaca(TipoDePedido tipo, EntradaMesa *entrada)
{
    if (tipo <= PEDIDO_NENHUM || tipo >= PEDIDO_TOTAL)
        return;

    if (trapaca_status[tipo] != POKER_OK) {
        aviso_das_trapacas = motivo_da_trapaca(trapaca_status[tipo]);
        return;
    }

    aviso_das_trapacas = NULL;

    switch (tipo) {
        case PEDIDO_MARCAR_CARTA:
        case PEDIDO_MANGA:
            trapaca_em_andamento = tipo;
            tela_de_trapacas = TRAPACAS_CARTA_DA_MAO;
            break;

        case PEDIDO_SUBORNO_MESA:
        case PEDIDO_SUBORNO_MAO:
            trapaca_em_andamento = tipo;
            tela_de_trapacas = TRAPACAS_ESCOLHER_CARTA;
            break;

        default:
            entrada->pedido = montar_pedido(tipo, 0, (PokerCard){ 0, POKER_CLUBS });
            fechar_trapacas();
            break;
    }
}

/** Lê o teclado e o mouse enquanto o menu das trapaças está aberto. */
static void ler_entrada_das_trapacas(EntradaMesa *entrada)
{
    int botao = botao_clicado();
    bool voltar = IsKeyPressed(KEY_ESCAPE) || botao == BOTAO_VOLTAR_DAS_TRAPACAS;

    if (tela_de_trapacas == TRAPACAS_MENU) {
        if (voltar || IsKeyPressed(KEY_T)) {
            fechar_trapacas();
            return;
        }

        for (int i = 0; i < PEDIDO_TOTAL - 1; i++) {
            if (IsKeyPressed(KEY_ONE + i))
                escolher_trapaca((TipoDePedido)(i + 1), entrada);
        }

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            destaque_das_trapacas = (destaque_das_trapacas + PEDIDO_TOTAL - 2) % (PEDIDO_TOTAL - 1);
        else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            destaque_das_trapacas = (destaque_das_trapacas + 1) % (PEDIDO_TOTAL - 1);

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
            escolher_trapaca((TipoDePedido)(destaque_das_trapacas + 1), entrada);

        if (mouse_se_moveu()) {
            int sob = botao_sob_o_mouse();

            if (sob > BOTAO_TRAPACA && sob < BOTAO_TRAPACA + PEDIDO_TOTAL)
                destaque_das_trapacas = sob - BOTAO_TRAPACA - 1;
        }

        if (botao > BOTAO_TRAPACA && botao < BOTAO_TRAPACA + PEDIDO_TOTAL)
            escolher_trapaca((TipoDePedido)(botao - BOTAO_TRAPACA), entrada);

        return;
    }

    if (voltar) {
        tela_de_trapacas = TRAPACAS_MENU;
        aviso_das_trapacas = NULL;
        return;
    }

    if (tela_de_trapacas == TRAPACAS_CARTA_DA_MAO) {
        for (int i = 0; i < 2; i++) {
            if (IsKeyPressed(KEY_ONE + i) || botao == BOTAO_CARTA_DA_MAO + i) {
                if (!trapaca_vale_para_a_carta[trapaca_em_andamento][i]) {
                    aviso_das_trapacas = "Essa carta já está marcada.";
                    continue;
                }

                entrada->pedido = montar_pedido(trapaca_em_andamento, i, (PokerCard){ 0, POKER_CLUBS });
                fechar_trapacas();
                return;
            }
        }

        return;
    }

    /* TRAPACAS_ESCOLHER_CARTA */
    if (IsKeyPressed(KEY_RIGHT) || botao == BOTAO_VALOR_MAIS)
        valor_do_seletor = valor_do_seletor >= 14 ? 2 : valor_do_seletor + 1;
    else if (IsKeyPressed(KEY_LEFT) || botao == BOTAO_VALOR_MENOS)
        valor_do_seletor = valor_do_seletor <= 2 ? 14 : valor_do_seletor - 1;

    if (IsKeyPressed(KEY_UP) || botao == BOTAO_NAIPE_ACIMA)
        naipe_do_seletor = (naipe_do_seletor + 1) % 4;
    else if (IsKeyPressed(KEY_DOWN) || botao == BOTAO_NAIPE_ABAIXO)
        naipe_do_seletor = (naipe_do_seletor + 3) % 4;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || botao == BOTAO_CONFIRMAR_CARTA) {
        PokerCard carta = { valor_do_seletor, (PokerSuit)naipe_do_seletor };

        if (carta_esta_a_vista(carta)) {
            aviso_das_trapacas = "Você já vê essa carta. Peça outra.";
            return;
        }

        entrada->pedido = montar_pedido(trapaca_em_andamento, 0, carta);
        fechar_trapacas();
    }
}

EntradaMesa interface_mesa_ler_entrada(void)
{
    EntradaMesa entrada = { COMANDO_NENHUM, 0, false, false, { PEDIDO_NENHUM, 0, { 0, POKER_CLUBS } } };

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

    if (IsKeyPressed(KEY_UP)) {
        entrada.ajuste_aposta = 1;
    } else if (IsKeyPressed(KEY_DOWN)) {
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

    return entrada;
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
    ha_cartas_reveladas = false;
    fechar_trapacas();
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

void interface_mesa_registrar_evento(const EventoJogo *evento)
{
    char linha[TAMANHO_LINHA];
    char cartas[64];
    static const char *RUAS[] = {"Pré-flop", "Flop", "Turn", "River"};

    switch (evento->tipo) {
        case EVENTO_MAO_INICIADA:
            ha_cartas_reveladas = false;
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
            if (evento->trapaca.pedido == PEDIDO_SUBORNO_VER &&
                evento->trapaca.resultado == TRAPACA_SUCESSO &&
                evento->trapaca.quantidade_de_cartas == 2) {
                cartas_reveladas[0] = evento->trapaca.cartas[0];
                cartas_reveladas[1] = evento->trapaca.cartas[1];
                ha_cartas_reveladas = true;
            }

            linha_da_trapaca(&evento->trapaca, linha, sizeof linha);
            guardar_linha(linha);
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
            break;

        case EVENTO_CARTAS_DISTRIBUIDAS:
            break;
    }
}

/**
 * Diz, ao lado do nome do adversário, como ele joga esta mão quando o estilo
 * dele mudou em relação ao de base (por exemplo " (joga medroso)"). Devolve
 * texto vazio se o estilo é o de sempre.
 */
static void texto_da_mudanca_de_estilo(const Partida *partida, char *out, size_t tamanho)
{
    Torneio base = partida->torneio_da_mao;
    const char *nome;

    base.fichas_adversario = torneio_fichas_iniciais_adversario(base.confronto);
    out[0] = '\0';

    if (partida->ia.style == partida_estilo_do_adversario(&base)) {
        return;
    }

    switch (partida->ia.style) {
        case POKER_AI_AGGRESSIVE:
            nome = "agressivo";
            break;

        case POKER_AI_CAUTIOUS:
            nome = "medroso";
            break;

        default:
            nome = "equilibrado";
            break;
    }

    snprintf(out, tamanho, " (joga %s)", nome);
}

/** Desenha uma linha de texto simples e desce a posição vertical. */
static void desenhar_linha(const char *texto, int *y, Color cor)
{
    DrawText(texto, 40, *y, 22, cor);
    *y += 32;
}

/**
 * Desenha a linha de ações da vez do jogador, cada ação como um botão ao
 * lado do outro. O valor da aposta e os botões de menos e mais só aparecem
 * se ele pode apostar.
 */
static void desenhar_acoes_do_jogador(int y, PokerChips aposta_escolhida)
{
    char rotulo[64];
    int x = 40;

    DrawText("SUA VEZ:", x, y, 22, COR_OURO);
    x += MeasureText("SUA VEZ:", 22) + 24;

    x += botao_de_texto("[F] desistir", x, y, 22, COR_OURO, BOTAO_DESISTIR) + 24;
    x += botao_de_texto("[C] passar ou pagar", x, y, 22, COR_OURO, BOTAO_PASSAR_OU_PAGAR) + 24;

    if (aposta_escolhida > 0) {
        snprintf(rotulo, sizeof rotulo, "[R] apostar %lld", (long long)aposta_escolhida);
        x += botao_de_texto(rotulo, x, y, 22, COR_OURO, BOTAO_APOSTAR) + 12;
        x += botao_de_texto("[-]", x, y, 22, COR_OURO, BOTAO_DIMINUIR) + 12;
        x += botao_de_texto("[+]", x, y, 22, COR_OURO, BOTAO_AUMENTAR) + 24;
    }

    x += botao_de_texto("[A] all-in", x, y, 22, COR_OURO, BOTAO_ALL_IN) + 24;
    botao_de_texto("[T] trapaças", x, y, 22, COR_OURO, BOTAO_TRAPACAS);
}

/** Escreve o texto de um valor de carta, de 2 a 14. */
static const char *texto_do_valor(int valor)
{
    static const char *VALORES[] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
    };

    return valor >= 2 && valor <= 14 ? VALORES[valor - 2] : "?";
}

static const char *texto_do_naipe(int naipe)
{
    static const char *NAIPES[] = { "paus", "ouros", "copas", "espadas" };

    return naipe >= 0 && naipe <= 3 ? NAIPES[naipe] : "?";
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

/** Desenha o menu das trapaças por cima da mesa, na tela em que ele está. */
static void desenhar_painel_das_trapacas(const Partida *partida)
{
    static const char *ROTULOS[] = {
        "Batizar a bebida do adversário",
        "Cúmplice",
        "Marcar uma carta",
        "Subornar o dealer: carta na mesa",
        "Subornar o dealer: carta na sua mão",
        "Subornar o dealer: ver as cartas dele",
        "Ás na manga"
    };
    static const char *DESCRICOES[] = {
        "Embriaga o adversário por 3 mãos: ele joga pior e desconfia menos.",
        "O cúmplice conta que tipo de mão o adversário tem agora.",
        "Marca uma carta sua para reconhecê-la na mão dele nas próximas mãos.",
        "O dealer põe a carta escolhida como a primeira da próxima rua do board.",
        "O dealer põe a carta escolhida na sua mão, na próxima mão.",
        "O dealer mostra as duas cartas do adversário.",
        "Troca uma carta da sua mão pela carta da manga."
    };
    char linha[TAMANHO_LINHA];
    char carta[8];
    int y = 308;

    if (tela_de_trapacas == TRAPACAS_FECHADAS)
        return;

    DrawRectangle(24, 296, LARGURA_TELA - 48, ALTURA_TELA - 316, COR_FUNDO);
    DrawRectangleLines(24, 296, LARGURA_TELA - 48, ALTURA_TELA - 316, COR_OURO);

    if (tela_de_trapacas == TRAPACAS_MENU) {
        DrawText("TRAPAÇAS  (1 a 7, W/S e ENTER ou cliques escolhem; ESC fecha)", 40, y, 22, COR_OURO);
        botao_de_texto("[voltar]", LARGURA_TELA - 150, y, 22, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DAS_TRAPACAS);
        y += 40;

        for (int i = 0; i < PEDIDO_TOTAL - 1; i++) {
            TipoDePedido tipo = (TipoDePedido)(i + 1);
            bool vale = trapaca_status[tipo] == POKER_OK;
            const char *sufixo = "";
            char manga[24] = "";

            if (!vale)
                sufixo = trapaca_status[tipo] == POKER_ERR_CHIPS ? "   (sem fichas)" : "   (não vale agora)";

            if (tipo == PEDIDO_MANGA) {
                texto_da_carta(partida->cheats.carta_na_manga, carta, sizeof carta);
                snprintf(manga, sizeof manga, " (manga: %s)", carta);
            }

            snprintf(linha, sizeof linha, "%s[%d] %s%s     custo %lld     risco %d%%%s%s",
                     i == destaque_das_trapacas ? "> " : "  ", i + 1, ROTULOS[i], manga,
                     (long long)partida_custo_da_trapaca(partida, tipo),
                     partida_chance_de_descoberta(partida, tipo), sufixo,
                     i == destaque_das_trapacas ? " <" : "");
            botao_de_texto(linha, 40, y, 22,
                           !vale ? COR_TEXTO_SUAVE : (i == destaque_das_trapacas ? COR_OURO : COR_TEXTO),
                           BOTAO_TRAPACA + (int)tipo);
            y += 34;
        }

        y += 10;
        DrawText(DESCRICOES[destaque_das_trapacas], 40, y, 22, COR_TEXTO);
        y += 34;

        if (aviso_das_trapacas != NULL)
            DrawText(aviso_das_trapacas, 40, y, 22, COR_OURO);

        return;
    }

    if (tela_de_trapacas == TRAPACAS_CARTA_DA_MAO) {
        snprintf(linha, sizeof linha, "%s: QUAL CARTA DA SUA MÃO?  (1 ou 2; ESC volta)",
                 trapaca_em_andamento == PEDIDO_MANGA ? "ÁS NA MANGA" : "MARCAR CARTA");
        DrawText(linha, 40, y, 22, COR_OURO);
        botao_de_texto("[voltar]", LARGURA_TELA - 150, y, 22, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DAS_TRAPACAS);
        y += 60;

        for (int i = 0; i < 2; i++) {
            bool vale = trapaca_vale_para_a_carta[trapaca_em_andamento][i];

            texto_da_carta(partida->mao.cartas[TORNEIO_ASSENTO_JOGADOR][i], carta, sizeof carta);
            snprintf(linha, sizeof linha, "[%d] %s%s", i + 1, carta, vale ? "" : "   (não vale)");
            botao_de_texto(linha, 40, y, 36, vale ? COR_OURO : COR_TEXTO_SUAVE, BOTAO_CARTA_DA_MAO + i);
            y += 60;
        }

        if (aviso_das_trapacas != NULL)
            DrawText(aviso_das_trapacas, 40, y, 22, COR_OURO);

        return;
    }

    snprintf(linha, sizeof linha, "SUBORNO: QUE CARTA VOCÊ PEDE AO DEALER?  (ESC volta)");
    DrawText(linha, 40, y, 22, COR_OURO);
    botao_de_texto("[voltar]", LARGURA_TELA - 150, y, 22, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DAS_TRAPACAS);
    y += 50;

    snprintf(linha, sizeof linha, "%s de %s", texto_do_valor(valor_do_seletor), texto_do_naipe(naipe_do_seletor));
    DrawText(linha, 40, y, 44, carta_esta_a_vista((PokerCard){ valor_do_seletor, (PokerSuit)naipe_do_seletor })
                               ? COR_TEXTO_SUAVE : COR_OURO);
    y += 70;

    botao_de_texto("[<] valor", 40, y, 22, COR_OURO, BOTAO_VALOR_MENOS);
    botao_de_texto("[>] valor", 240, y, 22, COR_OURO, BOTAO_VALOR_MAIS);
    botao_de_texto("[^] naipe", 460, y, 22, COR_OURO, BOTAO_NAIPE_ACIMA);
    botao_de_texto("[v] naipe", 680, y, 22, COR_OURO, BOTAO_NAIPE_ABAIXO);
    y += 50;

    botao_de_texto("[ENTER] pedir esta carta", 40, y, 22, COR_OURO, BOTAO_CONFIRMAR_CARTA);
    y += 40;
    DrawText("Setas: esquerda e direita mudam o valor, para cima e para baixo o naipe.", 40, y, 20, COR_TEXTO_SUAVE);
    y += 34;

    if (aviso_das_trapacas != NULL)
        DrawText(aviso_das_trapacas, 40, y, 22, COR_OURO);
}

void interface_mesa_desenhar(const Partida *partida, PokerChips aposta_escolhida)
{
    const Mao *mao = &partida->mao;
    const PokerTable *mesa = &mao->mesa;
    bool terminou = mao->situacao == MAO_ENCERRADA || mao->situacao == MAO_REGISTRADA;
    char linha[TAMANHO_LINHA];
    char estilo_da_mao[24];
    char cartas[64];
    PokerCard ordenadas[MAO_CARTAS_ORDENADAS];
    size_t quantidade;
    int y = 24;
    int extra = 0;

    registrar_disponibilidade(partida);
    comecar_desenho();

    if (partida->situacao == PARTIDA_CONFRONTO_VENCIDO && textura_vitoria.id != 0) {
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
    }else if (partida->situacao == PARTIDA_JOGADOR_FALIDO && textura_derrota.id != 0) {
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
    texto_da_mudanca_de_estilo(partida, estilo_da_mao, sizeof estilo_da_mao);
    snprintf(linha, sizeof linha, "CONFRONTO %d/%d: %s%s     MÃO %d     BLINDS %lld/%lld",
             partida->torneio.confronto + 1, TORNEIO_CONFRONTOS,
             partida_nome_do_adversario(partida->torneio.confronto),
             estilo_da_mao,
             partida->torneio.maos_jogadas + (terminou ? 0 : 1),
             (long long)(mesa->big_blind / 2), (long long)mesa->big_blind);
    desenhar_linha(linha, &y, COR_OURO);

    /* Depois da mão, o torneio manda: ele já inclui o que a roleta devolveu. */
    snprintf(linha, sizeof linha, "VOCÊ: %lld     ADVERSÁRIO: %lld     POTE: %lld     BALAS NO TAMBOR: %d/6",
             (long long)(terminou ? partida->torneio.fichas_jogador
                                  : mesa->player[TORNEIO_ASSENTO_JOGADOR].chips),
             (long long)(terminou ? partida->torneio.fichas_adversario
                                  : mesa->player[TORNEIO_ASSENTO_ADVERSARIO].chips),
             (long long)poker_pot(mesa),
             partida->risco.balas_roleta);
    desenhar_linha(linha, &y, COR_TEXTO);

    {
        char c0[8];
        char c1[8];

        texto_da_carta(mao->cartas[TORNEIO_ASSENTO_JOGADOR][0], c0, sizeof c0);
        texto_da_carta(mao->cartas[TORNEIO_ASSENTO_JOGADOR][1], c1, sizeof c1);
        snprintf(linha, sizeof linha, "SUAS CARTAS: %s%s %s%s", c0,
                 is_card_marked(&partida->cheats, mao->cartas[TORNEIO_ASSENTO_JOGADOR][0]) ? " (marcada)" : "",
                 c1,
                 is_card_marked(&partida->cheats, mao->cartas[TORNEIO_ASSENTO_JOGADOR][1]) ? " (marcada)" : "");
        desenhar_linha(linha, &y, COR_TEXTO);
    }

    texto_das_cartas(mao->board, (int)mao->cartas_no_board, cartas, sizeof cartas);
    snprintf(linha, sizeof linha, "BOARD: %s", mao->cartas_no_board > 0 ? cartas : "-");
    desenhar_linha(linha, &y, COR_TEXTO);

    quantidade = mao_cartas_ordenadas(mao, TORNEIO_ASSENTO_JOGADOR, ordenadas);
    texto_das_cartas(ordenadas, (int)quantidade, cartas, sizeof cartas);
    snprintf(linha, sizeof linha, "SUA MÃO ORDENADA: %s", quantidade > 0 ? cartas : "-");
    desenhar_linha(linha, &y, COR_TEXTO);

    {
        char manga[8];
        int marcas = 0;

        for (int i = 0; i < 5; i++)
            marcas += partida->cheats.turnos_marcacao[i] > 0 ? 1 : 0;

        texto_da_carta(partida->cheats.carta_na_manga, manga, sizeof manga);

        if (partida->cheats.maos_embriagado > 0) {
            snprintf(linha, sizeof linha, "TRAPAÇAS: manga %s     cartas marcadas: %d     adversário embriagado (%d mãos)",
                     manga, marcas, partida->cheats.maos_embriagado);
        } else {
            snprintf(linha, sizeof linha, "TRAPAÇAS: manga %s     cartas marcadas: %d     adversário sóbrio", manga, marcas);
        }

        desenhar_linha(linha, &y, COR_TEXTO);
    }

    if (terminou && mao->resultado.showdown) {
        texto_das_cartas(mao->cartas[TORNEIO_ASSENTO_ADVERSARIO], 2, cartas, sizeof cartas);
    } else if (ha_cartas_reveladas) {
        texto_das_cartas(cartas_reveladas, 2, cartas, sizeof cartas);
        strncat(cartas, " (suborno)", sizeof cartas - strlen(cartas) - 1);
    } else {
        snprintf(cartas, sizeof cartas, "ocultas");
    }

    snprintf(linha, sizeof linha, "CARTAS DO ADVERSÁRIO: %s", cartas);
    desenhar_linha(linha, &y, COR_TEXTO);
    y += 16;

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            if (tela_de_trapacas != TRAPACAS_FECHADAS)
                DrawText("Escolhendo uma trapaça...", 40, y, 22, COR_OURO);
            else
                desenhar_acoes_do_jogador(y, aposta_escolhida);
            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            DrawText("Vez do adversário...", 40, y, 22, COR_OURO);
            break;

        case PARTIDA_MAO_ENCERRADA:
            botao_de_texto("Mão encerrada. ENTER ou clique aqui para a próxima mão", 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            break;

        case PARTIDA_CONFRONTO_VENCIDO:
            botao_de_texto("Você venceu o confronto! ENTER ou clique aqui para o próximo adversário", 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            break;

        case PARTIDA_ROLETA:
            snprintf(linha, sizeof linha, "ROLETA RUSSA: %d bala(s) no tambor de 6. ENTER ou clique aqui para puxar o gatilho",
                     partida->risco.balas_roleta);
            botao_de_texto(linha, 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            extra = 32;
            snprintf(linha, sizeof linha, "Você faliu. Se sobreviver, volta ao confronto com %lld fichas.",
                     (long long)(torneio_fichas_iniciais_adversario(partida->torneio.confronto) / 2));
            DrawText(linha, 40, y + extra, 22, COR_TEXTO);
            break;

        case PARTIDA_JOGADOR_MORTO:
            botao_de_texto("Você morreu na roleta. ENTER ou clique aqui para recomeçar", 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            break;

        case PARTIDA_JOGADOR_FALIDO:
            botao_de_texto("Você faliu e não pode mais voltar. ENTER ou clique aqui para recomeçar", 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            break;

        case PARTIDA_CAMPEAO:
            botao_de_texto("Você é o campeão! ENTER ou clique aqui para recomeçar", 40, y, 22, COR_OURO, BOTAO_CONTINUAR);
            break;

        case PARTIDA_VAZIA:
            DrawText("Sem partida", 40, y, 22, COR_OURO);
            break;
    }

    y += 32 + 16 + extra;

    for (int i = 0; i < eventos_registrados; i++) {
        desenhar_linha(registro_de_eventos[i], &y, COR_TEXTO_SUAVE);
    }

    if (tela_de_trapacas == TRAPACAS_FECHADAS)
        botao_de_texto("ESC ou clique aqui volta ao menu", 40, ALTURA_TELA - 40, 20, COR_TEXTO_SUAVE, BOTAO_VOLTAR_DA_MESA);

    desenhar_painel_das_trapacas(partida);
    terminar_desenho();
}
