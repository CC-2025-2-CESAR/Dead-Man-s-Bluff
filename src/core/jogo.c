#include "core/jogo.h"
#include "core/partida.h"
#include "ui/interface.h"

#include <stdbool.h>
#include <time.h>

#define PAUSA_DO_ADVERSARIO 0.8

/** Telas e fases pelas quais o jogo passa. */
typedef enum {
    ESTADO_MENU,
    ESTADO_MESA,
    ESTADO_SAIR
} EstadoJogo;

/**
 * Tudo o que a tela da mesa guarda entre quadros.
 *
 * partida: a partida em andamento.
 * ativa: se a partida foi iniciada e ainda não foi encerrada.
 * espera_do_adversario: segundos que o adversário já esperou para agir.
 * aposta: total de aposta que o jogador está ajustando, ou 0.
 */
typedef struct {
    Partida partida;
    bool ativa;
    double espera_do_adversario;
    PokerChips aposta;
} Mesa;

static Mesa mesa;

/**
 * Executa um quadro da tela inicial: lê a entrada, desenha o menu e
 * devolve o estado seguinte (continua no menu, vai para a mesa ou sai).
 */
static EstadoJogo executar_quadro_menu(OpcaoMenu *opcao)
{
    bool confirmou = interface_menu_atualizar(opcao);

    interface_menu_desenhar(*opcao);

    if (!confirmou)
        return ESTADO_MENU;

    return *opcao == MENU_JOGAR ? ESTADO_MESA : ESTADO_SAIR;
}

/**
 * Começa uma partida nova, com a semente tirada do relógio.
 *
 * @return true se a partida começou.
 */
static bool iniciar_partida(Mesa *mesa)
{
    interface_mesa_limpar_eventos();
    mesa->espera_do_adversario = 0.0;
    mesa->aposta = 0;
    mesa->ativa = partida_iniciar(&mesa->partida, (unsigned)time(NULL));
    return mesa->ativa;
}

/** Encerra a partida em andamento, se houver, e libera o que ela guarda. */
static void encerrar_partida(Mesa *mesa)
{
    partida_encerrar(&mesa->partida);
    mesa->ativa = false;
}

/** Entrega à interface os eventos que a partida registrou desde o último quadro. */
static void repassar_eventos(Partida *partida)
{
    EventoJogo evento;

    while (fila_eventos_retirar(&partida->eventos, &evento))
        interface_mesa_registrar_evento(&evento);
}

/**
 * Faz a partida andar um quadro: aplica o que o jogador pediu na vez dele,
 * deixa o adversário agir depois de uma pausa e segue para a mão ou a partida
 * seguinte quando o jogador pede.
 *
 * @param entrada o que o jogador fez neste quadro.
 * @param tempo segundos que o quadro durou.
 */
static void atualizar_partida(Mesa *mesa, const EntradaMesa *entrada, double tempo)
{
    Partida *partida = &mesa->partida;

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            mesa->aposta = partida_ajustar_aposta(partida, mesa->aposta, entrada->ajuste_aposta);

            if (entrada->comando != COMANDO_NENHUM &&
                partida_comando_jogador(partida, entrada->comando, mesa->aposta) == POKER_OK) {
                mesa->aposta = 0;
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            mesa->espera_do_adversario += tempo;

            if (mesa->espera_do_adversario >= PAUSA_DO_ADVERSARIO) {
                partida_agir_adversario(partida, NULL);
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
            if (entrada->continuar) {
                partida_continuar(partida);
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_CAMPEAO:
            if (entrada->continuar)
                iniciar_partida(mesa);
            break;

        case PARTIDA_VAZIA:
            break;
    }
}

/**
 * Executa um quadro da mesa: lê a entrada, faz a partida andar, entrega os
 * eventos à interface e desenha. Devolve o estado seguinte (continua na mesa
 * ou volta ao menu quando o jogador pede para voltar).
 */
static EstadoJogo executar_quadro_mesa(void)
{
    EntradaMesa entrada;

    if (!mesa.ativa && !iniciar_partida(&mesa))
        return ESTADO_MENU;

    entrada = interface_mesa_ler_entrada();

    if (entrada.voltar) {
        encerrar_partida(&mesa);
        return ESTADO_MENU;
    }

    atualizar_partida(&mesa, &entrada, interface_tempo_do_quadro());
    repassar_eventos(&mesa.partida);
    interface_mesa_desenhar(&mesa.partida, mesa.aposta);

    return ESTADO_MESA;
}

void jogo_executar(void)
{
    EstadoJogo estado = ESTADO_MENU;
    OpcaoMenu opcao = MENU_JOGAR;

    interface_iniciar();

    while (estado != ESTADO_SAIR && !interface_fechar_solicitado()) {
        switch (estado) {
            case ESTADO_MENU:
                estado = executar_quadro_menu(&opcao);
                break;

            case ESTADO_MESA:
                estado = executar_quadro_mesa();
                break;

            case ESTADO_SAIR:
                break;
        }
    }

    if (mesa.ativa)
        encerrar_partida(&mesa);

    interface_encerrar();
}
