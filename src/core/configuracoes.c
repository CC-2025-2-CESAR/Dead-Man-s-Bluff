#include "core/configuracoes.h"

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RESOLUCAO_PADRAO 1

static const Resolucao RESOLUCOES[RESOLUCOES_TOTAL] = {
    {960, 540, false},
    {1280, 720, false},
    {1600, 900, false},
    {1920, 1080, false},
    {0, 0, true}
};

void configuracoes_iniciar(Configuracoes *configuracoes)
{
    if (configuracoes == NULL) {
        return;
    }

    configuracoes->resolucao = RESOLUCAO_PADRAO;
    configuracoes->volume_geral = 80;
    configuracoes->volume_sfx = 80;
    configuracoes->volume_musica = 40;
}

int configuracoes_volume(const Configuracoes *configuracoes, CanalVolume canal)
{
    if (configuracoes == NULL) {
        return 0;
    }

    switch (canal) {
    case VOLUME_GERAL: return configuracoes->volume_geral;
    case VOLUME_SFX: return configuracoes->volume_sfx;
    case VOLUME_MUSICA: return configuracoes->volume_musica;
    default: return 0;
    }
}

bool configuracoes_definir_volume(Configuracoes *configuracoes, CanalVolume canal, int valor)
{
    int *volume;

    if (configuracoes == NULL) {
        return false;
    }

    switch (canal) {
    case VOLUME_GERAL: volume = &configuracoes->volume_geral; break;
    case VOLUME_SFX: volume = &configuracoes->volume_sfx; break;
    case VOLUME_MUSICA: volume = &configuracoes->volume_musica; break;
    default: return false;
    }

    *volume = valor < 0 ? 0 : valor > 100 ? 100 : valor;
    return true;
}

bool configuracoes_carregar_volumes(Configuracoes *configuracoes, const char *caminho)
{
    char conteudo[256];
    int versao, geral, sfx, musica;
    int lidos = 0;
    size_t tamanho;
    bool leitura_ok;
    FILE *arquivo;

    if (configuracoes == NULL || caminho == NULL || caminho[0] == '\0') {
        return false;
    }

    arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        return false;
    }

    tamanho = fread(conteudo, 1, sizeof conteudo - 1, arquivo);
    leitura_ok = !ferror(arquivo) && feof(arquivo);
    if (fclose(arquivo) != 0 || !leitura_ok) {
        return false;
    }
    conteudo[tamanho] = '\0';

    if (sscanf(conteudo, "DMBAUDIO %3d geral %3d sfx %3d musica %3d %n",
               &versao, &geral, &sfx, &musica, &lidos) != 4 ||
        lidos == 0 || (size_t)lidos != tamanho || versao != 1 ||
        geral < 0 || geral > 100 || sfx < 0 || sfx > 100 || musica < 0 || musica > 100) {
        return false;
    }

    configuracoes->volume_geral = geral;
    configuracoes->volume_sfx = sfx;
    configuracoes->volume_musica = musica;
    return true;
}

bool configuracoes_salvar_volumes(const Configuracoes *configuracoes, const char *caminho)
{
    char temporario[512];
    FILE *arquivo;
    bool escrita_ok;
    int tamanho;

    if (configuracoes == NULL || caminho == NULL || caminho[0] == '\0') {
        return false;
    }
    for (int canal = 0; canal < VOLUMES_TOTAL; canal++) {
        int volume = configuracoes_volume(configuracoes, (CanalVolume)canal);
        if (volume < 0 || volume > 100) {
            return false;
        }
    }
    tamanho = snprintf(temporario, sizeof temporario, "%s.tmp", caminho);
    if (tamanho < 0 || tamanho >= (int)sizeof temporario) {
        return false;
    }

    arquivo = fopen(temporario, "wb");
    if (arquivo == NULL) {
        return false;
    }
    escrita_ok = fprintf(arquivo, "DMBAUDIO 1\ngeral %d\nsfx %d\nmusica %d\n",
                          configuracoes->volume_geral, configuracoes->volume_sfx,
                          configuracoes->volume_musica) > 0;
    if (fclose(arquivo) != 0) {
        escrita_ok = false;
    }

#ifdef _WIN32
    if (!escrita_ok || !MoveFileExA(temporario, caminho,
                                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    if (!escrita_ok || rename(temporario, caminho) != 0) {
#endif
        remove(temporario);
        return false;
    }
    return true;
}

Resolucao configuracoes_resolucao(int indice)
{
    if (indice < 0 || indice >= RESOLUCOES_TOTAL) {
        return (Resolucao){0, 0, false};
    }

    return RESOLUCOES[indice];
}

Resolucao configuracoes_resolucao_atual(const Configuracoes *configuracoes)
{
    if (configuracoes == NULL) {
        return (Resolucao){0, 0, false};
    }

    return configuracoes_resolucao(configuracoes->resolucao);
}

bool configuracoes_escolher_resolucao(Configuracoes *configuracoes, int indice)
{
    if (configuracoes == NULL || indice < 0 || indice >= RESOLUCOES_TOTAL) {
        return false;
    }

    configuracoes->resolucao = indice;
    return true;
}

int configuracoes_mover_selecao(int selecionada, int passos)
{
    long long base = selecionada < 0 || selecionada >= RESOLUCOES_TOTAL ? 0 : selecionada;
    long long destino = (base + passos) % RESOLUCOES_TOTAL;

    if (destino < 0) {
        destino += RESOLUCOES_TOTAL;
    }

    return (int)destino;
}
