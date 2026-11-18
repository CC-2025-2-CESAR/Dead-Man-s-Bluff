#ifndef AUDIO_H
#define AUDIO_H

#include "core/configuracoes.h"

typedef enum {
    SOM_EMBARALHAR,
    SOM_DISTRIBUIR,
    SOM_COLOCAR_CARTA,
    SOM_VIRAR_CARTA,
    SOM_GIRAR_TAMBOR,
    SOM_ENGATILHAR,
    SOM_TENSAO,
    SOM_DISPARO,
    SONS_TOTAL
} EfeitoSonoro;

void audio_iniciar(const Configuracoes *configuracoes);
void audio_atualizar(double segundos, const Configuracoes *configuracoes);
void audio_encerrar(void);
void audio_tocar(EfeitoSonoro efeito);
void audio_parar_efeitos(void);
void audio_suspender_efeitos(bool suspender);
void audio_agendar(EfeitoSonoro efeito, double atraso);
void audio_cancelar_agendados(void);

#endif
