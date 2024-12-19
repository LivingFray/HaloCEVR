#pragma once
#include <d3d9.h>
#include "Hook.h"
#include "SigScanner.h"
#include "FunctionTypeDefs.h"
#include "../Helpers/Renderer.h"
#include "../Helpers/Objects.h"

#define DEFINE_HOOK_FULL(Name, RetCall, ...) static inline Hook<Func_##Name> Name; static RetCall H_##Name(__VA_ARGS__)
#define DEFINE_HOOK(Name, ...) DEFINE_HOOK_FULL(Name, void, __VA_ARGS__)

class Hooks
{
public:
	static void InitHooks();
	static void EnableAllHooks();

	static void SetByte(long long Address, byte Byte);
	static void SetBytes(long long Address, int Length, byte* Bytes);
	static void NOPInstructions(long long Address, int Length);
	static bool Freeze();
	static void Unfreeze();
	static void ResolveIndirect(struct Offset& offset, long long& Address);

	// All Hooks go here:
	DEFINE_HOOK_FULL(InitDirectX, bool);
	DEFINE_HOOK_FULL(DrawFrame, void, Renderer* param1, short param2, short* param3, float param4, float deltaTime);
	DEFINE_HOOK(DrawHUD);
	DEFINE_HOOK(DrawMenu);
	DEFINE_HOOK(DrawLoadingScreen);
	DEFINE_HOOK(DrawCrosshair);
	DEFINE_HOOK(DrawImage);
	DEFINE_HOOK(SetViewModelPosition);
	DEFINE_HOOK(HandleInputs);
	DEFINE_HOOK(UpdatePitchYaw);
	DEFINE_HOOK(SetViewportScale);
	DEFINE_HOOK(SetMousePosition);
	DEFINE_HOOK(UpdateMouseInfo);
	DEFINE_HOOK_FULL(FireWeapon, void __cdecl, HaloID param1, short param2);
	DEFINE_HOOK_FULL(ThrowGrenade, void __cdecl, HaloID param1, bool param2);
	DEFINE_HOOK_FULL(DrawLoadingScreen2, void __fastcall, void* param1);
	DEFINE_HOOK(DrawCinematicBars);
	DEFINE_HOOK(DrawViewModel);
	DEFINE_HOOK_FULL(ReloadStart, void __cdecl, HaloID param1, short param2, bool param3);
	DEFINE_HOOK(ReloadEnd);

	// All direct patches go here:
	static void P_FixTabOut();
	static void P_RemoveCutsceneFPSCap();
	static void P_KeepViewModelVisible(bool bAlwaysShow);
	static void P_EnableUIAlphaWrite();
	static void P_DisableCrouchCamera();
	static void P_ForceCmdLineArgs();
	static void P_RemoveCinematicBars();

	static void P_DontStealMouse();

	static void SetCameraMatrices(struct Viewport* viewport, struct CameraFrustum* frustum, struct CameraRenderMatrices* crm, bool bDoProjection);

	static inline Offsets o;

protected:
	static inline Func_SetCameraMatrices oSetCameraMatrices = nullptr;
};

#undef DEFINE_HOOK
#undef DEFINE_HOOK_FULL