#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "Config/Config.h"
#include "VR/IVR.h"
#include "Helpers/Renderer.h"
#include "Helpers/RenderTarget.h"
#include "Helpers/Objects.h"
#include "Maths/Vectors.h"
#include "WeaponHandler.h"
#include "InputHandler.h"
#include "InGameRenderer.h"
#include "WeaponHapticsConfig.h"
#include "Profiler.h"
#include "UI/UIRenderer.h"
#include "UI/SettingsMenu.h"

enum class ERenderState { UNKNOWN, LEFT_EYE, RIGHT_EYE, GAME, SCOPE };

// Internal vehicle tuning values. These are deliberately not user settings:
// motion-controlled aiming always uses the separated native-camera path.
struct VehicleReleasePolicy
{
	static constexpr float SimulationTickSeconds = 1.0f / 30.0f;
	static constexpr float ModelAimActivationDelaySeconds = 1.0f;
	static constexpr float ModelAimTransitionSeconds = 1.0f;
	static constexpr float FirstPersonCameraActivationDelaySeconds = 1.0f;
	static constexpr float MaximumTurnSpeedDegreesPerSecond = 360.0f;
};

class Game
{
public:
	static Game instance;

	void Init();
	void Shutdown();

	void OnInitDirectX();
	void SetRenderTickProgress(float tickProgress);
	void PreDrawFrame(struct Renderer* renderer, float deltaTime);
	void PreDrawEye(struct Renderer* renderer, float deltaTime, int eye);
	void PostDrawEye(struct Renderer* renderer, float deltaTime, int eye);
	bool PreDrawScope(struct Renderer* renderer, float deltaTime);
	void PostDrawScope(struct Renderer* renderer, float deltaTime);
	void PreDrawMirror(struct Renderer* renderer, float deltaTime);
	void PostDrawMirror(struct Renderer* renderer, float deltaTime);
	void PostDrawFrame(struct Renderer* renderer, float deltaTime);
	Vector3 GetSmoothedInput() const;

	bool PreDrawHUD();
	void PostDrawHUD();

	// Render Halo's world-linked HUD indicator passes through a center-HMD,
	// yaw-only camera whose projection matches the physical HUD overlay.
	bool BeginWorldSpaceIndicatorProjectionOverride();
	void EndWorldSpaceIndicatorProjectionOverride();

	bool PreDrawMenu();
	void PostDrawMenu();

	bool PreDrawLoading(int param1, struct Renderer* renderer);
	void PostDrawLoading(int param1, struct Renderer* renderer);

	bool PreDrawCrosshair(short* anchorLocation);
	void PostDrawCrosshair();

	void PreDrawImage(void* param1, void* param2);
	void PostDrawImage(void* param1, void* param2);

	void UpdateViewModel(HaloID& id, struct Vector3* pos, struct Vector3* facing, struct Vector3* up, struct TransformQuat* BoneTransforms, struct Transform* OutBoneTransforms);
	void PreFireWeapon(HaloID& WeaponID, short param2);
	void PostFireWeapon(HaloID& WeaponID, short param2);
	void PreThrowGrenade(HaloID& playerID);
	void PostThrowGrenade(HaloID& playerID);
	bool GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand);
	bool GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetVehicleVRAim(Vector3& outPosition, Vector3& outAim, Vector3& outUp) const;
	bool IsLocalPlayerOperatingMountedVehicleTurret() const;
	bool IsJoinedMultiplayerClient() const;
	bool IsMultiplayerSession() const;
	void UpdateSinglePlayerMeleeAim(bool meleePressed);
	bool GetSinglePlayerMeleeAim(Vector3& outAim) const;
	void ReloadStart(HaloID param1, short param2, bool param3);
	void ReloadEnd(short param1, HaloID param2);

	void UpdateInputs();
	void CalculateSmoothedInput();
	void UpdateRoomScaleMovement();
	void UpdateMPRoomScaleBodyOnNetworkTick();
	void SetMPRoomScaleManualWorldInput(const Vector3& manualWorldInput);
	void BeginTemporaryFireRelocation();
	void EndTemporaryFireRelocation();

	void UpdateCamera(float& yaw, float& pitch);
	void SetMousePosition(int& x, int& y);
	void UpdateMouseInfo(struct MouseInfo* mouseInfo);

	void SetViewportScale(struct Viewport* viewport);

	bool GetDrawMirror() const { return mirrorSource == ERenderState::GAME && c_DrawMirror->Value(); }

	ERenderState GetRenderState() const { return renderState; }

	bool ShouldUseOriginalScope() const { return bUse3DOFAiming || c_UseOriginalScope->Value(); }

	float GetScopeSize() const { return ShouldUseOriginalScope() ? c_3DOFScopeScale->Value() : c_ScopeScale->Value(); }

	float MetresToWorld(float m) const;
	float WorldToMetres(float w) const;

	inline IVR* GetVR() const { return vr; }

	UINT backBufferWidth = 600;
	UINT backBufferHeight = 600;

	// HACK: Some places it is hard to get the delta time (e.g. updating the camera)
	// Using the last known delta time should be good enough
	float lastDeltaTime = 0.0f;

	// Halo passes elapsed simulation-tick time to DrawFrame, not a normalized
	// interpolation fraction. Stock Halo simulates at exactly 30 Hz, so the
	// locked vehicle camera normalizes that engine phase against 1/30 second.
	// The headset/render refresh rate never contributes to this denominator.
	float renderTickElapsedSeconds = 0.0f;
	bool bRenderTickElapsedSecondsValid = false;
	float renderTickIntervalSeconds = 1.0f / 30.0f;
	bool bRenderTickIntervalSecondsValid = true;

	bool bNeedsRecentre = true;
	bool bUseTwoHandAim = false;
	bool bLeftHanded = false;
	bool bUse3DOFAiming = false;

	Config config;
	bool bIsFiring = false;
	bool bIsReloading = false;

	InGameRenderer inGameRenderer;
	InGameRenderer scopeRenderer;
	UIRenderer* uiRenderer;
	SettingsMenu* settingsMenu;

	bool bDetectedChimera = false;
	Vector3 LastLookDir;
	WeaponHapticsConfigManager weaponHapticsConfig;

	bool bLoadedConfig = false;
	bool bSavedConfig = false;

	bool bIsCustom = false;

	UINT overlayWidth = 640;
	UINT overlayHeight = 640;

#if USE_PROFILER
	Profiler profiler;
#endif
protected:

	void CreateConsole();

	void PatchGame();

	void SetupConfigs();

	void CalcFPS(float deltaTime);
#if USE_PROFILER
	void DumpProfilerData();
#endif

	void UpdateCrosshairAndScope();
	void SetScopeTransform(Matrix4& newTransform, bool bIsVisible);
	void ApplyFirstPersonVehicleDriverCamera();
	BoolProperty* GetCurrentFirstPersonVehicleCameraSetting() const;
	bool IsCurrentVehicleSeatUsingCustomFirstPersonCamera() const;
	bool IsCurrentVehicleNativeHandheldPassengerView() const;
	bool IsCurrentVehicleViewFirstPerson() const;
	void BeginFirstPersonVehicleBodyMask(Renderer* renderer);
	void EndFirstPersonVehicleBodyMask();

	void StoreRenderTargets();
	void RestoreRenderTargets();

	void CreateTextureAndSurface(UINT Width, UINT Height, DWORD Usage, D3DFORMAT Format, struct IDirect3DSurface9** OutSurface, struct IDirect3DTexture9** OutTexture);

	WeaponHandler weaponHandler;
	InputHandler inputHandler;

	struct FPSTracker
	{
		float timeSinceFPSUpdate = 0.0f;
		int framesSinceFPSUpdate = 0;

		int fps = 0;
	} fpsTracker;

	FILE* consoleOut = nullptr;

	IVR* vr;

	RenderTarget gameRenderTargets[8];

	struct IDirect3DSurface9* uiSurface;
	struct IDirect3DSurface9* crosshairSurface;
	struct IDirect3DSurface9* uiRealSurface;
	struct IDirect3DSurface9* crosshairRealSurface;

	struct IDirect3DSurface9* scopeSurfaces[3];
	struct IDirect3DTexture9* scopeTextures[3];

	ERenderState renderState = ERenderState::UNKNOWN;

	CameraFrustum frustum1;
	CameraFrustum frustum2;

	short realZoom = -1;
	sRect realRect;
	sRect realLoadRect;
	UINT realUIWidth;
	UINT realUIHeight;

	DWORD realAlphaFunc;
	DWORD realAlphaSrc;
	DWORD realAlphaDest;

	// Halo's stock vehicle path suppresses the first-person viewmodel. Keep that
	// suppression for driver/turret seats, but release it for native handheld
	// passengers so their normal on-foot weapon remains visible while scoped.
	bool bVehicleViewModelSuppressionActive = false;

	bool bInVehicle = false;
	bool bHasWeapon = true;

	// Keep campaign melee on controller aim through Halo's delayed
	// melee-impact window.
	float singlePlayerMeleeAimRemaining = 0.0f;

	// Temporarily substitute horizontal HMD heading only while Halo renders the
	// HUD, then restore the exact gameplay/network state.
	bool bHUDHMDHeadingOverrideActive = false;
	UnitDynamicObject* hudHMDHeadingUnit = nullptr;
	Vector3 hudSavedCameraLookDir{};

	// Halo's stock teammate and objective markers are projected from the active
	// eye camera, then displayed on a separate yaw-only HUD overlay. Save the
	// eye matrices while either dedicated indicator pass uses the matched camera.
	bool bWorldSpaceIndicatorProjectionOverrideActive = false;
	CameraRenderMatrices* worldSpaceIndicatorActiveCameraMatrices = nullptr;
	CameraRenderMatrices worldSpaceIndicatorSavedCameraMatrices{};
	Vector3 hudSavedLastLookDir{};
	Vector3 hudSavedFacingDir{};
	Vector3 hudSavedFacing{};
	Vector3 hudSavedDesiredAim{};
	Vector3 hudSavedAim{};
	Vector3 hudSavedAim2{};
	Vector3 hudSavedAim3{};

	// Vehicle motion tracker contacts must use HMD heading even though controller
	// aim is cached in Halo's desktop input fields. Restore the exact values after
	// the render frame so gameplay and network aim remain unchanged.
	bool bMotionTrackerHMDInputOverrideActive = false;
	float motionTrackerSavedInputYaw = 0.0f;
	float motionTrackerSavedInputPitch = 0.0f;

	// Hide only the local biped head by temporarily zeroing its render-node scale.
	// The exact value is restored after each eye, keeping simulation/network state
	// and all non-head body nodes untouched.
	bool bFirstPersonVehicleHeadMaskActive = false;
	float* firstPersonVehicleHeadScaleAddress = nullptr;
	float firstPersonVehicleSavedHeadScale = 1.0f;
	// The frustum drawPlayer bit is not consulted for Halo's seated local biped.
	// Hide that actual render object instead, and restore its exact scale after
	// each eye. The first-person hands/weapon use a separate viewmodel hierarchy.
	bool bFirstPersonVehicleBodyScaleMaskActive = false;
	float* firstPersonVehicleBodyScaleAddress = nullptr;
	float firstPersonVehicleSavedBodyScale = 1.0f;

	// Halo owns the base view during cinematics. Track render-frame transitions so
	// VR aim remains quarantined for the rest of the frame in which a cutscene ends.
	unsigned int cameraFrameSerial = 0;
	unsigned int cameraCutsceneResumeFrame = 0;
	bool bCameraWasInCutscene = false;
	bool bCameraCutsceneResumeGuardActive = false;

	// Halo keeps ownership of its third-person-to-first-person camera blend for a
	// short interval after exiting a seat that used HaloCEVR's dedicated unarmed
	// vehicle camera. Armed passenger seats remain on the sampled controller-aim
	// path and do not inherit this timed exit guard.
	bool bCameraWasInVehicle = false;
	bool bCameraWasUsingDedicatedVehicleCamera = false;
	bool bVehicleExitCameraGuardActive = false;
	float vehicleExitCameraGuardElapsedSeconds = 0.0f;

	ERenderState mirrorSource;

	bool bHasShutdown = true;

	bool bWasLoading = false;

	bool bIgnoreNextRoomScaleMovement = false;

	// Multiplayer roomscale world-lock state. A joined client cannot
	// authoritatively teleport its biped, so the remaining horizontal head/body
	// separation is submitted through Halo's real Controls buffer. Accepted
	// roomscale-authored body motion is removed from the tracking offset so the
	// world view stays fixed while the biped catches up underneath the HMD.
	Vector3 mpRoomScaleAccumulatedOffset{};
	Vector3 mpRoomScaleLastAppliedOffset{};
	Vector3 mpRoomScaleLastAppliedPosition{};
	HaloID mpRoomScaleLastAppliedID{ 0xFFFF, 0xFFFF };
	HaloID mpRoomScaleLastResolvedID{ 0xFFFF, 0xFFFF };
	float mpRoomScaleLastInputShare = 0.0f;
	float mpRoomScaleSmoothedRun = 0.0f;
	float mpRoomScaleSmoothedStrafe = 0.0f;
	// Keep simultaneous-stick roomscale requests in world space. The command basis
	// follows the VR gun and can rotate rapidly during one steady physical step.
	Vector3 mpRoomScaleSmoothedWorldInput{};
	// Raw stick locomotion expressed in the user's configured world-space movement
	// reference (HMD/left hand/right hand), captured before multiplayer
	// counter-rotation and digitalisation.
	Vector3 mpRoomScaleManualWorldInput{};
	bool bMPRoomScaleHasAccumulatedOffset = false;
	bool bMPRoomScaleHasAppliedOffset = false;
	// Hysteresis state for joined-client body catch-up. Activation, settling and
	// ramp distances are configurable; sudden tracking-origin jumps are handled
	// separately from legitimate large-play-space movement.
	bool bMPRoomScaleCorrectionActive = false;
	// A joined client's horizontal HMD displacement must remain
	// outside the activation radius briefly before automatic body catch-up begins.
	// This filters ordinary sway, small crouch shifts and one-frame tracking noise.
	float mpRoomScaleActivationHoldSeconds = 0.0f;
	Vector3 mpRoomScaleLastTrackingSample{};
	bool bMPRoomScaleHasTrackingSample = false;
	// FireWeapon temporarily places the local biped at the tracked muzzle so Halo
	// creates the projectile at the controller. Ignore that short-lived position in
	// the joined-client roomscale observer.
	bool bMPRoomScaleTemporaryFireRelocation = false;

	//======Configs======//
public:

	BoolProperty* c_ShowConsole = nullptr;
	BoolProperty* c_DrawMirror = nullptr;
	IntProperty* c_MirrorEye = nullptr;
	FloatProperty* c_CrosshairDistance = nullptr;
	FloatProperty* c_FirstPersonVehicleCrosshairDistance = nullptr;
	FloatProperty* c_ThirdPersonVehicleCrosshairDistance = nullptr;
	FloatProperty* c_CrosshairScale = nullptr;
	FloatProperty* c_MenuOverlayDistance = nullptr;
	FloatProperty* c_UIOverlayDistance = nullptr;
	FloatProperty* c_UIOverlayScale = nullptr;
	FloatProperty* c_MenuOverlayScale = nullptr;
	FloatProperty* c_UIOverlayCurvature = nullptr;
	FloatProperty* c_UIOverlayRenderScale = nullptr;
	BoolProperty* c_ShowCrosshair = nullptr;
	BoolProperty* c_ShowCrosshairInVehicles = nullptr;
	BoolProperty* c_AlwaysHideWarthogCrosshair = nullptr;
	BoolProperty* c_SnapTurn = nullptr;
	FloatProperty* c_SnapTurnAmount = nullptr;
	FloatProperty* c_SmoothTurnAmount = nullptr;
	BoolProperty* c_RoomScaleMovement = nullptr;
	BoolProperty* c_MultiplayerHitboxAccurateProjectiles = nullptr;
	BoolProperty* c_MPRoomScaleWhileMoving = nullptr;
	FloatProperty* c_MPRoomScaleActivationDistance = nullptr;
	FloatProperty* c_MPRoomScaleActivationDelay = nullptr;
	FloatProperty* c_MPRoomScaleSettleDistance = nullptr;
	FloatProperty* c_MPRoomScaleFullSpeedDistance = nullptr;
	FloatProperty* c_MPRoomScaleMaximumTrackingDistance = nullptr;
	IntProperty* c_HandRelativeMovement = nullptr;
	FloatProperty* c_HandRelativeOffsetRotation = nullptr;
	FloatProperty* c_HorizontalVehicleTurnAmount = nullptr;
	FloatProperty* c_VerticalVehicleTurnAmount = nullptr;
	BoolProperty* c_MotionControlledVehicles = nullptr;
	BoolProperty* c_DisableWarthogSteeringMotionControls = nullptr;
	BoolProperty* c_SmoothVehicleVRAim = nullptr;
	FloatProperty* c_VehicleVRAimSmoothingAmount = nullptr;
	IntProperty* c_HideHeadInFirstPersonVehicles = nullptr;
	BoolProperty* c_FirstPersonWarthogDriver = nullptr;
	Vector3Property* c_WarthogDriverOffset = nullptr;
	BoolProperty* c_FirstPersonWarthogTurret = nullptr;
	Vector3Property* c_WarthogTurretOffset = nullptr;
	BoolProperty* c_FirstPersonGhost = nullptr;
	Vector3Property* c_GhostOffset = nullptr;
	BoolProperty* c_FirstPersonScorpion = nullptr;
	Vector3Property* c_ScorpionOffset = nullptr;
	BoolProperty* c_FirstPersonBanshee = nullptr;
	Vector3Property* c_BansheeOffset = nullptr;
	BoolProperty* c_FirstPersonShade = nullptr;
	Vector3Property* c_ShadeOffset = nullptr;
	BoolProperty* c_UseJumpForVehicleSecondaryFire = nullptr;
	BoolProperty* c_OffhandHandFlashlight = nullptr;
	FloatProperty* c_LeftHandFlashlightDistance = nullptr;
	FloatProperty* c_RightHandFlashlightDistance = nullptr;
	BoolProperty* c_EnableWeaponHolsters = nullptr;
	FloatProperty* c_LeftShoulderHolsterActivationDistance = nullptr;
	Vector3Property* c_LeftShoulderHolsterOffset = nullptr;
	FloatProperty* c_RightShoulderHolsterActivationDistance = nullptr;
	Vector3Property* c_RightShoulderHolsterOffset = nullptr;
	Vector3Property* c_ControllerOffset = nullptr;
	Vector3Property* c_ControllerRotation = nullptr;
	FloatProperty* c_ScopeRenderScale = nullptr;
	FloatProperty* c_ScopeScale = nullptr;
	FloatProperty* c_CampaignZoom1Magnification = nullptr;
	FloatProperty* c_CampaignZoom2Magnification = nullptr;
	FloatProperty* c_CustomEditionZoom1Magnification = nullptr;
	FloatProperty* c_CustomEditionZoom2Magnification = nullptr;
	BoolProperty* c_LockScopeRoll = nullptr;
	Vector3Property* c_ScopeOffsetPistol = nullptr;
	Vector3Property* c_ScopeOffsetSniper = nullptr;
	Vector3Property* c_ScopeOffsetRocket = nullptr;
	FloatProperty* c_LeftHandMeleeSwingSpeed = nullptr;
	FloatProperty* c_RightHandMeleeSwingSpeed = nullptr;
	BoolProperty* c_SinglePlayerMeleeWeaponAim = nullptr;
	FloatProperty* c_CrouchHeight = nullptr;
	BoolProperty* c_ShowRoomCentre = nullptr;
	BoolProperty* c_ToggleGrip = nullptr;
	FloatProperty* c_TwoHandDistance = nullptr;
	BoolProperty* c_LeftHanded = nullptr;
	FloatProperty* c_SwapHandDistance = nullptr;
	StringProperty* c_d3d9Path = nullptr;
	FloatProperty* c_WeaponSmoothingAmountNoZoom = nullptr;
	FloatProperty* c_WeaponSmoothingAmountOneZoom = nullptr;
	FloatProperty* c_WeaponSmoothingAmountTwoZoom = nullptr;
	FloatProperty* c_TEMPViewportLeft = nullptr;
	FloatProperty* c_TEMPViewportRight = nullptr;
	FloatProperty* c_TEMPViewportTop = nullptr;
	FloatProperty* c_TEMPViewportBottom = nullptr;
	BoolProperty* c_Use3DOFAiming = nullptr;
	BoolProperty* c_UseOriginalScope = nullptr;
	Vector3Property* c_3DOFWeaponOffset = nullptr;
	FloatProperty* c_3DOFWeaponSmoothingAmount = nullptr;
	FloatProperty* c_3DOFScopeScale = nullptr;
};
