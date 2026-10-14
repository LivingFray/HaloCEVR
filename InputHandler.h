#pragma once
#include "VR/IVR.h"
#include "Helpers/Objects.h"
#include "Immersion/InteractionModel.h"
#include "Immersion/RetailWeaponEngine.h"
#include <chrono>
#include <string>

class InputHandler
{
public:
	void RegisterInputs();
	void InitializePhysicalWeapons();
	bool ShouldBlockPhysicalFire(const struct HaloID& weapon) const;
    bool PhysicalRecoilState(const HaloID& weapon, Immersion::FireState& state, bool& supported) const;
	bool GetSupportOnlyTransform(Matrix4& transform) const;
	bool PhysicalSupportOnly() const { return physicalSession.Active() && physicalSession.SupportOnly(); }
	bool PhysicalWeaponsStowed() const { return physicalControlsActive && physicalSession.Active() && physicalSession.Held() == Immersion::None; }
	bool CanRenderEmptyHands() const {
		Immersion::NativeInventory native;
		return PhysicalWeaponsStowed() && physicalEngine.Read(native) && native.current == Immersion::None;
	}
    bool ShouldHidePhysicalViewModel() const {
        return physicalControlsActive && physicalSession.Active() &&
            !physicalEngine.ViewModelReady(physicalSession.Held());
    }
    bool PhysicalViewModelWeapon(HaloID& weapon, HaloID& tag, HaloID& animations) const {
        Immersion::Object w,t,a;
        if (!physicalEngine.ViewModelWeapon(w,t,a)) return false;
        weapon={static_cast<uint16_t>(w),static_cast<uint16_t>(w>>16)};
        tag={static_cast<uint16_t>(t),static_cast<uint16_t>(t>>16)};
        animations={static_cast<uint16_t>(a),static_cast<uint16_t>(a>>16)};
        return true;
    }
    bool PhysicalVisualsActive() const {
        Immersion::NativeInventory native;
        return physicalControlsActive && physicalSession.Active() && physicalEngine.Read(native);
    }
    std::array<Immersion::HolsterMarker,2> HolsterMarkers() const {
        std::array<Immersion::HolsterMarker,2> markers{};
        for (unsigned home=0; home<2; ++home) {
            markers[home].weapon=physicalSession.Items().SlotOwner(home);
            markers[home].equipped=physicalSession.Items().InHand(markers[home].weapon);
        }
        return markers;
    }
    bool DrawBodyModels(bool hands, const std::array<Immersion::HolsterMarker,2>& markers,
        const Matrix4* poses, const Matrix4* markerPoses) const {
        return physicalEngine.DrawBodyModels(hands,markers,poses,markerPoses);
    }
    void DrawOnFootBody()const{physicalEngine.DrawOnFootBody();}
    void BeginAccuracyShot(Immersion::Object weapon,short trigger,float scale){physicalEngine.BeginAccuracyShot(weapon,trigger,scale);}
    void EndAccuracyShot(){physicalEngine.EndAccuracyShot();}
    float AccuracyReticleScale(float scale)const{return physicalEngine.AccuracyReticleScale(scale);}
    float HolsterYaw() const { return holsterYaw; }
	void UpdateRegisteredInputs();
	void UpdateInputs(bool bInVehicle);
	void UpdateCamera(float& yaw, float& pitch);
    void UpdateTutorialLook();
	void UpdateCameraForVehicles(float& yaw, float& pitch);
    Vector2 VehicleLookInput();
	void SetMousePosition(int& x, int& y);
	void UpdateMouseInfo(struct MouseInfo* mouseInfo);
	bool GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand);
	void CalculateSmoothedInput();

	Vector3 smoothedPosition = Vector3(0.0f, 0.0f, 0.0f);

	// Track previous yaw offset to detect snap turns for weapon position smoothing
	float lastSmoothingYawOffset = 0.0f;
	bool bLastYawInitialized = false;

protected:


	unsigned char UpdateFlashlight();
	unsigned char UpdateGripFlashlight();
	unsigned char UpdateHolsterSwitchWeapons();
	unsigned char UpdateMelee();
	unsigned char UpdateCrouch();

	// Update Controls that rely on the distance between hands
	void UpdateHandsProximity();
	void CheckSwapWeaponHand();
	void UpdateTwoHandedHold(float handDistance, bool handsWithinSwapWeaponDistance);

	char lastSnapState = 0;
	unsigned char mouseDownState = 0;

	bool bHoldingMenu = false;
	std::chrono::steady_clock::time_point menuHeldTime{},menuEscapeReleaseTime{};
    Immersion::Button menuButton,menuChordTrigger;
    bool menuChordReady=false,menuChordUsed=false,menuRightTriggerBlocked=false;
    InputBindingID menuBinding=0;
    bool menuArmed=false;
    Immersion::Button recentreButton;
    std::chrono::steady_clock::time_point recentreHeldTime{};
    bool recentreArmed=false,recentreFired=false,recentreTap=false;
    void UpdateMenuInput();

	bool bWasGripping = false;
	bool bWasSwappingHands = false;
	
	InputBindingID Jump = 0;
	InputBindingID SwitchGrenades = 0;
	InputBindingID Interact = 0;
	InputBindingID SwitchWeapons = 0;
	InputBindingID Melee = 0;
	InputBindingID Flashlight = 0;
	InputBindingID Grenade = 0;
	InputBindingID Fire = 0;
	InputBindingID MenuForward = 0;
	InputBindingID MenuBack = 0;
	InputBindingID Crouch = 0;
	InputBindingID Zoom = 0;
	InputBindingID Reload = 0;
	InputBindingID PhysicalMove=0, PhysicalLook=0;
	InputBindingID Move = 0;
	InputBindingID Look = 0;
	
	InputBindingID Recentre = 0;
	InputBindingID TwoHandGrip = 0;

	InputBindingID SwapWeaponHand = 0;
	InputBindingID OffhandSwapWeaponHand = 0;

private:
	void ReportPhysicalStatus(const char* status);
	std::string physicalStatus;
	bool UpdatePhysicalWeapons(bool inVehicle);
	void UpdatePhysicalButtons();
	Immersion::Button physicalGrenadeCycle;
    bool physicalCycleArmed=false;
	bool physicalWeaponsEnabled = false;
	bool physicalControlsActive = false;
	bool physicalFrameValid = false;
	bool physicalCanFire = false;
	bool physicalTriggerReady = false;
	std::chrono::steady_clock::time_point lastPhysicalInputUpdate{};
	Immersion::RetailWeaponEngine physicalEngine;
	Immersion::WeaponSession physicalSession;
	std::array<Immersion::Button,2> physicalGripButtons;
	Immersion::HolsterHover holsterHover;
	Immersion::Button menuConfirm;
	Matrix4 supportRelativeTransform;
	Matrix4 lastHeldTransform;
	bool haveHeldTransform = false;
	float holsterYaw = 0;
	bool haveHolsterYaw = false;
	InputBindingID PhysicalLeftGrip = 0;
	InputBindingID PhysicalRightGrip = 0;
	InputBindingID PhysicalLeftTrigger = 0;
	InputBindingID PhysicalRightTrigger = 0;
	InputBindingID PhysicalJump = 0;
	InputBindingID PhysicalGrenade = 0;
	InputBindingID PhysicalGrenadeCycle = 0;
    InputBindingID PhysicalRecentre = 0;
    InputBindingID PhysicalInteract = 0;
	Immersion::HeadTouch headTouch;
    float flashlightPulse=0.f;
	std::array<Immersion::Button, 2> headGripButtons;
	std::array<bool, 2> headGripConsumed{};
	std::chrono::steady_clock::time_point lastHeadTouchUpdate{};
	bool IsHandInHolster(const Vector3& handPos, const Vector3& holsterPos, const float& holsterActivationDistance);
};

