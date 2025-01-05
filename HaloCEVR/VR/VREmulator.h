#pragma once
#include "IVR.h"
#include <wtypes.h>
#include <d3d9.h>

class VREmulator : public IVR
{
public:
	// Start Interface IVR
	void Init();
	void OnGameFinishInit();
	void Shutdown();
	void UpdatePoses();
	void PreDrawFrame(struct Renderer* renderer, float deltaTime);
	void PostDrawFrame(struct Renderer* renderer, float deltaTime);

	void UpdateCameraFrustum(struct CameraFrustum* frustum, int eye);

	int GetViewWidth() { return 1400; }
	int GetViewHeight() { return 1400; }
	float GetViewWidthStretch() { return 1.0f; }
	float GetViewHeightStretch() { return 1.0f; }
	float GetAspect() { return 1.0f; }
	int GetScopeWidth() { return 800; }
	int GetScopeHeight() { return 600; }
	void Recentre() {};
	void SetLocationOffset(Vector3 newOffset) {}
	Vector3 GetLocationOffset() { return Vector3(0.0f, 0.0f, 0.0f); }
	void SetYawOffset(float newOffset) {}
	float GetYawOffset() { return 0.0f; }
	Matrix4 GetHMDTransform(bool bRenderPose = false) { return Matrix4(); }
	Matrix4 GetControllerTransform(ControllerRole role, bool bRenderPose = false);
	Matrix4 GetRawControllerTransform(ControllerRole role, bool bRenderPose = false);
	Matrix4 GetControllerBoneTransform(ControllerRole role, int bone, bool bRenderPose = false);
	Vector3 GetControllerVelocity(ControllerRole role, bool bRenderPose = false);
	bool TryGetControllerFacing(ControllerRole role, Vector3& outDirection);
	struct IDirect3DSurface9* GetRenderSurface(int eye);
	struct IDirect3DTexture9* GetRenderTexture(int eye);
	struct IDirect3DSurface9* GetUISurface();
	struct IDirect3DSurface9* GetCrosshairSurface();
	struct IDirect3DSurface9* GetScopeSurface();
	struct IDirect3DTexture9* GetScopeTexture();
	void SetMouseVisibility(bool bIsVisible) {}
	void SetCrosshairTransform(class Matrix4& newTransform);
	void UpdateInputs();
	InputBindingID RegisterBoolInput(std::string set, std::string action);
	InputBindingID RegisterVector2Input(std::string set, std::string action);
	bool GetBoolInput(InputBindingID id);
	bool GetBoolInput(InputBindingID id, bool& bHasChanged);
	Vector2 GetVector2Input(InputBindingID id);
	Vector2 GetMousePos() { return Vector2(0.0f, 0.0f); }
	bool GetMouseDown() { return false; }
	void ShowKeyboard(const std::string& textBuffer);
	bool IsKeyboardVisible();
	void HideKeyboard();
	std::string GetKeyboardInput();
	std::string GetDeviceName();
	// End Interface IVR

protected:
	void CreateSharedTarget();

	void CreateTexAndSurface(int index, UINT width, UINT height, DWORD usage, D3DFORMAT format);

	void DrawEye(struct Renderer* renderer, float deltaTime, int eye);

	HWND hWnd;
	struct IDirect3DDevice9* mirrorDevice = nullptr;

	struct IDirect3DSurface9* uiSurface = nullptr;
	struct IDirect3DTexture9* uiTexture = nullptr;
	struct IDirect3DSurface9* crosshairSurface = nullptr;
	struct IDirect3DTexture9* crosshairTexture = nullptr;
	struct IDirect3DSurface9* scopeSurface = nullptr;
	struct IDirect3DTexture9* scopeTexture = nullptr;
	struct IDirect3DSurface9* eyeSurface_Game[2][2];
	struct IDirect3DSurface9* eyeSurface_VR[2][2];

	struct IDirect3DTexture9* eyeTexture_Game[2][2];
	struct IDirect3DTexture9* eyeTexture_VR[2][2];

	struct Binding
	{
		std::string bindingName;
		int virtualKey = 0;
		bool bHasChanged = false;
		bool bPressed = false;
	};

	Binding bindings[13] = {
		{"Jump", VK_SPACE},
		{"SwitchGrenades", 'G'},
		{"Interact", 'E'},
		{"SwitchWeapons", VK_TAB},
		{"Melee", 'Q'},
		{"Flashlight", 'F'},
		{"Grenade", VK_RBUTTON},
		{"Fire", VK_LBUTTON},
		{"MenuBack", 'P'}, // Intentionally weird binding because we don't override this in the same way and it would conflict
		{"Crouch", VK_LCONTROL},
		{"Zoom", 'Z'},
		{"Reload", 'R'},
		{"EMU_MoveHandSwap", 'H'}
	};

	struct AxisBinding
	{
		int virtualKey = 0;
		int scale = 0;
		int axisId = 0;
	};

	struct Axis2D
	{
		std::string axisName;
		int indexX = 0;
		int indexY = 0;
	};

	float axes1D[6] = {
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};

	Axis2D axes2D[3] =
	{
		{"Move", 0, 1},
		{"EMU_MoveHandFlat", 2, 3},
		{"EMU_MoveHandVert", 4, 5}
	};

	AxisBinding axisBindings[10] =
	{
		{'W', 1, 1},
		{'S', -1, 1},
		{'A', -1, 0},
		{'D', 1, 0},
		{'I', 1, 3},
		{'K', -1, 3},
		{'J', -1, 2},
		{'L', 1, 2},
		{'U', -1, 4},
		{'O', 1, 4},
	};

	InputBindingID inputMoveHandFlat = 0;
	InputBindingID inputMoveHandVert = 0;
	InputBindingID inputMoveHandSwap = 0;
	bool bMoveHand = true;

	Vector3 mainHandOffset;
	Vector3 mainHandRot;

	bool bKeyboardActive = false;
	std::string keyboardBuffer;
	bool keystate[128];
};

