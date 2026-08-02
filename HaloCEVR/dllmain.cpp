#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "Logger.h"
#include "Game.h"
#include "DirectXWrappers/IDirect3D9ExWrapper.h"

#pragma comment(lib, "d3d9.lib")

typedef IDirect3D9* (WINAPI *LPD3D_Direct3DCreate9)(UINT nSDKVersion);
typedef HRESULT (WINAPI *LPD3D_Direct3DCreate9Ex)(UINT nSDKVersion, IDirect3D9Ex**);
typedef void(WINAPI* LPD3D_Direct3DShaderValidatorCreate9)(void);

bool bHasInit = false;
HMODULE dllHandle;
LPD3D_Direct3DCreate9 fnDirect3DCreate9 = nullptr;
LPD3D_Direct3DCreate9Ex fnDirect3DCreate9Ex = nullptr;

Logger Logger::log("VR/inject.log");
Logger::LoggerAlert Logger::err(&Logger::log);
Game Game::instance;

BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Logger::log << "[DLL] HaloCEVR attached" << std::endl;
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		Game::instance.Shutdown();
		bHasInit = false;
		break;
	}
	return TRUE;
}

//==== DLL proxying stuff below ====//

bool LoadRealDLL()
{
	if (bHasInit)
	{
		Logger::log << "[DLL] Multiple calls to LoadRealDLL, returning existing handle: " << dllHandle << std::endl;
		return dllHandle != NULL;
	}
	bHasInit = true;
	Game::instance.Init();

	if (Game::instance.c_d3d9Path->Value() != "")
	{
		dllHandle = LoadLibraryA(Game::instance.c_d3d9Path->Value().c_str());

		if (dllHandle)
		{
			Logger::log << "[DLL] Loading real DirectX9 runtime from " << Game::instance.c_d3d9Path->Value() << std::endl;
		}
		else
		{
			Logger::log << "[DLL] Could not load real Directx9 runtime from '" << Game::instance.c_d3d9Path->Value() << "'. Falling back to System32's version" << std::endl;
		}
	}

	if (!dllHandle)
	{
		char buffer[MAX_PATH + 1];

		GetSystemDirectoryA(buffer, sizeof(buffer));
		buffer[MAX_PATH] = 0;
		std::string dllFileName = buffer;
		dllFileName += "\\d3d9.dll";

		Logger::log << "[DLL] Loading real DirectX9 runtime from " << dllFileName << std::endl;

		dllHandle = LoadLibraryA(dllFileName.c_str());
	}

	if (!dllHandle)
	{
		Logger::err << "Could not load the real d3d9.dll file from the system directory" << std::endl;
		return false;
	}

	fnDirect3DCreate9 = reinterpret_cast<LPD3D_Direct3DCreate9>(GetProcAddress(dllHandle, "Direct3DCreate9"));
	fnDirect3DCreate9Ex = reinterpret_cast<LPD3D_Direct3DCreate9Ex>(GetProcAddress(dllHandle, "Direct3DCreate9Ex"));

	if (!fnDirect3DCreate9 || !fnDirect3DCreate9Ex)
	{
		FreeLibrary(dllHandle);
		dllHandle = nullptr;
		Logger::err << "Could not get create functions from real d3d9.dll" << std::endl;
		return false;
	}

	Logger::log << "[DLL] Loaded real d3d9.dll" << std::endl;
	return true;
}

#pragma comment(linker, "/export:Direct3DCreate9Ex=?H_Direct3DCreate9Ex@@YGJIPAPAUIDirect3D9Ex@@@Z")
HRESULT WINAPI H_Direct3DCreate9Ex(UINT nSDKVersion, IDirect3D9Ex** outDirect3D9Ex)
{
	Logger::log << "[DLL] Intercepting call to Direct3dCreate9Ex" << std::endl;
	if (!LoadRealDLL())
	{
		Logger::err << "[DLL] Failed to load real d3d9.dll" << std::endl;
		return D3DERR_NOTAVAILABLE;
	}

	IDirect3D9Ex* RealD3D = nullptr;

	HRESULT result = fnDirect3DCreate9Ex(nSDKVersion, &RealD3D);

	if (FAILED(result))
	{
		Logger::err << "Call to Direct3DCreate9Ex Failed: " << result << std::endl;
		return result;
	}

	*outDirect3D9Ex = new IDirect3D9ExWrapper(RealD3D);

	return S_OK;
}

#pragma comment(linker, "/export:Direct3DCreate9=?H_Direct3DCreate9@@YGPAUIDirect3D9@@I@Z")
IDirect3D9* WINAPI H_Direct3DCreate9(UINT nSDKVersion)
{
	Logger::log << "[DLL] Intercepting call to Direct3dCreate9" << std::endl;
	if (!LoadRealDLL())
	{
		Logger::err << "[DLL] Failed to load real d3d9.dll" << std::endl;
		return nullptr;
	}

	IDirect3D9Ex* d3dEx = nullptr;
	HRESULT result = H_Direct3DCreate9Ex(nSDKVersion, &d3dEx);

	if (SUCCEEDED(result))
	{
		int numAdapters = d3dEx->GetAdapterCount();
		Logger::log << "Adapters: " << numAdapters << std::endl;

		D3DDISPLAYMODE outMode;

		for (int i = 0; i < numAdapters; i++)
		{
			Logger::log << "Adapter " << i << ": " << (uint32_t) d3dEx->GetAdapterDisplayMode(i, &outMode) << " " << outMode.Width << "x" << outMode.Height << " " << (outMode.Format) << std::endl;
		}
	}

	return d3dEx;
}
