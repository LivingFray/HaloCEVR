#include <d3d9.h>
#include "VREmulator.h"
#include "../Helpers/DX9.h"
#include "../Helpers/RenderTarget.h"
#include "../Helpers/Camera.h"
#include "../Helpers/Renderer.h"
#include "../Logger.h"
#include "../DirectXWrappers/IDirect3DDevice9ExWrapper.h"
#include "../Game.h"

template<typename T, std::size_t N>
constexpr std::size_t arraySize(T(&)[N]) {
	return N;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	} break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void VREmulator::Init()
{
	inputMoveHandFlat = RegisterVector2Input("default", "EMU_MoveHandFlat");
	inputMoveHandVert = RegisterVector2Input("default", "EMU_MoveHandVert");
	inputMoveHandSwap = RegisterBoolInput("default", "EMU_MoveHandSwap");

	mainHandOffset = Vector3(0.0f, -0.25f, -0.25f);
}

void VREmulator::OnGameFinishInit()
{
	HWND hWnd;
	WNDCLASSEX wc;
	HINSTANCE hInstance = GetModuleHandle(NULL);

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = L"WindowClass";

	RegisterClassEx(&wc);

	hWnd = CreateWindowEx(NULL, L"WindowClass", L"Mirror Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 600, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, SW_SHOW);

	if (!Helpers::GetDirect3D9())
	{
		Logger::log << "GetDirect3D9 returned null, aborting" << std::endl;
		return;
	}

	IDirect3DSurface9* gameSurface = nullptr;

	Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &gameSurface);

	D3DSURFACE_DESC desc;

	gameSurface->GetDesc(&desc);
	gameSurface->Release();

	D3DPRESENT_PARAMETERS present;
	ZeroMemory(&present, sizeof(present));
	present.Windowed = TRUE;
	present.SwapEffect = D3DSWAPEFFECT_DISCARD;
	present.hDeviceWindow = hWnd;
	present.BackBufferWidth = 1200;
	present.BackBufferHeight = 600;
	present.BackBufferFormat = desc.Format;
	present.MultiSampleQuality = desc.MultiSampleQuality;
	present.MultiSampleType = desc.MultiSampleType;

	HRESULT result = S_OK;

	result = Helpers::GetDirect3D9()->CreateDevice(0, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present, &mirrorDevice);

	if (FAILED(result))
	{
		Logger::log << "IDirect3DDevice9::CreateDevice failed: " << result << std::endl;
		return;
	}

	CreateSharedTarget();

	Logger::log << "Created Mirror D3D window" << std::endl;
}

void VREmulator::Shutdown()
{
}

void VREmulator::UpdatePoses()
{
	// Pretend we're waiting for the poses
	Sleep(2);
}

void VREmulator::UpdateCameraFrustum(CameraFrustum* frustum, int eye)
{
	// Emulate a 64mm IPD
	const float DIST = Game::instance.MetresToWorld(64.0f / 1000.0f);

	Vector3 rightVec = frustum->facingDirection.cross(frustum->upDirection);

	frustum->position += rightVec * DIST * (float)(2 * eye - 1);

	frustum->fov = 1.0472f; // 60 degrees
}

Matrix4 VREmulator::GetControllerTransform(ControllerRole role, bool bRenderPose)
{
	if (role == (Game::instance.bLeftHanded ? ControllerRole::Right : ControllerRole::Left))
	{
		return Matrix4().translate(0.0f, 0.25f, -0.25f);
	}
	else
	{
		Matrix4 trans;
		trans.rotateZ(mainHandRot.z);
		trans.rotateY(mainHandRot.y);
		trans.rotateX(mainHandRot.x);
		trans.translate(mainHandOffset);
		return trans;
	}
}

Matrix4 VREmulator::GetRawControllerTransform(ControllerRole role, bool bRenderPose)
{
    return GetControllerTransform(role, bRenderPose);
}

Matrix4 VREmulator::GetControllerBoneTransform(ControllerRole role, int bone, bool bRenderPose)
{
	return GetControllerTransform(role, bRenderPose);
}

Vector3 VREmulator::GetControllerVelocity(ControllerRole Role, bool bRenderPose)
{
	return Vector3();
}

bool VREmulator::TryGetControllerFacing(ControllerRole role, Vector3& outDirection)
{
    return false;
}

IDirect3DSurface9* VREmulator::GetRenderSurface(int eye)
{
	return eyeSurface_Game[eye][0];
}

IDirect3DTexture9* VREmulator::GetRenderTexture(int eye)
{
	return eyeTexture_Game[eye][0];
}

IDirect3DSurface9* VREmulator::GetUISurface()
{
	return uiSurface;
}

IDirect3DSurface9* VREmulator::GetCrosshairSurface()
{
	return crosshairSurface;
}

IDirect3DSurface9* VREmulator::GetScopeSurface()
{
	return scopeSurface;
}

IDirect3DTexture9* VREmulator::GetScopeTexture()
{
	return scopeTexture;
}

void VREmulator::SetCrosshairTransform(Matrix4& newTransform)
{
	Vector3 pos = (newTransform * Vector3(0.0f, 0.0f, 0.0f)) * Game::instance.MetresToWorld(1.0f) + Helpers::GetCamera().position;
	Matrix3 rot;
	Vector2 size(1.33f, 1.0f);
	size *= Game::instance.MetresToWorld(15.0f);

	newTransform.translate(-pos);
	newTransform.rotate(90.0f, newTransform.getLeftAxis());
	newTransform.rotate(-90.0f, newTransform.getUpAxis());
	newTransform.rotate(-90.0f, newTransform.getLeftAxis());

	for (int i = 0; i < 3; i++)
	{
		rot.setColumn(i, &newTransform.get()[i * 4]);
	}

	Game::instance.inGameRenderer.DrawRenderTarget(crosshairTexture, pos, rot, size, false);
}

void VREmulator::UpdateInputs()
{
	for (size_t i = 0; i < arraySize(bindings); i++)
	{
		bool bPressed = GetAsyncKeyState(bindings[i].virtualKey) & 0x8000;
		bindings[i].bHasChanged = bPressed != bindings[i].bPressed;
		bindings[i].bPressed = bPressed;
	}

	for (size_t i = 0; i < arraySize(axes1D); i++)
	{
		axes1D[i] = 0.0f;
	}

	for (size_t i = 0; i < arraySize(axisBindings); i++)
	{
		bool bPressed = GetAsyncKeyState(axisBindings[i].virtualKey) & 0x8000;
		if (bPressed)
		{
			axes1D[axisBindings[i].axisId] += axisBindings[i].scale * 1.0f;
		}
	}

	// Respond to fake inputs used to control gun hand

	Vector2 handMoveFlat = GetVector2Input(inputMoveHandFlat) * Game::instance.lastDeltaTime;
	Vector2 handMoveVert = GetVector2Input(inputMoveHandVert) * Game::instance.lastDeltaTime;

	// Swap between moving/rotating
	bool bhandModeChanged;
	bool bSwapHandMove = GetBoolInput(inputMoveHandSwap, bhandModeChanged);
	if (bhandModeChanged && bSwapHandMove)
	{
		bMoveHand ^= true;
	}

	constexpr float moveSpeed = 0.5f;
	constexpr float rotSpeed = 180.0f;

	if (bMoveHand)
	{
		mainHandOffset.x += handMoveFlat.x * moveSpeed;
		mainHandOffset.y += handMoveFlat.y * moveSpeed;
		mainHandOffset.z += handMoveVert.x * moveSpeed;
	}
	else
	{
		mainHandRot.x += handMoveFlat.x * rotSpeed;
		mainHandRot.y += handMoveFlat.y * rotSpeed;
		mainHandRot.z += handMoveVert.x * rotSpeed;
	}
}

InputBindingID VREmulator::RegisterBoolInput(std::string set, std::string action)
{
	for (size_t i = 0; i < arraySize(bindings); i++)
	{
		if (bindings[i].bindingName == action)
		{
			return i;
		}
	}
	return 999;
}

InputBindingID VREmulator::RegisterVector2Input(std::string set, std::string action)
{
	for (size_t i = 0; i < arraySize(axes2D); i++)
	{
		if (axes2D[i].axisName == action)
		{
			return i;
		}
	}
	return 999;
}

bool VREmulator::GetBoolInput(InputBindingID id)
{
	static bool bDummy = false;
	return GetBoolInput(id, bDummy);
}

bool VREmulator::GetBoolInput(InputBindingID id, bool& bHasChanged)
{
	if (id < arraySize(bindings))
	{
		bHasChanged = bindings[id].bHasChanged;
		return bindings[id].bPressed;
	}
	return false;
}

Vector2 VREmulator::GetVector2Input(InputBindingID id)
{
	if (id < arraySize(axes2D))
	{
		return Vector2(axes1D[axes2D[id].indexX], axes1D[axes2D[id].indexY]);
	}
	return Vector2();
}

void VREmulator::PreDrawFrame(struct Renderer* renderer, float deltaTime)
{
	if (!mirrorDevice)
	{
		Logger::log << "No Mirror Device, can't draw" << std::endl;
		return;
	}

	HRESULT result;

	result = mirrorDevice->BeginScene();
	if (FAILED(result))
	{
		Logger::log << "Failed to call mirror begin scene: " << result << std::endl;
	}

	Vector3 camPos = Helpers::GetCamera().position;

	Vector3 pos = Helpers::GetCamera().position + Helpers::GetCamera().lookDir * Game::instance.MetresToWorld(3.0f);

	Matrix3 rot;
	Vector2 size(1.33f, 1.0f);
	size *= Game::instance.MetresToWorld(2.0f);

	Matrix4 overlayTransform;
	overlayTransform.translate(pos);
	overlayTransform.lookAt(camPos, Vector3(0.0f, 0.0f, 1.0f));

	overlayTransform.translate(-pos);
	overlayTransform.rotate(-90.0f, overlayTransform.getLeftAxis());
	overlayTransform.translate(camPos);

	for (int i = 0; i < 3; i++)
	{
		rot.setColumn(i, &overlayTransform.get()[i * 4]);
	}

	Game::instance.inGameRenderer.DrawRenderTarget(uiTexture, pos, rot, size, false);
	//Game::instance.inGameRenderer.DrawCoordinate(pos, rot, 0.05f, false);
}

void VREmulator::DrawEye(struct Renderer* renderer, float deltaTime, int eye)
{
	IDirect3DSurface9* mirrorSurface = nullptr;
	HRESULT result = mirrorDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mirrorSurface);

	if (FAILED(result))
	{
		Logger::log << "Failed to get mirror back buffer: " << result << std::endl;
	}

	D3DSURFACE_DESC mirrorDesc;

	mirrorSurface->GetDesc(&mirrorDesc);

	UINT halfWidth = mirrorDesc.Width / 2;

	RECT rcDest;
	rcDest.left = eye * halfWidth;
	rcDest.top = 0;
	rcDest.right = halfWidth + eye * halfWidth;
	rcDest.bottom = mirrorDesc.Height;

	// Flip eyes for cross eyed
	result = mirrorDevice->StretchRect(eyeSurface_VR[1 - eye][0], nullptr, mirrorSurface, &rcDest, D3DTEXF_NONE);
	
	if (FAILED(result))
	{
		Logger::log << "Failed to copy from shared texture (" << eye << "): " << result << std::endl;
	}

	mirrorSurface->Release();
}

void VREmulator::PostDrawFrame(struct Renderer* renderer, float deltaTime)
{
	IDirect3DQuery9* pEventQuery = nullptr;
	Helpers::GetDirect3DDevice9()->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);
	if (pEventQuery != nullptr)
	{
		pEventQuery->Issue(D3DISSUE_END);
		while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
		pEventQuery->Release();
	}

	DrawEye(renderer, deltaTime, 0);
	DrawEye(renderer, deltaTime, 1);

	HRESULT result = mirrorDevice->EndScene();
	if (FAILED(result))
	{
		Logger::log << "Failed to end scene: " << result << std::endl;
	}

	result = mirrorDevice->Present(nullptr, nullptr, nullptr, nullptr);
	if (FAILED(result))
	{
		Logger::log << "Failed to present mirror view: " << result << std::endl;
	}
}

void VREmulator::CreateSharedTarget()
{
	D3DSURFACE_DESC desc;

	Helpers::GetRenderTargets()[0].renderSurface->GetDesc(&desc);
		
	D3DSURFACE_DESC desc2;

	Helpers::GetRenderTargets()[1].renderSurface->GetDesc(&desc2);

	const UINT width = GetViewWidth();
	const UINT height = GetViewHeight();

	CreateTexAndSurface(0, width, height, desc.Usage, desc.Format);
	CreateTexAndSurface(1, width, height, desc.Usage, desc.Format);

	HRESULT res = Helpers::GetDirect3DDevice9()->CreateTexture(Game::instance.c_UIOverlayWidth->Value(), Game::instance.c_UIOverlayHeight->Value(), 1, desc2.Usage, desc2.Format, D3DPOOL_DEFAULT, &uiTexture, NULL);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create UI texture: " << res << std::endl;
	}

	res = uiTexture->GetSurfaceLevel(0, &uiSurface);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create UI render target: " << res << std::endl;
	}

	res = Helpers::GetDirect3DDevice9()->CreateTexture(Game::instance.c_UIOverlayWidth->Value(), Game::instance.c_UIOverlayHeight->Value(), 1, desc2.Usage, desc2.Format, D3DPOOL_DEFAULT, &crosshairTexture, NULL);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create crosshair texture: " << res << std::endl;
	}

	res = crosshairTexture->GetSurfaceLevel(0, &crosshairSurface);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create crosshair render target: " << res << std::endl;
	}

	res = Helpers::GetDirect3DDevice9()->CreateTexture(GetScopeWidth(), GetScopeHeight(), 1, desc.Usage, desc.Format, D3DPOOL_DEFAULT, &scopeTexture, NULL);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create scope texture: " << res << std::endl;
	}

	res = scopeTexture->GetSurfaceLevel(0, &scopeSurface);

	if (FAILED(res))
	{
		Logger::log << "Couldn't create scope render target: " << res << std::endl;
	}
}


void VREmulator::CreateTexAndSurface(int index, UINT width, UINT height, DWORD usage, D3DFORMAT format)
{
	for (int i = 0; i < 2; i++)
	{
		HANDLE sharedHandle = nullptr;

		// Create texture on game

		HRESULT result = Helpers::GetDirect3DDevice9()->CreateTexture(width, height, 1, usage, format, D3DPOOL_DEFAULT, &eyeTexture_Game[i][index], &sharedHandle);
		if (FAILED(result))
		{
			Logger::log << "Failed to create game " << index << " texture: " << result << std::endl;
		}

		// Created shared texture on vr
		result = mirrorDevice->CreateTexture(width, height, 1, usage, format, D3DPOOL_DEFAULT, &eyeTexture_VR[i][index], &sharedHandle);
		if (FAILED(result))
		{
			Logger::log << "Failed to create vr " << index << " texture: " << result << std::endl;
		}

		// Extract game surface
		result = eyeTexture_Game[i][index]->GetSurfaceLevel(0, &eyeSurface_Game[i][index]);

		if (FAILED(result))
		{
			Logger::log << "Failed to get game render surface from " << index << ": " << result << std::endl;
		}

		// Extract vr surface
		result = eyeTexture_VR[i][index]->GetSurfaceLevel(0, &eyeSurface_VR[i][index]);

		if (FAILED(result))
		{
			Logger::log << "Failed to get vr render surface from " << index << ": " << result << std::endl;
		}
	}
}
