#pragma once
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "WeaponHandler.h"

#define HAPTICS_DEBUG 0


using json = nlohmann::json;

struct WeaponHapticArg
{
    float StartSecondsDelay;
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
    bool ReloadOnChange = true;
};

class WeaponHapticsConfigManager {
public:
    // Default constructor
    WeaponHapticsConfigManager();
    WeaponHaptic GetWeaponHaptics(WeaponType Type);
    void LoadConfig();
    std::filesystem::file_time_type Version;
    bool ReloadOnChange = true;

protected:
    std::list<WeaponHaptic> hapticList;

    WeaponHapticArg GetWeaponHapticArgFromJson(json WeaponHapticArg);

private:

};