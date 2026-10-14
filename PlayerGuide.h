#pragma once
#include <array>
namespace StylishGuide {
struct Page {const char* title;std::array<const char*,4> steps;};
inline constexpr std::array<Page,10> Pages{{
    {"QUEST 3 / BUTTONS",{{
        "RT / LT: fire with the hand holding the gun.",
        "B: interact. B also starts reloads in right-handed mode.",
        "Left-handed mode: tap Y to reload. Hold Y for 5 seconds to recenter instead.",
        "Menu: hold X, then press RT. In the Ghost, tap Y instead."}}},
    {"GRIPS / HOLSTERS",{{
        "Hold grip to keep a weapon. Either hand can grab it. Let go away from a holster to drop it.",
        "Right-handed layout: weapon holsters at right hip and right shoulder. Left-handed mode mirrors them.",
        "Release grip in a weapon holster to stow. Grip at a weapon pickup prompt to pick it up.",
        "Hold the other grip at the support point for less recoil. The Magazine Zone is on the opposite hip."}}},
    {"MOVEMENT / COMFORT",{{
        "Left stick: move. Right stick: turn. Right A: jump. Crouch physically to duck.",
        "Flashlight: squeeze either grip at the left temple (right-handed mode), or right temple (left-handed mode).",
        "Hold physical left Y for 5 seconds to refresh your standing height. Release before repeating.",
        "Basic: recoil, World Scale and Show Legs (off by default). Try World Scale 0.8."}}},
    {"MAGNUM / AR / SNIPER",{{
        "Press the reload button: B in right-handed mode, or tap Y in left-handed mode, to eject the magazine.",
        "Use your free hand: hold grip at the Magazine Zone to take a fresh magazine.",
        "Bring it to the magazine well until it seats. You can then release the free hand's grip.",
        "If the gun ran empty, grip and pull its slide or charging handle backward, then return or release."}}},
    {"SLIDES / CHARGING HANDLES",{{
        "Magnum: grip over the slide. AR and sniper: grip at the charging handle on the side of the gun.",
        "The free hand latches to the action. Pull backward relative to the gun, not both hands together.",
        "Only the Magnum allows free cycling. AR and sniper handles engage when an empty reload needs charging.",
        "If the gun ran empty, insert ammunition first, then finish a full pull to make it ready."}}},
    {"SHOTGUN",{{
        "After each shot, pull the foregrip back and push it forward. The spent shell ejects on the pull.",
        "For shells, hold free-hand grip at the Magazine Zone. Bring each shell to the top loading port.",
        "If it ran empty, pump after loading. A ready shotgun does not allow extra pumping.",
        "Basic / Shotgun Shell Ratio: choose 1 to 4 rounds per inserted shell, limited by reserve ammo."}}},
    {"ROCKET LAUNCHER",{{
        "Press the reload button to open the casing. Press it again to close; insertion never closes it.",
        "Grip at the open casing to take a spent rocket. Move it away and release grip to discard it.",
        "Grip a fresh rocket at the Magazine Zone. Bring it to the open casing to seat it.",
        "Repeat for the second rocket if needed, then press the reload button to close before firing."}}},
    {"GRENADES / ALIEN WEAPONS",{{
        "Plasma: left chest. Frag: right chest. These sides stay fixed in either weapon-handedness mode.",
        "Either grip takes a grenade. Swing and release to throw; a faster swing throws harder. No pin step.",
        "Release at its chest holster to put it back. Right-stick up no longer throws grenades.",
        "Needler: press the reload button; no auto-reload. Plasma weapons use batteries, with no manual reload."}}},
    {"VEHICLES / TURRETS",{{
        "Warthog driver: RT forward, LT reverse, left stick to steer. B enters or exits vehicles.",
        "Ghost: left stick moves forward/back and strafes. Right stick aims/turns. Either trigger fires. Tap Y for menu.",
        "Warthog gunner and Shade: right stick aims, RT fires. Adjust Turret Sensitivity in Basic.",
        "Ghost Aim Sensitivity is separate from turrets. The default 1.0 is faster than before."}}},
    {"TANK / PELICAN",{{
        "Scorpion: RT fires the main cannon. LT fires the machine gun. A no longer fires the machine gun.",
        "Pelican: look around naturally, or use the right stick to turn and look up/down during the ride.",
        "Pelican stick look changes your view only. The mission script still controls the flight.",
        "After exiting a vehicle, native camera corrections pause briefly while live headset tracking continues."}}}
}};
}
