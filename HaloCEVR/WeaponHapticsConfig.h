#pragma once
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "WeaponHandler.h"

#define HAPTICS_DEBUG 1


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

struct PlasmaPistolSettings
{
    int CooldownTicks = 0;
    int RampUpTicks = 0;
    int CooldownTimer = 0;
    int RampUpTimer = 0;
    int CooldownDeadzoneTicks = 0;
    int CooldownGrowthFactor = 0;
    bool isCharging = false;
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
    bool PlasmaPistolCanCharge();
    void WeaponFired(WeaponType Weapon);


protected:
    std::list<WeaponHaptic> hapticList;
    PlasmaPistolSettings plasmaPistolSettings = {};
    WeaponHapticArg GetWeaponHapticArgFromJson(json WeaponHapticArg);

private:

};