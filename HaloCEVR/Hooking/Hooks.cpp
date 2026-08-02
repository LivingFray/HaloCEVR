#include "Hooks.h"
#include "../Helpers/Player.h"
#include "../Helpers/Renderer.h"
#include "../Helpers/DX9.h"
#include "../Helpers/Cutscene.h"
#include "../Game.h"
#include "../Helpers/Menus.h"
#include "../Helpers/Maths.h"
#include "../Helpers/Objects.h"
#include "../Helpers/Assets.h"
#include "../Helpers/Version.h"

#include <tlhelp32.h>
#include <cmath>
#include <cstring>
#include "../DirectXWrappers/IDirect3DDevice9ExWrapper.h"
#include "../Helpers/CmdLineArgs.h"

#include "../Profiler.h"

#define CREATEHOOK(Func) Func##.CreateHook(#Func, o.##Func##, &H_##Func##)
#define RESOLVEINDIRECT(Name) ResolveIndirect(o.I_##Name, o.##Name)

namespace
{
	using HudRenderPlayersFunction = void(__cdecl*)();
	using HudRenderNavPointsFunction = void(__cdecl*)(void*);

	class HudRenderPlayersHook final :
		public Hook<HudRenderPlayersFunction>
	{
	public:
		void CallOriginal()
		{
			if (Original)
			{
				Original();
			}
		}
	};

	class HudRenderNavPointsHook final :
		public Hook<HudRenderNavPointsFunction>
	{
	public:
		void CallOriginal(void* arg0)
		{
			if (Original)
			{
				Original(arg0);
			}
		}
	};

	// OpenSauce's Halo Custom Edition 1.10 layout identifies the dedicated
	// teammate/player-indicator function at absolute 0x4AD830. HaloCEVR offsets
	// are module-relative, so the matching image offset is 0x0AD830.
	Offset g_hudRenderPlayersOffset{
		"HudRenderPlayers",
		0x0AD830,
		""
	};
	HudRenderPlayersHook g_hudRenderPlayersHook{};
	bool g_bHudRenderPlayersHookCreated = false;

	// Mission waypoints, object markers and multiplayer game-goal indicators
	// are rendered by Halo's separate HUD nav-point function at 0x4B2F80.
	Offset g_hudRenderNavPointsOffset{
		"HudRenderNavPoints",
		0x0B2F80,
		""
	};
	HudRenderNavPointsHook g_hudRenderNavPointsHook{};
	bool g_bHudRenderNavPointsHookCreated = false;

	void __cdecl H_HudRenderPlayers()
	{
		const bool bProjectionOverridden =
			Game::instance.BeginWorldSpaceIndicatorProjectionOverride();

		g_hudRenderPlayersHook.CallOriginal();

		if (bProjectionOverridden)
		{
			Game::instance.EndWorldSpaceIndicatorProjectionOverride();
		}
	}

	void __cdecl H_HudRenderNavPoints(void* arg0)
	{
		const bool bProjectionOverridden =
			Game::instance.BeginWorldSpaceIndicatorProjectionOverride();

		g_hudRenderNavPointsHook.CallOriginal(arg0);

		if (bProjectionOverridden)
		{
			Game::instance.EndWorldSpaceIndicatorProjectionOverride();
		}
	}

	bool ResolveRelativeCallTargetOffset(
		long long callOffset,
		long long& outTargetOffset)
	{
		HMODULE module = GetModuleHandleA(nullptr);
		if (!module)
		{
			return false;
		}

		MODULEINFO moduleInfo{};
		if (!GetModuleInformation(
			GetCurrentProcess(),
			module,
			&moduleInfo,
			sizeof(moduleInfo)))
		{
			return false;
		}

		if (callOffset < 0 ||
			callOffset + 5 >
				static_cast<long long>(moduleInfo.SizeOfImage))
		{
			return false;
		}

		const uint8_t* image =
			reinterpret_cast<const uint8_t*>(
				moduleInfo.lpBaseOfDll);
		if (image[callOffset] != 0xE8)
		{
			return false;
		}

		int32_t relativeTarget = 0;
		std::memcpy(
			&relativeTarget,
			image + callOffset + 1,
			sizeof(relativeTarget));

		const long long targetOffset =
			callOffset + 5 +
			static_cast<long long>(relativeTarget);
		if (targetOffset < 0 ||
			targetOffset >=
				static_cast<long long>(moduleInfo.SizeOfImage))
		{
			return false;
		}

		outTargetOffset = targetOffset;
		return true;
	}

	bool ResolveRetailWorldSpaceIndicatorOffsets(
		long long& outPlayersOffset,
		long long& outNavPointsOffset)
	{
		// Proven Custom Edition 1.10 layout:
		//   DrawCrosshair        0x0AFEC0
		//   HudRenderPlayers    0x0AD830
		//   HudRenderNavPoints  0x0B2F80
		//   Players CALL        0x0ACA6E
		//   Nav-points CALL     0x0ACAED
		//
		// Retail and Custom Edition retain this local interface layout while
		// earlier executable sections shift. Base the search on DrawCrosshair,
		// which HaloCEVR has already located by a full byte signature.
		constexpr long long CeDrawCrosshair = 0x0AFEC0;
		constexpr long long CePlayersCall = 0x0ACA6E;
		constexpr long long CeNavPointsCall = 0x0ACAED;
		constexpr long long CePlayersTarget = 0x0AD830;
		constexpr long long CeNavPointsTarget = 0x0B2F80;

		constexpr long long PlayersCallDelta =
			CePlayersCall - CeDrawCrosshair;
		constexpr long long NavPointsCallDelta =
			CeNavPointsCall - CeDrawCrosshair;
		constexpr long long PlayersTargetDelta =
			CePlayersTarget - CeDrawCrosshair;
		constexpr long long NavPointsTargetDelta =
			CeNavPointsTarget - CeDrawCrosshair;
		constexpr long long CallSpacing =
			CeNavPointsCall - CePlayersCall;

		const long long drawCrosshairOffset =
			Hooks::o.DrawCrosshair.Offset;
		const long long expectedPlayersCall =
			drawCrosshairOffset + PlayersCallDelta;
		const long long expectedPlayersTarget =
			drawCrosshairOffset + PlayersTargetDelta;
		const long long expectedNavPointsTarget =
			drawCrosshairOffset + NavPointsTargetDelta;

		// First try the exact relative layout. If a small retail-only insertion
		// exists in the parent HUD function, search nearby while requiring:
		//   - both instructions are CALL rel32;
		//   - the calls remain exactly 0x7F apart;
		//   - both destinations remain close to their expected local targets.
		constexpr long long SearchRadius = 0x600;
		constexpr long long TargetTolerance = 0x1800;

		for (long long adjustment = 0;
			adjustment <= SearchRadius;
			++adjustment)
		{
			const long long signedAdjustments[2] =
			{
				adjustment,
				-adjustment
			};

			for (long long signedAdjustment : signedAdjustments)
			{
				if (adjustment == 0 &&
					signedAdjustment < 0)
				{
					continue;
				}

				const long long playersCall =
					expectedPlayersCall +
					signedAdjustment;
				const long long navPointsCall =
					playersCall +
					CallSpacing;

				long long playersTarget = -1;
				long long navPointsTarget = -1;
				if (!ResolveRelativeCallTargetOffset(
						playersCall,
						playersTarget) ||
					!ResolveRelativeCallTargetOffset(
						navPointsCall,
						navPointsTarget))
				{
					continue;
				}

				const long long playersDistance =
					std::llabs(
						playersTarget -
						expectedPlayersTarget);
				const long long navPointsDistance =
					std::llabs(
						navPointsTarget -
						expectedNavPointsTarget);

				if (playersDistance >
						TargetTolerance ||
					navPointsDistance >
						TargetTolerance)
				{
					continue;
				}

				// Sanity checks matching the known interface layout:
				// player indicators precede DrawCrosshair, nav points follow it.
				if (playersTarget >=
						drawCrosshairOffset ||
					navPointsTarget <=
						drawCrosshairOffset ||
					playersTarget ==
						navPointsTarget)
				{
					continue;
				}

				outPlayersOffset = playersTarget;
				outNavPointsOffset = navPointsTarget;
				return true;
			}
		}

		return false;
	}

	void CreateWorldSpaceIndicatorHooks(
		const char* gameType)
	{
		const bool bIsRetail =
			gameType &&
			std::strcmp("halor", gameType) == 0;

		if (!bIsRetail)
		{
			// Proven Halo Custom Edition 1.10 addresses.
			g_hudRenderPlayersOffset.Offset = 0x0AD830;
			g_hudRenderNavPointsOffset.Offset = 0x0B2F80;
		}
		else
		{
			long long playersOffset = -1;
			long long navPointsOffset = -1;

			if (!ResolveRetailWorldSpaceIndicatorOffsets(
					playersOffset,
					navPointsOffset))
			{
				Logger::err
					<< "[Hook] Could not safely resolve Halo Retail "
					<< "world-space indicator routines; skipping them"
					<< std::endl;
				return;
			}

			g_hudRenderPlayersOffset.Offset =
				playersOffset;
			g_hudRenderNavPointsOffset.Offset =
				navPointsOffset;

		}

		g_hudRenderPlayersHook.CreateHook(
			"HudRenderPlayers",
			g_hudRenderPlayersOffset,
			reinterpret_cast<LPVOID>(
				&H_HudRenderPlayers));
		g_bHudRenderPlayersHookCreated = true;

		g_hudRenderNavPointsHook.CreateHook(
			"HudRenderNavPoints",
			g_hudRenderNavPointsOffset,
			reinterpret_cast<LPVOID>(
				&H_HudRenderNavPoints));
		g_bHudRenderNavPointsHookCreated = true;
	}

	struct ArmedPassengerOnFootTransportState
	{
		bool active = false;
		HaloID localBipedID{ 0xFFFF, 0xFFFF };
		HaloID localPlayerID{ 0xFFFF, 0xFFFF };
		HaloID vehicleID{ 0xFFFF, 0xFFFF };
		HaloID weaponID{ 0xFFFF, 0xFFFF };
		uint32_t seatIndex = 0xFFFFFFFF;

		Vector3 trackedAim{};
		Camera savedCamera{};
		Vector3 savedLastLookDir{};
	};

	ArmedPassengerOnFootTransportState
		g_armedPassengerOnFootTransport{};

	struct VehicleTagPathView
	{
		char pad_0000[16];
		const char* path;
	};

	bool IsShadeTagPath(
		const char* tagPath)
	{
		return tagPath &&
			(std::strstr(tagPath, "shade") != nullptr ||
				std::strstr(tagPath, "c gun turret") != nullptr ||
				std::strstr(tagPath, "c_gun_turret") != nullptr ||
				std::strstr(tagPath, "c-gun-turret") != nullptr ||
				std::strstr(tagPath, "covenant_turret") != nullptr ||
				std::strstr(tagPath, "covenant turret") != nullptr ||
				std::strstr(tagPath, "plasma_turret") != nullptr);
	}

	struct VehicleSeatClassification
	{
		bool supported = false;
		bool warthogDriver = false;
		bool warthogSidePassenger = false;
	};

	VehicleSeatClassification
		ClassifyVehicleSeat(
			BaseDynamicObject* parentVehicle,
			UnitDynamicObject* localUnit)
	{
		VehicleSeatClassification result{};

		if (!parentVehicle ||
			!localUnit ||
			parentVehicle->N0000027E != ObjectType::VEHICLE)
		{
			return result;
		}

		Asset_Generic* vehicleTag =
			Helpers::GetTypedAsset<Asset_Generic>(
				parentVehicle->tagID);
		const VehicleTagPathView* tagView =
			reinterpret_cast<
				const VehicleTagPathView*>(
					vehicleTag);

		if (!tagView || !tagView->path)
		{
			return result;
		}

		const bool bIsWarthog =
			std::strstr(tagView->path, "warthog") != nullptr;
		const bool bIsGhost =
			std::strstr(tagView->path, "ghost") != nullptr;
		const bool bIsScorpion =
			std::strstr(tagView->path, "scorpion") != nullptr;
		const bool bIsBanshee =
			std::strstr(tagView->path, "banshee") != nullptr;
		const bool bIsShade =
			IsShadeTagPath(tagView->path);

		const uint32_t seatLowByte =
			localUnit->parentSeatIndex & 0xFFu;
		const bool bHasLocalWeapon =
			localUnit->weapon.id != 0xFFFF &&
			localUnit->weapon.index != 0xFFFF;

		result.warthogDriver =
			bIsWarthog &&
			seatLowByte == 0 &&
			!bHasLocalWeapon;
		const bool bWarthogGunner =
			bIsWarthog &&
			seatLowByte == 1;
		result.warthogSidePassenger =
			bIsWarthog &&
			seatLowByte == 2;
		const bool bScorpionDriver =
			bIsScorpion &&
			seatLowByte == 0;

		result.supported =
			result.warthogDriver ||
			bWarthogGunner ||
			bIsGhost ||
			bScorpionDriver ||
			bIsBanshee ||
			bIsShade;

		return result;
	}

	struct VehicleModelAimState
	{
		bool cameraOverrideActive = false;
		bool contextActive = false;
		HaloID parentID{
			0xFFFF,
			0xFFFF
		};
		uint32_t seatLowByte = 0xFFFFFFFFu;

		Camera savedCamera{};
		Vector3 savedLastLookDir{};

		bool activationComplete = false;
		std::chrono::steady_clock::time_point
			seatEntryTime =
				std::chrono::steady_clock::now();

		bool haveNativeCameraSample = false;
		Vector3 lastNativeCameraAim{};
		float pendingYaw = 0.0f;
		float pendingPitch = 0.0f;
	};

	VehicleModelAimState g_vehicleModelAim{};

	float NormalizeVehicleModelAimRadians(
		float radians)
	{
		constexpr float Pi =
			3.14159265358979323846f;
		constexpr float TwoPi =
			Pi * 2.0f;

		while (radians > Pi)
		{
			radians -= TwoPi;
		}
		while (radians < -Pi)
		{
			radians += TwoPi;
		}

		return radians;
	}

	void RestoreVehicleModelAimCamera()
	{
		VehicleModelAimState& state =
			g_vehicleModelAim;

		if (!state.cameraOverrideActive)
		{
			return;
		}

		Helpers::GetCamera() =
			state.savedCamera;
		Game::instance.LastLookDir =
			state.savedLastLookDir;
		state.cameraOverrideActive = false;
	}

	void ResetVehicleModelAimState()
	{
		RestoreVehicleModelAimCamera();

		VehicleModelAimState& state =
			g_vehicleModelAim;
		state =
			VehicleModelAimState{};
	}

	bool PrepareVehicleModelAimCommand(
		float& outYaw,
		float& outPitch)
	{
		RestoreVehicleModelAimCamera();

		const bool bEnabled =
			Game::instance.c_MotionControlledVehicles &&
			Game::instance.c_MotionControlledVehicles->Value();

		if (!bEnabled)
		{
			ResetVehicleModelAimState();
			return false;
		}

		UnitDynamicObject* localUnit =
			static_cast<UnitDynamicObject*>(
				Helpers::GetLocalPlayer());
		if (!localUnit ||
			localUnit->parent.id == 0xFFFF ||
			localUnit->parent.index == 0xFFFF)
		{
			ResetVehicleModelAimState();
			return false;
		}

		BaseDynamicObject* parentVehicle =
			Helpers::GetDynamicObject(
				localUnit->parent);
		if (!parentVehicle ||
			parentVehicle->N0000027E !=
				ObjectType::VEHICLE)
		{
			ResetVehicleModelAimState();
			return false;
		}

		const VehicleSeatClassification vehicleSeat =
			ClassifyVehicleSeat(
				parentVehicle,
				localUnit);
		const uint32_t seatLowByte =
			localUnit->parentSeatIndex &
			0xFFu;

		if (!vehicleSeat.supported ||
			vehicleSeat.warthogSidePassenger)
		{
			// Explicitly supported:
			// Warthog driver/gunner, Ghost, Scorpion, Banshee and Shade.
			// The Warthog side passenger keeps handheld/on-foot VR behavior.
			ResetVehicleModelAimState();
			return false;
		}

		const bool bIgnoredWarthogDriver =
			vehicleSeat.warthogDriver &&
			Game::instance
				.c_DisableWarthogSteeringMotionControls &&
			Game::instance
				.c_DisableWarthogSteeringMotionControls
				->Value();

		if (bIgnoredWarthogDriver)
		{
			ResetVehicleModelAimState();
			return false;
		}

		VehicleModelAimState& state =
			g_vehicleModelAim;
		const bool bNewContext =
			!state.contextActive ||
			state.parentID.id !=
				localUnit->parent.id ||
			state.parentID.index !=
				localUnit->parent.index ||
			state.seatLowByte !=
				seatLowByte;

		const auto now =
			std::chrono::steady_clock::now();

		const float activationDelay =
			VehicleReleasePolicy::ModelAimActivationDelaySeconds;
		const float transitionSeconds =
			VehicleReleasePolicy::ModelAimTransitionSeconds;

		if (bNewContext)
		{
			state =
				VehicleModelAimState{};
			state.contextActive = true;
			state.parentID =
				localUnit->parent;
			state.seatLowByte =
				seatLowByte;
			state.seatEntryTime = now;
		}

		const float seatedSeconds =
			std::chrono::duration<float>(
				now -
					state.seatEntryTime)
				.count();

		if (seatedSeconds < activationDelay)
		{
			// Keep native right-stick model control while the entry animation
			// settles. Reticle and projectile aim remain controller-tracked.
			return false;
		}

		if (!state.activationComplete)
		{
			state.activationComplete = true;
			state.haveNativeCameraSample = false;
			state.pendingYaw = 0.0f;
			state.pendingPitch = 0.0f;
		}

		Vector3 trackedPosition{};
		Vector3 trackedAim{};
		Vector3 trackedUp{};
		if (!Game::instance.GetVehicleVRAim(
				trackedPosition,
				trackedAim,
				trackedUp) ||
			!(std::isfinite(trackedAim.x) &&
				std::isfinite(trackedAim.y) &&
				std::isfinite(trackedAim.z) &&
				trackedAim.lengthSqr() >
					0.000001f))
		{
			return false;
		}

		Vector3 nativeCameraAim =
			Helpers::GetCamera().lookDir;
		if (!(std::isfinite(nativeCameraAim.x) &&
				std::isfinite(nativeCameraAim.y) &&
				std::isfinite(nativeCameraAim.z) &&
				nativeCameraAim.lengthSqr() >
					0.000001f))
		{
			return false;
		}

		trackedAim.normalize();
		nativeCameraAim.normalize();

		const bool bCameraSampleChanged =
			!state.haveNativeCameraSample ||
			(nativeCameraAim -
				state.lastNativeCameraAim)
					.lengthSqr() >
				0.0000000001f;

		if (bCameraSampleChanged)
		{
			state.haveNativeCameraSample = true;
			state.lastNativeCameraAim =
				nativeCameraAim;
			state.pendingYaw = 0.0f;
			state.pendingPitch = 0.0f;
		}

		const float nativeYaw =
			std::atan2(
				nativeCameraAim.y,
				nativeCameraAim.x);
		const float trackedYaw =
			std::atan2(
				trackedAim.y,
				trackedAim.x);
		const float nativePitch =
			std::atan2(
				nativeCameraAim.z,
				std::sqrt(
					nativeCameraAim.x *
						nativeCameraAim.x +
					nativeCameraAim.y *
						nativeCameraAim.y));
		const float trackedPitch =
			std::atan2(
				trackedAim.z,
				std::sqrt(
					trackedAim.x *
						trackedAim.x +
					trackedAim.y *
						trackedAim.y));

		const float observedYawError =
			NormalizeVehicleModelAimRadians(
				trackedYaw -
				nativeYaw);
		const float observedPitchError =
			NormalizeVehicleModelAimRadians(
				trackedPitch -
				nativePitch);

		float linearTransition =
			(seatedSeconds -
				activationDelay) /
			transitionSeconds;

		if (linearTransition < 0.0f)
		{
			linearTransition = 0.0f;
		}
		else if (linearTransition > 1.0f)
		{
			linearTransition = 1.0f;
		}

		const float transitionBlend =
			linearTransition *
			linearTransition *
			(3.0f -
				2.0f *
					linearTransition);

		const float transitionYawError =
			NormalizeVehicleModelAimRadians(
				observedYawError *
					transitionBlend);
		const float transitionPitchError =
			NormalizeVehicleModelAimRadians(
				observedPitchError *
					transitionBlend);
		const float remainingYawError =
			NormalizeVehicleModelAimRadians(
				transitionYawError -
					state.pendingYaw);
		const float remainingPitchError =
			NormalizeVehicleModelAimRadians(
				transitionPitchError -
					state.pendingPitch);

		float frameSeconds =
			Game::instance.lastDeltaTime;
		if (!std::isfinite(frameSeconds) ||
			frameSeconds <= 0.0f)
		{
			frameSeconds =
				1.0f / 90.0f;
		}
		else if (frameSeconds > 0.05f)
		{
			frameSeconds = 0.05f;
		}

		const float maximumTurnSpeed =
			VehicleReleasePolicy::MaximumTurnSpeedDegreesPerSecond;

		const float maximumStep =
			maximumTurnSpeed *
			(3.14159265358979323846f /
				180.0f) *
			frameSeconds;

		auto clampStep =
			[maximumStep](float value)
		{
			if (value > maximumStep)
			{
				return maximumStep;
			}
			if (value < -maximumStep)
			{
				return -maximumStep;
			}
			return value;
		};

		outYaw =
			clampStep(
				remainingYawError);
		outPitch =
			clampStep(
				remainingPitchError);

		state.pendingYaw =
			NormalizeVehicleModelAimRadians(
				state.pendingYaw +
				outYaw);
		state.pendingPitch =
			NormalizeVehicleModelAimRadians(
				state.pendingPitch +
				outPitch);

		// Save the already-correct native camera only after the native first pass.
		// The second original call may rotate the turret/vehicle and its temporary
		// camera together; the camera is restored immediately afterward.
		state.savedCamera =
			Helpers::GetCamera();
		state.savedLastLookDir =
			Game::instance.LastLookDir;
		state.cameraOverrideActive = true;

		return true;
	}

	void FinishVehicleModelAimCommand()
	{
		RestoreVehicleModelAimCamera();
	}

	void ApplyVehicleMotionAim()
	{
		const bool bEnabled =
			Game::instance.c_MotionControlledVehicles &&
			Game::instance.c_MotionControlledVehicles->Value();

		if (!bEnabled)
		{
			return;
		}

		UnitDynamicObject* localUnit =
			static_cast<UnitDynamicObject*>(
				Helpers::GetLocalPlayer());
		if (!localUnit ||
			localUnit->parent.id == 0xFFFF ||
			localUnit->parent.index == 0xFFFF)
		{
			return;
		}

		BaseDynamicObject* parentVehicle =
			Helpers::GetDynamicObject(
				localUnit->parent);
		if (!parentVehicle ||
			parentVehicle->N0000027E !=
				ObjectType::VEHICLE)
		{
			return;
		}

		Vector3 trackedPosition{};
		Vector3 trackedAim{};
		Vector3 trackedUp{};
		if (!Game::instance.GetVehicleVRAim(
				trackedPosition,
				trackedAim,
				trackedUp) ||
			!(std::isfinite(trackedAim.x) &&
				std::isfinite(trackedAim.y) &&
				std::isfinite(trackedAim.z) &&
				trackedAim.lengthSqr() >
					0.000001f))
		{
			return;
		}

		trackedAim.normalize();

		localUnit->desiredAim = trackedAim;
		localUnit->aim = trackedAim;
		localUnit->aim2 = trackedAim;
		localUnit->aim3 = trackedAim;
		localUnit->aimVelocity =
			Vector3(
				0.0f,
				0.0f,
				0.0f);
	}

	bool SamePassengerHaloID(const HaloID& a, const HaloID& b)
	{
		return a.index == b.index && a.id == b.id;
	}

	bool ValidPassengerHaloID(const HaloID& id)
	{
		return id.index != 0xFFFF && id.id != 0xFFFF;
	}

	bool FinitePassengerAim(const Vector3& aim)
	{
		return std::isfinite(aim.x) &&
			std::isfinite(aim.y) &&
			std::isfinite(aim.z) &&
			aim.lengthSqr() > 0.000001f;
	}

	float NormalizePassengerRadians(float value)
	{
		constexpr float Pi = 3.14159265358979323846f;
		constexpr float TwoPi = Pi * 2.0f;

		while (value > Pi)
		{
			value -= TwoPi;
		}
		while (value < -Pi)
		{
			value += TwoPi;
		}
		return value;
	}

	bool ResolveExactLocalArmedPassenger(
		HaloID& outBipedID,
		UnitDynamicObject*& outUnit,
		HaloID& outVehicleID,
		HaloID& outWeaponID)
	{
		outBipedID = HaloID{ 0xFFFF, 0xFFFF };
		outUnit = nullptr;
		outVehicleID = HaloID{ 0xFFFF, 0xFFFF };
		outWeaponID = HaloID{ 0xFFFF, 0xFFFF };

		if (!Game::instance.IsMultiplayerSession() ||
			!Helpers::GetLocalPlayerID(outBipedID))
		{
			return false;
		}

		BaseDynamicObject* localObject =
			Helpers::GetDynamicObject(outBipedID);
		if (!localObject ||
			localObject->N0000027E != ObjectType::BIPED)
		{
			return false;
		}

		UnitDynamicObject* localUnit =
			reinterpret_cast<UnitDynamicObject*>(localObject);
		if (!ValidPassengerHaloID(localUnit->playerID) ||
			!ValidPassengerHaloID(localUnit->parent) ||
			!ValidPassengerHaloID(localUnit->weapon))
		{
			return false;
		}

		BaseDynamicObject* vehicle =
			Helpers::GetDynamicObject(localUnit->parent);
		BaseDynamicObject* weapon =
			Helpers::GetDynamicObject(localUnit->weapon);

		if (!vehicle ||
			vehicle->N0000027E != ObjectType::VEHICLE ||
			!weapon ||
			weapon->N0000027E != ObjectType::WEAPON)
		{
			return false;
		}

		// Exact local armed passenger:
		// local PlayerDatum biped -> vehicle parent
		// local equipped weapon -> parent is that exact biped
		//
		// This excludes remote occupants and vehicle-owned gunner weapons.
		if (!SamePassengerHaloID(weapon->parent, outBipedID))
		{
			return false;
		}

		outUnit = localUnit;
		outVehicleID = localUnit->parent;
		outWeaponID = localUnit->weapon;
		return true;
	}

	void RestoreArmedPassengerOnFootTransportDefensively()
	{
		if (!g_armedPassengerOnFootTransport.active)
		{
			return;
		}

		const ArmedPassengerOnFootTransportState state =
			g_armedPassengerOnFootTransport;
		g_armedPassengerOnFootTransport.active = false;

		BaseDynamicObject* object =
			Helpers::GetDynamicObject(
				const_cast<HaloID&>(state.localBipedID));
		if (object &&
			object->N0000027E == ObjectType::BIPED)
		{
			UnitDynamicObject* unit =
				reinterpret_cast<UnitDynamicObject*>(object);
			unit->parent = state.vehicleID;
			unit->parentSeatIndex = state.seatIndex;
		}

		Helpers::GetCamera() = state.savedCamera;
		Game::instance.LastLookDir =
			state.savedLastLookDir;
	}

	void PrepareArmedPassengerOnFootTransport(
		float& yaw,
		float& pitch)
	{
		// Restore any interrupted transport before starting another update.
		RestoreArmedPassengerOnFootTransportDefensively();

		HaloID localBipedID{ 0xFFFF, 0xFFFF };
		HaloID vehicleID{ 0xFFFF, 0xFFFF };
		HaloID weaponID{ 0xFFFF, 0xFFFF };
		UnitDynamicObject* localUnit = nullptr;

		if (!ResolveExactLocalArmedPassenger(
			localBipedID,
			localUnit,
			vehicleID,
			weaponID))
		{
			return;
		}

		Vector3 trackedPosition{};
		Vector3 trackedAim{};
		Vector3 trackedUp{};
		if (!Game::instance.GetWorldWeaponAim(
			trackedPosition,
			trackedAim,
			trackedUp) ||
			!FinitePassengerAim(trackedAim))
		{
			return;
		}
		trackedAim.normalize();

		Vector3 commandBaseAim = localUnit->aim;
		if (!FinitePassengerAim(commandBaseAim))
		{
			commandBaseAim =
				Helpers::GetCamera().lookDir;
		}
		if (!FinitePassengerAim(commandBaseAim))
		{
			return;
		}
		commandBaseAim.normalize();

		const float trackedYaw =
			std::atan2(trackedAim.y, trackedAim.x);
		const float baseYaw =
			std::atan2(commandBaseAim.y, commandBaseAim.x);
		const float trackedPitch =
			std::atan2(
				trackedAim.z,
				std::sqrt(
					trackedAim.x * trackedAim.x +
					trackedAim.y * trackedAim.y));
		const float basePitch =
			std::atan2(
				commandBaseAim.z,
				std::sqrt(
					commandBaseAim.x * commandBaseAim.x +
					commandBaseAim.y * commandBaseAim.y));

		const float networkYaw =
			NormalizePassengerRadians(trackedYaw - baseYaw);
		const float networkPitch =
			NormalizePassengerRadians(trackedPitch - basePitch);

		ArmedPassengerOnFootTransportState& state =
			g_armedPassengerOnFootTransport;
		state.active = true;
		state.localBipedID = localBipedID;
		state.localPlayerID = localUnit->playerID;
		state.vehicleID = vehicleID;
		state.weaponID = weaponID;
		state.seatIndex = localUnit->parentSeatIndex;
		state.trackedAim = trackedAim;
		state.savedCamera = Helpers::GetCamera();
		state.savedLastLookDir =
			Game::instance.LastLookDir;

		// Temporarily use the on-foot aim path so UpdatePitchYaw can submit the
		// local controller direction without the passenger-seat camera path.
		localUnit->parent = HaloID{ 0xFFFF, 0xFFFF };
		localUnit->parentSeatIndex = 0xFFFFFFFF;

		// Submit the controller delta through Halo's normal multiplayer aim update.
		yaw = networkYaw;
		pitch = networkPitch;
	}

	void FinishArmedPassengerOnFootTransport()
	{
		if (!g_armedPassengerOnFootTransport.active)
		{
			return;
		}

		const ArmedPassengerOnFootTransportState state =
			g_armedPassengerOnFootTransport;
		g_armedPassengerOnFootTransport.active = false;

		BaseDynamicObject* object =
			Helpers::GetDynamicObject(
				const_cast<HaloID&>(state.localBipedID));
		if (!object ||
			object->N0000027E != ObjectType::BIPED)
		{
			Helpers::GetCamera() = state.savedCamera;
			Game::instance.LastLookDir =
				state.savedLastLookDir;
			return;
		}

		UnitDynamicObject* currentUnit =
			reinterpret_cast<UnitDynamicObject*>(object);
		// Restore the passenger relationship immediately after UpdatePitchYaw.
		currentUnit->parent = state.vehicleID;
		currentUnit->parentSeatIndex =
			state.seatIndex;

		// Restore the saved passenger camera after the aim command is updated.
		Helpers::GetCamera() = state.savedCamera;
		Game::instance.LastLookDir =
			state.savedLastLookDir;

		const bool bIdentityStillExact =
			SamePassengerHaloID(
				currentUnit->playerID,
				state.localPlayerID) &&
			SamePassengerHaloID(
				currentUnit->weapon,
				state.weaponID);

		if (bIdentityStillExact)
		{
			// Keep the local aim fields aligned with the submitted controller direction.
			currentUnit->desiredAim = state.trackedAim;
			currentUnit->aim = state.trackedAim;
			currentUnit->aim2 = state.trackedAim;
			currentUnit->aim3 = state.trackedAim;
			currentUnit->aimVelocity =
				Vector3(0.0f, 0.0f, 0.0f);
		}
	}
}

void Hooks::InitHooks()
{
	VR_PROFILE_SCOPE(Hooks_InitHooks);

	RESOLVEINDIRECT(GameVersion);
	RESOLVEINDIRECT(GameType);
	RESOLVEINDIRECT(AssetsArray);
	RESOLVEINDIRECT(Controls);
	RESOLVEINDIRECT(DirectX9);
	RESOLVEINDIRECT(DirectX9Device);
	RESOLVEINDIRECT(HideMouse);
	RESOLVEINDIRECT(ObjectTable);
	RESOLVEINDIRECT(PlayerTable);
	RESOLVEINDIRECT(LocalPlayer);
	RESOLVEINDIRECT(WindowRect);
	RESOLVEINDIRECT(RenderTargets);
	RESOLVEINDIRECT(CameraMatrices);
	RESOLVEINDIRECT(InputData);
	RESOLVEINDIRECT(CurrentRect);
	RESOLVEINDIRECT(LoadingState);
	RESOLVEINDIRECT(CmdLineArgs);
	RESOLVEINDIRECT(IsWindowed);
	RESOLVEINDIRECT(CutsceneData);
	RESOLVEINDIRECT(CampaignLoading);

	const char* gameType = Helpers::GetGameTypeString();

	CREATEHOOK(InitDirectX);
	CREATEHOOK(DrawFrame);
	CREATEHOOK(DrawHUD);
	CREATEHOOK(DrawMenu);
	CREATEHOOK(DrawLoadingScreen);
	CREATEHOOK(DrawCrosshair);

	// DrawCrosshair is now signature-resolved for whichever executable is
	// running, allowing safe Custom Edition or retail indicator resolution.
	CreateWorldSpaceIndicatorHooks(gameType);

	CREATEHOOK(DrawImage);
	CREATEHOOK(SetViewModelPosition);
	CREATEHOOK(HandleInputs);
	CREATEHOOK(UpdatePitchYaw);
	CREATEHOOK(SetViewportScale);
	CREATEHOOK(SetMousePosition);
	CREATEHOOK(UpdateMouseInfo);
	CREATEHOOK(FireWeapon);
	CREATEHOOK(ThrowGrenade);
	CREATEHOOK(DrawLoadingScreen2);
	CREATEHOOK(DrawCinematicBars);
	CREATEHOOK(DrawViewModel);
	CREATEHOOK(ReloadStart);
	CREATEHOOK(ReloadEnd);

	// These are handled with a direct patch, so manually scan them
	SigScanner::UpdateOffset(o.CutsceneFPSCap);
	SigScanner::UpdateOffset(o.CreateMouseDevice);
	SigScanner::UpdateOffset(o.SetViewModelVisible);
	SigScanner::UpdateOffset(o.TextureAlphaWrite);
	SigScanner::UpdateOffset(o.TextAlphaWrite);
	SigScanner::UpdateOffset(o.CrouchHeight);
	SigScanner::UpdateOffset(o.CinematicBarDrawCall);

	// There's almost certainly a better way to detect chimera than this
	Game::instance.bDetectedChimera |= SigScanner::UpdateOffset(o.TabOutVideo, false) < 0;
	Game::instance.bDetectedChimera |= SigScanner::UpdateOffset(o.TabOutVideo2, false) < 0;
	Game::instance.bDetectedChimera |= SigScanner::UpdateOffset(o.TabOutVideo3, false) < 0;

	if (Game::instance.bDetectedChimera)
	{
		Logger::log << "[Hook] Detected known chimera patches, disabling conflicting patches and enabling chimera compatibility" << std::endl;
	}

	SigScanner::UpdateOffset(o.SetCameraMatrices);
	oSetCameraMatrices = reinterpret_cast<Func_SetCameraMatrices>(o.SetCameraMatrices.Address);
}

#undef CREATEHOOK
#undef RESOLVEINDIRECT

void Hooks::EnableAllHooks()
{
	VR_PROFILE_SCOPE(Hooks_EnableAllHooks);

	InitDirectX.EnableHook();
	DrawFrame.EnableHook();
	DrawHUD.EnableHook();
	if (g_bHudRenderPlayersHookCreated)
	{
		g_hudRenderPlayersHook.EnableHook();
	}
	if (g_bHudRenderNavPointsHookCreated)
	{
		g_hudRenderNavPointsHook.EnableHook();
	}
	DrawMenu.EnableHook();
	DrawLoadingScreen.EnableHook();
	DrawCrosshair.EnableHook();
	//DrawImage.EnableHook();
	SetViewModelPosition.EnableHook();
	HandleInputs.EnableHook();
	UpdatePitchYaw.EnableHook();
	SetViewportScale.EnableHook();
	SetMousePosition.EnableHook();
	UpdateMouseInfo.EnableHook();
	FireWeapon.EnableHook();
	ThrowGrenade.EnableHook();
	DrawLoadingScreen2.EnableHook();
	DrawCinematicBars.EnableHook();
	DrawViewModel.EnableHook();
	ReloadStart.EnableHook();
	ReloadEnd.EnableHook();

	Freeze();

	P_RemoveCutsceneFPSCap();
	P_KeepViewModelVisible(false);
	P_EnableUIAlphaWrite();
	P_DisableCrouchCamera();
	P_ForceCmdLineArgs();
	P_RemoveCinematicBars();

	P_DontStealMouse();

	// If we think the user has chimera installed, don't try to patch their patches
	if (!Game::instance.bDetectedChimera)
	{
		P_FixTabOut();
	}

	Unfreeze();
}

//===============================//Helpers//===================================//

void Hooks::SetByte(long long Address, byte Byte)
{
	byte bytes[1]{ Byte };
	SetBytes(Address, 1, bytes);
}

void Hooks::SetBytes(long long Address, int Length, byte* Bytes)
{
	LPVOID Addr = reinterpret_cast<LPVOID>(Address);
	DWORD OldProt;
	VirtualProtect(Addr, Length, PAGE_EXECUTE_READWRITE, &OldProt);
	for (int i = 0; i < Length; i++)
	{
		*reinterpret_cast<char*>(Address + i) = Bytes[i];
	}
	if (OldProt != PAGE_EXECUTE_READWRITE)
	{
		DWORD NewProt;
		VirtualProtect(Addr, Length, OldProt, &NewProt);
	}
}

void Hooks::NOPInstructions(long long Address, int Length)
{
	LPVOID Addr = reinterpret_cast<LPVOID>(Address);
	DWORD OldProt;
	VirtualProtect(Addr, Length, PAGE_EXECUTE_READWRITE, &OldProt);
	for (int i = 0; i < Length; i++)
	{
		*reinterpret_cast<byte*>(Address + i) = 0x90;
	}
	if (OldProt != PAGE_EXECUTE_READWRITE)
	{
		DWORD NewProt;
		VirtualProtect(Addr, Length, OldProt, &NewProt);
	}
}

// Below is borrowed from minhook to freeze threads while applying patches
#define THREAD_ACCESS \
    (THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SET_CONTEXT)
#define INITIAL_THREAD_CAPACITY 128

HANDLE g_hHeap = NULL;

typedef struct _FROZEN_THREADS
{
	LPDWORD pItems;         // Data heap
	UINT    capacity;       // Size of allocated data heap, items
	UINT    size;           // Actual number of data items
} FROZEN_THREADS, * PFROZEN_THREADS;

static BOOL EnumerateThreads(PFROZEN_THREADS pThreads)
{
	LPDWORD p;
	BOOL succeeded = FALSE;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 te;
		te.dwSize = sizeof(THREADENTRY32);
		if (Thread32First(hSnapshot, &te))
		{
			succeeded = TRUE;
			do
			{
				if (te.dwSize >= (FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(DWORD))
					&& te.th32OwnerProcessID == GetCurrentProcessId()
					&& te.th32ThreadID != GetCurrentThreadId())
				{
					if (pThreads->pItems == NULL)
					{
						pThreads->capacity = INITIAL_THREAD_CAPACITY;
						pThreads->pItems
							= (LPDWORD)HeapAlloc(g_hHeap, 0, pThreads->capacity * sizeof(DWORD));
						if (pThreads->pItems == NULL)
						{
							succeeded = FALSE;
							break;
						}
					}
					else if (pThreads->size >= pThreads->capacity)
					{
						pThreads->capacity *= 2;
						p = (LPDWORD)HeapReAlloc(
							g_hHeap, 0, pThreads->pItems, pThreads->capacity * sizeof(DWORD));
						if (p == NULL)
						{
							succeeded = FALSE;
							break;
						}

						pThreads->pItems = p;
					}
					pThreads->pItems[pThreads->size++] = te.th32ThreadID;
				}

				te.dwSize = sizeof(THREADENTRY32);
			} while (Thread32Next(hSnapshot, &te));

			if (succeeded && GetLastError() != ERROR_NO_MORE_FILES)
				succeeded = FALSE;

			if (!succeeded && pThreads->pItems != NULL)
			{
				HeapFree(g_hHeap, 0, pThreads->pItems);
				pThreads->pItems = NULL;
			}
		}
		CloseHandle(hSnapshot);
	}

	return succeeded;
}

//-------------------------------------------------------------------------
FROZEN_THREADS Threads;

bool Hooks::Freeze()
{
	Threads.pItems = NULL;
	Threads.capacity = 0;
	Threads.size = 0;
	g_hHeap = HeapCreate(0, 0, 0);
	if (!EnumerateThreads(&Threads))
	{
		return false;
	}

	if (Threads.pItems != NULL)
	{
		UINT i;
		for (i = 0; i < Threads.size; ++i)
		{
			HANDLE hThread = OpenThread(THREAD_ACCESS, FALSE, Threads.pItems[i]);
			if (hThread != NULL)
			{
				SuspendThread(hThread);
				CloseHandle(hThread);
			}
		}
	}

	return true;
}

//-------------------------------------------------------------------------
void Hooks::Unfreeze()
{
	if (Threads.pItems != NULL)
	{
		UINT i;
		for (i = 0; i < Threads.size; ++i)
		{
			HANDLE hThread = OpenThread(THREAD_ACCESS, FALSE, Threads.pItems[i]);
			if (hThread != NULL)
			{
				ResumeThread(hThread);
				CloseHandle(hThread);
			}
		}

		HeapFree(g_hHeap, 0, Threads.pItems);
	}
	HeapDestroy(g_hHeap);
}

//-------------------------------------------------------------------------
void Hooks::ResolveIndirect(Offset& offset, long long& Address)
{
	SigScanner::UpdateOffset(offset);
	void* pointer = *reinterpret_cast<void**>(offset.Address + Address);
	Address = reinterpret_cast<long long>(pointer);
	Logger::log << "[Hook] Calculated indirect address: 0x" << std::hex << Address << std::dec << std::endl;
}

//===============================//Hooks//===================================//

bool Hooks::H_InitDirectX()
{
	VR_PROFILE_SCOPE(Hooks_InitDirectX);

	bool bSuccess = InitDirectX.Original();

	Game::instance.OnInitDirectX();

	return bSuccess;
}

void Hooks::H_DrawFrame(Renderer* param1, short param2, short* param3, float tickProgress, float deltaTime)
{
	VR_PROFILE_SCOPE(Hooks_DrawFrame);
	/*
	In order to get each perspective (left eye, right eye, PiP scope, mirror [todo: just blit an eye for the mirror])
	we need to call the draw function multiple times, but should take care to avoid creating multiple d3d scenes as
	according to the docs you should only call this once per frame.
	The shim we use to upgrade D3D9 to D3D9Ex has some functionality added to skip these calls when necessary
	*/

	bool bDrawMirror = Game::instance.GetDrawMirror();

	Camera trueCamera;

	// Chimera does its own thing with camera interpolation, steal that for the camera position during rendering
	if (Game::instance.bDetectedChimera)
	{
		trueCamera = Helpers::GetCamera();
		Helpers::GetCamera().position = param1->frustum.position;
		Helpers::GetCamera().lookDir = param1->frustum.facingDirection;
		Helpers::GetCamera().lookDirUp = param1->frustum.upDirection;
		Game::instance.LastLookDir = param1->frustum.facingDirection;
	}


	Game::instance.SetRenderTickProgress(tickProgress);
	Game::instance.PreDrawFrame(param1, deltaTime);
	reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextEnd = true;

	// Draw scope if necessary
	if (Game::instance.PreDrawScope(param1, deltaTime))
	{
		VR_PROFILE_START(Hooks_DrawFrame_DrawScope);
		DrawFrame.Original(param1, param2, param3, tickProgress, deltaTime);
		VR_PROFILE_STOP(Hooks_DrawFrame_DrawScope);
		Game::instance.PostDrawScope(param1, deltaTime);

		reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextStart = true;
		reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextEnd = true;
	}

	// Draw left eye
	Game::instance.PreDrawEye(param1, deltaTime, 0);
	VR_PROFILE_START(Hooks_DrawFrame_DrawLeftEye);
	DrawFrame.Original(param1, param2, param3, tickProgress, deltaTime);
	VR_PROFILE_STOP(Hooks_DrawFrame_DrawLeftEye);
	Game::instance.PostDrawEye(param1, deltaTime, 0);

	reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextStart = true;
	reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextEnd = bDrawMirror;

	// Draw right eye
	Game::instance.PreDrawEye(param1, deltaTime, 1);
	VR_PROFILE_START(Hooks_DrawFrame_DrawRightEye);
	DrawFrame.Original(param1, param2, param3, tickProgress, deltaTime);
	VR_PROFILE_STOP(Hooks_DrawFrame_DrawRightEye);
	Game::instance.PostDrawEye(param1, deltaTime, 1);

	// Draw Mirror view, but only if the secret mirror mode is set in settings due to performance cost
	if (bDrawMirror)
	{
		reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bIgnoreNextStart = true;

		Game::instance.PreDrawMirror(param1, deltaTime);
		VR_PROFILE_START(Hooks_DrawFrame_DrawMirror);
		DrawFrame.Original(param1, param2, param3, tickProgress, deltaTime);
		VR_PROFILE_STOP(Hooks_DrawFrame_DrawMirror);
		Game::instance.PostDrawMirror(param1, deltaTime);
	}
	Game::instance.PostDrawFrame(param1, deltaTime);

	if (Game::instance.bDetectedChimera)
	{
		Helpers::GetCamera() = trueCamera;
	}
}

void Hooks::H_DrawHUD()
{
	VR_PROFILE_SCOPE(Hooks_DrawHUD);
	if (Game::instance.PreDrawHUD())
	{
		DrawHUD.Original();
		Game::instance.PostDrawHUD();
	}
}

void __declspec(naked) Hooks::H_DrawMenu()
{
	// Original function uses AX register as param_1
	_asm
	{
		pushad
	}

	{
		VR_PROFILE_START(Hooks_DrawMenu);

		if (Game::instance.PreDrawMenu())
		{
			DrawMenu.Original();
			Game::instance.PostDrawMenu();
		}

		VR_PROFILE_STOP(Hooks_DrawMenu);
	}

	_asm
	{
		popad
		ret
	}
}

void __declspec(naked) Hooks::H_DrawLoadingScreen()
{
	// Function has two params
	// int param1 : EAX
	// Renderer* renderer : Stack[0x4]

	static int param1;
	static Renderer* param2;

	_asm
	{
		// Grab param1
		mov param1, eax
		// Grab param2
		mov eax, [esp + 0x4]
		mov param2, eax

		pushad
	}

	if (Game::instance.PreDrawLoading(param1, param2))
	{

		_asm
		{
			popad

			// Re-push parameters
			push eax
			mov eax, param1
		}

		DrawLoadingScreen.Original();

		_asm
		{
			pushad
		}

		Game::instance.PostDrawLoading(param1, param2);

		_asm
		{
			popad

			add esp, 0x4
			ret
		}
	}
	else
	{
		_asm
		{
			popad

			ret
		}
	}
}

void __declspec(naked) Hooks::H_DrawCrosshair()
{
	static void* retAddress;
	static float* param1; // We don't actually need this param, but we steal its register for moving some memory around
	static short* param6;

	_asm
	{
		mov param1, eax
		mov eax, [esp + 0xc]
		mov param6, eax
		mov eax, param1
		pop retAddress
		pushad
	}

	if (Game::instance.PreDrawCrosshair(param6))
	{

		_asm
		{
			popad
		}

		DrawCrosshair.Original();

		_asm
		{
			pushad
		}

		Game::instance.PostDrawCrosshair();

	}
	_asm
	{
		popad
		push retAddress
		ret
	}
}

void __declspec(naked) Hooks::H_DrawImage()
{
	static void* param1;
	static void* param2;

	_asm
	{
		mov param1, eax
		mov eax, [esp + 0x4]
		mov param2, eax
		pushad
	}

	Game::instance.PreDrawImage(param1, param2);

	_asm
	{
		popad
		mov eax, param2
		push eax
		mov eax, param1
	}

	DrawImage.Original();

	_asm
	{
		pushad
	}

	Game::instance.PostDrawImage(param1, param2);

	_asm
	{
		popad
		add esp, 0x4
		ret
	}
}

void __declspec(naked) Hooks::H_SetViewModelPosition()
{
	static Vector3* pos;
	static Vector3* facing;
	static Vector3* up;
	static HaloID id;
	static TransformQuat* quatTrans;
	static Transform* trans;

	_asm
	{
		mov id, eax // Get ID from EAX param
		mov pos, ecx // Get Position from ECX param
		mov ecx, [esp + 0x10] // Get UpVector from 0x10 param
		mov up, ecx
		mov ecx, [esp + 0xc] // Get FacingVector from 0xc param
		mov facing, ecx
		mov ecx, [esp + 0x8] // Get QuatTransforms from 0x8 param
		mov quatTrans, ecx
		mov ecx, [esp + 0x4] // Get Transforms from 0x4 param
		mov trans, ecx
		mov ecx, pos // Restore Position in ECX register
	}

	// The original function was simple enough + required enough patches for VR
	// that it is easier to just fully reimplement it in the mod
	Game::instance.UpdateViewModel(id, pos, facing, up, quatTrans, trans);

	_asm
	{
		ret
	}
}

void Hooks::H_HandleInputs()
{
	HandleInputs.Original();

	Game::instance.UpdateInputs();
	Game::instance.CalculateSmoothedInput();
}

void __declspec(naked) Hooks::H_UpdatePitchYaw()
{
	static short playerId;
	static float yaw;
	static float pitch;
	static float hiddenModelYaw;
	static float hiddenModelPitch;
	static bool bRunHiddenVehicleModelPass;

	// Extract parameters.
	_asm
	{
		mov playerId, ax
		mov eax, [esp + 0x4]
		mov yaw, eax
		mov eax, [esp + 0x8]
		mov pitch, eax
		movsx eax, playerId
	}

	/*
	Original stack:
	0x0: ret address
	0x4: yaw
	0x8: pitch

	Post-hook stack:
	0x0: hook ret address
	0x4: ret address
	0x8: yaw
	0xc: pitch

	Fixed stack:
	0x0: hook ret address
	0x4: yaw
	0x8: pitch
	0xc: ret address
	0x10: yaw
	0x14: pitch
	*/

	_asm
	{
		pushad
	}

	// This pass produces Halo's native right-stick camera input.
	Game::instance.UpdateCamera(
		yaw,
		pitch);
	Game::instance.UpdateMPRoomScaleBodyOnNetworkTick();

	PrepareArmedPassengerOnFootTransport(
		yaw,
		pitch);

	_asm
	{
		popad
	}

	// First pass: always allow Halo to complete its proper native camera update.
	_asm
	{
		push pitch
		push yaw
	}

	UpdatePitchYaw.Original();

	_asm
	{
		pushad
	}

	FinishArmedPassengerOnFootTransport();

	// Prepare the model-aim command only after Halo's native camera/right-stick
	// pass has finished.
	bRunHiddenVehicleModelPass =
		PrepareVehicleModelAimCommand(
			hiddenModelYaw,
			hiddenModelPitch);

	if (!bRunHiddenVehicleModelPass)
	{
		ApplyVehicleMotionAim();
	}

	_asm
	{
		popad
	}

	if (bRunHiddenVehicleModelPass)
	{
		// Second pass: rotate Halo's authoritative turret/vehicle aim using the
		// hand command while a copy of the already-correct native camera is held.
		_asm
		{
			movsx eax, playerId
			push hiddenModelPitch
			push hiddenModelYaw
		}

		UpdatePitchYaw.Original();

		_asm
		{
			add esp, 0x8
			pushad
		}

		FinishVehicleModelAimCommand();

		// Preserve the exact tracked direction used by the reticle/projectile
		// after Halo has updated the physical turret/vehicle model.
		ApplyVehicleMotionAim();

		_asm
		{
			popad
		}
	}

	// Remove the first pass's re-pushed arguments and return.
	_asm
	{
		add esp, 0x8
		ret
	}
}


void __declspec(naked) Hooks::H_SetViewportScale()
{
	static Viewport* view;

	_asm
	{
		mov view, ecx
	}

	SetViewportScale.Original();

	_asm
	{
		pushad
	}

	Game::instance.SetViewportScale(view);

	_asm
	{
		popad
	}

	_asm
	{
		ret
	}
}

void __declspec(naked) Hooks::H_SetMousePosition()
{
	static int x;
	static int y;

	_asm
	{
		mov x, eax
		mov y, ecx
	}

	_asm
	{
		pushad
	}

	Game::instance.SetMousePosition(x, y);

	_asm
	{
		popad
	}

	_asm
	{
		mov eax, x
		mov ecx, y
	}

	SetMousePosition.Original();

	_asm
	{
		ret
	}
}

void __declspec(naked) Hooks::H_UpdateMouseInfo()
{
	static MouseInfo* mouseInfo;

	_asm
	{
		push eax
		mov eax, [esp + 0x8]
		mov mouseInfo, eax
		pop eax
	}

	_asm
	{
		push mouseInfo
	}

	UpdateMouseInfo.Original();

	_asm
	{
		pushad
	}

	Game::instance.UpdateMouseInfo(mouseInfo);

	_asm
	{
		popad
	}

	_asm
	{
		add esp, 0x4
		ret
	}
}

void Hooks::H_FireWeapon(HaloID param1, short param2)
{
	VR_PROFILE_SCOPE(Hooks_FireWeapon);

	Game::instance.PreFireWeapon(param1, param2);

	FireWeapon.Original(param1, param2);

	Game::instance.PostFireWeapon(param1, param2);

}


void Hooks::H_ThrowGrenade(HaloID param1, bool param2)
{
	VR_PROFILE_SCOPE(Hooks_ThrowGrenade);

	Game::instance.PreThrowGrenade(param1);

	ThrowGrenade.Original(param1, param2);

	Game::instance.PostThrowGrenade(param1);
}

void Hooks::H_DrawLoadingScreen2(void* param1)
{
	D3DVIEWPORT9 currentViewport;
	D3DVIEWPORT9 desiredViewport
	{
		0,
		0,
		static_cast<DWORD>(Game::instance.overlayWidth),
		static_cast<DWORD>(Game::instance.overlayHeight),
		0.0f,
		1.0f
	};

	Helpers::GetDirect3DDevice9()->GetViewport(&currentViewport);
	Helpers::GetDirect3DDevice9()->SetViewport(&desiredViewport);

	DrawLoadingScreen2.Original(param1);

	Helpers::GetDirect3DDevice9()->SetViewport(&currentViewport);
}

void Hooks::H_DrawCinematicBars()
{
	D3DVIEWPORT9 currentViewport;
	D3DVIEWPORT9 desiredViewport
	{
		0,
		0,
		static_cast<DWORD>(Game::instance.overlayWidth),
		static_cast<DWORD>(Game::instance.overlayHeight),
		0.0f,
		1.0f
	};

	Helpers::GetDirect3DDevice9()->GetViewport(&currentViewport);
	Helpers::GetDirect3DDevice9()->SetViewport(&desiredViewport);

	DrawCinematicBars.Original();

	Helpers::GetDirect3DDevice9()->SetViewport(&currentViewport);
}

void Hooks::H_DrawViewModel()
{
	VR_PROFILE_SCOPE(Hooks_DrawViewModel);

	// Skip weapon rendering during cutscenes
	CutsceneData* cutscene = Helpers::GetCutsceneData();
	if (cutscene && cutscene->bInCutscene)
	{
		return;
	}

	// Skip weapon rendering during scope render when using original scope
	// This fixes an issue where the weapon model was showing up in the scope
	if (Game::instance.GetRenderState() == ERenderState::SCOPE && Game::instance.ShouldUseOriginalScope())
	{
		return;
	}

	if (Game::instance.bLeftHanded)
	{
		Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

		reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bSkipWinding = true;

		DrawViewModel.Original();

		reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9())->bSkipWinding = false;

		Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	}
	else
	{
		DrawViewModel.Original();
	}
}

void Hooks::H_ReloadStart(HaloID param1, short param2, bool param3)
{
	VR_PROFILE_SCOPE(Hooks_ReloadStart);

	ReloadStart.Original(param1, param2, param3);

	Game::instance.ReloadStart(param1, param2, param3);
}

void __declspec(naked) Hooks::H_ReloadEnd()
{
	static short param1;
	static HaloID param2;

	_asm
	{
		mov param1, cx
		push eax
		mov eax, [esp + 0x8]
		mov param2, eax
		pop eax
		push param2
	}

	ReloadEnd.Original();

	Game::instance.ReloadEnd(param1, param2);

	_asm
	{
		add esp, 0x4
		ret;
	}
}

//================================//Patches//================================//

void Hooks::P_FixTabOut()
{
	// There are 3 checks in the main loop for focus
	// Attempting to patch this out with minhook would be painful

	// Always jump past the first check
	SetByte(o.TabOutVideo.Address + 6, 0xEB);
	// NOP a JNZ in the second check
	NOPInstructions(o.TabOutVideo2.Address, 2);
	// Always jump past the final check
	SetByte(o.TabOutVideo3.Address + 6, 0xEB);

	// TODO: There's a more complex patch I think to do with fullscreen
}

void Hooks::P_RemoveCutsceneFPSCap()
{
	// Instead of setting the flag for capping the fps when we are in a cutscene, just clear it
	byte bytes[2]{ 0x32, 0xdb };
	SetBytes(o.CutsceneFPSCap.Address, 2, bytes);
}

void Hooks::P_KeepViewModelVisible(bool bAlwaysShow)
{
	VR_PROFILE_SCOPE(Hooks_P_KeepViewModelVisible); // Wouldn't normally profile patches, but this one gets dynamically applied at run time
	// Replace "bShowViewModel = false" with "bShowViewModel = true"
	SetByte(o.SetViewModelVisible.Address + 0x55, bAlwaysShow ? 0x0 : 0x1);
}

void Hooks::P_EnableUIAlphaWrite()
{
	// By default the UI doesn't write to the alpha channel (since its being drawn last + straight onto the main image)
	// We need the alpha value in order to have transparency in floating UI elements
	SetByte(o.TextureAlphaWrite.Address + 0x19, 0xf);
	SetByte(o.TextAlphaWrite.Address + 0x72, 0xf);
}

void Hooks::P_DisableCrouchCamera()
{
	// When not playing in seated mode (i.e. motion controlled crouching is disabled) patch out the behaviour that
	// lowers the camera position when crouching. In theory this could be worked around in the mod's game code,
	// but the game ticks at a fixed rate and interpolates the camera, which is harder to account for than just disabling crouch
	if (Game::instance.c_CrouchHeight->Value() >= 0.0f)
	{
		// Replace "crouchAlpha = player->crouchProgress" with "crouchAlpha = 0.0"
		// Uses a "0.0" constant from later in the function
		byte bytes[6] = { 0xd9, 0x05, 0x0, 0x0, 0x0, 0x0 };

		*(uint32_t*)(&bytes[2]) = *reinterpret_cast<uint32_t*>(o.CrouchHeight.Address + 0x11);

		SetBytes(o.CrouchHeight.Address, 6, bytes);
	}
}

void Hooks::P_ForceCmdLineArgs()
{
	// While the game probably works fine in fullscreen or with bink videos, its easier to just force some known values
	// and not have to debug issues that only occur on say a 4k fullscreen monitor during the splash screen

	CmdLineArgs& args = Helpers::GetCmdLineArgs();
	args.NoVideo = 1;
	args.Width640 = 1;
	*reinterpret_cast<int*>(o.IsWindowed) = 1; // This isn't stored with the rest of the args for some reason
}

void Hooks::P_RemoveCinematicBars()
{
	// I would just disable the function that draws these entirely, but then we lose the level titles
	long long FirstAddress = o.CinematicBarDrawCall.Address;


	NOPInstructions(FirstAddress, 5);
	NOPInstructions(FirstAddress + 0x74, 5);
}

void Hooks::P_DontStealMouse()
{
	// Halo claims exclusive rights over the mouse which causes all sorts of chaos when trying to pause the game with a debugger
	// Changing it to non-exclusive is fine for our purposes (VR/testing),
	// but does cause the mouse to stop being locked to the window in game
	SetByte(o.CreateMouseDevice.Address + 0x5B, 6);
}

void Hooks::SetCameraMatrices(struct Viewport* viewport, struct CameraFrustum* frustum, struct CameraRenderMatrices* crm, bool bDoProjection)
{
	// Halo loves its non-standard calling conventions...
	_asm
	{
		push eax
		push ecx
		push esi
		movzx eax, bDoProjection
		push eax
		mov eax, viewport
		mov ecx, frustum
		mov esi, crm
		call oSetCameraMatrices
		pop esi
		pop ecx
		pop eax
		add esp, 0x4
	}
}
