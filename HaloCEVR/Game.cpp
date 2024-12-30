#define EMULATE_VR 0
#include "Game.h"
#include "Logger.h"
#include "Hooking/Hooks.h"
#include "Helpers/DX9.h"
#include "Helpers/RenderTarget.h"
#include "Helpers/Renderer.h"
#include "Helpers/Camera.h"
#include "Helpers/Menus.h"
#include "Helpers/Objects.h"
#include "Helpers/Maths.h"

#if EMULATE_VR
#include "VR/VREmulator.h"
#else
#include "VR/OpenVR.h"
#endif

#if USE_PROFILER
#include <algorithm>
#endif


void Game::Init()
{
	Logger::log << "[Game] HaloCEVR initialising..." << std::endl;

#if USE_PROFILER
	profiler.Init();
#endif

	SetupConfigs();

	CreateConsole();

	PatchGame();

#if EMULATE_VR
	vr = new VREmulator();
#else
	vr = new OpenVR();
#endif

	vr->Init();

	Game::instance.bLeftHanded = Game::instance.c_LeftHanded->Value();
	inputHandler.RegisterInputs();

	backBufferWidth = vr->GetViewWidth();
	backBufferHeight = vr->GetViewHeight();

	bHasShutdown = false;

	Logger::log << "[Game] HaloCEVR initialised" << std::endl;
}

void Game::Shutdown()
{
	if (bHasShutdown)
	{
		return;
	}
	bHasShutdown = true;

	Logger::log << "[Game] HaloCEVR shutting down..." << std::endl;

	vr->Shutdown();

#if USE_PROFILER
	profiler.Shutdown();
#endif

	MH_STATUS hookStatus = MH_DisableHook(MH_ALL_HOOKS);

	if (hookStatus != MH_OK)
	{
		Logger::log << "[Game] Could not remove hooks: " << MH_StatusToString(hookStatus) << std::endl;
	}

	hookStatus = MH_Uninitialize();

	if (hookStatus != MH_OK)
	{
		Logger::log << "[Game] Could not uninitialise MinHook: " << MH_StatusToString(hookStatus) << std::endl;
	}

	if (c_ShowConsole && c_ShowConsole->Value())
	{
		if (consoleOut)
		{
			fclose(consoleOut);
		}
		FreeConsole();
	}
}

void Game::OnInitDirectX()
{
	Logger::log << "[Game] Game has finished DirectX initialisation" << std::endl;

	if (!Helpers::GetDirect3DDevice9())
	{
		Logger::err << "Couldn't get game's direct3d device" << std::endl;
		return;
	}

	SetForegroundWindow(GetActiveWindow());

	vr->OnGameFinishInit();

	uiSurface = vr->GetUISurface();
	crosshairSurface = vr->GetCrosshairSurface();

	scopeSurfaces[0] = vr->GetScopeSurface();
	scopeTextures[0] = vr->GetScopeTexture();

	D3DSURFACE_DESC desc;
	scopeSurfaces[0]->GetDesc(&desc);

	CreateTextureAndSurface(desc.Width, desc.Height, desc.Usage, desc.Format, &scopeSurfaces[1], &scopeTextures[1]);
	CreateTextureAndSurface(desc.Width / 2, desc.Height / 2, desc.Usage, desc.Format, &scopeSurfaces[2], &scopeTextures[2]);
}

void Game::PreDrawFrame(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawFrame);

	lastDeltaTime = deltaTime;

	renderState = ERenderState::UNKNOWN;

	bool bIsLoading = Helpers::IsCampaignLoading();

	if (bWasLoading && !bIsLoading)
	{
		bNeedsRecentre = true;
	}
	bWasLoading = bIsLoading;

	//CalcFPS(deltaTime);

	vr->SetMouseVisibility(Helpers::IsMouseVisible());
	vr->UpdatePoses();

	UpdateCrosshairAndScope();

	StoreRenderTargets();

	sRect* window = Helpers::GetWindowRect();
	window->top = 0;
	window->left = 0;
	window->right = vr->GetViewWidth();
	window->bottom = vr->GetViewHeight();

	VR_PROFILE_START(Game_PreDrawFrame_ClearSurfaces);
	// Clear UI surfaces
	IDirect3DSurface9* currentSurface = nullptr;
	Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &currentSurface);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, crosshairSurface);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, scopeSurfaces[0]);
	Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, currentSurface);
	currentSurface->Release();
	VR_PROFILE_STOP(Game_PreDrawFrame_ClearSurfaces);

	frustum1 = renderer->frustum;
	frustum2 = renderer->frustum2;

	if (bNeedsRecentre)
	{
		bNeedsRecentre = false;
		vr->Recentre();
	}

	UnitDynamicObject* Player = static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());
	if (Player)
	{
		bool bNewShowViewModel = Player->parent.id != 0xffff;

		if (bNewShowViewModel != bShowViewModel)
		{
			// Self modifying code is the best code
			Hooks::P_KeepViewModelVisible(bNewShowViewModel);

			bShowViewModel = bNewShowViewModel;
		}
		bInVehicle = bNewShowViewModel;
		bHasWeapon = Player->weapon.id != 0xffff;
	}

	if (c_ShowRoomCentre->Value())
	{
		VR_PROFILE_SCOPE(Game_PreDrawFrame_DrawRoomCentre);

		Vector3 position = Helpers::GetCamera().position;
		position.z -= 0.62f;
		Vector3 upVector(0.0f, 0.0f, 1.0f);
		Vector3 forwardVector(1.0f, 0.0f, 0.0f);

		inGameRenderer.DrawPolygon(position, upVector, forwardVector, 8, MetresToWorld(0.25f), D3DCOLOR_ARGB(50, 85, 250, 239), false);
	}

#if 0
	// Debug draw controller position (why, oh why, does steamvr make getting a consistent controller position/orientation so hard?)
	Matrix4 controller = vr->GetControllerTransform(ControllerRole::Right);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &controller.get()[i * 4]);
	}

	Vector3 worldPos = Helpers::GetCamera().position;

	inGameRenderer.DrawCoordinate(controller * Vector3(0.0f, 0.0f, 0.0f) * MetresToWorld(1.0f) + worldPos, handRotation3, 0.05f, false);

	for (int i = 0; i < 31; i++)
	{
		Matrix4 bone = vr->GetControllerBoneTransform(ControllerRole::Right, i);

		for (int i = 0; i < 3; i++)
		{
			handRotation3.setColumn(i, &bone.get()[i * 4]);
		}

		inGameRenderer.DrawCoordinate(bone * Vector3(0.0f, 0.0f, 0.0f) * MetresToWorld(1.0f) + worldPos, handRotation3, i == 1 ? 0.05f : 0.005f, false);
	}

	controller = vr->GetControllerTransform(ControllerRole::Right);

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &controller.get()[i * 4]);
	}

	inGameRenderer.DrawCoordinate(controller * Vector3(0.0f, 0.0f, 0.0f) * MetresToWorld(1.0f) + worldPos, handRotation3, 0.05f, false);
#endif

	vr->PreDrawFrame(renderer, deltaTime);
}

void Game::PreDrawEye(Renderer* renderer, float deltaTime, int eye)
{
	VR_PROFILE_SCOPE(Game_PreDrawEye);

	renderState = eye == 0 ? ERenderState::LEFT_EYE : ERenderState::RIGHT_EYE;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	vr->UpdateCameraFrustum(&renderer->frustum, eye);
	vr->UpdateCameraFrustum(&renderer->frustum2, eye);

	RenderTarget* primaryRenderTarget = Helpers::GetRenderTargets();

	primaryRenderTarget[0].renderSurface = vr->GetRenderSurface(eye);
	primaryRenderTarget[0].renderTexture = vr->GetRenderTexture(eye);
	primaryRenderTarget[0].width = vr->GetViewWidth();
	primaryRenderTarget[0].height = vr->GetViewHeight();

	inGameRenderer.ExtractMatrices(renderer);
}


void Game::PostDrawEye(struct Renderer* renderer, float deltaTime, int eye)
{
	VR_PROFILE_SCOPE(Game_PostDrawEye);

	if (Helpers::IsLoading() || Helpers::IsCampaignLoading())
	{
		Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	}

	inGameRenderer.Render(Helpers::GetDirect3DDevice9());
}

bool Game::PreDrawScope(Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawScope);

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetLocalPlayer());

	if (!player || player->zoom == -1)
	{
		return false;
	}

	renderState = ERenderState::SCOPE;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	Vector3 aimPos, aimDir, upDir;

	weaponHandler.GetWorldWeaponAim(aimPos, aimDir);

	upDir = Vector3(0.0f, 0.0f, 1.0f);
	upDir = aimDir.cross(upDir);
	upDir = upDir.cross(aimDir);

	renderer->frustum.position = aimPos;
	renderer->frustum2.position = aimPos;
	renderer->frustum.facingDirection = aimDir;
	renderer->frustum2.facingDirection = aimDir;
	renderer->frustum.upDirection = upDir;
	renderer->frustum2.upDirection = upDir;

	RenderTarget* primaryRenderTarget = Helpers::GetRenderTargets();

	primaryRenderTarget[0].renderSurface = scopeSurfaces[0];
	primaryRenderTarget[0].renderTexture = scopeTextures[0];
	primaryRenderTarget[0].width = vr->GetScopeWidth();
	primaryRenderTarget[0].height = vr->GetScopeHeight();
	primaryRenderTarget[1].renderSurface = scopeSurfaces[1];
	primaryRenderTarget[1].renderTexture = scopeTextures[1];
	primaryRenderTarget[1].width = vr->GetScopeWidth();
	primaryRenderTarget[1].height = vr->GetScopeHeight();
	primaryRenderTarget[2].renderSurface = scopeSurfaces[2];
	primaryRenderTarget[2].renderTexture = scopeTextures[2];
	primaryRenderTarget[2].width = vr->GetScopeWidth() / 2;
	primaryRenderTarget[2].height = vr->GetScopeHeight() / 2;


	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = vr->GetScopeWidth();
	windowMain->bottom = vr->GetScopeHeight();

	{
		sRect& window = renderer->frustum.WindowViewport;
		window.top = 0;
		window.left = 0;
		window.right = vr->GetScopeWidth();
		window.bottom = vr->GetScopeHeight();

		sRect& window2 = renderer->frustum2.WindowViewport;
		window2.top = 0;
		window2.left = 0;
		window2.right = vr->GetScopeWidth();
		window2.bottom = vr->GetScopeHeight();
	}

	return true;
}

void Game::PostDrawScope(Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PostDrawScope);

	RestoreRenderTargets();

	Vector2 innerSize = Vector2(0.0f, 0.0f);
	Vector2 size = Vector2(static_cast<float>(vr->GetScopeWidth()), static_cast<float>(vr->GetScopeHeight()));
	Vector2 centre = size / 2;
	int sides = 32;
	float radius = size.y * 0.25f;
	D3DCOLOR color = D3DCOLOR_ARGB(255, 0, 0, 0);

	scopeRenderer.ExtractMatrices(renderer);

	// Sniper scope is a rounded square, so we need to separate the quadrants and change the radius
	if (weaponHandler.IsSniperScope())
	{
		radius = size.y * 0.03125f;
		const float scopeWidth = 0.605f * size.x - radius * 2.0f;
		const float scopeHeight = 0.505f * size.y - radius * 2.0f;
		innerSize = Vector2(scopeWidth, scopeHeight);
	}

	scopeRenderer.DrawInvertedShape2D(centre, innerSize, size, sides, radius, color);

	scopeRenderer.Render(Helpers::GetDirect3DDevice9());
	scopeRenderer.PostRender();

}

void Game::PreDrawMirror(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PreDrawMirror);

	renderState = ERenderState::GAME;

	renderer->frustum = frustum1;
	renderer->frustum2 = frustum2;

	RestoreRenderTargets();

	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = Helpers::GetRenderTargets()[0].width;
	windowMain->bottom = Helpers::GetRenderTargets()[0].height;

	inGameRenderer.ExtractMatrices(renderer);
}

void Game::PostDrawMirror(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(Game_PostDrawMirror);

	// Do something here to copy the image into the backbuffer correctly

	inGameRenderer.ClearRenderTargets();
	inGameRenderer.Render(Helpers::GetDirect3DDevice9());
}

void Game::PostDrawFrame(struct Renderer* renderer, float deltaTime)
{
	VR_PROFILE_START(Game_PostDrawFrame);

	RestoreRenderTargets();
	vr->PostDrawFrame(renderer, deltaTime);
	inGameRenderer.PostRender();

	sRect* windowMain = Helpers::GetWindowRect();
	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = Helpers::GetRenderTargets()[0].width;
	windowMain->bottom = Helpers::GetRenderTargets()[0].height;

	if (c_DrawMirror->Value() && mirrorSource != ERenderState::GAME)
	{
		VR_PROFILE_SCOPE(Game_PostDrawFrame_Mirror);

		int sWidth = vr->GetViewWidth();
		int sHeight = vr->GetViewHeight();

		float sourceAspect = static_cast<float>(sWidth) / static_cast<float>(sHeight);

		int dWidth = Helpers::GetRenderTargets()[0].width;
		int dHeight = Helpers::GetRenderTargets()[0].height;

		int trueWidth = 640;
		int trueHeight = 480;

		float destAspect = static_cast<float>(trueWidth) / static_cast<float>(trueHeight);

		RECT destRect{};

		if (sourceAspect > destAspect)
		{
			destRect.left = 0;
			destRect.right = dWidth;

			float scale = destAspect / sourceAspect;

			int scaledSize = static_cast<int>(0.5f * (1.0f - scale) * dHeight);

			destRect.top = scaledSize;
			destRect.bottom = dHeight - scaledSize;
		}
		else
		{
			destRect.top = 0;
			destRect.bottom = dHeight;

			float scale = sourceAspect / destAspect;

			int scaledSize = static_cast<int>(0.5f * (1.0f - scale) * dWidth);

			destRect.left = scaledSize;
			destRect.right = dWidth - scaledSize;
		}


		int eye = mirrorSource == ERenderState::LEFT_EYE ? 0 : 1;

		IDirect3DSurface9* currentSurface = nullptr;
		Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &currentSurface);
		Helpers::GetDirect3DDevice9()->SetRenderTarget(0, Helpers::GetRenderTargets()[0].renderSurface);
		Helpers::GetDirect3DDevice9()->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
		Helpers::GetDirect3DDevice9()->SetRenderTarget(0, currentSurface);
		currentSurface->Release();
		Helpers::GetDirect3DDevice9()->StretchRect(vr->GetRenderSurface(eye), nullptr, Helpers::GetRenderTargets()[0].renderSurface, &destRect, D3DTEXF_LINEAR);
	}

	VR_PROFILE_STOP(Game_PostDrawFrame);

#if USE_PROFILER
	profiler.NewFrame();
#endif
}

bool Game::PreDrawHUD()
{
	VR_PROFILE_SCOPE(Game_PreDrawHUD);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// Remove zoom effect from game view
		if (GetRenderState() == ERenderState::GAME)
		{
			short* zoom = &Helpers::GetInputData().zoomLevel;
			realZoom = *zoom;
			*zoom = -1;
		}

		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME || GetRenderState() == ERenderState::SCOPE;
	}

	short* zoom = &Helpers::GetInputData().zoomLevel;
	realZoom = *zoom;
	*zoom = -1;

	Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &uiRealSurface);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;
	realUIWidth = Helpers::GetRenderTargets()[1].width;
	realUIHeight = Helpers::GetRenderTargets()[1].height;
	Helpers::GetRenderTargets()[1].width = c_UIOverlayWidth->Value();
	Helpers::GetRenderTargets()[1].height = c_UIOverlayHeight->Value();

	sRect* windowMain = Helpers::GetCurrentRect();
	realRect = *windowMain;

	windowMain->top = 0;
	windowMain->left = 0;
	windowMain->right = c_UIOverlayWidth->Value();
	windowMain->bottom = c_UIOverlayHeight->Value();

	return true;
}

void Game::PostDrawHUD()
{
	VR_PROFILE_SCOPE(Game_PostDrawHUD);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// Remove zoom effect from game view
		if (GetRenderState() == ERenderState::GAME)
		{
			short* zoom = &Helpers::GetInputData().zoomLevel;
			*zoom = realZoom;
		}

		return;
	}

	short* zoom = &Helpers::GetInputData().zoomLevel;
	*zoom = realZoom;

	sRect* windowMain = Helpers::GetCurrentRect();
	*windowMain = realRect;

	Helpers::GetRenderTargets()[1].width = realUIWidth;
	Helpers::GetRenderTargets()[1].height = realUIHeight;

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiRealSurface);

}

bool Game::PreDrawMenu()
{
	VR_PROFILE_SCOPE(Game_PreDrawMenu);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME;
	}

	Helpers::GetDirect3DDevice9()->GetRenderTarget(0, &uiRealSurface);
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiSurface);
	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;

	return true;
}

void Game::PostDrawMenu()
{
	VR_PROFILE_SCOPE(Game_PostDrawMenu);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		return;
	}

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, uiRealSurface);
}

D3DVIEWPORT9 currentViewport;
bool Game::PreDrawLoading(int param1, struct Renderer* renderer)
{
	VR_PROFILE_SCOPE(Game_PreDrawLoading);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		// ...but try to avoid breaking the game view (for now at least)
		return GetRenderState() == ERenderState::GAME;
	}

	uiRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	Helpers::GetRenderTargets()[1].renderSurface = uiSurface;

	return true;
}

void Game::PostDrawLoading(int param1, struct Renderer* renderer)
{
	VR_PROFILE_SCOPE(Game_PostDrawLoading);

	// Only render UI once per frame
	if (GetRenderState() != ERenderState::LEFT_EYE)
	{
		return;
	}

	Helpers::GetRenderTargets()[1].renderSurface = uiRealSurface;
}

bool Game::PreDrawCrosshair(short* anchorLocation)
{
	VR_PROFILE_SCOPE(Game_PreDrawCrosshair);

	// Draw things normally for the scope
	if (GetRenderState() == ERenderState::SCOPE)
	{
		return true;
	}

	crosshairRealSurface = Helpers::GetRenderTargets()[1].renderSurface;
	if (anchorLocation && *anchorLocation == 4) // Centre = 4
	{
		if (realZoom != -1)
		{
			return false;
		}
		Helpers::GetRenderTargets()[1].renderSurface = crosshairSurface;
		Helpers::GetDirect3DDevice9()->SetRenderTarget(0, crosshairSurface);
	}

	return true;
}

void Game::PostDrawCrosshair()
{
	VR_PROFILE_SCOPE(Game_PostDrawCrosshair);

	// Draw things normally for the scope
	if (GetRenderState() == ERenderState::SCOPE)
	{
		return;
	}

	Helpers::GetRenderTargets()[1].renderSurface = crosshairRealSurface;
	Helpers::GetDirect3DDevice9()->SetRenderTarget(0, crosshairRealSurface);
}

void Game::PreDrawImage(void* param1, void* param2)
{
	VR_PROFILE_SCOPE(Game_PreDrawImage);

	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_ALPHAFUNC, &realAlphaFunc);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_SRCBLENDALPHA, &realAlphaSrc);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_DESTBLENDALPHA, &realAlphaDest);

	//Logger::log << realAlphaFunc << ", " << realAlphaSrc << ", " << realAlphaDest << std::endl;

	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHAFUNC, D3DBLENDOP_ADD);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_SRCALPHA);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
}

void Game::PostDrawImage(void* param1, void* param2)
{
	VR_PROFILE_SCOPE(Game_PostDrawImage);

	DWORD tmp1, tmp2, tmp3;

	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_ALPHAFUNC, &tmp1);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_SRCBLENDALPHA, &tmp2);
	Helpers::GetDirect3DDevice9()->GetRenderState(D3DRS_DESTBLENDALPHA, &tmp3);

	//Logger::log << tmp1 << ", " << tmp2 << ", " << tmp3 << std::endl;

	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_ALPHAFUNC, realAlphaFunc);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_SRCBLENDALPHA, realAlphaSrc);
	Helpers::GetDirect3DDevice9()->SetRenderState(D3DRS_DESTBLENDALPHA, realAlphaDest);
}

void Game::UpdateViewModel(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* BoneTransforms, Transform* OutBoneTransforms)
{
	VR_PROFILE_SCOPE(Game_UpdateViewModel);
	weaponHandler.UpdateViewModel(id, pos, facing, up, BoneTransforms, OutBoneTransforms);
}

void Game::PreFireWeapon(HaloID& weaponID, short param2)
{
	VR_PROFILE_SCOPE(Game_PreFireWeapon);
	weaponHandler.PreFireWeapon(weaponID, param2);
}

void Game::PostFireWeapon(HaloID& weaponID, short param2)
{
	VR_PROFILE_SCOPE(Game_PostFireWeapon);
	weaponHandler.PostFireWeapon(weaponID, param2);
}

void Game::PreThrowGrenade(HaloID& playerID)
{
	VR_PROFILE_SCOPE(Game_PreThrowGrenade);
	weaponHandler.PreThrowGrenade(playerID);
}

void Game::PostThrowGrenade(HaloID& playerID)
{
	VR_PROFILE_SCOPE(Game_PostThrowGrenade);
	weaponHandler.PostThrowGrenade(playerID);
}

void Game::UpdateInputs()
{
	VR_PROFILE_SCOPE(Game_UpdateInputs);
	inputHandler.UpdateInputs(bInVehicle);

#if USE_PROFILER
	static bool bWasPressed = false;

	bool bPressed = GetAsyncKeyState(VK_F5) & 0x8000;

	if (bPressed && !bWasPressed)
	{
		DumpProfilerData();
	}

	bWasPressed = bPressed;
#endif
}

void Game::CalculateSmoothedInput()
{
	VR_PROFILE_SCOPE(Game_CalculateSmoothedInput);
	inputHandler.CalculateSmoothedInput();
}

bool Game::GetCalculatedHandPositions(Matrix4& controllerTransform, Vector3& dominantHandPos, Vector3& offHand)
{
	VR_PROFILE_SCOPE(Game_GetCalculatedHandPositions);
	return inputHandler.GetCalculatedHandPositions(controllerTransform, dominantHandPos, offHand);
}

void Game::ReloadStart(HaloID param1, short param2, bool param3)
{
	VR_PROFILE_SCOPE(Game_ReloadStart);
	// TODO: Check if this reload was from the player (can probably be done by checking the weapon's parent ID matches the player)
	WeaponDynamicObject* weaponObject = static_cast<WeaponDynamicObject*>((Helpers::GetDynamicObject(param1)));

	Weapon& weapon = weaponObject->weaponData[param2];

	// Reload function gets called whenever the player tries to reload, if reloadstate is 1 then a reload was actually triggered
	if (weapon.reloadState == 1)
	{
		Logger::log << "Reload Start (" << param1 << ", " << param2 << ", " << param3 << ")" << std::endl;
	}
}

void Game::ReloadEnd(short param1, HaloID param2)
{
	VR_PROFILE_SCOPE(Game_ReloadEnd);
	Logger::log << "Reload End" << std::endl;
}

Vector3 Game::GetSmoothedInput() const
{
	return inputHandler.smoothedPosition;
}

void Game::UpdateCamera(float& yaw, float& pitch)
{
	VR_PROFILE_SCOPE(Game_UpdateCamera);
	// Don't bother simulating inputs if we aren't actually in vr
#if EMULATE_VR
	return;
#endif

	if (bInVehicle && !bHasWeapon)
	{
		inputHandler.UpdateCameraForVehicles(yaw, pitch);
	}
	else
	{
		inputHandler.UpdateCamera(yaw, pitch);
	}
}

void Game::SetMousePosition(int& x, int& y)
{
	VR_PROFILE_SCOPE(Game_SetMousePosition);
	// Don't bother simulating inputs if we aren't actually in vr
#if EMULATE_VR
	return;
#endif
	inputHandler.SetMousePosition(x, y);
}

void Game::UpdateMouseInfo(MouseInfo* mouseInfo)
{
	VR_PROFILE_SCOPE(Game_UpdateMouseInfo);
	// Don't bother simulating inputs if we aren't actually in vr
#if EMULATE_VR
	return;
#endif
	inputHandler.UpdateMouseInfo(mouseInfo);
}

void Game::SetViewportScale(Viewport* viewport)
{
	VR_PROFILE_SCOPE(Game_SetViewportScale);
	// This appears to be broken, the code this is overriding does something very weird with the scaling
	/*
	viewport->left = -1.0f;
	viewport->right = 1.0f;
	viewport->bottom = 1.0f;
	viewport->top = -1.0f;
	*/
	viewport->left = c_TEMPViewportLeft->Value();
	viewport->right = c_TEMPViewportRight->Value();
	viewport->top = c_TEMPViewportTop->Value();
	viewport->bottom = c_TEMPViewportBottom->Value();
}

float Game::MetresToWorld(float m) const
{
	return m / 3.048f;
}

float Game::WorldToMetres(float w) const
{
	return w * 3.048f;
}

void Game::CreateConsole()
{
	if (!c_ShowConsole || !c_ShowConsole->Value())
	{
		return;
	}

	AllocConsole();
	freopen_s(&consoleOut, "CONOUT$", "w", stdout);
	std::cout.clear();
}

void Game::PatchGame()
{
	VR_PROFILE_SCOPE(Game_PatchGame);
	MH_STATUS hookStatus;

	if ((hookStatus = MH_Initialize()) != MH_OK)
	{
		Logger::err << "Could not initialise MinHook: " << MH_StatusToString(hookStatus) << std::endl;
	}
	else
	{
		Logger::log << "[Game] Creating hooks" << std::endl;
		Hooks::InitHooks();
		Logger::log << "[Game] Enabling hooks" << std::endl;
		Hooks::EnableAllHooks();
	}
}

void Game::SetupConfigs()
{
	VR_PROFILE_SCOPE(Game_SetupConfigs);

	Config config;

	// Window settings
	c_ShowConsole = config.RegisterBool("ShowConsole", "Create a console window at launch for debugging purposes", false);
	c_DrawMirror = config.RegisterBool("DrawMirror", "Update the desktop window display to show the current game view, rather than leaving it on the splash screen", true);
	c_MirrorEye = config.RegisterInt("MirrorEye", "Index of the eye to use for the mirror view  (0 = left, 1 = right)", 0);
	// UI settings
	c_CrosshairDistance = config.RegisterFloat("CrosshairDistance", "Distance in metres in front of the weapon to display the crosshair", 15.0f);
	c_MenuOverlayDistance = config.RegisterFloat("MenuOverlayDistance", "Distance in metres in front of the HMD to display the menu", 15.0f);
	c_UIOverlayDistance = config.RegisterFloat("UIOverlayDistance", "Distance in metres in front of the HMD to display the UI", 15.0f);
	c_UIOverlayScale = config.RegisterFloat("UIOverlayScale", "Width of the UI overlay in metres", 10.0f);
	c_MenuOverlayScale = config.RegisterFloat("MenuOverlayScale", "Width of the menu overlay in metres", 10.0f);
	c_CrosshairScale = config.RegisterFloat("CrosshairScale", "Width of the crosshair overlay in metres", 10.0f);
	c_UIOverlayCurvature = config.RegisterFloat("UIOverlayCurvature", "Curvature of the UI Overlay, on a scale of 0 to 1", 0.1f);
	c_UIOverlayWidth = config.RegisterInt("UIOverlayWidth", "Width of the UI overlay in pixels", 800);
	c_UIOverlayHeight = config.RegisterInt("UIOverlayHeight", "Height of the UI overlay in pixels", 600);
	c_ShowCrosshair = config.RegisterBool("ShowCrosshair", "Display a floating crosshair in the world at the location you are aiming", true);
	// Control settings
	c_LeftHanded = config.RegisterBool("LeftHanded", "Make the left hand the dominant hand by default. This swaps the button bindings but doesn't swap the sticks. Left handed bindings with the sticks swapped can be found in the SteamVR overlay", false);
	c_SnapTurn = config.RegisterBool("SnapTurn", "The look input will instantly rotate the view by a fixed amount, rather than smoothly rotating", true);
	c_SnapTurnAmount = config.RegisterFloat("SnapTurnAmount", "Rotation in degrees a single snap turn will rotate the view by", 45.0f);
	c_SmoothTurnAmount = config.RegisterFloat("SmoothTurnAmount", "Rotation in degrees per second the view will turn at when not using snap turning", 90.0f);
	c_HandRelativeMovement = config.RegisterInt("HandRelativeMovement", "Movement is relative to hand orientation, rather than head, 0 = off, 1 = left, 2 = right", 0);
	c_HandRelativeOffsetRotation = config.RegisterFloat("HandRelativeOffsetRotation", "Hand direction rotational offset in degrees used for hand-relative movement", -20.0f);
	c_HorizontalVehicleTurnAmount = config.RegisterFloat("HorizontalVehicleTurnAmount", "Rotation in degrees per second the view will turn horizontally when in vehicles (<0 to invert)", 90.0f);
	c_VerticalVehicleTurnAmount = config.RegisterFloat("VerticalVehicleTurnAmount", "Rotation in degrees per second the view will turn vertically when in vehicles (<0 to invert)", 45.0f);
	c_ToggleGrip = config.RegisterBool("ToggleGrip", "When true releasing two handed weapons requires pressing the grip action again", false);
	c_TwoHandDistance = config.RegisterFloat("TwoHandDistance", "Maximum distance between both hands where the off hand grip action will enable two handed aiming (<0 for any distance)", 0.8f);
	c_SwapHandDistance = config.RegisterFloat("SwapHandDistance", "Maximum distance between both hands where the swap weapon hand grip action will swap your weapon into the opposite hand (<0 to disable)", 0.2f);
	c_OffhandHandFlashlight = config.RegisterBool("OffhandHandFlashlight", "Use your offhand for toggling the flashlight, your offhand hand is the hand not holding a weapon", true);
	c_LeftHandFlashlightDistance = config.RegisterFloat("LeftHandFlashlight", "Bringing the left hand within this distance of the head will toggle the flashlight (<0 to disable)", 0.2f);
	c_RightHandFlashlightDistance = config.RegisterFloat("RightHandFlashlight", "Bringing the right hand within this distance of the head will toggle the flashlight (<0 to disable)", 0.2f);
	c_LeftHandMeleeSwingSpeed = config.RegisterFloat("LeftHandMeleeSwingSpeed", "Minimum vertical velocity of left hand required to initiate a melee attack in m/s (<0 to disable)", 2.5f);
	c_RightHandMeleeSwingSpeed = config.RegisterFloat("RightHandMeleeSwingSpeed", "Minimum vertical velocity of right hand required to initiate a melee attack in m/s (<0 to disable)", 2.5f);
	c_CrouchHeight = config.RegisterFloat("CrouchHeight", "Minimum height to duck by in metres to automatically trigger the crouch input in game (<0 to disable)", 0.15f);
	// Hand settings
	c_ControllerOffset = config.RegisterVector3("ControllerOffset", "Offset from the controller's position used when calculating the in game hand position", Vector3(0.0f, 0.0f, 0.0f));
	c_ControllerRotation = config.RegisterVector3("ControllerRotation", "Rotation added to the controller when calculating the in game hand rotation", Vector3(0.0f, 0.0f, 0.0f));
	c_ScopeRenderScale = config.RegisterFloat("ScopeRenderScale", "Size of the scope render target, expressed as a proportion of the headset's render scale (e.g. 0.5 = half resolution)", 1.0f);
	c_ScopeScale = config.RegisterFloat("ScopeScale", "Width of the scope view in metres", 0.05f);
	c_ScopeOffsetPistol = config.RegisterVector3("ScopeOffsetPistol", "Offset of the scope view relative to the pistol's location", Vector3(-0.1f, 0.0f, 0.15f));
	c_ScopeOffsetSniper = config.RegisterVector3("ScopeOffsetSniper", "Offset of the scope view relative to the pistol's location", Vector3(-0.15f, 0.0f, 0.15f));
	c_ScopeOffsetRocket = config.RegisterVector3("ScopeOffsetRocket", "Offset of the scope view relative to the pistol's location", Vector3(0.1f, 0.2f, 0.1f));
	c_WeaponSmoothingAmountNoZoom = config.RegisterFloat("UnzoomedWeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when not zoomed in (0 is disabled, 1 is maximum, recommended around 0-0.2)", 0.0f);
	c_WeaponSmoothingAmountOneZoom = config.RegisterFloat("Zoom1WeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when zoomed in once, eg zooming on the pistol (0 is disabled, 1 is maximum, recommended around 0.3-0.5)", 0.4f);
	c_WeaponSmoothingAmountTwoZoom = config.RegisterFloat("Zoom2WeaponSmoothingAmount", "Amount of smoothing applied to weapon movement when zoomed in twice, eg second zoom on sniper (0 is disabled, 1 is maximum, recommended around 0.6-1)", 0.6f);
	// Weapon holster settings
	c_EnableWeaponHolsters = config.RegisterBool("EnableWeaponHolsters", "When enabled Weapons can only be switched by using the 'SwitchWeapons' binding while the dominant hand is within distance of a holster", true);
	c_LeftShoulderHolsterActivationDistance = config.RegisterFloat("LeftShoulderHolsterDistance", "The 'size' of the left shoulder holster. This is the distance that the dominant hand needs to be from the holster to change weapons (<0 to disable)", 0.3f);
	c_LeftShoulderHolsterOffset = config.RegisterVector3("LeftShoulderHolsterOffset", "The (foward, left, up) Offset of the left shoulder holster relative to the headset's location", Vector3(-0.15f, 0.25f, -0.25f));
	c_RightShoulderHolsterActivationDistance = config.RegisterFloat("RightShoulderHolsterDistance", "The 'size' of the right shoulder holster. This is the distance that the dominant hand needs to be from the holster to change weapons (<0 to disable)", 0.3f);
	c_RightShoulderHolsterOffset = config.RegisterVector3("RightShoulderHolsterOffset", "The (foward, left, up) Offset of the right shoulder holster relative to the headset's location", Vector3(-0.15f, -0.25f, -0.25f));
	// Misc settings
	c_ShowRoomCentre = config.RegisterBool("ShowRoomCentre", "Draw an indicator at your feet to show where the player character is actually positioned", true);
	c_d3d9Path = config.RegisterString("CustomD3D9Path", "If set first try to load d3d9.dll from the specified path instead of from system32", "");
	c_TEMPViewportLeft = config.RegisterFloat("TEMP_ViewportLeft", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", -1.0f);
	c_TEMPViewportRight = config.RegisterFloat("TEMP_ViewportRight", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", 1.0f);
	c_TEMPViewportTop = config.RegisterFloat("TEMP_ViewportTop", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", -1.0f);
	c_TEMPViewportBottom = config.RegisterFloat("TEMP_ViewportBottom", "Some headsets experience warping when turning, as a workaround the viewport scaling has been exposed so users can adjust them until the warping stops", 1.0f);

	const bool bLoadedConfig = config.LoadFromFile("VR/config.txt");
	const bool bSavedConfig = config.SaveToFile("VR/config.txt");

	if (!bLoadedConfig)
	{
		Logger::log << "[Config] First time startup, generating default config file" << std::endl;
	}

	if (!bSavedConfig)
	{
		Logger::log << "[Config] Couldn't save config file, halo is likely running as non-administrator from a protected directory" << std::endl;
	}

	// First run, but couldn't create config file
	if (!bLoadedConfig && !bSavedConfig)
	{
		Logger::err << "Could not create /VR/config.txt.\nHalo may have been installed in Program Files, to generate the config file either run halo.exe as an administrator or reinstall the game in a non-protected folder (e.g. Documents)." << std::endl;
	}

	weaponHandler.localOffset = Vector3(c_ControllerOffset->Value().x, c_ControllerOffset->Value().y, c_ControllerOffset->Value().z);
	weaponHandler.localRotation = Vector3(c_ControllerRotation->Value().x, c_ControllerRotation->Value().y, c_ControllerRotation->Value().z);

	if (c_MirrorEye->Value() == 0)
	{
		mirrorSource = ERenderState::LEFT_EYE;
	}
	else if (c_MirrorEye->Value() == 1)
	{
		mirrorSource = ERenderState::RIGHT_EYE;
	}
	else if (c_MirrorEye->Value() == 2)
	{
		Logger::log << "[Config] MirrorEye set to 'game'. This is intended for debugging, may not look correct, and will likely impact performance" << std::endl;
		mirrorSource = ERenderState::GAME;
	}
	else
	{
		Logger::log << "[Config] Invalid value for MirrorEye, defaulting to left eye" << std::endl;
		mirrorSource = ERenderState::LEFT_EYE;
	}

	//Logger::log << "[Config] Loaded configs" << std::endl;
}

void Game::CalcFPS(float deltaTime)
{
	fpsTracker.framesSinceFPSUpdate++;
	fpsTracker.timeSinceFPSUpdate += deltaTime;

	if (fpsTracker.timeSinceFPSUpdate > 1.0f)
	{
		fpsTracker.timeSinceFPSUpdate = 0.0f;
		fpsTracker.fps = fpsTracker.framesSinceFPSUpdate;
		fpsTracker.framesSinceFPSUpdate = 0;
		Logger::log << fpsTracker.fps << std::endl;
	}
}

#if USE_PROFILER
void Game::DumpProfilerData()
{
	VR_PROFILE_SCOPE(Game_DumpProfilerData);

	// TODO: Create a better system for viewing this data (e.g. real time display/proper dedicated profile files)
	std::vector<Profiler::FrameTimings*> frameTimings;
	Game::instance.profiler.GetTimings(frameTimings);

	Logger::log << "[Profiler] Dumping last 30 seconds of profiler data..." << std::endl;

	float minFrame = FLT_MAX;
	float maxFrame = -FLT_MAX;
	float totalFrame = 0.0f;
	int numFrames = 0;

	Profiler::time_point now = std::chrono::high_resolution_clock::now();

	std::unordered_map<std::string, Profiler::Timings> totalTimes;

	for (auto it = frameTimings.rbegin(); it != frameTimings.rend(); ++it)
	{
		float ago = std::chrono::duration<float, std::milli>(now - (*it)->frameEnd).count();
		if (ago > 30.0f * 1000.0f)
		{
			break;
		}

		float duration = std::chrono::duration<float, std::milli>((*it)->frameEnd - (*it)->frameStart).count();

		minFrame = (std::min)(minFrame, duration);
		maxFrame = (std::max)(maxFrame, duration);
		totalFrame += duration;
		numFrames++;

		for (auto& kv : (*it)->timings)
		{
			const std::string& eventName = kv.first;
			const Profiler::Timings* timings = kv.second;
			/*
			Logger::log << "[Profiler] "
				<< eventName
				<< ": calls " << timings->numHits
				<< " min. " << timings->minTime
				<< " avg. " << timings->totalTime / timings->numHits
				<< " max. " << timings->maxTime
				<< std::endl;
			*/

			totalTimes[eventName].minTime = (std::min)(totalTimes[eventName].minTime, timings->minTime);
			totalTimes[eventName].maxTime = (std::max)(totalTimes[eventName].maxTime, timings->maxTime);
			totalTimes[eventName].numHits += timings->numHits;
			totalTimes[eventName].totalTime += timings->totalTime;
		}
	}

	Logger::log << "[Profiler] Frame times: min. " << minFrame << " avg. " << (totalFrame / numFrames) << " max. " << maxFrame << std::endl;
	Logger::log << "[Profiler] Total Event times: " << std::endl;

	std::vector<std::pair<std::string, Profiler::Timings>> topTimings;
	for (auto& kv : totalTimes)
	{
		topTimings.push_back(kv);
	}

	std::sort(topTimings.begin(), topTimings.end(), [](const auto& a, const auto& b)
		{
			return a.second.totalTime > b.second.totalTime;
		}
	);

	for (auto& kv : topTimings)
	{
		Logger::log << "[Profiler] "
			<< kv.first
			<< ": calls " << kv.second.numHits
			<< " min. " << kv.second.minTime
			<< " avg. " << kv.second.totalTime / kv.second.numHits
			<< " max. " << kv.second.maxTime
			<< std::endl;
	}
}
#endif

void Game::UpdateCrosshairAndScope()
{
	VR_PROFILE_SCOPE(Game_UpdateCrosshairAndScope);

	auto fixupRotation = [](Matrix4& m, Vector3& pos) {
		m.translate(-pos);
		m.rotate(90.0f, m.getUpAxis());
		m.rotate(-90.0f, m.getLeftAxis());
		m.translate(pos);
		};

	Vector3 aimPos, aimDir;

	if (bInVehicle && !bHasWeapon)
	{
		aimPos = Vector3();
		aimDir = Helpers::GetCamera().lookDir;
	}
	else
	{
		bool bHasCrosshair = weaponHandler.GetLocalWeaponAim(aimPos, aimDir);

		if (!bHasCrosshair)
		{
			return;
		}
	}

	Matrix4 overlayTransform;

	Vector3 targetPos = aimPos + aimDir * c_CrosshairDistance->Value();

	Vector3 hmdPos = vr->GetHMDTransform(true) * Vector3(0.0f, 0.0f, 0.0f);

	overlayTransform.translate(targetPos);
	overlayTransform.lookAt(hmdPos, Vector3(0.0f, 0.0f, 1.0f));

	fixupRotation(overlayTransform, targetPos);

	if (c_ShowCrosshair->Value())
	{
		vr->SetCrosshairTransform(overlayTransform);
	}
	overlayTransform.identity();

	short zoom = Helpers::GetInputData().zoomLevel;

	Vector3 upDir = Vector3(0.0f, 0.0f, 1.0f);

	bool bHasScope = (zoom != -1) && weaponHandler.GetLocalWeaponScope(aimPos, aimDir, upDir);

	if (!bHasScope)
	{
		SetScopeTransform(overlayTransform, false);
		return;
	}

	overlayTransform.translate(aimPos);
	overlayTransform.lookAt(aimPos - aimDir, upDir);

	fixupRotation(overlayTransform, aimPos);

	SetScopeTransform(overlayTransform, true);
}

void Game::SetScopeTransform(Matrix4& newTransform, bool bIsVisible)
{
	VR_PROFILE_SCOPE(Game_SetScopeTransform);

	if (!bIsVisible)
	{
		return;
	}

	Vector3 scopeUp = newTransform.getForwardAxis();
	Vector3 scopeFacing = -newTransform.getLeftAxis();

	Vector3 pos = (newTransform * Vector3(0.0f, 0.0f, 0.0f)) * MetresToWorld(1.0f) + Helpers::GetCamera().position;
	Matrix3 rot;
	Vector2 size(1.0f, 0.75f);
	size *= MetresToWorld(GetScopeSize());

	newTransform.translate(-pos);
	newTransform.rotate(90.0f, newTransform.getLeftAxis());
	newTransform.rotate(-90.0f, newTransform.getUpAxis());
	newTransform.rotate(-90.0f, newTransform.getLeftAxis());

	for (int i = 0; i < 3; i++)
	{
		rot.setColumn(i, &newTransform.get()[i * 4]);
	}

	inGameRenderer.DrawPolygon(pos, scopeFacing, scopeUp, 32, MetresToWorld(GetScopeSize() * 0.5f), D3DCOLOR_ARGB(0, 0, 0, 0), false);

	float SCOPE_DEPTH = 2.0f;
	float SCOPE_INNER_SCALE = 80.0f;

	pos = pos - scopeFacing * MetresToWorld(SCOPE_DEPTH);
	size *= SCOPE_INNER_SCALE;

	inGameRenderer.DrawRenderTarget(vr->GetScopeTexture(), pos, rot, size, false, true);
}

void Game::StoreRenderTargets()
{
	VR_PROFILE_SCOPE(Game_StoreRenderTargets);

	for (int i = 0; i < 8; i++)
	{
		gameRenderTargets[i].width = Helpers::GetRenderTargets()[i].width;
		gameRenderTargets[i].height = Helpers::GetRenderTargets()[i].height;
		gameRenderTargets[i].format = Helpers::GetRenderTargets()[i].format;
		gameRenderTargets[i].renderSurface = Helpers::GetRenderTargets()[i].renderSurface;
		gameRenderTargets[i].renderTexture = Helpers::GetRenderTargets()[i].renderTexture;
	}
}

void Game::RestoreRenderTargets()
{
	VR_PROFILE_SCOPE(Game_RestoreRenderTargets);

	for (int i = 0; i < 8; i++)
	{
		Helpers::GetRenderTargets()[i].width = gameRenderTargets[i].width;
		Helpers::GetRenderTargets()[i].height = gameRenderTargets[i].height;
		Helpers::GetRenderTargets()[i].format = gameRenderTargets[i].format;
		Helpers::GetRenderTargets()[i].renderSurface = gameRenderTargets[i].renderSurface;
		Helpers::GetRenderTargets()[i].renderTexture = gameRenderTargets[i].renderTexture;
	}
}

void Game::CreateTextureAndSurface(UINT Width, UINT Height, DWORD Usage, D3DFORMAT Format, IDirect3DSurface9** OutSurface, IDirect3DTexture9** OutTexture)
{
	VR_PROFILE_SCOPE(Game_CreateTextureAndSurface);

	HRESULT result = Helpers::GetDirect3DDevice9()->CreateTexture(Width, Height, 1, Usage, Format, D3DPOOL_DEFAULT, OutTexture, nullptr);
	if (FAILED(result))
	{
		Logger::err << "[DX9] Failed to create game texture: " << result << std::endl;
		return;
	}

	result = (*OutTexture)->GetSurfaceLevel(0, OutSurface);
	if (FAILED(result))
	{
		Logger::err << "[DX9] Failed to retrieve game surface: " << result << std::endl;
		return;
	}
}
