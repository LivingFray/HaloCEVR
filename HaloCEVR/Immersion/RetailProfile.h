#pragma once
// Derived from the supplied executable; regenerate only after verifying disassembly.
#include <cstdint>
namespace Immersion {
struct CodeFingerprint { std::uint32_t rva, length, fnv1a; };
constexpr CodeFingerprint RetailCode[] = {
    {0x16D6E0u, 0x2A7u, 0xC8F7AB6Bu}, // unit_ready_desired_weapon
    {0x16DEC0u, 0x104u, 0x970EDD5Fu}, // unit_drop_current_weapon
    {0x169970u, 0x2Cu, 0xCFC145F2u}, // unit_inventory_get_weapon
    {0x16ED00u, 0x254u, 0x1363CFB8u}, // unit_drop_item
    {0xF51C0u, 0xF8u, 0xA7D1CDA7u}, // object_set_position
    {0xF7B70u, 0xC3u, 0x1DE6AD80u}, // object_validate_drop_position
    {0x5C319u, 0x11u, 0x0B689C53u}, // game_engine_assignment
    {0x7F390u, 0xF0u, 0xA9B01DECu}, // script_look_history_tests
    {0x727EFu, 0x64u, 0xC9224EF8u}, // native_look_history_update
    {0x16D400u, 0x207u, 0x90A70BE7u}, // unit_add_weapon
    {0x78E00u, 0x106u, 0xA18135DEu}, // player_offer_interaction
    {0x16DA00u, 0x71u, 0x9B51A0B5u}, // unit_can_use_weapon
    {0x72100u, 0x53u, 0x3691012Fu}, // player_control_set_weapon_index
};
// Validate before detouring; the entry itself is patched by MinHook afterward.
constexpr CodeFingerprint RetailPickupEntry =
    {0x788A0u, 0x39Eu, 0x2B9BD32Eu};
// Retained research fingerprint; no visibility detour is installed.
constexpr CodeFingerprint RetailVisibilityEntry =
    {0x93E50u, 0x4Du, 0x50C792F8u};
// Complete native model/remapping/effect refresh; no inline assembly.
constexpr CodeFingerprint RetailViewModelRefresh =
    {0x93C60u, 0x1E7u, 0xE8638AFDu};
constexpr CodeFingerprint RetailPoseEntry =
    {0x92430u, 0x55u, 0xB82607ECu};
constexpr CodeFingerprint RetailWeaponHUDEntry =
    {0xB1E20u, 0x1C5u, 0xAB9DC868u};
constexpr CodeFingerprint RetailModelDraw =
    {0xD6FC0u, 0xD5u, 0x7104452Eu};
constexpr CodeFingerprint RetailObjectLighting =
    {0x10EA00u, 0x4Au, 0x9654CD2Eu};
// Historical R1.28 research fingerprints only. R1.29 removes the PVS detour.
constexpr CodeFingerprint RetailStructureVisibilityEntry={0x1537c0u,0x27u,0x9cf1b7a7u};
constexpr CodeFingerprint RetailStructurePvsBranch={0x153834u,0x50u,0xe7733d33u};
// Interior blocks remain unpatched by the existing FireWeapon entry hook.
constexpr CodeFingerprint RetailFireMagazines =
    {0xC400Du, 0x99u, 0x9E0486C3u};
constexpr CodeFingerprint RetailFireSuccess =
    {0xC4328u, 0x50u, 0x4E14B4F0u};
constexpr CodeFingerprint RetailFireEnergy =
    {0xC4444u, 0xA1u, 0x225934B9u};
// Interior of the already-hooked reload-end routine.
constexpr CodeFingerprint RetailReloadLayout =
    {0xC3923u, 0xAAu, 0x2AD163D3u};
constexpr CodeFingerprint RetailNativeSound =
    {0x143CE0u, 0x9Cu, 0x47FB663Cu};
constexpr CodeFingerprint RetailNativeSoundStop =
    {0x149EE0u, 0x40u, 0x000A3561u};
constexpr CodeFingerprint RetailWeaponEffect =
    {0xC47D0u, 0xC6u, 0xE0D041F7u};
constexpr CodeFingerprint RetailGrenadeAttach =
    {0x16E280u, 0x1BEu, 0x918F6445u}; // grenade attach: inventory count, global projectile reference, native state and attached datum
constexpr CodeFingerprint RetailGrenadeRelease =
    {0x16E473u, 0x31u, 0xEBB0AEA2u}; // grenade release interior: excludes existing hooked entry
constexpr CodeFingerprint RetailObjectNodes =
    {0xF6080u, 0xFCu, 0x8A036D5Cu}; // object marker: dynamic node block at signed offset 0x1f2
constexpr CodeFingerprint RetailSeatCamera =
    {0x168F80u, 0x204u, 0xD304F679u}; // unit camera: parent vehicle, seat index and tag seat block
// Exact alternate hash after HaloCEVR P_DisableCrouchCamera changes only
// 0x568ff0: d9 86 0c 05 00 00 -> d9 05 c0 2a 67 00. No wildcard bytes.
constexpr std::uint32_t RetailSeatCameraCrouchDisabledFNV = 0xDA188F9Eu;
constexpr CodeFingerprint RetailGrenadeCommand =
    {0x71AE0u, 0x2C0u, 0x1D69B3F7u}; // Persistent player grenade type command.
}
