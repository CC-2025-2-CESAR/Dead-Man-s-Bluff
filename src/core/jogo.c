#include "core/configuracoes.h"
#include "core/jogo.h"
#include "core/partida.h"
#include "core/salvamento.h"
#include "ui/interface.h"
#include "ui/animacoes.h"

#include <stdbool.h>
#include <time.h>

#define PAUSA_DO_ADVERSARIO 0.8

/** Telas e fases pelas quais o jogo passa. */
typedef enum {
    ESTADO_MENU,
    ESTADO_MESA,
    ESTADO_CONFIGURACOES,
    ESTADO_CONFIRMAR_NOVO_JOGO,
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
static Configuracoes configuracoes;
static int selecao_da_resolucao;
static OpcaoMenu opcao_do_menu;
static bool ha_jogo_salvo;
static const char *aviso_do_menu;

/**
 * Confere se há um jogo salvo e põe a seleção do menu em Continuar, se houver,
 * ou em Novo jogo.
 */
static void atualizar_menu(void)
{
    ha_jogo_salvo = salvamento_existe(SALVAMENTO_ARQUIVO);
    opcao_do_menu = ha_jogo_salvo ? MENU_CONTINUAR : MENU_NOVO_JOGO;
}

/**
 * Guarda a partida em andamento no arquivo de salvamento. Quando a partida
 * acabou, por falência ou título, apaga o salvamento, porque não há mais o
 * que continuar. Uma falha ao gravar não interrompe o jogo.
 */
static void salvar_progresso(const Mesa *mesa)
{
    PartidaSalva salva;

    if (!mesa->ativa)
        return;

    if (mesa->partida.situacao == PARTIDA_JOGADOR_FALIDO ||
        mesa->partida.situacao == PARTIDA_JOGADOR_MORTO ||
        mesa->partida.situacao == PARTIDA_CAMPEAO)
        salvamento_apagar(SALVAMENTO_ARQUIVO);
    else if (partida_exportar(&mesa->partida, &salva))
        salvamento_gravar(&salva, SALVAMENTO_ARQUIVO);
}

/**
 * Retoma a partida do arquivo de salvamento.
 *
 * @return true se a partida voltou ao ponto em que foi guardada.
 */
static bool continuar_partida(Mesa *mesa)
{
    PartidaSalva salva;

    interface_mesa_limpar_eventos();
    mesa->espera_do_adversario = 0.0;
    mesa->aposta = 0;
    mesa->ativa = salvamento_ler(SALVAMENTO_ARQUIVO, &salva) &&
                  partida_restaurar(&mesa->partida, &salva);
    if (mesa->ativa)
        animacoes_ignorar_proximo_inicio();
    return mesa->ativa;
}

/**
 * Executa um quadro da tela inicial: lê a entrada, desenha o menu e
 * devolve o estado seguinte (continua no menu, retoma o jogo salvo, começa
 * um jogo novo, vai para as configurações ou sai).
 */
static EstadoJogo executar_quadro_menu(void)
{
    bool confirmou = interface_menu_atualizar(&opcao_do_menu, ha_jogo_salvo);

    interface_menu_desenhar(opcao_do_menu, ha_jogo_salvo, aviso_do_menu);

    if (!confirmou)
        return ESTADO_MENU;

    aviso_do_menu = NULL;

    switch (opcao_do_menu) {
        case MENU_CONTINUAR:
            if (continuar_partida(&mesa))
                return ESTADO_MESA;

            salvamento_apagar(SALVAMENTO_ARQUIVO);
            atualizar_menu();
            aviso_do_menu = "Não foi possível carregar o jogo salvo.";
            return ESTADO_MENU;

        case MENU_NOVO_JOGO:
            return ha_jogo_salvo ? ESTADO_CONFIRMAR_NOVO_JOGO : ESTADO_MESA;

        case MENU_CONFIGURACOES:
            selecao_da_resolucao = configuracoes.resolucao;
            return ESTADO_CONFIGURACOES;

        case MENU_SAIR:
        case MENU_TOTAL:
            break;
    }

    return ESTADO_SAIR;
}

/**
 * Executa um quadro da pergunta que aparece ao começar um jogo novo quando
 * já existe um salvo, porque o jogo novo o substitui.
 */
static EstadoJogo executar_quadro_confirmar_novo_jogo(void)
{
    RespostaConfirmacao resposta = interface_confirmacao_ler_resposta();

    interface_confirmacao_desenhar("COMEÇAR UM JOGO NOVO?",
                                   "O jogo salvo será apagado e não poderá ser retomado.");

    if (resposta == RESPOSTA_SIM)
        return ESTADO_MESA;

    return resposta == RESPOSTA_NAO ? ESTADO_MENU : ESTADO_CONFIRMAR_NOVO_JOGO;
}

/**
 * Executa um quadro da tela de configurações: move a seleção, aplica a
 * resolução escolhida se ela cabe no monitor e volta ao menu quando o
 * jogador pede.
 */
static EstadoJogo executar_quadro_configuracoes(void)
{
    EntradaConfiguracoes entrada = interface_configuracoes_ler_entrada();

    if (entrada.voltar)
        return ESTADO_MENU;

    if (entrada.apontada >= 0)
        selecao_da_resolucao = entrada.apontada;

    selecao_da_resolucao = configuracoes_mover_selecao(selecao_da_resolucao, entrada.movimento);

    if (entrada.confirmar &&
        interface_aplicar_resolucao(configuracoes_resolucao(selecao_da_resolucao)))
        configuracoes_escolher_resolucao(&configuracoes, selecao_da_resolucao);

    interface_configuracoes_desenhar(&configuracoes, selecao_da_resolucao);

    return ESTADO_CONFIGURACOES;
}

/**
 * Começa uma partida nova, com a semente tirada do relógio, e já a guarda,
 * no lugar de qualquer jogo salvo antes.
 *
 * @return true se a partida começou.
 */
static bool iniciar_partida(Mesa *mesa)
{
    interface_mesa_limpar_eventos();
    mesa->espera_do_adversario = 0.0;
    mesa->aposta = 0;
    mesa->ativa = partida_iniciar(&mesa->partida, (unsigned)time(NULL));

    if (mesa->ativa)
        salvar_progresso(mesa);

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
 * @return true se a partida avançou neste quadro, e então vale guardá-la.
 */
static bool atualizar_partida(Mesa *mesa, const EntradaMesa *entrada, double tempo)
{
    Partida *partida = &mesa->partida;
    bool avancou = false;

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            /* A trapaça não gasta a vez; se for descoberta, a partida passa para a roleta. */
            if (entrada->pedido.tipo != PEDIDO_NENHUM &&
                partida_trapacear(partida, entrada->pedido, NULL) == POKER_OK)
                avancou = true;

            if (partida->situacao != PARTIDA_VEZ_DO_JOGADOR)
                break;

            mesa->aposta = partida_ajustar_aposta(partida, mesa->aposta, entrada->ajuste_aposta);

            if (entrada->comando != COMANDO_NENHUM &&
                partida_comando_jogador(partida, entrada->comando, mesa->aposta) == POKER_OK) {
                mesa->aposta = 0;
                mesa->espera_do_adversario = 0.0;
                avancou = true;
            }
            break;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            mesa->espera_do_adversario += tempo;

            if (mesa->espera_do_adversario >= PAUSA_DO_ADVERSARIO) {
                avancou = partida_agir_adversario(partida, NULL) == POKER_OK;
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
            if (entrada->continuar) {
                avancou = partida_continuar(partida);
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_ROLETA:
            if (entrada->continuar) {
                avancou = partida_puxar_gatilho(partida);
                mesa->espera_do_adversario = 0.0;
            }
            break;

        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_JOGADOR_MORTO:
        case PARTIDA_CAMPEAO:
            if (entrada->continuar)
                iniciar_partida(mesa);
            break;

        case PARTIDA_VAZIA:
            break;
    }

    return avancou;
}

/**
 * Executa um quadro da mesa: lê a entrada, faz a partida andar, guarda o
 * progresso quando ela avança, entrega os eventos à interface e desenha.
 * Devolve o estado seguinte (continua na mesa ou volta ao menu quando o
 * jogador pede para voltar, já com o jogo guardado para continuar depois).
 */
static EstadoJogo executar_quadro_mesa(void)
{
    EntradaMesa entrada;

    if (!mesa.ativa && !iniciar_partida(&mesa))
        return ESTADO_MENU;

    repassar_eventos(&mesa.partida);
    entrada = interface_mesa_ler_entrada();

    if (entrada.voltar) {
        salvar_progresso(&mesa);
        encerrar_partida(&mesa);
        atualizar_menu();
        return ESTADO_MENU;
    }

    if (animacoes_embaralhando()) {
        animacoes_atualizar(interface_tempo_do_quadro());
        interface_mesa_desenhar(&mesa.partida, mesa.aposta);
        return ESTADO_MESA;
    }

    if (atualizar_partida(&mesa, &entrada, interface_tempo_do_quadro()))
        salvar_progresso(&mesa);

    repassar_eventos(&mesa.partida);
    interface_mesa_desenhar(&mesa.partida, mesa.aposta);

    return ESTADO_MESA;
}

void jogo_executar(void)
{
    EstadoJogo estado = ESTADO_MENU;

    configuracoes_iniciar(&configuracoes);
    atualizar_menu();
    interface_iniciar();

    while (estado != ESTADO_SAIR && !interface_fechar_solicitado()) {
        switch (estado) {
            case ESTADO_MENU:
                estado = executar_quadro_menu();
                break;

            case ESTADO_MESA:
                estado = executar_quadro_mesa();
                break;

            case ESTADO_CONFIGURACOES:
                estado = executar_quadro_configuracoes();
                break;

            case ESTADO_CONFIRMAR_NOVO_JOGO:
                estado = executar_quadro_confirmar_novo_jogo();
                break;

            case ESTADO_SAIR:
                break;
        }
    }

    if (mesa.ativa) {
        salvar_progresso(&mesa);
        encerrar_partida(&mesa);
    }

    interface_encerrar();
}
