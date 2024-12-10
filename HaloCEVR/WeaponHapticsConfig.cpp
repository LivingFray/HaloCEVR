#include "WeaponHapticsConfig.h"
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "Logger.h"
using json = nlohmann::json;
namespace fs = std::filesystem;

WeaponHapticsConfigManager::WeaponHapticsConfigManager()
{
	Logger::log << "[WeaponHapticsConfig] Initializing" << std::endl;
	LoadConfig();
}

void WeaponHapticsConfigManager::LoadConfig()
{
	std::string hapticsConfig = "VR/haptics.json";

	fs::path f{ hapticsConfig };

	if (!fs::exists(f))
	{
		Logger::log << "[WeaponHapticsConfig] Haptics file " << hapticsConfig << " was not found." << std::endl;
		return;
	}

	std::filesystem::file_time_type latestVersion = std::filesystem::last_write_time(hapticsConfig);

	bool reload = ReloadOnChange && (Version < latestVersion);

	//Set to true by default, so always runs once.
	if (reload)
	{
		Logger::log << "[WeaponHapticsConfig] Loading Config" << std::endl;

		hapticList = {};
		Version = latestVersion;
		std::ifstream ifs(hapticsConfig);
		json jf = json::parse(ifs);

		try 
		{
			json plasmaPistol = jf["PlasmaPistol"];
			plasmaPistolSettings.CooldownTicks = plasmaPistol["CooldownTicks"];
			plasmaPistolSettings.CooldownDeadzoneTicks = plasmaPistol["CooldownDeadzoneTicks"];
			plasmaPistolSettings.RampUpTicks = plasmaPistol["RampUpTicks"];
			plasmaPistolSettings.CooldownGrowthFactor = plasmaPistol["CooldownGrowthFactor"];
		}
		catch (...)
		{
			Logger::log << "[WeaponHapticsConfig] There was an issue loading PlasmaPistolSettings " << std::endl;
		}

		ReloadOnChange = jf["ReloadOnChange"];

		json haptics = jf["Haptics"];
		for (const auto& item : haptics.items())
		{
			try {
				WeaponHaptic haptic;
				haptic.Weapon = item.value()["Weapon"];
				haptic.Description = item.value()["Description"];

				json oneHandJson = item.value()["OneHand"];
				json twoHandJson = item.value()["TwoHand"];

				json dominant = twoHandJson["Dominant"];
				json nondominant = twoHandJson["Nondominant"];

				haptic.OneHand = GetWeaponHapticArgFromJson(oneHandJson);

				WeaponHapticTwoHand twoHands;
				twoHands.Dominant = GetWeaponHapticArgFromJson(dominant);
				twoHands.Nondominant = GetWeaponHapticArgFromJson(nondominant);

				haptic.TwoHand = twoHands;

				hapticList.push_back(haptic);
			}
			catch (...)
			{
				Logger::log << "[WeaponHapticsConfig] There was an issue parsing haptics item " << item.key() << std::endl;
			}
		}

#if HAPTICS_DEBUG

		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol CooldownTicks:" << plasmaPistolSettings.CooldownTicks << std::endl;
		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol CooldownDeadzoneTicks:" << plasmaPistolSettings.CooldownDeadzoneTicks << std::endl;
		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol RampUpTicks:" << plasmaPistolSettings.RampUpTicks << std::endl;
		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol CooldownGrowthFactor:" << plasmaPistolSettings.CooldownGrowthFactor << std::endl;


		for (WeaponHaptic haptic : hapticList)
		{
			Logger::log << "[WeaponHapticsConfig] Haptic Item " << static_cast<int>(haptic.Weapon) << std::endl;
			Logger::log << "[WeaponHapticsConfig] Description: " << haptic.Description << std::endl;
			Logger::log << "[WeaponHapticsConfig] OneHandAmp: " << haptic.OneHand.Amplitude << std::endl;
			Logger::log << "[WeaponHapticsConfig] TwoHandDominantAmp: " << haptic.TwoHand.Dominant.Amplitude << std::endl;
			Logger::log << "[WeaponHapticsConfig] TwoHandNonDominantAmp: " << haptic.TwoHand.Nondominant.Amplitude << std::endl;
		}
#endif
	}
}


WeaponHapticArg WeaponHapticsConfigManager::GetWeaponHapticArgFromJson(json arg)
{
	WeaponHapticArg haptic;
	haptic.StartSecondsDelay = arg["StartSecondsDelay"];
	haptic.DurationSeconds = arg["DurationSeconds"];
	haptic.Frequency = arg["Frequency"];
	haptic.Amplitude = arg["Amplitude"];
	return haptic;
}


bool WeaponHapticsConfigManager::PlasmaPistolCanCharge()
{
	bool canCharge = false;

	//If we're already charging, of course we can charge.
	if (plasmaPistolSettings.isCharging)
	{
		Logger::log << "[WeaponHapticsConfig]: plasma pistol is charging" << std::endl;

		if (plasmaPistolSettings.RampUpTicks != 0)
		{
			plasmaPistolSettings.RampUpTicks--;
		}

		Logger::log << "[WeaponHapticsConfig]: plasma pistol ramp up: " << plasmaPistolSettings.RampUpTicks << std::endl;

		canCharge = true;
	}
	else
	{
		Logger::log << "[WeaponHapticsConfig]: plasma pistol is not charging." << std::endl;

		if (plasmaPistolSettings.CooldownTimer != 0)
		{
			Logger::log << "[WeaponHapticsConfig]: plasma pistol cannot charge" << std::endl;
			Logger::log << "[WeaponHapticsConfig]: plasma pistol charge cooldown: " << plasmaPistolSettings.CooldownTimer << std::endl;
			plasmaPistolSettings.CooldownTimer--;
			canCharge = false;
		}
		else 
		{
			plasmaPistolSettings.isCharging = true;
			Logger::log << "[WeaponHapticsConfig]: plasma pistol can charge" << std::endl;
			canCharge = true;
		}
	}

#if HAPTICS_DEBUG
	Logger::log << "[WeaponHapticsConfig]: Checking if plasma pistol can fire" << std::endl;
	Logger::log << "[WeaponHapticsConfig]: CooldownTimer: " << plasmaPistolSettings.CooldownTimer << std::endl;
	Logger::log << "[WeaponHapticsConfig]: can charge " << canCharge << std::endl;
#endif
	
	return canCharge;
}

void WeaponHapticsConfigManager::WeaponFired(WeaponType Weapon)
{
	if (Weapon == WeaponType::PlasmaPistol)
	{
		Logger::log << "[WeaponHapticsConfig]: settings weapon fired" << std::endl;

		int totalTimeCharging = plasmaPistolSettings.RampUpTicks - plasmaPistolSettings.RampUpTimer;

		if (totalTimeCharging >= plasmaPistolSettings.CooldownDeadzoneTicks)
		{
			Logger::log << "[WeaponHapticsConfig]: settings cooldown timer" << std::endl;

			plasmaPistolSettings.CooldownTimer = plasmaPistolSettings.CooldownTicks * (1 + totalTimeCharging / plasmaPistolSettings.RampUpTicks) ^ 5;
			Logger::log << "[WeaponHapticsConfig]: settings cooldown timer:" << plasmaPistolSettings.CooldownTimer <<  std::endl;

		}
		else 
		{
			plasmaPistolSettings.CooldownTimer = 0;
		}

		plasmaPistolSettings.isCharging = false;
	}

}

WeaponHaptic WeaponHapticsConfigManager::GetWeaponHaptics(WeaponType Weapon)
{
	LoadConfig();
	WeaponHaptic haptic;
	haptic.Weapon = WeaponType::Unknown;

	WeaponHapticArg defaultHaptics = {};
	WeaponHapticTwoHand twoHandDefault = {};

	defaultHaptics.Amplitude = 0;
	defaultHaptics.DurationSeconds = 0;
	defaultHaptics.Frequency = 0;
	defaultHaptics.StartSecondsDelay = 0;

	haptic.OneHand = defaultHaptics;
	haptic.TwoHand = twoHandDefault;
	haptic.TwoHand.Dominant = defaultHaptics;
	haptic.TwoHand.Nondominant = defaultHaptics;

	for (WeaponHaptic currentHaptic : hapticList)
	{
		if (currentHaptic.Weapon == Weapon)
		{
			haptic = currentHaptic;
			break;
		}
	}

	if (Weapon == WeaponType::PlasmaPistol)
	{
		int rampUpTime = plasmaPistolSettings.RampUpTicks - plasmaPistolSettings.RampUpTimer;

		double percentageOfChange = std::max(.2, (double) rampUpTime / plasmaPistolSettings.RampUpTicks);

		WeaponHapticArg plasmaPistolHaptics = {};
		WeaponHapticTwoHand plasmaPistolTwoHands = {};

		plasmaPistolHaptics.Amplitude = haptic.OneHand.Amplitude * percentageOfChange;
		plasmaPistolHaptics.DurationSeconds = haptic.OneHand.DurationSeconds;
		plasmaPistolHaptics.Frequency = haptic.OneHand.Frequency * percentageOfChange;
		plasmaPistolHaptics.StartSecondsDelay = 0;

		plasmaPistolTwoHands.Dominant.Amplitude = haptic.TwoHand.Dominant.Amplitude * percentageOfChange;
		plasmaPistolTwoHands.Dominant.DurationSeconds = haptic.TwoHand.Dominant.DurationSeconds;
		plasmaPistolTwoHands.Dominant.Frequency = haptic.TwoHand.Dominant.Frequency * percentageOfChange;
		plasmaPistolTwoHands.Dominant.StartSecondsDelay = haptic.TwoHand.Dominant.StartSecondsDelay;

		plasmaPistolTwoHands.Nondominant.Amplitude = haptic.TwoHand.Nondominant.Amplitude * percentageOfChange;
		plasmaPistolTwoHands.Nondominant.DurationSeconds = haptic.TwoHand.Nondominant.DurationSeconds;
		plasmaPistolTwoHands.Nondominant.Frequency = haptic.TwoHand.Nondominant.Frequency * percentageOfChange;
		plasmaPistolTwoHands.Nondominant.StartSecondsDelay = haptic.TwoHand.Nondominant.StartSecondsDelay;

		WeaponHaptic plasmaPistolAdjusted = {};
		plasmaPistolAdjusted.Description = haptic.Description;
		plasmaPistolAdjusted.Weapon = haptic.Weapon;
		plasmaPistolAdjusted.OneHand = plasmaPistolHaptics;
		plasmaPistolAdjusted.TwoHand = plasmaPistolTwoHands;

		haptic = plasmaPistolAdjusted;
	}

	return haptic;

}