#include "core/salvamento.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 30
#define SEMENTES_DA_BUSCA 600
#define LIMITE_DE_PASSOS 200000
#define ARQUIVO_DE_TESTE "teste_salvamento.sav"

typedef PokerAction (*Politica)(const Partida *partida, unsigned *sorteio);

static Partida original;
static Partida refeita;

static const PokerAction DESISTIR = {POKER_FOLD, 0};
static const PokerAction PASSAR = {POKER_CHECK, 0};
static const PokerAction PAGAR = {POKER_CALL, 0};
static const PokerAction ALL_IN = {POKER_ALL_IN, 0};

static unsigned proximo_aleatorio(unsigned *semente)
{
    *semente = *semente * 1664525u + 1013904223u;
    return *semente >> 8;
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static PokerAction politica_aleatoria(const Partida *partida, unsigned *sorteio)
{
    PokerLegalActions legais;
    unsigned dado = proximo_aleatorio(sorteio) % 100;

    CHECK(partida_acoes_legais(partida, &legais) == POKER_OK);

    if (dado < 8) {
        return DESISTIR;
    }

    if (dado < 16 && legais.all_in) {
        return ALL_IN;
    }

    if (dado < 40 && (legais.bet || legais.raise)) {
        PokerChips faixa = legais.max_total - legais.min_total + 1;
        PokerChips total = legais.min_total +
            (PokerChips)(proximo_aleatorio(sorteio) % (unsigned)faixa);

        return (PokerAction){legais.bet ? POKER_BET : POKER_RAISE, total};
    }

    return legais.check ? PASSAR : PAGAR;
}

static PokerAction politica_all_in(const Partida *partida, unsigned *sorteio)
{
    PokerLegalActions legais;

    (void)sorteio;
    CHECK(partida_acoes_legais(partida, &legais) == POKER_OK);

    if (legais.all_in) {
        return ALL_IN;
    }

    return legais.check ? PASSAR : PAGAR;
}

static bool partida_terminou(const Partida *partida)
{
    return partida->situacao == PARTIDA_JOGADOR_FALIDO ||
           partida->situacao == PARTIDA_JOGADOR_MORTO ||
           partida->situacao == PARTIDA_CAMPEAO;
}

static void drenar_eventos(Partida *partida)
{
    EventoJogo evento;

    while (fila_eventos_retirar(&partida->eventos, &evento)) {
    }
}

/**
 * Faz a partida dar um passo: ação do jogador, ação da IA ou a mão seguinte.
 *
 * @return false se a partida terminou e não há mais o que fazer.
 */
static bool dar_um_passo(Partida *partida, Politica politica, unsigned *sorteio)
{
    drenar_eventos(partida);

    switch (partida->situacao) {
        case PARTIDA_VEZ_DO_JOGADOR:
            CHECK(partida_agir_jogador(partida, politica(partida, sorteio)) == POKER_OK);
            return true;

        case PARTIDA_VEZ_DO_ADVERSARIO:
            CHECK(partida_agir_adversario(partida, NULL) == POKER_OK);
            return true;

        case PARTIDA_MAO_ENCERRADA:
        case PARTIDA_CONFRONTO_VENCIDO:
            CHECK(partida_continuar(partida));
            return true;

        case PARTIDA_ROLETA:
            CHECK(partida_puxar_gatilho(partida));
            return true;

        case PARTIDA_JOGADOR_FALIDO:
        case PARTIDA_JOGADOR_MORTO:
        case PARTIDA_CAMPEAO:
            return false;

        case PARTIDA_VAZIA:
            break;
    }

    CHECK(false);
    return false;
}

static void conferir_torneios_iguais(const Torneio *a, const Torneio *b)
{
    CHECK(a->confronto == b->confronto);
    CHECK(a->fichas_jogador == b->fichas_jogador);
    CHECK(a->fichas_adversario == b->fichas_adversario);
    CHECK(a->maos_jogadas == b->maos_jogadas);
    CHECK(a->dealer == b->dealer);
    CHECK(a->recuperacoes == b->recuperacoes);
    CHECK(a->situacao == b->situacao);
}

static void conferir_riscos_iguais(const SistemaRisco *a, const SistemaRisco *b)
{
    CHECK(a->balas_roleta == b->balas_roleta);

    for (int i = 0; i < PROB_TRAPACA_TOTAL; i++) {
        CHECK(a->usos_trapaca[i] == b->usos_trapaca[i]);
    }
}

static void conferir_registros_iguais(const RegistroDaMao *a, const RegistroDaMao *b)
{
    CHECK(a->tipo == b->tipo);

    if (a->tipo == REGISTRO_ACAO) {
        CHECK(a->acao.type == b->acao.type);
        CHECK(a->acao.total == b->acao.total);
    }
}

static void conferir_partidas_iguais(const Partida *a, const Partida *b)
{
    const Mao *ma = &a->mao;
    const Mao *mb = &b->mao;

    CHECK(a->situacao == b->situacao);
    conferir_torneios_iguais(&a->torneio, &b->torneio);
    conferir_torneios_iguais(&a->torneio_da_mao, &b->torneio_da_mao);
    CHECK(a->semente_da_mao == b->semente_da_mao);
    CHECK(a->estado_das_sementes == b->estado_das_sementes);
    CHECK(a->rng_acao.state == b->rng_acao.state);
    CHECK(a->rng_equidade.state == b->rng_equidade.state);
    CHECK(a->adversario_embriagado == b->adversario_embriagado);
    conferir_riscos_iguais(&a->risco, &b->risco);
    CHECK(a->estado_do_risco == b->estado_do_risco);
    conferir_riscos_iguais(&a->risco_da_mao, &b->risco_da_mao);
    CHECK(a->estado_do_risco_da_mao == b->estado_do_risco_da_mao);
    CHECK(a->roleta_motivo == b->roleta_motivo);
    CHECK(a->quantidade_no_registro == b->quantidade_no_registro);
    CHECK(a->registro_perdido == b->registro_perdido);

    for (size_t i = 0; i < a->quantidade_no_registro; i++) {
        conferir_registros_iguais(&a->registro[i], &b->registro[i]);
    }

    CHECK(a->ia.style == b->ia.style);
    CHECK(a->ia.action_rng == &a->rng_acao && b->ia.action_rng == &b->rng_acao);
    CHECK(a->ia.equity_rng == &a->rng_equidade && b->ia.equity_rng == &b->rng_equidade);

    CHECK(ma->situacao == mb->situacao);
    CHECK(ma->cartas_no_board == mb->cartas_no_board);
    CHECK(ma->baralho.count == mb->baralho.count);
    CHECK(ma->quantidade_de_acoes == mb->quantidade_de_acoes);
    CHECK(ma->acoes_perdidas == mb->acoes_perdidas);

    for (size_t i = 0; i < ma->quantidade_de_acoes; i++) {
        CHECK(ma->acoes[i].type == mb->acoes[i].type);
        CHECK(ma->acoes[i].total == mb->acoes[i].total);
    }

    for (int assento = 0; assento < 2; assento++) {
        const PokerPlayer *ja = &ma->mesa.player[assento];
        const PokerPlayer *jb = &mb->mesa.player[assento];

        CHECK(ja->chips == jb->chips);
        CHECK(ja->committed == jb->committed);
        CHECK(ja->street_bet == jb->street_bet);
        CHECK(ja->folded == jb->folded);
        CHECK(ja->acted == jb->acted);

        for (int i = 0; i < 2; i++) {
            CHECK(mesma_carta(ma->cartas[assento][i], mb->cartas[assento][i]));
        }
    }

    for (size_t i = 0; i < ma->cartas_no_board; i++) {
        CHECK(mesma_carta(ma->board[i], mb->board[i]));
    }

    CHECK(ma->mesa.dealer == mb->mesa.dealer);
    CHECK(ma->mesa.actor == mb->mesa.actor);
    CHECK(ma->mesa.street == mb->mesa.street);
    CHECK(ma->mesa.phase == mb->mesa.phase);
    CHECK(ma->mesa.big_blind == mb->mesa.big_blind);
    CHECK(ma->mesa.current_bet == mb->mesa.current_bet);
    CHECK(ma->mesa.min_raise == mb->mesa.min_raise);
    CHECK(ma->mesa.initial_total == mb->mesa.initial_total);
}

static void conferir_salvas_iguais(const PartidaSalva *a, const PartidaSalva *b)
{
    conferir_torneios_iguais(&a->torneio, &b->torneio);
    CHECK(a->semente_da_mao == b->semente_da_mao);
    CHECK(a->estado_das_sementes == b->estado_das_sementes);
    CHECK(a->rng_acao == b->rng_acao);
    CHECK(a->rng_equidade == b->rng_equidade);
    CHECK(a->adversario_embriagado == b->adversario_embriagado);
    conferir_riscos_iguais(&a->risco, &b->risco);
    CHECK(a->estado_do_risco == b->estado_do_risco);
    CHECK(a->situacao == b->situacao);
    CHECK(a->cartas_no_board == b->cartas_no_board);
    CHECK(a->fichas[0] == b->fichas[0] && a->fichas[1] == b->fichas[1]);
    CHECK(a->quantidade_no_registro == b->quantidade_no_registro);

    for (int assento = 0; assento < 2; assento++) {
        for (int i = 0; i < 2; i++) {
            CHECK(mesma_carta(a->cartas[assento][i], b->cartas[assento][i]));
        }
    }

    for (size_t i = 0; i < a->cartas_no_board; i++) {
        CHECK(mesma_carta(a->board[i], b->board[i]));
    }

    for (size_t i = 0; i < a->quantidade_no_registro; i++) {
        conferir_registros_iguais(&a->registro[i], &b->registro[i]);
    }
}

/** Guarda a partida, passa os dados por texto e a refaz no mesmo lugar. */
static void restaurar_por_texto(Partida *partida)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    PartidaSalva lida;
    size_t tamanho;

    CHECK(partida_exportar(partida, &guardada));
    tamanho = salvamento_formatar(&guardada, texto, sizeof texto);
    CHECK(tamanho > 0);
    CHECK(salvamento_interpretar(texto, tamanho, &lida));
    CHECK(partida_restaurar(partida, &lida));
}

/**
 * Guarda a partida, passa os dados por texto e refaz outra partida com eles,
 * conferindo que as duas ficam iguais.
 */
static void conferir_ida_e_volta(const Partida *origem, Partida *destino)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    PartidaSalva lida;
    size_t tamanho;

    CHECK(partida_exportar(origem, &guardada));
    tamanho = salvamento_formatar(&guardada, texto, sizeof texto);
    CHECK(tamanho > 0 && tamanho < SALVAMENTO_TAMANHO_MAXIMO);
    CHECK(salvamento_interpretar(texto, tamanho, &lida));
    conferir_salvas_iguais(&guardada, &lida);
    CHECK(partida_restaurar(destino, &lida));
    conferir_partidas_iguais(origem, destino);
}

/**
 * Faz as duas partidas andarem juntas, com as mesmas decisões, até o fim da
 * partida ou do limite, conferindo que continuam iguais a cada passo.
 *
 * @return quantos passos deram.
 */
static int correr_juntas(Partida *a, Partida *b, Politica politica, unsigned *sorteio)
{
    int passos = 0;

    while (passos < LIMITE_DE_PASSOS) {
        conferir_partidas_iguais(a, b);
        drenar_eventos(b);

        switch (a->situacao) {
            case PARTIDA_VEZ_DO_JOGADOR:
            {
                PokerAction acao;

                drenar_eventos(a);
                acao = politica(a, sorteio);
                CHECK(partida_agir_jogador(a, acao) == POKER_OK);
                CHECK(partida_agir_jogador(b, acao) == POKER_OK);
                break;
            }

            case PARTIDA_VEZ_DO_ADVERSARIO:
                drenar_eventos(a);
                CHECK(partida_agir_adversario(a, NULL) == POKER_OK);
                CHECK(partida_agir_adversario(b, NULL) == POKER_OK);
                break;

            case PARTIDA_MAO_ENCERRADA:
            case PARTIDA_CONFRONTO_VENCIDO:
                drenar_eventos(a);
                CHECK(partida_continuar(a));
                CHECK(partida_continuar(b));
                break;

            case PARTIDA_ROLETA:
                drenar_eventos(a);
                CHECK(partida_puxar_gatilho(a));
                CHECK(partida_puxar_gatilho(b));
                break;

            case PARTIDA_JOGADOR_FALIDO:
            case PARTIDA_JOGADOR_MORTO:
            case PARTIDA_CAMPEAO:
                CHECK(!partida_exportar(a, &(PartidaSalva){0}));
                CHECK(!partida_exportar(b, &(PartidaSalva){0}));
                return passos;

            case PARTIDA_VAZIA:
                CHECK(false);
        }

        passos++;
    }

    CHECK(false);
    return passos;
}

/** Anda com a partida até ela chegar à situação pedida; false se acabar antes. */
static bool andar_ate(Partida *partida, PartidaSituacao alvo, Politica politica, unsigned *sorteio)
{
    for (int passos = 0; passos < LIMITE_DE_PASSOS; passos++) {
        if (partida->situacao == alvo) {
            return true;
        }

        if (!dar_um_passo(partida, politica, sorteio)) {
            return false;
        }
    }

    return false;
}

static void test_formato_de_texto_vai_e_volta(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    static char outro[SALVAMENTO_TAMANHO_MAXIMO];

    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        unsigned sorteio = semente * 31u + 7u;
        int passos = (int)(proximo_aleatorio(&sorteio) % 120);
        PartidaSalva guardada;
        PartidaSalva lida;
        size_t tamanho;

        CHECK(partida_iniciar(&original, semente));

        for (int i = 0; i < passos && !partida_terminou(&original); i++) {
            CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
        }

        if (partida_terminou(&original)) {
            partida_encerrar(&original);
            continue;
        }

        CHECK(partida_exportar(&original, &guardada));
        tamanho = salvamento_formatar(&guardada, texto, sizeof texto);
        CHECK(tamanho > 0);
        CHECK(strlen(texto) == tamanho);
        CHECK(strncmp(texto, "DMBSAVE 2\n", 10) == 0);
        CHECK(strcmp(texto + tamanho - 4, "fim\n") == 0);

        CHECK(salvamento_interpretar(texto, tamanho, &lida));
        conferir_salvas_iguais(&guardada, &lida);
        CHECK(salvamento_formatar(&lida, outro, sizeof outro) == tamanho);
        CHECK(strcmp(texto, outro) == 0);

        partida_encerrar(&original);
    }
}

static void test_refazer_a_partida_a_reproduz_e_ela_continua_igual(void)
{
    int pontos_salvos = 0;

    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        unsigned sorteio = semente * 7919u + 13u;
        int passos = 1 + (int)(proximo_aleatorio(&sorteio) % 60);

        CHECK(partida_iniciar(&original, semente));
        original.adversario_embriagado = semente % 2 == 0;

        for (int i = 0; i < passos && !partida_terminou(&original); i++) {
            CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
        }

        if (partida_terminou(&original)) {
            CHECK(!partida_exportar(&original, &(PartidaSalva){0}));
            partida_encerrar(&original);
            continue;
        }

        conferir_ida_e_volta(&original, &refeita);
        CHECK(refeita.adversario_embriagado == (semente % 2 == 0));
        correr_juntas(&original, &refeita, politica_aleatoria, &sorteio);
        CHECK(partida_terminou(&original) && partida_terminou(&refeita));
        pontos_salvos++;

        partida_encerrar(&original);
        partida_encerrar(&refeita);
    }

    CHECK(pontos_salvos > SEMENTES / 2);
}

static void test_refazer_em_cada_situacao_continuavel(void)
{
    static const PartidaSituacao ALVOS[] = {
        PARTIDA_VEZ_DO_JOGADOR,
        PARTIDA_VEZ_DO_ADVERSARIO,
        PARTIDA_MAO_ENCERRADA,
        PARTIDA_CONFRONTO_VENCIDO,
        PARTIDA_ROLETA
    };

    for (size_t i = 0; i < sizeof ALVOS / sizeof ALVOS[0]; i++) {
        bool achou = false;

        for (unsigned semente = 1; semente <= SEMENTES_DA_BUSCA && !achou; semente++) {
            unsigned sorteio = semente * 104729u + 3u;

            CHECK(partida_iniciar(&original, semente));

            if (ALVOS[i] == PARTIDA_CONFRONTO_VENCIDO || ALVOS[i] == PARTIDA_ROLETA) {
                achou = andar_ate(&original, ALVOS[i], politica_all_in, &sorteio);
            } else {
                for (int passos = (int)(semente % 40); passos > 0 && !partida_terminou(&original); passos--) {
                    CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
                }

                achou = andar_ate(&original, ALVOS[i], politica_aleatoria, &sorteio);
            }

            if (achou) {
                conferir_ida_e_volta(&original, &refeita);
                correr_juntas(&original, &refeita, politica_aleatoria, &sorteio);
                CHECK(partida_terminou(&original) && partida_terminou(&refeita));
                partida_encerrar(&refeita);
            }

            partida_encerrar(&original);
        }

        CHECK(achou);
    }
}

static void test_salvar_de_novo_depois_de_refazer(void)
{
    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        unsigned sorteio = semente * 6151u + 1u;
        int passos = 0;

        CHECK(partida_iniciar(&original, semente));
        CHECK(partida_iniciar(&refeita, semente));
        conferir_partidas_iguais(&original, &refeita);

        while (!partida_terminou(&original) && passos < LIMITE_DE_PASSOS) {
            unsigned mesmo_sorteio = sorteio;

            CHECK(dar_um_passo(&refeita, politica_aleatoria, &mesmo_sorteio));
            CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
            conferir_partidas_iguais(&original, &refeita);

            if (++passos % 17 == 0 && !partida_terminou(&refeita)) {
                restaurar_por_texto(&refeita);
                conferir_partidas_iguais(&original, &refeita);
            }
        }

        CHECK(partida_terminou(&original) && partida_terminou(&refeita));
        conferir_partidas_iguais(&original, &refeita);
        partida_encerrar(&original);
        partida_encerrar(&refeita);
    }
}

static void test_a_mao_guarda_o_torneio_e_a_semente(void)
{
    Torneio antes;
    unsigned primeira;

    CHECK(partida_iniciar(&original, 11));
    conferir_torneios_iguais(&original.torneio_da_mao, &original.torneio);
    primeira = original.semente_da_mao;

    while (original.situacao == PARTIDA_VEZ_DO_JOGADOR ||
           original.situacao == PARTIDA_VEZ_DO_ADVERSARIO) {
        if (original.situacao == PARTIDA_VEZ_DO_JOGADOR) {
            CHECK(partida_agir_jogador(&original, DESISTIR) == POKER_OK);
        } else {
            CHECK(partida_agir_adversario(&original, NULL) == POKER_OK);
        }
    }

    CHECK(original.situacao == PARTIDA_MAO_ENCERRADA);
    CHECK(original.semente_da_mao == primeira);
    CHECK(original.torneio_da_mao.maos_jogadas + 1 == original.torneio.maos_jogadas);

    antes = original.torneio;
    CHECK(partida_continuar(&original));
    conferir_torneios_iguais(&original.torneio_da_mao, &antes);
    CHECK(original.semente_da_mao != primeira);
    partida_encerrar(&original);
}

static void test_exportar_recusa_o_que_nao_pode_continuar(void)
{
    PartidaSalva salva = {0};
    Partida vazia = {0};

    salva.quantidade_no_registro = 777;
    CHECK(!partida_exportar(&vazia, &salva));
    CHECK(!partida_exportar(NULL, &salva));
    CHECK(partida_iniciar(&original, 5));
    CHECK(!partida_exportar(&original, NULL));
    CHECK(salva.quantidade_no_registro == 777);

    original.situacao = PARTIDA_JOGADOR_FALIDO;
    CHECK(!partida_exportar(&original, &salva));
    original.situacao = PARTIDA_JOGADOR_MORTO;
    CHECK(!partida_exportar(&original, &salva));
    original.situacao = PARTIDA_CAMPEAO;
    CHECK(!partida_exportar(&original, &salva));
    original.situacao = PARTIDA_VAZIA;
    CHECK(!partida_exportar(&original, &salva));
    CHECK(salva.quantidade_no_registro == 777);

    CHECK(partida_iniciar(&original, 5));
    original.registro_perdido = true;
    CHECK(!partida_exportar(&original, &salva));
    CHECK(salva.quantidade_no_registro == 777);
    original.registro_perdido = false;
    CHECK(partida_exportar(&original, &salva));
    partida_encerrar(&original);
}

/**
 * Leva a partida original a um ponto com o board no flop ou além e várias
 * ações já feitas, que dá bons dados para adulterar.
 */
static void preparar_ponto_com_board(void)
{
    for (unsigned semente = 1; semente < SEMENTES_DA_BUSCA; semente++) {
        unsigned sorteio = semente;

        CHECK(partida_iniciar(&original, semente));

        for (int passos = 0; passos < 80 && !partida_terminou(&original); passos++) {
            if (original.situacao == PARTIDA_VEZ_DO_JOGADOR &&
                original.mao.cartas_no_board >= 3 &&
                original.mao.quantidade_de_acoes >= 4) {
                return;
            }

            CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
        }

        partida_encerrar(&original);
    }

    CHECK(false);
}

/** Tenta refazer a partida com os dados adulterados e confere a recusa. */
static void conferir_recusa(const PartidaSalva *adulterada)
{
    CHECK(partida_iniciar(&refeita, 3));
    CHECK(refeita.situacao != PARTIDA_VAZIA);
    CHECK(!partida_restaurar(&refeita, adulterada));
    CHECK(refeita.situacao == PARTIDA_VAZIA);
    CHECK(refeita.mao.baralho.head == NULL && refeita.mao.baralho.count == 0);
}

static void test_refazer_recusa_dados_adulterados(void)
{
    PartidaSalva boa;
    PartidaSalva ruim;

    preparar_ponto_com_board();
    CHECK(partida_exportar(&original, &boa));
    CHECK(partida_restaurar(&refeita, &boa));
    conferir_partidas_iguais(&original, &refeita);
    partida_encerrar(&refeita);

    ruim = boa;
    ruim.cartas[0][0].rank = ruim.cartas[0][0].rank == 14 ? 2 : ruim.cartas[0][0].rank + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.cartas[1][1].suit = (PokerSuit)(((int)ruim.cartas[1][1].suit + 1) % 4);
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.board[0].rank = ruim.board[0].rank == 14 ? 2 : ruim.board[0].rank + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.cartas_no_board--;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.fichas[0]++;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.fichas[1]--;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_VEZ_DO_ADVERSARIO;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_MAO_ENCERRADA;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.semente_da_mao++;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.quantidade_no_registro--;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.registro[ruim.quantidade_no_registro] = ruim.registro[ruim.quantidade_no_registro - 1];
    ruim.quantidade_no_registro++;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.registro[0].acao.type =
        (PokerActionType)((int)ruim.registro[0].acao.type == POKER_FOLD ? POKER_CHECK : POKER_FOLD);
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.registro[0].tipo = REGISTRO_GATILHO;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.quantidade_no_registro = PARTIDA_REGISTRO_MAXIMO + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.risco.balas_roleta = 0;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.risco.balas_roleta = 7;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.risco.usos_trapaca[2] = -1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.risco.usos_trapaca[PROB_TRAPACA_TOTAL - 1] = PARTIDA_USOS_MAXIMOS + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_JOGADOR_MORTO;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_ROLETA;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.cartas_no_board = MAO_CARTAS_NO_BOARD + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.rng_acao = 0;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.rng_equidade = 0;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.situacao = TORNEIO_CONFRONTO_VENCIDO;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.confronto = TORNEIO_CONFRONTOS;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.confronto = -1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.dealer = 2;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.fichas_jogador = -1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.fichas_adversario = POKER_MAX_STACK + 1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.maos_jogadas = -1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.torneio.recuperacoes = -1;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_JOGADOR_FALIDO;
    conferir_recusa(&ruim);

    ruim = boa;
    ruim.situacao = PARTIDA_VAZIA;
    conferir_recusa(&ruim);

    CHECK(!partida_restaurar(NULL, &boa));
    CHECK(!partida_restaurar(&refeita, NULL));
    CHECK(refeita.situacao == PARTIDA_VAZIA);

    CHECK(partida_restaurar(&refeita, &boa));
    conferir_partidas_iguais(&original, &refeita);

    partida_encerrar(&refeita);
    partida_encerrar(&original);
}

static void test_o_estado_das_sementes_so_vale_para_as_proximas_maos(void)
{
    PartidaSalva boa;
    PartidaSalva outra;
    unsigned sorteio = 3;

    CHECK(partida_iniciar(&original, 8));
    CHECK(andar_ate(&original, PARTIDA_MAO_ENCERRADA, politica_aleatoria, &sorteio));
    CHECK(partida_exportar(&original, &boa));

    outra = boa;
    outra.estado_das_sementes ^= 0x1234u;

    CHECK(partida_restaurar(&refeita, &boa));
    CHECK(partida_restaurar(&original, &outra));
    CHECK(partida_continuar(&refeita));
    CHECK(partida_continuar(&original));
    CHECK(refeita.semente_da_mao != original.semente_da_mao);

    partida_encerrar(&refeita);
    partida_encerrar(&original);
}

/** Monta um texto de salvamento de uma partida real, para os testes de texto. */
static size_t texto_de_exemplo(char *texto, size_t capacidade, PartidaSalva *guardada)
{
    preparar_ponto_com_board();
    CHECK(partida_exportar(&original, guardada));
    partida_encerrar(&original);
    return salvamento_formatar(guardada, texto, capacidade);
}

static void test_texto_cortado_ou_acrescido_e_recusado(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO + 16];
    PartidaSalva guardada;
    PartidaSalva lida;
    size_t tamanho = texto_de_exemplo(texto, SALVAMENTO_TAMANHO_MAXIMO, &guardada);

    CHECK(tamanho > 50);
    CHECK(salvamento_interpretar(texto, tamanho, &lida));
    CHECK(salvamento_interpretar(texto, tamanho - 1, &lida));

    for (size_t corte = 0; corte + 1 < tamanho; corte++) {
        CHECK(!salvamento_interpretar(texto, corte, &lida));
    }

    memcpy(texto + tamanho, "x\n", 3);
    CHECK(!salvamento_interpretar(texto, tamanho + 2, &lida));
    memcpy(texto + tamanho, "fim\n", 5);
    CHECK(!salvamento_interpretar(texto, tamanho + 4, &lida));
    texto[tamanho] = '\0';

    CHECK(!salvamento_interpretar(NULL, tamanho, &lida));
    CHECK(!salvamento_interpretar(texto, tamanho, NULL));
    CHECK(!salvamento_interpretar(texto, SALVAMENTO_TAMANHO_MAXIMO + 1, &lida));
}

static void test_cabecalho_e_palavras_erradas_sao_recusados(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    static char alterado[SALVAMENTO_TAMANHO_MAXIMO];
    static const char *const TROCAS[][2] = {
        {"DMBSAVE 2", "DMBSAVE 1"},
        {"DMBSAVE 2", "DMBSAVE 3"},
        {"DMBSAVE 2", "dmbsave 2"},
        {"DMBSAVE 2", "DMBSAVE 0"},
        {"DMBSAVE 2", "DMBSAVE"},
        {"torneio", "torneo"},
        {"mao ", "mao x"},
        {"sementes", "semente"},
        {"ia ", "iaa "},
        {"risco", "riscos"},
        {"cartas", "carta"},
        {"board", "borda"},
        {"fichas", "fichs"},
        {"acoes", "acao"},
        {"fim", "fin"}
    };
    PartidaSalva guardada;
    PartidaSalva lida;
    size_t tamanho = texto_de_exemplo(texto, sizeof texto, &guardada);

    CHECK(tamanho > 0);

    for (size_t i = 0; i < sizeof TROCAS / sizeof TROCAS[0]; i++) {
        const char *onde = strstr(texto, TROCAS[i][0]);
        size_t antes;
        size_t depois;

        CHECK(onde != NULL);
        antes = (size_t)(onde - texto);
        depois = strlen(TROCAS[i][0]);
        CHECK(antes + strlen(TROCAS[i][1]) + (tamanho - antes - depois) < sizeof alterado);

        memcpy(alterado, texto, antes);
        memcpy(alterado + antes, TROCAS[i][1], strlen(TROCAS[i][1]));
        memcpy(alterado + antes + strlen(TROCAS[i][1]), onde + depois, tamanho - antes - depois);

        CHECK(!salvamento_interpretar(alterado, antes + strlen(TROCAS[i][1]) + tamanho - antes - depois, &lida));
    }
}

static void test_valores_fora_dos_limites_sao_recusados(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva boa;
    PartidaSalva ruim;
    PartidaSalva lida;

    CHECK(texto_de_exemplo(texto, sizeof texto, &boa) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.confronto = TORNEIO_CONFRONTOS;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.confronto = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.fichas_jogador = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.fichas_adversario = POKER_MAX_STACK;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));
    ruim.torneio.fichas_adversario = POKER_MAX_STACK + 1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.maos_jogadas = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.dealer = 2;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.torneio.recuperacoes = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_VAZIA;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_JOGADOR_FALIDO;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_CONFRONTO_VENCIDO;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.rng_acao = 0;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.rng_equidade = 0;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.estado_das_sementes = UINT64_MAX;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));
    CHECK(lida.estado_das_sementes == UINT64_MAX);

    ruim = boa;
    ruim.cartas[0][0].rank = 1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.cartas[1][0].rank = 15;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.cartas[1][1].suit = (PokerSuit)4;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.board[0].rank = 0;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.fichas[0] = -5;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.registro[0].acao.type = (PokerActionType)7;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.registro[0].acao.total = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.registro[0].tipo = REGISTRO_GATILHO;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));
    CHECK(lida.registro[0].tipo == REGISTRO_GATILHO);

    ruim = boa;
    ruim.quantidade_no_registro = PARTIDA_REGISTRO_MAXIMO + 1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) == 0);

    ruim = boa;
    ruim.risco.balas_roleta = 0;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.risco.balas_roleta = 7;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.risco.balas_roleta = 6;
    ruim.risco.usos_trapaca[PROB_TRAPACA_TOTAL - 1] = PARTIDA_USOS_MAXIMOS;
    ruim.estado_do_risco = UINT64_MAX;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));
    CHECK(lida.risco.balas_roleta == 6);
    CHECK(lida.risco.usos_trapaca[PROB_TRAPACA_TOTAL - 1] == PARTIDA_USOS_MAXIMOS);
    CHECK(lida.estado_do_risco == UINT64_MAX);

    ruim = boa;
    ruim.risco.usos_trapaca[0] = -1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.risco.usos_trapaca[1] = PARTIDA_USOS_MAXIMOS + 1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_JOGADOR_MORTO;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_CAMPEAO;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(!salvamento_interpretar(texto, strlen(texto), &lida));

    ruim = boa;
    ruim.situacao = PARTIDA_ROLETA;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) > 0);
    CHECK(salvamento_interpretar(texto, strlen(texto), &lida));
    CHECK(lida.situacao == PARTIDA_ROLETA);

    ruim = boa;
    ruim.cartas_no_board = MAO_CARTAS_NO_BOARD + 1;
    CHECK(salvamento_formatar(&ruim, texto, sizeof texto) == 0);
}

static void test_numeros_malformados_sao_recusados(void)
{
    static const char *const BASE =
        "DMBSAVE 2\n"
        "torneio 0 1990 2010 3 1 0\n"
        "mao 1234 1 0\n"
        "sementes 99\n"
        "ia 5 6\n"
        "risco 2 0 1 0 3 0 77\n"
        "cartas 14 3 7 0 2 1 9 2\n"
        "board 0\n"
        "fichas 1990 2010\n"
        "acoes 1\n"
        "2 0\n"
        "fim\n";
    static const char *const RUINS[][2] = {
        {"1990 2010", "1990 +2010"},
        {"1990 2010", "1990 20x10"},
        {"1990 2010", "1990 2010.5"},
        {"1990 2010", "1990 99999999999999999999"},
        {"1990 2010", "1990 0x7D2"},
        {"mao 1234", "mao -1234"},
        {"mao 1234", "mao 4294967296"},
        {"sementes 99", "sementes -99"},
        {"sementes 99", "sementes 99999999999999999999999"},
        {"ia 5 6", "ia 5"},
        {"ia 5 6", "ia 5 -6"},
        {"cartas 14 3", "cartas 14"},
        {"board 0", "board -1"},
        {"board 0", "board 6"},
        {"acoes 1", "acoes 2"},
        {"acoes 1", "acoes 0"},
        {"acoes 1", "acoes 513"},
        {"2 0\nfim", "2 0 3\nfim"},
        {"2 0\nfim", "7 0\nfim"},
        {"2 0\nfim", "6 5\nfim"},
        {"2 0\nfim", "6\nfim"},
        {"risco 2 0", "risco 0 0"},
        {"risco 2 0", "risco 7 0"},
        {"risco 2 0", "risco -1 0"},
        {"risco 2 0", "risco 2 -1"},
        {"risco 2 0 1", "risco 2 0 1000001"},
        {"0 3 0 77", "0 3 77"},
        {"0 3 0 77", "0 3 0 -77"},
        {"0 3 0 77", "0 3 0 77 8"},
        {"mao 1234 1 0", "mao 1234 5 0"},
        {"mao 1234 1 0", "mao 1234 6 0"},
        {"mao 1234 1 0", "mao 1234 8 0"},
        {"mao 1234 1 0", "mao 1234 9 0"},
        {"sementes 99", "sementes 1234567890123456789012345678901"},
        {"sementes 99", "sementes 12345678901234567890123456789012"},
        {"sementes 99", "sementes 1234567890123456789012345678901234567890123456789012345678901234"},
        {"mao 1234", "mao 1111111111111111111111111111111"},
        {"mao 1234", "mao 11111111111111111111111111111111"},
        {"2010 3 1 0", "2010 1000001 1 0"},
        {"2010 3 1 0", "2010 3 1 1000001"},
        {"board 0", "board 6 2 0 3 0 4 0 5 0 6 0 7 0"}
    };
    static const char *const BOAS[][2] = {
        {"2010 3 1 0", "2010 1000000 1 0"},
        {"2010 3 1 0", "2010 3 1 1000000"},
        {"board 0", "board 5 2 0 3 0 4 0 5 0 6 0"},
        {"mao 1234", "mao 4294967295"},
        {"sementes 99", "sementes 18446744073709551615"},
        {"2 0\nfim", "6 0\nfim"},
        {"2 0\nfim", "5 0\nfim"},
        {"risco 2 0 1 0 3 0 77", "risco 6 1000000 1000000 0 0 0 18446744073709551615"},
        {"risco 2 0 1 0 3 0 77", "risco 1 0 0 0 0 0 0"},
        {"mao 1234 1 0", "mao 1234 7 0"}
    };
    PartidaSalva lida;

    CHECK(salvamento_interpretar(BASE, strlen(BASE), &lida));
    CHECK(lida.torneio.fichas_jogador == 1990 && lida.torneio.fichas_adversario == 2010);
    CHECK(lida.semente_da_mao == 1234 && lida.estado_das_sementes == 99);
    CHECK(lida.rng_acao == 5 && lida.rng_equidade == 6);
    CHECK(lida.situacao == PARTIDA_VEZ_DO_JOGADOR && !lida.adversario_embriagado);
    CHECK(lida.risco.balas_roleta == 2 && lida.estado_do_risco == 77);
    CHECK(lida.risco.usos_trapaca[0] == 0 && lida.risco.usos_trapaca[1] == 1);
    CHECK(lida.risco.usos_trapaca[2] == 0 && lida.risco.usos_trapaca[3] == 3);
    CHECK(lida.risco.usos_trapaca[4] == 0);
    CHECK(lida.quantidade_no_registro == 1);
    CHECK(lida.registro[0].tipo == REGISTRO_ACAO && lida.registro[0].acao.type == POKER_CALL);
    CHECK(mesma_carta(lida.cartas[0][0], (PokerCard){14, POKER_SPADES}));
    CHECK(mesma_carta(lida.cartas[1][1], (PokerCard){9, POKER_HEARTS}));
    CHECK(lida.cartas_no_board == 0);

    for (size_t i = 0; i < sizeof RUINS / sizeof RUINS[0]; i++) {
        static char alterado[1024];
        const char *onde = strstr(BASE, RUINS[i][0]);
        size_t antes;

        CHECK(onde != NULL);
        antes = (size_t)(onde - BASE);
        CHECK(snprintf(alterado, sizeof alterado, "%.*s%s%s",
                       (int)antes, BASE, RUINS[i][1], onde + strlen(RUINS[i][0])) < (int)sizeof alterado);
        CHECK(!salvamento_interpretar(alterado, strlen(alterado), &lida));
    }

    for (size_t i = 0; i < sizeof BOAS / sizeof BOAS[0]; i++) {
        static char alterado[1024];
        const char *onde = strstr(BASE, BOAS[i][0]);

        CHECK(onde != NULL);
        CHECK(snprintf(alterado, sizeof alterado, "%.*s%s%s",
                       (int)(onde - BASE), BASE, BOAS[i][1], onde + strlen(BOAS[i][0])) < (int)sizeof alterado);
        CHECK(salvamento_interpretar(alterado, strlen(alterado), &lida));
    }

    {
        static char muitas[8192];
        const char *onde = strstr(BASE, "acoes 1\n2 0\n");
        size_t usado = (size_t)(onde - BASE);

        for (int quantidade = PARTIDA_REGISTRO_MAXIMO; quantidade <= PARTIDA_REGISTRO_MAXIMO + 1; quantidade++) {
            memcpy(muitas, BASE, usado);
            usado = (size_t)(onde - BASE);
            usado += (size_t)snprintf(muitas + usado, sizeof muitas - usado, "acoes %d\n", quantidade);

            for (int i = 0; i < quantidade; i++) {
                usado += (size_t)snprintf(muitas + usado, sizeof muitas - usado, "2 0\n");
            }

            usado += (size_t)snprintf(muitas + usado, sizeof muitas - usado, "fim\n");
            CHECK(usado < sizeof muitas);
            CHECK(salvamento_interpretar(muitas, usado, &lida) == (quantidade == PARTIDA_REGISTRO_MAXIMO));
            usado = (size_t)(onde - BASE);
        }
    }

    {
        static char com_nulo[512];
        static const char *const ONDE_COLOCAR[] = {"acoes 1", "fim", "torneio 0", "mao 1234", "sementes 99"};

        for (size_t i = 0; i < sizeof ONDE_COLOCAR / sizeof ONDE_COLOCAR[0]; i++) {
            const char *onde = strstr(BASE, ONDE_COLOCAR[i]);
            size_t depois = (size_t)(onde - BASE) + strlen(ONDE_COLOCAR[i]);
            size_t total = strlen(BASE);

            CHECK(onde != NULL && total + 1 < sizeof com_nulo);
            memcpy(com_nulo, BASE, depois);
            com_nulo[depois] = '\0';
            memcpy(com_nulo + depois + 1, BASE + depois, total - depois);
            CHECK(!salvamento_interpretar(com_nulo, total + 1, &lida));
        }
    }

    {
        char sem_dados[sizeof "DMBSAVE 2\nfim\n"] = "DMBSAVE 2\nfim\n";

        CHECK(!salvamento_interpretar(sem_dados, sizeof sem_dados - 1, &lida));
        CHECK(!salvamento_interpretar("", 0, &lida));
        CHECK(!salvamento_interpretar("\n\n  \n", 5, &lida));
    }

    {
        char texto_com_nulo[256];
        size_t n = strlen(BASE);

        memcpy(texto_com_nulo, BASE, n + 1);
        texto_com_nulo[n - 5] = '\0';
        CHECK(!salvamento_interpretar(texto_com_nulo, n, &lida));
    }
}

static void test_bytes_estragados_nunca_derrubam_o_jogo(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    static char estragado[SALVAMENTO_TAMANHO_MAXIMO];
    static char reescrito[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    unsigned sorteio = 99;
    size_t tamanho = texto_de_exemplo(texto, sizeof texto, &guardada);
    int aceitos = 0;

    CHECK(tamanho > 0);

    for (int rodada = 0; rodada < 6000; rodada++) {
        PartidaSalva lida;
        size_t trocas = 1 + proximo_aleatorio(&sorteio) % 3;

        memcpy(estragado, texto, tamanho);

        for (size_t i = 0; i < trocas; i++) {
            size_t onde = proximo_aleatorio(&sorteio) % tamanho;
            unsigned byte = proximo_aleatorio(&sorteio) % 4;

            estragado[onde] = byte == 0 ? '\0'
                            : byte == 1 ? (char)0xFF
                            : byte == 2 ? ' '
                            : (char)('0' + proximo_aleatorio(&sorteio) % 75);
        }

        if (salvamento_interpretar(estragado, tamanho, &lida)) {
            size_t n = salvamento_formatar(&lida, reescrito, sizeof reescrito);
            PartidaSalva de_novo;

            aceitos++;
            CHECK(n > 0);
            CHECK(salvamento_interpretar(reescrito, n, &de_novo));
            conferir_salvas_iguais(&lida, &de_novo);
            (void)partida_restaurar(&refeita, &lida);
            partida_encerrar(&refeita);
        }
    }

    CHECK(aceitos < 6000);
}

static void test_formatar_recusa_buffer_pequeno_e_argumentos_nulos(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    static char pequeno[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    size_t tamanho = texto_de_exemplo(texto, sizeof texto, &guardada);

    CHECK(tamanho > 0);

    for (size_t capacidade = 1; capacidade <= tamanho; capacidade++) {
        memset(pequeno, 'x', sizeof pequeno);
        CHECK(salvamento_formatar(&guardada, pequeno, capacidade) == 0);
        CHECK(pequeno[0] == '\0');
    }

    CHECK(salvamento_formatar(&guardada, pequeno, tamanho + 1) == tamanho);
    CHECK(strcmp(pequeno, texto) == 0);
    CHECK(salvamento_formatar(&guardada, pequeno, 0) == 0);
    CHECK(salvamento_formatar(&guardada, NULL, 100) == 0);
    CHECK(salvamento_formatar(NULL, pequeno, sizeof pequeno) == 0);
}

static void test_arquivo_grava_le_e_apaga(void)
{
    static char texto[SALVAMENTO_TAMANHO_MAXIMO];
    PartidaSalva guardada;
    PartidaSalva lida;
    PartidaSalva outra;
    FILE *arquivo;

    CHECK(texto_de_exemplo(texto, sizeof texto, &guardada) > 0);
    salvamento_apagar(ARQUIVO_DE_TESTE);

    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));

    CHECK(salvamento_gravar(&guardada, ARQUIVO_DE_TESTE));
    CHECK(salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(salvamento_ler(ARQUIVO_DE_TESTE, &lida));
    conferir_salvas_iguais(&guardada, &lida);
    CHECK(fopen(ARQUIVO_DE_TESTE ".tmp", "rb") == NULL);

    outra = guardada;
    outra.semente_da_mao += 1;
    outra.torneio.fichas_jogador += 1;
    CHECK(salvamento_gravar(&outra, ARQUIVO_DE_TESTE));
    CHECK(salvamento_ler(ARQUIVO_DE_TESTE, &lida));
    conferir_salvas_iguais(&outra, &lida);

    salvamento_apagar(ARQUIVO_DE_TESTE);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));
    salvamento_apagar(ARQUIVO_DE_TESTE);

    CHECK(!salvamento_gravar(&guardada, "nao_existe_esta_pasta/progresso.sav"));
    CHECK(!salvamento_gravar(NULL, ARQUIVO_DE_TESTE));
    CHECK(!salvamento_gravar(&guardada, NULL));
    CHECK(!salvamento_existe(NULL));
    CHECK(!salvamento_ler(NULL, &lida));
    salvamento_apagar(NULL);

    arquivo = fopen(ARQUIVO_DE_TESTE ".tmp", "wb");
    CHECK(arquivo != NULL);
    fputs("sobra de uma gravacao interrompida", arquivo);
    fclose(arquivo);
    salvamento_apagar(ARQUIVO_DE_TESTE);
    CHECK(fopen(ARQUIVO_DE_TESTE ".tmp", "rb") == NULL);
}

static void test_arquivo_estragado_e_reconhecido(void)
{
    static char grande[SALVAMENTO_TAMANHO_MAXIMO + 100];
    PartidaSalva lida;
    FILE *arquivo;

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fputs("isto nao e um salvamento\n", arquivo);
    fclose(arquivo);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fputs("DMBSAVE 2\ntorneio 0 1 2\n", arquivo);
    fclose(arquivo);
    CHECK(salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fputs("DMBSAVE 1\n", arquivo);
    fclose(arquivo);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fputs("DMBSAVE 3\n", arquivo);
    fclose(arquivo);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fputs("DMBSAVE 2 extra\n", arquivo);
    fclose(arquivo);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));

    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fclose(arquivo);
    CHECK(!salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));

    memset(grande, ' ', sizeof grande);
    memcpy(grande, "DMBSAVE 2\n", 10);
    arquivo = fopen(ARQUIVO_DE_TESTE, "wb");
    CHECK(arquivo != NULL);
    fwrite(grande, 1, sizeof grande, arquivo);
    fclose(arquivo);
    CHECK(salvamento_existe(ARQUIVO_DE_TESTE));
    CHECK(!salvamento_ler(ARQUIVO_DE_TESTE, &lida));

    salvamento_apagar(ARQUIVO_DE_TESTE);
}

/** Leva a partida original até a roleta de falência; false se a semente não faliu o jogador. */
static bool levar_original_a_roleta(unsigned semente)
{
    unsigned sorteio = semente * 104729u + 3u;

    CHECK(partida_iniciar(&original, semente));
    return andar_ate(&original, PARTIDA_ROLETA, politica_all_in, &sorteio);
}

static void test_a_roleta_e_o_gatilho_vao_e_voltam(void)
{
    int sobreviveu = 0;
    int morreu = 0;

    for (unsigned semente = 1; semente <= SEMENTES_DA_BUSCA && (!sobreviveu || !morreu); semente++) {
        PartidaSalva guardada;
        unsigned sorteio = semente * 17u + 1u;
        bool encontrou = levar_original_a_roleta(semente);

        if (!encontrou || original.quantidade_no_registro == 0) {
            partida_encerrar(&original);
            continue;
        }

        original.estado_do_risco = semente;
        original.estado_do_risco_da_mao = semente;
        CHECK(partida_exportar(&original, &guardada));
        CHECK(guardada.situacao == PARTIDA_ROLETA);
        CHECK(guardada.estado_do_risco == semente);

        /* Refeita no ponto da roleta, ainda antes do gatilho. */
        conferir_ida_e_volta(&original, &refeita);
        CHECK(refeita.situacao == PARTIDA_ROLETA);
        CHECK(refeita.roleta_motivo == ROLETA_FALENCIA);

        /* Depois do gatilho a mão guarda a entrada dele, e refazer reproduz o resultado. */
        CHECK(partida_puxar_gatilho(&original));
        CHECK(original.registro[original.quantidade_no_registro - 1].tipo == REGISTRO_GATILHO);

        if (original.situacao == PARTIDA_JOGADOR_MORTO) {
            morreu++;
            CHECK(!partida_exportar(&original, &guardada));
        } else {
            sobreviveu++;
            CHECK(original.situacao == PARTIDA_MAO_ENCERRADA);
            conferir_ida_e_volta(&original, &refeita);
            CHECK(refeita.torneio.recuperacoes == 1);
            CHECK(refeita.risco.balas_roleta == 2);
            correr_juntas(&original, &refeita, politica_aleatoria, &sorteio);
        }

        partida_encerrar(&refeita);
        partida_encerrar(&original);
    }

    CHECK(sobreviveu > 0 && morreu > 0);
}

static void test_partida_refeita_pode_ser_liberada_e_reusada(void)
{
    PartidaSalva guardada;
    unsigned sorteio = 17;

    CHECK(partida_iniciar(&original, 21));

    for (int i = 0; i < 12 && !partida_terminou(&original); i++) {
        CHECK(dar_um_passo(&original, politica_aleatoria, &sorteio));
    }

    CHECK(!partida_terminou(&original));
    CHECK(partida_exportar(&original, &guardada));

    for (int vez = 0; vez < 5; vez++) {
        CHECK(partida_restaurar(&refeita, &guardada));
        conferir_partidas_iguais(&original, &refeita);
    }

    CHECK(partida_iniciar(&refeita, 22));
    CHECK(partida_restaurar(&refeita, &guardada));
    conferir_partidas_iguais(&original, &refeita);

    partida_encerrar(&refeita);
    partida_encerrar(&original);
    partida_encerrar(&refeita);
}

int main(void)
{
    test_formato_de_texto_vai_e_volta();
    test_refazer_a_partida_a_reproduz_e_ela_continua_igual();
    test_refazer_em_cada_situacao_continuavel();
    test_salvar_de_novo_depois_de_refazer();
    test_a_mao_guarda_o_torneio_e_a_semente();
    test_exportar_recusa_o_que_nao_pode_continuar();
    test_refazer_recusa_dados_adulterados();
    test_o_estado_das_sementes_so_vale_para_as_proximas_maos();
    test_texto_cortado_ou_acrescido_e_recusado();
    test_cabecalho_e_palavras_erradas_sao_recusados();
    test_valores_fora_dos_limites_sao_recusados();
    test_numeros_malformados_sao_recusados();
    test_bytes_estragados_nunca_derrubam_o_jogo();
    test_formatar_recusa_buffer_pequeno_e_argumentos_nulos();
    test_arquivo_grava_le_e_apaga();
    test_arquivo_estragado_e_reconhecido();
    test_a_roleta_e_o_gatilho_vao_e_voltam();
    test_partida_refeita_pode_ser_liberada_e_reusada();
    puts("OK: salvamento (texto, arquivo, refazer a partida em cada situacao e continuar igual ate o fim do torneio).");
    return EXIT_SUCCESS;
}
