#include "core/salvamento.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CABECALHO "DMBSAVE"
#define VERSAO 1
#define TAMANHO_DO_TOKEN 32
#define TAMANHO_DO_CAMINHO 512
#define LIMITE_DE_CONTADORES 1000000

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
        salva->quantidade_de_acoes > MAO_ACOES_MAXIMAS ||
        salva->cartas_no_board > MAO_CARTAS_NO_BOARD) {
        return 0;
    }

    ok = anexar(saida, capacidade, &usado, "%s %d\n", CABECALHO, VERSAO) &&
         anexar(saida, capacidade, &usado, "torneio %d %lld %lld %d %d %d\n",
                salva->torneio.confronto,
                (long long)salva->torneio.fichas_jogador,
                (long long)salva->torneio.fichas_adversario,
                salva->torneio.maos_jogadas,
                salva->torneio.dealer,
                salva->torneio.recuperacoes) &&
         anexar(saida, capacidade, &usado, "mao %u %d %d\n",
                salva->semente_da_mao,
                (int)salva->situacao,
                salva->adversario_embriagado ? 1 : 0) &&
         anexar(saida, capacidade, &usado, "sementes %llu\n",
                (unsigned long long)salva->estado_das_sementes) &&
         anexar(saida, capacidade, &usado, "ia %llu %llu\n",
                (unsigned long long)salva->rng_acao,
                (unsigned long long)salva->rng_equidade) &&
         anexar(saida, capacidade, &usado, "cartas");

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
         anexar(saida, capacidade, &usado, "acoes %d\n", (int)salva->quantidade_de_acoes);

    for (size_t i = 0; ok && i < salva->quantidade_de_acoes; i++) {
        ok = anexar(saida, capacidade, &usado, "%d %lld\n",
                    (int)salva->acoes[i].type, (long long)salva->acoes[i].total);
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

static bool ler_torneio(Leitor *leitor, Torneio *torneio)
{
    long long confronto, fichas_jogador, fichas_adversario, maos, dealer, recuperacoes;

    if (!esperar(leitor, "torneio") ||
        !ler_inteiro(leitor, 0, TORNEIO_CONFRONTOS - 1, &confronto) ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_jogador) ||
        !ler_inteiro(leitor, 0, POKER_MAX_STACK, &fichas_adversario) ||
        !ler_inteiro(leitor, 0, LIMITE_DE_CONTADORES, &maos) ||
        !ler_inteiro(leitor, TORNEIO_ASSENTO_JOGADOR, TORNEIO_ASSENTO_ADVERSARIO, &dealer) ||
        !ler_inteiro(leitor, 0, LIMITE_DE_CONTADORES, &recuperacoes)) {
        return false;
    }

    torneio->confronto = (int)confronto;
    torneio->fichas_jogador = (PokerChips)fichas_jogador;
    torneio->fichas_adversario = (PokerChips)fichas_adversario;
    torneio->maos_jogadas = (int)maos;
    torneio->dealer = (int)dealer;
    torneio->recuperacoes = (int)recuperacoes;
    torneio->situacao = TORNEIO_EM_ANDAMENTO;
    return true;
}

static bool ler_mao(Leitor *leitor, PartidaSalva *salva)
{
    long long semente, situacao, embriagado;

    if (!esperar(leitor, "mao") ||
        !ler_inteiro(leitor, 0, UINT_MAX, &semente) ||
        !ler_inteiro(leitor, PARTIDA_VEZ_DO_JOGADOR, PARTIDA_CONFRONTO_VENCIDO, &situacao) ||
        !ler_inteiro(leitor, 0, 1, &embriagado)) {
        return false;
    }

    salva->semente_da_mao = (unsigned)semente;
    salva->situacao = (PartidaSituacao)situacao;
    salva->adversario_embriagado = embriagado == 1;
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
        !ler_inteiro(leitor, 0, MAO_ACOES_MAXIMAS, &quantidade)) {
        return false;
    }

    salva->fichas[0] = (PokerChips)fichas_jogador;
    salva->fichas[1] = (PokerChips)fichas_adversario;
    salva->quantidade_de_acoes = (size_t)quantidade;

    for (size_t i = 0; i < salva->quantidade_de_acoes; i++) {
        long long tipo, total;

        if (!ler_inteiro(leitor, POKER_FOLD, POKER_ALL_IN, &tipo) ||
            !ler_inteiro(leitor, 0, POKER_MAX_STACK, &total)) {
            return false;
        }

        salva->acoes[i].type = (PokerActionType)tipo;
        salva->acoes[i].total = (PokerChips)total;
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
