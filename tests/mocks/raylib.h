#ifndef TEST_RAYLIB_H
#define TEST_RAYLIB_H

#include <stdbool.h>

typedef struct { int id; } Sound;
typedef struct { int id; bool looping; } Music;
typedef struct {
    unsigned frameCount;
    unsigned sampleRate;
    unsigned sampleSize;
    unsigned channels;
    void *data;
} Wave;

enum { LOG_WARNING = 4 };

void InitAudioDevice(void);
bool IsAudioDeviceReady(void);
void CloseAudioDevice(void);
void SetMasterVolume(float volume);
Sound LoadSound(const char *caminho);
bool IsSoundValid(Sound som);
void SetSoundVolume(Sound som, float volume);
void PlaySound(Sound som);
void StopSound(Sound som);
void UnloadSound(Sound som);
Wave LoadWave(const char *caminho);
bool IsWaveValid(Wave onda);
void WaveCrop(Wave *onda, int primeiro, int ultimo);
Sound LoadSoundFromWave(Wave onda);
void UnloadWave(Wave onda);
Music LoadMusicStream(const char *caminho);
bool IsMusicValid(Music musica);
void SetMusicVolume(Music musica, float volume);
void PlayMusicStream(Music musica);
void UpdateMusicStream(Music musica);
void UnloadMusicStream(Music musica);
void TraceLog(int nivel, const char *formato, ...);

#endif
