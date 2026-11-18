#include "ui/audio.h"
#include "raylib.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static bool dispositivo_pronto;
static bool falhar_dispositivo;
static bool falhar_arquivos;
static int aberturas, fechamentos, sons_carregados, sons_liberados;
static int musicas_iniciadas, musicas_atualizadas, musicas_liberadas;
static int reproducoes, paradas, avisos, cortes;
static int som_reproduzido[64];
static const char *arquivos[32];
static float volumes[32], volume_master, volume_fundo;
static int corte_inicio[3], corte_fim[3];
static bool musica_em_loop;

static Configuracoes padrao(void)
{
    return (Configuracoes){ .resolucao = 1, .volume_geral = 70,
        .volume_sfx = 80, .volume_musica = 35 };
}

static bool proximo(float atual, float esperado)
{
    return fabsf(atual - esperado) < 0.0001f;
}

static void limpar_mock(void)
{
    dispositivo_pronto = false;
    falhar_dispositivo = false;
    falhar_arquivos = false;
    aberturas = fechamentos = sons_carregados = sons_liberados = 0;
    musicas_iniciadas = musicas_atualizadas = musicas_liberadas = 0;
    reproducoes = paradas = avisos = cortes = 0;
    volume_master = volume_fundo = -1.0f;
    musica_em_loop = false;
    memset(arquivos, 0, sizeof arquivos);
    memset(volumes, 0, sizeof volumes);
}

void InitAudioDevice(void)
{
    aberturas++;
    dispositivo_pronto = !falhar_dispositivo;
}

bool IsAudioDeviceReady(void) { return dispositivo_pronto; }
void CloseAudioDevice(void) { fechamentos++; dispositivo_pronto = false; }
void SetMasterVolume(float volume) { volume_master = volume; }

Sound LoadSound(const char *caminho)
{
    if (falhar_arquivos)
        return (Sound){ 0 };
    CHECK(sons_carregados < 31);
    arquivos[++sons_carregados] = caminho;
    return (Sound){ sons_carregados };
}

bool IsSoundValid(Sound som) { return som.id > 0; }

void SetSoundVolume(Sound som, float volume)
{
    CHECK(IsSoundValid(som));
    volumes[som.id] = volume;
}

void PlaySound(Sound som)
{
    CHECK(IsSoundValid(som));
    CHECK(reproducoes < 64);
    som_reproduzido[reproducoes++] = som.id;
}

void StopSound(Sound som) { CHECK(IsSoundValid(som)); paradas++; }
void UnloadSound(Sound som) { CHECK(IsSoundValid(som)); sons_liberados++; }

Wave LoadWave(const char *caminho)
{
    if (falhar_arquivos)
        return (Wave){ 0 };
    return (Wave){ 4000, 1000, 16, 1, (void *)caminho };
}

bool IsWaveValid(Wave onda) { return onda.data != NULL; }

void WaveCrop(Wave *onda, int primeiro, int ultimo)
{
    CHECK(cortes < 3);
    CHECK(primeiro >= 0 && ultimo > primeiro && (unsigned)ultimo <= onda->frameCount);
    corte_inicio[cortes] = primeiro;
    corte_fim[cortes++] = ultimo;
    onda->frameCount = (unsigned)(ultimo - primeiro);
}

Sound LoadSoundFromWave(Wave onda) { return LoadSound(onda.data); }
void UnloadWave(Wave onda) { CHECK(IsWaveValid(onda)); }

Music LoadMusicStream(const char *caminho)
{
    CHECK(strcmp(caminho, "assets/sounds/casino.mp3") == 0);
    return (Music){ falhar_arquivos ? 0 : 1, false };
}

bool IsMusicValid(Music musica) { return musica.id > 0; }
void SetMusicVolume(Music musica, float volume)
{
    CHECK(IsMusicValid(musica));
    volume_fundo = volume;
}
void PlayMusicStream(Music musica)
{
    CHECK(IsMusicValid(musica));
    musicas_iniciadas++;
    musica_em_loop = musica.looping;
}
void UpdateMusicStream(Music musica) { CHECK(IsMusicValid(musica)); musicas_atualizadas++; }
void UnloadMusicStream(Music musica) { CHECK(IsMusicValid(musica)); musicas_liberadas++; }
void TraceLog(int nivel, const char *formato, ...)
{
    CHECK(nivel == LOG_WARNING && formato != NULL);
    avisos++;
}

static void test_volumes_independentes_e_musica_continua(void)
{
    Configuracoes cfg = padrao();
    limpar_mock();
    audio_iniciar(&cfg);
    CHECK(aberturas == 1 && sons_carregados == 10);
    CHECK(musicas_iniciadas == 1 && musica_em_loop);
    CHECK(proximo(volume_master, 0.70f));
    CHECK(proximo(volume_fundo, 0.35f));
    for (int i = 1; i <= sons_carregados; i++)
        CHECK(proximo(volumes[i], 0.80f));
    CHECK(cortes == 3);
    CHECK(corte_inicio[0] == 150 && corte_fim[0] == 1120);
    CHECK(corte_inicio[1] == 600 && corte_fim[1] == 1180);
    CHECK(corte_inicio[2] == 400 && corte_fim[2] == 1720);

    cfg.volume_geral = 0;
    cfg.volume_sfx = 0;
    cfg.volume_musica = 100;
    audio_atualizar(0.1, &cfg);
    CHECK(proximo(volume_master, 0.0f));
    CHECK(proximo(volumes[1], 0.0f));
    CHECK(proximo(volume_fundo, 1.0f));
    CHECK(musicas_atualizadas == 1 && musicas_iniciadas == 1);

    cfg.volume_geral = INT_MAX;
    cfg.volume_sfx = INT_MIN;
    cfg.volume_musica = INT_MAX;
    audio_atualizar(0.1, &cfg);
    CHECK(proximo(volume_master, 1.0f));
    CHECK(proximo(volumes[1], 0.0f));
    CHECK(proximo(volume_fundo, 1.0f));
    audio_iniciar(&cfg);
    CHECK(aberturas == 1 && musicas_iniciadas == 1);
    audio_encerrar();
    CHECK(sons_liberados == 10 && musicas_liberadas == 1 && fechamentos == 1);
    audio_encerrar();
    CHECK(fechamentos == 1);
}

static void test_fila_suspensao_e_cancelamento(void)
{
    Configuracoes cfg = padrao();
    limpar_mock();
    audio_iniciar(&cfg);
    audio_tocar((EfeitoSonoro)-1);
    audio_tocar(SONS_TOTAL);
    audio_agendar(SOM_DISPARO, NAN);
    audio_agendar(SOM_DISPARO, INFINITY);
    CHECK(reproducoes == 0);
    audio_agendar(SOM_ENGATILHAR, 1.05);
    audio_atualizar(1.0, &cfg);
    CHECK(reproducoes == 0);
    audio_atualizar(0.06, &cfg);
    CHECK(reproducoes == 1);
    CHECK(strstr(arquivos[som_reproduzido[0]], "revolver_gatilho") != NULL);
    audio_atualizar(1.0, &cfg);
    CHECK(reproducoes == 1);

    audio_agendar(SOM_DISPARO, 0.5);
    audio_cancelar_agendados();
    audio_atualizar(1.0, &cfg);
    CHECK(reproducoes == 1);
    audio_agendar(SOM_DISPARO, 0.5);
    audio_suspender_efeitos(true);
    audio_tocar(SOM_GIRAR_TAMBOR);
    audio_agendar(SOM_DISPARO, 0.5);
    audio_atualizar(1.0, &cfg);
    CHECK(reproducoes == 1 && musicas_atualizadas == 5);
    audio_suspender_efeitos(false);
    audio_atualizar(1.0, &cfg);
    CHECK(reproducoes == 1);
    audio_agendar(SOM_TENSAO, -0.5);
    CHECK(reproducoes == 2);
    audio_parar_efeitos();
    CHECK(paradas >= 10 && musicas_liberadas == 0);

    for (int i = 0; i < 17; i++)
        audio_agendar(SOM_DISPARO, 0.5);
    CHECK(avisos == 1);
    audio_atualizar(0.6, &cfg);
    CHECK(reproducoes == 18);
    audio_encerrar();
}

static void test_tomadas_sem_mudar_sorteio_do_jogo(void)
{
    Configuracoes cfg = padrao();
    int esperado;
    limpar_mock();
    srand(1567);
    esperado = rand();
    srand(1567);
    audio_iniciar(&cfg);
    for (int i = 0; i < 4; i++)
        audio_tocar(SOM_DISTRIBUIR);
    CHECK(rand() == esperado);
    CHECK(strstr(arquivos[som_reproduzido[0]], "carta_distribuir_1") != NULL);
    CHECK(strstr(arquivos[som_reproduzido[1]], "carta_distribuir_2") != NULL);
    CHECK(strstr(arquivos[som_reproduzido[2]], "carta_distribuir_3") != NULL);
    CHECK(som_reproduzido[3] == som_reproduzido[0]);
    audio_tocar(SOM_EMBARALHAR);
    audio_tocar(SOM_EMBARALHAR);
    audio_tocar(SOM_EMBARALHAR);
    CHECK(strstr(arquivos[som_reproduzido[4]], "carta_distribuir_2") != NULL);
    CHECK(strstr(arquivos[som_reproduzido[5]], "carta_distribuir_3") != NULL);
    CHECK(strstr(arquivos[som_reproduzido[6]], "carta_distribuir_1") != NULL);
    audio_encerrar();
}

static void test_falhas_de_dispositivo_e_arquivos(void)
{
    Configuracoes cfg = padrao();
    limpar_mock();
    falhar_dispositivo = true;
    audio_iniciar(&cfg);
    audio_tocar(SOM_DISPARO);
    audio_atualizar(1.0, &cfg);
    audio_encerrar();
    CHECK(reproducoes == 0 && sons_carregados == 0 && fechamentos == 0);

    limpar_mock();
    falhar_arquivos = true;
    audio_iniciar(&cfg);
    audio_tocar(SOM_DISPARO);
    audio_tocar(SOM_DISTRIBUIR);
    audio_atualizar(1.0, &cfg);
    audio_encerrar();
    CHECK(reproducoes == 0 && sons_liberados == 0 && musicas_iniciadas == 0);
    CHECK(fechamentos == 1);

    limpar_mock();
    dispositivo_pronto = true;
    audio_iniciar(&cfg);
    audio_encerrar();
    CHECK(aberturas == 0 && fechamentos == 0);
}

int main(void)
{
    test_volumes_independentes_e_musica_continua();
    test_fila_suspensao_e_cancelamento();
    test_tomadas_sem_mudar_sorteio_do_jogo();
    test_falhas_de_dispositivo_e_arquivos();
    puts("audio: todos os testes passaram");
    return EXIT_SUCCESS;
}
