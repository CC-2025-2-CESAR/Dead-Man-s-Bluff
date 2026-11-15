#include "core/configuracoes.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static bool resolucao_zerada(Resolucao r)
{
    return r.largura == 0 && r.altura == 0 && !r.tela_cheia;
}

static void test_padrao_e_a_janela_de_1280_por_720(void)
{
    Configuracoes configuracoes = {-1};
    Resolucao padrao;

    configuracoes_iniciar(&configuracoes);
    padrao = configuracoes_resolucao_atual(&configuracoes);

    CHECK(padrao.largura == 1280);
    CHECK(padrao.altura == 720);
    CHECK(!padrao.tela_cheia);

    configuracoes_iniciar(NULL);
}

static void test_lista_de_resolucoes(void)
{
    int tela_cheia = 0;

    for (int i = 0; i < RESOLUCOES_TOTAL; i++) {
        Resolucao r = configuracoes_resolucao(i);

        if (r.tela_cheia) {
            tela_cheia++;
            CHECK(i == RESOLUCOES_TOTAL - 1);
            CHECK(r.largura == 0 && r.altura == 0);
            continue;
        }

        CHECK(r.largura > 0 && r.altura > 0);
        CHECK(r.largura * 9 == r.altura * 16);

        if (i > 0) {
            Resolucao anterior = configuracoes_resolucao(i - 1);

            CHECK(r.largura > anterior.largura);
            CHECK(r.altura > anterior.altura);
        }
    }

    CHECK(tela_cheia == 1);
}

static void test_indice_invalido_devolve_resolucao_zerada(void)
{
    CHECK(resolucao_zerada(configuracoes_resolucao(-1)));
    CHECK(resolucao_zerada(configuracoes_resolucao(RESOLUCOES_TOTAL)));
    CHECK(resolucao_zerada(configuracoes_resolucao(INT_MAX)));
    CHECK(resolucao_zerada(configuracoes_resolucao(INT_MIN)));
    CHECK(resolucao_zerada(configuracoes_resolucao_atual(NULL)));
}

static void test_escolher_resolucao(void)
{
    Configuracoes configuracoes;

    configuracoes_iniciar(&configuracoes);

    for (int i = 0; i < RESOLUCOES_TOTAL; i++) {
        Resolucao esperada = configuracoes_resolucao(i);
        Resolucao atual;

        CHECK(configuracoes_escolher_resolucao(&configuracoes, i));
        CHECK(configuracoes.resolucao == i);

        atual = configuracoes_resolucao_atual(&configuracoes);
        CHECK(atual.largura == esperada.largura);
        CHECK(atual.altura == esperada.altura);
        CHECK(atual.tela_cheia == esperada.tela_cheia);
    }

    CHECK(configuracoes_escolher_resolucao(&configuracoes, 2));
    CHECK(!configuracoes_escolher_resolucao(&configuracoes, -1));
    CHECK(!configuracoes_escolher_resolucao(&configuracoes, RESOLUCOES_TOTAL));
    CHECK(!configuracoes_escolher_resolucao(&configuracoes, INT_MAX));
    CHECK(!configuracoes_escolher_resolucao(NULL, 1));
    CHECK(configuracoes.resolucao == 2);
}

static void test_mover_selecao_da_a_volta_na_lista(void)
{
    CHECK(configuracoes_mover_selecao(0, 1) == 1);
    CHECK(configuracoes_mover_selecao(1, 1) == 2);
    CHECK(configuracoes_mover_selecao(2, -1) == 1);
    CHECK(configuracoes_mover_selecao(3, 0) == 3);

    CHECK(configuracoes_mover_selecao(RESOLUCOES_TOTAL - 1, 1) == 0);
    CHECK(configuracoes_mover_selecao(0, -1) == RESOLUCOES_TOTAL - 1);

    CHECK(configuracoes_mover_selecao(0, RESOLUCOES_TOTAL) == 0);
    CHECK(configuracoes_mover_selecao(2, -RESOLUCOES_TOTAL) == 2);
    CHECK(configuracoes_mover_selecao(1, 2 * RESOLUCOES_TOTAL + 1) == 2);
    CHECK(configuracoes_mover_selecao(1, -2 * RESOLUCOES_TOTAL - 1) == 0);
}

static void test_mover_selecao_aguenta_valores_extremos(void)
{
    for (int selecionada = -3; selecionada <= RESOLUCOES_TOTAL + 3; selecionada++) {
        int passos[] = {INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1, 0, 1, -1};

        for (size_t i = 0; i < sizeof passos / sizeof passos[0]; i++) {
            int destino = configuracoes_mover_selecao(selecionada, passos[i]);

            CHECK(destino >= 0 && destino < RESOLUCOES_TOTAL);
        }
    }

    CHECK(configuracoes_mover_selecao(INT_MAX, 1) == 1);
    CHECK(configuracoes_mover_selecao(INT_MIN, 0) == 0);
    CHECK(configuracoes_mover_selecao(-1, 1) == 1);
    CHECK(configuracoes_mover_selecao(RESOLUCOES_TOTAL, -1) == RESOLUCOES_TOTAL - 1);
}

int main(void)
{
    test_padrao_e_a_janela_de_1280_por_720();
    test_lista_de_resolucoes();
    test_indice_invalido_devolve_resolucao_zerada();
    test_escolher_resolucao();
    test_mover_selecao_da_a_volta_na_lista();
    test_mover_selecao_aguenta_valores_extremos();
    puts("OK: configuracoes (resolucao padrao, lista 16:9 com tela cheia, escolha e selecao circular).");
    return EXIT_SUCCESS;
}
