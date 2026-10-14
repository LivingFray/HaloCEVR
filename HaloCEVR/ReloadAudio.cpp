#include "ReloadAudio.h"
#include "Game.h"
#include "Immersion/RetailProfile.h"
#include <Windows.h>
#include <MinHook.h>

namespace {
using RawFunction=void(*)();
RawFunction originalSound=nullptr,originalEffect=nullptr;
bool ready=false,attempted=false;
thread_local bool manualEffectMuted=false;
Immersion::Object playback=Immersion::None;
bool Fingerprint(const Immersion::CodeFingerprint& f){
    std::array<unsigned char,1024> bytes{};SIZE_T read=0;
    if(f.length>bytes.size() || !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(0x400000+f.rva),
        bytes.data(),f.length,&read) || read!=f.length)return false;
    std::uint32_t hash=2166136261u;for(unsigned i=0;i<f.length;++i)hash=(hash^bytes[i])*16777619u;
    return hash==f.fnv1a;
}
bool __cdecl DeferSound(Immersion::Object weapon,Immersion::Object sound,std::uintptr_t caller){
    // Only first-person animation sounds. The firing-effect blast is untouched.
    return ready && (manualEffectMuted || ((caller==0x4932abu || caller==0x492ac9u) && Game::instance.ShouldDeferManualPumpSound(weapon,sound)));
}
bool __cdecl DeferEffect(Immersion::Object weapon,Immersion::Object effect){
    return ready && Game::instance.ShouldDeferManualPumpEffect(weapon,effect);
}
void __declspec(naked) OnSound(){
    __asm {
        pushfd
        pushad
        mov eax, [esp+36]
        mov ecx, [esp+40]
        mov edx, [esp+4]
        push eax
        push ecx
        push edx
        call DeferSound
        add esp, 12
        test al, al
        jz passSound
        popad
        popfd
        mov eax, 0xffffffff
        ret
    passSound:
        popad
        popfd
        jmp dword ptr [originalSound]
    }
}
void __declspec(naked) OnEffect(){
    __asm {
        pushfd
        pushad
        mov eax, [esp]
        mov ecx, [esp+24]
        push eax
        push ecx
        call DeferEffect
        add esp, 8
        test al, al
        jz passEffect
        popad
        popfd
        mov eax, 0xffffffff
        ret
    passEffect:
        popad
        popfd
        jmp dword ptr [originalEffect]
    }
}
}
bool InitializeReloadAudio(){
    if(attempted)return ready;
    attempted=true;
    using namespace Immersion;
    if(!Fingerprint(RetailNativeSound) || !Fingerprint(RetailNativeSoundStop) || !Fingerprint(RetailWeaponEffect))return false;
    void* sound=reinterpret_cast<void*>(0x543ce0);void* effect=reinterpret_cast<void*>(0x4c47d0);
    if(MH_CreateHook(sound,&OnSound,reinterpret_cast<void**>(&originalSound))!=MH_OK)return false;
    if(MH_CreateHook(effect,&OnEffect,reinterpret_cast<void**>(&originalEffect))!=MH_OK){MH_RemoveHook(sound);return false;}
    if(MH_EnableHook(sound)!=MH_OK || MH_EnableHook(effect)!=MH_OK){
        MH_DisableHook(sound);MH_DisableHook(effect);MH_RemoveHook(sound);MH_RemoveHook(effect);return false;
    }
    ready=true;return true;
}
void StopNativeReloadSound(){
    if(!ready || playback==Immersion::None)return;
    const std::uintptr_t function=0x549ee0;const auto handle=playback;playback=Immersion::None;
    __asm {mov ecx, handle
        call function}
}
bool PlayNativeReloadSound(Immersion::Object weapon,Immersion::Object sound,float volume){
    if(!ready || sound==Immersion::None)return false;
    if(volume<=0)return true;
    StopNativeReloadSound();Immersion::Object result;
    // Same register/stack ABI as the native first-person sound call at 4932a6.
    __asm {
        push esi
        mov esi, weapon
        push 1
        push volume
        push -1
        push sound
        mov eax, dword ptr ds:[0x696718]
        mov ecx, dword ptr ds:[0x6966f8]
        call dword ptr [originalSound]
        add esp, 16
        mov result, eax
        pop esi
    }
    playback=result;return result!=Immersion::None;
}
bool PlayNativeReloadEffect(Immersion::Object weapon,Immersion::Object effect){
    if(!ready || effect==Immersion::None)return false;
    // A replayed casing effect must not restart a complete pump sound. The
    // manually timed PCM back/forward cues own this sound; blast calls outside
    // this narrow synchronous effect dispatch remain untouched.
    const bool priorMute=manualEffectMuted;manualEffectMuted=true;
    __asm {
        push edi
        mov edi, effect
        mov ecx, weapon
        push 0
        push 0
        call dword ptr [originalEffect]
        add esp, 8
        pop edi
    }
    manualEffectMuted=priorMute;return true;
}

void StopReloadAudio(){StopNativeReloadSound();StopReloadPcm();}
