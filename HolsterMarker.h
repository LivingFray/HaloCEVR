#pragma once
#include "InteractionModel.h"
#include <cstring>

namespace Immersion {
struct HolsterMarker {
    Object weapon = None;
    bool equipped = false;
};
// Asset paths come from the loaded map. Unknown/custom models get the compact
// long-gun scale too, rather than disappearing through a weapon whitelist.
inline float HolsterMarkerScale(const char* path, unsigned home) {
    const bool compactWeapon = path && (std::strstr(path,"pistol") ||
        std::strstr(path,"magnum") || std::strstr(path,"needler"));
    return (compactWeapon ? 0.65f : 0.30f) * (home == 0 ? 0.60f : 1.0f);
}
inline Point HolsterMarkerAnchor(unsigned home, const Layout& layout, Point head, float yaw) {
    if (home == 1) return layout.Anchor(Zone::WeaponHip,head,yaw);
    // Display for the shoulder slot only. The grab zone stays at the shoulder.
    return head + Point{0.21f*std::cos(yaw),0.21f*std::sin(yaw),-0.43f};
}
inline const char* HolsterLabel(unsigned home, bool leftHanded) {
    return home==0 ? (leftHanded ? "Left Shoulder" : "Right Shoulder") :
        (leftHanded ? "Left Hip (Here)" : "Right Hip (Here)");
}
}
