#pragma once
#include "Immersion/ReloadCue.h"
// Retail native audio/effect adapter and queued playback of copied game PCM.
// No synthesized tones or bundled recordings.
bool InitializeReloadAudio();
bool PlayNativeReloadSound(Immersion::Object weapon,Immersion::Object sound,float volume);
bool PlayNativeReloadEffect(Immersion::Object weapon,Immersion::Object effect);
void StopReloadAudio();

void StopNativeReloadSound();
bool QueueReloadPcm(const Immersion::ReloadPcm& pcm,float volume);
void UpdateReloadPcm();
void StopReloadPcm();
