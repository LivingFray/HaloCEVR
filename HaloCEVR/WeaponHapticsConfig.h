#pragma once
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "WeaponHandler.h"

using json = nlohmann::json;

struct WeaponHapticArg
{
    int StartSecondsDelay;
    float DurationSeconds;
    float Frequency;
    float Amplitude;
};

struct WeaponHapticTwoHand {
    WeaponHapticArg Dominant;
    WeaponHapticArg Nondominant;
};

struct WeaponHaptic
{
    WeaponType Weapon;
    std::string Description;
    WeaponHapticArg OneHand;
    WeaponHapticTwoHand TwoHand;

};

struct WeaponHapticsConfig
{
    std::vector<WeaponHaptic> Haptics;
};

class WeaponHapticsConfigManager {
public:
    // Default constructor
    WeaponHapticsConfigManager();
    WeaponHaptic GetWeaponHaptics(WeaponType Type);

protected:
    std::list<WeaponHaptic> hapticList = {};

    WeaponHapticArg GetWeaponHapticArgFromJson(json WeaponHapticArg);

private:

};