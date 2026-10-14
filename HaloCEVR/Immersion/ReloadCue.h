#pragma once
#include "InteractionModel.h"
#include <array>
#include <vector>
namespace Immersion {
enum class ReloadCue { Eject, Grab, Insert, ActionGrab, Pull, Close, Drop, Count };
struct ReloadSoundSlice { unsigned part=0, parts=1; };
struct ReloadPcm {
    unsigned rate=0,channels=0;
    std::vector<std::int16_t> samples;
};
struct ReloadFeedbackAssets {
    std::array<Object,static_cast<unsigned>(ReloadCue::Count)> sounds{};
    std::array<Object,16> pumpSounds{};
    unsigned pumpSoundCount=0;
    std::array<ReloadSoundSlice,static_cast<unsigned>(ReloadCue::Count)> slices{};
    Object chamberEffect=None;
    bool chamberIsSound=false;
    ReloadFeedbackAssets(){sounds.fill(None);pumpSounds.fill(None);}
    bool IsPumpSound(Object sound)const {
        for(unsigned i=0;i<pumpSoundCount;++i)if(pumpSounds[i]==sound)return true;
        return false;
    }
};
}
