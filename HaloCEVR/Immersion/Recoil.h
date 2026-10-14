#pragma once
#include "InteractionModel.h"

namespace Immersion {
// Retail 1.10 firing observations, read around the existing FireWeapon hook.
// No ammunition, weapon tags, animations or player-control state are written.
struct FireState {
    Object unit = None, weapon = None;
    std::uint32_t tick = 0;
    std::array<std::int16_t,2> loaded{};
    float heat = 0, energyUsed = 0;
};
inline bool FiredBetween(const FireState& before, const FireState& after) {
    if (before.unit == None || before.weapon == None || before.unit != after.unit || before.weapon != after.weapon) return false;
    // Tick changes cover energy guns and infinite ammo. Resource changes also
    // cover another discharge in the same simulation tick (including tick 0).
    return before.tick != after.tick || after.loaded[0] < before.loaded[0] ||
        after.loaded[1] < before.loaded[1] || after.heat > before.heat || after.energyUsed > before.energyUsed;
}
struct RecoilProfile {
    float pitchDegrees, backMetres, recoverySeconds, limitDegrees;
};
struct RecoilPose { float pitch = 0, back = 0; };

// One native held weapon at this checkpoint. Full salted handles prevent a
// reused object-table index or another gun inheriting the previous kick.
class Recoil {
public:
    void Reset() { *this = {}; }
    void Context(Object owner, Object held) {
        if (owner != unit || held != weapon) { Reset(); unit=owner; weapon=held; }
    }
    void Advance(double now) {
        if (!std::isfinite(now)) { Reset(); return; }
        if (clockValid) {
            if (now < time) pose = {};
            else {
                const float decay = static_cast<float>(std::exp(-(now-time)/recovery));
                pose.pitch *= decay; pose.back *= decay;
            }
        }
        time=now; clockValid=true;
    }
    void Shot(const RecoilProfile& profile, bool supported, float strength, float supportedFraction, double now) {
        Advance(now);
        if (unit == None || weapon == None || !std::isfinite(now)) return;
        strength = Bounded(strength,1.0f,0.0f,3.0f);
        supportedFraction = Bounded(supportedFraction,0.35f,0.0f,1.0f);
        const float scale = strength * (supported ? supportedFraction : 1.0f);
        recovery = Bounded(profile.recoverySeconds,0.20f,0.05f,1.0f);
        const float pitch = Bounded(profile.pitchDegrees,2.0f,0.0f,12.0f);
        const float back = Bounded(profile.backMetres,0.02f,0.0f,0.08f);
        const float limit = Bounded(profile.limitDegrees,12.0f,0.0f,20.0f);
        // Cap accumulation, but changing support must not abruptly erase an
        // earlier unsupported kick. That residual still returns naturally.
        pose.pitch += (std::min)(pitch*scale,(std::max)(0.0f,limit*scale-pose.pitch));
        pose.back += (std::min)(back*scale,(std::max)(0.0f,0.10f*scale-pose.back));
    }
    RecoilPose Pose(Object held) const { return held == weapon ? pose : RecoilPose{}; }
private:
    static float Bounded(float value,float fallback,float low,float high) {
        return (std::max)(low,(std::min)(high,std::isfinite(value)?value:fallback));
    }
    Object unit=None, weapon=None;
    RecoilPose pose{};
    double time=0;
    float recovery=0.20f;
    bool clockValid=false;
};
}
