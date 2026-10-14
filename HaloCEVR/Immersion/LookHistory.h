#pragma once
#include "InteractionModel.h"

namespace Immersion {
// Halo's script tests read action history, not the render camera. Record only
// real directional motion; never mark the whole tutorial complete on entry.
class LookHistory {
public:
    void Reset() { initialized = false; yawTravel = pitchTravel = stickX = stickY = 0; }
    std::uint32_t Update(Point forward, bool valid, float elapsed) {
        constexpr float pi = 3.14159265358979323846f;
        if (!valid || !forward.Finite() || forward.LengthSquared() < 0.5f ||
            !std::isfinite(elapsed) || elapsed <= 0 || elapsed > 0.25f) { Reset(); return 0; }
        const float yaw = std::atan2(forward.y,forward.x);
        const float pitch = std::atan2(forward.z,std::sqrt(forward.x*forward.x+forward.y*forward.y));
        if (!initialized) { lastYaw = yaw; lastPitch = pitch; initialized = true; return 0; }
        const float dy = std::remainder(yaw-lastYaw,2*pi), dp = pitch-lastPitch;
        lastYaw = yaw; lastPitch = pitch;
        if (std::abs(dy) > pi/3 || std::abs(dp) > pi/3) { Reset(); return 0; }
        auto direction = [](float delta, float& travel, std::uint32_t positive, std::uint32_t negative) {
            // A reversal starts a new stroke; tiny tracking jitter cannot add
            // up across alternating directions. Three degrees is deliberate.
            if (delta*travel < 0) travel = 0;
            travel += delta;
            if (std::abs(travel) < 0.0523598776f) return std::uint32_t{0};
            const auto bit = travel > 0 ? positive : negative;
            travel = 0; return bit;
        };
        return direction(dp,pitchTravel,0x80,0x100) | direction(dy,yawTravel,0x200,0x400);
    }
    std::uint32_t UpdateStick(float x,float y,bool valid,float elapsed){
        if(!valid || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(elapsed) || elapsed<=0 || elapsed>.25f){
            stickX=stickY=0;return 0;
        }
        auto axis=[&](float value,float& travel,std::uint32_t positive,std::uint32_t negative){
            if(std::fabs(value)<.4f){travel=0;return std::uint32_t{0};}
            if(value*travel<0)travel=0;
            travel+=std::copysign(elapsed,value);
            if(std::fabs(travel)<.08f)return std::uint32_t{0};
            travel=0;return value>0?positive:negative;
        };
        // Positive stick X turns right; positive Y means look up. Recording Y
        // does not pitch the VR view or restore the old stick grenade binding.
        return axis(x,stickX,0x400,0x200)|axis(y,stickY,0x80,0x100);
    }
private:
    float stickX=0,stickY=0;
    bool initialized = false;
    float lastYaw = 0, lastPitch = 0, yawTravel = 0, pitchTravel = 0;
};
}
