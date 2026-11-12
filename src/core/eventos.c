#include "core/eventos.h"

#include <string.h>

void fila_eventos_iniciar(FilaEventos *fila)
{
    if (fila == NULL) {
        return;
    }

    memset(fila, 0, sizeof *fila);
}

bool fila_eventos_adicionar(FilaEventos *fila, const EventoJogo *evento)
{
    size_t posicao;

    if (fila == NULL || evento == NULL) {
        return false;
    }

    if (fila->quantidade == FILA_EVENTOS_CAPACIDADE) {
        fila->perdidos++;
        return false;
    }

    posicao = (fila->inicio + fila->quantidade) % FILA_EVENTOS_CAPACIDADE;
    fila->itens[posicao] = *evento;
    fila->quantidade++;
    return true;
}

bool fila_eventos_retirar(FilaEventos *fila, EventoJogo *saida)
{
    if (fila == NULL || saida == NULL || fila->quantidade == 0) {
        return false;
    }

    *saida = fila->itens[fila->inicio];
    fila->inicio = (fila->inicio + 1) % FILA_EVENTOS_CAPACIDADE;
    fila->quantidade--;
    return true;
}

size_t fila_eventos_tamanho(const FilaEventos *fila)
{
    return fila == NULL ? 0 : fila->quantidade;
}
