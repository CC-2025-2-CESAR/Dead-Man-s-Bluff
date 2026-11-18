#include "core/salvamento.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CABECALHO "DMBSAVE"
#define VERSAO 4
#define TAMANHO_DO_TOKEN 32
#define TAMANHO_DO_CAMINHO 512
#define LIMITE_DE_CONTADORES 1000000

/**
 * Código de uma entrada do registro: 0 a 5 são as ações do motor, seguidas de
 * total; CODIGO_DO_GATILHO é o gatilho da roleta, seguido de 0;
 * CODIGO_DA_TRAPACA é uma trapaça, seguida do tipo, da carta da mão e da carta.
 */
#define CODIGO_DO_GATILHO ((int)POKER_ALL_IN + 1)
#define CODIGO_DA_TRAPACA ((int)POKER_ALL_IN + 2)

/** Posição de leitura dentro do texto de um salvamento. */
typedef struct {
    const char *atual;
    const char *fim;
} Leitor;

/**
 * Escreve um trecho formatado em saida, a partir de usado, e avança usado.
 *
 * @return false se o trecho não coube.
 */
static bool anexar(char *saida, size_t capacidade, size_t *usado, const char *formato, ...)
{
    va_list argumentos;
    int escritos;

    if (*usado >= capacidade) {
        return false;
    }

    va_start(argumentos, formato);
    escritos = vsnprintf(saida + *usado, capacidade - *usado, formato, argumentos);
    va_end(argumentos);

    if (escritos < 0 || (size_t)escritos >= capacidade - *usado) {
        return false;
    }

    *usado += (size_t)escritos;
    return true;
}

static bool anexar_carta(char *saida, size_t capacidade, size_t *usado, PokerCard carta)
{
    return anexar(saida, capacidade, usado, " %d %d", carta.rank, (int)carta.suit);
}

size_t salvamento_formatar(const PartidaSalva *salva, char *saida, size_t capacidade)
{
    size_t usado = 0;
    bool ok;

    if (salva == NULL || saida == NULL || capacidade == 0 ||
        salva->quantidade_no_registro > PARTIDA_REGISTRO_MAXIMO ||
        salva->cartas_no_board > MAO_CARTAS_NO_BOARD) {
        return 0;
    }

    ok = anexar(saida, capacidade, &usado, "%s %d\n", CABECALHO, VERSAO) &&
         anexar(saida, capacidade, &usado, "torneio %d %lld %lld %d %d %d %d\n",
                salva->torneio.confronto,
                (long long)salva->torneio.fichas_jogador,
                (long long)salva->torneio.fichas_adversario,
                salva->torneio.maos_jogadas,
                salva->torneio.dealer,
                salva->torneio.recuperacoes,
                salva->torneio.segunda_vida_usada ? 1 : 0) &&
         anexar(saida, capacidade, &usado, "mao %u %d\n",
                salva->semente_da_mao,
                (int)salva->situacao) &&
         anexar(saida, capacidade, &usado, "sementes %llu\n",
                (unsigned long long)salva->estado_das_sementes) &&
         anexar(saida, capacidade, &usado, "ia %llu %llu\n",
                (unsigned long long)salva->rng_acao,
                (unsigned long long)salva->rng_equidade) &&
         anexar(saida, capacidade, &usado, "risco %d", salva->risco.balas_roleta);

    for (int i = 0; ok && i < PROB_TRAPACA_TOTAL; i++) {
        ok = anexar(saida, capacidade, &usado, " %d", salva->risco.usos_trapaca[i]);
    }

    ok = ok &&
         anexar(saida, capacidade, &usado, " %llu\n",
                (unsigned long long)salva->estado_do_risco) &&
         anexar(saida, capacidade, &usado, "trapacas %d %d %d %d",
                salva->cheats.maos_embriagado,
                salva->cheats.usos_cumplice_blind,
                salva->cheats.suborno_usado_na_mao ? 1 : 0,
                salva->cheats.as_na_manga_usado ? 1 : 0) &&
         anexar_carta(saida, capacidade, &usado, salva->cheats.carta_na_manga) &&
         anexar(saida, capacidade, &usado, " %d %d",
                salva->cheats.carta_comprada_pendente ? 1 : 0,
                salva->cheats.destino_carta_comprada) &&
         anexar_carta(saida, capacidade, &usado, salva->cheats.carta_comprada);

    for (int i = 0; ok && i < 5; i++) {
        ok = anexar_carta(saida, capacidade, &usado, salva->cheats.cartas_marcadas[i]) &&
             anexar(saida, capacidade, &usado, " %d", salva->cheats.turnos_marcacao[i]);
    }

    ok = ok && anexar(saida, capacidade, &usado, "\ncartas");

    for (int assento = 0; ok && assento < 2; assento++) {
        for (int i = 0; ok && i < 2; i++) {
            ok = anexar_carta(saida, capacidade, &usado, salva->cartas[assento][i]);
        }
    }

    ok = ok && anexar(saida, capacidade, &usado, "\nboard %d", (int)salva->cartas_no_board);

    for (size_t i = 0; ok && i < salva->cartas_no_board; i++) {
        ok = anexar_carta(saida, capacidade, &usado, salva->board[i]);
    }

    ok = ok &&
         anexar(saida, capacidade, &usado, "\nfichas %lld %lld\n",
                (long long)salva->fichas[0], (long long)salva->fichas[1]) &&
         anexar(saida, capacidade, &usado, "acoes %d\n", (int)salva->quantidade_no_registro);

    for (size_t i = 0; ok && i < salva->quantidade_no_registro; i++) {
        const RegistroDaMao *entrada = &salva->registro[i];

        if (entrada->tipo == REGISTRO_GATILHO) {
            ok = anexar(saida, capacidade, &usado, "%d 0\n", CODIGO_DO_GATILHO);
        } else if (entrada->tipo == REGISTRO_TRAPACA) {
            ok = anexar(saida, capacidade, &usado, "%d %d %d %d %d\n", CODIGO_DA_TRAPACA,
                        (int)entrada->pedido.tipo, entrada->pedido.carta_da_mao,
                        entrada->pedido.carta.rank, (int)entrada->pedido.carta.suit);
        } else {
            ok = anexar(saida, capacidade, &usado, "%d %lld\n",
                        (int)entrada->acao.type, (long long)entrada->acao.total);
        }
    }

    ok = ok && anexar(saida, capacidade, &usado, "fim\n");

    if (!ok) {
        saida[0] = '\0';
        return 0;
    }

    return usado;
}

/** Copia a próxima palavra do texto, sem os espaços em volta. */
static bool proximo_token(Leitor *leitor, char *token, size_t capacidade)
{
    size_t tamanho = 0;

    while (leitor->atual < leitor->fim &&
           (*leitor->atual == ' ' || *leitor->atual == '\n' ||
            *leitor->atual == '\r' || *leitor->atual == '\t')) {
        leitor->atual++;
    }

    while (leitor->atual < leitor->fim &&
           *leitor->atual != ' ' && *leitor->atual != '\n' &&
           *leitor->atual != '\r' && *leitor->atual != '\t') {
        if (*leitor->atual == '\0' || tamanho + 1 >= capacidade) {
            return false;
        }

        token[tamanho++] = *leitor->atual++;
    }

    token[tamanho] = '\0';
    return tamanho > 0;
}

static bool esperar(Leitor *leitor, const char *palavra)
{
    char token[TAMANHO_DO_TOKEN];

    return proximo_token(leitor, token, sizeof token) && strcmp(token, palavra) == 0;
}

/** Lê um inteiro com sinal opcional, dentro de [minimo, maximo]. */
static bool ler_inteiro(Leitor *leitor, long long minimo, long long maximo, long long *saida)
{
    char token[TAMANHO_DO_TOKEN];
    char *fim;
    long long valor;

    if (!proximo_token(leitor, token, sizeof token) ||
        !((token[0] >= '0' && token[0] <= '9') || token[0] == '-')) {
        return false;
    }

    errno = 0;
    valor = strtoll(token, &fim, 10);

    if (errno != 0 || *fim != '\0' || valor < minimo || valor > maximo) {
        return false;
    }

    *saida = valor;
    return true;
}

/** Lê um inteiro de 64 bits sem sinal. */
static bool ler_sem_sinal(Leitor *leitor, uint64_t *saida)
{
    char token[TAMANHO_DO_TOKEN];
    char *fim;
    unsigned long long valor;

    if (!proximo_token(leitor, token, sizeof token) ||
        token[0] < '0' || token[0] > '9') {
        return false;
    }

    errno = 0;
    valor = strtoull(token, &fim, 10);

    if (errno != 0 || *fim != '\0') {
        return false;
    }

    *saida = (uint64_t)valor;
    return true;
}

static bool ler_carta(Leitor *leitor, PokerCard *carta)
{
    long long valor;
    long long naipe;

    if (!ler_inteiro(leitor, 2, 14, &valor) ||
        !ler_inteiro(leitor, POKER_CLUBS, POKER_SPADES, &naipe)) {
        return false;
    }

    carta->rank = (int)valor;
    carta->suit = (PokerSuit)naipe;
    return true;
}

/** Como ler_carta, mas aceita a carta vazia (valor 0), que o jogo usa para "nenhuma". */
static bool ler_carta_ou_vazia(Leitor *leitor, PokerCard *carta)
{
    long long valor;
    long long naipe;

    if (!ler_inteiro(leitor, 0, 14, &valor) || valor == 1 ||
        !ler_inteiro(leitor, POKER_CLUBS, POKER_SPADES, &naipe)) {
        return false;
    }

    carta->rank = (int)valor;
    carta->suit = (PokerSuit)naipe;
    return true;
}

static bool ler_torneio(Leitor *leitor, Torneio *torneio)
{
    long long confronto, fichas_jogador, fichas_adversario, maos, dealer, recuperacoes, segunda_vida;

    if (!esperar(leitor, "torneio") ||
        !ler_inteiro(leitor, 0, TORNEIO_CONFRONTOS - 1, &confronto) ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_jogador) ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_adversario) ||
        !ler_inteiro(leitor, 0, LIMITE_DE_CONTADORES, &maos) ||
        !ler_inteiro(leitor, TORNEIO_ASSENTO_JOGADOR, TORNEIO_ASSENTO_ADVERSARIO, &dealer) ||
        !ler_inteiro(leitor, 0, LIMITE_DE_CONTADORES, &recuperacoes) ||
        !ler_inteiro(leitor, 0, 1, &segunda_vida)) {
        return false;
    }

    torneio->confronto = (int)confronto;
    torneio->fichas_jogador = (PokerChips)fichas_jogador;
    torneio->fichas_adversario = (PokerChips)fichas_adversario;
    torneio->maos_jogadas = (int)maos;
    torneio->dealer = (int)dealer;
    torneio->recuperacoes = (int)recuperacoes;
    torneio->segunda_vida_usada = segunda_vida == 1;
    torneio->situacao = TORNEIO_EM_ANDAMENTO;
    return true;
}

/** Diz se uma situação é daquelas em que a partida pode ser guardada e continuada. */
static bool situacao_guardavel(long long situacao)
{
    return (situacao >= PARTIDA_VEZ_DO_JOGADOR && situacao <= PARTIDA_CONFRONTO_VENCIDO) ||
           situacao == PARTIDA_ROLETA;
}

static bool ler_mao(Leitor *leitor, PartidaSalva *salva)
{
    long long semente, situacao;

    if (!esperar(leitor, "mao") ||
        !ler_inteiro(leitor, 0, UINT_MAX, &semente) ||
        !ler_inteiro(leitor, PARTIDA_VEZ_DO_JOGADOR, PARTIDA_JOGADOR_MORTO, &situacao) ||
        !situacao_guardavel(situacao)) {
        return false;
    }

    salva->semente_da_mao = (unsigned)semente;
    salva->situacao = (PartidaSituacao)situacao;
    return true;
}

static bool ler_geradores(Leitor *leitor, PartidaSalva *salva)
{
    return esperar(leitor, "sementes") &&
           ler_sem_sinal(leitor, &salva->estado_das_sementes) &&
           esperar(leitor, "ia") &&
           ler_sem_sinal(leitor, &salva->rng_acao) &&
           ler_sem_sinal(leitor, &salva->rng_equidade) &&
           salva->rng_acao != 0 && salva->rng_equidade != 0;
}

/** Lê o risco no início da mão: balas, usos de cada trapaça e o gerador do risco. */
static bool ler_risco(Leitor *leitor, PartidaSalva *salva)
{
    long long balas;

    if (!esperar(leitor, "risco") ||
        !ler_inteiro(leitor, 1, 6, &balas)) {
        return false;
    }

    salva->risco.balas_roleta = (int)balas;

    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        long long usos;

        if (!ler_inteiro(leitor, 0, PARTIDA_USOS_MAXIMOS, &usos)) {
            return false;
        }

        salva->risco.usos_trapaca[i] = (int)usos;
    }

    return ler_sem_sinal(leitor, &salva->estado_do_risco);
}

/**
 * Lê o estado das trapaças no início da mão: bebedeira, usos do cúmplice, a
 * carta da manga, o suborno à espera e as cinco marcas, cada uma com a carta e
 * as mãos que ainda lhe restam (0 quando o espaço está vazio).
 */
static bool ler_trapacas(Leitor *leitor, PartidaSalva *salva)
{
    CheatState *cheats = &salva->cheats;
    long long embriagado, usos, suborno_usado, manga_usada, pendente, destino;

    if (!esperar(leitor, "trapacas") ||
        !ler_inteiro(leitor, 0, PARTIDA_USOS_MAXIMOS, &embriagado) ||
        !ler_inteiro(leitor, 0, PARTIDA_USOS_MAXIMOS, &usos) ||
        !ler_inteiro(leitor, 0, 1, &suborno_usado) ||
        !ler_inteiro(leitor, 0, 1, &manga_usada) ||
        !ler_carta(leitor, &cheats->carta_na_manga) ||
        !ler_inteiro(leitor, 0, 1, &pendente) ||
        !ler_inteiro(leitor, -1, 1, &destino) ||
        !ler_carta_ou_vazia(leitor, &cheats->carta_comprada)) {
        return false;
    }

    cheats->maos_embriagado = (int)embriagado;
    cheats->usos_cumplice_blind = (int)usos;
    cheats->suborno_usado_na_mao = suborno_usado == 1;
    cheats->as_na_manga_usado = manga_usada == 1;
    cheats->carta_comprada_pendente = pendente == 1;
    cheats->destino_carta_comprada = (int)destino;

    for (int i = 0; i < 5; i++) {
        long long turnos;

        if (!ler_carta_ou_vazia(leitor, &cheats->cartas_marcadas[i]) ||
            !ler_inteiro(leitor, 0, PARTIDA_MARCA_DURACAO, &turnos) ||
            (cheats->cartas_marcadas[i].rank == 0) != (turnos == 0)) {
            return false;
        }

        cheats->turnos_marcacao[i] = (int)turnos;
    }

    return true;
}

static bool ler_cartas(Leitor *leitor, PartidaSalva *salva)
{
    long long quantidade;

    if (!esperar(leitor, "cartas")) {
        return false;
    }

    for (int assento = 0; assento < 2; assento++) {
        for (int i = 0; i < 2; i++) {
            if (!ler_carta(leitor, &salva->cartas[assento][i])) {
                return false;
            }
        }
    }

    if (!esperar(leitor, "board") ||
        !ler_inteiro(leitor, 0, MAO_CARTAS_NO_BOARD, &quantidade)) {
        return false;
    }

    salva->cartas_no_board = (size_t)quantidade;

    for (size_t i = 0; i < salva->cartas_no_board; i++) {
        if (!ler_carta(leitor, &salva->board[i])) {
            return false;
        }
    }

    return true;
}

static bool ler_fichas_e_acoes(Leitor *leitor, PartidaSalva *salva)
{
    long long fichas_jogador, fichas_adversario, quantidade;

    if (!esperar(leitor, "fichas") ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_jogador) ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_adversario) ||
        !esperar(leitor, "acoes") ||
        !ler_inteiro(leitor, 0, PARTIDA_REGISTRO_MAXIMO, &quantidade)) {
        return false;
    }

    salva->fichas[0] = (PokerChips)fichas_jogador;
    salva->fichas[1] = (PokerChips)fichas_adversario;
    salva->quantidade_no_registro = (size_t)quantidade;

    for (size_t i = 0; i < salva->quantidade_no_registro; i++) {
        long long codigo, total, tipo, carta_da_mao;
        RegistroDaMao *entrada = &salva->registro[i];

        if (!ler_inteiro(leitor, POKER_FOLD, CODIGO_DA_TRAPACA, &codigo)) {
            return false;
        }

        if (codigo == CODIGO_DA_TRAPACA) {
            if (!ler_inteiro(leitor, PEDIDO_BEBIDA, PEDIDO_TOTAL - 1, &tipo) ||
                !ler_inteiro(leitor, 0, 1, &carta_da_mao) ||
                !ler_carta_ou_vazia(leitor, &entrada->pedido.carta)) {
                return false;
            }

            entrada->tipo = REGISTRO_TRAPACA;
            entrada->pedido.tipo = (TipoDePedido)tipo;
            entrada->pedido.carta_da_mao = (int)carta_da_mao;
            continue;
        }

        if (!ler_inteiro(leitor, 0, codigo == CODIGO_DO_GATILHO ? 0 : POKER_MAX_STACK, &total)) {
            return false;
        }

        if (codigo == CODIGO_DO_GATILHO) {
            entrada->tipo = REGISTRO_GATILHO;
        } else {
            entrada->tipo = REGISTRO_ACAO;
            entrada->acao.type = (PokerActionType)codigo;
            entrada->acao.total = (PokerChips)total;
        }
    }

    return true;
}

bool salvamento_interpretar(const char *texto, size_t tamanho, PartidaSalva *salva)
{
    Leitor leitor;
    PartidaSalva lido = {0};
    long long versao;
    char resto[TAMANHO_DO_TOKEN];

    if (texto == NULL || salva == NULL || tamanho > SALVAMENTO_TAMANHO_MAXIMO) {
        return false;
    }

    leitor.atual = texto;
    leitor.fim = texto + tamanho;

    if (!esperar(&leitor, CABECALHO) ||
        !ler_inteiro(&leitor, VERSAO, VERSAO, &versao) ||
        !ler_torneio(&leitor, &lido.torneio) ||
        !ler_mao(&leitor, &lido) ||
        !ler_geradores(&leitor, &lido) ||
        !ler_risco(&leitor, &lido) ||
        !ler_trapacas(&leitor, &lido) ||
        !ler_cartas(&leitor, &lido) ||
        !ler_fichas_e_acoes(&leitor, &lido) ||
        !esperar(&leitor, "fim") ||
        proximo_token(&leitor, resto, sizeof resto)) {
        return false;
    }

    *salva = lido;
    return true;
}

bool salvamento_gravar(const PartidaSalva *salva, const char *caminho)
{
    char texto[SALVAMENTO_TAMANHO_MAXIMO];
    char temporario[TAMANHO_DO_CAMINHO];
    size_t tamanho = salvamento_formatar(salva, texto, sizeof texto);
    FILE *arquivo;
    bool gravado;

    if (tamanho == 0 || caminho == NULL ||
        snprintf(temporario, sizeof temporario, "%s.tmp", caminho) >= (int)sizeof temporario) {
        return false;
    }

    arquivo = fopen(temporario, "wb");

    if (arquivo == NULL) {
        return false;
    }

    gravado = fwrite(texto, 1, tamanho, arquivo) == tamanho;
    gravado = fclose(arquivo) == 0 && gravado;

    if (!gravado) {
        remove(temporario);
        return false;
    }

    remove(caminho);

    if (rename(temporario, caminho) != 0) {
        remove(temporario);
        return false;
    }

    return true;
}

bool salvamento_ler(const char *caminho, PartidaSalva *salva)
{
    char texto[SALVAMENTO_TAMANHO_MAXIMO + 1];
    FILE *arquivo = caminho != NULL ? fopen(caminho, "rb") : NULL;
    size_t lidos;

    if (arquivo == NULL) {
        return false;
    }

    lidos = fread(texto, 1, sizeof texto, arquivo);
    fclose(arquivo);

    return lidos <= SALVAMENTO_TAMANHO_MAXIMO && salvamento_interpretar(texto, lidos, salva);
}

bool salvamento_existe(const char *caminho)
{
    char linha[TAMANHO_DO_TOKEN * 2];
    char resto[TAMANHO_DO_TOKEN];
    FILE *arquivo = caminho != NULL ? fopen(caminho, "rb") : NULL;
    Leitor leitor;
    long long versao;
    bool existe;

    if (arquivo == NULL) {
        return false;
    }

    existe = fgets(linha, sizeof linha, arquivo) != NULL;
    fclose(arquivo);

    if (!existe) {
        return false;
    }

    leitor.atual = linha;
    leitor.fim = linha + strlen(linha);

    return esperar(&leitor, CABECALHO) &&
           ler_inteiro(&leitor, VERSAO, VERSAO, &versao) &&
           !proximo_token(&leitor, resto, sizeof resto);
}

void salvamento_apagar(const char *caminho)
{
    char temporario[TAMANHO_DO_CAMINHO];

    if (caminho == NULL) {
        return;
    }

    remove(caminho);

    if (snprintf(temporario, sizeof temporario, "%s.tmp", caminho) < (int)sizeof temporario) {
        remove(temporario);
    }
}
