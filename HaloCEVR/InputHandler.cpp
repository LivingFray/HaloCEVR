#include "InputHandler.h"
#include "Game.h"
#include "Helpers/Controls.h"
#include "Helpers/Camera.h"
#include "Helpers/Menus.h"
#include "Helpers/Maths.h"
#include "Helpers/Objects.h"

#include <chrono>
#include <cmath>


#define RegisterBoolInput(set, x) x = vr->RegisterBoolInput(set, #x);
#define RegisterVector2Input(set, x) x = vr->RegisterVector2Input(set, #x);
#define ApplyBoolInput(x) controls.##x = vr->GetBoolInput(x) ? 127 : 0;
#define ApplyImpulseBoolInput(x) controls.##x = vr->GetBoolInput(x, bHasChanged) && bHasChanged ? 127 : 0;

namespace
{
	constexpr float MP_MOVE_DEADZONE = 0.25f;

	// DirectInput reads scan codes better than virtual keys in Halo CE.
	constexpr WORD SC_W = 0x11;
	constexpr WORD SC_A = 0x1E;
	constexpr WORD SC_S = 0x1F;
	constexpr WORD SC_D = 0x20;

	bool g_mpWDown = false;
	bool g_mpADown = false;
	bool g_mpSDown = false;
	bool g_mpDDown = false;

	void SetScanCodeHeld(WORD scanCode, bool shouldBeDown, bool& isDown)
	{
		if (shouldBeDown == isDown)
		{
			return;
		}

		INPUT input{};
		input.type = INPUT_KEYBOARD;
		input.ki.dwFlags = KEYEVENTF_SCANCODE;
		input.ki.wScan = scanCode;

		if (!shouldBeDown)
		{
			input.ki.dwFlags |= KEYEVENTF_KEYUP;
		}

		SendInput(1, &input, sizeof(INPUT));
		isDown = shouldBeDown;
	}

	void ReleaseMPDigitalMoveKeys()
	{
		SetScanCodeHeld(SC_W, false, g_mpWDown);
		SetScanCodeHeld(SC_A, false, g_mpADown);
		SetScanCodeHeld(SC_S, false, g_mpSDown);
		SetScanCodeHeld(SC_D, false, g_mpDDown);
	}

	void ApplyMPDigitalMoveKeys(Vector2& moveInput)
	{
		// SteamVR Move axis: x = right, y = forward after any hand-relative rotation.
		const bool wantForward = moveInput.y > MP_MOVE_DEADZONE;
		const bool wantBack = moveInput.y < -MP_MOVE_DEADZONE;
		const bool wantLeft = moveInput.x < -MP_MOVE_DEADZONE;
		const bool wantRight = moveInput.x > MP_MOVE_DEADZONE;

		// Allow diagonals by holding both axes at once, e.g. W + A.
		SetScanCodeHeld(SC_W, wantForward, g_mpWDown);
		SetScanCodeHeld(SC_S, wantBack, g_mpSDown);
		SetScanCodeHeld(SC_A, wantLeft, g_mpADown);
		SetScanCodeHeld(SC_D, wantRight, g_mpDDown);

		// Feed full digital values into Halo's controls struct for consistency.
		moveInput.x = (wantRight ? 1.0f : 0.0f) + (wantLeft ? -1.0f : 0.0f);
		moveInput.y = (wantForward ? 1.0f : 0.0f) + (wantBack ? -1.0f : 0.0f);
	}

	bool IsValidHaloID(const HaloID& id)
	{
		return id.index != 0xFFFF && id.id != 0xFFFF;
	}

	constexpr float PI_F = 3.14159265358979323846f;
	constexpr float TWO_PI_F = 6.28318530717958647692f;

	float NormalizeRadians(float radians)
	{
		while (radians > PI_F)
		{
			radians -= TWO_PI_F;
		}

		while (radians < -PI_F)
		{
			radians += TWO_PI_F;
		}

		return radians;
	}

	UnitDynamicObject* ResolveMovementBiped()
	{
		HaloID selectedID;
		if (!Helpers::GetLocalPlayerID(selectedID))
		{
			return nullptr;
		}

		BaseDynamicObject* selectedObj = Helpers::GetDynamicObject(selectedID);
		if (!selectedObj || selectedObj->N0000027E != ObjectType::BIPED)
		{
			return nullptr;
		}

		UnitDynamicObject* selectedUnit = reinterpret_cast<UnitDynamicObject*>(selectedObj);

		// Prefer the held weapon's parent biped. Multiplayer can expose a predicted
		// local biped separately from the player-table entry.
		HaloID weaponID = selectedUnit->weapon;
		if (!IsValidHaloID(weaponID))
		{
			weaponID = selectedObj->weapon;
		}

		BaseDynamicObject* weaponObj = Helpers::GetDynamicObject(weaponID);
		if (weaponObj)
		{
			BaseDynamicObject* parentObj = Helpers::GetDynamicObject(weaponObj->parent);
			if (parentObj && parentObj->N0000027E == ObjectType::BIPED)
			{
				return reinterpret_cast<UnitDynamicObject*>(parentObj);
			}
		}

		return selectedUnit;
	}

	void ApplyMPDirectBipedMovement(const Vector2& moveInput)
	{
		UnitDynamicObject* unit = ResolveMovementBiped();

		if (!unit)
		{
			return;
		}

		// Same sign convention as the existing Controls write:
		// controls.Left = -MoveInput.x; controls.Forwards = MoveInput.y
		unit->run = moveInput.y;
		unit->strafe = -moveInput.x;
	}

	void ApplySoloAnalogDiagonalMovement(Vector2& moveInput)
	{
		// Keep the solo path genuinely analog. SteamVR normally supplies a circular
		// unit vector, but cap unusual square bindings without changing direction.
		const float inputMagnitude = moveInput.length();
		if (inputMagnitude > 1.0f)
		{
			moveInput /= inputMagnitude;
		}

		UnitDynamicObject* unit = ResolveMovementBiped();
		if (!unit)
		{
			return;
		}

		// Hooks::H_HandleInputs calls Halo first, then the VR handler. Update all
		// movement surfaces after Halo's original pass so neither axis is discarded.
		// Unlike the MP workaround, this does not inject W/A/S/D or quantize values.
		InputData& inputData = Helpers::GetInputData();
		inputData.forward = moveInput.y;
		inputData.left = -moveInput.x;
		unit->run = moveInput.y;
		unit->strafe = -moveInput.x;
	}
}


void InputHandler::RegisterInputs()
{
	IVR* vr = Game::instance.GetVR();

	// These bindings will always be the same and won't change depending on action sets
	RegisterBoolInput("left handed", SwapWeaponHand);
	OffhandSwapWeaponHand = SwapWeaponHand;
	RegisterBoolInput("default", SwapWeaponHand);

	UpdateRegisteredInputs();
}

void InputHandler::UpdateRegisteredInputs()
{
	IVR* vr = Game::instance.GetVR();

	const char* actionSet = Game::instance.bLeftHanded ? "left handed" : "default";

	// These bindings will change depending on action sets
	RegisterBoolInput(actionSet, Jump);
	RegisterBoolInput(actionSet, SwitchGrenades);
	RegisterBoolInput(actionSet, Interact);
	RegisterBoolInput(actionSet, SwitchWeapons);
	RegisterBoolInput(actionSet, Melee);
	RegisterBoolInput(actionSet, Flashlight);
	RegisterBoolInput(actionSet, Grenade);
	RegisterBoolInput(actionSet, Fire);
	RegisterBoolInput(actionSet, MenuForward);
	RegisterBoolInput(actionSet, MenuBack);
	RegisterBoolInput(actionSet, Crouch);
	RegisterBoolInput(actionSet, Zoom);
	RegisterBoolInput(actionSet, Reload);
	RegisterBoolInput(actionSet, TwoHandGrip);

	RegisterVector2Input(actionSet, Move);
	RegisterVector2Input(actionSet, Look);
}

float AngleBetweenVector2(const Vector2& v1, const Vector2& v2)
{
	const float dot = v1.dot(v2);
	const float determinant = v1.x * v2.y - v1.y * v2.x;
	const float angle = atan2(determinant, dot);
	return angle;
}

Vector2 RotateVector2(const Vector2& v, float angle)
{
	Vector2 rotated;
	rotated.x = v.x * cos(angle) - v.y * sin(angle);
	rotated.y = v.x * sin(angle) + v.y * cos(angle);
	return rotated;
}

static bool GetConfiguredMovementForward(Vector3& outForward)
{
	IVR* vr = Game::instance.GetVR();
	const int movementMode = Game::instance.c_HandRelativeMovement->Value();

	if (movementMode == 1 || movementMode == 2)
	{
		const bool leftHand = movementMode == 1;
		const ControllerRole role = leftHand ? ControllerRole::Left : ControllerRole::Right;
		Matrix4 controllerTransform = vr->GetControllerTransform(role);
		const float offset = Game::instance.c_HandRelativeOffsetRotation->Value();
		controllerTransform.rotateZ(leftHand ? offset : -offset);
		outForward = controllerTransform.getLeftAxis();
	}
	else
	{
		// Mode 0 is explicitly HMD-relative. Do not derive this direction from
		// Halo's camera: multiplayer network aim intentionally points Halo's
		// authoritative aim at the weapon for correct remote firing.
		outForward = vr->GetHMDTransform().getLeftAxis();
	}

	outForward.z = 0.0f;
	return outForward.lengthSqr() > 0.000001f;
}

static bool ApplyMultiplayerMovementBasis(Vector2& moveInput)
{
	Vector3 desiredForward;
	if (!GetConfiguredMovementForward(desiredForward))
	{
		return false;
	}

	Vector3 weaponPosition;
	Vector3 commandForward;
	Vector3 weaponUp;
	const bool bUsingWeaponAim = Game::instance.GetWorldWeaponAim(
		weaponPosition, commandForward, weaponUp);
	commandForward.z = 0.0f;

	if (!bUsingWeaponAim || commandForward.lengthSqr() <= 0.000001f)
	{
		// Spawn/weapon transitions temporarily lack a weapon basis. During those
		// frames UpdateCamera also falls back to the game camera/HMD path.
		commandForward = Game::instance.bDetectedChimera
			? Game::instance.LastLookDir
			: Helpers::GetCamera().lookDir;
		commandForward.z = 0.0f;
	}

	if (commandForward.lengthSqr() <= 0.000001f)
	{
		return false;
	}

	const Vector2 rawMoveInput = moveInput;
	// Give the joined-client roomscale servo the user's locomotion before the
	// weapon-command counter-rotation and digital W/A/S/D transport.
	Vector3 manualWorldInput(0.0f, 0.0f, 0.0f);
	Vector3 normalizedDesiredForward = desiredForward;
	if (normalizedDesiredForward.lengthSqr() > 0.000001f)
	{
		normalizedDesiredForward.normalize();
		const Vector3 desiredRight(
			normalizedDesiredForward.y,
			-normalizedDesiredForward.x,
			0.0f);
		manualWorldInput = normalizedDesiredForward * rawMoveInput.y +
			desiredRight * rawMoveInput.x;
	}
	Game::instance.SetMPRoomScaleManualWorldInput(manualWorldInput);

	const float angle = AngleBetweenVector2(
		Vector2(commandForward.x, commandForward.y),
		Vector2(desiredForward.x, desiredForward.y));

	if (!std::isfinite(angle))
	{
		return false;
	}

	moveInput = RotateVector2(moveInput, angle);

	return true;
}

#define DRAW_DEBUG_MOVE 0

void InputHandler::UpdateInputs(bool bInVehicle)
{
	IVR* vr = Game::instance.GetVR();

	vr->UpdateInputs();

	static bool bHasChanged = false;

	if (Game::instance.bIsCustom)
	{
		Controls_Custom& controls = Helpers::GetControlsCustom();

		ApplyBoolInput(Jump);
		ApplyImpulseBoolInput(SwitchGrenades);
		ApplyBoolInput(Interact);
		ApplyBoolInput(Melee);
		ApplyBoolInput(Flashlight);
		ApplyBoolInput(Grenade);
		ApplyBoolInput(Fire);
		ApplyBoolInput(MenuForward);
		ApplyBoolInput(MenuBack);
		ApplyBoolInput(Crouch);
		ApplyImpulseBoolInput(Zoom);
		ApplyBoolInput(Reload);

		if (bInVehicle &&
			Game::instance.c_UseJumpForVehicleSecondaryFire &&
			Game::instance.c_UseJumpForVehicleSecondaryFire->Value())
		{
			// Halo reuses the grenade control as vehicle secondary fire. Replace it
			// with the Jump binding while seated, rather than activating both.
			controls.Grenade = controls.Jump;
			controls.Jump = 0;
		}

		Game::instance.bIsFiring = controls.Fire;
	}
	else
	{
		Controls& controls = Helpers::GetControls();

		ApplyBoolInput(Jump);
		ApplyImpulseBoolInput(SwitchGrenades);
		ApplyBoolInput(Interact);
		ApplyBoolInput(Melee);
		ApplyBoolInput(Flashlight);
		ApplyBoolInput(Grenade);
		ApplyBoolInput(Fire);
		ApplyBoolInput(MenuForward);
		ApplyBoolInput(MenuBack);
		ApplyBoolInput(Crouch);
		ApplyImpulseBoolInput(Zoom);
		ApplyBoolInput(Reload);

		if (bInVehicle &&
			Game::instance.c_UseJumpForVehicleSecondaryFire &&
			Game::instance.c_UseJumpForVehicleSecondaryFire->Value())
		{
			// Halo reuses the grenade control as vehicle secondary fire. Replace it
			// with the Jump binding while seated, rather than activating both.
			controls.Grenade = controls.Jump;
			controls.Jump = 0;
		}

		Game::instance.bIsFiring = controls.Fire;
	}


	unsigned char MotionControlFlashlight = UpdateFlashlight();
	if (MotionControlFlashlight > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Flashlight = MotionControlFlashlight;
		}
		else
		{
			Helpers::GetControls().Flashlight = MotionControlFlashlight;
		}
	}

	if (Game::instance.c_EnableWeaponHolsters->Value())
	{
		unsigned char HolsterSwitchWeapons = UpdateHolsterSwitchWeapons();
		bool bSwitchWeaponsPressed = vr->GetBoolInput(SwitchWeapons);

		if (HolsterSwitchWeapons > 0 && bSwitchWeaponsPressed)
		{
			if (Game::instance.bIsCustom)
			{
				Controls_Custom& controls = Helpers::GetControlsCustom();
				ApplyImpulseBoolInput(SwitchWeapons);
			}
			else
			{
				Controls& controls = Helpers::GetControls();
				ApplyImpulseBoolInput(SwitchWeapons);
			}
		}
	}
	else
	{
		if (Game::instance.bIsCustom)
		{
			Controls_Custom& controls = Helpers::GetControlsCustom();
			ApplyImpulseBoolInput(SwitchWeapons);
		}
		else
		{
			Controls& controls = Helpers::GetControls();
			ApplyImpulseBoolInput(SwitchWeapons);
		}
	}

	unsigned char MotionControlMelee = UpdateMelee();
	if (MotionControlMelee > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Melee = MotionControlMelee;
		}
		else
		{
			Helpers::GetControls().Melee = MotionControlMelee;
		}
	}

	// Use Halo's final melee control after button and gesture inputs are merged
	// so either activation method starts the campaign weapon-aim latch.
	const bool bMeleePressed =
		Game::instance.bIsCustom
			? Helpers::GetControlsCustom().Melee > 0
			: Helpers::GetControls().Melee > 0;
	Game::instance.UpdateSinglePlayerMeleeAim(bMeleePressed);

	unsigned char MotionControlCrouch = UpdateCrouch();
	if (MotionControlCrouch > 0)
	{
		if (Game::instance.bIsCustom)
		{
			Helpers::GetControlsCustom().Crouch = MotionControlCrouch;
		}
		else
		{
			Helpers::GetControls().Crouch = MotionControlCrouch;
		}
	}

	const float holdToRecentreTime = 1000.0f;

	bool bMenuChanged;
	bool bMenuPressed = vr->GetBoolInput(MenuBack, bMenuChanged);

	if (bMenuPressed)
	{
		if (bMenuChanged)
		{
			menuHeldTime = std::chrono::high_resolution_clock::now();
		}

		float heldTime = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - menuHeldTime).count();

		if (heldTime > 100.0f)
		{
			float progress = std::min(heldTime, holdToRecentreTime) / holdToRecentreTime;

			Matrix4 handTrans = vr->GetControllerTransform(ControllerRole::Left, true);

			Vector3 handPos = (handTrans * Vector3(0.0f, 0.0f, 0.0f)) * Game::instance.MetresToWorld(1.0f) + Helpers::GetCamera().position;

			Vector3 centre = handPos + Vector3(0.0f, 0.0f, Game::instance.MetresToWorld(0.1f));
			Vector3 facing = -handTrans.getLeftAxis();
			Vector3 upVector = handTrans.getForwardAxis();

			Game::instance.inGameRenderer.DrawPolygon(centre, facing, upVector, 16, Game::instance.MetresToWorld(0.01f), D3DCOLOR_XRGB(255, 0, 0), false, progress);
		}
	}
	else if (bMenuChanged)
	{
		float heldTime = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - menuHeldTime).count();

		if (heldTime > holdToRecentreTime)
		{
			Game::instance.bNeedsRecentre = true;
		}
		else
		{
			INPUT input{};
			input.type = INPUT_KEYBOARD;
			input.ki.dwFlags = KEYEVENTF_SCANCODE; // DirectInput only detects scancodes
			input.ki.wScan = 01; // Escape
			SendInput(1, &input, sizeof(INPUT));

			bHoldingMenu = true;
		}
	}
	else if (bHoldingMenu)
	{
		bHoldingMenu = false;

		INPUT input{};
		input.type = INPUT_KEYBOARD;
		input.ki.dwFlags = KEYEVENTF_SCANCODE; // DirectInput only detects scancodes
		input.ki.wScan = 01; // Escape
		input.ki.dwFlags |= KEYEVENTF_KEYUP;
		SendInput(1, &input, sizeof(INPUT));
	}

	UpdateHandsProximity();

	// Clear the sample first so failed basis resolution, vehicles and non-client
	// paths can never reuse a stick direction from an earlier frame.
	Game::instance.SetMPRoomScaleManualWorldInput(Vector3(0.0f, 0.0f, 0.0f));
	Vector2 MoveInput = vr->GetVector2Input(Move);
	// Use Halo's actual network role for movement. A one-player lobby host reports
	// multiplayer even though the player table contains only one entry, so a
	// player-count gate cannot reliably select the digital diagonal transport.
	// UpdateCamera uses this same role detector too. That matters in an empty lobby:
	// the player table still has one entry, but Halo is already using its multiplayer
	// command/aim basis and must not fall back to the campaign HMD camera path.
	const bool bUseMPMovementPatch = !bInVehicle &&
		Game::instance.IsMultiplayerSession();
	const bool bUseMultiplayerMovementBasis = bUseMPMovementPatch;

	if (bUseMultiplayerMovementBasis)
	{
		// Both a local host and a joined client submit multiplayer movement in the
		// controller/weapon aim basis used for hit registration. Convert the user's
		// configured HMD/hand-relative stick into that command basis for either role.
		// This changes locomotion only; UpdateCamera retains controller aiming.
		ApplyMultiplayerMovementBasis(MoveInput);
	}
	else if (!bInVehicle && (Game::instance.c_HandRelativeMovement->Value() != 0))
	{
		Camera& cam = Helpers::GetCamera();
		Vector3 camForward = cam.lookDir;

		const bool leftHand = (Game::instance.c_HandRelativeMovement->Value() == 1);
		const ControllerRole role = leftHand ? ControllerRole::Left : ControllerRole::Right;
		Matrix4 controllerTransform = vr->GetControllerTransform(role); // TODO: Not sure about bRenderPose bool param here. Doesn't seem to matter.
		const float offset = Game::instance.c_HandRelativeOffsetRotation->Value();
		controllerTransform.rotateZ(leftHand ? offset : -offset);
		Vector3 handForward = controllerTransform.getLeftAxis();

#if DRAW_DEBUG_MOVE
		Vector3 start = cam.position + cam.lookDirUp * -Game::instance.MetresToWorld(0.1f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + camForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 255, 0, 0), false, 0.5f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + handForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 0, 0, 255), false, 0.5f);
		handForward.z = camForward.z = 0.0f;
		Game::instance.inGameRenderer.DrawLine3D(start, start + camForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 255, 75, 0), false, 0.5f);
		Game::instance.inGameRenderer.DrawLine3D(start, start + handForward * Game::instance.MetresToWorld(2.0f), D3DCOLOR_ARGB(255, 100, 0, 255), false, 0.5f);

		start = cam.position + cam.lookDir * Game::instance.MetresToWorld(2.00f);
		const Vector3 camRight = cam.lookDir.cross(cam.lookDirUp);
		Vector3 end = start + cam.lookDirUp * MoveInput.y * Game::instance.MetresToWorld(1.0f) + camRight * MoveInput.x * Game::instance.MetresToWorld(1.0f);
		Game::instance.inGameRenderer.DrawLine3D(start, end, D3DCOLOR_ARGB(255, 0, 255, 0), false, 0.5f);
#endif

		const float angle = AngleBetweenVector2(Vector2(camForward.x, camForward.y), Vector2(handForward.x, handForward.y));
		MoveInput = RotateVector2(MoveInput, angle);

#if DRAW_DEBUG_MOVE
		end = start + cam.lookDirUp * MoveInput.y * Game::instance.MetresToWorld(1.0f) + camRight * MoveInput.x * Game::instance.MetresToWorld(1.0f);
		Game::instance.inGameRenderer.DrawLine3D(start, end, D3DCOLOR_ARGB(255, 255, 255, 0), false, 0.5f);
#endif
	}

	// Multiplayer uses digital diagonals; single-player keeps analog movement.
	if (bUseMPMovementPatch)
	{
		// Apply basis compensation before quantizing both movement axes.
		ApplyMPDigitalMoveKeys(MoveInput);
		ApplyMPDirectBipedMovement(MoveInput);
	}
	else
	{
		ReleaseMPDigitalMoveKeys();

		if (!bInVehicle)
		{
			// Campaign and non-network play retain the raw analog vector. A one-player
			// multiplayer host takes the digital path above.
			ApplySoloAnalogDiagonalMovement(MoveInput);
		}
	}

	if (Game::instance.bIsCustom)
	{
		Controls_Custom& controls = Helpers::GetControlsCustom();

		controls.Left = -MoveInput.x;
		controls.Forwards = MoveInput.y;
	}
	else
	{
		Controls& controls = Helpers::GetControls();

		controls.Left = -MoveInput.x;
		controls.Forwards = MoveInput.y;
	}
}

void InputHandler::UpdateCamera(float& yaw, float& pitch, bool bArmedVehicle)
{
	IVR* vr = Game::instance.GetVR();

	Vector2 lookInput = vr->GetVector2Input(Look);

	float yawOffset = vr->GetYawOffset();

	if (Game::instance.c_SnapTurn->Value())
	{
		if (lastSnapState == 1)
		{
			if (lookInput.x < 0.4f)
			{
				lastSnapState = 0;
			}
		}
		else if (lastSnapState == -1)
		{
			if (lookInput.x > -0.4f)
			{
				lastSnapState = 0;
			}
		}
		else
		{
			if (lookInput.x > 0.6f)
			{
				lastSnapState = 1;
				yawOffset += Game::instance.c_SnapTurnAmount->Value();
			}
			else if (lookInput.x < -0.6f)
			{
				lastSnapState = -1;
				yawOffset -= Game::instance.c_SnapTurnAmount->Value();
			}
		}
	}
	else
	{
		yawOffset += lookInput.x * Game::instance.c_SmoothTurnAmount->Value() * Game::instance.lastDeltaTime;
	}

	vr->SetYawOffset(yawOffset);

	// Passenger camera updates occur on Halo's simulation cadence. Keep the base
	// camera stable while the HMD renderer and WeaponHandler provide free-look and aim.
	const bool bUseArmedVehicleSampleGate = bArmedVehicle;

	// Ignore stale vehicle-camera state during the first on-foot exit frames.
	static bool bWasUsingArmedVehicleSampleGate = false;
	static float armedVehicleExitGuardRemaining = 0.0f;
	static float armedVehicleExitPoseHoldRemaining = 0.0f;
	static Camera lastStableArmedVehicleCamera{};
	static Vector3 lastStableArmedVehicleLastLookDir{};
	static bool bHaveStableArmedVehicleCamera = false;

	float cameraDeltaTime = Game::instance.lastDeltaTime;
	if (!std::isfinite(cameraDeltaTime) || cameraDeltaTime < 0.0f)
	{
		cameraDeltaTime = 0.0f;
	}
	else if (cameraDeltaTime > 0.1f)
	{
		cameraDeltaTime = 0.1f;
	}

	if (bUseArmedVehicleSampleGate)
	{
		const Camera& camera = Helpers::GetCamera();
		const bool bFiniteCamera =
			std::isfinite(camera.position.x) &&
			std::isfinite(camera.position.y) &&
			std::isfinite(camera.position.z) &&
			std::isfinite(camera.lookDir.x) &&
			std::isfinite(camera.lookDir.y) &&
			std::isfinite(camera.lookDir.z) &&
			camera.lookDir.lengthSqr() > 0.000001f;

		if (bFiniteCamera)
		{
			lastStableArmedVehicleCamera = camera;
			lastStableArmedVehicleLastLookDir =
				Game::instance.LastLookDir;
			bHaveStableArmedVehicleCamera = true;
		}
	}

	if (bWasUsingArmedVehicleSampleGate && !bUseArmedVehicleSampleGate)
	{
		// Hold the last valid armed-seat camera briefly while the exit view settles.
		armedVehicleExitGuardRemaining = 0.90f;
		armedVehicleExitPoseHoldRemaining = 0.15f;
	}

	bWasUsingArmedVehicleSampleGate = bUseArmedVehicleSampleGate;

	if (!bUseArmedVehicleSampleGate && armedVehicleExitGuardRemaining > 0.0f)
	{
		armedVehicleExitGuardRemaining -= cameraDeltaTime;
		if (armedVehicleExitGuardRemaining < 0.0f)
		{
			armedVehicleExitGuardRemaining = 0.0f;
		}

		if (armedVehicleExitPoseHoldRemaining > 0.0f)
		{
			armedVehicleExitPoseHoldRemaining -= cameraDeltaTime;
			if (armedVehicleExitPoseHoldRemaining < 0.0f)
			{
				armedVehicleExitPoseHoldRemaining = 0.0f;
			}

			if (bHaveStableArmedVehicleCamera)
			{
				Camera& camera = Helpers::GetCamera();

				const bool bFinitePosition =
					std::isfinite(camera.position.x) &&
					std::isfinite(camera.position.y) &&
					std::isfinite(camera.position.z);
				const float positionJumpSqr = bFinitePosition
					? (camera.position -
						lastStableArmedVehicleCamera.position).lengthSqr()
					: 99999999.0f;

				// Do not freeze normal exit translation. Clamp only a clearly invalid
				// position or a jump of more than three world units in these first frames.
				if (!bFinitePosition || positionJumpSqr > 9.0f)
				{
					camera.position =
						lastStableArmedVehicleCamera.position;
				}

				camera.lookDir =
					lastStableArmedVehicleCamera.lookDir;
				camera.lookDirUp =
					lastStableArmedVehicleCamera.lookDirUp;
				Game::instance.LastLookDir =
					lastStableArmedVehicleLastLookDir;
			}
		}

		bArmedVehicleCameraSampleValid = false;
		yaw = 0.0f;
		pitch = 0.0f;
		return;
	}

	// Chimera's interpolated frustum is the best on-foot comparison direction.
	// An armed vehicle seat is different: Halo updates its authoritative base
	// camera only on the simulation cadence while this hook can run every rendered
	// frame. Use the non-interpolated camera direction there so one sample can be
	// recognized and consumed exactly once.
	Vector3 lookGame = bUseArmedVehicleSampleGate
		? Helpers::GetCamera().lookDir
		: (Game::instance.bDetectedChimera
			? Game::instance.LastLookDir
			: Helpers::GetCamera().lookDir);

	if (bUseArmedVehicleSampleGate)
	{
		const bool bFiniteSample =
			std::isfinite(lookGame.x) &&
			std::isfinite(lookGame.y) &&
			std::isfinite(lookGame.z) &&
			lookGame.lengthSqr() > 0.000001f;

		if (!bFiniteSample)
		{
			// Never feed invalid vehicle-camera state into Halo's view controller.
			bArmedVehicleCameraSampleValid = false;
			yaw = 0.0f;
			pitch = 0.0f;
			return;
		}

		const bool bRepeatedSample = bArmedVehicleCameraSampleValid &&
			(lookGame - armedVehicleCameraSample).lengthSqr() <= 0.0000000001f;
		if (bRepeatedSample)
		{
			// The previous absolute correction is already queued for this simulation
			// sample. Reapplying it two or three more times caused the passenger-seat
			// camera and network projectile direction to diverge explosively.
			yaw = 0.0f;
			pitch = 0.0f;
			return;
		}

		armedVehicleCameraSample = lookGame;
		bArmedVehicleCameraSampleValid = true;
	}
	else
	{
		bArmedVehicleCameraSampleValid = false;
	}

	// The exact armed-passenger hook writes unrestricted controller aim after
	// Halo's original UpdatePitchYaw has applied its desktop seat arc. Keep the
	// passenger camera HMD-relative: the HMD renderer already supplies free-look,
	// while feeding weapon deltas here makes the base camera repeatedly collide
	// with that desktop arc and produces the stepped/jumping feel.
	if (bUseArmedVehicleSampleGate)
	{
		yaw = 0.0f;
		pitch = 0.0f;
		return;
	}

	// Update multiplayer network aim continuously while on foot.
	const bool bUseMPNetworkAim = Game::instance.IsMultiplayerSession();

	if (bUseMPNetworkAim)
	{
		Vector3 weaponPosition;
		Vector3 weaponAim;
		Vector3 weaponUp;

		if (Game::instance.GetWorldWeaponAim(weaponPosition, weaponAim, weaponUp) && weaponAim.lengthSqr() > 1e-8f)
		{
			weaponAim.normalize();

			const float yawWeapon = atan2(weaponAim.y, weaponAim.x);
			const float yawGame = atan2(lookGame.y, lookGame.x);
			yaw = NormalizeRadians(yawWeapon - yawGame);

			const float pitchWeapon = atan2(weaponAim.z, sqrt(weaponAim.x * weaponAim.x + weaponAim.y * weaponAim.y));
			const float pitchGame = atan2(lookGame.z, sqrt(lookGame.x * lookGame.x + lookGame.y * lookGame.y));
			pitch = NormalizeRadians(pitchWeapon - pitchGame);

			// Do not mirror this aim into UnitDynamicObject fields. Doing so makes
			// remote player models follow the local VR controller pose.

			return;
		}
	}

	Vector3 lookHMD = vr->GetHMDTransform().getLeftAxis();

	// During the campaign melee-impact window, drive Halo's hidden desktop aim
	// from the weapon while per-eye rendering stays head-tracked. Multiplayer,
	// vehicle and cutscene paths are rejected by the getter.
	Vector3 singlePlayerMeleeAim;
	if (Game::instance.GetSinglePlayerMeleeAim(singlePlayerMeleeAim))
	{
		lookHMD = singlePlayerMeleeAim;
	}

	// Apply deltas from the HMD in single player / fallback cases.
	float yawHMD = atan2(lookHMD.y, lookHMD.x);
	float yawGame = atan2(lookGame.y, lookGame.x);
	yaw = NormalizeRadians(yawHMD - yawGame);

	float pitchHMD = atan2(lookHMD.z, sqrt(lookHMD.x * lookHMD.x + lookHMD.y * lookHMD.y));
	float pitchGame = atan2(lookGame.z, sqrt(lookGame.x * lookGame.x + lookGame.y * lookGame.y));
	pitch = NormalizeRadians(pitchHMD - pitchGame);
}

void InputHandler::UpdateCameraForVehicles(float& yaw, float& pitch)
{
	IVR* vr = Game::instance.GetVR();
	Vector2 lookInput = vr->GetVector2Input(Look);

	const float DegToRad = 3.141593f / 180.0f;

	float frameSeconds = Game::instance.lastDeltaTime;
	if (!std::isfinite(frameSeconds) || frameSeconds <= 0.0f)
	{
		frameSeconds = 1.0f / 90.0f;
	}
	else if (frameSeconds > 0.05f)
	{
		frameSeconds = 0.05f;
	}

	const float YawDelta =
		lookInput.x *
		Game::instance.c_HorizontalVehicleTurnAmount->Value() *
		frameSeconds;
	const float PitchDelta =
		lookInput.y *
		Game::instance.c_VerticalVehicleTurnAmount->Value() *
		frameSeconds;

	float yawOffset = vr->GetYawOffset();
	yawOffset += YawDelta;
	vr->SetYawOffset(yawOffset);

	// The visible camera always receives Halo's native right-stick input.
	// Motion-controller aim is applied separately after the native camera pass.
	yaw = -DegToRad * YawDelta;
	pitch = DegToRad * PitchDelta;
}

unsigned char InputHandler::UpdateFlashlight()
{
	IVR* vr = Game::instance.GetVR();

	Vector3 headPos = vr->GetHMDTransform() * Vector3(-0.1f, 0.0f, 0.0f);

	float leftDistance = Game::instance.c_LeftHandFlashlightDistance->Value();
	float rightDistance = Game::instance.c_RightHandFlashlightDistance->Value();

	bool offhandFlashlightEnabled = Game::instance.c_OffhandHandFlashlight->Value();

	bool checkLeftHand = !offhandFlashlightEnabled || !Game::instance.bLeftHanded;
	if (checkLeftHand && leftDistance > 0.0f)
	{
		Vector3 handPos = vr->GetRawControllerTransform(ControllerRole::Left) * Vector3(0.0f, 0.0f, 0.0f);

		if ((headPos - handPos).lengthSqr() < leftDistance * leftDistance)
		{
			return 127;
		}
	}

	bool checkRightHand = !offhandFlashlightEnabled || Game::instance.bLeftHanded;
	if (checkRightHand && rightDistance > 0.0f)
	{
		Vector3 handPos = vr->GetRawControllerTransform(ControllerRole::Right) * Vector3(0.0f, 0.0f, 0.0f);

		if ((headPos - handPos).lengthSqr() < rightDistance * rightDistance)
		{
			return 127;
		}
	}

	return 0;
}

unsigned char InputHandler::UpdateHolsterSwitchWeapons()
{
	IVR* vr = Game::instance.GetVR();

	Matrix4 headTransform = vr->GetHMDTransform();

	// Calculate shoulder holster positions with the correct offset
	Vector3 leftShoulderPos = headTransform * Game::instance.c_LeftShoulderHolsterOffset->Value();
	Vector3 rightShoulderPos = headTransform * Game::instance.c_RightShoulderHolsterOffset->Value();

	Vector3 handPos;
	if (Game::instance.bLeftHanded)
	{
		handPos = vr->GetRawControllerTransform(ControllerRole::Left) * Vector3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		handPos = vr->GetRawControllerTransform(ControllerRole::Right) * Vector3(0.0f, 0.0f, 0.0f);
	}

	if (InputHandler::IsHandInHolster(handPos, leftShoulderPos, Game::instance.c_LeftShoulderHolsterActivationDistance->Value())
		|| InputHandler::IsHandInHolster(handPos, rightShoulderPos, Game::instance.c_RightShoulderHolsterActivationDistance->Value()))
	{
		return 127;
	}

	return 0;
}

// Helper function to check if a hand is in a holster
bool InputHandler::IsHandInHolster(const Vector3& handPos, const Vector3& holsterPos, const float& holsterActivationDistance)
{
	return (holsterPos - handPos).lengthSqr() < holsterActivationDistance * holsterActivationDistance;
}

unsigned char InputHandler::UpdateMelee()
{
	IVR* vr = Game::instance.GetVR();

	Vector3 handVel = vr->GetControllerVelocity(ControllerRole::Left);

	handVel *= Game::instance.WorldToMetres(1.0f);

	if (Game::instance.c_LeftHandMeleeSwingSpeed->Value() > 0.0f && abs(handVel.z) > Game::instance.c_LeftHandMeleeSwingSpeed->Value())
	{
		return 127;
	}

	handVel = vr->GetControllerVelocity(ControllerRole::Right);

	handVel *= Game::instance.WorldToMetres(1.0f);

	if (Game::instance.c_RightHandMeleeSwingSpeed->Value() > 0.0f && abs(handVel.z) > Game::instance.c_RightHandMeleeSwingSpeed->Value())
	{
		return 127;
	}

	return 0;
}

unsigned char InputHandler::UpdateCrouch()
{
	IVR* vr = Game::instance.GetVR();

	Vector3 headPos = vr->GetHMDTransform() * Vector3(0.0f, 0.0f, 0.0f);

	float crouchHeight = Game::instance.c_CrouchHeight->Value();

	if (crouchHeight < 0)
	{
		return 0;
	}

	if (headPos.z < -crouchHeight)
	{
		return 127;
	}

	return 0;
}

void InputHandler::SetMousePosition(int& x, int& y)
{
	Vector2 mousePos = Game::instance.GetVR()->GetMousePos();
	// Best I can tell the mouse is always scaled to a 640x480 canvas
	x = static_cast<int>(mousePos.x * 640);
	y = static_cast<int>(mousePos.y * 480);
}

void InputHandler::UpdateMouseInfo(MouseInfo* mouseInfo)
{
	if (Game::instance.GetVR()->GetMouseDown())
	{
		if (mouseDownState < 255)
		{
			mouseDownState++;
		}
	}
	else
	{
		mouseDownState = 0;
	}

	mouseInfo->buttonState[0] = mouseDownState;
}

bool InputHandler::GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand)
{
	ControllerRole dominant = Game::instance.bLeftHanded ? ControllerRole::Left : ControllerRole::Right;
	ControllerRole nonDominant = Game::instance.bLeftHanded ? ControllerRole::Right : ControllerRole::Left;

	controllerTransform = Game::instance.GetVR()->GetControllerTransform(dominant, true);

	Vector3 poseDirection;
	bool bHasPoseData = Game::instance.GetVR()->TryGetControllerFacing(dominant, poseDirection);

	// When 2h aiming point the main hand at the offhand
	if (Game::instance.bUseTwoHandAim || bHasPoseData)
	{
		Matrix4 aimingTransform = Game::instance.GetVR()->GetRawControllerTransform(dominant, true);
		Matrix4 offHandTransform = Game::instance.GetVR()->GetRawControllerTransform(nonDominant, true);

		const Vector3 actualControllerPos = controllerTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 mainHandPos = aimingTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 offHandPos = Game::instance.bUseTwoHandAim ? offHandTransform * Vector3(0.0f, 0.0f, 0.0f) : mainHandPos + poseDirection;
		Vector3 toOffHand = (offHandPos - mainHandPos);

		// Avoid NaN errors
		if (toOffHand.lengthSqr() < 1e-8)
		{
			return false;
		}

		toOffHand.normalize();
		dominantHandPos = actualControllerPos;
		offHand = toOffHand;

		return true;
	}

	return false;
}

void InputHandler::CalculateSmoothedInput()
{
	Matrix4 controllerTransform;
	Vector3 actualControllerPos;
	Vector3 toOffHand;

	if (!GetCalculatedHandPositions(controllerTransform, actualControllerPos, toOffHand))
	{
		return;
	}

	if (!bLastYawInitialized)
	{
		lastSmoothingYawOffset = Game::instance.GetVR()->GetYawOffset();
		bLastYawInitialized = true;
	}

	// Detect snap turns and rotate smoothed position to prevent lerping artifact
	float currentYawOffset = Game::instance.GetVR()->GetYawOffset();
	float yawDelta = currentYawOffset - lastSmoothingYawOffset;

	// Detect snap turns and rotate smoothed position
	Helpers::RotateForSnapTurn(smoothedPosition, yawDelta, Game::instance.c_SnapTurnAmount->Value());

	// Update tracked yaw offset for next frame
	lastSmoothingYawOffset = currentYawOffset;

	float userInput = 0.0f;
	short zoom = Helpers::GetInputData().zoomLevel;

	if (zoom == -1)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountNoZoom->Value();
	}
	else if (zoom == 0)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountOneZoom->Value();
	}
	else if (zoom == 1)
	{
		userInput = Game::instance.c_WeaponSmoothingAmountTwoZoom->Value();
	}

	float clampedValue = std::clamp(userInput, 0.0f, 2.0f);
	if (clampedValue == 0.0f)
	{
		smoothedPosition = actualControllerPos + toOffHand;
	}
	else
	{
		const float scaleFactor = (-20.0f / 9.0f);

		float h = 90.0f * log2(1.0f - exp(clampedValue * scaleFactor));

		float t = 1.0f - pow(2.0f, Game::instance.lastDeltaTime * h);

		smoothedPosition = Helpers::Lerp(smoothedPosition, actualControllerPos + toOffHand, t);
	}
}

void InputHandler::UpdateHandsProximity()
{
	float swapHandDistance = Game::instance.c_SwapHandDistance->Value();

	const Vector3 leftPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Left, true) * Vector3(0.0f, 0.0f, 0.0f);
	const Vector3 rightPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Right, true) * Vector3(0.0f, 0.0f, 0.0f);
	float handDistance = (rightPos - leftPos).lengthSqr();

	bool handsWithinSwapWeaponDistance = false;
	if (swapHandDistance >= 0.0f && handDistance < swapHandDistance * swapHandDistance)
	{
		handsWithinSwapWeaponDistance = true;
		CheckSwapWeaponHand();
	}

	UpdateTwoHandedHold(handDistance, handsWithinSwapWeaponDistance);
}

void InputHandler::CheckSwapWeaponHand()
{
	IVR* vr = Game::instance.GetVR();

	bool bWeaponHandChanged;
	bool bOffhandWeaponHandChanged;
	bool bIsSwitchHandsPressed = vr->GetBoolInput(SwapWeaponHand, bWeaponHandChanged);
	bool bIsOffhandSwitchHandsPressed = vr->GetBoolInput(OffhandSwapWeaponHand, bOffhandWeaponHandChanged);

	bool offHandGrabbedWeapon = false;
	bool dominantHandReleasedWeapon = false;

	if (!Game::instance.bLeftHanded)
	{
		offHandGrabbedWeapon = bIsSwitchHandsPressed && bWeaponHandChanged && !bIsOffhandSwitchHandsPressed;
		dominantHandReleasedWeapon = bIsSwitchHandsPressed && !bIsOffhandSwitchHandsPressed && bOffhandWeaponHandChanged;
	}
	else
	{
		offHandGrabbedWeapon = bIsOffhandSwitchHandsPressed && bOffhandWeaponHandChanged && !bIsSwitchHandsPressed;
		dominantHandReleasedWeapon = bIsOffhandSwitchHandsPressed && !bIsSwitchHandsPressed && bWeaponHandChanged;
	}

	if (offHandGrabbedWeapon || dominantHandReleasedWeapon)
	{
		// Enable left handed and update the bindings to use the relevant action set
		Game::instance.bLeftHanded = !Game::instance.bLeftHanded;

		UpdateRegisteredInputs();
	}
}

void InputHandler::UpdateTwoHandedHold(float handDistance, bool handsWithinSwapWeaponDistance)
{
	// Two hand aim is disabled when 3DOF is enabled.
	if (Game::instance.bUse3DOFAiming) {
		Game::instance.bUseTwoHandAim = false;
		return;
	}

	IVR* vr = Game::instance.GetVR();

	bool bGripChanged;
	bool bIsGripping = vr->GetBoolInput(TwoHandGrip, bGripChanged);

	if (handsWithinSwapWeaponDistance)
	{
		if (!bIsGripping) {
			Game::instance.bUseTwoHandAim = false;
		}
		return;
	}

	if (Game::instance.c_ToggleGrip->Value())
	{
		if (bGripChanged && bIsGripping)
		{
			bWasGripping ^= true;
		}
		bIsGripping = bWasGripping;
	}

	float twoHandDistance = Game::instance.c_TwoHandDistance->Value();
	if (twoHandDistance >= 0.0f)
	{
		if (bGripChanged)
		{
			if (bIsGripping)
			{
				if (handDistance < twoHandDistance * twoHandDistance)
				{
					Game::instance.bUseTwoHandAim = true;
				}
			}
			else
			{
				Game::instance.bUseTwoHandAim = false;
			}
		}
	}
	else
	{
		Game::instance.bUseTwoHandAim = bIsGripping;
	}
}

#undef ApplyBoolInput
#undef ApplyImpulseBoolInput
#undef RegisterBoolInput
#undef RegisterVector2Input
