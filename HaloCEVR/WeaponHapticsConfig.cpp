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
	std::string hapticsConfig = "VR/OpenVR/haptics.json";

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
			plasmaPistolSettings.RampUpTicks = plasmaPistol["RampUpTicks"];
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

		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol RampUpTicks:" << plasmaPistolSettings.RampUpTicks << std::endl;

		for (WeaponHaptic haptic : hapticList)
		{
			Logger::log << "[WeaponHapticsConfig] Haptic Item " << static_cast<int>(haptic.Weapon) << std::endl;
			Logger::log << "[WeaponHapticsConfig] UsePulse: " << haptic.OneHand.UsePulse << std::endl;
			Logger::log << "[WeaponHapticsConfig] UsePulse Dominant: " << haptic.TwoHand.Dominant.UsePulse << std::endl;

			//Logger::log << "[WeaponHapticsConfig] OneHandAmp: " << haptic.OneHand.Amplitude << std::endl;
			//Logger::log << "[WeaponHapticsConfig] TwoHandDominantAmp: " << haptic.TwoHand.Dominant.Amplitude << std::endl;
			//Logger::log << "[WeaponHapticsConfig] TwoHandNonDominantAmp: " << haptic.TwoHand.Nondominant.Amplitude << std::endl;
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
	haptic.UsePulse = arg["UsePulse"];
	haptic.PulseMicroseconds = arg["PulseMicroseconds"];

	return haptic;
}

void WeaponHapticsConfigManager::SetPlasmaPistolCharging()
{
	if (plasmaPistolSettings.isCharging == false)
	{
		plasmaPistolSettings.isCharging = true;
#if HAPTICS_DEBUG

		Logger::log << "[WeaponHapticsConfig]: Settings plasma pistol charging to true" << std::endl;
#endif
	}
}



bool WeaponHapticsConfigManager::IsPlasmaPistolCharging()
{
	if (plasmaPistolSettings.isCharging == true && plasmaPistolSettings.RampUpTimer < plasmaPistolSettings.RampUpTicks)
	{
		plasmaPistolSettings.RampUpTimer++;
#if HAPTICS_DEBUG

		Logger::log << "[WeaponHapticsConfig]: PlasmaPistolIsCharging" << std::endl;
		Logger::log << "[WeaponHapticsConfig]: RampUpTimer" << plasmaPistolSettings.RampUpTimer << std::endl;
#endif
	}
	else if (plasmaPistolSettings.isCharging == true && plasmaPistolSettings.RampUpTimer == plasmaPistolSettings.RampUpTicks)
	{
#if HAPTICS_DEBUG
		Logger::log << "[WeaponHapticsConfig]: PlasmaPistolAtFullCharge" << std::endl;
#endif
	}

	return plasmaPistolSettings.isCharging;
}

void WeaponHapticsConfigManager::WeaponFired(WeaponType Weapon)
{
#if HAPTICS_DEBUG
	Logger::log << "[WeaponHapticsConfig]: Weapon fired" << std::endl;
#endif 

	if (plasmaPistolSettings.isCharging == true)
	{
		plasmaPistolSettings.isCharging = false;
		plasmaPistolSettings.RampUpTimer = 0;
#if HAPTICS_DEBUG
		Logger::log << "[WeaponHapticsConfig]: Settings plasma pistol charging to false" << std::endl;
#endif

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
		if (plasmaPistolSettings.isCharging == false)
		{
			plasmaPistolSettings.isCharging = true;
			plasmaPistolSettings.RampUpTimer = 0;
		}

		double percentage = plasmaPistolSettings.RampUpTimer / plasmaPistolSettings.RampUpTicks;
	
		double percentageOfChange = std::max(.2, percentage);

		WeaponHapticArg plasmaPistolHaptics = {};
		WeaponHapticTwoHand plasmaPistolTwoHands = {};

		plasmaPistolHaptics.UsePulse = haptic.OneHand.UsePulse;
		plasmaPistolHaptics.PulseMicroseconds = haptic.OneHand.PulseMicroseconds;

		plasmaPistolHaptics.Amplitude = haptic.OneHand.Amplitude * 1/percentageOfChange;
		plasmaPistolHaptics.DurationSeconds = haptic.OneHand.DurationSeconds;
		plasmaPistolHaptics.Frequency = haptic.OneHand.Frequency * percentageOfChange;
		plasmaPistolHaptics.StartSecondsDelay = 0;

		plasmaPistolTwoHands.Dominant.Amplitude = haptic.TwoHand.Dominant.Amplitude * 1/percentageOfChange;
		plasmaPistolTwoHands.Dominant.DurationSeconds = haptic.TwoHand.Dominant.DurationSeconds;
		plasmaPistolTwoHands.Dominant.Frequency = haptic.TwoHand.Dominant.Frequency * percentageOfChange;
		plasmaPistolTwoHands.Dominant.StartSecondsDelay = haptic.TwoHand.Dominant.StartSecondsDelay;
		plasmaPistolTwoHands.Dominant.UsePulse = haptic.TwoHand.Dominant.UsePulse;
		plasmaPistolTwoHands.Dominant.PulseMicroseconds = haptic.TwoHand.Dominant.PulseMicroseconds;

		plasmaPistolTwoHands.Nondominant.Amplitude = haptic.TwoHand.Nondominant.Amplitude * 1/percentageOfChange;
		plasmaPistolTwoHands.Nondominant.DurationSeconds = haptic.TwoHand.Nondominant.DurationSeconds;
		plasmaPistolTwoHands.Nondominant.Frequency = haptic.TwoHand.Nondominant.Frequency * percentageOfChange;
		plasmaPistolTwoHands.Nondominant.StartSecondsDelay = haptic.TwoHand.Nondominant.StartSecondsDelay;
		plasmaPistolTwoHands.Nondominant.PulseMicroseconds = haptic.TwoHand.Nondominant.PulseMicroseconds;
		plasmaPistolTwoHands.Nondominant.UsePulse = haptic.TwoHand.Nondominant.UsePulse;

		WeaponHaptic plasmaPistolAdjusted = {};
		plasmaPistolAdjusted.Description = haptic.Description;
		plasmaPistolAdjusted.Weapon = haptic.Weapon;
		plasmaPistolAdjusted.OneHand = plasmaPistolHaptics;
		plasmaPistolAdjusted.TwoHand = plasmaPistolTwoHands;

		haptic = plasmaPistolAdjusted;
	}

	return haptic;

}

void WeaponHapticsConfigManager::HandleWeaponHaptics(IVR* vr, ControllerRole role, WeaponHapticArg haptics)
{
	if (!vr)
	{
		return;
	}

	Logger::log << "[Weapon Haptics] Here: use pulse is " << haptics.UsePulse << std::endl;


	if (haptics.UsePulse == true)
	{
		vr->TriggerHapticPulse(role, haptics.PulseMicroseconds);
	}
	else
	{
		vr->TriggerHapticVibration(role, haptics.StartSecondsDelay, haptics.DurationSeconds, haptics.Frequency, haptics.Amplitude);
	}
}