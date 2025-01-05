#pragma once
#include <string>
#include <wtypes.h>
#include <Psapi.h>

struct Offset
{
	// Name (used for debugging only)
	std::string DebugName;
	// Offset of the function in memory
	long long Offset;
	// Hex Signature of the function (?/?? signifies an unknown value)
	std::string Signature;
	// Offset of the start of the signature from the function entry point
	long long SignatureOffset = 0;
	// The actual address, including the module offset
	long long Address;
};

#define INDIRECT(Name, KnownOffset, ByteOffset, Signature) Offset I_##Name = {#Name, KnownOffset, Signature}; long long Name = ByteOffset
#define OFFSET(Name, KnownOffset, Signature) Offset Name = {#Name, KnownOffset, Signature}

class Offsets
{
public:
	INDIRECT(GameVersion,         0x138056, 0x01, "A1 ?? ?? ?? ?? 81 C2 04 01 00 00 89 02");
	INDIRECT(GameType,            0x14192e, 0x05, "52 8d 75 d0");
	INDIRECT(AssetsArray,         0x042455, 0x4B, "74 31 57 0f bf f8 69 ff 0c 08 00 00 81 c7 ?? ?? ?? ?? e8");
	INDIRECT(Controls,            0x08cca0, 0x12, "a1 ?? ?? ?? ?? 83 ec ?? 53 55 56 57 b9 ?? ?? ?? ?? be");
	INDIRECT(DirectX9,            0x1169c0, 0xE4, "83 ec 60 8b 15 ?? ?? ?? ?? 8b 0d ?? ?? ?? ?? 53 33 db");
	INDIRECT(DirectX9Device,      0x1169c0, 0x4B, "83 ec 60 8b 15 ?? ?? ?? ?? 8b 0d ?? ?? ?? ?? 53 33 db");
	INDIRECT(HideMouse,           0x045640, 0x23, "8a 0d ?? ?? ?? ?? 8b 44 24 04 8b 15 ?? ?? ?? ?? 81 ec 90 00 00 00");
	INDIRECT(ObjectTable,         0x0f4ad0, 0x32, "51 53 56 57 e8 ?? ?? ?? ?? e8 ?? ?? ?? ?? e8 ?? ?? ?? ?? 68 00 08 00 00 68 ?? ?? ?? ?? bb 0c 00 00 00");
	INDIRECT(PlayerTable,         0x0735b0, 0x21, "51 53 56 6a 10 68 ?? ?? ?? ?? bb 00 02 00 00 e8 ?? ?? ?? ?? 6a 10");
	INDIRECT(LocalPlayer,         0x047880, 0x0F, "8b 44 24 04 8b 0d ?? ?? ?? ?? 83 ec 24 56 a3 ?? ?? ?? ?? 8b 71 04");
	INDIRECT(WindowRect,          0x0ca1a0, 0x3E, "66 a1 ?? ?? ?? ?? 81 ec 18 03 00 00 66 3d 01 00 53 55 56 57");
	INDIRECT(RenderTargets,       0x12ccc0, 0x1E, "83 ec 38 56 57 66 8b f0 33 ff 66 83 fe 09 ?? ?? 66 85 f6");
	INDIRECT(CameraMatrices,      0x10bfb0, 0x5A, "81 ec a0 02 00 00 53 55 8b ac 24 ac 02 00 00 56 8b 35");
	INDIRECT(InputData,           0x0735b0, 0xAD, "51 53 56 6a 10 68 ?? ?? ?? ?? bb 00 02 00 00 e8 ?? ?? ?? ?? 6a 10");
	INDIRECT(CurrentRect,         0x12ccc0, 0x51, "83 ec 38 56 57 66 8b f0 33 ff 66 83 fe 09 ?? ?? 66 85 f6");
	INDIRECT(LoadingState,        0x097410, 0x01, "a1 ?? ?? ?? ?? 81 ec 18 04 00 00 57 33 ff 3b c7 0f 84 ?? ?? ?? ?? a1 ?? ?? ?? ?? 56 83 ce ff");
	INDIRECT(CmdLineArgs,         0x140ee0, 0x5F, "53 57 6a 01 33 db ff 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? ff 15 ?? ?? ?? ?? 33 c0 b9 41 00 00 00");
	INDIRECT(IsWindowed,          0x1169c0, 0x0B, "83 ec 60 8b 15 ?? ?? ?? ?? 8b 0d ?? ?? ?? ?? 53 33 db");
	INDIRECT(CutsceneData,        0x07f8d5, 0x0D, "83 c4 10 85 c0 74 25 8a 00 84 c0");
	INDIRECT(CampaignLoading,     0x0c7886, 0x02, "38 1d ?? ?? ?? ?? 74 ?? e8 ?? ?? ?? ?? 38 1d ?? ?? ?? ?? 74 ?? e8");

	OFFSET(TabOutVideo,           0x0c7b74, "38 1D ?? ?? ?? ?? 74 0E 83 FD 01 74 09 83 FD 02 0F 85");
	OFFSET(TabOutVideo2,          0x0c801c, "75 11 38 1D ?? ?? ?? ?? 75 04 3A C3 74 05 C6 44 24 17 01");
	OFFSET(TabOutVideo3,          0x0c829c, "38 1D ?? ?? ?? ?? 74 29 0F BF 05 ?? ?? ?? ?? 3B C3 7E 0E");
	OFFSET(CutsceneFPSCap,        0x0c9fb5, "b3 01 eb ?? 32 db 8b 2d ?? ?? ?? ?? 8d 4c");
	OFFSET(CreateMouseDevice,     0x0919c0, "6a 17 ff 15 ?? ?? ?? ?? 85 c0 74 ?? 66 c7 05 ?? ?? ?? ?? 02 00");
	OFFSET(SetViewModelVisible,   0x092430, "51 8b 0d ?? ?? ?? ?? 66 83 f9 ff 74 ?? 8b 15 ?? ?? ?? ?? 56 0f bf f1");
	OFFSET(TextureAlphaWrite,     0x11c9d1, "6a 01 6a 16 33 ?? 50 89 ?? ?? ?? ff 91 e4 00 00 00 a1 ?? ?? ?? ?? 8b 10 6a 07 68 a8 00 00 00 50 ff 92 e4 00 00 00 a1 ?? ?? ?? ?? 8b 08 6a 01 6a 1b 50");
	OFFSET(TextAlphaWrite,        0x131b80, "a0 ?? ?? ?? ?? 84 c0 0f 84 ?? ?? ?? ?? 66 83 ?? ?? ?? ?? 00 01 0f 85 ?? ?? ?? ?? 53 55 56");
	OFFSET(CrouchHeight,          0x168ff0, "d9 86 0c 05 00 00 f6 86 cc 04 00 00 01 75 ?? d8 15");
	OFFSET(CinematicBarDrawCall,  0x049b25, "e8 ?? ?? ?? ?? 0f bf 15 ?? ?? ?? ?? 89 54 24 18 db 44 24 18 d9 5c 24 18 d9 44 24 18");

	OFFSET(InitDirectX,           0x1169c0, "83 ec 60 8b 15 ?? ?? ?? ?? 8b 0d ?? ?? ?? ?? 53 33 db");
	OFFSET(DrawFrame,             0x10bea0, "83 ec 14 8b 15 ?? ?? ?? ?? 8b 44 24 24 8b 4c 24 28 42");
	OFFSET(DrawHUD,               0x094730, "83 ec 40 66 83 3d ?? ?? ?? ?? ff ?? ?? ?? ?? ?? ?? 8d 04 24");
	OFFSET(DrawMenu,              0x0984c0, "33 c9 83 ec 14 66 3d ff ff 0f 94 c1 53 33 db 49 23 c8");
	OFFSET(DrawLoadingScreen,     0x10bdc0, "55 8b ec 83 e4 f8 81 ec 5c 02 00 00 53 56 8b 75 08");
	OFFSET(DrawFrameMPLoading,    0x10c590, "8b 15 ?? ?? ?? ?? 81 ec 70 02 00 00 53 56 42 57");
	OFFSET(DrawCrosshair,         0x0acad0, "83 ec 28 84 db 55 8b 6c 24 38 56 8b 74 24 38 57 8b f8");
	OFFSET(DrawImage,             0x11c9a0, "81 ec ?? ?? 00 00 56 8b f0 a0 ?? ?? ?? ?? 84 c0");
	OFFSET(DrawLoadingScreen2,    0x097410, "a1 ?? ?? ?? ?? 81 ec 18 04 00 00 57 33 ff 3b c7 0f 84 ?? ?? ?? ?? a1 ?? ?? ?? ?? 56 83 ce ff");
	OFFSET(DrawCinematicBars,     0x0499c0, "8b 15 ?? ?? ?? ?? 8a 4a 08 83 ec 34 84 c9 53 55 56 57");
	OFFSET(DrawViewModel,	      0x0924b0, "66 8b 0d ?? ?? ?? ?? 81 ec 3c 0d 00 00 66 83 f9 ff 53 55 56 57");

	OFFSET(SetViewModelPosition,  0x0d6880, "81 ec f0 00 00 00 53 55 25 ff ff 00 00 56 8b f1 8b 0d");
	OFFSET(HandleInputs,          0x08b4b0, "83 ec 08 56 57 8d 44 24 08 50 ff 15 ?? ?? ?? ?? 8b 4c 24 0c");
	OFFSET(UpdatePitchYaw,        0x072160, "81 ec a4 00 00 00 8b 15 ?? ?? ?? ?? 53 0f bf c8");
	OFFSET(SetMousePosition,      0x097250, "56 8b 35 ?? ?? ?? ?? 33 d2 3b f0 75 ?? 39 0d");
	OFFSET(UpdateMouseInfo,       0x091bc0, "8b 01 53 55 8b 6c 24 0c 89 45 00 8b 51 04 f7 da 56");
	OFFSET(FireWeapon,            0x0c3f10, "81 ec 94 00 00 00 8b 84 24 98 00 00 00 8b 0d ?? ?? ?? ?? 8b 51 34 53");
	OFFSET(ThrowGrenade,          0x16e440, "8b 44 24 04 8b 0d ?? ?? ?? ?? 8b 51 34 8b 0d ?? ?? ?? ?? 83 ec 3c");
	OFFSET(ReloadStart,           0x0c35b0, "83 ec 0c 8b 54 24 10 53 55 0f bf 6c 24 1c 56 8b 35");
	OFFSET(ReloadEnd,             0x0c3900, "8b 44 24 04 8b 15 ?? ?? ?? ?? 8b 52 34 53 8b 1d ?? ?? ?? ?? 25 ff ff 00 00 55 56");
	
	OFFSET(SetViewportSize,       0x0c8da0, "83 ec 10 53 55 56 57 8b f8 33 c0 83 ff 01 0f 9e c0");
	OFFSET(SetViewportScale,      0x10ca90, "51 0f bf 50 2e 56 0f bf 70 30 57 0f bf 78 2c 2b f7");
	OFFSET(SetCameraMatrices,     0x10cc40, "83 ec 54 53 55 57 8b f9 0f bf 57 2e 0f bf 4f 32 2b ca");
};

class SigScanner
{
public:

	// Try to update the offset based on its signature
	// Returns -1 if unsuccessful, 0 if offset already matched, NewOffset if updated
	static long long UpdateOffset(struct Offset& Offset, bool bErrorOnFail = true);
};

#undef OFFSET
#undef INDIRECT