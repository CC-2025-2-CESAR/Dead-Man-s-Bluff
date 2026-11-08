#include "core/jogo.h"
#include "ui/interface.h"

/** Telas e fases pelas quais o jogo passa. */
typedef enum {
    ESTADO_MENU,
    ESTADO_MESA,
    ESTADO_SAIR
} EstadoJogo;

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
 * Executa um quadro da mesa: desenha a tela e devolve o estado seguinte
 * (continua na mesa ou volta ao menu quando o jogador pede para voltar).
 */
static EstadoJogo executar_quadro_mesa(void)
{
    bool voltar = interface_voltar_pressionado();

    interface_mesa_desenhar();

    return voltar ? ESTADO_MENU : ESTADO_MESA;
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

    interface_encerrar();
}
