#include "WeaponHapticsConfig.h"
#include <../../../ThirdParty/nlohmann/json.hpp>
#include "Logger.h"
using json = nlohmann::json;

WeaponHapticsConfigManager::WeaponHapticsConfigManager()
{
	Logger::log << "[WeaponHapticsConfig] Initializing" << std::endl;

	std::ifstream ifs("VR/haptics.json");
	json jf = json::parse(ifs);

	json haptics = jf["Haptics"];

	std::list<WeaponHaptic> hapticList = {};

	for (const auto& item : haptics.items())
	{
		Logger::log << "[WeaponHapticsConfig] Found Item " << item.key() << std::endl;
		Logger::log << "[WeaponHapticsConfig] Item is weapon type" << item.value() << std::endl;

		WeaponHaptic haptic;
		haptic.Weapon = item.value()["Weapon"];
		haptic.Description = item.value()["Description"];
		haptic.StartSecondsDelay = item.value()["StartSecondsDelay"];
		haptic.DurationSeconds = item.value()["DurationSeconds"];
		haptic.Frequency = item.value()["Frequency"];
		haptic.Amplitude = item.value()["Amplitude"];

		hapticList.push_back(haptic);

	}

	for (WeaponHaptic haptic : hapticList)
	{
		Logger::log << "[WeaponHapticsConfig] Haptic Item " << static_cast<int>(haptic.Weapon) << " (" << haptic.Description << "; Amplitude:" << static_cast<float>(haptic.Amplitude) << ")" << std::endl;

	}
}