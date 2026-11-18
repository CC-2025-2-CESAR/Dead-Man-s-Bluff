#include "ui/audio.h"
#include "raylib.h"

#include <math.h>
#include <stddef.h>

#define TOMADAS_CARTA 3
#define AGENDAMENTOS_TOTAL 16

typedef struct {
    EfeitoSonoro efeito;
    double restante;
    bool ativo;
} SomAgendado;

static const char *const CAMINHOS[SONS_TOTAL] = {
    [SOM_EMBARALHAR] = "assets/sounds/carta_distribuir_1.mp3",
    [SOM_DISTRIBUIR] = "assets/sounds/carta_distribuir_1.mp3",
    [SOM_COLOCAR_CARTA] = "assets/sounds/carta_mesa.mp3",
    [SOM_VIRAR_CARTA] = "assets/sounds/carta_virar.mp3",
    [SOM_GIRAR_TAMBOR] = "assets/sounds/revolver_giro.mp3",
    [SOM_ENGATILHAR] = "assets/sounds/revolver_gatilho.mp3",
    [SOM_TENSAO] = "assets/sounds/descoberto.mp3",
    [SOM_DISPARO] = "assets/sounds/revolver_tiro.mp3"
};
static const char *const CAMINHOS_TOMADAS[TOMADAS_CARTA - 1] = {
    "assets/sounds/carta_distribuir_2.mp3",
    "assets/sounds/carta_distribuir_3.mp3"
};

static Sound sons[SONS_TOTAL];
static Sound tomadas[TOMADAS_CARTA - 1];
static Music musica;
static SomAgendado agendados[AGENDAMENTOS_TOTAL];
static bool iniciado;
static bool dispositivo_proprio;
static bool efeitos_suspensos;
static unsigned proxima_tomada;
static int volume_geral = -1;
static int volume_sfx = -1;
static int volume_musica = -1;

static int limitar_volume(int volume)
{
    if (volume < 0)
        return 0;
    if (volume > 100)
        return 100;
    return volume;
}

static bool efeito_valido(EfeitoSonoro efeito)
{
    return efeito >= SOM_EMBARALHAR && efeito < SONS_TOTAL;
}

static Sound carregar_efeito(EfeitoSonoro efeito)
{
    double inicio, fim;
    Wave onda;
    Sound som;
    int primeiro, ultimo;

    switch (efeito) {
    case SOM_GIRAR_TAMBOR:
        inicio = 0.15;
        fim = 1.12;
        break;
    case SOM_ENGATILHAR:
        inicio = 0.60;
        fim = 1.18;
        break;
    case SOM_TENSAO:
        inicio = 0.40;
        fim = 1.72;
        break;
    default:
        return LoadSound(CAMINHOS[efeito]);
    }

    onda = LoadWave(CAMINHOS[efeito]);
    if (!IsWaveValid(onda))
        return (Sound){ 0 };

    primeiro = (int)(inicio * onda.sampleRate);
    ultimo = (int)(fim * onda.sampleRate);
    if ((unsigned)ultimo > onda.frameCount)
        ultimo = (int)onda.frameCount;
    if (ultimo > primeiro)
        WaveCrop(&onda, primeiro, ultimo);
    som = LoadSoundFromWave(onda);
    UnloadWave(onda);
    return som;
}

static void aplicar_volumes(const Configuracoes *configuracoes)
{
    int geral, sfx, fundo;

    if (configuracoes == NULL)
        return;

    geral = limitar_volume(configuracoes->volume_geral);
    sfx = limitar_volume(configuracoes->volume_sfx);
    fundo = limitar_volume(configuracoes->volume_musica);

    if (geral != volume_geral) {
        SetMasterVolume((float)geral / 100.0f);
        volume_geral = geral;
    }
    if (sfx != volume_sfx) {
        for (int i = 0; i < SONS_TOTAL; i++) {
            if (IsSoundValid(sons[i]))
                SetSoundVolume(sons[i], (float)sfx / 100.0f);
        }
        for (int i = 0; i < TOMADAS_CARTA - 1; i++) {
            if (IsSoundValid(tomadas[i]))
                SetSoundVolume(tomadas[i], (float)sfx / 100.0f);
        }
        volume_sfx = sfx;
    }
    if (fundo != volume_musica) {
        if (IsMusicValid(musica))
            SetMusicVolume(musica, (float)fundo / 100.0f);
        volume_musica = fundo;
    }
}

void audio_iniciar(const Configuracoes *configuracoes)
{
    if (iniciado) {
        aplicar_volumes(configuracoes);
        return;
    }

    dispositivo_proprio = !IsAudioDeviceReady();
    if (dispositivo_proprio)
        InitAudioDevice();
    if (!IsAudioDeviceReady())
        return;

    iniciado = true;
    efeitos_suspensos = false;
    proxima_tomada = 0;
    volume_geral = -1;
    volume_sfx = -1;
    volume_musica = -1;
    for (int i = 0; i < SONS_TOTAL; i++)
        sons[i] = carregar_efeito((EfeitoSonoro)i);
    for (int i = 0; i < TOMADAS_CARTA - 1; i++)
        tomadas[i] = LoadSound(CAMINHOS_TOMADAS[i]);

    musica = LoadMusicStream("assets/sounds/casino.mp3");
    musica.looping = true;
    aplicar_volumes(configuracoes);
    if (IsMusicValid(musica))
        PlayMusicStream(musica);
}

void audio_atualizar(double segundos, const Configuracoes *configuracoes)
{
    if (!iniciado)
        return;

    aplicar_volumes(configuracoes);
    if (IsMusicValid(musica))
        UpdateMusicStream(musica);

    if (!isfinite(segundos) || segundos <= 0.0)
        return;

    for (int i = 0; i < AGENDAMENTOS_TOTAL; i++) {
        if (!agendados[i].ativo)
            continue;
        agendados[i].restante -= segundos;
        if (agendados[i].restante <= 0.0) {
            EfeitoSonoro efeito = agendados[i].efeito;
            agendados[i].ativo = false;
            audio_tocar(efeito);
        }
    }
}

void audio_tocar(EfeitoSonoro efeito)
{
    Sound som;

    if (!iniciado || efeitos_suspensos || !efeito_valido(efeito))
        return;

    som = sons[efeito];
    if (efeito == SOM_DISTRIBUIR || efeito == SOM_EMBARALHAR) {
        unsigned tomada = proxima_tomada;
        proxima_tomada = (proxima_tomada + 1) % TOMADAS_CARTA;
        if (tomada > 0 && IsSoundValid(tomadas[tomada - 1]))
            som = tomadas[tomada - 1];
    }
    if (IsSoundValid(som))
        PlaySound(som);
}

void audio_cancelar_agendados(void)
{
    for (int i = 0; i < AGENDAMENTOS_TOTAL; i++)
        agendados[i].ativo = false;
}

void audio_parar_efeitos(void)
{
    audio_cancelar_agendados();
    if (!iniciado)
        return;

    for (int i = 0; i < SONS_TOTAL; i++) {
        if (IsSoundValid(sons[i]))
            StopSound(sons[i]);
    }
    for (int i = 0; i < TOMADAS_CARTA - 1; i++) {
        if (IsSoundValid(tomadas[i]))
            StopSound(tomadas[i]);
    }
}

void audio_suspender_efeitos(bool suspender)
{
    if (suspender)
        audio_parar_efeitos();
    efeitos_suspensos = suspender;
}

void audio_agendar(EfeitoSonoro efeito, double atraso)
{
    if (!iniciado || efeitos_suspensos || !efeito_valido(efeito) || !isfinite(atraso))
        return;
    if (atraso <= 0.0) {
        audio_tocar(efeito);
        return;
    }

    for (int i = 0; i < AGENDAMENTOS_TOTAL; i++) {
        if (!agendados[i].ativo) {
            agendados[i] = (SomAgendado){ efeito, atraso, true };
            return;
        }
    }
    TraceLog(LOG_WARNING, "AUDIO: Fila de efeitos sonoros cheia");
}

void audio_encerrar(void)
{
    audio_parar_efeitos();
    if (!iniciado)
        return;

    if (IsMusicValid(musica))
        UnloadMusicStream(musica);
    musica = (Music){ 0 };
    for (int i = 0; i < SONS_TOTAL; i++) {
        if (IsSoundValid(sons[i]))
            UnloadSound(sons[i]);
        sons[i] = (Sound){ 0 };
    }
    for (int i = 0; i < TOMADAS_CARTA - 1; i++) {
        if (IsSoundValid(tomadas[i]))
            UnloadSound(tomadas[i]);
        tomadas[i] = (Sound){ 0 };
    }
    if (dispositivo_proprio)
        CloseAudioDevice();
    iniciado = false;
    dispositivo_proprio = false;
    efeitos_suspensos = false;
}
