// Production native adapter with only x86 call thunks and process-memory IO
// replaced. Actual instruction ranges are checked by verify_retail_profile.py.
#include "HaloCEVR/ReloadAudio.h"
#include "HaloCEVR/Immersion/RetailProfile.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#define __cdecl
using namespace Immersion;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
struct Game {
    static Game instance;
    // INSERT_PRODUCTION_RELOAD_ACCESSORS
    void SetReloadEnabledForTest(bool value){reloadHandler.enabled=value;}
protected:
    // Keep the actual Game member's access level in this compile fixture.
    struct Reload {
        bool enabled=true;
        bool PumpSupportsWeapon()const{return false;}
        bool DeferPumpSound(Object w,Object s)const{return enabled && w==11 && s==80;}
        bool DeferPumpEffect(Object w,Object e)const{return enabled && w==11 && e==90;}
    } reloadHandler;
};Game Game::instance;
static bool fingerprintPass=true,rejectPlayback=false;
static int failCall=0,calls=0,removed=0,disabled=0;
constexpr int MH_OK=0;
int MH_CreateHook(void*,void*,void** original){*original=reinterpret_cast<void*>(0x10000);return ++calls==failCall?1:0;}
int MH_EnableHook(void*){return ++calls==failCall?1:0;}
int MH_RemoveHook(void*){++removed;return 0;}
int MH_DisableHook(void*){++disabled;return 0;}
static std::vector<Object> stopped,played,effects;
void NativeStop(Object handle){stopped.push_back(handle);}
Object NativeSound(Object weapon,Object sound,float volume){CHECK(weapon==11);CHECK(volume>0);played.push_back(sound);return rejectPlayback?None:81;}
void NativeEffect(Object weapon,Object effect){CHECK(weapon==11);effects.push_back(effect);}
void StopReloadPcm(){}
// INSERT_PRODUCTION_AUDIO
void reset(){ready=attempted=false;playback=None;fingerprintPass=true;failCall=calls=removed=disabled=0;stopped.clear();played.clear();effects.clear();}
int main(){
    reset();fingerprintPass=false;CHECK(!InitializeReloadAudio());CHECK(calls==0);CHECK(!InitializeReloadAudio());
    for(int failure:{1,2,3,4}){reset();failCall=failure;CHECK(!InitializeReloadAudio());CHECK(!ready);CHECK(!PlayNativeReloadSound(11,80,.6f));
        CHECK(played.empty());CHECK(!PlayNativeReloadEffect(11,90));CHECK(effects.empty());
        if(failure>=3){CHECK(removed==2);CHECK(disabled==2);}else CHECK(removed==(failure==2?1:0));}
    reset();CHECK(InitializeReloadAudio());CHECK(calls==4);CHECK(InitializeReloadAudio());CHECK(calls==4);
    CHECK(DeferSound(11,80,0x4932ab));CHECK(DeferSound(11,80,0x492ac9));CHECK(!DeferSound(11,80,0x4c486b)); // Blast/effect caller preserved.
    CHECK(!DeferSound(22,80,0x4932ab));CHECK(!DeferSound(11,81,0x4932ab));
    CHECK(DeferEffect(11,90));CHECK(!DeferEffect(11,91));CHECK(!DeferEffect(22,90));
    Game::instance.SetReloadEnabledForTest(false);CHECK(!DeferSound(11,80,0x4932ab));CHECK(!DeferEffect(11,90));
    CHECK(PlayNativeReloadSound(11,80,.6f));CHECK(played.size()==1);CHECK(PlayNativeReloadSound(11,82,.6f));CHECK(stopped.size()==1);
    CHECK(PlayNativeReloadSound(11,82,0));CHECK(played.size()==2);CHECK(!PlayNativeReloadSound(11,None,.6f));
    StopReloadAudio();CHECK(stopped.size()==2);StopReloadAudio();CHECK(stopped.size()==2);
    manualEffectMuted=true;CHECK(DeferSound(11,80,0x4c486b));manualEffectMuted=false;CHECK(!DeferSound(11,80,0x4c486b));
    CHECK(PlayNativeReloadEffect(11,90));CHECK(!manualEffectMuted);CHECK(effects.size()==1);CHECK(!PlayNativeReloadEffect(11,None));
    rejectPlayback=true;CHECK(!PlayNativeReloadSound(11,80,.6f));StopReloadAudio();CHECK(stopped.size()==2);
    std::cout<<checks<<" native reload audio guard/rollback/lifetime checks passed (native-call doubles)\n";
}
