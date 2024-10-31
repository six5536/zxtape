#ifndef _mac_audio_h_
#define _mac_audio_h_

#include <stdbool.h>

typedef void (*AudioBufferCallback)(void *buffer, unsigned int bufferSize);

void InitMacSound(AudioBufferCallback callback);
void DeinitMacSound(void);
void MacStartSound(void);
void MacStopSound(void);
// void ConfigureSoundEffects(void);
void PlayAlertSound(void);
bool GetMute(void);
void SetMute(bool mute);
unsigned int GetPlaybackRate(void);
unsigned int GetIntervalMs(void);

#endif