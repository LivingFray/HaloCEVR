#include <d3d11.h>
#include <d3d9.h>
#include <algorithm>
#include <filesystem>
#include "OpenVR.h"
#include "../Logger.h"
#include "../Game.h"
#include "../Profiler.h"
#include "../Helpers/DX9.h"
#include "../Helpers/Renderer.h"
#include "../Helpers/RenderTarget.h"
#include "../Helpers/Camera.h"
#include "../Helpers/Cutscene.h"
#include "../Helpers/Menus.h"
#include "../WeaponHapticsConfig.h"

#pragma comment(lib, "openvr_api.lib")
#pragma comment(lib, "d3d11.lib")

void OpenVR::Init()
{
	VR_PROFILE_SCOPE(OpenVR_Init);

	Logger::log << "[OpenVR] Initialising OpenVR" << std::endl;

	vr::EVRInitError initError = vr::VRInitError_None;
	vrSystem = vr::VR_Init(&initError, vr::VRApplication_Scene);

	if (initError != vr::VRInitError_None)
	{
		Logger::err << "[OpenVR] VR_Init failed: " << vr::VR_GetVRInitErrorAsEnglishDescription(initError) << std::endl;
		return;
	}

	vrCompositor = vr::VRCompositor();

	if (!vrCompositor)
	{
		Logger::err << "[OpenVR] VRCompositor failed" << std::endl;
		return;
	}

	vrOverlay = vr::VROverlay();

	if (!vrOverlay)
	{
		Logger::err << "[OpenVR] VROverlay failed" << std::endl;
		return;
	}

	vrInput = vr::VRInput();

	if (!vrInput)
	{
		Logger::err << "[OpenVR] VRInput failed" << std::endl;
		return;
	}

	keyboardBuffer = new char[256];

	vrOverlay->CreateOverlay("UIOverlay", "UIOverlay", &uiOverlay);
	vrOverlay->SetOverlayFlag(uiOverlay, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true);
	vrOverlay->SetOverlayFlag(uiOverlay, vr::VROverlayFlags_IsPremultiplied, true);
	vrOverlay->ShowOverlay(uiOverlay);

	std::filesystem::path manifest = std::filesystem::current_path() / "VR" / "OpenVR" / "haloce.vrmanifest";
	vr::EVRApplicationError appErr = vr::VRApplications()->AddApplicationManifest(manifest.string().c_str());

	if (appErr != vr::VRApplicationError_None)
	{
		Logger::log << "[OpenVR] Could not add application manifest: " << appErr << std::endl;
	}

	appErr = vr::VRApplications()->IdentifyApplication(GetCurrentProcessId(), "livingfray.haloce");

	if (appErr != vr::VRApplicationError_None)
	{
		Logger::log << "[OpenVR] Could not set id: " << appErr << std::endl;
	}


	std::filesystem::path actions = std::filesystem::current_path() / "VR" / "OpenVR" / "actions.json";
	const auto manifestError=vrInput->SetActionManifestPath(actions.string().c_str());
	Logger::log << "[OpenVR] Action manifest: " << actions.string() << " result=" << manifestError << std::endl;

	vr::EVRInputError ActionSetError = vrInput->GetActionSetHandle("/actions/default", &actionSets[0].ulActionSet);

	if (ActionSetError != vr::EVRInputError::VRInputError_None)
	{
		Logger::err << "[OpenVR] Could not get action set: " << ActionSetError << std::endl;
	}

	vr::EVRInputError skeletonError = vrInput->GetActionHandle("/actions/default/in/LeftHand", &leftHandSkeleton);

	if (skeletonError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get left skeleton binding: " << skeletonError << std::endl;
	}

	skeletonError = vrInput->GetActionHandle("/actions/default/in/RightHand", &rightHandSkeleton);

	if (skeletonError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get right skeleton binding: " << skeletonError << std::endl;
	}

	bHasValidTipPoses = true;
	vr::EVRInputError poseError = vrInput->GetActionHandle("/actions/default/in/LeftTip", &leftHandTip);

	if (poseError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get left hand pose: " << poseError << std::endl;
		bHasValidTipPoses = false;
	}

	poseError = vrInput->GetActionHandle("/actions/default/in/RightTip", &rightHandTip);

	if (poseError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get right hand pose: " << poseError << std::endl;
		bHasValidTipPoses = false;
	}

	vr::EVRInputError leftFireError = vrInput->GetActionHandle("/actions/default/out/LeftFire", &leftFire);
	vr::EVRInputError rightFireError = vrInput->GetActionHandle("/actions/default/out/RightFire", &rightFire);

	if (leftFireError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get leftFire (vibration) action: " << leftFireError << std::endl;
	}

	if (rightFireError != vr::EVRInputError::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get leftFire (vibration) action: " << rightFireError << std::endl;
	}

	UpdateInputs();
	UpdateSkeleton(ControllerRole::Left);
	UpdateSkeleton(ControllerRole::Right);
	UpdatePose(ControllerRole::Left);
	UpdatePose(ControllerRole::Right);

	vrSystem->GetRecommendedRenderTargetSize(&recommendedWidth, &recommendedHeight);

	// Voodoo magic to convert normal view frustums into asymmetric ones through selective cropping

	float l_left = 0.0f, l_right = 0.0f, l_top = 0.0f, l_bottom = 0.0f;
	vrSystem->GetProjectionRaw(vr::EVREye::Eye_Left, &l_left, &l_right, &l_top, &l_bottom);
	Logger::log << "[OpenVR] Left eye raw projection[l, r, t, b] = [" << l_left << ", " << l_right << ", " << l_top << ", " << l_bottom << "]" << std::endl;

	float r_left = 0.0f, r_right = 0.0f, r_top = 0.0f, r_bottom = 0.0f;
	vrSystem->GetProjectionRaw(vr::EVREye::Eye_Right, &r_left, &r_right, &r_top, &r_bottom);
	Logger::log << "[OpenVR] Right eye raw projection[l, r, t, b] = [" << r_left << ", " << r_right << ", " << r_top << ", " << r_bottom << "]" << std::endl;

	float tanHalfFov[2];

	tanHalfFov[0] = (std::max)({ -l_left, l_right, -r_left, r_right });
	tanHalfFov[1] = (std::max)({ -l_top, l_bottom, -r_top, r_bottom });
	Logger::log << "[OpenVR] tanHalfFov[horiz, vert] = [" << tanHalfFov[0] << ", " << tanHalfFov[1] << "]" << std::endl;

	textureBounds[0].uMin = 0.5f + 0.5f * l_left / tanHalfFov[0];
	textureBounds[0].uMax = 0.5f + 0.5f * l_right / tanHalfFov[0];
	textureBounds[0].vMin = 0.5f - 0.5f * l_bottom / tanHalfFov[1];
	textureBounds[0].vMax = 0.5f - 0.5f * l_top / tanHalfFov[1];
	Logger::log << "[OpenVR] Left eye textureBounds[uMin, uMax, vMin, vMax] = [" << textureBounds[0].uMin << ", " << textureBounds[0].uMax << ", " << textureBounds[0].vMin << ", " << textureBounds[0].vMax << "]" << std::endl;

	textureBounds[1].uMin = 0.5f + 0.5f * r_left / tanHalfFov[0];
	textureBounds[1].uMax = 0.5f + 0.5f * r_right / tanHalfFov[0];
	textureBounds[1].vMin = 0.5f - 0.5f * r_bottom / tanHalfFov[1];
	textureBounds[1].vMax = 0.5f - 0.5f * r_top / tanHalfFov[1];
	Logger::log << "[OpenVR] Right eye textureBounds[uMin, uMax, vMin, vMax] = [" << textureBounds[1].uMin << ", " << textureBounds[1].uMax << ", " << textureBounds[1].vMin << ", " << textureBounds[1].vMax << "]" << std::endl;

	aspect = tanHalfFov[0] / tanHalfFov[1];
	fov = 2.0f * atan(tanHalfFov[1]);

	realWidth = recommendedWidth;
	realHeight = recommendedHeight;

	recommendedWidth = static_cast<uint32_t>(recommendedWidth / (std::max)(textureBounds[0].uMax - textureBounds[0].uMin, textureBounds[1].uMax - textureBounds[1].uMin));
	recommendedHeight = static_cast<uint32_t>(recommendedHeight / (std::max)(textureBounds[0].vMax - textureBounds[0].vMin, textureBounds[1].vMax - textureBounds[1].vMin));

	Logger::log << "[OpenVR] Stretched Width/Height from " << realWidth << "x" << realHeight << " to " << recommendedWidth << "x" << recommendedHeight << std::endl;
	Logger::log << "[OpenVR] Desired fov = " << (fov * (180.0f / 3.141593f)) << " Desired aspect ratio = " << aspect << std::endl;

	Logger::log << "[OpenVR] VR systems created successfully" << std::endl;
}

void OpenVR::OnGameFinishInit()
{
	VR_PROFILE_SCOPE(OpenVR_OnGameFinishInit);

	HRESULT result = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &d3dDevice, NULL, NULL);

	if (FAILED(result))
	{
		Logger::log << "[OpenVR] Could not initialise DirectX11: " << result << std::endl;
	}
	
	D3DSURFACE_DESC desc, desc2;
	Helpers::GetRenderTargets()[0].renderSurface->GetDesc(&desc);
	Helpers::GetRenderTargets()[1].renderSurface->GetDesc(&desc2);

	// Create shared textures
	// Eyes
	CreateTexAndSurface(0, recommendedWidth, recommendedHeight, desc.Usage, desc.Format);
	CreateTexAndSurface(1, recommendedWidth, recommendedHeight, desc.Usage, desc.Format);
	// UI Layers
	CreateTexAndSurface(uiSurface, Game::instance.overlayWidth, Game::instance.overlayHeight, desc2.Usage, desc2.Format);
	CreateTexAndSurface(crosshairSurface, Game::instance.overlayWidth, Game::instance.overlayHeight, desc2.Usage, desc2.Format);
	
	scopeWidth = static_cast<uint32_t>(Game::instance.c_ScopeRenderScale->Value() * recommendedWidth);
	scopeHeight = static_cast<uint32_t>(Game::instance.c_ScopeRenderScale->Value() * recommendedWidth * 0.75f); // Maintain the 4x3 aspect ratio halo works best with
	CreateTexAndSurface(scopeSurface, scopeWidth, scopeHeight, desc2.Usage, desc2.Format);

	vr::EVROverlayError err;

	err = vrOverlay->SetOverlayWidthInMeters(uiOverlay, Game::instance.c_UIOverlayScale->Value());
	if (err != vr::VROverlayError_None)
	{
		Logger::log << "[OpenVR] Error setting overlay width: " << err << std::endl;
	}
	Logger::log << "[OpenVR] Set UI Width = " << Game::instance.c_UIOverlayScale->Value() << std::endl;

	float curvature = Game::instance.c_UIOverlayCurvature->Value();
	if (curvature != 0.0f)
	{
		vrOverlay->SetOverlayCurvature(uiOverlay, curvature);
		Logger::log << "[OpenVR] Set UI Curvature = " << curvature << std::endl;
	}

	vr::HmdMatrix34_t overlayTransform = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 2.0f,
		0.0f, 0.0f, 1.0f, -3.0f
	};

	vrOverlay->SetOverlayTransformAbsolute(uiOverlay, vr::ETrackingUniverseOrigin::TrackingUniverseStanding, &overlayTransform);

	Logger::log << "[OpenVR] Finished Initialisation" << std::endl;
}

void OpenVR::Shutdown()
{
	if (vrSystem)
	{
		vr::VR_Shutdown();
	}
}


void OpenVR::CreateTexAndSurface(int index, UINT Width, UINT Height, DWORD Usage, D3DFORMAT Format)
{
	VR_PROFILE_SCOPE(OpenVR_CreateTexAndSurface);

	HANDLE sharedHandle = nullptr;

	// Create texture on game
	HRESULT result = Helpers::GetDirect3DDevice9()->CreateTexture(Width, Height, 1, Usage, Format, D3DPOOL_DEFAULT, &gameRenderTexture[index], &sharedHandle);
	if (FAILED(result))
	{
		Logger::err << "[OpenVR] Failed to create game " << index << " texture: " << result << std::endl;
		return;
	}

	result = gameRenderTexture[index]->GetSurfaceLevel(0, &gameRenderSurface[index]);
	if (FAILED(result))
	{
		Logger::err << "[OpenVR] Failed to retrieve game " << index << " surface: " << result << std::endl;
		return;
	}

	ID3D11Resource* tempResource = nullptr;

	// Open shared texture on vr
	result = d3dDevice->OpenSharedResource(sharedHandle, __uuidof(ID3D11Resource), (void**)&tempResource);

	if (FAILED(result))
	{
		Logger::err << "[OpenVR] Failed to open shared resource " << index << ": " << result << std::endl;
		return;
	}

	result = tempResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&vrRenderTexture[index]);
	tempResource->Release();

	if (FAILED(result))
	{
		Logger::err << "[OpenVR] Failed to query texture interface " << index << ": " << result << std::endl;
		return;
	}

	D3D11_TEXTURE2D_DESC desc;
	vrRenderTexture[index]->GetDesc(&desc);

	Logger::log << "[OpenVR] Created shared texture " << index << ", " << desc.Width << "x" << desc.Height << std::endl;
}

void OpenVR::UpdatePoses()
{
	VR_PROFILE_SCOPE(OpenVR_UpdatePoses);

	if (!vrCompositor)
	{
		return;
	}

	VR_PROFILE_START(OpenVR_WaitGetPoses);
	vrCompositor->WaitGetPoses(renderPoses, vr::k_unMaxTrackedDeviceCount, gamePoses, vr::k_unMaxTrackedDeviceCount);
	VR_PROFILE_STOP(OpenVR_WaitGetPoses);

	UpdateSkeleton(ControllerRole::Left);
	UpdateSkeleton(ControllerRole::Right);
	UpdatePose(ControllerRole::Left);
	UpdatePose(ControllerRole::Right);

	if (!vrOverlay || !bMouseVisible)
	{
		return;
	}

	vr::VREvent_t vrEvent;
	while (vrOverlay->PollNextOverlayEvent(uiOverlay, &vrEvent, sizeof(vrEvent)))
	{
		switch (vrEvent.eventType)
		{
		case vr::VREvent_MouseMove:
			mousePos.x = vrEvent.data.mouse.x;
			mousePos.y = 1.0f - vrEvent.data.mouse.y;
			bMousePositionValid = true;
			break;
		case vr::VREvent_MouseButtonDown:
			bMouseDown = true;
			break;
		case vr::VREvent_MouseButtonUp:
			bMouseDown = false;
			break;
		case vr::VREvent_KeyboardClosed_Global:
		case vr::VREvent_KeyboardDone:
			if (Game::instance.uiRenderer) Game::instance.uiRenderer->UpdateActiveButton(nullptr);
			break;
		default:
			break;
		}
	}
	// Recovery build: retain SteamVR's established overlay event path. Do not
	// poll action state again from rendering or call the new intersection path.
}

void OpenVR::UpdateMenuPointer()
{
	if (!vrSystem || !vrInput || !vrOverlay || !bMouseVisible) return;
	std::array<Immersion::MenuRay,2> rays{};
	for (unsigned h=0; h<2; ++h) {
		auto& ray = rays[h];
		const auto index = vrSystem->GetTrackedDeviceIndexForControllerRole(h == 0 ?
			vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
		if (index >= vr::k_unMaxTrackedDeviceCount || !renderPoses[index].bDeviceIsConnected ||
			!renderPoses[index].bPoseIsValid) continue;
		ray.valid = TryGetBoolInput(physicalBindings[h+2].id,ray.down);
		if (!ray.valid) continue;
		// Overlay intersection consumes standing-space poses, without Halo's
		// recenter/yaw/weapon offsets. -Z is the controller's pointing axis.
		const auto& pose = renderPoses[index].mDeviceToAbsoluteTracking;
		vr::VROverlayIntersectionParams_t params{};
		params.eOrigin = vr::TrackingUniverseStanding;
		for (unsigned axis=0; axis<3; ++axis) {
			params.vSource.v[axis] = pose.m[axis][3];
			params.vDirection.v[axis] = -pose.m[axis][2];
		}
		vr::VROverlayIntersectionResults_t result{};
		ray.hit = vrOverlay->ComputeOverlayIntersection(uiOverlay,&params,&result);
		ray.u = result.vUVs.v[0]; ray.v = result.vUVs.v[1];
	}
	const auto point = menuPointer.Update(rays);
	bMousePositionValid = point.positioned;
	if (point.positioned) mousePos = Vector2(point.x,point.y);
	// Do not let a lost controller/overlay leave a mouse button stuck down.
	bMouseDown = point.down;
}

void OpenVR::UpdateSkeleton(ControllerRole hand)
{
	VR_PROFILE_SCOPE(OpenVR_UpdateSkeleton);
	skeletonValid[hand == ControllerRole::Left ? 0 : 1] = false;

	if (!vrInput)
	{
		return;
	}

	vr::InputSkeletalActionData_t actionData;

	vr::EVRInputError err = vrInput->GetSkeletalActionData(hand == ControllerRole::Left ? leftHandSkeleton : rightHandSkeleton, &actionData, sizeof(vr::InputSkeletalActionData_t));

	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not update skeleton action for hand " << static_cast<int>(hand) << ": " << err << std::endl;
		return;
	}

	if (!actionData.bActive)
	{
		return;
	}

	err = vrInput->GetSkeletalBoneData(
		hand == ControllerRole::Left ? leftHandSkeleton : rightHandSkeleton,
		vr::VRSkeletalTransformSpace_Model,
		vr::VRSkeletalMotionRange_WithController,
		bones[hand == ControllerRole::Left ? 0 : 1],
		31
	);

	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not update skeleton for hand " << static_cast<int>(hand) << ": " << err << std::endl;
		return;
	}
	
	const int h = hand == ControllerRole::Left ? 0 : 1;
	skeletonValid[h] = true;

	if (!bHasCachedWrists[h])
	{
		cachedWrists[h] = bones[h][1];
		bHasCachedWrists[h] = true;
	}
}

void OpenVR::UpdatePose(ControllerRole hand)
{
	VR_PROFILE_SCOPE(OpenVR_UpdatePose);

	if (!vrInput || !bHasValidTipPoses)
	{
		return;
	}

	vr::EVRInputError err = vrInput->GetPoseActionDataForNextFrame(
		hand == ControllerRole::Left ? leftHandTip : rightHandTip, vr::TrackingUniverseStanding,
		hand == ControllerRole::Left ? &leftHandTipPose : &rightHandTipPose,
		sizeof(vr::InputPoseActionData_t),
		vr::k_ulInvalidInputValueHandle
	);

	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Can't get tip pose for " << static_cast<int>(hand) << ": " << err << " (aiming may be incorrectly offset)" << std::endl;
		return;
	}
}

void OpenVR::PreDrawFrame(Renderer* renderer, float deltaTime)
{
}

void OpenVR::PositionOverlay()
{
	VR_PROFILE_SCOPE(OpenVR_PositionOverlay);

	// Get the HMD's position and rotation
	vr::HmdMatrix34_t mat = renderPoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
	vr::HmdVector3_t position;
	position.v[0] = mat.m[0][3];
	position.v[1] = mat.m[1][3];
	position.v[2] = mat.m[2][3];

	float distance = bMouseVisible ? Game::instance.c_MenuOverlayDistance->Value() : Game::instance.c_UIOverlayDistance->Value();

	float len = sqrt(mat.m[0][2] * mat.m[0][2] + mat.m[2][2] * mat.m[2][2]);

	distance /= len;

	position.v[0] += mat.m[0][2] * -distance;
	position.v[2] += mat.m[2][2] * -distance;

	// Rotate only around Y for yaw
	float yaw = atan2(-mat.m[2][0], mat.m[2][2]);
	vr::HmdMatrix34_t transform = {
		cos(yaw), 0, sin(yaw), position.v[0],
		0, 0.75f, 0, position.v[1],
		-sin(yaw), 0, cos(yaw), position.v[2]
	};

	// Set the transform for the overlay
	vrOverlay->SetOverlayTransformAbsolute(uiOverlay, vr::TrackingUniverseStanding, &transform);
}

void OpenVR::PostDrawFrame(Renderer* renderer, float deltaTime)
{
	VR_PROFILE_SCOPE(OpenVR_PostDrawFrame);

	if (!vrCompositor)
	{
		return;
	}

	// Wait for frame to finish rendering
	VR_PROFILE_START(OpenVR_QueryD3D);
	IDirect3DQuery9* pEventQuery = nullptr;
	Helpers::GetDirect3DDevice9()->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery);
	if (pEventQuery != nullptr)
	{
		pEventQuery->Issue(D3DISSUE_END);
		while (pEventQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK);
		pEventQuery->Release();
	}
	VR_PROFILE_STOP(OpenVR_QueryD3D);

	VR_PROFILE_START(OpenVR_SubmitEyes);
	vr::Texture_t leftEye { (void*)vrRenderTexture[0], vr::TextureType_DirectX, vr::ColorSpace_Auto};
	vr::EVRCompositorError error = vrCompositor->Submit(vr::Eye_Left, &leftEye, &textureBounds[0], vr::Submit_Default);

	if (error != vr::VRCompositorError_None)
	{
		Logger::log << "[OpenVR] Could not submit left eye texture: " << error << std::endl;
	}

	vr::Texture_t rightEye{ (void*)vrRenderTexture[1], vr::TextureType_DirectX, vr::ColorSpace_Auto };
	error = vrCompositor->Submit(vr::Eye_Right, &rightEye, &textureBounds[1], vr::Submit_Default);

	if (error != vr::VRCompositorError_None)
	{
		Logger::log << "[OpenVR] Could not submit right eye texture: " << error << std::endl;
	}
	VR_PROFILE_STOP(OpenVR_SubmitEyes);

	PositionOverlay();

	vr::Texture_t uiTex{ (void*)vrRenderTexture[uiSurface], vr::TextureType_DirectX, vr::ColorSpace_Auto };
	vr::EVROverlayError oError = vrOverlay->SetOverlayTexture(uiOverlay, &uiTex);

	if (oError != vr::EVROverlayError::VROverlayError_None)
	{
		Logger::log << "[OpenVR] Could not submit ui texture: " << oError << std::endl;
	}

	VR_PROFILE_START(OpenVR_PostPresentHandoff);
	vrCompositor->PostPresentHandoff();
	VR_PROFILE_STOP(OpenVR_PostPresentHandoff);
}

void OpenVR::UpdateCameraFrustum(CameraFrustum* frustum, int eye)
{
	VR_PROFILE_SCOPE(OpenVR_UpdateCameraFrustum);
	frustum->fov = fov;

	Matrix4 eyeMatrix = GetHMDMatrixPoseEye((vr::Hmd_Eye) eye);

	Matrix4 headMatrix = GetHMDTransform(true);

	// Yaw should follow cutscene camera
	CutsceneData* cutscene = Helpers::GetCutsceneData();

	if (cutscene->bInCutscene && !Game::instance.interactions.ViewOverrideActive())
	{
		float cameraYaw = atan2(frustum->facingDirection.y, frustum->facingDirection.x) * (180.0f / 3.1415926f);
		headMatrix.rotateZ(cameraYaw);
	}

    Game::instance.interactions.ApplyViewRotation(headMatrix);
	Matrix4 viewMatrix = (headMatrix * eyeMatrix.invert()).scale(Game::instance.MetresToWorld(1.0f));

	Matrix3 rotationMatrix = GetRotationMatrix(headMatrix);


	frustum->facingDirection = Vector3(1.0f, 0.0f, 0.0f);
	frustum->upDirection = Vector3(0.0f, 0.0f, 1.0f);

	frustum->facingDirection = (rotationMatrix * frustum->facingDirection).normalize();
	frustum->upDirection = (rotationMatrix * frustum->upDirection).normalize();

	Vector3 newPos = viewMatrix * Vector3(0.0f, 0.0f, 0.0f);

	frustum->position = frustum->position + newPos;
}

int OpenVR::GetViewWidth()
{
    return recommendedWidth;
}

int OpenVR::GetViewHeight()
{
    return recommendedHeight;
}

float OpenVR::GetViewWidthStretch()
{
	return recommendedWidth / static_cast<float>(realWidth);
}

float OpenVR::GetViewHeightStretch()
{
	return recommendedHeight / static_cast<float>(realHeight);
}

float OpenVR::GetAspect()
{
	return aspect;
}

int OpenVR::GetScopeWidth()
{
	return scopeWidth;
}

int OpenVR::GetScopeHeight()
{
	return scopeHeight;
}

void OpenVR::Recentre()
{
	SetLocationOffset(ConvertSteamVRMatrixToMatrix4(renderPoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking) * Vector3(0.0f, 0.0f, 0.0f));
}

void OpenVR::SetLocationOffset(Vector3 newOffset)
{
	positionOffset = newOffset;
}

Vector3 OpenVR::GetLocationOffset()
{
	return positionOffset;
}

void OpenVR::SetYawOffset(float newOffset)
{
	yawOffset = newOffset;
}

float OpenVR::GetYawOffset()
{
	return yawOffset;
}

void OpenVR::TriggerHapticVibration(ControllerRole role, float fStartSecondsFromNow, float fDurationSeconds, float fFrequency, float fAmplitude)
{
#if HAPTICS_DEBUG
		Logger::log << "[WeaponHaptics] TriggerHapticVibration called with: \tRole: " << static_cast<int>(role)
		<<
		"\tstartSeconds: "
		<< fStartSecondsFromNow
		<< "\t"
		"duration: "
		<< fDurationSeconds
		<< "\t"
		"frequency: "
		<< fFrequency
		<< "\t"
		"amplitude: "
		<< fAmplitude
		<< "\t"
		<< std::endl;
#endif 

	vr::VRActionHandle_t* action;
	
	if (role == ControllerRole::Left)
	{
		action = &leftFire;
	}
	else 
	{
		action = &rightFire;
	}

	if (action)
	{
		vr::EVRInputError error = vrInput->TriggerHapticVibrationAction(*action, fStartSecondsFromNow, fDurationSeconds, fFrequency, fAmplitude, vr::k_ulInvalidInputValueHandle);

#if HAPTICS_DEBUG
		if (error != vr::EVRInputError::VRInputError_None)
		{
			Logger::log << "[OpenVR] Could not trigger haptic vibration action: " << error << std::endl;
		}
#endif
	}
}

void OpenVR::TriggerHapticPulse(ControllerRole role, short usDurationMicroSec)
{
#if HAPTICS_DEBUG
	Logger::log << "[WeaponHaptics] TriggerHapticPulse called with: \tRole: " << static_cast<int>(role)
		<<
		"\tusDurationMicroSec: "
		<< usDurationMicroSec
		<< "\t"
		<< std::endl;
#endif 
	
	vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(role == ControllerRole::Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
	vrSystem->TriggerHapticPulse(controllerIndex, 0, usDurationMicroSec);
}

Matrix4 OpenVR::GetHMDTransform(bool bRenderPose)
{
	VR_PROFILE_SCOPE(OpenVR_GetHMDTransform);
	if (bRenderPose)
	{
		return ConvertSteamVRMatrixToMatrix4(renderPoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking).translate(-positionOffset).rotateZ(-yawOffset);
	}
	else
	{
		return ConvertSteamVRMatrixToMatrix4(gamePoses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking).translate(-positionOffset).rotateZ(-yawOffset);
	}
}

bool OpenVR::IsHMDTracked() const
{
	const auto& head = gamePoses[vr::k_unTrackedDeviceIndex_Hmd];
	return vrSystem && head.bDeviceIsConnected && head.bPoseIsValid;
}

bool OpenVR::IsControllerTracked(ControllerRole role) const
{
	if (!vrSystem) return false;
	const auto index = vrSystem->GetTrackedDeviceIndexForControllerRole(
		role == ControllerRole::Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
	if (index >= vr::k_unMaxTrackedDeviceCount) return false;
	const auto& head = gamePoses[vr::k_unTrackedDeviceIndex_Hmd];
	const auto& hand = gamePoses[index];
	return head.bDeviceIsConnected && head.bPoseIsValid && hand.bDeviceIsConnected && hand.bPoseIsValid;
}

Matrix4 OpenVR::GetRawControllerTransform(ControllerRole role, bool bRenderPose)
{
	VR_PROFILE_SCOPE(OpenVR_GetRawControllerTransform);
	if (!vrSystem) return Matrix4();
	vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(role == ControllerRole::Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
	if (controllerIndex >= vr::k_unMaxTrackedDeviceCount) return Matrix4();

	Matrix4 outMatrix;

	if (bRenderPose)
	{
		return ConvertSteamVRMatrixToMatrix4(renderPoses[controllerIndex].mDeviceToAbsoluteTracking).translate(-positionOffset).rotateZ(-yawOffset);
	}
	else
	{
		return ConvertSteamVRMatrixToMatrix4(gamePoses[controllerIndex].mDeviceToAbsoluteTracking).translate(-positionOffset).rotateZ(-yawOffset);
	}
}

Matrix4 OpenVR::GetControllerTransformInternal(ControllerRole role, int bone, bool bRenderPose)
{
	VR_PROFILE_SCOPE(OpenVR_GetControllerTransformInternal);
	if (!vrSystem)
	{
		return Matrix4();
	}

	vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(role == ControllerRole::Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);

	const int roleId = role == ControllerRole::Left ? 0 : 1;
	if ((bone < 0 && !bHasCachedWrists[roleId]) || bone >= 31 ||
		(bone >= 0 && !skeletonValid[roleId])) return GetRawControllerTransform(role,bRenderPose);

	vr::VRBoneTransform_t boneTransform = bone < 0 ? cachedWrists[roleId] : bones[roleId][bone];

	Matrix4 outMatrix = GetRawControllerTransform(role, bRenderPose);

	Vector3 bonePos = Vector3(boneTransform.position.v[0], boneTransform.position.v[1], boneTransform.position.v[2]);
	Vector4 quat = Vector4(boneTransform.orientation.x, boneTransform.orientation.y, boneTransform.orientation.z, boneTransform.orientation.w);

	Matrix4 boneMatrix;
	Transform tempTransform;
	Helpers::MakeTransformFromQuat(&quat, &tempTransform);

	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			// Not sure why get is const, you can directly set the values with setrow/setcolumn anyway
			const_cast<float*>(boneMatrix.get())[x + y * 4] = tempTransform.rotation[x + y * 3];
		}
	}

	boneMatrix.setColumn(3, bonePos);

	Matrix4 boneMatrixGame(
		boneMatrix.get()[2 + 2 * 4], boneMatrix.get()[0 + 2 * 4], -boneMatrix.get()[1 + 2 * 4], 0.0,
		boneMatrix.get()[2 + 0 * 4], boneMatrix.get()[0 + 0 * 4], -boneMatrix.get()[1 + 0 * 4], 0.0,
		-boneMatrix.get()[2 + 1 * 4], -boneMatrix.get()[0 + 1 * 4], boneMatrix.get()[1 + 1 * 4], 0.0,
		-boneMatrix.get()[2 + 3 * 4], -boneMatrix.get()[0 + 3 * 4], boneMatrix.get()[1 + 3 * 4], 1.0f
	);

	Vector3 pos = boneMatrixGame * Vector3(0.0f, 0.0f, 0.0f);
	boneMatrixGame.translate(-pos);
	boneMatrixGame.rotate(180.0f, boneMatrixGame * Vector3(0.0f, 0.0f, 1.0f));
	boneMatrixGame.translate(pos);

	outMatrix = outMatrix * boneMatrixGame;

	return outMatrix;
}

Matrix4 OpenVR::GetControllerTransform(ControllerRole role, bool bRenderPose)
{
	return GetControllerTransformInternal(role, -1, bRenderPose);
}

Matrix4 OpenVR::GetControllerBoneTransform(ControllerRole role, int bone, bool bRenderPose)
{
	return GetControllerTransformInternal(role, bone, bRenderPose);
}

Vector3 OpenVR::GetControllerVelocity(ControllerRole role, bool bRenderPose)
{
	VR_PROFILE_SCOPE(OpenVR_GetControllerVelocity);
	if (!vrSystem)
	{
		return Vector3();
	}
	
	vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(role == ControllerRole::Left ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);

	vr::HmdVector3_t velocity;

	if (bRenderPose)
	{
		velocity = renderPoses[controllerIndex].vVelocity;
	}
	else
	{
		velocity = gamePoses[controllerIndex].vVelocity;
	}

	Matrix4 rotMat;
	rotMat.rotateZ(-yawOffset);

	return rotMat * Vector3(-velocity.v[2], -velocity.v[0], velocity.v[1]) * Game::instance.MetresToWorld(1.0f);
}

bool OpenVR::TryGetControllerFacing(ControllerRole role, Vector3& outDirection)
{
	VR_PROFILE_SCOPE(OpenVR_TryGetControllerFacing);

	// todo: only get once, cache offset, use that
	if (bHasValidTipPoses)
	{
		const vr::InputPoseActionData_t& data = role == ControllerRole::Left ? leftHandTipPose : rightHandTipPose;
		if (!data.bActive || !data.pose.bPoseIsValid)
		{
			return false;
		}

		Matrix4 facingMatrix = ConvertSteamVRMatrixToMatrix4(data.pose.mDeviceToAbsoluteTracking).translate(-positionOffset).rotateZ(-yawOffset);

		outDirection = facingMatrix.getLeftAxis();
	}
	return bHasValidTipPoses;
}

IDirect3DSurface9* OpenVR::GetRenderSurface(int eye)
{
    return gameRenderSurface[eye];
}

IDirect3DTexture9* OpenVR::GetRenderTexture(int eye)
{
    return gameRenderTexture[eye];
}

IDirect3DSurface9* OpenVR::GetUISurface()
{
    return gameRenderSurface[uiSurface];
}

IDirect3DSurface9* OpenVR::GetCrosshairSurface()
{
	return gameRenderSurface[crosshairSurface];
}

IDirect3DSurface9* OpenVR::GetScopeSurface()
{
	return gameRenderSurface[scopeSurface];
}

IDirect3DTexture9* OpenVR::GetScopeTexture()
{
	return gameRenderTexture[scopeSurface];
}

void OpenVR::SetMouseVisibility(bool bIsVisible)
{
	VR_PROFILE_SCOPE(OpenVR_SetMouseVisibility);

	if (!vrOverlay)
	{
		return;
	}

	if (bMouseVisible == bIsVisible)
	{
		return;
	}

	bMouseVisible = bIsVisible;
	bMousePositionValid = false;
	menuPointer.Reset();
	Logger::log << "[VRMenu] " << (bIsVisible ? "Native overlay events; right A confirms focused buttons." : "Closed.") << std::endl;

	if (!bMouseVisible)
	{
		bMouseDown = false;
	}

	vrOverlay->SetOverlayInputMethod(uiOverlay, bIsVisible ? vr::VROverlayInputMethod_Mouse : vr::VROverlayInputMethod_None);
	vrOverlay->SetOverlayWidthInMeters(uiOverlay, bIsVisible ? Game::instance.c_MenuOverlayScale->Value() : Game::instance.c_UIOverlayScale->Value());
}

void OpenVR::SetCrosshairTransform(Matrix4& newTransform)
{
	VR_PROFILE_SCOPE(OpenVR_SetCrosshairTransform);

	// This should be moved into game code really

	Vector3 pos = (newTransform * Vector3(0.0f, 0.0f, 0.0f)) * Game::instance.MetresToWorld(1.0f) + Helpers::GetCamera().position;
	Matrix3 rot;
	Vector2 size(1.33f, 1.0f);
	size *= Game::instance.MetresToWorld(Game::instance.c_CrosshairScale->Value()) * Game::instance.AccuracyReticleScale();

	newTransform.translate(-pos);
	newTransform.rotate(90.0f, newTransform.getLeftAxis());
	newTransform.rotate(-90.0f, newTransform.getUpAxis());
	newTransform.rotate(-90.0f, newTransform.getLeftAxis());

	for (int i = 0; i < 3; i++)
	{
		rot.setColumn(i, &newTransform.get()[i * 4]);
	}

	Game::instance.inGameRenderer.DrawRenderTarget(gameRenderTexture[crosshairSurface], pos, rot, size, false);
}

void OpenVR::SetActiveActionSet(int index, const std::string& actionSetName)
{	
    vr::EVRInputError error = vrInput->GetActionSetHandle(actionSetName.c_str(), &actionSets[index].ulActionSet);
    if (error != vr::VRInputError_None)
    {
        Logger::err << "[OpenVR] Could not set active action set: " << actionSetName << " Error: " << error << std::endl;
    }
}

void OpenVR::UpdateInputs()
{
	for (auto& binding : physicalBindings) binding.rebaseline = false;
	SetActiveActionSet(0, "/actions/default");
	SetActiveActionSet(1, "/actions/left handed");

	VR_PROFILE_SCOPE(OpenVR_UpdateInputs);

	vr::EVRInputError error = vrInput->UpdateActionState(actionSets, sizeof(vr::VRActiveActionSet_t), 2);
	inputStateValid = error == vr::VRInputError_None;

	if (error != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not update inputs: " << error << std::endl;
	}
}

InputBindingID OpenVR::RegisterBoolInput(std::string set, std::string action)
{
	InputBindingID id = 0;
	vr::EVRInputError err = vrInput->GetActionHandle(("/actions/" + set + "/in/" + action).c_str(), &id);
	// Existing SteamVR personal bindings may omit newly added optional actions.
	// Their old grip/fire actions remain usable. Keep these aliases tied to a
	// physical input source, never to the current firing hand or saved layout.
	const char* names[] = {"PhysicalLeftGrip","PhysicalRightGrip","PhysicalLeftTrigger","PhysicalRightTrigger",
		"PhysicalJump","PhysicalGrenade","PhysicalGrenadeCycle","PhysicalRecentre","PhysicalInteract"};
	const char* aliases[] = {"/actions/default/in/SwapWeaponHand","/actions/left handed/in/SwapWeaponHand",
		"/actions/left handed/in/Fire","/actions/default/in/Fire",
		"/actions/default/in/Grenade","/actions/default/in/Jump","/actions/default/in/SwitchGrenades","/actions/default/in/MenuBack","/actions/default/in/Interact"};
	const bool leftSource[] = {true,false,true,false,false,false,true,true,false};
	if (set == "default") for (unsigned slot=0; slot<physicalBindings.size(); ++slot) if (action == names[slot]) {
		auto& binding = physicalBindings[slot];
		binding = {}; binding.name = action;
		binding.primary = err == vr::VRInputError_None ? id : 0;
		if (vrInput->GetActionHandle(aliases[slot],&binding.fallback) != vr::VRInputError_None) binding.fallback = 0;
		if (vrInput->GetInputSourceHandle(leftSource[slot] ? "/user/hand/left" : "/user/hand/right",&binding.source)
			!= vr::VRInputError_None) binding.source = vr::k_ulInvalidInputValueHandle;
		binding.id = binding.primary ? binding.primary : binding.fallback;
		Logger::log << "[PhysicalInput] Registered " << action << " with fixed-hand fallback " << aliases[slot] << std::endl;
		return binding.id;
	}
	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not register bool input /actions/" << set << "/in/" << action << ": " << err << std::endl;
	}
	else
	{
		Logger::log << "[OpenVR] Registered /actions/" << set << "/in/" << action << " with id " << id << std::endl;
	}
	return id;
}

InputBindingID OpenVR::RegisterVector2Input(std::string set, std::string action)
{
	if(set=="default" && (action=="PhysicalMove" || action=="PhysicalLook")){
		const unsigned hand=action=="PhysicalMove"?0u:1u;
		auto& binding=physicalSticks[hand];binding={};binding.name=action;
		// Keep this ID distinct from all aliases so registering an old Move/Look
		// action for on-foot controls cannot accidentally restrict it to one hand.
		binding.id=(~InputBindingID(0))-hand;
		const std::string paths[]={"/actions/default/in/"+action,
			"/actions/default/in/Move","/actions/left handed/in/Move",
			"/actions/default/in/Look","/actions/left handed/in/Look"};
		for(unsigned i=0;i<binding.actions.size();++i)
			if(vrInput->GetActionHandle(paths[i].c_str(),&binding.actions[i])!=vr::VRInputError_None)binding.actions[i]=0;
		if(vrInput->GetInputSourceHandle(hand==0?"/user/hand/left":"/user/hand/right",&binding.source)!=vr::VRInputError_None)
			binding.source=vr::k_ulInvalidInputValueHandle;
		Logger::log<<"[PhysicalStick] Registered "<<action<<" with original Move/Look fallbacks on "
			<<(hand==0?"left":"right")<<" controller"<<std::endl;
		return binding.id;
	}
	InputBindingID id=0;
	vr::EVRInputError err = vrInput->GetActionHandle(("/actions/" + set + "/in/" + action).c_str(), &id);
	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not register vector2 input /actions/" << set << "/in/" << action << ": " << err << std::endl;
	}
	else
	{
		Logger::log << "[OpenVR] Registered /actions/" << set << "/in/" << action << " with id " << id << std::endl;
	}
	return id;
}

bool OpenVR::GetBoolInput(InputBindingID id)
{
	static bool dummy = false;
	return GetBoolInput(id, dummy);
}

bool OpenVR::GetBoolInput(InputBindingID id, bool& bHasChanged)
{
	bHasChanged = false;
	if (!vrInput || id == 0 || !inputStateValid) return false;
	vr::InputDigitalActionData_t digital{};
	vr::EVRInputError err = vrInput->GetDigitalActionData(id, &digital, sizeof(vr::InputDigitalActionData_t), vr::k_ulInvalidInputValueHandle);
	if (err != vr::VRInputError_None)
	{
		Logger::log << "[OpenVR] Could not get digital action: " << err << std::endl;
		return false;
	}
	if (!digital.bActive) return false;

	bHasChanged = digital.bChanged;

	if (digital.bChanged && digital.bState)
	{
		Logger::log << "[OpenVR] Getting binding " << id << std::endl;
	}

	return digital.bState;
}

bool OpenVR::TryGetBoolInput(InputBindingID id, bool& down)
{
	down = false;
	if (!vrInput || !inputStateValid || id == 0) return false;
	for (auto& binding : physicalBindings) if (binding.id == id) {
		auto read = [&](InputBindingID action) {
			vr::InputDigitalActionData_t sample{};
			if (!action || binding.source == vr::k_ulInvalidInputValueHandle ||
				vrInput->GetDigitalActionData(action,&sample,sizeof(sample),binding.source) != vr::VRInputError_None || !sample.bActive)
				return false;
			down = sample.bState;
			return true;
		};
		const int route = read(binding.primary) ? 1 : (read(binding.fallback) ? 2 : 0);
		binding.rebaseline = binding.rebaseline || (binding.route >= 0 && route != binding.route);
		if (route != binding.route) {
			Logger::log << "[PhysicalInput] " << binding.name << ": " <<
				(route == 1 ? "physical action active" : route == 2 ? "using original fixed-hand binding" : "no active binding on its physical hand") << std::endl;
			binding.route = route;
		}
		// A changed source must not synthesize a grip release or trigger press.
		return route != 0 && !binding.rebaseline;
	}
	vr::InputDigitalActionData_t digital{};
	if (vrInput->GetDigitalActionData(id, &digital, sizeof(digital), vr::k_ulInvalidInputValueHandle)
		!= vr::VRInputError_None || !digital.bActive) return false;
	down = digital.bState;
	return true;
}

Vector2 OpenVR::GetVector2Input(InputBindingID id)
{
	if(!vrInput || !inputStateValid || !id)return {};
	for(auto& binding:physicalSticks)if(binding.id==id){
		Vector2 value{};int route=0;
		if(binding.source!=vr::k_ulInvalidInputValueHandle)for(unsigned i=0;i<binding.actions.size();++i){
			vr::InputAnalogActionData_t sample{};
			// GetActionHandle may succeed for an action missing from the active
			// manifest. Check the actual read, then try the original bindings.
			if(!binding.actions[i] || vrInput->GetAnalogActionData(binding.actions[i],&sample,sizeof(sample),binding.source)!=vr::VRInputError_None ||
				!sample.bActive || !std::isfinite(sample.x) || !std::isfinite(sample.y))continue;
			value={sample.x,sample.y};route=static_cast<int>(i)+1;break;
		}
		if(route!=binding.route){
			const char* labels[]={"no active binding on its physical hand","physical action active",
				"using original default Move","using original left-handed Move",
				"using original default Look","using original left-handed Look"};
			Logger::log<<"[PhysicalStick] "<<binding.name<<": "<<labels[route]<<std::endl;
			binding.route=route;
		}
		// An active centered stick wins. Never add aliases or substitute the
		// opposite hand, even when it is moving and this hand is centered.
		return value;
	}
	vr::InputAnalogActionData_t Analog{};
	vr::EVRInputError err = vrInput->GetAnalogActionData(id, &Analog, sizeof(vr::InputAnalogActionData_t), vr::k_ulInvalidInputValueHandle);
	if(analogErrors[id]!=static_cast<int>(err)){
		Logger::log<<"[OpenVR] Analog action "<<id<<" result="<<err<<std::endl;
		analogErrors[id]=static_cast<int>(err);
	}

	if(err!=vr::VRInputError_None || !Analog.bActive || !std::isfinite(Analog.x) || !std::isfinite(Analog.y))return {};
	return Vector2(Analog.x, Analog.y);
}

Vector2 OpenVR::GetMousePos()
{
	return mousePos;
}

bool OpenVR::GetMouseDown()
{
	return bMouseDown;
}

void OpenVR::ShowKeyboard(const std::string& textBuffer)
{
	if (!vrOverlay)
	{
		return;
	}

	strncpy(keyboardBuffer, textBuffer.c_str(), 256);

	vrOverlay->ShowKeyboardForOverlay(uiOverlay, vr::k_EGamepadTextInputModeSubmit, vr::k_EGamepadTextInputLineModeSingleLine, vr::KeyboardFlag_Modal, "VR Settings", 256, textBuffer.c_str(), 0);
	bKeyboardVisible = true;
}

bool OpenVR::IsKeyboardVisible()
{
	return bKeyboardVisible;
}

void OpenVR::HideKeyboard()
{
	if (!vrOverlay)
	{
		return;
	}

	vrOverlay->HideKeyboard();
	bKeyboardVisible = false;
}

std::string OpenVR::GetKeyboardInput()
{
	if (!vrOverlay)
	{
		return std::string();
	}

	vrOverlay->GetKeyboardText(keyboardBuffer, 256);
	return keyboardBuffer;
}

std::string OpenVR::GetDeviceName()
{
	if (!vrSystem)
	{
		return "Unknown";
	}

	char str[128] = {};

	uint32_t size = vrSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_ModelNumber_String, str, 128);

	if (size == 0)
	{
		return "Unknown";
	}

	return str;
}
