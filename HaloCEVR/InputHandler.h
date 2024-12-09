#pragma once
#include "VR/IVR.h"
#include <chrono>

class InputHandler
{
public:
	void RegisterInputs();
	void UpdateInputs(bool bInVehicle);
	void UpdateCamera(float& yaw, float& pitch);
	void UpdateCameraForVehicles(float& yaw, float& pitch);
	void SetMousePosition(int& x, int& y);
	void UpdateMouseInfo(struct MouseInfo* mouseInfo);
	bool GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand);
	void CalculateSmoothedInput(); 

	Vector3 smoothedPosition = Vector3(0.0f, 0.0f, 0.0f);

protected:


	unsigned char UpdateFlashlight();
	unsigned char UpdateHolsterSwitchWeapons();
	unsigned char UpdateMelee();
	unsigned char UpdateCrouch();
	
	char lastSnapState = 0;
	unsigned char mouseDownState = 0;

	bool bHoldingMenu = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> menuHeldTime;

	bool bWasGripping = false;

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
	InputBindingID Move = 0;
	InputBindingID Look = 0;
	
	InputBindingID Recentre = 0;
	InputBindingID TwoHandGrip = 0;

private:
	bool IsHandInHolster(const Vector3& handPos, const Vector3& holsterPos, const float& holsterActivationDistance);
};

