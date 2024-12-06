#include "WeaponHapticsConfig.h"
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "Logger.h"
using json = nlohmann::json;
namespace fs = std::filesystem;

WeaponHapticsConfigManager::WeaponHapticsConfigManager()
{
	Logger::log << "[WeaponHapticsConfig] Initializing" << std::endl;

	std::string hapticsConfig = "VR/haptics.json";

	fs::path f{ hapticsConfig };

	if (!fs::exists(f))
	{
		Logger::log << "[WeaponHapticsConfig] Haptics file " << hapticsConfig << " was not found." << std::endl;
		return;
	}

	std::ifstream ifs("VR/haptics.json");
	json jf = json::parse(ifs);

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
		catch(...)
		{
			Logger::log << "[WeaponHapticsConfig] There was an issue parsing haptics item " << item.key() << std::endl;

		}
	}

	for (WeaponHaptic haptic : hapticList)
	{
		Logger::log << "[WeaponHapticsConfig] Haptic Item " << static_cast<int>(haptic.Weapon) << std::endl;
		Logger::log << "Description: " << haptic.Description << std::endl;
		Logger::log << "OneHandAmp: " << haptic.OneHand.Amplitude << std::endl;
		Logger::log << "TwoHandDominantAmp: " << haptic.TwoHand.Dominant.Amplitude << std::endl;
		Logger::log << "TwoHandNonDominantAmp: " << haptic.TwoHand.Nondominant.Amplitude << std::endl;
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

WeaponHaptic WeaponHapticsConfigManager::GetWeaponHaptics(WeaponType Weapon)
{
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
	return haptic;

}