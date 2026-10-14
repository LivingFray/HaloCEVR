#pragma once
#include "InteractionModel.h"
namespace Immersion {
// Trigger angles are radians. Keep a useful cone, not a zero-spread laser.
inline float ARSpreadScale(float value){
    return std::isfinite(value)?(std::max)(.35f,(std::min)(value,1.f)):.65f;
}
struct ARSpreadPatch {
    Object tag=None;std::uint32_t data=0,trigger=0;
    std::array<float,3> original{},tuned{};bool applied=false;
};
}
