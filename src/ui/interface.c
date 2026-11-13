#include "ui/interface.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define TITULO_JANELA "Dead Man's Bluff"
#define EVENTOS_NO_REGISTRO 6
#define TAMANHO_LINHA 160

static char registro_de_eventos[EVENTOS_NO_REGISTRO][TAMANHO_LINHA];
static int eventos_registrados;

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

double interface_tempo_do_quadro(void)
{
    return (double)GetFrameTime();
}

EntradaMesa interface_mesa_ler_entrada(void)
{
    EntradaMesa entrada = {COMANDO_NENHUM, 0, false, false};

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
}

void interface_mesa_registrar_evento(const EventoJogo *evento)
{
    char linha[TAMANHO_LINHA];
    char cartas[64];
    static const char *RUAS[] = {"Pré-flop", "Flop", "Turn", "River"};

    switch (evento->tipo) {
        case EVENTO_MAO_INICIADA:
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

        case EVENTO_CARTAS_DISTRIBUIDAS:
            break;
    }
}

/** Desenha uma linha de texto simples e desce a posição vertical. */
static void desenhar_linha(const char *texto, int *y, Color cor)
{
    DrawText(texto, 40, *y, 22, cor);
    *y += 32;
}

void interface_mesa_desenhar(const Partida *partida, PokerChips aposta_escolhida)
{
    const Mao *mao = &partida->mao;
    const PokerTable *mesa = &mao->mesa;
    bool terminou = mao->situacao == MAO_ENCERRADA || mao->situacao == MAO_REGISTRADA;
    char linha[TAMANHO_LINHA];
    char cartas[64];
    int y = 24;

    BeginDrawing();
    ClearBackground(COR_FUNDO);

    snprintf(linha, sizeof linha, "CONFRONTO %d/%d: %s     MÃO %d     BLINDS %lld/%lld",
             partida->torneio.confronto + 1, TORNEIO_CONFRONTOS,
             partida_nome_do_adversario(partida->torneio.confronto),
             partida->torneio.maos_jogadas + (terminou ? 0 : 1),
             (long long)(mesa->big_blind / 2), (long long)mesa->big_blind);
    desenhar_linha(linha, &y, COR_OURO);

    snprintf(linha, sizeof linha, "VOCÊ: %lld     ADVERSÁRIO: %lld     POTE: %lld",
             (long long)mesa->player[TORNEIO_ASSENTO_JOGADOR].chips,
             (long long)mesa->player[TORNEIO_ASSENTO_ADVERSARIO].chips,
             (long long)poker_pot(mesa));
    desenhar_linha(linha, &y, COR_TEXTO);

    texto_das_cartas(mao->cartas[TORNEIO_ASSENTO_JOGADOR], 2, cartas, sizeof cartas);
    snprintf(linha, sizeof linha, "SUAS CARTAS: %s", cartas);
    desenhar_linha(linha, &y, COR_TEXTO);

    texto_das_cartas(mao->board, (int)mao->cartas_no_board, cartas, sizeof cartas);
    snprintf(linha, sizeof linha, "BOARD: %s", mao->cartas_no_board > 0 ? cartas : "-");
    desenhar_linha(linha, &y, COR_TEXTO);

    if (terminou && mao->resultado.showdown) {
        texto_das_cartas(mao->cartas[TORNEIO_ASSENTO_ADVERSARIO], 2, cartas, sizeof cartas);
    } else {
        snprintf(cartas, sizeof cartas, "ocultas");
    }

    snprintf(linha, sizeof linha, "CARTAS DO ADVERSÁRIO: %s", cartas);
    desenhar_linha(linha, &y, COR_TEXTO);
    y += 16;

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            if (aposta_escolhida > 0) {
                snprintf(linha, sizeof linha,
                         "SUA VEZ: [F] desistir  [C] passar ou pagar  [R] apostar %lld (setas ajustam)  [A] all-in",
                         (long long)aposta_escolhida);
            } else {
                snprintf(linha, sizeof linha, "SUA VEZ: [F] desistir  [C] passar ou pagar  [A] all-in");
            }
            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            snprintf(linha, sizeof linha, "Vez do adversário...");
            break;

        case PARTIDA_MAO_ENCERRADA:
            snprintf(linha, sizeof linha, "Mão encerrada. ENTER para a próxima mão");
            break;

        case PARTIDA_CONFRONTO_VENCIDO:
            snprintf(linha, sizeof linha, "Você venceu o confronto! ENTER para o próximo adversário");
            break;

        case PARTIDA_JOGADOR_FALIDO:
            snprintf(linha, sizeof linha, "Você faliu. ENTER para recomeçar");
            break;

        case PARTIDA_CAMPEAO:
            snprintf(linha, sizeof linha, "Você é o campeão! ENTER para recomeçar");
            break;

        case PARTIDA_VAZIA:
            snprintf(linha, sizeof linha, "Sem partida");
            break;
    }

    desenhar_linha(linha, &y, COR_OURO);
    y += 16;

    for (int i = 0; i < eventos_registrados; i++) {
        desenhar_linha(registro_de_eventos[i], &y, COR_TEXTO_SUAVE);
    }

    DrawText("ESC volta ao menu", 40, ALTURA_TELA - 40, 20, COR_TEXTO_SUAVE);

    EndDrawing();
}
