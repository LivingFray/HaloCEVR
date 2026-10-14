#pragma once
#include "InteractionModel.h"
#include <cstring>

namespace Immersion {
enum class ReloadWeaponKind { Native, Pistol, Rifle, Sniper, Shotgun, Rocket, Needler };
inline ReloadWeaponKind ReloadWeaponKindForPath(const char* path) {
    if (std::strstr(path,"\\pistol\\")) return ReloadWeaponKind::Pistol;
    if (std::strstr(path,"\\assault rifle\\")) return ReloadWeaponKind::Rifle;
    if (std::strstr(path,"\\sniper rifle\\")) return ReloadWeaponKind::Sniper;
    if (std::strstr(path,"\\shotgun\\")) return ReloadWeaponKind::Shotgun;
    if (std::strstr(path,"\\rocket launcher\\")) return ReloadWeaponKind::Rocket;
    if (std::strstr(path,"\\needler\\")) return ReloadWeaponKind::Needler;
    return ReloadWeaponKind::Native; // Plasma and unsupported/custom weapons.
}
struct ReloadAmmo {
    Object unit=None, weapon=None, tag=None;
    ReloadWeaponKind kind=ReloadWeaponKind::Native;
    int loaded=0, reserve=0, capacity=0;
    int nativeState=0;
    std::uint32_t tick=0;
};
inline bool ValidReloadAmmo(const ReloadAmmo& a) {
    return a.unit!=None && a.weapon!=None && a.tag!=None && a.kind!=ReloadWeaponKind::Native &&
        a.capacity>0 && a.capacity<=256 && a.loaded>=0 && a.loaded<=a.capacity &&
        a.reserve>=0 && a.reserve<=32767;
}
inline bool MagazineReload(ReloadWeaponKind k) {
    return k==ReloadWeaponKind::Pistol || k==ReloadWeaponKind::Rifle || k==ReloadWeaponKind::Sniper;
}
struct ReloadSockets {
    Point insert{}, action{};
};
inline ReloadSockets DefaultReloadSockets(ReloadWeaponKind kind) {
    switch(kind) {
    case ReloadWeaponKind::Pistol: return {{0.0f,0,-0.09f},{0.04f,0,0.07f}};
    case ReloadWeaponKind::Rifle: return {{-0.10f,0,-0.06f},{0.08f,0.06f,0.07f}};
    case ReloadWeaponKind::Sniper: return {{-0.06f,0,-0.08f},{0.09f,0.05f,0.09f}};
    case ReloadWeaponKind::Shotgun: return {{0.09f,0,0.16f},{0.36f,0,0}};
    case ReloadWeaponKind::Rocket: return {{0.02f,0,0.13f},{0.10f,0,0.17f}};
    default: return {};
    }
}
// Local to the actual gun, not the world: a short slide-length capsule with
// a top-side gate. Handle support below the slide remains a separate gesture.
inline bool NearPistolSlide(Point fromSlide,float radius) {
    if(!fromSlide.Finite() || !std::isfinite(radius) || radius<=0 || fromSlide.z<-.035f)return false;
    const float along=(std::max)(-.065f,(std::min)(.065f,fromSlide.x));
    return Within(fromSlide,{along,0,0},radius);
}
inline bool NearPistolMagazinePort(Point hand,Point port,Point up,float radius) {
    if(!up.Finite())return false;
    // Widen the approach and extend it 7 cm below the well. This lets the
    // magazine latch before physical controller housings collide.
    const float below=(std::max)(0.f,(std::min)(.07f,(port-hand).Dot(up)));
    return Within(hand,port-up*below,(std::max)(radius,.18f));
}
// Persistent for a weapon datum across stows and hand changes. Native ammunition
// is the authority. The caller commits this candidate only after a checked write.
struct ReloadState {
    Object weapon=None, tag=None;
    ReloadWeaponKind kind=ReloadWeaponKind::Native;
    bool magazine=true, needsRack=false, open=false, spentShell=false;
    int spent=0, observedLoaded=0;
    void Observe(const ReloadAmmo& ammo) {
        if (weapon!=ammo.weapon || tag!=ammo.tag) {
            *this={};weapon=ammo.weapon;tag=ammo.tag;kind=ammo.kind;
            needsRack=ammo.loaded==0 && kind!=ReloadWeaponKind::Rocket;
            spent=kind==ReloadWeaponKind::Rocket ? ammo.capacity-ammo.loaded : 0;
        } else if (ammo.loaded<observedLoaded) {
            if (kind==ReloadWeaponKind::Shotgun) spentShell=true;
            if (kind==ReloadWeaponKind::Shotgun || ammo.loaded==0) needsRack=kind!=ReloadWeaponKind::Rocket;
            if (kind==ReloadWeaponKind::Rocket) spent=(std::min)(ammo.capacity,spent+observedLoaded-ammo.loaded);
        }
        observedLoaded=ammo.loaded;
    }
    bool BlocksFire()const {return !magazine || needsRack || open;}
    bool RecoverUnseatedRounds(ReloadAmmo& ammo) {
        // Halo may change ammo while the object is dropped/reacquired or an
        // already-running native reload finishes. Ammo cannot stay loaded in
        // our absent magazine: return it to reserve before the next hand grab.
        // Commit this candidate through the same checked write as insertion.
        if (!MagazineReload(kind) || magazine || ammo.loaded<=0 || ammo.loaded+ammo.reserve>32767) return false;
        ammo.reserve+=ammo.loaded;ammo.loaded=0;observedLoaded=0;
        return true; // Preserve the original tactical/empty chamber state.
    }
    bool Eject(ReloadAmmo& ammo) {
        if (!MagazineReload(kind) || !magazine || ammo.loaded+ammo.reserve>32767) return false;
        needsRack=needsRack || ammo.loaded==0;
        // Retain remaining rounds in reserve; an accidental reload never destroys ammo.
        ammo.reserve+=ammo.loaded;ammo.loaded=0;observedLoaded=0;magazine=false;return true;
    }
    int Insert(ReloadAmmo& ammo,int shellRatio) {
        if (ammo.reserve<=0 || ammo.loaded>=ammo.capacity ||
            (MagazineReload(kind) && magazine) || (kind==ReloadWeaponKind::Rocket && (!open || ammo.loaded+spent>=ammo.capacity))) return 0;
        const int request=kind==ReloadWeaponKind::Shotgun ? (std::max)(1,(std::min)(4,shellRatio)) :
            kind==ReloadWeaponKind::Rocket ? 1 : ammo.capacity;
        const int count=(std::min)(request,(std::min)(ammo.reserve,ammo.capacity-ammo.loaded));
        ammo.loaded+=count;ammo.reserve-=count;observedLoaded=ammo.loaded;
        if (MagazineReload(kind)) magazine=true;
        return count;
    }
    bool Rack(const ReloadAmmo& ammo) {
        if (kind==ReloadWeaponKind::Rocket || !magazine || (ammo.loaded<=0 && kind!=ReloadWeaponKind::Shotgun)) return false;
        needsRack=ammo.loaded==0;return true;
    }
};
enum class ReloadTool { None, Magazine, Shell, Rocket, SpentRocket, Action };
// A complete backward-then-forward stroke is required. Lateral/upward movement
// and moving both controllers together do not cock the action.
struct ReloadStroke {
    Point start{};
    bool pulled=false;
    float travel=0;
    bool Update(Point local,float length,float returnFraction=.30f) {
        const auto d=local-start;
        if (!d.Finite() || d.y*d.y+d.z*d.z>0.09f) return false;
        travel=(std::max)(0.0f,(std::min)(length,-d.x));
        if (-d.x>=length) pulled=true;
        return pulled && -d.x<=length*returnFraction;
    }
};
}
