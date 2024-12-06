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

WeaponHaptic WeaponHapticsConfigManager::GetWeaponHaptics(WeaponType Weapon)
{
	WeaponHaptic haptic;
	haptic.Weapon = WeaponType::Unknown;
	haptic.StartSecondsDelay = 0;
	haptic.DurationSeconds = 1;
	haptic.Description = "Unknown";
	haptic.Frequency = 30;
	haptic.Amplitude = .5;

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