#include "WeaponHandler.h"
#include "Helpers/Objects.h"
#include "Helpers/Camera.h"
#include "Helpers/Assets.h"
#include "Helpers/Maths.h"
#include "Logger.h"
#include "Game.h"
#include "Hooking/SigScanner.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>


namespace
{
	struct VehicleMotionShotOverrideState
	{
		bool active = false;
		Camera savedCamera{};
		Vector3 savedLastLookDir{};

		UnitDynamicObject* ownerUnit = nullptr;
		Vector3 savedOwnerFacingDir{};
		Vector3 savedOwnerFacing{};
		Vector3 savedOwnerDesiredAim{};
		Vector3 savedOwnerAim{};
		Vector3 savedOwnerAimVelocity{};
		Vector3 savedOwnerAim2{};
		Vector3 savedOwnerAim3{};

		BaseDynamicObject* weaponObject = nullptr;
		Vector3 savedWeaponFacingDir{};
		Vector3 savedWeaponUpDirection{};
	};

	VehicleMotionShotOverrideState vehicleMotionShotOverride{};

	bool IsFiniteShotVector(const Vector3& value)
	{
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	void RestoreVehicleMotionShotOverride()
	{
		VehicleMotionShotOverrideState& state =
			vehicleMotionShotOverride;

		if (!state.active)
		{
			return;
		}

		Helpers::GetCamera() =
			state.savedCamera;
		Game::instance.LastLookDir =
			state.savedLastLookDir;

		if (state.ownerUnit)
		{
			state.ownerUnit->facingDir =
				state.savedOwnerFacingDir;
			state.ownerUnit->facing =
				state.savedOwnerFacing;
			state.ownerUnit->desiredAim =
				state.savedOwnerDesiredAim;
			state.ownerUnit->aim =
				state.savedOwnerAim;
			state.ownerUnit->aimVelocity =
				state.savedOwnerAimVelocity;
			state.ownerUnit->aim2 =
				state.savedOwnerAim2;
			state.ownerUnit->aim3 =
				state.savedOwnerAim3;
		}

		if (state.weaponObject)
		{
			state.weaponObject->facingDir =
				state.savedWeaponFacingDir;
			state.weaponObject->upDirection =
				state.savedWeaponUpDirection;
		}

		state.active = false;
		state.ownerUnit = nullptr;
		state.weaponObject = nullptr;
	}

	bool BeginVehicleMotionShotOverride(
		BaseDynamicObject* weaponObject,
		UnitDynamicObject* ownerUnit,
		BaseDynamicObject* parentVehicle)
	{
		RestoreVehicleMotionShotOverride();

		const bool bEnabled =
			Game::instance.c_MotionControlledVehicles &&
			Game::instance.c_MotionControlledVehicles->Value();

		if (!bEnabled ||
			!weaponObject ||
			!ownerUnit ||
			!parentVehicle ||
			parentVehicle->N0000027E !=
				ObjectType::VEHICLE)
		{
			return false;
		}

		Vector3 trackedPosition{};
		Vector3 trackedAim{};
		Vector3 trackedUp{};
		if (!Game::instance.GetVehicleVRAim(
				trackedPosition,
				trackedAim,
				trackedUp) ||
			!IsFiniteShotVector(trackedAim) ||
			trackedAim.lengthSqr() <=
				0.000001f)
		{
			return false;
		}

		trackedAim.normalize();

		if (!IsFiniteShotVector(trackedUp))
		{
			trackedUp =
				Helpers::GetCamera().lookDirUp;
		}

		// Keep the firing basis orthogonal. A nearly parallel up vector can make
		// the temporary weapon transform singular for steep vertical aiming.
		trackedUp =
			trackedUp -
			trackedAim *
				trackedUp.dot(trackedAim);

		if (!IsFiniteShotVector(trackedUp) ||
			trackedUp.lengthSqr() <=
				0.000001f)
		{
			trackedUp =
				Helpers::GetCamera().lookDirUp -
				trackedAim *
					Helpers::GetCamera()
						.lookDirUp
						.dot(trackedAim);
		}

		if (!IsFiniteShotVector(trackedUp) ||
			trackedUp.lengthSqr() <=
				0.000001f)
		{
			trackedUp =
				Vector3(
					0.0f,
					0.0f,
					1.0f) -
				trackedAim *
					trackedAim.z;
		}

		if (!IsFiniteShotVector(trackedUp) ||
			trackedUp.lengthSqr() <=
				0.000001f)
		{
			return false;
		}

		trackedUp.normalize();

		VehicleMotionShotOverrideState& state =
			vehicleMotionShotOverride;
		state.active = true;
		state.savedCamera =
			Helpers::GetCamera();
		state.savedLastLookDir =
			Game::instance.LastLookDir;

		state.ownerUnit = ownerUnit;
		state.savedOwnerFacingDir =
			ownerUnit->facingDir;
		state.savedOwnerFacing =
			ownerUnit->facing;
		state.savedOwnerDesiredAim =
			ownerUnit->desiredAim;
		state.savedOwnerAim =
			ownerUnit->aim;
		state.savedOwnerAimVelocity =
			ownerUnit->aimVelocity;
		state.savedOwnerAim2 =
			ownerUnit->aim2;
		state.savedOwnerAim3 =
			ownerUnit->aim3;

		state.weaponObject = weaponObject;
		state.savedWeaponFacingDir =
			weaponObject->facingDir;
		state.savedWeaponUpDirection =
			weaponObject->upDirection;

		// FireWeapon samples several overlapping representations depending on the
		// vehicle family. Override every known shot-time source, but only for the
		// duration of FireWeapon.Original(). No camera/update loop receives this
		// direction, so the native camera remains isolated.
		Helpers::GetCamera().lookDir =
			trackedAim;
		Helpers::GetCamera().lookDirUp =
			trackedUp;
		Game::instance.LastLookDir =
			trackedAim;

		ownerUnit->facingDir =
			trackedAim;
		ownerUnit->facing =
			trackedAim;
		ownerUnit->desiredAim =
			trackedAim;
		ownerUnit->aim =
			trackedAim;
		ownerUnit->aimVelocity =
			Vector3(
				0.0f,
				0.0f,
				0.0f);
		ownerUnit->aim2 =
			trackedAim;
		ownerUnit->aim3 =
			trackedAim;

		weaponObject->facingDir =
			trackedAim;
		weaponObject->upDirection =
			trackedUp;

		return true;
	}

	// An exact zero scale makes the skinned arm transform singular.  Vertices shared
	// by the hidden arm and visible hand can then form degenerate triangles, which
	// show up as the thin black line seen on some first-person models.  Keep the
	// hidden geometry microscopic, but leave the transform finite and well-defined.
	constexpr float HIDDEN_ARM_MICRO_SCALE = 0.001f;
	static_assert(HIDDEN_ARM_MICRO_SCALE > 0.0f && HIDDEN_ARM_MICRO_SCALE < 0.01f,
		"Hidden arm scale must stay finite and microscopic");

	// Halo CE compiled weapon layout (Win32):
	//   weapon + 0x4FC -> trigger reflexive
	//   trigger size   -> 0x114
	//   trigger + 0x00 -> flags
	//   trigger + 0x6E -> projectile count
	//   trigger + 0xA0 -> projectile tag ID
	// These offsets are from the public HEK weapon definition and were verified
	// against the original Coop Evolved cache used to reproduce this issue.
	constexpr size_t WEAPON_TRIGGER_REFLEXIVE_OFFSET = 0x4FC;
	constexpr size_t WEAPON_TRIGGER_SIZE = 0x114;
	constexpr size_t TRIGGER_PROJECTILES_PER_SHOT_OFFSET = 0x6E;
	constexpr size_t TRIGGER_PROJECTILE_TAG_ID_OFFSET = 0xA0;
	constexpr uint32_t PROJECTILE_IS_CLIENT_SIDE_ONLY = 0x2000;
	constexpr const char* COOP_PROJECTILE_PATH_PREFIX = "alpha_firefight\\";
	constexpr uint32_t HALO_MAP_HEADER_MAGIC = 0x68656164;
	constexpr size_t HALO_MAP_HEADER_READ_SIZE = 0x68;
	constexpr size_t HALO_MAP_NAME_OFFSET = 0x20;
	constexpr size_t HALO_MAP_BUILD_OFFSET = 0x40;
	constexpr size_t HALO_MAP_CRC_OFFSET = 0x64;

	struct OfficialCoopMapRevision
	{
		const char* name;
		uint32_t crc;
	};

	// Exact cache CRCs from the public Coop Evolved 1.9.0, 1.10.0 and 1.10.1
	// packages. The CRC is paired with the map name so a rebuilt cache with the
	// same _coop_evolved filename remains outside this compatibility path.
	constexpr OfficialCoopMapRevision OFFICIAL_COOP_MAPS[] =
	{
		// 1.9.0
		{ "a30_coop_evolved", 0x8DB60CB9 },
		{ "a50_coop_evolved", 0x543194E4 },
		{ "b30_coop_evolved", 0x31411FF4 },
		{ "b40_coop_evolved", 0x261DA6FC },
		{ "c10_coop_evolved", 0xE2181C48 },
		{ "c20_coop_evolved", 0x75FD2762 },
		{ "d40_coop_evolved", 0xCC217B31 },
		// 1.10.0
		{ "a10_coop_evolved", 0xEB847992 },
		{ "a30_coop_evolved", 0xF32C3703 },
		{ "a50_coop_evolved", 0xAD93C68A },
		{ "b30_coop_evolved", 0x7B53A877 },
		{ "b40_coop_evolved", 0x44C299E2 },
		{ "c10_coop_evolved", 0xD415F5BE },
		{ "c20_coop_evolved", 0x5CC2A53C },
		{ "c40_coop_evolved", 0x5A3FF0C4 },
		{ "d20_coop_evolved", 0x90A8A11C },
		{ "d40_coop_evolved", 0x28994C55 },
		// 1.10.1
		{ "a10_coop_evolved", 0xE55E3A05 },
		{ "a30_coop_evolved", 0x064B4889 },
		{ "a50_coop_evolved", 0x76A37213 },
		{ "b30_coop_evolved", 0xA3F8E69D },
		{ "b40_coop_evolved", 0x5BE906D9 },
		{ "c10_coop_evolved", 0xE2131E56 },
		{ "c20_coop_evolved", 0xEDD47F9C },
		{ "c40_coop_evolved", 0x54C21F93 },
		{ "d20_coop_evolved", 0x784F8912 },
		{ "d40_coop_evolved", 0x7E011D61 }
	};

	enum class CoopMapScope
	{
		Unavailable,
		OfficialRelease,
		OtherMap
	};

	struct RuntimeTagReflexive
	{
		int32_t count;
		uint8_t* elements;
		uint32_t id;
	};

	static_assert(sizeof(RuntimeTagReflexive) == 0x0C,
		"Halo tag reflexives require the Win32 build");

	bool IsReadableMemory(const void* address, size_t size)
	{
		if (!address || size == 0)
		{
			return false;
		}

		uintptr_t current = reinterpret_cast<uintptr_t>(address);
		const uintptr_t end = current + size;
		if (end < current)
		{
			return false;
		}

		while (current < end)
		{
			MEMORY_BASIC_INFORMATION info{};
			if (VirtualQuery(reinterpret_cast<const void*>(current), &info, sizeof(info)) == 0 ||
				info.State != MEM_COMMIT ||
				(info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
			{
				return false;
			}

			const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
			if (regionEnd <= current)
			{
				return false;
			}
			current = regionEnd < end ? regionEnd : end;
		}

		return true;
	}

	bool IsWritableMemory(const void* address, size_t size)
	{
		if (!IsReadableMemory(address, size))
		{
			return false;
		}

		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(address, &info, sizeof(info)) == 0)
		{
			return false;
		}

		const DWORD protection = info.Protect & 0xFF;
		return protection == PAGE_READWRITE ||
			protection == PAGE_WRITECOPY ||
			protection == PAGE_EXECUTE_READWRITE ||
			protection == PAGE_EXECUTE_WRITECOPY;
	}

	bool ReadTagPath(const char* source, char (&destination)[260])
	{
		if (!source)
		{
			return false;
		}

		for (size_t i = 0; i < sizeof(destination); i++)
		{
			if (!IsReadableMemory(source + i, 1))
			{
				return false;
			}

			destination[i] = source[i];
			if (destination[i] == '\0')
			{
				return true;
			}
		}

		destination[sizeof(destination) - 1] = '\0';
		return false;
	}

	bool ReadVehicleTagPath(
		const BaseDynamicObject* vehicleObject,
		char (&vehiclePath)[260])
	{
		if (!vehicleObject ||
			vehicleObject->N0000027E != ObjectType::VEHICLE)
		{
			return false;
		}

		Asset_Generic* vehicleTag =
			Helpers::GetTypedAsset<Asset_Generic>(vehicleObject->tagID);
		if (!vehicleTag ||
			!IsReadableMemory(vehicleTag, sizeof(Asset_Generic)))
		{
			return false;
		}

		const char* vehiclePathPointer = *reinterpret_cast<char* const*>(
			reinterpret_cast<const uint8_t*>(vehicleTag) + 0x10);
		return ReadTagPath(vehiclePathPointer, vehiclePath);
	}


	// The Banshee and Scorpion calculate their real weapon direction from Halo's
	// vehicle crosshair. Relocating their temporary firing carrier to the tracked VR
	// hand makes only the local visual projectile follow the controller while the
	// authoritative hit remains on the crosshair. Keep those vehicle families on
	// Halo's native firing position and aim for both primary and secondary triggers.
	bool ShouldUseNativeVehicleProjectileVisuals(
		const BaseDynamicObject* vehicleObject)
	{
		char vehiclePath[260]{};
		if (!ReadVehicleTagPath(vehicleObject, vehiclePath))
		{
			return false;
		}

		return std::strstr(vehiclePath, "banshee") != nullptr ||
			std::strstr(vehiclePath, "scorpion") != nullptr;
	}

	bool ReadFixedMapString(const uint8_t* source, char (&destination)[33])
	{
		if (!IsReadableMemory(source, 32))
		{
			return false;
		}

		std::memcpy(destination, source, 32);
		destination[32] = '\0';
		return true;
	}

	const uint8_t* GetCurrentMapHeader()
	{
		// Halo compares the current header magic through this absolute address. The
		// operand points directly at the persistent 0x800-byte map-header buffer.
		static Offset mapHeaderOffset = {
			"CurrentMapHeader",
			0x000000,
			"81 3D ?? ?? ?? ?? ?? ?? ?? ?? 8B 3D",
			0,
			0
		};
		static const uint8_t* mapHeader = nullptr;
		static bool bAttemptedResolution = false;

		if (!bAttemptedResolution)
		{
			bAttemptedResolution = true;
			if (SigScanner::UpdateOffset(mapHeaderOffset, false) >= 0 &&
				IsReadableMemory(reinterpret_cast<const void*>(mapHeaderOffset.Address + 2),
					sizeof(uint32_t)))
			{
				const uint32_t mapHeaderAddress = *reinterpret_cast<const uint32_t*>(
					mapHeaderOffset.Address + 2);
				mapHeader = reinterpret_cast<const uint8_t*>(
					static_cast<uintptr_t>(mapHeaderAddress));
			}
		}

		return IsReadableMemory(mapHeader, HALO_MAP_HEADER_READ_SIZE) ? mapHeader : nullptr;
	}

	CoopMapScope GetCoopMapScope()
	{
		const uint8_t* header = GetCurrentMapHeader();
		if (!header)
		{
			return CoopMapScope::Unavailable;
		}

		const uint32_t magic = *reinterpret_cast<const uint32_t*>(header);
		const uint32_t crc = *reinterpret_cast<const uint32_t*>(header + HALO_MAP_CRC_OFFSET);
		char mapName[33]{};
		char build[33]{};
		if (magic != HALO_MAP_HEADER_MAGIC ||
			!ReadFixedMapString(header + HALO_MAP_NAME_OFFSET, mapName) ||
			!ReadFixedMapString(header + HALO_MAP_BUILD_OFFSET, build))
		{
			return CoopMapScope::Unavailable;
		}

		bool bOfficialRelease = false;
		for (const OfficialCoopMapRevision& revision : OFFICIAL_COOP_MAPS)
		{
			if (revision.crc == crc && _stricmp(revision.name, mapName) == 0)
			{
				bOfficialRelease = true;
				break;
			}
		}

		return bOfficialRelease ? CoopMapScope::OfficialRelease : CoopMapScope::OtherMap;
	}

	bool IsUserHitboxAccurateProjectileModeEnabled()
	{
		return Game::instance.c_MultiplayerHitboxAccurateProjectiles &&
			Game::instance.c_MultiplayerHitboxAccurateProjectiles->Value();
	}

	bool ShouldUseHitboxAccurateProjectileMode(CoopMapScope mapScope)
	{
		// Official Coop Evolved caches use Mimic's authoritative damaging shot.
		// Retaining a second tracked-muzzle local projectile cannot be made visually
		// clean for every projectile and impact family, so those caches always use
		// hitbox-accurate mode. Normal LAN and rebuilt maps obey the checkbox.
		return mapScope == CoopMapScope::OfficialRelease ||
			IsUserHitboxAccurateProjectileModeEnabled();
	}

	bool IsOriginalCoopProjectilePath(const char* path)
	{
		char safePath[260]{};
		if (!ReadTagPath(path, safePath))
		{
			return false;
		}

		const size_t prefixLength = std::strlen(COOP_PROJECTILE_PATH_PREFIX);
		return _strnicmp(safePath, COOP_PROJECTILE_PATH_PREFIX, prefixLength) == 0 &&
			strstr(safePath, "\\projectile\\") != nullptr;
	}

	bool IsSupportedCoopWeaponProjectilePair(const char* weaponPath,
		const char* projectilePath)
	{
		if (!weaponPath || !projectilePath)
		{
			return false;
		}

		if (_stricmp(weaponPath, "weapons\\assault rifle\\assault rifle") == 0)
		{
			return _stricmp(projectilePath,
				"alpha_firefight\\weapons\\assault_rifle\\projectile\\bullet") == 0;
		}
		if (_stricmp(weaponPath, "weapons\\pistol\\pistol") == 0)
		{
			return _stricmp(projectilePath,
				"alpha_firefight\\weapons\\pistol\\projectile\\bullet") == 0;
		}
		if (_stricmp(weaponPath, "weapons\\plasma pistol\\plasma pistol") == 0)
		{
			// Coop Evolved uses separate normal and charged triggers.
			return _stricmp(projectilePath, "weapons\\plasma pistol\\bolt") == 0 ||
				_stricmp(projectilePath, "weapons\\plasma rifle\\charged bolt") == 0;
		}
		if (_stricmp(weaponPath, "weapons\\needler\\needler") == 0)
		{
			return _stricmp(projectilePath, "weapons\\needler\\needle") == 0;
		}
		if (_stricmp(weaponPath, "weapons\\needler\\mp_needler") == 0)
		{
			return _stricmp(projectilePath, "weapons\\needler\\mp_needle") == 0;
		}

		return false;
	}

	float FireGuardDist2XY(const Vector3& a, const Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		return dx * dx + dy * dy;
	}

	bool FireGuardSameHaloID(const HaloID& a, const HaloID& b)
	{
		return a.index == b.index && a.id == b.id;
	}

	bool FireGuardLooksValidHaloID(const HaloID& id)
	{
		return id.index != 0xFFFF && id.id != 0xFFFF;
	}

	// Fire hooks can run for remote players. Resolve the local VR/camera biped so
	// those remote shots are never relocated with the local controller aim.
	UnitDynamicObject* ResolveMPFireGuardLocalVRBiped(HaloID& outID)
	{
		outID.index = 0xFFFF;
		outID.id = 0xFFFF;

		ObjectTable& objectTable = Helpers::GetObjectTable();
		if (!objectTable.elements || objectTable.currentSize == 0)
		{
			return nullptr;
		}

		const Vector3 cameraPos = Helpers::GetCamera().position;

		Vector3 weaponPos{};
		Vector3 weaponAim{};
		Vector3 weaponUp{};
		const bool bHasWeaponAim =
			Game::instance.GetWorldWeaponAim(weaponPos, weaponAim, weaponUp);

		UnitDynamicObject* bestUnit = nullptr;
		float bestScore = 99999999.0f;
		HaloID bestID{ 0xFFFF, 0xFFFF };

		for (uint16_t i = 0; i < objectTable.currentSize; i++)
		{
			ObjectDatum& datum = objectTable.elements[i];
			BaseDynamicObject* obj = datum.dynamicObject;

			if (!obj || obj->N0000027E != ObjectType::BIPED)
			{
				continue;
			}

			UnitDynamicObject* unit = reinterpret_cast<UnitDynamicObject*>(obj);
			if (unit->health <= 0.0f)
			{
				continue;
			}

			float score = FireGuardDist2XY(unit->position, cameraPos);

			if (bHasWeaponAim)
			{
				score += FireGuardDist2XY(unit->position, weaponPos) * 0.35f;
			}

			if (FireGuardLooksValidHaloID(unit->weapon))
			{
				BaseDynamicObject* weaponObj = Helpers::GetDynamicObject(unit->weapon);
				if (weaponObj && weaponObj->N0000027E == ObjectType::WEAPON && weaponObj->parent.index == i && weaponObj->parent.id == datum.id)
				{
					score *= 0.65f;
				}
			}

			if (score < bestScore)
			{
				bestScore = score;
				bestUnit = unit;
				bestID.index = i;
				bestID.id = datum.id;
			}
		}

		if (!bestUnit)
		{
			return nullptr;
		}

		outID = bestID;
		return bestUnit;
	}

	// FireWeapon is also called for remote players on this client. Ownership must
	// be derived from live Halo object relationships, never camera proximity.
	//
	// Handheld weapon:
	//   weapon -> parent BIPED -> Unit.playerID
	//
	// Vehicle-mounted weapon:
	//   weapon -> parent VEHICLE -> attached BIPED whose equipped weapon or seat
	//   exactly matches the firing weapon.
	//
	// These are identity checks, not distance guesses.
	bool IsFiniteVisualAim(const Vector3& aim)
	{
		return std::isfinite(aim.x) &&
			std::isfinite(aim.y) &&
			std::isfinite(aim.z) &&
			aim.lengthSqr() > 0.000001f;
	}

	UnitDynamicObject* ResolveExactVehicleFireOwner(
		const HaloID& vehicleID,
		const HaloID& weaponID,
		const BaseDynamicObject* vehicleObject,
		const BaseDynamicObject* weaponObject,
		HaloID& outOwnerID)
	{
		outOwnerID = HaloID{ 0xFFFF, 0xFFFF };

		char vehiclePath[260]{};
		const bool bWarthog =
			ReadVehicleTagPath(vehicleObject, vehiclePath) &&
			std::strstr(vehiclePath, "warthog") != nullptr;

		ObjectTable& table = Helpers::GetObjectTable();
		if (!table.elements || table.currentSize == 0)
		{
			return nullptr;
		}

		UnitDynamicObject* weaponMatch = nullptr;
		HaloID weaponMatchID{ 0xFFFF, 0xFFFF };
		unsigned int weaponMatchCount = 0;

		UnitDynamicObject* seatMatch = nullptr;
		HaloID seatMatchID{ 0xFFFF, 0xFFFF };
		unsigned int seatMatchCount = 0;

		UnitDynamicObject* warthogGunnerMatch = nullptr;
		HaloID warthogGunnerMatchID{ 0xFFFF, 0xFFFF };
		unsigned int warthogGunnerMatchCount = 0;

		for (uint16_t i = 0; i < table.currentSize; ++i)
		{
			ObjectDatum& datum = table.elements[i];
			BaseDynamicObject* object = datum.dynamicObject;
			if (!object || object->N0000027E != ObjectType::BIPED)
			{
				continue;
			}

			UnitDynamicObject* unit =
				reinterpret_cast<UnitDynamicObject*>(object);
			if (unit->health <= 0.0f ||
				!FireGuardSameHaloID(unit->parent, vehicleID))
			{
				continue;
			}

			HaloID candidateID{ i, datum.id };

			// Every stock Warthog-family vehicle weapon is operated from seat 1.
			// The weapon object's attachment field can instead contain the driver
			// attachment, so it is not an ownership key for these turrets.
			if (bWarthog &&
				(unit->parentSeatIndex & 0xFFu) == 1u)
			{
				warthogGunnerMatch = unit;
				warthogGunnerMatchID = candidateID;
				++warthogGunnerMatchCount;
			}

			// Strongest live relationship: this occupant reports the exact firing
			// weapon as its equipped weapon.
			if (FireGuardSameHaloID(unit->weapon, weaponID))
			{
				weaponMatch = unit;
				weaponMatchID = candidateID;
				++weaponMatchCount;
			}

			// A vehicle-owned weapon can still identify its user through the exact
			// seat attachment value shared by the weapon and occupant.
			if (weaponObject &&
				unit->parentSeatIndex == weaponObject->parentSeatIndex)
			{
				seatMatch = unit;
				seatMatchID = candidateID;
				++seatMatchCount;
			}
		}

		// A Warthog has one firing seat. Never let its turret fall back to the
		// weapon attachment value and attribute a gunner shot to the driver.
		if (bWarthog)
		{
			if (warthogGunnerMatchCount == 1)
			{
				outOwnerID = warthogGunnerMatchID;
				return warthogGunnerMatch;
			}

			return nullptr;
		}

		// Never guess between occupants.
		if (weaponMatchCount == 1)
		{
			outOwnerID = weaponMatchID;
			return weaponMatch;
		}

		if (seatMatchCount == 1)
		{
			outOwnerID = seatMatchID;
			return seatMatch;
		}

		return nullptr;
	}

	// Remote fire calls can reuse the local command carrier. Substitute the resolved
	// remote owner's native aim only for the duration of FireWeapon.
	bool g_remoteShotVisualAimOverrideActive = false;
	UnitDynamicObject* g_remoteShotVisualCarrier = nullptr;
	Vector3 g_remoteShotVisualSavedCarrierAim{};

	void RestoreRemoteShotVisualAimOverride()
	{
		if (!g_remoteShotVisualAimOverrideActive)
		{
			return;
		}

		if (g_remoteShotVisualCarrier)
		{
			g_remoteShotVisualCarrier->aim =
				g_remoteShotVisualSavedCarrierAim;
		}

		g_remoteShotVisualCarrier = nullptr;
		g_remoteShotVisualAimOverrideActive = false;
	}

	bool BeginRemoteShotVisualAimOverride(
		UnitDynamicObject* localCommandCarrier,
		const Vector3& remoteNativeAim)
	{
		RestoreRemoteShotVisualAimOverride();

		if (!localCommandCarrier ||
			!IsFiniteVisualAim(remoteNativeAim))
		{
			return false;
		}

		Vector3 normalizedAim = remoteNativeAim;
		normalizedAim.normalize();

		g_remoteShotVisualCarrier = localCommandCarrier;
		g_remoteShotVisualSavedCarrierAim =
			localCommandCarrier->aim;
		localCommandCarrier->aim = normalizedAim;
		g_remoteShotVisualAimOverrideActive = true;
		return true;
	}
}

// This is a working decomp of the game's original logic for updating the view model's skeleton
// Only kept here for reference when working on the replacement function below
static void ReferenceUpdateViewModelImpl(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* boneTransforms, Transform* outBoneTransforms)
{
	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	AssetData_ModelAnimations* animationData = viewModel->Data;
	Bone* boneArray = animationData->BoneArray;

	Transform root;
	Helpers::MakeTransformFromXZ(up, facing, &root);
	root.translation = *pos;

	int i = 0;

	if (animationData->NumBones > 0)
	{
		int lastIndex = 1;
		int16_t bonesToProcess[64]{};

		bonesToProcess[0] = 0;

		do
		{
			const int16_t boneIdx = bonesToProcess[i];
			i++;
			const Bone& currentBone = boneArray[boneIdx];
			const Transform* parentTransform = boneIdx == 0 ? &root : &outBoneTransforms[currentBone.Parent];
			const TransformQuat* currentQuat = &boneTransforms[boneIdx];
			Transform tempTransform;
			Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
			tempTransform.scale = currentQuat->scale;
			tempTransform.translation = currentQuat->translation;
			Helpers::CombineTransforms(parentTransform, &tempTransform, &outBoneTransforms[boneIdx]);

			if (currentBone.LeftLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.LeftLeaf;
				lastIndex++;
			}
			if (currentBone.RightLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.RightLeaf;
				lastIndex++;
			}

		} while (i != lastIndex);
	}
}

void WeaponHandler::UpdateViewModel(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* boneTransforms, Transform* outBoneTransforms)
{
	Vector3& camPos = Helpers::GetCamera().position;

	pos->x = camPos.x;
	pos->y = camPos.y;
	pos->z = camPos.z;

	Matrix4 handTransform;
	CalculateHandTransform(pos, handTransform);

	if (Game::instance.bUse3DOFAiming)
	{
		// Initialize yaw offset tracking when entering 3DOF mode
		if (!bWasIn3DOFMode)
		{
			lastYawOffset = Game::instance.GetVR()->GetYawOffset();
			bWasIn3DOFMode = true;
		}

		// Apply HMD translation so weapon follows player's body position when leaning
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform();
		Vector3 hmdPosition = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		Vector3 hmdOffset = hmdPosition * Game::instance.MetresToWorld(1.0f);
		pos->x += hmdOffset.x;
		pos->y += hmdOffset.y;
		pos->z += hmdOffset.z;

		// Apply configurable weapon offset in world space (hot-reloadable)
		Vector3 configOffset = Game::instance.c_3DOFWeaponOffset->Value();
		Vector3 weaponOffset = configOffset * Game::instance.MetresToWorld(1.0f);
		pos->x += weaponOffset.x;
		pos->y += weaponOffset.y;
		pos->z += weaponOffset.z;

		// Apply controller rotation to weapon facing/up vectors
		// Only use pitch and yaw from the controller, zero out roll

		// Extract rotation from controller
		Vector3 controllerPos = handTransform * Vector3(0.0f, 0.0f, 0.0f);
		Matrix4 rotationOnly = handTransform;
		rotationOnly.translate(-controllerPos);

		// Get the facing direction from controller (this has pitch and yaw)
		Vector3 facingDir = rotationOnly * Vector3(1.0f, 0.0f, 0.0f);

		// Detect snap turns and rotate smoothed direction to prevent lerping artifact
		float currentYawOffset = Game::instance.GetVR()->GetYawOffset();
		float yawDelta = currentYawOffset - lastYawOffset;

		// Detect snap turns and rotate smoothed direction
		Helpers::RotateForSnapTurn(smoothed3DOFFacingDir, yawDelta, Game::instance.c_SnapTurnAmount->Value());

		// Update tracked yaw offset for next frame
		lastYawOffset = currentYawOffset;

		// Apply cosmetic motion smoothing to facing direction
		float smoothingAmount = Game::instance.c_3DOFWeaponSmoothingAmount->Value();
		float clampedSmoothing = std::clamp(smoothingAmount, 0.0f, 2.0f);

		if (clampedSmoothing > 0.0f)
		{
			const float scaleFactor = (-20.0f / 9.0f);
			float h = 90.0f * log2(1.0f - exp(clampedSmoothing * scaleFactor));
			float t = 1.0f - pow(2.0f, Game::instance.lastDeltaTime * h);
			smoothed3DOFFacingDir = Helpers::Lerp(smoothed3DOFFacingDir, facingDir, t);
			facingDir = smoothed3DOFFacingDir;
			facingDir.normalize();
		}
		else
		{
			// No smoothing - update cached value for when smoothing is re-enabled
			smoothed3DOFFacingDir = facingDir;
		}

		// Use world up (0, 0, 1) to derive a roll-free orientation
		// Cross product of world up and facing gives us the right vector
		Vector3 worldUp(0.0f, 0.0f, 1.0f);
		Vector3 rightDir = worldUp.cross(facingDir);
		rightDir.normalize();

		// Cross product of facing and right gives us the corrected up vector (roll-free)
		Vector3 upDir = facingDir.cross(rightDir);
		upDir.normalize();

		facing->x = facingDir.x;
		facing->y = facingDir.y;
		facing->z = facingDir.z;

		up->x = upDir.x;
		up->y = upDir.y;
		up->z = upDir.z;
	}
	else
	{
		// 6DOF MODE: Standard camera-relative orientation
		// Reset the 3DOF mode flag so we reinitialize when switching back
		bWasIn3DOFMode = false;

		facing->x = 1.0f;
		facing->y = 0.0f;
		facing->z = 0.0f;

		up->x = 0.0f;
		up->y = 0.0f;
		up->z = 1.0f;
	}

	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	if (!viewModel)
	{
		Logger::log << "[UpdateViewModel] Can't get view model asset" << std::endl;
		return;
	}

	AssetData_ModelAnimations* animationData = viewModel->Data;
	Bone* boneArray = animationData->BoneArray;

	Transform root;
	Helpers::MakeTransformFromXZ(up, facing, &root);
	root.translation = *pos;

	const bool bShouldUpdateCache = cachedViewModel.currentAsset != id;
	if (bShouldUpdateCache)
	{
		UpdateCache(id, animationData);
	}

	Transform unmodifiedHandTransform;
	CalculateBoneTransform(cachedViewModel.rightWristIndex, boneArray, root, boneTransforms, unmodifiedHandTransform);

	Transform realTransforms[64]{};

	if (animationData->NumBones > 0)
	{
		int lastIndex = 1;
		int16_t bonesToProcess[64]{};

		bonesToProcess[0] = 0;

		int i = 0;
		do
		{
			const int16_t boneIndex = bonesToProcess[i];
			i++;
			const Bone& currentBone = boneArray[boneIndex];
			Transform* parentTransform = boneIndex == 0 ? &root : &outBoneTransforms[currentBone.Parent];
			const TransformQuat* currentQuat = &boneTransforms[boneIndex];
			Transform tempTransform;
			Transform modifiedTransform;
			// For all bones but the root sub in the ACTUAL transform for the calculations (free from scaling/transform issues)
			if (boneIndex > 0)
			{
				modifiedTransform = *parentTransform;
				*parentTransform = realTransforms[currentBone.Parent];
			}

			Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
			tempTransform.scale = currentQuat->scale;

			if (boneIndex == cachedViewModel.displayIndex && Game::instance.bLeftHanded)
			{
				// Bit of a nasty place to do this, but we need to unflip the ammo counter on the BR
				Matrix3 rot = tempTransform.rotation;
				rot *= Matrix3(
					1.0f, 0.0f, 0.0f,
					0.0f, -1.0f, 0.0f,
					0.0f, 0.0f, 1.0f
				);

				for (int i = 0; i < 9; i++)
				{
					tempTransform.rotation[i] = rot[i];
				}
			}

			tempTransform.translation = currentQuat->translation;
			Helpers::CombineTransforms(parentTransform, &tempTransform, &outBoneTransforms[boneIndex]);

			if (boneIndex > 0)
			{
				// Restore the modified transform
				*parentTransform = modifiedTransform;
			}
			// Cache the calculated transform for this bone
			realTransforms[boneIndex] = outBoneTransforms[boneIndex];
			if (currentBone.Parent == 0 || boneArray[currentBone.Parent].Parent == 0)
			{
				// Hide arms/root in 6DOF mode only
				if (!Game::instance.bUse3DOFAiming)
				{
					outBoneTransforms[boneIndex].scale = HIDDEN_ARM_MICRO_SCALE;
				}
			}
			else if (boneIndex == cachedViewModel.rightWristIndex)
			{
				// Skip VR hand tracking in 3DOF mode - use original weapon bone transforms
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: VR hand tracking
					// This is dreadful code. Rework to be less insane
					if (!Game::instance.bUseTwoHandAim)
					{
						Matrix4 newTransform = handTransform;
						if (currentBone.RightLeaf != -1)
						{
							Bone& GunBone = boneArray[currentBone.RightLeaf];
							if (currentBone.RightLeaf == cachedViewModel.gunIndex)
							{
								const TransformQuat* GunQuat = &boneTransforms[currentBone.RightLeaf];
								Helpers::MakeTransformFromQuat(&GunQuat->rotation, &tempTransform);

								Matrix4 rotation;
								for (int x = 0; x < 3; x++)
								{
									for (int y = 0; y < 3; y++)
									{
										rotation[x + y * 4] = tempTransform.rotation[x + y * 3];
									}
								}

								if (Game::instance.bLeftHanded)
								{
									Matrix4 scale;
									scale.scale(1.0f, -1.0f, 1.0f);

									rotation = scale * rotation * scale;
								}

								newTransform = newTransform * rotation.invert();
							}
							else
							{
								Logger::log << "ERROR: Right leaf of " << currentBone.BoneName << " is " << GunBone.BoneName << std::endl;
							}

						}
						MoveBoneToTransform(boneIndex, newTransform, realTransforms, outBoneTransforms);
					}
					else
					{
						MoveBoneToTransform(boneIndex, handTransform, realTransforms, outBoneTransforms);
					}
					CreateEndCap(boneIndex, currentBone, boneArray, animationData->NumBones, outBoneTransforms);
				}
			}
			else if (boneIndex == cachedViewModel.leftWristIndex)
			{
				// Skip VR hand tracking in 3DOF mode - use original weapon bone transforms
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: VR hand tracking
					if (!Game::instance.bUseTwoHandAim)
					{
						Matrix4 newTransform = Game::instance.GetVR()->GetControllerTransform(Game::instance.bLeftHanded ? ControllerRole::Right : ControllerRole::Left, true);
						// Apply scale only to translation portion
						Vector3 translation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
						newTransform.translate(-translation);
						translation *= Game::instance.MetresToWorld(1.0f);
						translation += *pos;
						newTransform.translate(translation);

						MoveBoneToTransform(boneIndex, newTransform, realTransforms, outBoneTransforms);
					}
					else
					{
						// Convert both hand transforms to matrix4
						Matrix4 leftMatrix;
						TransformToMatrix4(outBoneTransforms[boneIndex], leftMatrix);
						Matrix4 rightMatrix;
						TransformToMatrix4(unmodifiedHandTransform, rightMatrix);

						// Get inverse of dominant hand, apply it to non-dominant to get delta
						rightMatrix.invertAffine();
						Matrix4 deltaMatrix = rightMatrix * leftMatrix;

						if (Game::instance.bLeftHanded)
						{
							Matrix4 flip;
							flip.scale(1.0f, -1.0f, 1.0f);

							deltaMatrix = flip * deltaMatrix * flip;
							leftMatrix = handTransform * deltaMatrix;
						}
						else
						{
							// Apply delta to controller transform
							leftMatrix = handTransform * deltaMatrix;
						}

						// Move non-dominant hand to new transform
						MoveBoneToTransform(boneIndex, leftMatrix, realTransforms, outBoneTransforms);
					}

					CreateEndCap(boneIndex, currentBone, boneArray, animationData->NumBones, outBoneTransforms);
				}
			}
			else if (boneIndex == cachedViewModel.gunIndex)
			{
				// Skip hand-relative gun calculations in 3DOF mode
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: Calculate gun position/rotation relative to hand
					Vector3& gunPos = outBoneTransforms[boneIndex].translation;
					Matrix3 gunRot = outBoneTransforms[boneIndex].rotation;

					if (Game::instance.bLeftHanded)
					{
						gunRot = gunRot * Matrix3(
							1.0f, 0.0f, 0.0f,
							0.0f, -1.0f, 0.0f,
							0.0f, 0.0f, 1.0f
						);
					}

					Vector3 handPos = handTransform * Vector3(0.0f, 0.0f, 0.0f);
					Matrix4 handRotation = handTransform.translate(-handPos);
					Matrix3 handRotation3;

					for (int i = 0; i < 3; i++)
					{
						handRotation3.setColumn(i, &handRotation.get()[i * 4]);
					}

					Matrix3 inverseHand = handRotation3;
					inverseHand.invert();

					cachedViewModel.fireOffset = (gunPos - handPos) + (gunRot * cachedViewModel.cookedFireOffset);
					cachedViewModel.fireOffset = inverseHand * cachedViewModel.fireOffset;

					cachedViewModel.gunOffset = (gunPos - handPos);
					cachedViewModel.gunOffset = inverseHand * cachedViewModel.gunOffset;

					cachedViewModel.fireRotation = cachedViewModel.cookedFireRotation * gunRot * inverseHand;
				}
			}

#if DRAW_DEBUG_AIM
			if (currentBone.Parent != -1)
			{
				Game::instance.inGameRenderer.DrawLine3D(outBoneTransforms[boneIndex].translation, outBoneTransforms[currentBone.Parent].translation, D3DCOLOR_ARGB(127, 127, 127, 127), false);
			}
#endif

			if (currentBone.LeftLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.LeftLeaf;
				lastIndex++;
			}
			if (currentBone.RightLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.RightLeaf;
				lastIndex++;
			}

		} while (i != lastIndex);
	}
}

inline void WeaponHandler::CalculateBoneTransform(int boneIndex, Bone* boneArray, Transform& root, TransformQuat* boneTransforms, Transform& outTransform) const
{
	// Clear to identity
	Vector3 xVec = Vector3(1.0f, 0.0f, 0.0f);
	Vector3 zVec = Vector3(0.0f, 0.0f, 1.0f);
	Helpers::MakeTransformFromXZ(&zVec, &xVec, &outTransform);

	if (boneIndex < 0)
	{
		return;
	}

	int currentIndex = boneIndex;

	while (true)
	{
		Bone& currentBone = boneArray[currentIndex];

		// Convert bone from TransformQuat to Transform
		const TransformQuat* currentQuat = &boneTransforms[currentIndex];
		Transform tempTransform;
		Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
		tempTransform.scale = currentQuat->scale;
		tempTransform.translation = currentQuat->translation;

		// Apply current transform to child transform
		Helpers::CombineTransforms(&tempTransform, &outTransform, &outTransform);

		if (currentIndex == 0)
		{
			break;
		}

		// Get next bone
		currentIndex = currentBone.Parent;
	}

	// Do root
	Helpers::CombineTransforms(&root, &outTransform, &outTransform);
}

inline void WeaponHandler::CalculateHandTransform(Vector3* pos, Matrix4& handTransform) const
{
	Matrix4 newTransform = GetDominantHandTransform();

	// Apply scale only to translation portion
	{
		Vector3 translation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
		newTransform.translate(-translation);
		translation *= Game::instance.MetresToWorld(1.0f);
		translation += *pos;
		newTransform.translate(translation);
	}

	handTransform = newTransform;
}

void WeaponHandler::CreateEndCap(int boneIndex, const Bone& currentBone, const Bone* boneArray, int numBones, Transform* outBoneTransforms) const
{
	if (!boneArray || !outBoneTransforms || boneIndex < 0 || boneIndex >= numBones)
	{
		return;
	}

	// Collapse every arm-specific ancestor to the wrist, not just the forearm.
	// Leaving the upper arm at its original location allows vertices blended
	// between the forearm and upper-arm bones to form a long, thin black tether.
	// Bone 0 is the shared model root, so stop before it to avoid moving the
	// opposite hand or weapon hierarchy.
	const Transform wristEndCap = outBoneTransforms[boneIndex];
	int ancestorIndex = currentBone.Parent;
	int visitedBones = 0;

	while (ancestorIndex > 0 && ancestorIndex < numBones && visitedBones < numBones)
	{
		Transform& ancestorTransform = outBoneTransforms[ancestorIndex];
		ancestorTransform.translation = wristEndCap.translation;
		for (int j = 0; j < 9; j++)
		{
			ancestorTransform.rotation[j] = wristEndCap.rotation[j];
		}
		ancestorTransform.scale = HIDDEN_ARM_MICRO_SCALE;

		const int nextAncestor = boneArray[ancestorIndex].Parent;
		if (nextAncestor == ancestorIndex)
		{
			break;
		}

		ancestorIndex = nextAncestor;
		visitedBones++;
	}
}

void WeaponHandler::MoveBoneToTransform(int boneIndex, const Matrix4& newTransform, Transform* realTransforms, Transform* outBoneTransforms) const
{
	// Move hands to match controllers
	Vector3 newTranslation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 newRotation4 = Game::instance.bLeftHanded ? newTransform * Matrix4().scale(1.0f, -1.0f, 1.0f) : newTransform;
	newRotation4.translate(-newTranslation);
	newRotation4.rotateZ(localRotation.z);
	newRotation4.rotateY(localRotation.y);
	newRotation4.rotateX(localRotation.x);

	//Add Local offset
	newTranslation += newRotation4 * localOffset;

	outBoneTransforms[boneIndex].translation = newTranslation;
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			outBoneTransforms[boneIndex].rotation[x + y * 3] = newRotation4.get()[x + y * 4];
		}
	}
	realTransforms[boneIndex] = outBoneTransforms[boneIndex]; // Re-cache value to use updated position
}

void WeaponHandler::UpdateCache(HaloID& id, AssetData_ModelAnimations* animationData)
{
#if DRAW_DEBUG_AIM
	Logger::log << "[UpdateCache] Swapped weapons, recaching " << id << std::endl;
#endif
	// Leave the cache invalid until all required multiplayer weapon data is available.
	cachedViewModel.currentAsset.index = 0xFFFF;
	cachedViewModel.currentAsset.id = 0xFFFF;

	cachedViewModel.leftWristIndex = -1;
	cachedViewModel.rightWristIndex = -1;
	cachedViewModel.gunIndex = -1;
	cachedViewModel.displayIndex = -1;

	Bone* boneArray = animationData->BoneArray;

	for (int i = 0; i < animationData->NumBones; i++)
	{
		Bone& CurrentBone = boneArray[i];

		if (cachedViewModel.leftWristIndex == -1 && strstr(CurrentBone.BoneName, "l wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Left Wrist @ " << i << std::endl;
#endif
			cachedViewModel.leftWristIndex = i;
		}
		else if (cachedViewModel.rightWristIndex == -1 && strstr(CurrentBone.BoneName, "r wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Right Wrist @ " << i << std::endl;
#endif
			cachedViewModel.rightWristIndex = i;
		}
		else if (cachedViewModel.gunIndex == -1 && strstr(CurrentBone.BoneName, "gun"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			cachedViewModel.gunIndex = i;
		}
		else if (cachedViewModel.gunIndex == -1 && strstr(CurrentBone.BoneName, "body"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			cachedViewModel.gunIndex = i;
		}
		else if (cachedViewModel.displayIndex == -1 && strstr(CurrentBone.BoneName, "display"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Display @ " << i << std::endl;
#endif
			cachedViewModel.displayIndex = i;
		}
#if DRAW_DEBUG_AIM
		else
		{
			Logger::log << "[UpdateCache] Skipped Bone " << CurrentBone.BoneName << std::endl;
		}
#endif
	}

	cachedViewModel.fireOffset = Vector3();
	cachedViewModel.cookedFireOffset = Vector3();
	cachedViewModel.cookedFireRotation = Matrix3();
	cachedViewModel.gunOffset = Vector3();

	// weapon model can be found from this chain:
	// player->WeaponID (DynamicObject)->WeaponID (weapon Asset)->WeaponData->ViewModelID (GBX Asset)
	// The local offset of the fire VFX (i.e. end of the barrel) is found in the "primary trigger" socket

	BaseDynamicObject* player = Helpers::GetLocalPlayer();
	if (!player)
	{
		Logger::log << "[UpdateCache] Can't find local player" << std::endl;
		return;
	}

	BaseDynamicObject* weaponObj = Helpers::GetDynamicObject(player->weapon);
	if (!weaponObj)
	{
		Logger::log << "[UpdateCache] Can't find weapon from WeaponID " << player->weapon << std::endl;
		Logger::log << "[UpdateCache] Player Tag = " << player->tagID << std::endl;
		return;
	}

	Asset_Weapon* weapon = Helpers::GetTypedAsset<Asset_Weapon>(weaponObj->tagID);
	if (!weapon)
	{
		Logger::log << "[UpdateCache] Can't find weapon asset from TagID " << weaponObj->tagID << std::endl;
		return;
	}

	cachedViewModel.weaponType = GetWeaponType(weapon);

	if (!weapon->WeaponData)
	{
		Logger::log << "[UpdateCache] Can't find weapon data in weapon asset " << weaponObj->tagID << std::endl;
		Logger::log << "[UpdateCache] Weapon Type = " << weapon->GroupID << std::endl;
		Logger::log << "[UpdateCache] Weapon Path = " << weapon->WeaponAsset << std::endl;
		return;
	}


	Asset_GBXModel* model = Helpers::GetTypedAsset<Asset_GBXModel>(weapon->WeaponData->ViewModelID);
	if (!model)
	{
		Logger::log << "[UpdateCache] Can't find GBX model from ViewModelID = " << weapon->WeaponData->ViewModelID << std::endl;
		return;
	}

#if DRAW_DEBUG_AIM
	std::string reversedGroup;
	reversedGroup.assign(model->GroupID, 4);
	std::reverse(reversedGroup.begin(), reversedGroup.end());
	Logger::log << "[UpdateCache] GBXModelTag = " << reversedGroup << std::endl;
	Logger::log << "[UpdateCache] GBXModelPath = " << model->ModelPath << std::endl;
#endif

	if (!model->ModelData)
	{
		return;
	}

#if DRAW_DEBUG_AIM
	Logger::log << "[UpdateCache] NumSockets = " << model->ModelData->NumSockets << std::endl;
#endif

	for (int i = 0; i < model->ModelData->NumSockets; i++)
	{
		GBXSocket& socket = model->ModelData->Sockets[i];
#if DRAW_DEBUG_AIM
		Logger::log << "[UpdateCache] Socket = " << socket.SocketName << std::endl;
#endif

		if (strstr(socket.SocketName, "primary trigger"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Effects location, Num Transforms = " << socket.NumTransforms << std::endl;
#endif

			if (socket.NumTransforms == 0)
			{
				Logger::log << "[UpdateCache] " << socket.SocketName << " has no transforms" << std::endl;
				break;
			}

			cachedViewModel.cookedFireOffset = socket.Transforms[0].Position;
			Transform rotation;
			Helpers::MakeTransformFromQuat(&socket.Transforms[0].QRotation, &rotation);
			cachedViewModel.cookedFireRotation = rotation.rotation;

#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Position = " << socket.Transforms[0].Position << std::endl;
			Logger::log << "[UpdateCache] Quaternion = " << socket.Transforms[0].QRotation << std::endl;
#endif
			break;
		}
	}

	// Mark the cache valid only after the required weapon and socket lookups succeed.
	cachedViewModel.currentAsset = id;
}

inline WeaponType WeaponHandler::GetWeaponType(Asset_Weapon* weapon) const
{
	WeaponType foundType = WeaponType::Unknown;
	if (strstr(weapon->WeaponAsset, "\\plasma pistol\\"))
	{
		foundType = WeaponType::PlasmaPistol;
	}
	else if (strstr(weapon->WeaponAsset, "\\sniper rifle\\"))
	{
		foundType = WeaponType::Sniper;
	}
	else if (strstr(weapon->WeaponAsset, "\\pistol\\"))
	{
		foundType = WeaponType::Pistol;
	}
	else if (strstr(weapon->WeaponAsset, "\\plasma rifle\\"))
	{
		foundType = WeaponType::PlasmaRifle;
	}
	else if (strstr(weapon->WeaponAsset, "\\shotgun\\"))
	{
		foundType = WeaponType::Shotgun;
	}
	else if (strstr(weapon->WeaponAsset, "\\assault rifle\\"))
	{
		foundType = WeaponType::AssaultRifle;
	}
	else if (strstr(weapon->WeaponAsset, "\\rocket launcher\\"))
	{
		foundType = WeaponType::RocketLauncher;
	}
	else if (strstr(weapon->WeaponAsset, "\\flamethrower\\"))
	{
		foundType = WeaponType::Flamethrower;
	}
	else if (strstr(weapon->WeaponAsset, "\\plasma_cannon\\"))
	{
		foundType = WeaponType::PlasmaCannon;
	}
	else if (strstr(weapon->WeaponAsset, "\\needler\\"))
	{
		foundType = WeaponType::Needler;
	}
	else if (strstr(weapon->WeaponAsset, "\\fuel rod\\"))
	{
		foundType = WeaponType::FuelRod;
	}
	else
	{
		Logger::log << "[UpdateCache] Unknown weapon with asset " << weapon->WeaponAsset << std::endl;
	}

	return foundType;
}

inline void WeaponHandler::TransformToMatrix4(Transform& inTransform, Matrix4& outMatrix) const
{
	// Assumes scale of 1!
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			// Not sure why get is const, you can directly set the values with setrow/setcolumn anyway
			const_cast<float*>(outMatrix.get())[x + y * 4] = inTransform.rotation[x + y * 3];
		}
	}
	outMatrix.setColumn(3, inTransform.translation);
}

Vector3 WeaponHandler::GetScopeLocation(WeaponType type) const
{
	Vector3 Scale = Game::instance.bLeftHanded ? Vector3(1.0f, -1.0f, 1.0f) : Vector3(1.0f, 1.0f, 1.0f);

	switch (type)
	{
	case WeaponType::RocketLauncher:
		return Game::instance.c_ScopeOffsetRocket->Value() * Scale;
	case WeaponType::Sniper:
		return Game::instance.c_ScopeOffsetSniper->Value() * Scale;
	case WeaponType::Unknown:
	case WeaponType::Pistol:
		return Game::instance.c_ScopeOffsetPistol->Value() * Scale;
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

Matrix4 WeaponHandler::GetDominantHandTransform() const
{
	Matrix4 controllerTransform;
	Vector3 actualControllerPos;
	Vector3 toOffHand;
	Vector3 smoothedPosition;

	if (!Game::instance.GetCalculatedHandPositions(controllerTransform, actualControllerPos, toOffHand))
	{
		return controllerTransform;
	}

	Vector3 upVector = controllerTransform.getForwardAxis();

	// In the unlikely event the player decides to put their hand directly above their other hand, avoid a DIV/0 error when doing the lookat
	if (upVector.dot(toOffHand) == 1.0f)
	{
		upVector += controllerTransform.getUpAxis() * 0.001f;
	}

	/*
	Matrix3 rot;
	for (int i = 0; i < 3; i++)
	{
		offHandTransform.setColumn(i, &rot.get()[i * 4]);
	}
	*/
	smoothedPosition = Game::instance.GetSmoothedInput();
	controllerTransform.lookAt(smoothedPosition, upVector);

	controllerTransform.translate(-actualControllerPos);
	controllerTransform.rotate(-90.0f, controllerTransform.getUpAxis());
	controllerTransform.rotate(-90.0f, controllerTransform.getLeftAxis());
	controllerTransform.translate(actualControllerPos);

	// Apply offset from weapon aiming here
	Matrix4 cachedRot4;

	for (int i = 0; i < 3; i++)
	{
		cachedRot4.setColumn(i, cachedViewModel.fireRotation.getColumn(i));
	}

	controllerTransform *= cachedRot4.invertAffine();
	return controllerTransform;
}

bool WeaponHandler::GetLocalWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	HaloID playerID;
	if (!Helpers::GetLocalPlayerID(playerID))
	{
		return false;
	}

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(playerID));
	if (!player)
	{
		return false;
	}

	// TODO: Handedness
	Matrix4 controllerPos = GetDominantHandTransform();

	Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 handRotation = controllerPos.translate(-handPos);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &handRotation.get()[i * 4]);
	}

	Matrix3 finalRot;

	if (Game::instance.bUse3DOFAiming)
	{
		// 3DOF MODE: Use pure controller rotation (matching bullet direction in RelocatePlayer)
		// fireRotation is NOT updated in 3DOF mode, so skip it
		finalRot = handRotation3;

		// Position from HMD instead of controller
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		outPosition = hmdPos;
	}
	else
	{
		// 6DOF MODE: Use fireRotation offset (existing behavior)
		finalRot = cachedViewModel.fireRotation * handRotation3;
		outPosition = handPos + handRotation * cachedViewModel.fireOffset * Game::instance.WorldToMetres(1.0f);
	}

	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);
	upDir = finalRot * Vector3(0.0f, 0.0f, 1.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_CrosshairDistance->Value());
	Game::instance.inGameRenderer.DrawLine3D(worldOutPos, aimTarget, D3DCOLOR_ARGB(255, 255, 20, 20));

	Game::instance.inGameRenderer.DrawLine3D(lastFireLocation, lastFireAim, D3DCOLOR_ARGB(255, 20, 255, 255));
#endif

	return true;
}

bool WeaponHandler::GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	bool bSuccess = GetLocalWeaponAim(outPosition, outAim, upDir);

	outPosition = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);

	return bSuccess;
}


bool WeaponHandler::GetLocalWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	HaloID playerID;
	if (!Helpers::GetLocalPlayerID(playerID))
	{
		return false;
	}

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(playerID));
	if (!player)
	{
		return false;
	}

	// TODO: Handedness
	Matrix4 controllerPos = GetDominantHandTransform();

	Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 handRotation = controllerPos.translate(-handPos);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &handRotation.get()[i * 4]);
	}

	Matrix3 finalRot;

	if (Game::instance.bUse3DOFAiming)
	{
		// 3DOF MODE: Use pure controller rotation (matching bullet direction in RelocatePlayer)
		// fireRotation is NOT updated in 3DOF mode, so skip it
		finalRot = handRotation3;

		// Position from HMD instead of controller
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		outPosition = hmdPos;
	}
	else
	{
		// 6DOF MODE: Use fireRotation offset and scope position (existing behavior)
		finalRot = cachedViewModel.fireRotation * handRotation3;

		Vector3 scopeOffset = GetScopeLocation(cachedViewModel.weaponType);
		Vector3 gunOffset = handPos + handRotation * cachedViewModel.gunOffset * Game::instance.WorldToMetres(1.0f);
		outPosition = gunOffset + finalRot * scopeOffset;
	}

	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);
	upDir = finalRot * Vector3(1.0f, 0.0f, 1.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_CrosshairDistance->Value());
	Game::instance.inGameRenderer.DrawLine3D(worldOutPos, aimTarget, D3DCOLOR_ARGB(255, 255, 20, 20));
#endif

	return true;
}

bool WeaponHandler::GetWorldWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	bool bSuccess = GetLocalWeaponScope(outPosition, outAim, upDir);

	outPosition = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);

	return bSuccess;
}

bool WeaponHandler::IsSniperScope() const
{
	return cachedViewModel.weaponType == WeaponType::Sniper;
}

void WeaponHandler::RelocatePlayer(HaloID& PlayerID)
{
	weaponFiredPlayer = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(PlayerID));

	if (!weaponFiredPlayer)
	{
		return;
	}

	Matrix4 controllerPos = GetDominantHandTransform();

	// Apply scale only to translation portion
	Vector3 translation = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	controllerPos.translate(-translation);
	translation *= Game::instance.MetresToWorld(1.0f);
	translation += weaponFiredPlayer->position;

	controllerPos.translate(translation);

	Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 handRotation = controllerPos.translate(-handPos);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &handRotation.get()[i * 4]);
	}

	// Cache the real values so we can restore them after running the original fire function
	realPlayerPosition = weaponFiredPlayer->position;
	realPlayerAim = weaponFiredPlayer->aim;

	// Mark the interval in which this biped is only a temporary projectile-origin
	// carrier so roomscale cannot mistake the muzzle position for locomotion.
	Game::instance.BeginTemporaryFireRelocation();

	// Multiplayer hit detection is authoritative around Halo's biped body, but the VR
	// controller/HMD transforms include the user's physical room-scale XY offset.
	// If we feed that XY offset into the temporary firing position, aim direction is
	// correct but the bullet origin drifts away from the server/body hit model as the
	// user walks around their playspace.  For MP only, keep controller rotation and
	// hand-relative position, but remove the HMD/playspace XY component from the
	// fire origin.  Leave Z alone so standing/crouch height behavior stays natural.
	// Use Halo's network role instead of player-table population. A dedicated Coop
	// lobby with one human is still multiplayer and must produce the exact same
	// tracked-muzzle state as a populated normal LAN game.
	const bool bStabilizeMPFireOrigin = Game::instance.IsMultiplayerSession();
	Vector3 mpHMDWorldXYOffset{ 0.0f, 0.0f, 0.0f };

	if (bStabilizeMPFireOrigin && Game::instance.GetVR())
	{
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdLocalPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		mpHMDWorldXYOffset = hmdLocalPos * Game::instance.MetresToWorld(1.0f);
		mpHMDWorldXYOffset.z = 0.0f;
	}

	if (Game::instance.bUse3DOFAiming)
	{
		// 3DOF MODE: Bullets originate from HMD (user's eyes)
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);

		Vector3 hmdPosWorld = hmdPos * Game::instance.MetresToWorld(1.0f);
		Vector3 stabilizedHMDPosWorld = hmdPosWorld;

		if (bStabilizeMPFireOrigin)
		{
			stabilizedHMDPosWorld.x -= mpHMDWorldXYOffset.x;
			stabilizedHMDPosWorld.y -= mpHMDWorldXYOffset.y;
		}

		weaponFiredPlayer->position = realPlayerPosition + stabilizedHMDPosWorld;

		weaponFiredPlayer->aim = handRotation3 * Vector3(1.0f, 0.0f, 0.0f);
	}
	else
	{
		// 6DOF MODE: Bullets originate from hand/gun barrel. In MP, remove only
		// the HMD/playspace XY component so physical walking does not drag the
		// network fire origin away from the authoritative biped body.
		Vector3 stabilizedHandPos = handPos;

		if (bStabilizeMPFireOrigin)
		{
			stabilizedHandPos.x -= mpHMDWorldXYOffset.x;
			stabilizedHandPos.y -= mpHMDWorldXYOffset.y;
		}

		weaponFiredPlayer->position = stabilizedHandPos + handRotation * cachedViewModel.fireOffset;
		weaponFiredPlayer->aim = (cachedViewModel.fireRotation * handRotation3) * Vector3(1.0f, 0.0f, 0.0f);
	}

	// Hitbox Accurate mode uses Halo's native multiplayer position and aim so the
	// visible projectile follows the same direction used for hit detection.
	if (Game::instance.IsMultiplayerSession())
	{
		const CoopMapScope mapScope = GetCoopMapScope();

		if (ShouldUseHitboxAccurateProjectileMode(mapScope))
		{
			weaponFiredPlayer->position = realPlayerPosition;
			weaponFiredPlayer->aim = realPlayerAim;
		}
	}

#if DRAW_DEBUG_AIM
	Vector3 internalFireOffset = Helpers::GetCamera().position - realPlayerPosition;
	lastFireLocation = weaponFiredPlayer->position + internalFireOffset;
	lastFireAim = lastFireLocation + weaponFiredPlayer->aim * 1.0f;
#endif
}

bool WeaponHandler::HandleCoopProjectileCompatibility(
	const HaloID& weaponTagID,
	short triggerIndex)
{
	// A stale patch should never survive the matching PostFireWeapon, but restore
	// defensively before touching another loaded tag.
	RestoreCoopClientProjectile();

	if (!Game::instance.IsJoinedMultiplayerClient() || triggerIndex < 0)
	{
		return false;
	}

	Asset_Weapon* weapon = Helpers::GetTypedAsset<Asset_Weapon>(weaponTagID);
	if (!weapon || !IsReadableMemory(weapon, sizeof(Asset_Weapon)) || !weapon->WeaponData)
	{
		return false;
	}

	uint8_t* weaponData = reinterpret_cast<uint8_t*>(weapon->WeaponData);
	RuntimeTagReflexive* triggers = reinterpret_cast<RuntimeTagReflexive*>(
		weaponData + WEAPON_TRIGGER_REFLEXIVE_OFFSET);

	if (!IsReadableMemory(triggers, sizeof(RuntimeTagReflexive)) ||
		triggers->count <= 0 || triggers->count > 8 ||
		triggerIndex >= triggers->count || !triggers->elements)
	{
		return false;
	}

	uint8_t* trigger = triggers->elements + static_cast<size_t>(triggerIndex) * WEAPON_TRIGGER_SIZE;
	uint32_t* flags = reinterpret_cast<uint32_t*>(trigger);
	int16_t* projectilesPerShot = reinterpret_cast<int16_t*>(
		trigger + TRIGGER_PROJECTILES_PER_SHOT_OFFSET);
	HaloID* projectileTagID = reinterpret_cast<HaloID*>(
		trigger + TRIGGER_PROJECTILE_TAG_ID_OFFSET);

	if (!IsReadableMemory(trigger, WEAPON_TRIGGER_SIZE) ||
		!IsWritableMemory(flags, sizeof(*flags)) ||
		!IsWritableMemory(projectilesPerShot, sizeof(*projectilesPerShot)) ||
		!IsReadableMemory(projectileTagID, sizeof(*projectileTagID)) ||
		!FireGuardLooksValidHaloID(*projectileTagID))
	{
		return false;
	}

	Asset_Generic* projectile = Helpers::GetTypedAsset<Asset_Generic>(*projectileTagID);
	if (!projectile || !IsReadableMemory(projectile, sizeof(Asset_Generic)))
	{
		return false;
	}

	const char* projectilePathPointer = *reinterpret_cast<char* const*>(
		reinterpret_cast<const uint8_t*>(projectile) + 0x10);
	char weaponPath[260]{};
	char projectilePath[260]{};
	if (!ReadTagPath(weapon->WeaponAsset, weaponPath) ||
		!ReadTagPath(projectilePathPointer, projectilePath))
	{
		return false;
	}

	// Some official caches reuse stock paths for plasma-pistol and Needler shots.
	// Scope by exact map CRC first, then accept only the verified weapon/projectile
	// pairs. Normal LAN and rebuilt caches retain their native projectile behavior.
	const CoopMapScope mapScope = GetCoopMapScope();
	if (mapScope == CoopMapScope::OtherMap)
	{
		return false;
	}
	if (mapScope == CoopMapScope::OfficialRelease)
	{
		if (!IsSupportedCoopWeaponProjectilePair(weaponPath, projectilePath))
		{
			return false;
		}
	}
	else if (!IsOriginalCoopProjectilePath(projectilePath))
	{
		// If this executable variant defeats the header signature, retain the narrow
		// alpha_firefight fallback but never broaden stock-path suppression.
		return false;
	}

	const uint32_t originalFlags = *flags;
	const int16_t originalProjectileCount = *projectilesPerShot;
	if ((originalFlags & PROJECTILE_IS_CLIENT_SIDE_ONLY) == 0 || originalProjectileCount <= 0)
	{
		return false;
	}

	// Reaching this point means the current shot passed either the exact official
	// cache fingerprint and verified tag pair, or the narrow alpha_firefight
	// fallback. Both paths suppress the duplicate local projectile independently of
	// the normal-multiplayer projectile-origin setting.

	coopTriggerFlags = flags;
	coopProjectilesPerShot = projectilesPerShot;
	coopOriginalTriggerFlags = originalFlags;
	coopOriginalProjectilesPerShot = originalProjectileCount;

	// Clearing the flag removes the tag's client-only classification. Setting the
	// local count to zero is the decisive part: this joined client still sends its
	// normal fire input/ammo event, but it does not create the harmless local visual
	// projectile beside Mimic's damaging server-authoritative projectile.
	*coopTriggerFlags = originalFlags & ~PROJECTILE_IS_CLIENT_SIDE_ONLY;
	*coopProjectilesPerShot = 0;
	return true;
}

void WeaponHandler::RestoreCoopClientProjectile()
{
	if (!coopTriggerFlags && !coopProjectilesPerShot)
	{
		return;
	}

	const bool flagsWritable = coopTriggerFlags &&
		IsWritableMemory(coopTriggerFlags, sizeof(*coopTriggerFlags));
	const bool countWritable = coopProjectilesPerShot &&
		IsWritableMemory(coopProjectilesPerShot, sizeof(*coopProjectilesPerShot));

	if (flagsWritable)
	{
		*coopTriggerFlags = coopOriginalTriggerFlags;
	}
	if (countWritable)
	{
		*coopProjectilesPerShot = coopOriginalProjectilesPerShot;
	}

	coopTriggerFlags = nullptr;
	coopProjectilesPerShot = nullptr;
	coopOriginalTriggerFlags = 0;
	coopOriginalProjectilesPerShot = 0;
}

void WeaponHandler::PreFireWeapon(HaloID& WeaponID, short param2)
{
	RestoreVehicleMotionShotOverride();
	RestoreCoopClientProjectile();
	RestoreRemoteShotVisualAimOverride();

	BaseDynamicObject* Object = Helpers::GetDynamicObject(WeaponID);

	weaponFiredPlayer = nullptr;

	HaloID selectedPlayerID{ 0xFFFF, 0xFFFF };
	bool foundSelectedPlayer = Helpers::GetLocalPlayerID(selectedPlayerID);
	UnitDynamicObject* selectedUnit = nullptr;
	if (foundSelectedPlayer)
	{
		BaseDynamicObject* selectedObj = Helpers::GetDynamicObject(selectedPlayerID);
		if (selectedObj && selectedObj->N0000027E == ObjectType::BIPED)
		{
			selectedUnit = reinterpret_cast<UnitDynamicObject*>(selectedObj);
		}
	}

	const bool bMultiplayer = Game::instance.IsMultiplayerSession();

	HaloID localVRBipedID = selectedPlayerID;
	UnitDynamicObject* localVRUnit = selectedUnit;
	if (bMultiplayer && !localVRUnit)
	{
		// Compatibility fallback only. A valid exact selected local biped may never
		// be replaced by a nearer remote player or vehicle occupant.
		localVRUnit =
			ResolveMPFireGuardLocalVRBiped(localVRBipedID);
	}

	if (!Object)
	{
		return;
	}

	// Resolve fire ownership from live weapon, biped, vehicle and seat relationships.
	HaloID firePlayerID{ 0xFFFF, 0xFFFF };
	bool bHaveFirePlayer = false;

	BaseDynamicObject* parentObj =
		Helpers::GetDynamicObject(Object->parent);
	UnitDynamicObject* fireOwnerUnit = nullptr;
	HaloID fireOwnerID{ 0xFFFF, 0xFFFF };
	if (parentObj &&
		parentObj->N0000027E == ObjectType::BIPED)
	{
		fireOwnerUnit =
			reinterpret_cast<UnitDynamicObject*>(parentObj);
		fireOwnerID = Object->parent;
	}
	else if (parentObj &&
		parentObj->N0000027E == ObjectType::VEHICLE)
	{
		fireOwnerUnit = ResolveExactVehicleFireOwner(
			Object->parent,
			WeaponID,
			parentObj,
			Object,
			fireOwnerID);
	}

	if (bMultiplayer)
	{
		const bool bHaveExactLocalOwner =
			localVRUnit != nullptr &&
			FireGuardLooksValidHaloID(localVRBipedID) &&
			FireGuardLooksValidHaloID(localVRUnit->playerID);

		const bool bHaveExactFireOwner =
			fireOwnerUnit != nullptr &&
			FireGuardLooksValidHaloID(fireOwnerID) &&
			FireGuardLooksValidHaloID(fireOwnerUnit->playerID);

		const bool bExactBipedIDMatch =
			bHaveExactLocalOwner &&
			bHaveExactFireOwner &&
			FireGuardSameHaloID(fireOwnerID, localVRBipedID);

		const bool bExactPlayerIDMatch =
			bHaveExactLocalOwner &&
			bHaveExactFireOwner &&
			FireGuardSameHaloID(
				fireOwnerUnit->playerID,
				localVRUnit->playerID);

		const bool bLocalVRFire =
			bExactBipedIDMatch ||
			bExactPlayerIDMatch;

		if (!bLocalVRFire)
		{
			// Unresolved or ambiguous vehicle calls remain completely vanilla.
			if (bHaveExactLocalOwner &&
				bHaveExactFireOwner &&
				IsFiniteVisualAim(fireOwnerUnit->aim))
			{
				BeginRemoteShotVisualAimOverride(
					localVRUnit,
					fireOwnerUnit->aim);
			}
			return;
		}

		firePlayerID = fireOwnerID;
		bHaveFirePlayer = true;
	}
	else if (fireOwnerUnit)
	{
		firePlayerID = fireOwnerID;
		bHaveFirePlayer = true;
	}
	else if (foundSelectedPlayer && selectedUnit)
	{
		firePlayerID = selectedPlayerID;
		bHaveFirePlayer = true;
	}

	if (!bHaveFirePlayer)
	{
		return;
	}

	HandleWeaponHaptics();
	Game::instance.weaponHapticsConfig.WeaponFired(cachedViewModel.weaponType);

	// Apply motion aim only to vehicle-owned weapons. Passenger handheld weapons
	// remain on the normal on-foot relocation path.
	BaseDynamicObject* directWeaponParentVehicle =
		parentObj &&
		parentObj->N0000027E ==
			ObjectType::VEHICLE
			? parentObj
			: nullptr;

	const bool bVehicleMotionShot =
		BeginVehicleMotionShotOverride(
			Object,
			fireOwnerUnit,
			directWeaponParentVehicle);

	// Most weapons intentionally relocate Halo's temporary firing carrier to the
	// tracked VR muzzle. The Banshee and Scorpion are exceptions: Halo already uses
	// their vehicle crosshair as the authoritative aim, so leave their carrier
	// untouched to keep the visual projectile on that same crosshair path. Resolve
	// the vehicle from either a vehicle-owned weapon or its attached firing biped.
	BaseDynamicObject* fireVehicle = nullptr;
	if (parentObj && parentObj->N0000027E == ObjectType::VEHICLE)
	{
		fireVehicle = parentObj;
	}
	else if (fireOwnerUnit &&
		FireGuardLooksValidHaloID(fireOwnerUnit->parent))
	{
		BaseDynamicObject* attachedParent =
			Helpers::GetDynamicObject(fireOwnerUnit->parent);
		if (attachedParent &&
			attachedParent->N0000027E == ObjectType::VEHICLE)
		{
			fireVehicle = attachedParent;
		}
	}

	if (!bVehicleMotionShot &&
		!ShouldUseNativeVehicleProjectileVisuals(
			fireVehicle))
	{
		RelocatePlayer(firePlayerID);
	}

	// Official Coop Evolved marks these local projectiles client-side-only while
	// Mimic also creates the authoritative server projectile. Suppression performs
	// its own release-cache and exact weapon/trigger checks; stock multiplayer,
	// rebuilt Coop caches and remote players remain untouched.
	if (Game::instance.IsJoinedMultiplayerClient())
	{
		HandleCoopProjectileCompatibility(Object->tagID, param2);
	}
}

void WeaponHandler::SetPlasmaPistolCharge()
{
	HaloID PlayerID;
	bool foundPlayer = Helpers::GetLocalPlayerID(PlayerID);

	if (foundPlayer && cachedViewModel.weaponType == WeaponType::PlasmaPistol)
	{
		Game::instance.weaponHapticsConfig.SetPlasmaPistolCharging();
	}
}

void WeaponHandler::HandlePlasmaPistolCharge()
{
	IVR* vr = Game::instance.GetVR();

	if (vr && cachedViewModel.weaponType == WeaponType::PlasmaPistol)
	{
		bool isCharging = Game::instance.weaponHapticsConfig.IsPlasmaPistolCharging();

		if (isCharging)
		{
			HandleWeaponHaptics();
		}
	}
}

inline void WeaponHandler::HandleWeaponHaptics() const
{
	IVR* vr = Game::instance.GetVR();

	if (cachedViewModel.weaponType != WeaponType::Unknown)
	{
		WeaponHapticsConfigManager hapticsManager = Game::instance.weaponHapticsConfig;
		WeaponHaptic haptic = hapticsManager.GetWeaponHaptics(cachedViewModel.weaponType);

#if HAPTICS_DEBUG
		Logger::log << "[Weapon Haptics] triggering haptics for weapon " << haptic.Description << std::endl;
#endif

		ControllerRole dominantHand = ControllerRole::Right;
		ControllerRole nondominantHand = ControllerRole::Left;

		if (Game::instance.bLeftHanded)
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Left is hand dominant hand. " << haptic.Description << std::endl;
#endif

			dominantHand = ControllerRole::Left;
			nondominantHand = ControllerRole::Right;
		}
		else
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Right is hand dominant hand. " << haptic.Description << std::endl;
#endif
		}

		if (Game::instance.bUseTwoHandAim)
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Gun is in two handed mode. " << haptic.Description << std::endl;
#endif
			WeaponHapticArg dominantHaptics = haptic.TwoHand.Dominant;
			WeaponHapticArg nondominantHaptics = haptic.TwoHand.Nondominant;
			hapticsManager.HandleWeaponHaptics(vr, dominantHand, dominantHaptics);
			hapticsManager.HandleWeaponHaptics(vr, nondominantHand, nondominantHaptics);
		}
		else
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Gun is in one handed mode. " << haptic.Description << std::endl;
#endif
			WeaponHapticArg hapticArgs = haptic.OneHand;
			hapticsManager.HandleWeaponHaptics(vr, dominantHand, hapticArgs);
		}
	}
}

void WeaponHandler::PostFireWeapon(HaloID& weaponID, short param2)
{
	// The motion-controlled vehicle firing transform exists only around
	// FireWeapon.Original(). Restore it before any later update or render pass.
	RestoreVehicleMotionShotOverride();

	// The exact remote-owner carrier override exists only around
	// FireWeapon.Original(); restore the local VR biped immediately.
	RestoreRemoteShotVisualAimOverride();

	// Restore the loaded weapon tag before restoring the temporary biped firing
	// state. This runs immediately after Halo's original FireWeapon call.
	RestoreCoopClientProjectile();

	// Restore state after firing the weapon
	if (weaponFiredPlayer)
	{
		weaponFiredPlayer->position = realPlayerPosition;
		weaponFiredPlayer->aim = realPlayerAim;
		weaponFiredPlayer = nullptr;
	}
	Game::instance.EndTemporaryFireRelocation();

}

void WeaponHandler::PreThrowGrenade(HaloID& playerID)
{
	weaponFiredPlayer = nullptr;

	// Check if the weapon is being used by the player
	HaloID PlayerID;
	if (Helpers::GetLocalPlayerID(PlayerID) && PlayerID == playerID)
	{
		RelocatePlayer(PlayerID);
	}
}

void WeaponHandler::PostThrowGrenade(HaloID& playerID)
{
	if (weaponFiredPlayer)
	{
		weaponFiredPlayer->position = realPlayerPosition;
		weaponFiredPlayer->aim = realPlayerAim;
		weaponFiredPlayer = nullptr;
	}
	Game::instance.EndTemporaryFireRelocation();
}
