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
			plasmaPistolSettings.CooldownMs = std::chrono::milliseconds(plasmaPistol["CooldownMs"]);
			plasmaPistolSettings.RampUpMs = std::chrono::milliseconds(plasmaPistol["RampUpMs"]);
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

		Logger::log << "[WeaponHapticsConfig]: Plasma Pistol Cooldown:" << plasmaPistolSettings.CooldownMs.count() << std::endl;


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
	auto now = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = now - plasmaPistolSettings.LastFire;

	bool canCharge = elapsed_seconds >= plasmaPistolSettings.CooldownMs;

#if HAPTICS_DEBUG
	Logger::log << "[WeaponHapticsConfig]: Checking if plasma pistol can fire" << std::endl;
	Logger::log << "[WeaponHapticsConfig]: duration was: " << elapsed_seconds.count() << std::endl;
	Logger::log << "[WeaponHapticsConfig]: cooldown: " << plasmaPistolSettings.CooldownMs.count() << std::endl;
	Logger::log << "[WeaponHapticsConfig]: can charge " << canCharge << std::endl;
#endif

	if (canCharge && plasmaPistolSettings.isCharging == false)
	{
		plasmaPistolSettings.isCharging = true;
		plasmaPistolSettings.ChargingTime = now;
	}

	return canCharge;
}

void WeaponHapticsConfigManager::WeaponFired(WeaponType Weapon)
{
	if (Weapon == WeaponType::PlasmaPistol)
	{
		Logger::log << "[WeaponHapticsConfig]: settings weapon fired" << std::endl;

		plasmaPistolSettings.LastFire = std::chrono::system_clock::now();
	}

	plasmaPistolSettings.isCharging = false;
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
		auto now = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = now - plasmaPistolSettings.ChargingTime;

		long long chargeDuration = std::min<long long>(plasmaPistolSettings.RampUpMs.count(), elapsed_seconds.count());

		double percentageOfCharge = chargeDuration / plasmaPistolSettings.RampUpMs.count();

		WeaponHapticArg plasmaPistolHaptics = {};
		WeaponHapticTwoHand plasmaPistolTwoHands = {};

		plasmaPistolHaptics.Amplitude = haptic.OneHand.Amplitude * percentageOfCharge;
		plasmaPistolHaptics.DurationSeconds = haptic.OneHand.DurationSeconds;
		plasmaPistolHaptics.Frequency = haptic.OneHand.Frequency * percentageOfCharge;
		plasmaPistolHaptics.StartSecondsDelay = 0;

		plasmaPistolTwoHands.Dominant.Amplitude = haptic.TwoHand.Dominant.Amplitude * percentageOfCharge;
		plasmaPistolTwoHands.Dominant.DurationSeconds = haptic.TwoHand.Dominant.DurationSeconds;
		plasmaPistolTwoHands.Dominant.Frequency = haptic.TwoHand.Dominant.Frequency * percentageOfCharge;
		plasmaPistolTwoHands.Dominant.StartSecondsDelay = haptic.TwoHand.Dominant.StartSecondsDelay;

		plasmaPistolTwoHands.Nondominant.Amplitude = haptic.TwoHand.Nondominant.Amplitude * percentageOfCharge;
		plasmaPistolTwoHands.Nondominant.DurationSeconds = haptic.TwoHand.Nondominant.DurationSeconds;
		plasmaPistolTwoHands.Nondominant.Frequency = haptic.TwoHand.Nondominant.Frequency * percentageOfCharge;
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