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
    Configuracoes configuracoes = {.resolucao = -1};
    Resolucao padrao;

    configuracoes_iniciar(&configuracoes);
    padrao = configuracoes_resolucao_atual(&configuracoes);

    CHECK(padrao.largura == 1280);
    CHECK(padrao.altura == 720);
    CHECK(!padrao.tela_cheia);
    CHECK(configuracoes.volume_geral == 80);
    CHECK(configuracoes.volume_sfx == 80);
    CHECK(configuracoes.volume_musica == 40);

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

static void test_volumes_sao_independentes_e_limitados(void)
{
    Configuracoes configuracoes;

    configuracoes_iniciar(&configuracoes);
    for (int canal = 0; canal < VOLUMES_TOTAL; canal++) {
        CHECK(configuracoes_definir_volume(&configuracoes, (CanalVolume)canal, INT_MIN));
        CHECK(configuracoes_volume(&configuracoes, (CanalVolume)canal) == 0);
        CHECK(configuracoes_definir_volume(&configuracoes, (CanalVolume)canal, INT_MAX));
        CHECK(configuracoes_volume(&configuracoes, (CanalVolume)canal) == 100);
        CHECK(configuracoes_definir_volume(&configuracoes, (CanalVolume)canal, canal * 25));
    }
    CHECK(configuracoes.volume_geral == 0);
    CHECK(configuracoes.volume_sfx == 25);
    CHECK(configuracoes.volume_musica == 50);
    CHECK(configuracoes.resolucao == 1);
    CHECK(!configuracoes_definir_volume(NULL, VOLUME_GERAL, 50));
    CHECK(!configuracoes_definir_volume(&configuracoes, (CanalVolume)-1, 50));
    CHECK(!configuracoes_definir_volume(&configuracoes, VOLUMES_TOTAL, 50));
    CHECK(configuracoes_volume(NULL, VOLUME_GERAL) == 0);
    CHECK(configuracoes_volume(&configuracoes, VOLUMES_TOTAL) == 0);
    CHECK(configuracoes.volume_geral == 0);
    CHECK(configuracoes.volume_sfx == 25);
    CHECK(configuracoes.volume_musica == 50);
}

static void test_volumes_persistem_sem_alterar_a_resolucao(void)
{
    const char *caminho = "build/test_configuracoes_audio.cfg";
    Configuracoes origem;
    Configuracoes destino;

    configuracoes_iniciar(&origem);
    configuracoes_iniciar(&destino);
    CHECK(configuracoes_escolher_resolucao(&destino, 3));
    CHECK(configuracoes_definir_volume(&origem, VOLUME_GERAL, 0));
    CHECK(configuracoes_definir_volume(&origem, VOLUME_SFX, 100));
    CHECK(configuracoes_definir_volume(&origem, VOLUME_MUSICA, 17));
    CHECK(configuracoes_salvar_volumes(&origem, caminho));
    CHECK(configuracoes_carregar_volumes(&destino, caminho));
    CHECK(destino.volume_geral == 0);
    CHECK(destino.volume_sfx == 100);
    CHECK(destino.volume_musica == 17);
    CHECK(destino.resolucao == 3);

    CHECK(configuracoes_definir_volume(&origem, VOLUME_MUSICA, 99));
    CHECK(configuracoes_salvar_volumes(&origem, caminho));
    CHECK(configuracoes_carregar_volumes(&destino, caminho));
    CHECK(destino.volume_musica == 99);
    CHECK(destino.resolucao == 3);
    CHECK(remove(caminho) == 0);
    CHECK(!configuracoes_carregar_volumes(&destino, caminho));
    CHECK(destino.volume_musica == 99);
}

static void test_arquivo_de_volume_invalido_nao_altera_nada(void)
{
    const char *caminho = "build/test_configuracoes_audio.cfg";
    const char *invalidos[] = {
        "DMBAUDIO 2\ngeral 10\nsfx 20\nmusica 30\n",
        "DMBAUDIO 1\ngeral 10\nsfx 20\n",
        "DMBAUDIO 1\ngeral 10\nsfx 20\nmusica 101\n",
        "DMBAUDIO 1\ngeral -1\nsfx 20\nmusica 30\n",
        "DMBAUDIO 1\ngeral 99999999999999999999999\nsfx 20\nmusica 30\n",
        "DMBAUDIO 1\ngeral 10\nsfx 20\nmusica 30\nlixo\n"
    };
    Configuracoes configuracoes;

    configuracoes_iniciar(&configuracoes);
    for (size_t i = 0; i < sizeof invalidos / sizeof invalidos[0]; i++) {
        FILE *arquivo = fopen(caminho, "wb");
        CHECK(arquivo != NULL);
        CHECK(fputs(invalidos[i], arquivo) >= 0);
        CHECK(fclose(arquivo) == 0);
        CHECK(!configuracoes_carregar_volumes(&configuracoes, caminho));
        CHECK(configuracoes.volume_geral == 80);
        CHECK(configuracoes.volume_sfx == 80);
        CHECK(configuracoes.volume_musica == 40);
    }
    CHECK(remove(caminho) == 0);
    CHECK(!configuracoes_carregar_volumes(NULL, caminho));
    CHECK(!configuracoes_carregar_volumes(&configuracoes, NULL));
    CHECK(!configuracoes_salvar_volumes(NULL, caminho));
    CHECK(!configuracoes_salvar_volumes(&configuracoes, NULL));
    CHECK(!configuracoes_salvar_volumes(&configuracoes, ""));
}

int main(void)
{
    test_padrao_e_a_janela_de_1280_por_720();
    test_lista_de_resolucoes();
    test_indice_invalido_devolve_resolucao_zerada();
    test_escolher_resolucao();
    test_mover_selecao_da_a_volta_na_lista();
    test_mover_selecao_aguenta_valores_extremos();
    test_volumes_sao_independentes_e_limitados();
    test_volumes_persistem_sem_alterar_a_resolucao();
    test_arquivo_de_volume_invalido_nao_altera_nada();
    puts("OK: configuracoes (resolucoes, volumes 0..100 independentes e persistencia validada).");
    return EXIT_SUCCESS;
}
