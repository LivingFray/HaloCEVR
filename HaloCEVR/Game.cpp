#include "Game.h"
#include "Logger.h"
#include "Hooking/Hooks.h"
#include "Helpers/DX9.h"
#include "Helpers/RenderTarget.h"
#include "Helpers/Renderer.h"
#include "Helpers/Camera.h"
#include "Helpers/Controls.h"
#include "Helpers/Menus.h"
#include "Helpers/Objects.h"
#include "Helpers/Assets.h"
#include "Helpers/Maths.h"

#include <cstring>
#include <chrono>

#ifdef EMULATE_VR
#include "VR/VREmulator.h"
#else
#include "VR/OpenVR.h"
#endif

#if USE_PROFILER
#include <algorithm>
#endif

#include "UI/UIRenderer.h"
#include "Helpers/Version.h"
#include "Helpers/Cutscene.h"

namespace
{
	bool IsMPPlayerTable()
	{
		PlayerTable& table = Helpers::GetPlayerTable();
		return table.currentSize > 1 || table.count > 1;
	}

	uint16_t GetNetworkServerType()
	{
		// Resolve Halo's network role directly from the retail executable.
		static Offset serverTypeOffset = {
			"RoomScaleServerType",
			0x000000,
			"0f bf 2d ?? ?? ?? ?? e8 ?? ?? ?? ?? 39 1d ?? ?? ?? ?? 75 05",
			0,
			0
		};
		static volatile const uint16_t* serverTypeAddress = nullptr;
		static bool bAttemptedResolution = false;

		if (!bAttemptedResolution)
		{
			bAttemptedResolution = true;
			if (SigScanner::UpdateOffset(serverTypeOffset, false) >= 0)
			{
				serverTypeAddress =
					*reinterpret_cast<volatile const uint16_t**>(serverTypeOffset.Address + 3);
			}
		}

		return serverTypeAddress ? *serverTypeAddress : 0xFFFF;
	}

	bool ShouldUseMPRoomScaleNetworkServo()
	{
		// Halo CE exposes the machine's actual role independently of the player
		// table. A joined client is not guaranteed to occupy any fixed table slot.
		constexpr uint16_t ServerNone = 0;
		constexpr uint16_t ServerJoinedClient = 1;
		constexpr uint16_t ServerLocalHost = 2;

		const uint16_t serverType = GetNetworkServerType();

		if (serverType == ServerJoinedClient)
		{
			return true;
		}

		if (serverType == ServerNone || serverType == ServerLocalHost)
		{
			return false;
		}

		// A future/unknown network value gets the conservative multiplayer path:
		// never perform host-authoritative position writes when a player table says
		// we are in multiplayer.
		return IsMPPlayerTable();
	}

	bool SameHaloIDLocal(const HaloID& a, const HaloID& b)
	{
		return a.index == b.index && a.id == b.id;
	}

	struct FirstPersonVehicleCameraTransformState
	{
		bool active = false;
		bool multiplayer = false;
		HaloID vehicleID{ 0xFFFF, 0xFFFF };
		uint32_t cameraContext = 0;
		float contextSeconds = 0.0f;
		bool capturedSeatBodyLocalValid = false;
		Vector3 capturedSeatBodyLocal{};
		Vector3 previousPosition{};
		Vector3 currentPosition{};
		Vector3 previousForward{};
		Vector3 currentForward{};
		Vector3 previousUp{};
		Vector3 currentUp{};
		float elapsedSeconds = 0.0f;

		// Multiplayer vehicle snapshots can arrive one render frame before Halo
		// resets its 30 Hz interpolation phase. Defer those poses until the reset
		// to avoid a mid-phase camera correction.
		Vector3 observedPosition{};
		Vector3 observedForward{};
		Vector3 observedUp{};
		bool deferredPoseValid = false;
		bool previousRenderPhaseValid = false;
		float previousRenderPhaseSeconds = 0.0f;
	};

	FirstPersonVehicleCameraTransformState firstPersonVehicleCameraTransformState{};

	void ResetFirstPersonVehicleCameraTransformState()
	{
		firstPersonVehicleCameraTransformState =
			FirstPersonVehicleCameraTransformState{};
	}

	bool IsFiniteVectorLocal(const Vector3& value)
	{
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	struct VehicleTagPathViewLocal
	{
		char pad_0000[16];
		const char* path;
	};

	bool IsCovenantGroundTurretTagPathLocal(
		const char* tagPath)
	{
		return tagPath &&
			(std::strstr(tagPath, "shade") != nullptr ||
				std::strstr(
					tagPath,
					"c gun turret") != nullptr ||
				std::strstr(
					tagPath,
					"c_gun_turret") != nullptr ||
				std::strstr(
					tagPath,
					"c-gun-turret") != nullptr ||
				std::strstr(
					tagPath,
					"covenant_turret") != nullptr ||
				std::strstr(
					tagPath,
					"covenant turret") != nullptr ||
				std::strstr(
					tagPath,
					"plasma_turret") != nullptr);
	}

	Vector3 LerpVectorLocal(
		const Vector3& from,
		const Vector3& to,
		float amount)
	{
		return from + (to - from) * amount;
	}

	// Captured joined-client exits from the dedicated vehicle camera remained in
	// Halo's third-person-to-first-person blend for about 0.83 seconds.
	constexpr float VehicleExitCameraGuardDurationSeconds = 0.90f;

	// A joined client's body position advances on Halo's simulation/network cadence,
	// not necessarily on the frame that authored the Controls values. Retain a short
	// causal history only for roomscale input that survived strict manual-axis
	// ownership, and discard it when the stick direction changes.
	constexpr unsigned int MPRoomScaleCommandHistoryCapacity = 32;
	struct MPRoomScaleCommandSample
	{
		HaloID targetID{ 0xFFFF, 0xFFFF };
		Vector3 manualIntentDirection{};
		Vector3 manualCommand{};
		Vector3 roomCommand{};
		Vector3 combinedCommand{};
		unsigned int sequence = 0;
		unsigned int commandClock = 0;
		bool valid = false;
	};

	MPRoomScaleCommandSample mpRoomScaleCommandHistory[
		MPRoomScaleCommandHistoryCapacity]{};
	unsigned int mpRoomScaleCommandHistoryWrite = 0;
	unsigned int mpRoomScaleCommandHistoryCount = 0;
	unsigned int mpRoomScaleCommandSequence = 0;
	unsigned int mpRoomScaleCommandClock = 0;

	void AdvanceMPRoomScaleCommandClock()
	{
		mpRoomScaleCommandClock++;
	}

	void ResetMPRoomScaleCommandHistory()
	{
		for (unsigned int i = 0; i < MPRoomScaleCommandHistoryCapacity; i++)
		{
			mpRoomScaleCommandHistory[i] = MPRoomScaleCommandSample{};
		}
		mpRoomScaleCommandHistoryWrite = 0;
		mpRoomScaleCommandHistoryCount = 0;
	}

	void RecordMPRoomScaleCommand(const HaloID& targetID,
		Vector3 manualIntentDirection, Vector3 manualCommand,
		Vector3 roomCommand, Vector3 combinedCommand)
	{
		manualIntentDirection.z = 0.0f;
		manualCommand.z = 0.0f;
		roomCommand.z = 0.0f;
		combinedCommand.z = 0.0f;
		if (manualIntentDirection.lengthSqr() < 0.000001f ||
			manualCommand.lengthSqr() < 0.000001f ||
			roomCommand.lengthSqr() < 0.000001f ||
			combinedCommand.lengthSqr() < 0.000001f)
		{
			return;
		}

		manualIntentDirection.normalize();

		MPRoomScaleCommandSample& sample =
			mpRoomScaleCommandHistory[mpRoomScaleCommandHistoryWrite];
		sample.targetID = targetID;
		sample.manualIntentDirection = manualIntentDirection;
		sample.manualCommand = manualCommand;
		sample.roomCommand = roomCommand;
		sample.combinedCommand = combinedCommand;
		sample.sequence = ++mpRoomScaleCommandSequence;
		sample.commandClock = mpRoomScaleCommandClock;
		sample.valid = true;

		mpRoomScaleCommandHistoryWrite =
			(mpRoomScaleCommandHistoryWrite + 1) % MPRoomScaleCommandHistoryCapacity;
		if (mpRoomScaleCommandHistoryCount < MPRoomScaleCommandHistoryCapacity)
		{
			mpRoomScaleCommandHistoryCount++;
		}
	}

	bool MatchMPRoomScaleCommand(const HaloID& targetID,
		const Vector3& actualBodyDelta,
		MPRoomScaleCommandSample& outSample)
	{
		if (mpRoomScaleCommandHistoryCount == 0 ||
			actualBodyDelta.lengthSqr() < 0.000001f)
		{
			return false;
		}

		Vector3 actualDirection = actualBodyDelta;
		actualDirection.z = 0.0f;
		if (actualDirection.lengthSqr() < 0.000001f)
		{
			return false;
		}
		actualDirection.normalize();

		float bestScore = -2.0f;
		float bestAlignment = -1.0f;
		bool found = false;
		for (unsigned int historyAge = 0;
			historyAge < mpRoomScaleCommandHistoryCount; historyAge++)
		{
			const unsigned int index =
				(mpRoomScaleCommandHistoryWrite + MPRoomScaleCommandHistoryCapacity -
					1 - historyAge) % MPRoomScaleCommandHistoryCapacity;
			const MPRoomScaleCommandSample& sample =
				mpRoomScaleCommandHistory[index];
			if (!sample.valid || !SameHaloIDLocal(sample.targetID, targetID))
			{
				continue;
			}
			const unsigned int clockAge =
				mpRoomScaleCommandClock - sample.commandClock;
			// Eight input updates leave network-tick margin without allowing old
			// commands to linger through a deliberate stick-direction change.
			if (clockAge > 8)
			{
				continue;
			}

			Vector3 combinedDirection = sample.combinedCommand;
			combinedDirection.normalize();
			const float alignment =
				actualDirection.dot(combinedDirection);
			// Prefer causal proximity when two commands explain the same movement, but
			// allow enough history for Halo's lower-frequency body simulation updates.
			const float score = alignment -
				static_cast<float>(clockAge) * 0.0025f;
			if (score > bestScore)
			{
				bestScore = score;
				bestAlignment = alignment;
				outSample = sample;
				found = true;
			}
		}

		return found && bestAlignment > 0.35f;
	}

	bool MPRoomScaleManualDirectionChanged(const HaloID& targetID,
		Vector3 manualIntentDirection)
	{
		manualIntentDirection.z = 0.0f;
		if (mpRoomScaleCommandHistoryCount == 0 ||
			manualIntentDirection.lengthSqr() < 0.000001f)
		{
			return false;
		}
		manualIntentDirection.normalize();

		for (unsigned int historyAge = 0;
			historyAge < mpRoomScaleCommandHistoryCount; historyAge++)
		{
			const unsigned int index =
				(mpRoomScaleCommandHistoryWrite + MPRoomScaleCommandHistoryCapacity -
					1 - historyAge) % MPRoomScaleCommandHistoryCapacity;
			const MPRoomScaleCommandSample& sample =
				mpRoomScaleCommandHistory[index];
			if (!sample.valid || !SameHaloIDLocal(sample.targetID, targetID))
			{
				continue;
			}

			// A 25-degree or larger change is a new manual command, not a
			// continuation of the roomscale transaction recorded before it.
			return manualIntentDirection.dot(sample.manualIntentDirection) < 0.90f;
		}

		return false;
	}

	// Halo can rewrite the Scorpion occupant's reported seat value after the
	// vehicle takes damage. Cache the role selected on entry so that damage cannot
	// permanently reclassify the driver as a side passenger.
	struct ScorpionCrosshairSeatCache
	{
		HaloID localPlayerID{ 0xFFFF, 0xFFFF };
		HaloID vehicleID{ 0xFFFF, 0xFFFF };
		bool bIsDriver = false;
		bool bValid = false;

		// Vehicle damage can briefly clear or replace the biped's parent datum.
		// Do not erase a known driver/passenger role from a one-frame transient.
		unsigned int confirmedOnFootFrames = 0;
	};

	ScorpionCrosshairSeatCache scorpionCrosshairSeatCache{};

	void ResetScorpionCrosshairSeatCache()
	{
		scorpionCrosshairSeatCache = ScorpionCrosshairSeatCache{};
	}

	bool IsCachedScorpionDriverForLocalPlayer()
	{
		if (!scorpionCrosshairSeatCache.bValid ||
			!scorpionCrosshairSeatCache.bIsDriver)
		{
			return false;
		}

		HaloID localPlayerID{ 0xFFFF, 0xFFFF };
		return Helpers::GetLocalPlayerID(localPlayerID) &&
			SameHaloIDLocal(
				scorpionCrosshairSeatCache.localPlayerID,
				localPlayerID);
	}

	// Suppress the optional vehicle crosshair in explicit non-aiming passenger
	// positions while preserving the Warthog turret and Scorpion driver sights.
	bool IsLocalPlayerInCrosshairExcludedVehicleSeat(
		bool* outIsWarthog = nullptr,
		bool* outUsesNativeVehicleAim = nullptr)
	{
		if (outIsWarthog)
		{
			*outIsWarthog = false;
		}
		if (outUsesNativeVehicleAim)
		{
			*outUsesNativeVehicleAim = false;
		}

		HaloID localPlayerID{ 0xFFFF, 0xFFFF };
		if (!Helpers::GetLocalPlayerID(localPlayerID))
		{
			// A transient resolver miss must not erase a valid Scorpion role.
			return false;
		}

		BaseDynamicObject* localObject =
			Helpers::GetDynamicObject(localPlayerID);
		if (!localObject || localObject->N0000027E != ObjectType::BIPED)
		{
			return false;
		}

		UnitDynamicObject* localUnit =
			reinterpret_cast<UnitDynamicObject*>(localObject);
		if (localUnit->parent.id == 0xFFFF ||
			localUnit->parent.index == 0xFFFF)
		{
			// Damage can leave the Scorpion driver logically seated while Halo's
			// biped parent datum reports invalid. Preserve the established role
			// unless the biped has also regained a normal handheld weapon for
			// several consecutive frames, which indicates a genuine exit.
			const bool bHasHandheldWeapon =
				localUnit->weapon.id != 0xFFFF &&
				localUnit->weapon.index != 0xFFFF;

			if (scorpionCrosshairSeatCache.bValid &&
				bHasHandheldWeapon)
			{
				++scorpionCrosshairSeatCache.confirmedOnFootFrames;
				if (scorpionCrosshairSeatCache.confirmedOnFootFrames >= 4)
				{
					ResetScorpionCrosshairSeatCache();
				}
			}
			else
			{
				scorpionCrosshairSeatCache.confirmedOnFootFrames = 0;
			}

			return scorpionCrosshairSeatCache.bValid
				? !scorpionCrosshairSeatCache.bIsDriver
				: false;
		}

		// Any valid parent observation cancels the transient-exit counter.
		scorpionCrosshairSeatCache.confirmedOnFootFrames = 0;

		BaseDynamicObject* vehicle =
			Helpers::GetDynamicObject(localUnit->parent);
		if (!vehicle || vehicle->N0000027E != ObjectType::VEHICLE)
		{
			return false;
		}

		// The strongest gunner/vehicle-driver relationship is not a hardcoded
		// seat number: Halo gives an aiming occupant a weapon whose parent is the
		// same vehicle containing the biped. Handheld passenger weapons remain
		// parented to the biped and therefore do not match.
		bool bUsesVehicleOwnedWeapon = false;
		if (localUnit->weapon.id != 0xFFFF &&
			localUnit->weapon.index != 0xFFFF)
		{
			BaseDynamicObject* localWeapon =
				Helpers::GetDynamicObject(localUnit->weapon);
			bUsesVehicleOwnedWeapon =
				localWeapon &&
				SameHaloIDLocal(
					localWeapon->parent,
					localUnit->parent);
		}

		if (outUsesNativeVehicleAim)
		{
			*outUsesNativeVehicleAim =
				bUsesVehicleOwnedWeapon;
		}

		// Retail Halo packs additional state into the upper bytes. The actual
		// vehicle seat index remains in the low byte.
		const uint32_t normalizedSeatIndex =
			localUnit->parentSeatIndex & 0xFFu;

		const bool bMatchesCachedScorpion =
			scorpionCrosshairSeatCache.bValid &&
			SameHaloIDLocal(
				scorpionCrosshairSeatCache.localPlayerID,
				localPlayerID) &&
			SameHaloIDLocal(
				scorpionCrosshairSeatCache.vehicleID,
				localUnit->parent);

		// Tag-index entries share a path pointer at offset 0x10. Matching the
		// vehicle tag path covers campaign, multiplayer and common variants.
		struct TagPathView
		{
			char pad_0000[16];
			const char* path;
		};

		Asset_Generic* vehicleTag =
			Helpers::GetTypedAsset<Asset_Generic>(vehicle->tagID);
		const TagPathView* tagPath =
			reinterpret_cast<const TagPathView*>(vehicleTag);
		if (!tagPath || !tagPath->path)
		{
			// Preserve the already-established role through a transient tag miss.
			return bMatchesCachedScorpion
				? !scorpionCrosshairSeatCache.bIsDriver
				: false;
		}

		const bool isWarthog =
			std::strstr(tagPath->path, "warthog") != nullptr;
		const bool isScorpion =
			std::strstr(tagPath->path, "scorpion") != nullptr;

		if (outIsWarthog)
		{
			*outIsWarthog = isWarthog;
		}

		if (!isScorpion)
		{
			// Entering any confirmed non-Scorpion vehicle invalidates the old role.
			ResetScorpionCrosshairSeatCache();
		}

		if (!isWarthog && !isScorpion)
		{
			return false;
		}

		// Warthog seat 1 is the turret. Driver seat 0 and side seat 2 remain
		// excluded. Masking the packed retail field preserves the same rule.
		if (isWarthog)
		{
			const bool bIsWarthogGunner =
				normalizedSeatIndex == 1;
			if (outUsesNativeVehicleAim)
			{
				*outUsesNativeVehicleAim =
					bIsWarthogGunner;
			}
			return !bIsWarthogGunner;
		}

		if (!bMatchesCachedScorpion)
		{
			const bool bSamePlayerHasEstablishedScorpionRole =
				scorpionCrosshairSeatCache.bValid &&
				SameHaloIDLocal(
					scorpionCrosshairSeatCache.localPlayerID,
					localPlayerID);

			scorpionCrosshairSeatCache.localPlayerID = localPlayerID;
			scorpionCrosshairSeatCache.vehicleID = localUnit->parent;

			if (!bSamePlayerHasEstablishedScorpionRole)
			{
				// Read the seat field only on the genuine first Scorpion entry.
				// Once established, never reread it merely because damage caused
				// Halo to replace the parent datum or its salt.
				scorpionCrosshairSeatCache.bIsDriver =
					normalizedSeatIndex == 0;
			}

			scorpionCrosshairSeatCache.bValid = true;
			scorpionCrosshairSeatCache.confirmedOnFootFrames = 0;
		}

		// Keep the entry-time driver/passenger classification across both seat
		// field corruption and parent-datum replacement caused by vehicle damage.
		// It is discarded only after a sustained real exit or a confirmed switch
		// into a non-Scorpion vehicle.
		return !scorpionCrosshairSeatCache.bIsDriver;
	}

}

void Game::Init()
{
	Logger::log << "[Game] HaloCEVR initialising..." << std::endl;

#if USE_PROFILER
	profiler.Init();
#endif

	SetupConfigs();

	CreateConsole();

	PatchGame();

	Logger::log << "[Game] Found Game Type: " << Helpers::GetGameTypeString() << std::endl;
	Logger::log << "[Game] Found Game Version: " << Helpers::GetVersionString() << std::endl;

	bIsCustom = std::strcmp("halor", Helpers::GetGameTypeString()) != 0;

#ifdef EMULATE_VR
	vr = new VREmulator();
#else
	vr = new OpenVR();
#endif

	vr->Init();

	Game::instance.bLeftHanded = Game::instance.c_LeftHanded->Value();
	Game::instance.bUse3DOFAiming = Game::instance.c_Use3DOFAiming->Value();
	inputHandler.RegisterInputs();

	backBufferWidth = vr->GetViewWidth();
	backBufferHeight = vr->GetViewHeight();

	bHasShutdown = false;

	Logger::log << "[Game] HaloCEVR initialised" << std::endl;
}

void Game::Shutdown()
{
	if (bHasShutdown)
	{
		return;
	}
	bHasShutdown = true;

	Logger::log << "[Game] HaloCEVR shutting down..." << std::endl;

	vr->Shutdown();

#if USE_PROFILER
	profiler.Shutdown();
#endif

	MH_STATUS hookStatus = MH_DisableHook(MH_ALL_HOOKS);

	if (hookStatus != MH_OK)
	{
		Logger::log << "[Game] Could not remove hooks: " << MH_StatusToString(hookStatus) << std::endl;
	}

	hookStatus = MH_Uninitialize();

	if (hookStatus != MH_OK)
	{
		Logger::log << "[Game] Could not uninitialise MinHook: " << MH_StatusToString(hookStatus) << std::endl;
	}

	if (c_ShowConsole && c_ShowConsole->Value())
	{
		if (consoleOut)
		{
			fclose(consoleOut);
		}
		FreeConsole();
	}
}

void Game::OnInitDirectX()
{
	Logger::log << "[Game] Game has finished DirectX initialisation" << std::endl;

	if (!Helpers::GetDirect3DDevice9())
	{
		Logger::err << "Couldn't get game's direct3d device" << std::endl;
		return;
	}

	SetForegroundWindow(GetActiveWindow());

	// Ideally these values would be in a 4:3 ratio, but this causes the mouse position to stop aligning correctly
	overlayWidth = static_cast<UINT>(std::max(vr->GetViewHeight(), vr->GetViewWidth()) * c_UIOverlayRenderScale->Value());
	if (overlayWidth < 640) { // Clamp low to 640px so user can't degrade/break the config UI
		overlayWidth = 640;
	}
	overlayHeight = overlayWidth;

	vr->OnGameFinishInit();

	uiSurface = vr->GetUISurface();
	crosshairSurface = vr->GetCrosshairSurface();

	scopeSurfaces[0] = vr->GetScopeSurface();
	scopeTextures[0] = vr->GetScopeTexture();

	D3DSURFACE_DESC desc;
	scopeSurfaces[0]->GetDesc(&desc);

	CreateTextureAndSurface(desc.Width, desc.Height, desc.Usage, desc.Format, &scopeSurfaces[1], &scopeTextures[1]);
	CreateTextureAndSurface(desc.Width / 2, desc.Height / 2, desc.Usage, desc.Format, &scopeSurfaces[2], &scopeTextures[2]);

	uiRenderer = new UIRenderer();

	uiRenderer->Init(Helpers::GetDirect3DDevice9());

	settingsMenu = new SettingsMenu();

	settingsMenu->CreateMenus();
}

void Game::SetRenderTickProgress(float tickProgress)
{
	if (!std::isfinite(tickProgress) ||
		tickProgress < 0.0f ||
		tickProgress > 0.25f)
	{
		renderTickElapsedSeconds = 0.0f;
		bRenderTickElapsedSecondsValid = false;
		return;
	}

	// Halo supplies elapsed simulation-tick time here. Normalize it against the
	// fixed 30 Hz simulation interval so display refresh and frame pacing cannot
	// alter vehicle-camera interpolation.
	renderTickIntervalSeconds =
		VehicleReleasePolicy::SimulationTickSeconds;
	bRenderTickIntervalSecondsValid = true;
	renderTickElapsedSeconds = tickProgress;
	bRenderTickElapsedSecondsValid = true;
}

void Game::PreDrawFrame(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawFrame);
	cameraFrameSerial++;

	// Recover render-only state if an earlier DrawFrame sequence exited before
	// PostDrawFrame.
	EndFirstPersonVehicleBodyMask();
	if (bMotionTrackerHMDInputOverrideActive)
	{
		InputData& staleInput = Helpers::GetInputData();
		staleInput.yaw = motionTrackerSavedInputYaw;
		staleInput.pitch = motionTrackerSavedInputPitch;
		bMotionTrackerHMDInputOverrideActive = false;
	}

	lastDeltaTime = deltaTime;

	renderState = ERenderState::UNKNOWN;

	bool bIsLoading = Helpers::IsCampaignLoading();

	if (bWasLoading && !bIsLoading)
	{
		bNeedsRecentre = true;
	}
	bWasLoading = bIsLoading;

	//CalcFPS(deltaTime);

	vr->SetMouseVisibility(Helpers::IsMouseVisible());
	vr->UpdatePoses();

	UpdateCrosshairAndScope();

	StoreRenderTargets();

	sRect* window = Helpers::GetWindowRect();
	window->top = 0;
	window->left = 0;
	window->right = vr->GetViewWidth();
	window->bottom = vr->GetViewHeight();

	VR_PROFILE_START(Game_PreDrawFrame_ClearSurfaces);
	// Clear UI surfaces
	IDirect3DSurface9* currentSurface = nullptr;
	Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &currentSurface);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, crosshairSurface);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, scopeSurfaces[0]);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, currentSurface);
	currentSurface->Release();
	VR_PROFILE_STOP(Game_PreDrawFrame_ClearSurfaces);

	frustum1 = renderer->frustum;
	frustum2 = renderer->frustum2;

	if (bNeedsRecentre)
	{
		bNeedsRecentre = false;
		vr->Recentre();
	}

	UnitDynamicObject* Player = static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());
	if (Player)
	{
		const bool bPlayerInVehicle =
			Player->parent.id != 0xFFFF &&
			Player->parent.index != 0xFFFF;
		const bool bNativeHandheldPassenger =
			IsCurrentVehicleNativeHandheldPassengerView();
		const bool bNeedsOnFootFirstPersonViewModel =
			bNativeHandheldPassenger;
		const bool bNewVehicleViewModelSuppression =
			bPlayerInVehicle &&
			!bNeedsOnFootFirstPersonViewModel;

		if (bNewVehicleViewModelSuppression !=
			bVehicleViewModelSuppressionActive)
		{
			// Halo's immediate value 0 suppresses the first-person viewmodel;
			// value 1 uses the normal on-foot viewmodel path.
			Hooks::P_KeepViewModelVisible(
				bNewVehicleViewModelSuppression);

			bVehicleViewModelSuppressionActive =
				bNewVehicleViewModelSuppression;
		}

		bInVehicle = bPlayerInVehicle;
		bHasWeapon = Player->weapon.id != 0xffff;
	}
	else
	{
		if (bVehicleViewModelSuppressionActive)
		{
			Hooks::P_KeepViewModelVisible(false);
			bVehicleViewModelSuppressionActive = false;
		}
		bInVehicle = false;
		bHasWeapon = false;
	}

	ApplyFirstPersonVehicleDriverCamera();

	if (c_ShowRoomCentre->Value())
	{
		VR_PROFILE_SCOPE(Game_PreDrawFrame_DrawRoomCentre);

		Vector3 position = Helpers::GetCamera().position;
		position.z -= 0.62f;
		Vector3 upVector(0.0f, 0.0f, 1.0f);
		Vector3 forwardVector(1.0f, 0.0f, 0.0f);

		inGameRenderer.DrawPolygon(position, upVector, forwardVector, 8, MetresToWorld(0.25f), D3DCOLOR_ARGB(50, 85, 250, 239), false);
	}


	// Controller aim is written into Halo's hidden desktop input cache. The
	// vehicle motion tracker reads that same cache, so substitute HMD angles only
	// for rendering and restore them in PostDrawFrame.
	if (bInVehicle || IsMultiplayerSession())
	{
		Vector3 hmdLook = vr->GetHMDTransform().getLeftAxis();
		const float horizontalLengthSqr =
			hmdLook.x * hmdLook.x + hmdLook.y * hmdLook.y;

		if (std::isfinite(hmdLook.x) &&
			std::isfinite(hmdLook.y) &&
			std::isfinite(hmdLook.z) &&
			horizontalLengthSqr > 0.00000001f)
		{
			InputData& radarInput = Helpers::GetInputData();
			motionTrackerSavedInputYaw = radarInput.yaw;
			motionTrackerSavedInputPitch = radarInput.pitch;
			radarInput.yaw = atan2(hmdLook.y, hmdLook.x);
			radarInput.pitch = atan2(
				hmdLook.z,
				sqrt(horizontalLengthSqr));
			bMotionTrackerHMDInputOverrideActive = true;
		}
	}

	vr->PreDrawFrame(renderer, deltaTime);
}

void Game::PreDrawEye(Renderer* renderer, float deltaTime, int eye)
{
	VR_PROFILE_SCOPE(Game_PreDrawEye);

	// Recover safely if an earlier eye render was interrupted.
	EndFirstPersonVehicleBodyMask();

	renderState = eye == 0 ? ERenderState::LEFT_EYE : ERenderState::RIGHT_EYE;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	vr->UpdateCameraFrustum(&renderer->frustum, eye);
	vr->UpdateCameraFrustum(&renderer->frustum2, eye);

	RenderTarget* primaryRenderTarget = Helpers::GetRenderTargets();

	primaryRenderTarget[0].renderSurface = vr->GetRenderSurface(eye);
	primaryRenderTarget[0].renderTexture = vr->GetRenderTexture(eye);
	primaryRenderTarget[0].width = vr->GetViewWidth();
	primaryRenderTarget[0].height = vr->GetViewHeight();

	inGameRenderer.ExtractMatrices(renderer);

	// This render-only body mask is restored as soon as Halo's eye draw returns.
	BeginFirstPersonVehicleBodyMask(renderer);
}


void Game::PostDrawEye(struct Renderer* renderer, float deltaTime, int eye)
{
	VR_PROFILE_SCOPE(Game_PostDrawEye);

	// Halo's world draw is complete. Restore the exact biped state before
	// HaloCEVR renders its own overlays or any later pass begins.
	EndFirstPersonVehicleBodyMask();

	if (Helpers::IsLoading() || Helpers::IsCampaignLoading())
	{
		Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	}

	inGameRenderer.Render(Helpers::GetDirect3DDevice9());
}

bool Game::PreDrawScope(Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawScope);

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());

	if (!player || player->zoom == -1)
	{
		return false;
	}

	renderState = ERenderState::SCOPE;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	// Scale tan(FOV / 2) so values above 1.00 reduce magnification without
	// allowing the perspective projection to reach 180 degrees.
	const short zoomLevel =
		Helpers::GetInputData().zoomLevel;

	float opticalScale = 1.0f;
	if (zoomLevel == 0)
	{
		opticalScale = bIsCustom
			? c_CustomEditionZoom1Magnification->Value()
			: c_CampaignZoom1Magnification->Value();
	}
	else if (zoomLevel == 1)
	{
		opticalScale = bIsCustom
			? c_CustomEditionZoom2Magnification->Value()
			: c_CampaignZoom2Magnification->Value();
	}

	if (!std::isfinite(opticalScale))
	{
		opticalScale = 1.0f;
	}
	opticalScale =
		std::clamp(opticalScale, 0.10f, 20.00f);

	constexpr float MinimumScopeFOV = 0.05f;
	constexpr float MaximumScopeFOV = 3.13f;

	auto ScaleScopeFOV =
		[
			opticalScale,
			MinimumScopeFOV,
			MaximumScopeFOV
		](float originalFOV)
		{
			if (!std::isfinite(originalFOV))
			{
				return originalFOV;
			}

			const float safeOriginalFOV =
				std::clamp(
					originalFOV,
					MinimumScopeFOV,
					MaximumScopeFOV);
			const float originalHalfFOV =
				safeOriginalFOV * 0.5f;
			const float scaledHalfFOV =
				std::atan(
					std::tan(originalHalfFOV) *
					opticalScale);

			return std::clamp(
				scaledHalfFOV * 2.0f,
				MinimumScopeFOV,
				MaximumScopeFOV);
		};

	renderer->frustum.fov =
		ScaleScopeFOV(renderer->frustum.fov);
	renderer->frustum2.fov =
		ScaleScopeFOV(renderer->frustum2.fov);

	Vector3 aimPos, aimDir, upDir;

	weaponHandler.GetWorldWeaponAim(aimPos, aimDir, upDir);

	if (!Game::instance.c_LockScopeRoll->Value())
	{
		upDir = Vector3(0.0f, 0.0f, 1.0f);
		upDir = aimDir.cross(upDir);
		upDir = upDir.cross(aimDir);
	}

	renderer->frustum.position = aimPos;
	renderer->frustum2.position = aimPos;
	renderer->frustum.facingDirection = aimDir;
	renderer->frustum2.facingDirection = aimDir;
	renderer->frustum.upDirection = upDir;
	renderer->frustum2.upDirection = upDir;

	RenderTarget* primaryRenderTarget = Helpers::GetRenderTargets();

	primaryRenderTarget[0].renderSurface = scopeSurfaces[0];
	primaryRenderTarget[0].renderTexture = scopeTextures[0];
	primaryRenderTarget[0].width = vr->GetScopeWidth();
	primaryRenderTarget[0].height = vr->GetScopeHeight();
	primaryRenderTarget[1].renderSurface = scopeSurfaces[1];
	primaryRenderTarget[1].renderTexture = scopeTextures[1];
	primaryRenderTarget[1].width = vr->GetScopeWidth();
	primaryRenderTarget[1].height = vr->GetScopeHeight();
	primaryRenderTarget[2].renderSurface = scopeSurfaces[2];
	primaryRenderTarget[2].renderTexture = scopeTextures[2];
	primaryRenderTarget[2].width = vr->GetScopeWidth() / 2;
	primaryRenderTarget[2].height = vr->GetScopeHeight() / 2;


	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = vr->GetScopeWidth();
	windowMain->bottom = vr->GetScopeHeight();

	{
		sRect& window = renderer->frustum.WindowViewport;
		window.top = 0;
		window.left = 0;
		window.right = vr->GetScopeWidth();
		window.bottom = vr->GetScopeHeight();

		sRect& window2 = renderer->frustum2.WindowViewport;
		window2.top = 0;
		window2.left = 0;
		window2.right = vr->GetScopeWidth();
		window2.bottom = vr->GetScopeHeight();
	}

	return true;
}

void Game::PostDrawScope(Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PostDrawScope);
	RestoreRenderTargets();

	Vector2 innerSize = Vector2(0.0f, 0.0f);
	Vector2 size = Vector2(static_cast<float>(vr->GetScopeWidth()), static_cast<float>(vr->GetScopeHeight()));
	Vector2 centre = size / 2;
	int sides = 32;
	float radius = size.y * 0.25f;
	D3DCOLOR color = D3DCOLOR_ARGB(255, 0, 0, 0);

	scopeRenderer.ExtractMatrices(renderer);

	// Sniper scope is a rounded square, so we need to separate the quadrants and change the radius
	if (weaponHandler.IsSniperScope())
	{
		radius = size.y * 0.03125f;
		const float scopeWidth = 0.605f * size.x - radius * 2.0f;
		const float scopeHeight = 0.505f * size.y - radius * 2.0f;
		innerSize = Vector2(scopeWidth, scopeHeight);
	}

	// For original scope mode we want to show more of the original scope graphics
	if (!ShouldUseOriginalScope())
	{
		scopeRenderer.DrawInvertedShape2D(centre, innerSize, size, sides, radius, color);
	}

	scopeRenderer.Render(Helpers::GetDirect3DDevice9());
	scopeRenderer.PostRender();

}

void Game::PreDrawMirror(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawMirror);

	renderState = ERenderState::GAME;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	RestoreRenderTargets();

	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = Helpers::GetRenderTargets()[0].width;
	windowMain->bottom = Helpers::GetRenderTargets()[0].height;

	inGameRenderer.ExtractMatrices(renderer);
}

void Game::PostDrawMirror(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PostDrawMirror);
	// Do something here to copy the image into the backbuffer correctly

	inGameRenderer.ClearRenderTargets();
	inGameRenderer.Render(Helpers::GetDirect3DDevice9());
}

void Game::PostDrawFrame(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_START(Game_PostDrawFrame);

	// Defensive cleanup for an interrupted eye draw.
	EndFirstPersonVehicleBodyMask();

	// Restore the exact controller/network input cache after every rendered pass.
	if (bMotionTrackerHMDInputOverrideActive)
	{
		InputData& radarInput = Helpers::GetInputData();
		radarInput.yaw = motionTrackerSavedInputYaw;
		radarInput.pitch = motionTrackerSavedInputPitch;
		bMotionTrackerHMDInputOverrideActive = false;
	}

	RestoreRenderTargets();
	vr->PostDrawFrame(renderer, deltaTime);
	inGameRenderer.PostRender();

	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = Helpers::GetRenderTargets()[0].width;
	windowMain->bottom = Helpers::GetRenderTargets()[0].height;

	if (c_DrawMirror->Value() && mirrorSource != ERenderState::GAME)
	{
		VR_PROFILE_SCOPE(Game_PostDrawFrame_Mirror);

		int sWidth = vr->GetViewWidth();
		int sHeight = vr->GetViewHeight();

		float sourceAspect = static_cast<float>(sWidth) / static_cast<float>(sHeight);

		int dWidth = Helpers::GetRenderTargets()[0].width;
		int dHeight = Helpers::GetRenderTargets()[0].height;

		int trueWidth = 640;
		int trueHeight = 480;

		float destAspect = static_cast<float>(trueWidth) / static_cast<float>(trueHeight);

		RECT destRect{};

		if (sourceAspect > destAspect)
		{
			destRect.left = 0;
			destRect.right = dWidth;

			float scale = destAspect / sourceAspect;

			int scaledSize = static_cast<int>(0.5f * (1.0f - scale) * dHeight);

			destRect.top = scaledSize;
			destRect.bottom = dHeight - scaledSize;
		}
		else
		{
			destRect.top = 0;
			destRect.bottom = dHeight;

			float scale = sourceAspect / destAspect;

			int scaledSize = static_cast<int>(0.5f * (1.0f - scale) * dWidth);

			destRect.left = scaledSize;
			destRect.right = dWidth - scaledSize;
		}


		int eye = mirrorSource == ERenderState::LEFT_EYE ? 0 : 1;

		IDirect3DSurface9* currentSurface = nullptr;
		Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &currentSurface);
		Helpers::GetDirect3DDevice9()->SetRenderTarget(0, Helpers::GetRenderTargets()[0].renderSurface);
		Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
		Helpers::GetDirect3DDevice9()->SetRenderTarget(0, currentSurface);
		currentSurface->Release();
		Helpers::GetDirect3DDevice9()->StretchRect(vr->GetRenderSurface(eye), nullptr, Helpers::GetRenderTargets()[0].renderSurface, &destRect, D3DTEXF_LINEAR);
	}

	VR_PROFILE_STOP(Game_PostDrawFrame);

#if USE_PROFILER
	profiler.NewFrame();
#endif
}

bool Game::PreDrawHUD()
{
	VR_PROFILE_SCOPE(Game_PreDrawHUD);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// Remove zoom effect from game view
		if (GetRenderState() == ERenderState::GAME)
		{
			short* zoom = &Helpers::GetInputData().zoomLevel;
			realZoom = *zoom;
			*zoom = -1;
		}

		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME || GetRenderState() == ERenderState::SCOPE;
	}

	// Heading-based HUD elements, including the motion tracker, should use the
	// player's head direction rather than controller/network aim.
	bHUDHMDHeadingOverrideActive = false;
	hudHMDHeadingUnit = nullptr;

	Vector3 hmdHeading = vr->GetHMDTransform().getLeftAxis();
	hmdHeading.z = 0.0f;
	if (std::isfinite(hmdHeading.x) &&
		std::isfinite(hmdHeading.y) &&
		hmdHeading.lengthSqr() > 0.00000001f)
	{
		hmdHeading.normalize();

		Camera& hudCamera = Helpers::GetCamera();
		hudSavedCameraLookDir = hudCamera.lookDir;
		hudSavedLastLookDir = LastLookDir;
		hudCamera.lookDir = hmdHeading;
		LastLookDir = hmdHeading;

		hudHMDHeadingUnit =
			static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());
		if (hudHMDHeadingUnit)
		{
			hudSavedFacingDir = hudHMDHeadingUnit->facingDir;
			hudSavedFacing = hudHMDHeadingUnit->facing;
			hudSavedDesiredAim = hudHMDHeadingUnit->desiredAim;
			hudSavedAim = hudHMDHeadingUnit->aim;
			hudSavedAim2 = hudHMDHeadingUnit->aim2;
			hudSavedAim3 = hudHMDHeadingUnit->aim3;

			hudHMDHeadingUnit->facingDir = hmdHeading;
			hudHMDHeadingUnit->facing = hmdHeading;
			hudHMDHeadingUnit->desiredAim = hmdHeading;
			hudHMDHeadingUnit->aim = hmdHeading;
			hudHMDHeadingUnit->aim2 = hmdHeading;
			hudHMDHeadingUnit->aim3 = hmdHeading;
		}

		bHUDHMDHeadingOverrideActive = true;
	}

	short* zoom = &Helpers::GetInputData().zoomLevel;
	realZoom = *zoom;
	*zoom = -1;

	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;
	realUIWidth = Helpers::GetRenderTargets()[1].width;
	realUIHeight = Helpers::GetRenderTargets()[1].height;
	Helpers::GetRenderTargets()[1].width = overlayWidth;
	Helpers::GetRenderTargets()[1].height = overlayHeight;

	sRect* windowMain = Helpers::GetCurrentRect();
	realRect = *windowMain;

	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = overlayWidth;
	windowMain->bottom = overlayHeight;

	return true;
}

void Game::PostDrawHUD()
{
	VR_PROFILE_SCOPE(Game_PostDrawHUD);

	// Defensive cleanup if Halo exits the player-indicator pass unexpectedly.
	EndWorldSpaceIndicatorProjectionOverride();

	// Restore gameplay and network aim immediately after the HUD draw.
	if (bHUDHMDHeadingOverrideActive)
	{
		Camera& hudCamera = Helpers::GetCamera();
		hudCamera.lookDir = hudSavedCameraLookDir;
		LastLookDir = hudSavedLastLookDir;

		if (hudHMDHeadingUnit)
		{
			hudHMDHeadingUnit->facingDir = hudSavedFacingDir;
			hudHMDHeadingUnit->facing = hudSavedFacing;
			hudHMDHeadingUnit->desiredAim = hudSavedDesiredAim;
			hudHMDHeadingUnit->aim = hudSavedAim;
			hudHMDHeadingUnit->aim2 = hudSavedAim2;
			hudHMDHeadingUnit->aim3 = hudSavedAim3;
		}

		hudHMDHeadingUnit = nullptr;
		bHUDHMDHeadingOverrideActive = false;
	}

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// Remove zoom effect from game view
		if (GetRenderState() == ERenderState::GAME)
		{
			short* zoom = &Helpers::GetInputData().zoomLevel;
			*zoom = realZoom;
		}

		return;
	}

	short* zoom = &Helpers::GetInputData().zoomLevel;
	*zoom = realZoom;

	sRect* windowMain = Helpers::GetCurrentRect();
	*windowMain = realRect;

	Helpers::GetRenderTargets()[1].width = realUIWidth;
	Helpers::GetRenderTargets()[1].height = realUIHeight;

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiRealSurface);

}

bool Game::BeginWorldSpaceIndicatorProjectionOverride()
{
	if (GetRenderState() != ERenderState::LEFT_EYE ||
		bWorldSpaceIndicatorProjectionOverrideActive)
	{
		return false;
	}

	CameraRenderMatrices* activeMatrices =
		Helpers::GetActiveCameraMatrices();
	if (!activeMatrices)
	{
		return false;
	}

	Matrix4 hmdTransform = vr->GetHMDTransform(true);
	Vector3 hmdForward = hmdTransform.getLeftAxis();
	hmdForward.z = 0.0f;

	if (!std::isfinite(hmdForward.x) ||
		!std::isfinite(hmdForward.y) ||
		hmdForward.lengthSqr() <= 0.00000001f)
	{
		return false;
	}
	hmdForward.normalize();

	Vector3 hmdPosition =
		hmdTransform * Vector3(0.0f, 0.0f, 0.0f);

	CameraFrustum overlayFrustum = frustum1;

	// Match the compositor overlay: center-HMD position, horizontal HMD yaw,
	// and world-up so head pitch/roll are not applied once in Halo's indicator
	// projection and a second time by the separately positioned HUD overlay.
	overlayFrustum.position =
		frustum1.position +
		hmdPosition * MetresToWorld(1.0f);
	overlayFrustum.facingDirection = hmdForward;
	overlayFrustum.upDirection = Vector3(0.0f, 0.0f, 1.0f);

	// OpenVR displays the square UI texture with a 0.75 vertical transform,
	// producing a physical 4:3 overlay. Derive the matching vertical FOV from
	// the user's configured width and distance.
	const float overlayWidthMetres =
		c_UIOverlayScale ? c_UIOverlayScale->Value() : 10.0f;
	const float overlayDistanceMetres =
		c_UIOverlayDistance ? c_UIOverlayDistance->Value() : 15.0f;
	const float safeDistance =
		overlayDistanceMetres > 0.01f
			? overlayDistanceMetres
			: 0.01f;
	const float overlayHeightMetres =
		overlayWidthMetres * 0.75f;
	overlayFrustum.fov =
		2.0f * std::atan(
			(overlayHeightMetres * 0.5f) /
			safeDistance);

	// Halo's HUD coordinate system is canonically 640x480. The square render
	// texture is vertically compressed by the compositor back to this 4:3 shape.
	overlayFrustum.WindowViewport.top = 0;
	overlayFrustum.WindowViewport.left = 0;
	overlayFrustum.WindowViewport.bottom = 480;
	overlayFrustum.WindowViewport.right = 640;
	overlayFrustum.InnerViewport =
		overlayFrustum.WindowViewport;

	worldSpaceIndicatorSavedCameraMatrices = *activeMatrices;
	worldSpaceIndicatorActiveCameraMatrices = activeMatrices;

	Viewport overlayViewport = activeMatrices->viewport;
	Hooks::SetCameraMatrices(
		&overlayViewport,
		&overlayFrustum,
		activeMatrices,
		true);

	bWorldSpaceIndicatorProjectionOverrideActive = true;
	return true;
}

void Game::EndWorldSpaceIndicatorProjectionOverride()
{
	if (!bWorldSpaceIndicatorProjectionOverrideActive)
	{
		return;
	}

	if (worldSpaceIndicatorActiveCameraMatrices)
	{
		*worldSpaceIndicatorActiveCameraMatrices =
			worldSpaceIndicatorSavedCameraMatrices;
	}

	worldSpaceIndicatorActiveCameraMatrices = nullptr;
	bWorldSpaceIndicatorProjectionOverrideActive = false;
}

bool Game::PreDrawMenu()
{
	VR_PROFILE_SCOPE(Game_PreDrawMenu);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME;
	}

	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;

	return true;
}

void Game::PostDrawMenu()
{
	VR_PROFILE_SCOPE(Game_PostDrawMenu);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		return;
	}

	if (Helpers::IsMouseVisible())
	{
		uiRenderer->Render();
	}

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiRealSurface);
}

D3DVIEWPORT9 currentViewport;
bool Game::PreDrawLoading(int param1, struct Renderer* renderer)
{
	VR_PROFILE_SCOPE(Game_PreDrawLoading);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME;
	}

	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;

	return true;
}

void Game::PostDrawLoading(int param1, struct Renderer* renderer)
{
	VR_PROFILE_SCOPE(Game_PostDrawLoading);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		return;
	}

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
}

bool Game::PreDrawCrosshair(short* anchorLocation)
{
	VR_PROFILE_SCOPE(Game_PreDrawCrosshair);

	// Draw things normally for the scope
	if (GetRenderState() == ERenderState::SCOPE)
	{
		return true;
	}

	crosshairRealSurface = Helpers::GetRenderTargets()[1].renderSurface;

	// Vehicle reticles use Halo's center-anchored crosshair pass. Retail also
	// invokes this hook for health and radar elements at anchors 0, 1 and 2, so
	// only the center pass belongs on the right-stick crosshair surface.
	const bool bCenteredCrosshair =
		anchorLocation &&
		*anchorLocation == 4; // Centre = 4

	if (bCenteredCrosshair)
	{
		if (realZoom != -1)
		{
			return false;
		}

		Helpers::GetRenderTargets()[1].renderSurface =
			crosshairSurface;
		Helpers::GetDirect3DDevice9()->SetRenderTarget(
			0,
			crosshairSurface);
	}

	return true;
}

void Game::PostDrawCrosshair()
{
	VR_PROFILE_SCOPE(Game_PostDrawCrosshair);

	// Draw things normally for the scope
	if (GetRenderState() == ERenderState::SCOPE)
	{
		return;
	}

	Helpers::GetRenderTargets()[1].renderSurface =
		crosshairRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(
		0,
		crosshairRealSurface);
}

void Game::PreDrawImage(void* param1, void* param2)
{
	VR_PROFILE_SCOPE(Game_PreDrawImage);

	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_ALPHAFUNC, &realAlphaFunc);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_SRCBLENDALPHA, &realAlphaSrc);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_DESTBLENDALPHA, &realAlphaDest);

	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHAFUNC, D3DBLENDOP_ADD);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_SRCALPHA);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
}

void Game::PostDrawImage(void* param1, void* param2)
{
	VR_PROFILE_SCOPE(Game_PostDrawImage);

	DWORD tmp1, tmp2, tmp3;

	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_ALPHAFUNC, &tmp1);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_SRCBLENDALPHA, &tmp2);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_DESTBLENDALPHA, &tmp3);

	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHAFUNC, realAlphaFunc);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_SRCBLENDALPHA, realAlphaSrc);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_DESTBLENDALPHA, realAlphaDest);
}

void Game::UpdateViewModel(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* BoneTransforms, Transform* OutBoneTransforms)
{
	VR_PROFILE_SCOPE(Game_UpdateViewModel);
	weaponHandler.UpdateViewModel(id, pos, facing, up, BoneTransforms, OutBoneTransforms);

	if (Game::instance.bIsFiring)
	{
		weaponHandler.SetPlasmaPistolCharge();
	}

	weaponHandler.HandlePlasmaPistolCharge();
}

void Game::PreFireWeapon(HaloID& weaponID, short param2)
{
	VR_PROFILE_SCOPE(Game_PreFireWeapon);
	weaponHandler.PreFireWeapon(weaponID, param2);
}

void Game::PostFireWeapon(HaloID& weaponID, short param2)
{
	VR_PROFILE_SCOPE(Game_PostFireWeapon);
	weaponHandler.PostFireWeapon(weaponID, param2);
}

void Game::PreThrowGrenade(HaloID& playerID)
{
	VR_PROFILE_SCOPE(Game_PreThrowGrenade);
	weaponHandler.PreThrowGrenade(playerID);
}

void Game::PostThrowGrenade(HaloID& playerID)
{
	VR_PROFILE_SCOPE(Game_PostThrowGrenade);
	weaponHandler.PostThrowGrenade(playerID);
}

void Game::SetMPRoomScaleManualWorldInput(const Vector3& manualWorldInput)
{
	mpRoomScaleManualWorldInput = manualWorldInput;
	mpRoomScaleManualWorldInput.z = 0.0f;
	if (!std::isfinite(mpRoomScaleManualWorldInput.x) ||
		!std::isfinite(mpRoomScaleManualWorldInput.y))
	{
		mpRoomScaleManualWorldInput = Vector3(0.0f, 0.0f, 0.0f);
	}
}

void Game::UpdateInputs()
{
	VR_PROFILE_SCOPE(Game_UpdateInputs);

	// Hot reload this flag
	bUse3DOFAiming = c_Use3DOFAiming->Value();

	inputHandler.UpdateInputs(bInVehicle);

	UpdateRoomScaleMovement();

#if USE_PROFILER
	static bool bWasPressed = false;

	bool bPressed = GetAsyncKeyState(VK_F5) & 0x8000;

	if (bPressed && !bWasPressed)
	{
		DumpProfilerData();
	}

	bWasPressed = bPressed;
#endif
}

void Game::CalculateSmoothedInput()
{
	VR_PROFILE_SCOPE(Game_CalculateSmoothedInput);
	inputHandler.CalculateSmoothedInput();
}

void Game::BeginTemporaryFireRelocation()
{
	// Joined-client roomscale observes body position independently during firing.
	if (!c_RoomScaleMovement || !c_RoomScaleMovement->Value() ||
		!ShouldUseMPRoomScaleNetworkServo())
	{
		return;
	}

	bMPRoomScaleTemporaryFireRelocation = true;
}

void Game::EndTemporaryFireRelocation()
{
	if (!bMPRoomScaleTemporaryFireRelocation)
	{
		return;
	}

	bMPRoomScaleTemporaryFireRelocation = false;
}


void Game::UpdateMPRoomScaleBodyOnNetworkTick()
{
	VR_PROFILE_SCOPE(Game_UpdateMPRoomScaleBodyOnNetworkTick);

	if (!c_RoomScaleMovement->Value())
	{
		mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
		bMPRoomScaleHasAccumulatedOffset = false;
		bMPRoomScaleHasAppliedOffset = false;
		mpRoomScaleLastInputShare = 0.0f;
		mpRoomScaleSmoothedRun = 0.0f;
		mpRoomScaleSmoothedStrafe = 0.0f;
		mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		ResetMPRoomScaleCommandHistory();
		bMPRoomScaleCorrectionActive = false;
		mpRoomScaleActivationHoldSeconds = 0.0f;
		bMPRoomScaleHasTrackingSample = false;
		bIgnoreNextRoomScaleMovement = true;
		return;
	}

	if (!ShouldUseMPRoomScaleNetworkServo())
	{
		return;
	}

	const bool bInCutscene = Helpers::GetCutsceneData()->bInCutscene;
	if (bInCutscene || bInVehicle)
	{
		mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
		bMPRoomScaleHasAccumulatedOffset = false;
		bMPRoomScaleHasAppliedOffset = false;
		mpRoomScaleLastInputShare = 0.0f;
		mpRoomScaleSmoothedRun = 0.0f;
		mpRoomScaleSmoothedStrafe = 0.0f;
		mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		ResetMPRoomScaleCommandHistory();
		bMPRoomScaleCorrectionActive = false;
		mpRoomScaleActivationHoldSeconds = 0.0f;
		bMPRoomScaleHasTrackingSample = false;
		bIgnoreNextRoomScaleMovement = true;
		return;
	}

	// Ignore the temporary muzzle relocation used while FireWeapon is running.
	if (bMPRoomScaleTemporaryFireRelocation)
	{
		return;
	}

	HaloID targetID{ 0xFFFF, 0xFFFF };
	UnitDynamicObject* target = nullptr;

	// Only the PlayerDatum/localID resolver may select a roomscale body.
	// Camera-nearest fallback is unsafe in Coop Evolved because a brief respawn gap
	// can select an AI or remote player's biped and feed its motion into our tracking
	// origin acknowledgement.
	if (Helpers::GetLocalPlayerID(targetID))
	{
		BaseDynamicObject* localObject = Helpers::GetDynamicObject(targetID);
		if (localObject && localObject->N0000027E == ObjectType::BIPED)
		{
			target = static_cast<UnitDynamicObject*>(localObject);
		}
	}

	if (!target)
	{
		mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
		bMPRoomScaleHasAccumulatedOffset = false;
		bMPRoomScaleHasAppliedOffset = false;
		mpRoomScaleLastInputShare = 0.0f;
		mpRoomScaleSmoothedRun = 0.0f;
		mpRoomScaleSmoothedStrafe = 0.0f;
		mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		ResetMPRoomScaleCommandHistory();
		bMPRoomScaleCorrectionActive = false;
		mpRoomScaleActivationHoldSeconds = 0.0f;
		bMPRoomScaleHasTrackingSample = false;
		bIgnoreNextRoomScaleMovement = true;
		return;
	}

	const bool bTargetChanged = !bMPRoomScaleHasAppliedOffset ||
		!SameHaloIDLocal(mpRoomScaleLastAppliedID, targetID);
	Vector3 actualBodyDelta(0.0f, 0.0f, 0.0f);
	Vector3 compensatedOffset(0.0f, 0.0f, 0.0f);
	if (bTargetChanged || bIgnoreNextRoomScaleMovement)
	{
		// Never carry a queued displacement across a spawn, seat exit or local-biped
		// hand-off. Consume any transition-only HMD delta so it cannot become movement
		// for the newly acquired body.
		Vector3 headWorldOffset = vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);
		headWorldOffset *= MetresToWorld(1.0f);
		headWorldOffset.z = 0.0f;
		if (bIgnoreNextRoomScaleMovement && headWorldOffset.lengthSqr() > 0.0000001f)
		{
			Vector3 trackingOffset = headWorldOffset;
			const float rotation = vr->GetYawOffset() * (3.141593f / 180.0f);
			const float cosAngle = std::cos(rotation);
			const float sinAngle = std::sin(rotation);
			const float newX = trackingOffset.x * cosAngle - trackingOffset.y * sinAngle;
			const float newY = trackingOffset.x * sinAngle + trackingOffset.y * cosAngle;
			trackingOffset.x = newX;
			trackingOffset.y = newY;
			vr->SetLocationOffset(trackingOffset * WorldToMetres(1.0f) + vr->GetLocationOffset());
		}

		mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
		mpRoomScaleLastAppliedOffset = Vector3(0.0f, 0.0f, 0.0f);
		mpRoomScaleLastAppliedPosition = target->position;
		mpRoomScaleLastAppliedID = targetID;
		bMPRoomScaleHasAppliedOffset = true;
		bMPRoomScaleHasAccumulatedOffset = false;
		mpRoomScaleLastInputShare = 0.0f;
		mpRoomScaleSmoothedRun = 0.0f;
		mpRoomScaleSmoothedStrafe = 0.0f;
		mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		ResetMPRoomScaleCommandHistory();
		bMPRoomScaleCorrectionActive = false;
		mpRoomScaleActivationHoldSeconds = 0.0f;
		bMPRoomScaleHasTrackingSample = false;
		bIgnoreNextRoomScaleMovement = false;

	}
	else
	{
		actualBodyDelta = target->position - mpRoomScaleLastAppliedPosition;
		actualBodyDelta.z = 0.0f;
		mpRoomScaleLastAppliedPosition = target->position;

		// The player has already seen their real step immediately through normal HMD
		// tracking. Cancel only roomscale-authored body progress from the OpenVR
		// origin so Halo's delayed network catch-up does not move the world again.
		Vector3 headWorldOffset =
			vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);
		headWorldOffset *= MetresToWorld(1.0f);
		headWorldOffset.z = 0.0f;

		const float teleportLimit = MetresToWorld(0.75f);
		const float requestLength = mpRoomScaleLastAppliedOffset.length();
		if (actualBodyDelta.lengthSqr() > teleportLimit * teleportLimit)
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			bMPRoomScaleHasAccumulatedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;
		}
		else
		{
			MPRoomScaleCommandSample matchedSample{};
			const bool bHadCausalHistory = mpRoomScaleCommandHistoryCount > 0;
			const bool bMatchedCausalCommand = MatchMPRoomScaleCommand(
				targetID, actualBodyDelta, matchedSample);

			if (bMatchedCausalCommand)
			{
				// The digital multiplayer command may itself point somewhat sideways from
				// the raw stick direction. Do not treat that quantisation error as
				// roomscale. Resolve the accepted body delta against the exact combined
				// command, then credit only the independently authored roomscale command.
				const float combinedCommandLengthSqr =
					matchedSample.combinedCommand.lengthSqr();
				const float bodyResponseScale = combinedCommandLengthSqr > 0.000001f
					? actualBodyDelta.dot(matchedSample.combinedCommand) /
						combinedCommandLengthSqr
					: 0.0f;
				Vector3 estimatedRoomDelta = bodyResponseScale > 0.0f
					? matchedSample.roomCommand * bodyResponseScale
					: Vector3(0.0f, 0.0f, 0.0f);
				const float actualBodyDistance = actualBodyDelta.length();
				const float estimatedRoomDistance = estimatedRoomDelta.length();
				if (estimatedRoomDistance > actualBodyDistance &&
					actualBodyDistance > 0.0f)
				{
					estimatedRoomDelta *= actualBodyDistance / estimatedRoomDistance;
				}

				Vector3 roomDirection = estimatedRoomDelta;
				float creditedRoomDistance = roomDirection.length();
				float availableRoomOffset = 0.0f;
				if (creditedRoomDistance > 0.000001f)
				{
					roomDirection /= creditedRoomDistance;
					availableRoomOffset = headWorldOffset.dot(roomDirection);
					if (availableRoomOffset <= 0.000001f)
					{
						creditedRoomDistance = 0.0f;
					}
					else if (creditedRoomDistance > availableRoomOffset)
					{
						creditedRoomDistance = availableRoomOffset;
					}
					compensatedOffset = roomDirection * creditedRoomDistance;
				}

				if (compensatedOffset.lengthSqr() > 0.0000001f)
				{
					Vector3 trackingOffset = compensatedOffset;
					const float rotation = vr->GetYawOffset() * (3.141593f / 180.0f);
					const float cosAngle = std::cos(rotation);
					const float sinAngle = std::sin(rotation);
					const float newX = trackingOffset.x * cosAngle - trackingOffset.y * sinAngle;
					const float newY = trackingOffset.x * sinAngle + trackingOffset.y * cosAngle;
					trackingOffset.x = newX;
					trackingOffset.y = newY;
					vr->SetLocationOffset(trackingOffset * WorldToMetres(1.0f) +
						vr->GetLocationOffset());
				}

				mpRoomScaleAccumulatedOffset = headWorldOffset - compensatedOffset;
				mpRoomScaleAccumulatedOffset.z = 0.0f;
				ResetMPRoomScaleCommandHistory();
			}
			else if (bHadCausalHistory &&
				actualBodyDelta.lengthSqr() > 0.000001f)
			{
				ResetMPRoomScaleCommandHistory();
			}
			else if (bMPRoomScaleHasAccumulatedOffset && requestLength > 0.0001f)
			{
				// Stick-idle and fallback paths use positive-only acknowledgement. Causal
				// history is populated only when roomscale survived strict axis ownership.
				const Vector3 requestDirection =
					mpRoomScaleLastAppliedOffset / requestLength;
				const float forwardProgress =
					actualBodyDelta.dot(requestDirection);
				const float availableOffset =
					headWorldOffset.dot(requestDirection);

				if (mpRoomScaleLastInputShare > 0.0f &&
					forwardProgress > 0.000001f &&
					availableOffset > 0.000001f)
				{
					float compensatedDistance =
						forwardProgress * mpRoomScaleLastInputShare;
					if (compensatedDistance > availableOffset)
					{
						compensatedDistance = availableOffset;
					}
					compensatedOffset = requestDirection * compensatedDistance;

					if (compensatedOffset.lengthSqr() > 0.0000001f)
					{
						Vector3 trackingOffset = compensatedOffset;
						const float rotation =
							vr->GetYawOffset() * (3.141593f / 180.0f);
						const float cosAngle = std::cos(rotation);
						const float sinAngle = std::sin(rotation);
						const float newX = trackingOffset.x * cosAngle -
							trackingOffset.y * sinAngle;
						const float newY = trackingOffset.x * sinAngle +
							trackingOffset.y * cosAngle;
						trackingOffset.x = newX;
						trackingOffset.y = newY;
						vr->SetLocationOffset(
							trackingOffset * WorldToMetres(1.0f) +
							vr->GetLocationOffset());
					}
				}

				mpRoomScaleAccumulatedOffset =
					headWorldOffset - compensatedOffset;
				mpRoomScaleAccumulatedOffset.z = 0.0f;
			}
		}
	}

	mpRoomScaleLastResolvedID = targetID;
}

void Game::UpdateRoomScaleMovement()
{
	VR_PROFILE_SCOPE(Game_UpdateRoomScaleMovement);

	if (!c_RoomScaleMovement->Value())
	{
		mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
		bMPRoomScaleHasAccumulatedOffset = false;
		bMPRoomScaleHasAppliedOffset = false;
		mpRoomScaleLastInputShare = 0.0f;
		mpRoomScaleSmoothedRun = 0.0f;
		mpRoomScaleSmoothedStrafe = 0.0f;
		mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		ResetMPRoomScaleCommandHistory();
		bMPRoomScaleCorrectionActive = false;
		mpRoomScaleActivationHoldSeconds = 0.0f;
		bMPRoomScaleHasTrackingSample = false;
		bIgnoreNextRoomScaleMovement = true;
		return;
	}

	const bool bUseMPNetworkServo = ShouldUseMPRoomScaleNetworkServo();
	if (bUseMPNetworkServo)
	{
		AdvanceMPRoomScaleCommandClock();
	}

	// This path normally runs after FireWeapon has returned, but keep it inert if a
	// weapon implementation re-enters HandleInputs while the biped is at the muzzle.
	// InputHandler has already written the real controller axes, so returning here
	// preserves user locomotion without authoring a false roomscale correction.
	if (bUseMPNetworkServo && bMPRoomScaleTemporaryFireRelocation)
	{
		return;
	}

	// Joined clients cannot authoritatively teleport their biped. Preserve normal
	// 1:1 HMD translation, then gently drive the body toward the remaining physical
	// offset through Halo's real Controls buffer. The network-tick path cancels only
	// that catch-up motion from the view, keeping the rendered world stationary.
	if (bUseMPNetworkServo)
	{
		const bool bInCutscene = Helpers::GetCutsceneData()->bInCutscene;
		if (bInCutscene || bInVehicle)
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			bMPRoomScaleHasAccumulatedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;
			bIgnoreNextRoomScaleMovement = true;
			return;
		}

		HaloID mpRoomScaleTargetID{ 0xFFFF, 0xFFFF };
		UnitDynamicObject* target = nullptr;

		if (Helpers::GetLocalPlayerID(mpRoomScaleTargetID))
		{
			BaseDynamicObject* localObject = Helpers::GetDynamicObject(mpRoomScaleTargetID);
			if (localObject && localObject->N0000027E == ObjectType::BIPED)
			{
				target = static_cast<UnitDynamicObject*>(localObject);
			}
		}

		if (!target)
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			bMPRoomScaleHasAccumulatedOffset = false;
			bMPRoomScaleHasAppliedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;
			bIgnoreNextRoomScaleMovement = true;
			return;
		}

		// Let the network hook finish transition initialization before accepting a
		// new physical displacement for this biped.
		if (bIgnoreNextRoomScaleMovement)
		{
			bMPRoomScaleHasAccumulatedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;
			return;
		}

		Vector3 headPos = vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);
		Vector3 headWorldOffset = headPos * MetresToWorld(1.0f);
		headWorldOffset.z = 0.0f;

		auto ClampedSetting = [](FloatProperty* property, float fallback,
			float minimum, float maximum)
		{
			float value = property ? property->Value() : fallback;
			if (!std::isfinite(value)) value = fallback;
			if (value < minimum) value = minimum;
			if (value > maximum) value = maximum;
			return value;
		};

		float activationDistanceMetres = ClampedSetting(
				c_MPRoomScaleActivationDistance, 0.30f, 0.05f, 2.0f);
		const float activationHoldRequiredSeconds = ClampedSetting(
				c_MPRoomScaleActivationDelay, 0.35f, 0.0f, 2.0f);
		float settleDistanceMetres = ClampedSetting(
				c_MPRoomScaleSettleDistance, 0.12f, 0.01f, 1.0f);
		float fullInputDistanceMetres = ClampedSetting(
				c_MPRoomScaleFullSpeedDistance, 1.20f, 0.15f, 4.0f);
		float maximumTrackingDistanceMetres = ClampedSetting(
				c_MPRoomScaleMaximumTrackingDistance, 8.0f, 1.0f, 20.0f);

		// Preserve hysteresis even if a user enters contradictory values in the
		// text config or in the all-settings page.
		const float maximumSettleDistance = activationDistanceMetres * 0.8f;
		if (settleDistanceMetres > maximumSettleDistance)
		{
			settleDistanceMetres = maximumSettleDistance;
		}
		if (fullInputDistanceMetres < activationDistanceMetres + 0.10f)
		{
			fullInputDistanceMetres = activationDistanceMetres + 0.10f;
		}
		if (maximumTrackingDistanceMetres < fullInputDistanceMetres + 0.50f)
		{
			maximumTrackingDistanceMetres = fullInputDistanceMetres + 0.50f;
		}

		const float activationDistance = MetresToWorld(activationDistanceMetres);
		const float settleDistance = MetresToWorld(settleDistanceMetres);
		const float fullInputDistance = MetresToWorld(fullInputDistanceMetres);
		const float maximumTrackingDistance =
			MetresToWorld(maximumTrackingDistanceMetres);

		if (!std::isfinite(headWorldOffset.x) ||
			!std::isfinite(headWorldOffset.y) ||
			!std::isfinite(headWorldOffset.z))
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;
			return;
		}

		// Detect tracking-origin discontinuities by frame-to-frame change. Gradual
		// walking can continue through the configurable large-play-space range, while
		// a discontinuity is absorbed into OpenVR's location offset once.
		float trackingSampleDelta = 0.0f;
		if (bMPRoomScaleHasTrackingSample)
		{
			trackingSampleDelta =
				(headWorldOffset - mpRoomScaleLastTrackingSample).length();
		}
		mpRoomScaleLastTrackingSample = headWorldOffset;
		const bool bHadTrackingSample = bMPRoomScaleHasTrackingSample;
		bMPRoomScaleHasTrackingSample = true;

		const float suddenTrackingJumpDistance = MetresToWorld(0.75f);
		const bool bSuddenTrackingJump = bHadTrackingSample &&
			trackingSampleDelta > suddenTrackingJumpDistance;
		const bool bBeyondConfiguredTrackingRange =
			headWorldOffset.length() > maximumTrackingDistance;

		if (bSuddenTrackingJump || bBeyondConfiguredTrackingRange)
		{
			Vector3 trackingOffset = headWorldOffset;
			const float rotation = vr->GetYawOffset() * (3.141593f / 180.0f);
			const float cosAngle = std::cos(rotation);
			const float sinAngle = std::sin(rotation);
			const float newX = trackingOffset.x * cosAngle -
				trackingOffset.y * sinAngle;
			const float newY = trackingOffset.x * sinAngle +
				trackingOffset.y * cosAngle;
			trackingOffset.x = newX;
			trackingOffset.y = newY;
			vr->SetLocationOffset(trackingOffset * WorldToMetres(1.0f) +
				vr->GetLocationOffset());

			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			bMPRoomScaleHasAccumulatedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			bMPRoomScaleHasTrackingSample = false;

			return;
		}

		// InputHandler has already written the controller state. Off gives the stick
		// complete priority. On tracks pending roomscale movement in world space, but
		// an occupied Halo command axis remains unchanged.
		float baseRun = 0.0f;
		float baseStrafe = 0.0f;
		if (bIsCustom)
		{
			Controls_Custom& controls = Helpers::GetControlsCustom();
			baseRun = controls.Forwards;
			baseStrafe = controls.Left;
		}
		else
		{
			Controls& controls = Helpers::GetControls();
			baseRun = controls.Forwards;
			baseStrafe = controls.Left;
		}

		constexpr float manualInputPriorityThreshold = 0.05f;
		const bool bManualRunActive =
			std::abs(baseRun) > manualInputPriorityThreshold;
		const bool bManualStrafeActive =
			std::abs(baseStrafe) > manualInputPriorityThreshold;
		const bool bManualMovementActive =
			bManualRunActive || bManualStrafeActive;
		const bool bAllowRoomScaleDuringStick =
			c_MPRoomScaleWhileMoving && c_MPRoomScaleWhileMoving->Value();
		if (bManualMovementActive && !bAllowRoomScaleDuringStick)
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			mpRoomScaleLastAppliedOffset = Vector3(0.0f, 0.0f, 0.0f);
			mpRoomScaleLastResolvedID = mpRoomScaleTargetID;
			bMPRoomScaleHasAccumulatedOffset = false;
			mpRoomScaleLastInputShare = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
			ResetMPRoomScaleCommandHistory();
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
			target->run = baseRun;
			target->strafe = baseStrafe;

			return;
		}

		// This is the remaining absolute head/body separation, not a per-frame
		// delta. Do not accumulate it: a physical step remains immediate and 1:1.
		mpRoomScaleAccumulatedOffset = headWorldOffset;

		// Controls.Forwards/Left on a joined client are interpreted in Halo's current
		// network-aim basis. InputHandler counter-rotates manual locomotion into this
		// basis, so roomscale must use the same basis to preserve world direction.
		Vector3 basisPosition{};
		Vector3 forward{};
		Vector3 basisUp{};
		const bool bHasWeaponCommandBasis =
			weaponHandler.GetWorldWeaponAim(basisPosition, forward, basisUp);
		forward.z = 0.0f;
		if (!bHasWeaponCommandBasis || forward.lengthSqr() < 0.000001f)
		{
			forward = target->facing;
			forward.z = 0.0f;
		}
		if (forward.lengthSqr() < 0.000001f)
		{
			forward = Helpers::GetCamera().lookDir;
			forward.z = 0.0f;
		}
		if (forward.lengthSqr() < 0.000001f)
		{
			forward = Vector3(1.0f, 0.0f, 0.0f);
		}
		forward.normalize();
		const Vector3 left(-forward.y, forward.x, 0.0f);

		// Keep ordinary head/crouch sway out of body follow with a configurable
		// comfort gate. Only avatar input is delayed; headset translation remains
		// immediate and one-to-one.
		float pendingDistance = mpRoomScaleAccumulatedOffset.length();

		if (!bMPRoomScaleCorrectionActive)
		{
			if (pendingDistance > activationDistance)
			{
				float activationSampleSeconds = 0.0f;
				if (std::isfinite(lastDeltaTime) && lastDeltaTime > 0.0f)
				{
					activationSampleSeconds = lastDeltaTime;
					if (activationSampleSeconds > 0.05f)
					{
						activationSampleSeconds = 0.05f;
					}
				}
				mpRoomScaleActivationHoldSeconds += activationSampleSeconds;

				if (mpRoomScaleActivationHoldSeconds >= activationHoldRequiredSeconds)
				{
					bMPRoomScaleCorrectionActive = true;
					mpRoomScaleActivationHoldSeconds = 0.0f;
				}
			}
			else
			{
				mpRoomScaleActivationHoldSeconds = 0.0f;
			}
		}
		else if (bMPRoomScaleCorrectionActive && pendingDistance <= settleDistance)
		{
			bMPRoomScaleCorrectionActive = false;
			mpRoomScaleActivationHoldSeconds = 0.0f;
		}

		if (!bMPRoomScaleCorrectionActive)
		{
			mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
			pendingDistance = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
		}

		float targetRoomRun = 0.0f;
		float targetRoomStrafe = 0.0f;
		Vector3 targetRoomWorld(0.0f, 0.0f, 0.0f);

		if (bMPRoomScaleCorrectionActive && pendingDistance > settleDistance)
		{
			float strength = (pendingDistance - settleDistance) /
				(fullInputDistance - settleDistance);
			if (strength > 1.0f)
			{
				strength = 1.0f;
			}

			const Vector3 movementDirection = mpRoomScaleAccumulatedOffset / pendingDistance;
			targetRoomWorld = movementDirection * strength;
			targetRoomRun = targetRoomWorld.dot(forward);
			targetRoomStrafe = targetRoomWorld.dot(left);
		}

		// Smooth command changes without filtering the headset. Only the avatar's
		// catch-up input is damped; physical head motion remains completely direct.
		// Keep correction direction in world space so turning the VR gun cannot
		// rotate a steady physical step inside the smoothing state.
		constexpr float inputSmoothing = 0.10f;
		mpRoomScaleSmoothedRun +=
			(targetRoomRun - mpRoomScaleSmoothedRun) * inputSmoothing;
		mpRoomScaleSmoothedStrafe +=
			(targetRoomStrafe - mpRoomScaleSmoothedStrafe) * inputSmoothing;
		mpRoomScaleSmoothedWorldInput +=
			(targetRoomWorld - mpRoomScaleSmoothedWorldInput) * inputSmoothing;
		const float localBasisRoomRun = mpRoomScaleSmoothedRun;
		const float localBasisRoomStrafe = mpRoomScaleSmoothedStrafe;
		const bool bUseWorldSpaceCandidate =
			bAllowRoomScaleDuringStick && bManualMovementActive;
		const float worldCandidateRoomRun =
			mpRoomScaleSmoothedWorldInput.dot(forward);
		const float worldCandidateRoomStrafe =
			mpRoomScaleSmoothedWorldInput.dot(left);
		const float candidateRoomRun = bUseWorldSpaceCandidate
			? worldCandidateRoomRun : localBasisRoomRun;
		const float candidateRoomStrafe = bUseWorldSpaceCandidate
			? worldCandidateRoomStrafe : localBasisRoomStrafe;

		float authoredRoomRun = candidateRoomRun;
		float authoredRoomStrafe = candidateRoomStrafe;
		Vector3 manualWorldDirection = mpRoomScaleManualWorldInput;
		manualWorldDirection.z = 0.0f;
		const float manualWorldMagnitude = manualWorldDirection.length();
		const Vector3 baseWorldCommand =
			forward * baseRun + left * baseStrafe;
		const float rawManualAlignment =
			manualWorldMagnitude > manualInputPriorityThreshold
			? baseWorldCommand.dot(manualWorldDirection / manualWorldMagnitude)
			: 0.0f;
		const bool bCausalStickTransport =
			bAllowRoomScaleDuringStick && bManualMovementActive &&
			manualWorldMagnitude > manualInputPriorityThreshold &&
			rawManualAlignment > 0.20f;
		const bool bProtectManualRun =
			bAllowRoomScaleDuringStick && bManualRunActive;
		const bool bProtectManualStrafe =
			bAllowRoomScaleDuringStick && bManualStrafeActive;
		if (bCausalStickTransport)
		{
			manualWorldDirection /= manualWorldMagnitude;
			if (MPRoomScaleManualDirectionChanged(
				mpRoomScaleTargetID, manualWorldDirection))
			{
				ResetMPRoomScaleCommandHistory();
			}
		}

		// Preserving only the stick's scalar projection can still weaken one component
		// of a diagonal. Keep every occupied digital axis immutable; roomscale may use
		// only a genuinely idle Halo command axis.
		if (bProtectManualRun)
		{
			authoredRoomRun = 0.0f;
			mpRoomScaleSmoothedRun = 0.0f;
			mpRoomScaleSmoothedWorldInput -=
				forward * mpRoomScaleSmoothedWorldInput.dot(forward);
		}
		if (bProtectManualStrafe)
		{
			authoredRoomStrafe = 0.0f;
			mpRoomScaleSmoothedStrafe = 0.0f;
			mpRoomScaleSmoothedWorldInput -=
				left * mpRoomScaleSmoothedWorldInput.dot(left);
		}

		auto ClampControlAxis = [](float value)
		{
			if (value > 1.0f) return 1.0f;
			if (value < -1.0f) return -1.0f;
			return value;
		};

		const float combinedRun =
			ClampControlAxis(baseRun + authoredRoomRun);
		const float combinedStrafe =
			ClampControlAxis(baseStrafe + authoredRoomStrafe);

		if (bIsCustom)
		{
			Controls_Custom& controls = Helpers::GetControlsCustom();
			controls.Forwards = combinedRun;
			controls.Left = combinedStrafe;
		}
		else
		{
			Controls& controls = Helpers::GetControls();
			controls.Forwards = combinedRun;
			controls.Left = combinedStrafe;
		}

		// Mirror the command into the resolved predicted biped as well. The Controls
		// buffer is what makes the command real; these fields reduce one-frame visual
		// lag where Halo has already instantiated the local prediction object.
		target->run = combinedRun;
		target->strafe = combinedStrafe;

		const float effectiveRoomRun = combinedRun - baseRun;
		const float effectiveRoomStrafe = combinedStrafe - baseStrafe;
		const float effectiveRoomMagnitude = std::sqrt(
			effectiveRoomRun * effectiveRoomRun +
			effectiveRoomStrafe * effectiveRoomStrafe);
		const bool bRoomScaleInputActive = effectiveRoomMagnitude > 0.0001f;
		float roomScaleInputShare = 0.0f;

		// Use causal decomposition only when a safe roomscale contribution survived
		// manual-axis protection. Stick-idle and raw-input fallback frames retain
		// scalar positive-only acknowledgement.
		if (bCausalStickTransport && bRoomScaleInputActive)
		{
			roomScaleInputShare = 1.0f;
		}
		else if (bRoomScaleInputActive && effectiveRoomMagnitude > 0.0001f)
		{
			const float roomInputDirRun = effectiveRoomRun / effectiveRoomMagnitude;
			const float roomInputDirStrafe = effectiveRoomStrafe / effectiveRoomMagnitude;
			const float baseAlongRequest = baseRun * roomInputDirRun +
				baseStrafe * roomInputDirStrafe;
			const float combinedAlongRequest = combinedRun * roomInputDirRun +
				combinedStrafe * roomInputDirStrafe;
			const float roomAuthoredAlongRequest =
				combinedAlongRequest - baseAlongRequest;

			if (combinedAlongRequest > 0.0001f &&
				roomAuthoredAlongRequest > 0.0001f)
			{
				roomScaleInputShare =
					roomAuthoredAlongRequest / combinedAlongRequest;
				if (roomScaleInputShare > 1.0f)
				{
					roomScaleInputShare = 1.0f;
				}
			}
		}

		const Vector3 effectiveRoomWorld =
			forward * effectiveRoomRun + left * effectiveRoomStrafe;
		const Vector3 combinedWorldCommand =
			forward * combinedRun + left * combinedStrafe;
		mpRoomScaleLastAppliedOffset = effectiveRoomWorld;
		mpRoomScaleLastResolvedID = mpRoomScaleTargetID;
		bMPRoomScaleHasAccumulatedOffset = bRoomScaleInputActive;
		mpRoomScaleLastInputShare = roomScaleInputShare;
		if (bCausalStickTransport && bRoomScaleInputActive)
		{
			RecordMPRoomScaleCommand(mpRoomScaleTargetID,
				manualWorldDirection, baseWorldCommand,
				effectiveRoomWorld,
				combinedWorldCommand);
		}
		else
		{
			// A frame that did not record a fresh protected-axis contribution must never
			// be explained later by older roomscale history. This makes left ->
			// forward-left and stick-release transitions take effect immediately while
			// the existing positive-only path handles the current request.
			ResetMPRoomScaleCommandHistory();
		}

		return;
	}

	// Single-player and the multiplayer host retain the original roomscale path
	// exactly: move the local biped and then recenter the OpenVR location offset.
	mpRoomScaleAccumulatedOffset = Vector3(0.0f, 0.0f, 0.0f);
	mpRoomScaleLastAppliedOffset = Vector3(0.0f, 0.0f, 0.0f);
	mpRoomScaleLastAppliedPosition = Vector3(0.0f, 0.0f, 0.0f);
	mpRoomScaleLastAppliedID = HaloID{ 0xFFFF, 0xFFFF };
	mpRoomScaleLastResolvedID = HaloID{ 0xFFFF, 0xFFFF };
	bMPRoomScaleHasAccumulatedOffset = false;
	bMPRoomScaleHasAppliedOffset = false;
	mpRoomScaleLastInputShare = 0.0f;
	mpRoomScaleSmoothedRun = 0.0f;
	mpRoomScaleSmoothedStrafe = 0.0f;
	mpRoomScaleSmoothedWorldInput = Vector3(0.0f, 0.0f, 0.0f);
	ResetMPRoomScaleCommandHistory();
	bMPRoomScaleCorrectionActive = false;
	mpRoomScaleActivationHoldSeconds = 0.0f;
	bMPRoomScaleHasTrackingSample = false;

	UnitDynamicObject* Player = static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());

	const bool bNoPlayer = Player == nullptr;
	const bool bInCutscene = Helpers::GetCutsceneData()->bInCutscene;

	// Restrict roomscale movement to normal movement (there's probably an elegant way to determine if the player can move, but I haven't RE'd it)
	if (bNoPlayer || bInCutscene || bInVehicle)
	{
		bIgnoreNextRoomScaleMovement = true;
		return;
	}

	// Calculate real-world offset from last frame
	Vector3 headPos = vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);

	// Convert to game units
	Vector3 desiredOffset = headPos * MetresToWorld(1.0f);
	desiredOffset.z = 0.0f;

	const float Rotation = vr->GetYawOffset() * (3.141593f / 180.0f);

#if 0
	inGameRenderer.DrawPolygon(Player->position + desiredOffset, Vector3(0.0f, 0.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), 4, MetresToWorld(0.25f), D3DCOLOR_ARGB(50, 85, 250, 239), false);
#endif

	// Directly adjust position, collisions are handled later in the tick
	if (!bIgnoreNextRoomScaleMovement)
	{
		Player->position += desiredOffset;
	}
	bIgnoreNextRoomScaleMovement = false;

	{
		// Rotate desiredOffset by yaw offset
		const float cosAngle = std::cos(Rotation);
		const float sinAngle = std::sin(Rotation);
		const float newX = desiredOffset.x * cosAngle - desiredOffset.y * sinAngle;
		const float newY = desiredOffset.x * sinAngle + desiredOffset.y * cosAngle;
		desiredOffset.x = newX;
		desiredOffset.y = newY;
	}

	// Move the camera offset backwards to recentre the player
	vr->SetLocationOffset(desiredOffset * WorldToMetres(1.0f) + vr->GetLocationOffset());
}

bool Game::GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand)
{
	VR_PROFILE_SCOPE(Game_GetCalculatedHandPositions);
	return inputHandler.GetCalculatedHandPositions(controllerTransform, dominantHandPos, offHand);
}

void Game::ReloadStart(HaloID param1, short param2, bool param3)
{
	VR_PROFILE_SCOPE(Game_ReloadStart);

	WeaponDynamicObject* weaponObject = static_cast<WeaponDynamicObject*>((Helpers::GetDynamicObject(param1)));

	HaloID playerID{};
	Helpers::GetLocalPlayerID(playerID);

	if (!weaponObject || weaponObject->parent != playerID)
	{
		return;
	}

	Weapon& weapon = weaponObject->weaponData[param2];

	// Reload function gets called whenever the player tries to reload, if reloadstate is 1 then a reload was actually triggered
	if (weapon.reloadState == 1)
	{
		bIsReloading = true;
	}
}

void Game::ReloadEnd(short param1, HaloID param2)
{
	VR_PROFILE_SCOPE(Game_ReloadEnd);

	WeaponDynamicObject* weaponObject = static_cast<WeaponDynamicObject*>((Helpers::GetDynamicObject(param2)));

	HaloID playerID{};
	Helpers::GetLocalPlayerID(playerID);

	if (!weaponObject || weaponObject->parent != playerID)
	{
		return;
	}

	bIsReloading = false;
}

Vector3 Game::GetSmoothedInput() const
{
	return inputHandler.smoothedPosition;
}

bool Game::GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	return weaponHandler.GetWorldWeaponAim(outPosition, outAim, upDir);
}

bool Game::GetVehicleVRAim(
	Vector3& outPosition,
	Vector3& outAim,
	Vector3& outUp) const
{
	if (!c_MotionControlledVehicles ||
		!c_MotionControlledVehicles->Value() ||
		!vr)
	{
		return false;
	}

	const bool bDisableWarthogSteeringMotionControls =
		c_DisableWarthogSteeringMotionControls &&
		c_DisableWarthogSteeringMotionControls->Value();

	if (bDisableWarthogSteeringMotionControls)
	{
		UnitDynamicObject* player =
			static_cast<UnitDynamicObject*>(
				Helpers::GetLocalPlayer());

		if (player &&
			player->parent.id != 0xFFFF &&
			player->parent.index != 0xFFFF)
		{
			BaseDynamicObject* vehicle =
				Helpers::GetDynamicObject(
					player->parent);

			if (vehicle &&
				vehicle->N0000027E ==
					ObjectType::VEHICLE)
			{
				struct TagPathView
				{
					char pad_0000[16];
					const char* path;
				};

				Asset_Generic* vehicleTag =
					Helpers::GetTypedAsset<
						Asset_Generic>(
							vehicle->tagID);
				const TagPathView* tagPath =
					reinterpret_cast<
						const TagPathView*>(
							vehicleTag);

				const bool bIsWarthog =
					tagPath &&
					tagPath->path &&
					std::strstr(
						tagPath->path,
						"warthog") != nullptr;
				const uint32_t normalizedSeatIndex =
					player->parentSeatIndex &
					0xFFu;
				const bool bHasLocalWeapon =
					player->weapon.id != 0xFFFF &&
					player->weapon.index !=
						0xFFFF;
				const bool
					bIsConfirmedWarthogDriver =
						bIsWarthog &&
						normalizedSeatIndex == 0 &&
						!bHasLocalWeapon;

				if (bIsConfirmedWarthogDriver)
				{
					// Returning false makes both the authoritative
					// vehicle aim and floating crosshair use their
					// finalized native right-stick fallback.
					return false;
				}
			}
		}
	}

	const ControllerRole dominantRole =
		bLeftHanded
			? ControllerRole::Left
			: ControllerRole::Right;

	Vector3 rawAim;
	if (!vr->TryGetControllerFacing(dominantRole, rawAim) ||
		!std::isfinite(rawAim.x) ||
		!std::isfinite(rawAim.y) ||
		!std::isfinite(rawAim.z) ||
		rawAim.lengthSqr() <= 0.000001f)
	{
		return false;
	}

	rawAim.normalize();

	// Use LivingFray's existing scoped-weapon smoothing response curve, but keep
	// independent vehicle settings. This shared result drives both Halo's
	// vehicle aim controller and the floating vehicle crosshair.
	static bool bSmoothingInitialized = false;
	static bool bLastYawInitialized = false;
	static Vector3 smoothedAim{};
	static float lastSmoothingYawOffset = 0.0f;

	const bool bSmoothingEnabled =
		c_SmoothVehicleVRAim &&
		c_SmoothVehicleVRAim->Value();
	float smoothingAmount =
		c_VehicleVRAimSmoothingAmount
			? c_VehicleVRAimSmoothingAmount->Value()
			: 0.0f;
	smoothingAmount =
		std::clamp(smoothingAmount, 0.0f, 2.0f);

	if (!bSmoothingEnabled ||
		smoothingAmount == 0.0f)
	{
		smoothedAim = rawAim;
		bSmoothingInitialized = true;
		bLastYawInitialized = false;
	}
	else
	{
		const float currentYawOffset =
			vr->GetYawOffset();

		if (!bSmoothingInitialized)
		{
			smoothedAim = rawAim;
			bSmoothingInitialized = true;
		}

		if (!bLastYawInitialized)
		{
			lastSmoothingYawOffset =
				currentYawOffset;
			bLastYawInitialized = true;
		}

		// Match LivingFray's snap-turn handling so the stored direction rotates
		// with the VR yaw offset instead of visibly trailing across a turn.
		const float yawDelta =
			currentYawOffset -
			lastSmoothingYawOffset;
		Helpers::RotateForSnapTurn(
			smoothedAim,
			yawDelta,
			c_SnapTurnAmount->Value());
		lastSmoothingYawOffset =
			currentYawOffset;

		const float scaleFactor =
			-20.0f / 9.0f;
		const float h =
			90.0f *
			log2(
				1.0f -
				exp(
					smoothingAmount *
					scaleFactor));

		float frameSeconds =
			lastDeltaTime;
		if (!std::isfinite(frameSeconds) ||
			frameSeconds <= 0.0f)
		{
			frameSeconds = 1.0f / 90.0f;
		}
		else if (frameSeconds > 0.05f)
		{
			frameSeconds = 0.05f;
		}

		const float t =
			1.0f -
			pow(
				2.0f,
				frameSeconds * h);

		smoothedAim =
			Helpers::Lerp(
				smoothedAim,
				rawAim,
				t);

		if (!std::isfinite(smoothedAim.x) ||
			!std::isfinite(smoothedAim.y) ||
			!std::isfinite(smoothedAim.z) ||
			smoothedAim.lengthSqr() <= 0.000001f)
		{
			smoothedAim = rawAim;
		}
		else
		{
			smoothedAim.normalize();
		}
	}

	outAim = smoothedAim;

	const Matrix4 controllerTransform =
		vr->GetControllerTransform(dominantRole, true);
	outPosition =
		controllerTransform * Vector3(0.0f, 0.0f, 0.0f);
	outUp =
		controllerTransform.getUpAxis();

	if (!std::isfinite(outPosition.x) ||
		!std::isfinite(outPosition.y) ||
		!std::isfinite(outPosition.z))
	{
		outPosition = Vector3();
	}

	if (!std::isfinite(outUp.x) ||
		!std::isfinite(outUp.y) ||
		!std::isfinite(outUp.z) ||
		outUp.lengthSqr() <= 0.000001f)
	{
		outUp = Vector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		outUp.normalize();
	}

	return true;
}

bool Game::IsLocalPlayerOperatingMountedVehicleTurret() const
{
	UnitDynamicObject* player =
		static_cast<UnitDynamicObject*>(
			Helpers::GetLocalPlayer());
	if (!player ||
		player->parent.id == 0xFFFF ||
		player->parent.index == 0xFFFF)
	{
		return false;
	}

	BaseDynamicObject* vehicle =
		Helpers::GetDynamicObject(
			player->parent);
	if (!vehicle ||
		vehicle->N0000027E != ObjectType::VEHICLE)
	{
		return false;
	}

	Asset_Generic* vehicleTag =
		Helpers::GetTypedAsset<Asset_Generic>(
			vehicle->tagID);
	const VehicleTagPathViewLocal* tagPath =
		reinterpret_cast<
			const VehicleTagPathViewLocal*>(
				vehicleTag);
	if (!tagPath || !tagPath->path)
	{
		return false;
	}

	const uint32_t normalizedSeatIndex =
		player->parentSeatIndex & 0xFFu;
	const bool bWarthogGunner =
		std::strstr(
			tagPath->path,
			"warthog") != nullptr &&
		normalizedSeatIndex == 1;
	const bool bCovenantGroundTurret =
		IsCovenantGroundTurretTagPathLocal(
			tagPath->path);

	return bWarthogGunner ||
		bCovenantGroundTurret;
}

bool Game::IsJoinedMultiplayerClient() const
{
	// Restrict locomotion-basis compensation to joined clients; host and
	// single-player movement use their existing paths.
	return GetNetworkServerType() == 1;
}

bool Game::IsMultiplayerSession() const
{
	// Halo exposes the network role even when the host is alone and the player
	// table still contains only one entry.
	constexpr uint16_t ServerJoinedClient = 1;
	constexpr uint16_t ServerLocalHost = 2;

	const uint16_t serverType = GetNetworkServerType();
	if (serverType == ServerJoinedClient || serverType == ServerLocalHost)
	{
		return true;
	}

	// Use the player table as a fallback during network-role transitions.
	return IsMPPlayerTable();
}

void Game::UpdateSinglePlayerMeleeAim(bool meleePressed)
{
	// Disabling the setting restores HaloCEVR's original HMD-directed campaign
	// melee. The default remains enabled for weapon-directed melee.
	if (!c_SinglePlayerMeleeWeaponAim ||
		!c_SinglePlayerMeleeWeaponAim->Value())
	{
		singlePlayerMeleeAimRemaining = 0.0f;
		return;
	}

	// Multiplayer already drives Halo's authoritative aim from the controller.
	// Add the behavior only to on-foot, non-network gameplay.
	CutsceneData* cutscene = Helpers::GetCutsceneData();
	if (IsMultiplayerSession() || bInVehicle ||
		(cutscene && cutscene->bInCutscene))
	{
		singlePlayerMeleeAimRemaining = 0.0f;
		return;
	}

	if (meleePressed)
	{
		// Halo performs the damaging trace after melee input starts. Retain the
		// weapon direction through the useful portion of the animation.
		singlePlayerMeleeAimRemaining = 0.50f;
		return;
	}

	float frameSeconds = lastDeltaTime;
	if (!std::isfinite(frameSeconds) || frameSeconds < 0.0f)
	{
		frameSeconds = 0.0f;
	}
	else if (frameSeconds > 0.10f)
	{
		frameSeconds = 0.10f;
	}

	singlePlayerMeleeAimRemaining -= frameSeconds;
	if (singlePlayerMeleeAimRemaining < 0.0f)
	{
		singlePlayerMeleeAimRemaining = 0.0f;
	}
}

bool Game::GetSinglePlayerMeleeAim(Vector3& outAim) const
{
	if (singlePlayerMeleeAimRemaining <= 0.0f ||
		IsMultiplayerSession() || bInVehicle)
	{
		return false;
	}

	Vector3 weaponPosition;
	Vector3 weaponUp;
	if (!GetWorldWeaponAim(weaponPosition, outAim, weaponUp) ||
		!std::isfinite(outAim.x) ||
		!std::isfinite(outAim.y) ||
		!std::isfinite(outAim.z) ||
		outAim.lengthSqr() <= 0.00000001f)
	{
		return false;
	}

	outAim.normalize();
	return true;
}

BoolProperty* Game::GetCurrentFirstPersonVehicleCameraSetting() const
{
	CutsceneData* cutscene = Helpers::GetCutsceneData();
	if (cutscene && cutscene->bInCutscene)
	{
		return nullptr;
	}

	UnitDynamicObject* player =
		static_cast<UnitDynamicObject*>(
			Helpers::GetLocalPlayer());
	if (!player ||
		player->parent.id == 0xFFFF ||
		player->parent.index == 0xFFFF)
	{
		return nullptr;
	}

	BaseDynamicObject* vehicle =
		Helpers::GetDynamicObject(
			player->parent);
	if (!vehicle ||
		vehicle->N0000027E != ObjectType::VEHICLE)
	{
		return nullptr;
	}

	Asset_Generic* vehicleTag =
		Helpers::GetTypedAsset<Asset_Generic>(
			vehicle->tagID);
	const VehicleTagPathViewLocal* tagPath =
		reinterpret_cast<
			const VehicleTagPathViewLocal*>(
				vehicleTag);
	if (!tagPath || !tagPath->path)
	{
		return nullptr;
	}

	const bool bIsWarthog =
		std::strstr(
			tagPath->path,
			"warthog") != nullptr;
	const bool bIsGhost =
		std::strstr(
			tagPath->path,
			"ghost") != nullptr;
	const bool bIsScorpion =
		std::strstr(
			tagPath->path,
			"scorpion") != nullptr;
	const bool bIsBanshee =
		std::strstr(
			tagPath->path,
			"banshee") != nullptr;
	const bool bIsCovenantGroundTurret =
		IsCovenantGroundTurretTagPathLocal(
			tagPath->path);
	const uint32_t normalizedSeatIndex =
		player->parentSeatIndex & 0xFFu;
	const bool bHasLocalWeapon =
		player->weapon.id != 0xFFFF &&
		player->weapon.index != 0xFFFF;

	const bool bIsConfirmedWarthogDriver =
		bIsWarthog &&
		normalizedSeatIndex == 0 &&
		!bHasLocalWeapon;
	const bool bIsConfirmedWarthogGunner =
		bIsWarthog &&
		normalizedSeatIndex == 1;
	const bool bIsConfirmedScorpionDriver =
		bIsScorpion &&
		normalizedSeatIndex == 0;

	return bIsConfirmedWarthogGunner
		? c_FirstPersonWarthogTurret
		: bIsConfirmedWarthogDriver
			? c_FirstPersonWarthogDriver
			: bIsGhost
				? c_FirstPersonGhost
				: bIsConfirmedScorpionDriver
					? c_FirstPersonScorpion
					: bIsBanshee
						? c_FirstPersonBanshee
						: bIsCovenantGroundTurret
							? c_FirstPersonShade
							: nullptr;
}

bool Game::IsCurrentVehicleSeatUsingCustomFirstPersonCamera() const
{
	BoolProperty* firstPersonSetting =
		GetCurrentFirstPersonVehicleCameraSetting();
	return firstPersonSetting &&
		firstPersonSetting->Value();
}

bool Game::IsCurrentVehicleNativeHandheldPassengerView() const
{
	UnitDynamicObject* player =
		static_cast<UnitDynamicObject*>(
			Helpers::GetLocalPlayer());
	if (!player ||
		player->parent.id == 0xFFFF ||
		player->parent.index == 0xFFFF ||
		player->weapon.id == 0xFFFF ||
		player->weapon.index == 0xFFFF)
	{
		return false;
	}

	BaseDynamicObject* vehicle =
		Helpers::GetDynamicObject(
			player->parent);
	BaseDynamicObject* weapon =
		Helpers::GetDynamicObject(
			player->weapon);
	if (!vehicle ||
		vehicle->N0000027E != ObjectType::VEHICLE ||
		!weapon ||
		weapon->N0000027E != ObjectType::WEAPON)
	{
		return false;
	}

	// Vehicle-owned weapons are driver/turret aim contexts whose native view is
	// third person unless one of the custom first-person toggles above is active.
	// A weapon parented anywhere else is the handheld, native-first-person
	// passenger path used by seats such as the Warthog side seat.
	return !SameHaloIDLocal(
		weapon->parent,
		player->parent);
}

bool Game::IsCurrentVehicleViewFirstPerson() const
{
	return IsCurrentVehicleSeatUsingCustomFirstPersonCamera() ||
		IsCurrentVehicleNativeHandheldPassengerView();
}

void Game::BeginFirstPersonVehicleBodyMask(
	Renderer* renderer)
{
	EndFirstPersonVehicleBodyMask();

	if (!renderer)
	{
		return;
	}

	const bool bNativeHandheldPassenger =
		IsCurrentVehicleNativeHandheldPassengerView();
	const bool bCustomFirstPersonVehicleCamera =
		IsCurrentVehicleSeatUsingCustomFirstPersonCamera();
	const bool bHideHead =
		c_HideHeadInFirstPersonVehicles &&
		c_HideHeadInFirstPersonVehicles->Value() == 1;

	if (!bNativeHandheldPassenger &&
		(!bCustomFirstPersonVehicleCamera ||
			!bHideHead))
	{
		return;
	}

	UnitDynamicObject* player =
		static_cast<UnitDynamicObject*>(
			Helpers::GetLocalPlayer());
	if (!player ||
		player->N0000027E != ObjectType::BIPED)
	{
		return;
	}

	if (bNativeHandheldPassenger)
	{
		if (!std::isfinite(player->scale))
		{
			return;
		}

		firstPersonVehicleBodyScaleAddress =
			&player->scale;
		firstPersonVehicleSavedBodyScale =
			player->scale;
		player->scale = 0.0f;
		bFirstPersonVehicleBodyScaleMaskActive = true;
		return;
	}

	Asset_Generic* bipedTag =
		Helpers::GetTypedAsset<Asset_Generic>(
			player->tagID);
	if (!bipedTag)
	{
		return;
	}

	// Retail Halo 1.10 tag-index entries store the tag-data pointer at 0x14.
	// In a biped tag, the signed head model-node index is at 0x4E6. Dynamic
	// biped model nodes begin at 0x550 and each node is 0x34 bytes; the first
	// float is its render scale.
	constexpr std::size_t TagDataPointerOffset = 0x14;
	constexpr std::size_t BipedHeadNodeIndexOffset = 0x4E6;
	constexpr std::size_t BipedModelNodesOffset = 0x550;
	constexpr std::size_t ModelNodeStride = 0x34;
	constexpr int MaximumBipedModelNodes = 64;

	std::uint8_t* tagData =
		*reinterpret_cast<std::uint8_t**>(
			reinterpret_cast<std::uint8_t*>(
				bipedTag) +
			TagDataPointerOffset);
	if (!tagData)
	{
		return;
	}

	const std::int16_t headNodeIndex =
		*reinterpret_cast<const std::int16_t*>(
			tagData +
			BipedHeadNodeIndexOffset);
	if (headNodeIndex < 0 ||
		headNodeIndex >= MaximumBipedModelNodes)
	{
		return;
	}

	float* headScale =
		reinterpret_cast<float*>(
			reinterpret_cast<std::uint8_t*>(
				player) +
			BipedModelNodesOffset +
			static_cast<std::size_t>(
				headNodeIndex) *
				ModelNodeStride);
	if (!headScale ||
		!std::isfinite(*headScale))
	{
		return;
	}

	firstPersonVehicleHeadScaleAddress =
		headScale;
	firstPersonVehicleSavedHeadScale =
		*headScale;
	*headScale = 0.0f;
	bFirstPersonVehicleHeadMaskActive = true;
}

void Game::EndFirstPersonVehicleBodyMask()
{
	if (bFirstPersonVehicleHeadMaskActive &&
		firstPersonVehicleHeadScaleAddress)
	{
		*firstPersonVehicleHeadScaleAddress =
			firstPersonVehicleSavedHeadScale;
	}
	bFirstPersonVehicleHeadMaskActive = false;
	firstPersonVehicleHeadScaleAddress = nullptr;

	if (bFirstPersonVehicleBodyScaleMaskActive &&
		firstPersonVehicleBodyScaleAddress)
	{
		*firstPersonVehicleBodyScaleAddress =
			firstPersonVehicleSavedBodyScale;
	}
	bFirstPersonVehicleBodyScaleMaskActive = false;
	firstPersonVehicleBodyScaleAddress = nullptr;
}


void Game::ApplyFirstPersonVehicleDriverCamera()
{
	CutsceneData* cutscene = Helpers::GetCutsceneData();
	if (cutscene && cutscene->bInCutscene)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	UnitDynamicObject* player =
		static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());
	if (!player ||
		player->parent.id == 0xFFFF ||
		player->parent.index == 0xFFFF)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	BaseDynamicObject* vehicle =
		Helpers::GetDynamicObject(player->parent);
	if (!vehicle || vehicle->N0000027E != ObjectType::VEHICLE)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	struct TagPathView
	{
		char pad_0000[16];
		const char* path;
	};

	Asset_Generic* vehicleTag =
		Helpers::GetTypedAsset<Asset_Generic>(vehicle->tagID);
	const TagPathView* tagPath =
		reinterpret_cast<const TagPathView*>(vehicleTag);
	if (!tagPath || !tagPath->path)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	const bool bIsWarthog =
		std::strstr(tagPath->path, "warthog") != nullptr;
	const bool bIsGhost =
		std::strstr(tagPath->path, "ghost") != nullptr;
	const bool bIsScorpion =
		std::strstr(tagPath->path, "scorpion") != nullptr;
	const bool bIsBanshee =
		std::strstr(tagPath->path, "banshee") != nullptr;
	const bool bIsCovenantGroundTurret =
		IsCovenantGroundTurretTagPathLocal(
			tagPath->path);
	const uint32_t normalizedSeatIndex =
		player->parentSeatIndex & 0xFFu;

	const bool bHasLocalWeapon =
		player->weapon.id != 0xFFFF &&
		player->weapon.index != 0xFFFF;

	// Keep the side passenger distinct from both Warthog aiming seats.
	const bool bIsConfirmedWarthogDriver =
		bIsWarthog &&
		normalizedSeatIndex == 0 &&
		!bHasLocalWeapon;
	const bool bIsConfirmedWarthogGunner =
		bIsWarthog &&
		normalizedSeatIndex == 1;
	const bool bIsConfirmedScorpionDriver =
		bIsScorpion &&
		normalizedSeatIndex == 0;
	const bool bIsSupportedSingleSeatVehicle =
		bIsGhost ||
		bIsBanshee ||
		bIsCovenantGroundTurret;

	const bool bSupportedCameraContext =
		bIsConfirmedWarthogDriver ||
		bIsConfirmedWarthogGunner ||
		bIsConfirmedScorpionDriver ||
		bIsSupportedSingleSeatVehicle;

	if (!bSupportedCameraContext)
	{
		// This includes the Warthog side passenger, whose original camera and
		// handheld VR weapon behavior must remain untouched.
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	BoolProperty* cameraEnabled =
		bIsConfirmedWarthogGunner
			? c_FirstPersonWarthogTurret
			: bIsConfirmedWarthogDriver
				? c_FirstPersonWarthogDriver
				: bIsGhost
					? c_FirstPersonGhost
					: bIsConfirmedScorpionDriver
						? c_FirstPersonScorpion
						: bIsBanshee
							? c_FirstPersonBanshee
							: bIsCovenantGroundTurret
								? c_FirstPersonShade
								: nullptr;
	Vector3Property* cameraOffset =
		bIsConfirmedWarthogGunner
			? c_WarthogTurretOffset
			: bIsConfirmedWarthogDriver
				? c_WarthogDriverOffset
				: bIsGhost
					? c_GhostOffset
					: bIsConfirmedScorpionDriver
						? c_ScorpionOffset
						: bIsBanshee
							? c_BansheeOffset
							: bIsCovenantGroundTurret
								? c_ShadeOffset
								: nullptr;

	const uint32_t cameraContext =
		bIsConfirmedWarthogDriver
			? 1u
			: bIsConfirmedWarthogGunner
				? 2u
				: bIsGhost
					? 3u
					: bIsConfirmedScorpionDriver
						? 4u
						: bIsBanshee
							? 5u
							: bIsCovenantGroundTurret
								? 6u
								: 0u;

	const bool bFirstPersonRequested =
		cameraEnabled &&
		cameraOffset &&
		cameraEnabled->Value();

	// The locked first-person origin must never consume Halo's native camera
	// position or orientation. Both carry the controller-driven chase/orbit
	// motion used for vehicle aiming. Instead, interpolate only the vehicle body
	// snapshots using Halo's normalized tick time, freeze the settled seat
	// anchor in vehicle coordinates and retain the original vehicle-relative
	// first-person offsets.

	// Halo's native camera is still allowed to run underneath this render-only
	// override, so reticle, projectile and physical vehicle/model aim behavior
	// remains unchanged.
	if (!bFirstPersonRequested)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	Vector3 liveForward = vehicle->facingDir;
	Vector3 liveUp = vehicle->upDirection;
	if (!IsFiniteVectorLocal(vehicle->position) ||
		!IsFiniteVectorLocal(liveForward) ||
		!IsFiniteVectorLocal(liveUp) ||
		liveForward.lengthSqr() <= 0.000001f ||
		liveUp.lengthSqr() <= 0.000001f)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}

	liveForward.normalize();
	liveUp.normalize();

	Vector3 liveLeft =
		liveUp.cross(liveForward);
	if (!IsFiniteVectorLocal(liveLeft) ||
		liveLeft.lengthSqr() <= 0.000001f)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}
	liveLeft.normalize();
	liveUp =
		liveForward.cross(liveLeft);
	if (!IsFiniteVectorLocal(liveUp) ||
		liveUp.lengthSqr() <= 0.000001f)
	{
		ResetFirstPersonVehicleCameraTransformState();
		return;
	}
	liveUp.normalize();

	float frameSeconds = lastDeltaTime;
	if (!std::isfinite(frameSeconds) ||
		frameSeconds <= 0.0f)
	{
		// Do not invent a display refresh interval. A single invalid sample
		// simply contributes no time to entry/anchor-settle bookkeeping.
		frameSeconds = 0.0f;
	}
	else if (frameSeconds > 0.25f)
	{
		// Bound long stalls without assuming a headset refresh rate.
		frameSeconds = 0.25f;
	}

	FirstPersonVehicleCameraTransformState& state =
		firstPersonVehicleCameraTransformState;
	const bool bMultiplayerCamera =
		IsMultiplayerSession();
	const bool bUseNormalizedEngineTickTime =
		bRenderTickElapsedSecondsValid &&
		bRenderTickIntervalSecondsValid &&
		renderTickIntervalSeconds > 0.000001f;
	const bool bNewVehicleCameraContext =
		!state.active ||
		!SameHaloIDLocal(
			state.vehicleID,
			player->parent) ||
		state.cameraContext != cameraContext ||
		state.multiplayer != bMultiplayerCamera;

	if (bNewVehicleCameraContext)
	{
		state.active = true;
		state.multiplayer = bMultiplayerCamera;
		state.vehicleID = player->parent;
		state.cameraContext = cameraContext;
		state.contextSeconds = 0.0f;
		state.capturedSeatBodyLocalValid = false;
		state.capturedSeatBodyLocal =
			Vector3(
				0.0f,
				0.0f,
				0.0f);
		state.previousPosition = vehicle->position;
		state.currentPosition = vehicle->position;
		state.previousForward = liveForward;
		state.currentForward = liveForward;
		state.previousUp = liveUp;
		state.currentUp = liveUp;
		state.elapsedSeconds =
			VehicleReleasePolicy::SimulationTickSeconds;
		state.observedPosition = vehicle->position;
		state.observedForward = liveForward;
		state.observedUp = liveUp;
		state.deferredPoseValid = false;
		state.previousRenderPhaseValid =
			bUseNormalizedEngineTickTime;
		state.previousRenderPhaseSeconds =
			renderTickElapsedSeconds;
	}
	else
	{
		state.elapsedSeconds += frameSeconds;
		state.contextSeconds += frameSeconds;

		bool bEnginePhaseReset = false;
		if (bUseNormalizedEngineTickTime)
		{
			bEnginePhaseReset =
				state.previousRenderPhaseValid &&
				renderTickElapsedSeconds + 0.000001f <
					state.previousRenderPhaseSeconds;
			state.previousRenderPhaseSeconds =
				renderTickElapsedSeconds;
			state.previousRenderPhaseValid = true;
		}
		else
		{
			state.previousRenderPhaseValid = false;
		}

		const bool bNewObservedPose =
			(vehicle->position - state.observedPosition).lengthSqr() >
				0.0000000001f ||
			(liveForward - state.observedForward).lengthSqr() >
				0.0000000001f ||
			(liveUp - state.observedUp).lengthSqr() >
				0.0000000001f;

		if (bNewObservedPose)
		{
			state.observedPosition = vehicle->position;
			state.observedForward = liveForward;
			state.observedUp = liveUp;
		}

		const bool bLowRefreshRender =
			bUseNormalizedEngineTickTime &&
			frameSeconds >=
				renderTickIntervalSeconds * 0.75f;

		if (!bMultiplayerCamera ||
			!bUseNormalizedEngineTickTime)
		{
			// Campaign and the invalid-phase fallback consume changed poses
			// immediately.
			if (bNewObservedPose)
			{
				state.previousPosition = state.currentPosition;
				state.currentPosition = state.observedPosition;
				state.previousForward = state.currentForward;
				state.currentForward = state.observedForward;
				state.previousUp = state.currentUp;
				state.currentUp = state.observedUp;
				state.elapsedSeconds = 0.0f;
			}
		}
		else if (bEnginePhaseReset || bLowRefreshRender)
		{
			if (bNewObservedPose || state.deferredPoseValid)
			{
				state.previousPosition = state.currentPosition;
				state.currentPosition = state.observedPosition;
				state.previousForward = state.currentForward;
				state.currentForward = state.observedForward;
				state.previousUp = state.currentUp;
				state.currentUp = state.observedUp;
				state.elapsedSeconds = 0.0f;
				state.deferredPoseValid = false;
			}
			else if (bEnginePhaseReset)
			{
				// Do not replay the previous interpolation interval if a network
				// tick advances without delivering a new vehicle pose.
				state.previousPosition = state.currentPosition;
				state.previousForward = state.currentForward;
				state.previousUp = state.currentUp;
				state.elapsedSeconds = 0.0f;
			}
		}
		else if (bNewObservedPose)
		{
			// Apply off-phase multiplayer poses at the next engine phase reset.
			state.deferredPoseValid = true;
		}
	}

	float interpolationAmount = 1.0f;

	if (bUseNormalizedEngineTickTime)
	{
		interpolationAmount =
			renderTickElapsedSeconds /
			renderTickIntervalSeconds;
	}
	else
	{
		interpolationAmount =
			state.elapsedSeconds /
			VehicleReleasePolicy::SimulationTickSeconds;
	}
	if (interpolationAmount < 0.0f)
	{
		interpolationAmount = 0.0f;
	}
	else if (interpolationAmount > 1.0f)
	{
		interpolationAmount = 1.0f;
	}

	const Vector3 vehiclePosition =
		LerpVectorLocal(
			state.previousPosition,
			state.currentPosition,
			interpolationAmount);
	Vector3 forward =
		LerpVectorLocal(
			state.previousForward,
			state.currentForward,
			interpolationAmount);
	Vector3 up =
		LerpVectorLocal(
			state.previousUp,
			state.currentUp,
			interpolationAmount);

	if (!IsFiniteVectorLocal(vehiclePosition) ||
		!IsFiniteVectorLocal(forward) ||
		!IsFiniteVectorLocal(up) ||
		forward.lengthSqr() <= 0.000001f ||
		up.lengthSqr() <= 0.000001f)
	{
		return;
	}

	forward.normalize();
	up.normalize();

	Vector3 left = up.cross(forward);
	if (!IsFiniteVectorLocal(left) ||
		left.lengthSqr() <= 0.000001f)
	{
		return;
	}
	left.normalize();

	// Rebuild an orthonormal up vector after interpolating the vehicle axes.
	up = forward.cross(left);
	if (!IsFiniteVectorLocal(up) ||
		up.lengthSqr() <= 0.000001f)
	{
		return;
	}
	up.normalize();

	Vector3 seatedPlayerBodyLocal =
		player->position;

	static_assert(
		VehicleReleasePolicy::ModelAimActivationDelaySeconds ==
			VehicleReleasePolicy::FirstPersonCameraActivationDelaySeconds,
		"The model-aim and first-person camera delays must stay synchronized.");
	const float activationDelay =
		VehicleReleasePolicy::FirstPersonCameraActivationDelaySeconds;

	if (state.contextSeconds >= activationDelay &&
		!state.capturedSeatBodyLocalValid &&
		IsFiniteVectorLocal(player->position))
	{
		// Once vehicle entry has settled, retain the seat attachment point in
		// vehicle coordinates to remove the final simulation-tick body wobble.
		state.capturedSeatBodyLocal =
			player->position;
		state.capturedSeatBodyLocalValid = true;
	}

	if (state.capturedSeatBodyLocalValid)
	{
		seatedPlayerBodyLocal =
			state.capturedSeatBodyLocal;
	}

	const Vector3 vehicleLockedSeatAnchor =
		vehiclePosition +
		forward * seatedPlayerBodyLocal.x +
		left * seatedPlayerBodyLocal.y +
		up * seatedPlayerBodyLocal.z;

	const Vector3 offsetMetres =
		cameraOffset->Value();
	const Vector3 cameraPosition =
		vehicleLockedSeatAnchor +
		forward *
			MetresToWorld(offsetMetres.x) +
		left *
			MetresToWorld(offsetMetres.y) +
		up *
			MetresToWorld(offsetMetres.z);

	if (!IsFiniteVectorLocal(cameraPosition))
	{
		return;
	}

	// Change only the neutral render-camera origin. OpenVR still adds the
	// tracked HMD position and orientation separately for each eye.
	frustum1.position = cameraPosition;
	frustum2.position = cameraPosition;
}

void Game::UpdateCamera(float& yaw, float& pitch)
{
	VR_PROFILE_SCOPE(Game_UpdateCamera);
	// Don't bother simulating inputs if we aren't actually in vr
#ifdef EMULATE_VR
	return;
#endif

	CutsceneData* cutscene = Helpers::GetCutsceneData();
	const bool bInCutscene = cutscene && cutscene->bInCutscene;
	if (bInCutscene)
	{
		// Halo's scripted camera owns the base view. The HMD is still applied by the
		// per-eye renderer, but no VR yaw/pitch may feed back into the cinematic.
		bCameraWasInCutscene = true;
		bCameraCutsceneResumeGuardActive = false;
		bCameraWasInVehicle = bInVehicle;
		bCameraWasUsingDedicatedVehicleCamera =
			bInVehicle &&
			(!bHasWeapon ||
				IsLocalPlayerOperatingMountedVehicleTurret());
		bVehicleExitCameraGuardActive = false;
		vehicleExitCameraGuardElapsedSeconds = 0.0f;
		inputHandler.bArmedVehicleCameraSampleValid = false;
		return;
	}

	if (bCameraWasInCutscene)
	{
		// Leave the remainder of the frame in which a cinematic ends to Halo.
		bCameraWasInCutscene = false;
		bCameraCutsceneResumeGuardActive = true;
		cameraCutsceneResumeFrame = cameraFrameSerial;
		inputHandler.bArmedVehicleCameraSampleValid = false;
		return;
	}

	if (bCameraCutsceneResumeGuardActive)
	{
		if (cameraCutsceneResumeFrame == cameraFrameSerial)
		{
			return;
		}
		bCameraCutsceneResumeGuardActive = false;
	}

	const bool bExitedVehicle = bCameraWasInVehicle && !bInVehicle;
	const bool bExitedDedicatedVehicleCamera =
		bCameraWasUsingDedicatedVehicleCamera && !bInVehicle;
	const bool bOperatingMountedVehicleTurret =
		bInVehicle &&
		IsLocalPlayerOperatingMountedVehicleTurret();
	const bool bUsesDedicatedVehicleAimCamera =
		bInVehicle &&
		(!bHasWeapon ||
			bOperatingMountedVehicleTurret);

	bCameraWasInVehicle = bInVehicle;
	bCameraWasUsingDedicatedVehicleCamera =
		bUsesDedicatedVehicleAimCamera;

	if (bInVehicle)
	{
		bVehicleExitCameraGuardActive = false;
		vehicleExitCameraGuardElapsedSeconds = 0.0f;
	}
	else if (bExitedDedicatedVehicleCamera)
	{
		bVehicleExitCameraGuardActive = true;
		vehicleExitCameraGuardElapsedSeconds = 0.0f;
	}
	else if (bExitedVehicle)
	{
		// Armed passenger seats remain in the standard camera family. Reset the
		// sample gate on the first on-foot call instead of freezing the camera and
		// releasing a large correction after a timed guard.
		bVehicleExitCameraGuardActive = false;
		vehicleExitCameraGuardElapsedSeconds = 0.0f;
	}

	bool bGuardVehicleExitCamera = false;
	if (bVehicleExitCameraGuardActive)
	{
		if (vehicleExitCameraGuardElapsedSeconds <
			VehicleExitCameraGuardDurationSeconds)
		{
			bGuardVehicleExitCamera = true;

			float frameSeconds = lastDeltaTime;
			if (!std::isfinite(frameSeconds) || frameSeconds < 0.0f)
			{
				frameSeconds = 0.0f;
			}
			else if (frameSeconds > 0.10f)
			{
				frameSeconds = 0.10f;
			}
			vehicleExitCameraGuardElapsedSeconds += frameSeconds;
		}
		else
		{
			bVehicleExitCameraGuardActive = false;
		}
	}

	const bool bArmedVehicle = bInVehicle && bHasWeapon;
	if (bGuardVehicleExitCamera)
	{
		inputHandler.bArmedVehicleCameraSampleValid = false;
	}
	else if (bUsesDedicatedVehicleAimCamera)
	{
		// Mounted turrets use the same separated vehicle-aim path as
		// driver-operated vehicles.
		inputHandler.bArmedVehicleCameraSampleValid = false;
		inputHandler.UpdateCameraForVehicles(
			yaw,
			pitch);
	}
	else
	{
		inputHandler.UpdateCamera(yaw, pitch, bArmedVehicle);
	}
}

void Game::SetMousePosition(int& x, int& y)
{
	VR_PROFILE_SCOPE(Game_SetMousePosition);
	// Don't bother simulating inputs if we aren't actually in vr
#ifndef EMULATE_VR
	inputHandler.SetMousePosition(x, y);
#endif
	if (Helpers::IsMouseVisible())
	{
		uiRenderer->MoveCursor(static_cast<float>(x), static_cast<float>(y));
#ifndef EMULATE_VR
		// Stop menu hover events happening while ui is up
		if (settingsMenu->bVisible)
		{
			x = 0;
			y = 0;
		}
#endif
	}
}

void Game::UpdateMouseInfo(MouseInfo* mouseInfo)
{
	VR_PROFILE_SCOPE(Game_UpdateMouseInfo);

	static char realLeftClickValue = 0;

	// Don't bother simulating inputs if we aren't actually in vr
#ifndef EMULATE_VR
	inputHandler.UpdateMouseInfo(mouseInfo);
#endif
	if (Helpers::IsMouseVisible() && mouseInfo->buttonState[0] == 1)
	{
		uiRenderer->Click();

#ifndef EMULATE_VR
		// Don't allow any mouse inputs to pass through to the real UI while ours is up
		if (settingsMenu->bVisible)
		{
			mouseInfo->buttonState[0] = 0;
		}
#endif
	}

	// Hack fix for closing settings menu if the player exits a menu with it open
	if (!Helpers::IsMouseVisible() && settingsMenu->bVisible)
	{
		uiRenderer->MoveCursor(0.0f, 0.0f);
		uiRenderer->Click();
	}
}

void Game::SetViewportScale(Viewport* viewport)
{
	VR_PROFILE_SCOPE(Game_SetViewportScale);
	// This appears to be broken, the code this is overriding does something very weird with the scaling
	/*
	viewport->left = -1.0f;
	viewport->right = 1.0f;
	viewport->bottom = 1.0f;
	viewport->top = -1.0f;
	*/
	viewport->left = c_TEMPViewportLeft->Value();
	viewport->right = c_TEMPViewportRight->Value();
	viewport->top = c_TEMPViewportTop->Value();
	viewport->bottom = c_TEMPViewportBottom->Value();
}

float Game::MetresToWorld(float m) const
{
	return m / 3.048f;
}

float Game::WorldToMetres(float w) const
{
	return w * 3.048f;
}

void Game::CreateConsole()
{
	if (!c_ShowConsole || !c_ShowConsole->Value())
	{
		return;
	}

	AllocConsole();
	freopen_s(&consoleOut, "CONOUT$", "w", stdout);
	std::cout.clear();
}

void Game::PatchGame()
{
	VR_PROFILE_SCOPE(Game_PatchGame);
	MH_STATUS hookStatus;

	if ((hookStatus = MH_Initialize()) != MH_OK)
	{
		Logger::err << "Could not initialise MinHook: " << MH_StatusToString(hookStatus) << std::endl;
	}
	else
	{
		Logger::log << "[Game] Creating hooks" << std::endl;
		Hooks::InitHooks();
		Logger::log << "[Game] Enabling hooks" << std::endl;
		Hooks::EnableAllHooks();
	}
}

void Game::SetupConfigs()
{
	VR_PROFILE_SCOPE(Game_SetupConfigs);

	// Window settings
	c_ShowConsole = config.RegisterBool("ShowConsole", "Create a console window at launch for debugging purposes", false);
	c_DrawMirror = config.RegisterBool("DrawMirror", "Update the desktop window display to show the current game view, rather than leaving it on the splash screen", true);
	c_MirrorEye = config.RegisterInt("MirrorEye", "Index of the eye to use for the mirror view  (0 = left, 1 = right)", 0);
	// UI settings
	c_CrosshairDistance = config.RegisterFloat("CrosshairDistance", "Distance in meters in front of the weapon to display the crosshair", 15.0f);
	c_FirstPersonVehicleCrosshairDistance = config.RegisterFloat(
		"FirstPersonVehicleCrosshairDistance",
		"Distance in meters from the VR aim origin to display the floating crosshair in a 1st Person vehicle view. This is independent from CrosshairDistance and includes native 1st Person passenger seats.",
		15.0f);
	c_ThirdPersonVehicleCrosshairDistance = config.RegisterFloat(
		"ThirdPersonVehicleCrosshairDistance",
		"Distance in meters from the VR aim origin to display the floating crosshair in a 3rd Person vehicle view. This is independent from CrosshairDistance.",
		15.0f);
	c_MenuOverlayDistance = config.RegisterFloat("MenuOverlayDistance", "Distance in meters in front of the HMD to display the menu", 15.0f);
	c_UIOverlayDistance = config.RegisterFloat("UIOverlayDistance", "Distance in meters in front of the HMD to display the UI", 15.0f);
	c_UIOverlayScale = config.RegisterFloat("UIOverlayScale", "Width of the UI overlay in meters", 10.0f);
	c_MenuOverlayScale = config.RegisterFloat("MenuOverlayScale", "Width of the menu overlay in meters", 10.0f);
	c_CrosshairScale = config.RegisterFloat("CrosshairScale", "Width of the crosshair overlay in meters", 10.0f);
	c_UIOverlayCurvature = config.RegisterFloat("UIOverlayCurvature", "Curvature of the UI Overlay, on a scale of 0 to 1", 0.1f);
	c_UIOverlayRenderScale = config.RegisterFloat("UIOverlayRenderScale", "Resolution of the UI overlay, expressed as a proportion of the headset's render scale (e.g. 0.5 = half resolution), low values default to 640px", 0.5f);
	c_ShowCrosshair = config.RegisterBool("ShowCrosshair", "Display a floating crosshair in the world at the location you are aiming", true);
	c_ShowCrosshairInVehicles = config.RegisterBool(
		"ShowCrosshairInVehicles",
		"Display the floating crosshair while in a vehicle even when ShowCrosshair is off, except in the Warthog driver/side passenger seats and Scorpion side passenger seats.",
		false);
	c_AlwaysHideWarthogCrosshair = config.RegisterBool(
		"AlwaysHideWarthogCrosshair",
		"Always hide the floating crosshair in every Warthog seat, overriding ShowCrosshair and ShowCrosshairInVehicles.",
		false);
	// Control settings
	c_LeftHanded = config.RegisterBool("LeftHanded", "Make the left hand the dominant hand by default. This swaps the button bindings but doesn't swap the sticks. Left handed bindings with the sticks swapped can be found in the SteamVR overlay", false);
	c_SnapTurn = config.RegisterBool("SnapTurn", "The look input will instantly rotate the view by a fixed amount, rather than smoothly rotating", true);
	c_SnapTurnAmount = config.RegisterFloat("SnapTurnAmount", "Rotation in degrees a single snap turn will rotate the view by", 45.0f);
	c_SmoothTurnAmount = config.RegisterFloat("SmoothTurnAmount", "Rotation in degrees per second the view will turn at when not using snap turning", 90.0f);
	c_RoomScaleMovement = config.RegisterBool("RoomScaleMovement", "Attempt to move the character to always match the headset's position. (May cause motion sickness, as collisions can cause a desync between physical and in-game movements)", true);
	c_MultiplayerHitboxAccurateProjectiles = config.RegisterBool(
		"MultiplayerHitboxAccurateProjectiles",
		"Controls projectile origin in standard multiplayer. Off (default): projectiles fire from the tracked VR weapon for the best visual alignment. On: projectiles use Halo's player-model origin for the closest match between visuals and hit registration. Official Coop Evolved maps always use Hitbox Accurate mode to prevent duplicate projectiles.",
		false);
	c_MPRoomScaleWhileMoving = config.RegisterBool(
		"MPRoomScaleWhileMoving",
		"Allow the in game body to follow roomscale movement while the left movement stick is used. On (default) attempts to allow roomscale movement while using the left stick.  Off pauses the in game body from catching up to roomscale movements until you let go of the movement stick.",
		true);
	c_MPRoomScaleActivationDistance = config.RegisterFloat(
		"MPRoomScaleActivationDistance",
		"Joined-client horizontal distance in meters that must be sustained before the in-game body follows roomscale movement. Larger values ignore more ordinary head sway.",
		0.0f);
	c_MPRoomScaleActivationDelay = config.RegisterFloat(
		"MPRoomScaleActivationDelay",
		"Seconds the joined-client roomscale activation distance must be sustained before body follow starts.",
		0.0f);
	c_MPRoomScaleSettleDistance = config.RegisterFloat(
		"MPRoomScaleSettleDistance",
		"Joined-client roomscale body follow stops within this many meters of the tracked headset, preventing tiny corrective shuffles.",
		0.0f);
	c_MPRoomScaleFullSpeedDistance = config.RegisterFloat(
		"MPRoomScaleFullSpeedDistance",
		"Joined-client roomscale separation in meters that requests full body catch-up speed; smaller offsets use gentler proportional movement.",
		0.0f);
	c_MPRoomScaleMaximumTrackingDistance = config.RegisterFloat(
		"MPRoomScaleMaximumTrackingDistance",
		"Maximum joined-client horizontal tracking offset in meters before HaloCEVR safely recentres a likely tracking-origin discontinuity. Large gradual roomscale walks remain supported below this value.",
		8.0f);
	c_HandRelativeMovement = config.RegisterInt("HandRelativeMovement", "Movement is relative to hand orientation, rather than head, 0 = off, 1 = left, 2 = right", 0);
	c_HandRelativeOffsetRotation = config.RegisterFloat("HandRelativeOffsetRotation", "Hand direction rotational offset in degrees used for hand-relative movement", -20.0f);
	c_HorizontalVehicleTurnAmount = config.RegisterFloat("HorizontalVehicleTurnAmount", "Rotation in degrees per second the view will turn horizontally when in vehicles (<0 to invert)", 90.0f);
	c_VerticalVehicleTurnAmount = config.RegisterFloat("VerticalVehicleTurnAmount", "Rotation in degrees per second the view will turn vertically when in vehicles (<0 to invert)", 45.0f);
	c_MotionControlledVehicles = config.RegisterBool(
		"MotionControlledVehicles",
		"Aim supported vehicle weapons with the dominant VR controller while retaining Halo's native 3rd Person cameras.",
		true);
	c_DisableWarthogSteeringMotionControls = config.RegisterBool(
		"DisableWarthogSteeringMotionControls",
		"Keeps the Warthog's native Halo/right-stick steering even when MotionControlledVehicles is enabled.",
		true);
	c_SmoothVehicleVRAim = config.RegisterBool(
		"SmoothVehicleVRAim",
		"Apply weapon smoothing to vehicle motion controlled aiming. Recommended if you notice camera shake from motion controlled vehicle aiming.",
		true);
	c_VehicleVRAimSmoothingAmount = config.RegisterFloat(
		"VehicleVRAimSmoothingAmount",
		"Amount of weapon smoothing applied to motion controlled vehicles. 1 is recommended to smooth camera shake from motion controlled 3rd Person vehicle cameras. 0 = off, 1 = medium smoothing.",
		1.0f);
	c_HideHeadInFirstPersonVehicles = config.RegisterInt(
		"HideHeadIn1stPersonVehicles",
		"0 = full body (vanilla), 1 = hide the head (recommended).",
		1);
	c_FirstPersonWarthogDriver = config.RegisterBool(
		"1stPersonWarthogDriver",
		"When enabled changes the view of driving the Warthog from 3rd Person to 1st Person.",
		false);
	c_WarthogDriverOffset = config.RegisterVector3(
		"WarthogDriverOffset",
		"1st Person Warthog driver camera offset from the seated player in meters: X = forward, Y = left, Z = up.",
		Vector3(-0.20f, 0.0f, 1.65f));
	c_FirstPersonWarthogTurret = config.RegisterBool(
		"1stPersonWarthogTurret",
		"When enabled changes the view of the Warthog Turret from 3rd Person to 1st Person.",
		false);
	c_WarthogTurretOffset = config.RegisterVector3(
		"WarthogTurretOffset",
		"1st Person Warthog Turret camera offset from the gunner in meters: X = forward, Y = left, Z = up.",
		Vector3(-1.40f, 0.0f, 1.35f));
	c_FirstPersonGhost = config.RegisterBool(
		"1stPersonGhost",
		"When enabled changes the view of the Ghost from 3rd Person to 1st Person.",
		false);
	c_GhostOffset = config.RegisterVector3(
		"GhostOffset",
		"1st Person Ghost camera offset from the seated player in meters: X = forward, Y = left, Z = up.",
		Vector3(-0.50f, 0.0f, 1.40f));
	c_FirstPersonScorpion = config.RegisterBool(
		"1stPersonScorpion",
		"When enabled changes the view of the Scorpion from 3rd Person to 1st Person.",
		false);
	c_ScorpionOffset = config.RegisterVector3(
		"ScorpionOffset",
		"1st Person Scorpion camera offset from the seated player in meters: X = forward, Y = left, Z = up.",
		Vector3(0.10f, 0.0f, 1.20f));
	c_FirstPersonBanshee = config.RegisterBool(
		"1stPersonBanshee",
		"When enabled changes the view of the Banshee from 3rd Person to 1st Person.",
		false);
	c_BansheeOffset = config.RegisterVector3(
		"BansheeOffset",
		"1st Person Banshee camera offset from the seated player in meters: X = forward, Y = left, Z = up.",
		Vector3(1.30f, 0.0f, -0.30f));
	c_FirstPersonShade = config.RegisterBool(
		"1stPersonShade",
		"When enabled changes the view of the Shade from 3rd Person to 1st Person.",
		false);
	c_ShadeOffset = config.RegisterVector3(
		"ShadeOffset",
		"1st Person Shade camera offset from the operator in meters: X = forward, Y = left, Z = up.",
		Vector3(0.05f, 0.0f, 2.00f));
	c_UseJumpForVehicleSecondaryFire = config.RegisterBool(
		"UseJumpForVehicleSecondaryFire",
		"While in a vehicle, use the Jump binding for secondary fire instead of the Grenade binding.",
		false);
	c_ToggleGrip = config.RegisterBool("ToggleGrip", "When true releasing two handed weapons requires pressing the grip action again", false);
	c_TwoHandDistance = config.RegisterFloat("TwoHandDistance", "Maximum distance between both hands where the off hand grip action will enable two handed aiming (<0 for any distance)", 0.8f);
	c_SwapHandDistance = config.RegisterFloat("SwapHandDistance", "Maximum distance between both hands where the swap weapon hand grip action will swap your weapon into the opposite hand (<0 to disable)", 0.2f);
	c_OffhandHandFlashlight = config.RegisterBool("OffhandHandFlashlight", "Use your offhand for toggling the flashlight, your offhand hand is the hand not holding a weapon", true);
	c_LeftHandFlashlightDistance = config.RegisterFloat("LeftHandFlashlight", "Bringing the left hand within this distance of the head will toggle the flashlight (<0 to disable)", 0.2f);
	c_RightHandFlashlightDistance = config.RegisterFloat("RightHandFlashlight", "Bringing the right hand within this distance of the head will toggle the flashlight (<0 to disable)", 0.2f);
	c_LeftHandMeleeSwingSpeed = config.RegisterFloat("LeftHandMeleeSwingSpeed", "Minimum vertical velocity of left hand required to initiate a melee attack in m/s (<0 to disable)", 2.5f);
	c_RightHandMeleeSwingSpeed = config.RegisterFloat("RightHandMeleeSwingSpeed", "Minimum vertical velocity of right hand required to initiate a melee attack in m/s (<0 to disable)", 2.5f);
	c_SinglePlayerMeleeWeaponAim = config.RegisterBool(
		"SinglePlayerMeleeWeaponAim",
		"Single-player melee direction. On: melee follows the weapon/aiming hand. Off: melee follows the HMD/head direction.",
		true);
	c_CrouchHeight = config.RegisterFloat("CrouchHeight", "Minimum height to duck by in meters to automatically trigger the crouch input in game (<0 to disable)", 0.15f);
	// Hand settings
	c_ControllerOffset = config.RegisterVector3("ControllerOffset", "Offset from the controller's position used when calculating the in game hand position", Vector3(0.0f, 0.0f, 0.0f));
	c_ControllerRotation = config.RegisterVector3("ControllerRotation", "Rotation added to the controller when calculating the in game hand rotation", Vector3(0.0f, 0.0f, 0.0f));
	c_ScopeRenderScale = config.RegisterFloat("ScopeRenderScale", "Size of the scope render target, expressed as a proportion of the headset's render scale (e.g. 0.5 = half resolution)", 1.0f);
	c_ScopeScale = config.RegisterFloat("ScopeScale", "Width of the scope view in meters (6DOF mode)", 0.05f);
	c_CampaignZoom1Magnification = config.RegisterFloat(
		"CampaignZoom1Magnification",
		"Gearbox campaign magnification adjustment for pistol zoom and sniper zoom level 1. 1.00 keeps the original magnification; values above 1.00 reduce magnification.",
		8.00f);
	c_CampaignZoom2Magnification = config.RegisterFloat(
		"CampaignZoom2Magnification",
		"Gearbox campaign magnification adjustment for sniper zoom level 2. 1.00 keeps the original magnification; values above 1.00 reduce magnification.",
		5.00f);
	c_CustomEditionZoom1Magnification = config.RegisterFloat(
		"CustomEditionZoom1Magnification",
		"Custom Edition magnification adjustment for pistol zoom and sniper zoom level 1. 1.00 keeps the original magnification; values above 1.00 reduce magnification.",
		1.00f);
	c_CustomEditionZoom2Magnification = config.RegisterFloat(
		"CustomEditionZoom2Magnification",
		"Custom Edition magnification adjustment for sniper zoom level 2. 1.00 keeps the original magnification; values above 1.00 reduce magnification.",
		1.00f);
	c_LockScopeRoll = config.RegisterBool("LockScopeRoll", "Set to true to keep the horizon level at all times in scopes. Leaving as false causes the scope view to rotate with the gun (pre-v1.3.0 behaviour)", true);
	c_UseOriginalScope = config.RegisterBool("UseOriginalScope", "Use original Halo scope graphics instead of VR weapon-attached scope. Automatically enabled in 3DOF mode.", false);
	c_ScopeOffsetPistol = config.RegisterVector3("ScopeOffsetPistol", "Offset of the scope view relative to the pistol's location", Vector3(-0.1f, 0.0f, 0.15f));
	c_ScopeOffsetSniper = config.RegisterVector3("ScopeOffsetSniper", "Offset of the scope view relative to the pistol's location", Vector3(-0.15f, 0.0f, 0.15f));
	c_ScopeOffsetRocket = config.RegisterVector3("ScopeOffsetRocket", "Offset of the scope view relative to the pistol's location", Vector3(0.1f, 0.2f, 0.1f));
	c_WeaponSmoothingAmountNoZoom = config.RegisterFloat("UnzoomedWeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when not zoomed in (0 is disabled, 2.0 is maximum, recommended around 0-0.7)", 0.0f);
	c_WeaponSmoothingAmountOneZoom = config.RegisterFloat("Zoom1WeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when zoomed in once, eg zooming on the pistol (0 is disabled, 2.0 is maximum, recommended around 0.3-1.0)", 0.4f);
	c_WeaponSmoothingAmountTwoZoom = config.RegisterFloat("Zoom2WeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when zoomed in twice, eg second zoom on sniper (0 is disabled, 2.0 is maximum, recommended around 0.6-1.25)", 0.6f);
	c_Use3DOFAiming = config.RegisterBool("Use3DOFAiming", "Use controller rotation only for aiming (3DOF) instead of position+rotation (6DOF). When enabled, weapon models follow hand position but bullets fire based on controller rotation only", false);
	c_3DOFWeaponOffset = config.RegisterVector3("3DOFWeaponOffset", "This is a cosmetic setting for the position offset for the 3DOF weapon model. This has no impact on gameplay. (right, forward, up in meters). Use negative Z to lower the weapon", Vector3(0.0f, 0.0f, -0.08f));
	c_3DOFWeaponSmoothingAmount = config.RegisterFloat("3DOFWeaponSmoothingAmount", "This is a cosmetic setting that controls the amount of smoothing applied to 3DOF weapon facing direction. This has no impact on gameplay. (0 is disabled, 2.0 is maximum, default is 1.5)", 1.5f);
	c_3DOFScopeScale = config.RegisterFloat("3DOFScopeScale", "Width of the scope view in meters (3DOF mode)", 7.f);
	// Weapon holster settings
	c_EnableWeaponHolsters = config.RegisterBool("EnableWeaponHolsters", "When enabled Weapons can only be switched by using the 'SwitchWeapons' binding while the dominant hand is within distance of a holster", true);
	c_LeftShoulderHolsterActivationDistance = config.RegisterFloat("LeftShoulderHolsterDistance", "The 'size' of the left shoulder holster. This is the distance that the dominant hand needs to be from the holster to change weapons (<0 to disable)", 0.3f);
	c_LeftShoulderHolsterOffset = config.RegisterVector3("LeftShoulderHolsterOffset", "The (foward, left, up) Offset of the left shoulder holster relative to the headset's location", Vector3(-0.15f, 0.25f, -0.25f));
	c_RightShoulderHolsterActivationDistance = config.RegisterFloat("RightShoulderHolsterDistance", "The 'size' of the right shoulder holster. This is the distance that the dominant hand needs to be from the holster to change weapons (<0 to disable)", 0.3f);
	c_RightShoulderHolsterOffset = config.RegisterVector3("RightShoulderHolsterOffset", "The (foward, left, up) Offset of the right shoulder holster relative to the headset's location", Vector3(-0.15f, -0.25f, -0.25f));
	// Misc settings
	c_ShowRoomCentre = config.RegisterBool("ShowRoomCentre", "Draw an indicator at your feet to show where the player character is actually positioned", true);
	c_d3d9Path = config.RegisterString("CustomD3D9Path", "If set first try to load d3d9.dll from the specified path instead of from system32", "");
	c_TEMPViewportLeft = config.RegisterFloat("TEMP_ViewportLeft", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", -1.0f);
	c_TEMPViewportRight = config.RegisterFloat("TEMP_ViewportRight", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", 1.0f);
	c_TEMPViewportTop = config.RegisterFloat("TEMP_ViewportTop", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", -1.0f);
	c_TEMPViewportBottom = config.RegisterFloat("TEMP_ViewportBottom", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", 1.0f);

	bLoadedConfig = config.LoadFromFile("VR/config.txt");
	bSavedConfig = config.SaveToFile("VR/config.txt");

	if (!bLoadedConfig)
	{
		Logger::log << "[Config] First time startup, generating default config file" << std::endl;
	}

	if (!bSavedConfig)
	{
		Logger::log << "[Config] Couldn't save config file, halo is likely running as non-administrator from a protected directory" << std::endl;
	}

	// First run, but couldn't create config file
	if (!bLoadedConfig && !bSavedConfig)
	{
		Logger::err << "Could not create /VR/config.txt.\nHalo may have been installed in Program Files, to generate the config file either run halo.exe as an administrator or reinstall the game in a non-protected folder (e.g. Documents)." << std::endl;
	}

	weaponHandler.localOffset = Vector3(c_ControllerOffset->Value().x, c_ControllerOffset->Value().y, c_ControllerOffset->Value().z);
	weaponHandler.localRotation = Vector3(c_ControllerRotation->Value().x, c_ControllerRotation->Value().y, c_ControllerRotation->Value().z);

	if (c_MirrorEye->Value() == 0)
	{
		mirrorSource = ERenderState::LEFT_EYE;
	}
	else if (c_MirrorEye->Value() == 1)
	{
		mirrorSource = ERenderState::RIGHT_EYE;
	}
	else if (c_MirrorEye->Value() == 2)
	{
		Logger::log << "[Config] MirrorEye set to 'game'. This is intended for debugging, may not look correct, and will likely impact performance" << std::endl;
		mirrorSource = ERenderState::GAME;
	}
	else
	{
		Logger::log << "[Config] Invalid value for MirrorEye, defaulting to left eye" << std::endl;
		mirrorSource = ERenderState::LEFT_EYE;
	}

	WeaponHapticsConfigManager weaponHapticsConfig;
}

void Game::CalcFPS(float deltaTime)
{
	fpsTracker.framesSinceFPSUpdate++;
	fpsTracker.timeSinceFPSUpdate += deltaTime;

	if (fpsTracker.timeSinceFPSUpdate > 1.0f)
	{
		fpsTracker.timeSinceFPSUpdate = 0.0f;
		fpsTracker.fps = fpsTracker.framesSinceFPSUpdate;
		fpsTracker.framesSinceFPSUpdate = 0;
		Logger::log << fpsTracker.fps << std::endl;
	}
}

#if USE_PROFILER
void Game::DumpProfilerData()
{
	VR_PROFILE_SCOPE(Game_DumpProfilerData);

	// TODO: Create a better system for viewing this data (e.g. real time display/proper dedicated profile files)
	std::vector<Profiler::FrameTimings*> frameTimings;
	Game::instance.profiler.GetTimings(frameTimings);

	Logger::log << "[Profiler] Dumping last 30 seconds of profiler data..." << std::endl;

	float minFrame = FLT_MAX;
	float maxFrame = -FLT_MAX;
	float totalFrame = 0.0f;
	int numFrames = 0;

	Profiler::time_point now = std::chrono::high_resolution_clock::now();

	std::unordered_map<std::string, Profiler::Timings> totalTimes;

	for (auto it = frameTimings.rbegin(); it != frameTimings.rend(); ++it)
	{
		float ago = std::chrono::duration<float, std::milli>(now - (*it)->frameEnd).count();
		if (ago > 30.0f * 1000.0f)
		{
			break;
		}

		float duration = std::chrono::duration<float, std::milli>((*it)->frameEnd - (*it)->frameStart).count();

		minFrame = (std::min)(minFrame, duration);
		maxFrame = (std::max)(maxFrame, duration);
		totalFrame += duration;
		numFrames++;

		for (auto& kv : (*it)->timings)
		{
			const std::string& eventName = kv.first;
			const Profiler::Timings* timings = kv.second;
			/*
			Logger::log << "[Profiler] "
				<< eventName
				<< ": calls " << timings->numHits
				<< " min. " << timings->minTime
				<< " avg. " << timings->totalTime / timings->numHits
				<< " max. " << timings->maxTime
				<< std::endl;
			*/

			totalTimes[eventName].minTime = (std::min)(totalTimes[eventName].minTime, timings->minTime);
			totalTimes[eventName].maxTime = (std::max)(totalTimes[eventName].maxTime, timings->maxTime);
			totalTimes[eventName].numHits += timings->numHits;
			totalTimes[eventName].totalTime += timings->totalTime;
		}
	}

	Logger::log << "[Profiler] Frame times: min. " << minFrame << " avg. " << (totalFrame / numFrames) << " max. " << maxFrame << std::endl;
	Logger::log << "[Profiler] Total Event times: " << std::endl;

	std::vector<std::pair<std::string, Profiler::Timings>> topTimings;
	for (auto& kv : totalTimes)
	{
		topTimings.push_back(kv);
	}

	std::sort(topTimings.begin(), topTimings.end(), [](const auto& a, const auto& b)
		{
			return a.second.totalTime > b.second.totalTime;
		}
	);

	for (auto& kv : topTimings)
	{
		Logger::log << "[Profiler] "
			<< kv.first
			<< ": calls " << kv.second.numHits
			<< " min. " << kv.second.minTime
			<< " avg. " << kv.second.totalTime / kv.second.numHits
			<< " max. " << kv.second.maxTime
			<< std::endl;
	}
}
#endif

void Game::UpdateCrosshairAndScope()
{
	VR_PROFILE_SCOPE(Game_UpdateCrosshairAndScope);

	auto fixupRotation = [](Matrix4& m, Vector3& pos) {
		m.translate(-pos);
		m.rotate(90.0f, m.getUpAxis());
		m.rotate(-90.0f, m.getLeftAxis());
		m.translate(pos);
		};

	Vector3 aimPos, aimDir, upDir;

	bool bIsWarthog = false;
	bool bUsesNativeVehicleAim = false;
	const bool bExcludedVehicleSeat =
		IsLocalPlayerInCrosshairExcludedVehicleSeat(
			&bIsWarthog,
			&bUsesNativeVehicleAim);
	const bool bCachedScorpionDriver =
		IsCachedScorpionDriverForLocalPlayer();

	// Campaign keeps Halo's native vehicle/right-stick direction. The normalized
	// seat classifier controls only whether that seat owns a vehicle reticle.
	const bool bCampaignVehicleCrosshair =
		!IsMultiplayerSession() &&
		bInVehicle;

	const bool bStableVehicleCrosshairContext =
		bCampaignVehicleCrosshair ||
		bInVehicle ||
		bCachedScorpionDriver ||
		bUsesNativeVehicleAim;
	UnitDynamicObject* liveCrosshairPlayer =
		static_cast<UnitDynamicObject*>(
			Helpers::GetLocalPlayer());
	BaseDynamicObject* liveCrosshairParent =
		liveCrosshairPlayer &&
			liveCrosshairPlayer->parent.id != 0xFFFF &&
			liveCrosshairPlayer->parent.index != 0xFFFF
			? Helpers::GetDynamicObject(
				liveCrosshairPlayer->parent)
			: nullptr;
	const bool bLiveVehicleCrosshairContext =
		liveCrosshairParent &&
		liveCrosshairParent->N0000027E ==
			ObjectType::VEHICLE;
	const bool bVehicleCrosshairDistanceContext =
		bStableVehicleCrosshairContext ||
		bLiveVehicleCrosshairContext;
	const bool bOperatingMountedVehicleTurret =
		bInVehicle &&
		IsLocalPlayerOperatingMountedVehicleTurret();
	const bool bNativeVehicleAimContext =
		bCampaignVehicleCrosshair ||
		(bInVehicle && !bHasWeapon) ||
		bOperatingMountedVehicleTurret ||
		bCachedScorpionDriver ||
		bUsesNativeVehicleAim;

	bool bHaveAim = false;
	if (bNativeVehicleAimContext)
	{
		// Use the same raw dominant-controller direction as the vehicle camera
		// controller so the floating reticle follows the requested VR aim.
		bHaveAim =
			GetVehicleVRAim(aimPos, aimDir, upDir);

		if (!bHaveAim)
		{
			// Default and tracking-failure fallback.
			aimPos = Vector3();
			aimDir = Helpers::GetCamera().lookDir;
			upDir = Vector3(0.0f, 0.0f, 1.0f);
			bHaveAim =
				std::isfinite(aimDir.x) &&
				std::isfinite(aimDir.y) &&
				std::isfinite(aimDir.z) &&
				aimDir.lengthSqr() > 0.000001f;
		}
	}
	else
	{
		bHaveAim =
			weaponHandler.GetLocalWeaponAim(
				aimPos,
				aimDir,
				upDir);
	}

	if (!bHaveAim)
	{
		return;
	}

	Matrix4 overlayTransform;

	Vector3 hmdPos = vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);

	// In 3DOF mode, crosshair projects from HMD position (matching bullet origin)
	// In 6DOF mode, crosshair projects from controller/weapon position
	Vector3 crosshairOrigin = bUse3DOFAiming ? hmdPos : aimPos;
	float crosshairDistance =
		c_CrosshairDistance->Value();
	if (bVehicleCrosshairDistanceContext)
	{
		FloatProperty* vehicleCrosshairDistance =
			IsCurrentVehicleViewFirstPerson()
				? c_FirstPersonVehicleCrosshairDistance
				: c_ThirdPersonVehicleCrosshairDistance;
		if (vehicleCrosshairDistance)
		{
			crosshairDistance =
				vehicleCrosshairDistance->Value();
		}
	}
	if (!std::isfinite(crosshairDistance))
	{
		crosshairDistance = 15.0f;
	}

	Vector3 targetPos =
		crosshairOrigin +
		aimDir *
			crosshairDistance;

	overlayTransform.translate(targetPos);
	overlayTransform.lookAt(hmdPos, Vector3(0.0f, 0.0f, 1.0f));

	fixupRotation(overlayTransform, targetPos);

	const bool bShowVehicleCrosshair =
		bStableVehicleCrosshairContext &&
		c_ShowCrosshairInVehicles->Value() &&
		!bExcludedVehicleSeat;
	const bool bHideWarthogCrosshair =
		bStableVehicleCrosshairContext &&
		bIsWarthog &&
		c_AlwaysHideWarthogCrosshair->Value();
	const bool bShowCrosshair =
		!bHideWarthogCrosshair &&
		(c_ShowCrosshair->Value() || bShowVehicleCrosshair);
	if (bShowCrosshair)
	{
		vr->SetCrosshairTransform(overlayTransform);
	}
	overlayTransform.identity();

	short zoom = Helpers::GetInputData().zoomLevel;

	bool bHasScope;
	Vector3 scopePos;

	if (ShouldUseOriginalScope())
	{
		// Original scope: position at crosshair
		bHasScope = (zoom != -1);
		scopePos = targetPos;
	}
	else
	{
		// VR scope: position at weapon
		bHasScope = (zoom != -1) && weaponHandler.GetLocalWeaponScope(aimPos, aimDir, upDir);
		scopePos = aimPos;
	}

	if (!bHasScope)
	{
		SetScopeTransform(overlayTransform, false);
		return;
	}

	overlayTransform.translate(scopePos);
	overlayTransform.lookAt(scopePos - aimDir, upDir);

	fixupRotation(overlayTransform, scopePos);

	SetScopeTransform(overlayTransform, true);
}

void Game::SetScopeTransform(Matrix4& newTransform, bool bIsVisible)
{
	VR_PROFILE_SCOPE(Game_SetScopeTransform);

	if (!bIsVisible)
	{
		return;
	}

	Vector3 scopeUp = newTransform.getForwardAxis();
	Vector3 scopeFacing = -newTransform.getLeftAxis();

	Vector3 pos = (newTransform * Vector3(0.0f, 0.0f, 0.0f)) * MetresToWorld(1.0f) + Helpers::GetCamera().position;
	Matrix3 rot;
	Vector2 size(1.0f, 0.75f);
	size *= MetresToWorld(GetScopeSize());

	newTransform.translate(-pos);
	newTransform.rotate(90.0f, newTransform.getLeftAxis());
	newTransform.rotate(-90.0f, newTransform.getUpAxis());
	newTransform.rotate(-90.0f, newTransform.getLeftAxis());

	for (int i = 0; i < 3; i++)
	{
		rot.setColumn(i, &newTransform.get()[i * 4]);
	}

	inGameRenderer.DrawPolygon(pos, scopeFacing, scopeUp, 32, MetresToWorld(GetScopeSize() * 0.5f), D3DCOLOR_ARGB(0, 0, 0, 0), false);

	float SCOPE_DEPTH = 2.0f;
	float SCOPE_INNER_SCALE = ShouldUseOriginalScope() ? 1.6f : 80.0f;

	pos = pos - scopeFacing * MetresToWorld(SCOPE_DEPTH);
	size *= SCOPE_INNER_SCALE;

	inGameRenderer.DrawRenderTarget(vr->GetScopeTexture(), pos, rot, size, false, true);
}

void Game::StoreRenderTargets()
{
	VR_PROFILE_SCOPE(Game_StoreRenderTargets);

	for (int i = 0; i < 8; i++)
	{
		gameRenderTargets[i].width = Helpers::GetRenderTargets()[i].width;
		gameRenderTargets[i].height = Helpers::GetRenderTargets()[i].height;
		gameRenderTargets[i].format = Helpers::GetRenderTargets()[i].format;
		gameRenderTargets[i].renderSurface = Helpers::GetRenderTargets()[i].renderSurface;
		gameRenderTargets[i].renderTexture = Helpers::GetRenderTargets()[i].renderTexture;
	}
}

void Game::RestoreRenderTargets()
{
	VR_PROFILE_SCOPE(Game_RestoreRenderTargets);

	for (int i = 0; i < 8; i++)
	{
		Helpers::GetRenderTargets()[i].width = gameRenderTargets[i].width;
		Helpers::GetRenderTargets()[i].height = gameRenderTargets[i].height;
		Helpers::GetRenderTargets()[i].format = gameRenderTargets[i].format;
		Helpers::GetRenderTargets()[i].renderSurface = gameRenderTargets[i].renderSurface;
		Helpers::GetRenderTargets()[i].renderTexture = gameRenderTargets[i].renderTexture;
	}
}

void Game::CreateTextureAndSurface(UINT Width, UINT Height, DWORD Usage, D3DFORMAT Format, IDirect3DSurface9** OutSurface, IDirect3DTexture9** OutTexture)
{
	VR_PROFILE_SCOPE(Game_CreateTextureAndSurface);

	HRESULT result = Helpers::GetDirect3DDevice9()->CreateTexture(Width, Height, 1, Usage, Format, D3DPOOL_DEFAULT, OutTexture, nullptr);
	if (FAILED(result))
	{
		Logger::err << "[DX9] Failed to create game texture: " << result << std::endl;
		return;
	}

	result = (*OutTexture)->GetSurfaceLevel(0, OutSurface);
	if (FAILED(result))
	{
		Logger::err << "[DX9] Failed to retrieve game surface: " << result << std::endl;
		return;
	}
}
