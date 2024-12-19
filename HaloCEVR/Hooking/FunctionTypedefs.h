#pragma once
#include "../Helpers/Objects.h"

typedef void(*Func_UpdateCameraRotation)();
typedef bool(*Func_InitDirectX)();
typedef void(__cdecl* Func_DrawFrame)(struct Renderer*, short, short*, float, float);
typedef void(*Func_SetViewportSize)();
typedef void(__stdcall* Func_DrawHUD)();
typedef void(*Func_DrawMenu)();
typedef void(__stdcall* Func_DrawScope)(void*);
typedef void(*Func_DrawLoadingScreen)();
typedef void(*Func_DrawCrosshair)();
typedef void(*Func_DrawImage)();
typedef void(*Func_SetViewModelPosition)();
typedef void(*Func_HandleInputs)();
typedef void(*Func_UpdatePitchYaw)();
typedef void(*Func_SetViewportScale)();
typedef void(*Func_SetMousePosition)();
typedef void(*Func_UpdateMouseInfo)();
typedef void(__cdecl* Func_FireWeapon)(HaloID, short);
typedef void(__cdecl* Func_ThrowGrenade)(HaloID, bool);
typedef void(*Func_SetCameraMatrices)();
typedef int(__cdecl* Func_MaybeNightVision)(float* param1);
typedef void(__fastcall* Func_DrawLoadingScreen2)(void* parma1);
typedef void(*Func_DrawCinematicBars)();
typedef void(*Func_DrawViewModel)();
typedef void(__cdecl* Func_ReloadStart)(HaloID, short, bool);
typedef void(*Func_ReloadEnd)();