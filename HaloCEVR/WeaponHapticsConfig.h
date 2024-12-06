#pragma once
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "WeaponHandler.h"

using json = nlohmann::json;

struct WeaponHaptic
{
    WeaponType Weapon;
    std::string Description;
    int StartSecondsDelay; 
    float DurationSeconds;
    float Frequency; 
    float Amplitude;
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

private:

};