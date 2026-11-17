#ifndef ANIMACOES_H
#define ANIMACOES_H

#include <stdbool.h>

void animacoes_iniciar(void);
void animacoes_encerrar(void);
void animacoes_comecar_embaralhamento(void);
void animacoes_cancelar(void);
void animacoes_ignorar_proximo_inicio(void);
bool animacoes_embaralhando(void);
bool animacoes_ativas(void);
bool animacoes_iniciais_ativas(void);
void animacoes_board_revelado(int quantidade);
int animacoes_board_frentes(void);
int animacoes_board_versos(void);
float animacoes_board_virada(void);
void animacoes_atualizar(double segundos);
void animacoes_desenhar(void);

#endif
