#include "core/configuracoes.h"

#include <stddef.h>

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
