#include "InputHandler.h"
#include "Game.h"
#include "Helpers/Controls.h"
#include "Helpers/Camera.h"
#include "Helpers/Menus.h"


#define RegisterBoolInput(x) x = vr->RegisterBoolInput("default", #x);
#define RegisterVector2Input(x) x = vr->RegisterVector2Input("default", #x);
#define ApplyBoolInput(x) controls.##x = vr->GetBoolInput(x) ? 127 : 0;
#define ApplyImpulseBoolInput(x) controls.##x = vr->GetBoolInput(x, bHasChanged) && bHasChanged ? 127 : 0;

void InputHandler::RegisterInputs()
{
	IVR* vr = Game::instance.GetVR();

	RegisterBoolInput(Jump);
	RegisterBoolInput(SwitchGrenades);
	RegisterBoolInput(OffhandSwitchGrenades);
	RegisterBoolInput(Interact);
	RegisterBoolInput(OffhandInteract);
	RegisterBoolInput(SwitchWeapons);
	RegisterBoolInput(OffhandSwitchWeapons);
	RegisterBoolInput(Melee);
	RegisterBoolInput(Flashlight);
	RegisterBoolInput(Grenade);
	RegisterBoolInput(OffhandGrenade);
	RegisterBoolInput(Fire);
	RegisterBoolInput(OffhandFire);
	RegisterBoolInput(MenuForward);
	RegisterBoolInput(MenuBack);
	RegisterBoolInput(OffhandMenuBack);
	RegisterBoolInput(Crouch);
	RegisterBoolInput(Zoom);
	RegisterBoolInput(OffhandZoom);
	RegisterBoolInput(Reload);
	RegisterBoolInput(OffhandReload);
	RegisterBoolInput(TwoHandGrip);
	RegisterBoolInput(OffhandTwoHandGrip);
	RegisterBoolInput(SwapWeaponHands);
	RegisterBoolInput(OffhandSwapWeaponHands);

	RegisterVector2Input(Move);
	RegisterVector2Input(Look);

	Game::instance.bLeftHanded = Game::instance.c_LeftHanded->Value();
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

#define DRAW_DEBUG_MOVE 0

void InputHandler::UpdateInputs(bool bInVehicle)
{
	IVR* vr = Game::instance.GetVR();

	vr->UpdateInputs();

	static bool bHasChanged = false;

	Controls& controls = Helpers::GetControls();

	// Bindings without gestures
	InputBindingID SwitchGrenadesBinding = Game::instance.bLeftHanded ? OffhandSwitchGrenades : SwitchGrenades;
	InputBindingID InteractBinding = Game::instance.bLeftHanded ? OffhandInteract : Interact;
	InputBindingID GrenadeBinding = Game::instance.bLeftHanded ? OffhandGrenade : Grenade;
	InputBindingID FireBinding = Game::instance.bLeftHanded ? OffhandFire : Fire;
	InputBindingID MenuBackBinding = Game::instance.bLeftHanded ? OffhandMenuBack : MenuBack;
	InputBindingID ZoomBinding = Game::instance.bLeftHanded ? OffhandZoom : Zoom;
	InputBindingID ReloadBinding = Game::instance.bLeftHanded ? OffhandReload : Reload;

	// Bindings with gestures
	InputBindingID SwitchWeaponsBinding = Game::instance.bLeftHanded ? OffhandSwitchWeapons : SwitchWeapons;
	InputBindingID SwapWeaponHandsBinding = Game::instance.bLeftHanded ? OffhandSwapWeaponHands : SwapWeaponHands;
	InputBindingID TwoHandGripBinding = Game::instance.bLeftHanded ? OffhandTwoHandGrip : TwoHandGrip;

	// Controls which will swap hands
	bool bSwitchGrenadesChanged;
	bool bZoomChanged;
	controls.SwitchGrenades = vr->GetBoolInput(SwitchGrenadesBinding, bSwitchGrenadesChanged) && bSwitchGrenadesChanged ? 127 : 0;
	controls.Interact = vr->GetBoolInput(InteractBinding) ? 127 : 0;
	controls.Grenade = vr->GetBoolInput(GrenadeBinding) ? 127 : 0;
	controls.Fire = vr->GetBoolInput(FireBinding) ? 127 : 0;
	controls.Zoom = vr->GetBoolInput(ZoomBinding, bZoomChanged) && bZoomChanged ? 127 : 0;
	controls.Reload = vr->GetBoolInput(ReloadBinding) ? 127 : 0;

	// Controls which will not swap hands
	ApplyBoolInput(Jump);
	ApplyBoolInput(Melee);
	ApplyBoolInput(Flashlight);
	ApplyBoolInput(MenuForward);
	ApplyBoolInput(Crouch);
	

	unsigned char MotionControlFlashlight = UpdateFlashlight();
	if (MotionControlFlashlight > 0)
	{
		controls.Flashlight = MotionControlFlashlight;
	}

	if (Game::instance.c_EnableWeaponHolsters->Value())
	{
		unsigned char HolsterSwitchWeapons = UpdateHolsterSwitchWeapons();
		bool bSwitchWeaponsChanged;
		bool bSwitchWeaponsPressed = vr->GetBoolInput(SwitchWeaponsBinding, bSwitchWeaponsChanged);

		if (HolsterSwitchWeapons > 0 && bSwitchWeaponsPressed && bSwitchWeaponsChanged)
		{
			controls.SwitchWeapons = vr->GetBoolInput(SwitchWeaponsBinding) ? 127 : 0;
		}
	}
	else
	{
		controls.SwitchWeapons = vr->GetBoolInput(SwitchWeaponsBinding) ? 127 : 0;
	}

	unsigned char MotionControlMelee = UpdateMelee();
	if (MotionControlMelee > 0)
	{
		controls.Melee = MotionControlMelee;
	}

	unsigned char MotionControlCrouch = UpdateCrouch();
	if (MotionControlCrouch > 0)
	{
		controls.Crouch = MotionControlCrouch;
	}

	const float holdToRecentreTime = 1000.0f;

	bool bMenuChanged;
	bool bMenuPressed = vr->GetBoolInput(MenuBackBinding, bMenuChanged);

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

	bool bWeaponHandChanged;
	bool bOffhandWeaponHandChanged;
	bool bIsSwitchHandsPressed = vr->GetBoolInput(SwapWeaponHands, bWeaponHandChanged);
	bool bIsOffhandSwitchHandsPressed = vr->GetBoolInput(OffhandSwapWeaponHands, bOffhandWeaponHandChanged);

	float swapHandDistance = Game::instance.c_SwapHandDistance->Value();

	bool handsWithinSwapWeaponDistance = false;
	if (swapHandDistance >= 0.0f)
	{
		// Check if hands are within swap distance
		const Vector3 leftPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Left, true) * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 rightPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Right, true) * Vector3(0.0f, 0.0f, 0.0f);
		handsWithinSwapWeaponDistance = (rightPos - leftPos).lengthSqr() < swapHandDistance * swapHandDistance;

		if (handsWithinSwapWeaponDistance)
		{
			if (!Game::instance.bLeftHanded && bWeaponHandChanged && bIsSwitchHandsPressed && !bIsOffhandSwitchHandsPressed)
			{
				Game::instance.bLeftHanded = true;
			}
			else if (Game::instance.bLeftHanded && bOffhandWeaponHandChanged && bIsOffhandSwitchHandsPressed && !bIsSwitchHandsPressed)
			{
				Game::instance.bLeftHanded = !Game::instance.bLeftHanded;
			}
		}
	}

	bool bGripChanged;
	bool bIsGripping = vr->GetBoolInput(TwoHandGripBinding, bGripChanged);

	if (handsWithinSwapWeaponDistance)
	{
		if (!bIsGripping) {
	        Game::instance.bUseTwoHandAim = false;
	    }
	}
	else
	{
	    if (Game::instance.c_ToggleGrip->Value())
	    {
	        if (bGripChanged && bIsGripping)
	        {
	            bWasGripping ^= true;
	        }

	        bIsGripping = bWasGripping;
	    }

	    float handDistance = Game::instance.c_TwoHandDistance->Value();

	    if (handDistance >= 0.0f)
	    {
	        if (bGripChanged)
	        {
	            if (bIsGripping)
	            {
	                const Vector3 leftPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Left, true) * Vector3(0.0f, 0.0f, 0.0f);
	                const Vector3 rightPos = Game::instance.GetVR()->GetControllerTransform(ControllerRole::Right, true) * Vector3(0.0f, 0.0f, 0.0f);

	                if ((rightPos - leftPos).lengthSqr() < handDistance * handDistance)
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

	Vector2 MoveInput = vr->GetVector2Input(Move);

	if (!bInVehicle && (Game::instance.c_HandRelativeMovement->Value() != 0))
	{
		Camera& cam = Helpers::GetCamera();
		Vector3 camForward = cam.lookDir;

		const bool leftHand = (Game::instance.c_HandRelativeMovement->Value() == 1);
		const ControllerRole role = leftHand ? ControllerRole::Left : ControllerRole::Right;
		Matrix4 controllerTransform = vr->GetControllerTransform(role); // TODO: Not sure about bRenderPose bool param here. Doesn't seem to matter.
		const float offset = Game::instance.c_HandRelativeOffsetRotation->Value();
		controllerTransform.rotateZ(leftHand ? offset: -offset);
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

	controls.Left = -MoveInput.x;
	controls.Forwards = MoveInput.y;
}

void InputHandler::UpdateCamera(float& yaw, float& pitch)
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

	Vector3 lookHMD = vr->GetHMDTransform().getLeftAxis();
	// Get current camera angle
	Vector3 lookGame = Game::instance.bDetectedChimera ? Game::instance.LastLookDir : Helpers::GetCamera().lookDir;
	// Apply deltas
	float yawHMD = atan2(lookHMD.y, lookHMD.x);
	float yawGame = atan2(lookGame.y, lookGame.x);
	yaw = (yawHMD - yawGame);

	float pitchHMD = atan2(lookHMD.z, sqrt(lookHMD.x * lookHMD.x + lookHMD.y * lookHMD.y));
	float pitchGame = atan2(lookGame.z, sqrt(lookGame.x * lookGame.x + lookGame.y * lookGame.y));
	pitch = (pitchHMD - pitchGame);
}

void InputHandler::UpdateCameraForVehicles(float& yaw, float& pitch)
{
	IVR* vr = Game::instance.GetVR();

	Vector2 lookInput = vr->GetVector2Input(Look);

	const float DegToRad = 3.141593f / 180.0f;

	const float YawDelta = lookInput.x * Game::instance.c_HorizontalVehicleTurnAmount->Value() * Game::instance.lastDeltaTime;
	const float PitchDelta = lookInput.y * Game::instance.c_VerticalVehicleTurnAmount->Value() * Game::instance.lastDeltaTime;

	float yawOffset = vr->GetYawOffset();
	
	yawOffset += YawDelta;

	vr->SetYawOffset(yawOffset);

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

	float clampedValue = std::clamp(userInput, 0.0f, 1.0f);

	float maxSmoothing = 20.0f;		//20 is already a bit ridiculous but just incase people need that much smoothing. 
	float speedRampup = 10.0f;		//This helps control the slowdown curve of the interpolation

	// Apply the smoothing using linear interpolation with the adjusted deltaTime
	float t = (clampedValue * maxSmoothing) * Game::instance.lastDeltaTime;
	smoothedPosition = Helpers::Lerp(smoothedPosition, actualControllerPos + toOffHand, exp(-t * speedRampup));
}


#undef ApplyBoolInput
#undef ApplyImpulseBoolInput
#undef RegisterBoolInput
#undef RegisterVector2Input