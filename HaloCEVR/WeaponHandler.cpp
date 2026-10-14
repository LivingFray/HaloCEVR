#include "WeaponHandler.h"
#include "Immersion/HandGripPose.h"
#include <chrono>
#include "Helpers/Objects.h"
#include "Helpers/Camera.h"
#include "Helpers/Assets.h"
#include "Helpers/Maths.h"
#include "Helpers/Menus.h"
#include "Helpers/Cutscene.h"
#include "Logger.h"
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <cstring>

// This is a working decomp of the game's original logic for updating the view model's skeleton
// Also used as a temporary fallback when no matching weapon has been resolved.
static void ReferenceUpdateViewModelImpl(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* boneTransforms, Transform* outBoneTransforms)
{
	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	AssetData_ModelAnimations* animationData = viewModel->Data;
    if (!animationData || !animationData->BoneArray || animationData->NumBones <= 0 || animationData->NumBones > 64) return;
	Bone* boneArray = animationData->BoneArray;
	Transform root;
	Helpers::MakeTransformFromXZ(up, facing, &root);
	root.translation = *pos;

	int i = 0;

	if (animationData->NumBones > 0)
	{
		int lastIndex = 1;
		int16_t bonesToProcess[64]{};

		bonesToProcess[0] = 0;

		do
		{
			const int16_t boneIdx = bonesToProcess[i];
			i++;
			const Bone& currentBone = boneArray[boneIdx];
			const Transform* parentTransform = boneIdx == 0 ? &root : &outBoneTransforms[currentBone.Parent];
			const TransformQuat* currentQuat = &boneTransforms[boneIdx];
			Transform tempTransform;
			Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
			tempTransform.scale = currentQuat->scale;
			tempTransform.translation = currentQuat->translation;
			Helpers::CombineTransforms(parentTransform, &tempTransform, &outBoneTransforms[boneIdx]);

			if (currentBone.LeftLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.LeftLeaf;
				lastIndex++;
			}
			if (currentBone.RightLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.RightLeaf;
				lastIndex++;
			}

		} while (i != lastIndex);
	}
}

void WeaponHandler::UpdateViewModel(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* boneTransforms, Transform* outBoneTransforms)
{
	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	if (!viewModel)
	{
		Logger::log << "[UpdateViewModel] Can't get view model asset" << std::endl;
		return;
	}

	AssetData_ModelAnimations* animationData = viewModel->Data;
    if (!animationData || !animationData->BoneArray || animationData->NumBones <= 0 || animationData->NumBones > 64) return;
	Bone* boneArray = animationData->BoneArray;

    HaloID weaponID{0xffff,0xffff}, tag{0xffff,0xffff}, animations{0xffff,0xffff};
    Asset_Weapon* weapon = nullptr;
    if (Game::instance.PhysicalVisualsActive()) {
        if (Game::instance.PhysicalViewModelWeapon(weaponID,tag,animations) && animations == id)
            weapon = Helpers::GetTypedAsset<Asset_Weapon>(tag);
    } else {
        const auto* player = Helpers::GetLocalPlayer();
        if (player) {
            weaponID = player->weapon;
            const auto* object = Helpers::GetDynamicObject(weaponID);
            if (object) weapon = Helpers::GetTypedAsset<Asset_Weapon>(object->tagID);
        }
    }
    if (!weapon || !weapon->WeaponData || weapon->WeaponData->ViewAnimationID != id) {
        cachedViewModel = {};
        ReferenceUpdateViewModelImpl(id,pos,facing,up,boneTransforms,outBoneTransforms);
        return;
    }
    if (!cachedViewModel.valid || cachedViewModel.currentAsset != id || cachedViewModel.currentWeapon != weaponID) {
        if (!UpdateCache(id,weaponID,weapon,animationData)) {
            cachedViewModel = {};
            // A rejected graph must not be traversed by the native fallback.
            for (int bone=0;bone<animationData->NumBones;++bone) outBoneTransforms[bone]={};
            return;
        }
    }

	Vector3& camPos = Helpers::GetCamera().position;

	pos->x = camPos.x;
	pos->y = camPos.y;
	pos->z = camPos.z;

	Matrix4 handTransform;
	CalculateHandTransform(pos, handTransform);

	if (Game::instance.bUse3DOFAiming)
	{
		// Initialize yaw offset tracking when entering 3DOF mode
		if (!bWasIn3DOFMode)
		{
			lastYawOffset = Game::instance.GetVR()->GetYawOffset();
			bWasIn3DOFMode = true;
		}

		// Apply HMD translation so weapon follows player's body position when leaning
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform();
		Vector3 hmdPosition = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		Vector3 hmdOffset = hmdPosition * Game::instance.MetresToWorld(1.0f);
		pos->x += hmdOffset.x;
		pos->y += hmdOffset.y;
		pos->z += hmdOffset.z;

		// Apply configurable weapon offset in world space (hot-reloadable)
		Vector3 configOffset = Game::instance.c_3DOFWeaponOffset->Value();
        Vector3 weaponOffset = configOffset * Game::instance.MetresToWorld(1.0f);
		pos->x += weaponOffset.x;
		pos->y += weaponOffset.y;
		pos->z += weaponOffset.z;

		// Apply controller rotation to weapon facing/up vectors
		// Only use pitch and yaw from the controller, zero out roll

		// Extract rotation from controller
		Vector3 controllerPos = handTransform * Vector3(0.0f, 0.0f, 0.0f);
		Matrix4 rotationOnly = handTransform;
		rotationOnly.translate(-controllerPos);

		// Get the facing direction from controller (this has pitch and yaw)
		Vector3 facingDir = rotationOnly * Vector3(1.0f, 0.0f, 0.0f);

		// Detect snap turns and rotate smoothed direction to prevent lerping artifact
		float currentYawOffset = Game::instance.GetVR()->GetYawOffset();
		float yawDelta = currentYawOffset - lastYawOffset;

		// Detect snap turns and rotate smoothed direction
		Helpers::RotateForSnapTurn(smoothed3DOFFacingDir, yawDelta, Game::instance.c_SnapTurnAmount->Value());

		// Update tracked yaw offset for next frame
		lastYawOffset = currentYawOffset;

		// Apply cosmetic motion smoothing to facing direction
		float smoothingAmount = Game::instance.c_3DOFWeaponSmoothingAmount->Value();
		float clampedSmoothing = std::clamp(smoothingAmount, 0.0f, 2.0f);

		if (clampedSmoothing > 0.0f)
		{
			const float scaleFactor = (-20.0f / 9.0f);
			float h = 90.0f * log2(1.0f - exp(clampedSmoothing * scaleFactor));
			float t = 1.0f - pow(2.0f, Game::instance.lastDeltaTime * h);
			smoothed3DOFFacingDir = Helpers::Lerp(smoothed3DOFFacingDir, facingDir, t);
			facingDir = smoothed3DOFFacingDir;
			facingDir.normalize();
		}
		else
		{
			// No smoothing - update cached value for when smoothing is re-enabled
			smoothed3DOFFacingDir = facingDir;
		}

		// Use world up (0, 0, 1) to derive a roll-free orientation
		// Cross product of world up and facing gives us the right vector
		Vector3 worldUp(0.0f, 0.0f, 1.0f);
		Vector3 rightDir = worldUp.cross(facingDir);
		rightDir.normalize();

		// Cross product of facing and right gives us the corrected up vector (roll-free)
		Vector3 upDir = facingDir.cross(rightDir);
		upDir.normalize();

		facing->x = facingDir.x;
		facing->y = facingDir.y;
		facing->z = facingDir.z;

		up->x = upDir.x;
		up->y = upDir.y;
		up->z = upDir.z;
	}
	else
	{
		// 6DOF MODE: Standard camera-relative orientation
		// Reset the 3DOF mode flag so we reinitialize when switching back
		bWasIn3DOFMode = false;

		facing->x = 1.0f;
		facing->y = 0.0f;
		facing->z = 0.0f;

		up->x = 0.0f;
		up->y = 0.0f;
		up->z = 1.0f;
	}

	Transform root;
	Helpers::MakeTransformFromXZ(up, facing, &root);
	root.translation = *pos;

	Transform unmodifiedHandTransform;
	CalculateBoneTransform(cachedViewModel.rightWristIndex, boneArray, root, boneTransforms, unmodifiedHandTransform);

	Transform realTransforms[64]{};

	if (animationData->NumBones > 0)
	{
		int lastIndex = 1;
		int16_t bonesToProcess[64]{};

		bonesToProcess[0] = 0;

		int i = 0;
		do
		{
			const int16_t boneIndex = bonesToProcess[i];
			i++;
			const Bone& currentBone = boneArray[boneIndex];
			Transform* parentTransform = boneIndex == 0 ? &root : &outBoneTransforms[currentBone.Parent];
			const TransformQuat* currentQuat = &boneTransforms[boneIndex];
			Transform tempTransform;
			Transform modifiedTransform;
			// For all bones but the root sub in the ACTUAL transform for the calculations (free from scaling/transform issues)
			if (boneIndex > 0)
			{
				modifiedTransform = *parentTransform;
				*parentTransform = realTransforms[currentBone.Parent];
			}

			Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
			tempTransform.scale = currentQuat->scale;

			if (boneIndex == cachedViewModel.displayIndex && Game::instance.bLeftHanded)
			{
				// Bit of a nasty place to do this, but we need to unflip the ammo counter on the BR
				Matrix3 rot = tempTransform.rotation;
				rot *= Matrix3(
					1.0f, 0.0f, 0.0f,
					0.0f, -1.0f, 0.0f,
					0.0f, 0.0f, 1.0f
				);
				
				for (int i = 0; i < 9; i++)
				{
					tempTransform.rotation[i] = rot[i];
				}
			}

			tempTransform.translation = currentQuat->translation;
			Helpers::CombineTransforms(parentTransform, &tempTransform, &outBoneTransforms[boneIndex]);

			if (boneIndex > 0)
			{
				// Restore the modified transform
				*parentTransform = modifiedTransform;
			}
			// Cache the calculated transform for this bone
			realTransforms[boneIndex] = outBoneTransforms[boneIndex];
			if (boneIndex > 0 && (currentBone.Parent == 0 || boneArray[currentBone.Parent].Parent == 0))
			{
				// Hide arms/root in 6DOF mode only
				if (!Game::instance.bUse3DOFAiming)
				{
					outBoneTransforms[boneIndex].scale = 0.0f;
				}
			}
			else if (boneIndex == cachedViewModel.rightWristIndex)
			{
				// Skip VR hand tracking in 3DOF mode - use original weapon bone transforms
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: VR hand tracking
					// This is dreadful code. Rework to be less insane
					if (!Game::instance.bUseTwoHandAim)
					{
						Matrix4 newTransform = handTransform;
						if (currentBone.RightLeaf != -1)
						{
							Bone& GunBone = boneArray[currentBone.RightLeaf];
							if (currentBone.RightLeaf == cachedViewModel.gunIndex)
							{
								const TransformQuat* GunQuat = &boneTransforms[currentBone.RightLeaf];
								Helpers::MakeTransformFromQuat(&GunQuat->rotation, &tempTransform);

								Matrix4 rotation;
								for (int x = 0; x < 3; x++)
								{
									for (int y = 0; y < 3; y++)
									{
										rotation[x + y * 4] = tempTransform.rotation[x + y * 3];
									}
								}

								if (Game::instance.bLeftHanded)
								{
									Matrix4 scale;
									scale.scale(1.0f, -1.0f, 1.0f);

									rotation = scale * rotation * scale;
								}

								newTransform = newTransform * rotation.invert();
							}
							else
							{
								Logger::log << "ERROR: Right leaf of " << currentBone.BoneName << " is " << GunBone.BoneName << std::endl;
							}

						}
						MoveBoneToTransform(boneIndex, newTransform, realTransforms, outBoneTransforms);
					}
					else
					{
						MoveBoneToTransform(boneIndex, handTransform, realTransforms, outBoneTransforms);
					}
					CreateEndCap(boneIndex, currentBone, outBoneTransforms);
				}
			}
			else if (boneIndex == cachedViewModel.leftWristIndex)
			{
				// Skip VR hand tracking in 3DOF mode - use original weapon bone transforms
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: VR hand tracking
					if (!Game::instance.bUseTwoHandAim)
					{
						Matrix4 newTransform = Game::instance.GetVR()->GetControllerTransform(Game::instance.bLeftHanded ? ControllerRole::Right : ControllerRole::Left, true);
						// Apply scale only to translation portion
						Vector3 translation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
						newTransform.translate(-translation);
						translation *= Game::instance.MetresToWorld(1.0f);
						translation += *pos;
						newTransform.translate(translation);

						MoveBoneToTransform(boneIndex, newTransform, realTransforms, outBoneTransforms);
					}
					else
					{
						// Convert both hand transforms to matrix4
						Matrix4 leftMatrix;
						TransformToMatrix4(outBoneTransforms[boneIndex], leftMatrix);
						Matrix4 rightMatrix;
						TransformToMatrix4(unmodifiedHandTransform, rightMatrix);

						// Get inverse of dominant hand, apply it to non-dominant to get delta
						rightMatrix.invertAffine();
						Matrix4 deltaMatrix = rightMatrix * leftMatrix;

						if (Game::instance.bLeftHanded)
						{
							Matrix4 flip;
							flip.scale(1.0f, -1.0f, 1.0f);

							deltaMatrix = flip * deltaMatrix * flip;
							leftMatrix = handTransform * deltaMatrix;
						}
						else
						{
							// Apply delta to controller transform
							leftMatrix = handTransform * deltaMatrix;
						}

						// Move non-dominant hand to new transform
						MoveBoneToTransform(boneIndex, leftMatrix, realTransforms, outBoneTransforms);
					}

					CreateEndCap(boneIndex, currentBone, outBoneTransforms);
				}				
			}
			else if (boneIndex == cachedViewModel.gunIndex)
			{
				// Skip hand-relative gun calculations in 3DOF mode
				if (!Game::instance.bUse3DOFAiming)
				{
					// 6DOF MODE: Calculate gun position/rotation relative to hand
					Vector3 gunPos = outBoneTransforms[boneIndex].translation;
					Matrix3 gunRot = outBoneTransforms[boneIndex].rotation;

					if (Game::instance.bLeftHanded)
					{
						gunRot = gunRot * Matrix3(
							1.0f, 0.0f, 0.0f,
							0.0f, -1.0f, 0.0f,
							0.0f, 0.0f, 1.0f
						);
					}

                    CacheFireAlignment(handTransform,gunPos,gunRot);
				}				
			}

#if DRAW_DEBUG_AIM
			if (currentBone.Parent != -1)
			{
				Game::instance.inGameRenderer.DrawLine3D(outBoneTransforms[boneIndex].translation, outBoneTransforms[currentBone.Parent].translation, D3DCOLOR_ARGB(127, 127, 127, 127), false);
			}
#endif

			if (currentBone.LeftLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.LeftLeaf;
				lastIndex++;
			}
			if (currentBone.RightLeaf != -1)
			{
				bonesToProcess[lastIndex] = currentBone.RightLeaf;
				lastIndex++;
			}

		} while (i != lastIndex);
	}
	// In a support-only hold, keep the gun transform attached to the supporting
	// hand, but move the empty firing hand away independently. Do not move the
	// gun (or its descendants) with the firing-hand skin bones.
	const int wrist = cachedViewModel.rightWristIndex;
	const int count = animationData->NumBones;
	if (Game::instance.PhysicalSupportOnly() && wrist >= 0 && wrist < count && count <= 64 &&
		cachedViewModel.gunIndex >= 0 && cachedViewModel.gunIndex < count && cachedViewModel.gunIndex != wrist) {
		Matrix4 oldWrist;
		TransformToMatrix4(outBoneTransforms[wrist],oldWrist);
		Matrix4 freeHand = Game::instance.GetVR()->GetControllerTransform(
			Game::instance.bLeftHanded ? ControllerRole::Left : ControllerRole::Right,true);
		Vector3 translation = freeHand*Vector3();
		freeHand.translate(-translation);
		freeHand.translate(*pos+translation*Game::instance.MetresToWorld(1));
		MoveBoneToTransform(wrist,freeHand,realTransforms,outBoneTransforms);
		Matrix4 newWrist;
		TransformToMatrix4(outBoneTransforms[wrist],newWrist);
		const Matrix4 delta = newWrist*oldWrist.invertAffine();
		for (int bone=0; bone<count; ++bone) {
			if (bone == wrist) continue;
			int ancestor = bone;
			bool handBone = false;
			for (int depth=0; depth<count && ancestor>=0 && ancestor<count; ++depth) {
				if (ancestor == cachedViewModel.gunIndex) break;
				if (ancestor == wrist) { handBone = true; break; }
				ancestor = boneArray[ancestor].Parent;
			}
			if (!handBone) continue;
			Matrix4 original;
			TransformToMatrix4(outBoneTransforms[bone],original);
			const Matrix4 moved = delta*original;
			outBoneTransforms[bone].translation = moved*Vector3();
			for (int column=0; column<3; ++column) for (int row=0; row<3; ++row)
				outBoneTransforms[bone].rotation[column*3+row] = moved[column*4+row];
		}
		if (boneArray[wrist].Parent >= 0 && boneArray[wrist].Parent < count)
			CreateEndCap(wrist,boneArray[wrist],outBoneTransforms);
	}
    // Reload hit testing samples the tracked palm before support skin snaps.
    Game::instance.ApplyManualReloadPose(cachedViewModel.currentWeapon,boneArray,outBoneTransforms,
        static_cast<unsigned>(count),cachedViewModel.gunIndex,cachedViewModel.leftWristIndex);
    // Compact weapons use a cupped support grip beside the firing hand.
    // This is a skin-only adjustment after gun/aim caching: the support hand
    // cannot rotate the weapon or change its bullet direction.
    const int support=cachedViewModel.leftWristIndex;
    if(Game::instance.bCompactSupportGrip && !Game::instance.bUse3DOFAiming &&
        count>0 && count<=64 && support>=0 && support<count && wrist>=0 && wrist<count &&
        cachedViewModel.gunIndex>=0 && cachedViewModel.gunIndex<count){
        Matrix4 basis=Immersion::GripRotation(handTransform);
        if(Game::instance.bLeftHanded)basis=basis*Matrix4().scale(1,-1,1);
        const bool pistol=cachedViewModel.weaponType==WeaponType::Pistol;
        // Magnum support cups beneath and in front of the firing hand, as
        // when the firing controller rests on the offhand palm.
        Matrix4 desired=basis*(pistol?Immersion::GripAxes({0,-1,0},{-1,0,0},{0,0,-1}):Immersion::GripAxes({1,0,0},{0,0,-1},{0,1,0}));
        const Vector3 gripOffset=pistol?Vector3(.10f,.015f,-.04f):Vector3(.115f,.025f,-.025f);
        const Vector3 center=outBoneTransforms[wrist].translation+(basis*gripOffset)*Game::instance.MetresToWorld(1);
        desired.translate(center);
        Matrix4 fallback=basis;fallback.translate(center-(basis*Vector3(.045f,0,0))*Game::instance.MetresToWorld(1));
        Immersion::PlacePalmGrip(boneArray,count,outBoneTransforms,support,cachedViewModel.gunIndex,nullptr,0,desired,fallback);
        Immersion::CloseGripHand(boneArray,count,outBoneTransforms,support,cachedViewModel.gunIndex,nullptr,0,.65f);
    }
}

inline void WeaponHandler::CalculateBoneTransform(int boneIndex, Bone* boneArray, Transform& root, TransformQuat* boneTransforms, Transform& outTransform) const
{
	// Clear to identity
	Vector3 xVec = Vector3(1.0f, 0.0f, 0.0f);
	Vector3 zVec = Vector3(0.0f, 0.0f, 1.0f);
	Helpers::MakeTransformFromXZ(&zVec, &xVec, &outTransform);

	if (boneIndex < 0)
	{
		return;
	}

	int currentIndex = boneIndex;
	
	while (true)
	{
		Bone& currentBone = boneArray[currentIndex];

		// Convert bone from TransformQuat to Transform
		const TransformQuat* currentQuat = &boneTransforms[currentIndex];
		Transform tempTransform;
		Helpers::MakeTransformFromQuat(&currentQuat->rotation, &tempTransform);
		tempTransform.scale = currentQuat->scale;
		tempTransform.translation = currentQuat->translation;

		// Apply current transform to child transform
		Helpers::CombineTransforms(&tempTransform, &outTransform, &outTransform);

		if (currentIndex == 0)
		{
			break;
		}

		// Get next bone
		currentIndex = currentBone.Parent;
	}

	// Do root
	Helpers::CombineTransforms(&root, &outTransform, &outTransform);
}

inline void WeaponHandler::CalculateHandTransform(Vector3* pos, Matrix4& handTransform) const
{
	Matrix4 newTransform = GetDominantHandTransform();

	// Apply scale only to translation portion
	{
		Vector3 translation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
		newTransform.translate(-translation);
		translation *= Game::instance.MetresToWorld(1.0f);
		translation += *pos;
		newTransform.translate(translation);
	}

	handTransform = newTransform;
}

void WeaponHandler::CreateEndCap(int boneIndex, const Bone& currentBone, Transform* outBoneTransforms) const
{
	// Parent bone to the position of the current bone with 0 scale to act as an end cap
	int idx = currentBone.Parent;
	outBoneTransforms[idx].translation = outBoneTransforms[boneIndex].translation;
	for (int j = 0; j < 9; j++)
	{
		outBoneTransforms[idx].rotation[j] = outBoneTransforms[boneIndex].rotation[j];
	}
	outBoneTransforms[idx].scale = 0.0f;
}

void WeaponHandler::MoveBoneToTransform(int boneIndex, const Matrix4& newTransform, Transform* realTransforms, Transform* outBoneTransforms) const
{
	// Move hands to match controllers
	Vector3 newTranslation = newTransform * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 newRotation4 = Game::instance.bLeftHanded ? newTransform * Matrix4().scale(1.0f, -1.0f, 1.0f) : newTransform;
	newRotation4.translate(-newTranslation);
	newRotation4.rotateZ(localRotation.z);
	newRotation4.rotateY(localRotation.y);
	newRotation4.rotateX(localRotation.x);

	//Add Local offset
	newTranslation += newRotation4 * localOffset;

	outBoneTransforms[boneIndex].translation = newTranslation;
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			outBoneTransforms[boneIndex].rotation[x + y * 3] = newRotation4.get()[x + y * 4];
		}
	}
	realTransforms[boneIndex] = outBoneTransforms[boneIndex]; // Re-cache value to use updated position
}

bool WeaponHandler::UpdateCache(HaloID& id, HaloID& weaponID, Asset_Weapon* weapon, AssetData_ModelAnimations* animationData)
{
#if DRAW_DEBUG_AIM
	Logger::log << "[UpdateCache] Swapped weapons, recaching " << id << std::endl;
#endif
    ViewModelCache candidate;
    candidate.currentAsset = id;
    candidate.currentWeapon = weaponID;
	candidate.leftWristIndex = -1;
	candidate.rightWristIndex = -1;
	candidate.gunIndex = -1;
	candidate.displayIndex = -1;

    if (!animationData || !animationData->BoneArray || animationData->NumBones <= 0 || animationData->NumBones > 64) return false;
	Bone* boneArray = animationData->BoneArray;
    bool visited[64]{};
    int pending[64]{}, size=1;
    visited[0]=true;
    for (int step=0; step<size; ++step) {
        const int index=pending[step];
        const auto& bone=boneArray[index];
        if (index && (bone.Parent < 0 || bone.Parent >= animationData->NumBones || !visited[bone.Parent])) return false;
        for (int child : {bone.LeftLeaf,bone.RightLeaf}) {
            if (child == -1) continue;
            if (child < 0 || child >= animationData->NumBones || visited[child] || size >= 64) return false;
            visited[child]=true; pending[size++]=child;
        }
        int ancestor=index;
        for (int depth=0; ancestor != 0; ++depth) {
            if (depth >= animationData->NumBones || ancestor < 0 || ancestor >= animationData->NumBones) return false;
            ancestor=boneArray[ancestor].Parent;
        }
    }
    if (size != animationData->NumBones) return false;

	for (int i = 0; i < animationData->NumBones; i++)
	{
		Bone& CurrentBone = boneArray[i];
        char name[33]{};
        std::memcpy(name,CurrentBone.BoneName,32);

		if (candidate.leftWristIndex == -1 && strstr(name, "l wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Left Wrist @ " << i << std::endl;
#endif
			candidate.leftWristIndex = i;
		}
		else if (candidate.rightWristIndex == -1 && strstr(name, "r wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Right Wrist @ " << i << std::endl;
#endif
			candidate.rightWristIndex = i;
		}
		else if (candidate.gunIndex == -1 && strstr(name, "gun"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			candidate.gunIndex = i;
		}
		else if (candidate.gunIndex == -1 && strstr(name, "body"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			candidate.gunIndex = i;
		}
		else if (candidate.displayIndex == -1 && strstr(name, "display"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Display @ " << i << std::endl;
#endif
			candidate.displayIndex = i;
		}
#if DRAW_DEBUG_AIM
		else
		{
			Logger::log << "[UpdateCache] Skipped Bone " << CurrentBone.BoneName << std::endl;
		}
#endif
	}

	candidate.fireOffset = Vector3();
	candidate.cookedFireOffset = Vector3();
	candidate.cookedFireRotation = Matrix3();
	candidate.gunOffset = Vector3();

    if (candidate.leftWristIndex <= 0 || candidate.rightWristIndex <= 0 || candidate.gunIndex < 0 ||
        !weapon || !weapon->WeaponData || !weapon->WeaponAsset) return false;
	candidate.weaponType = GetWeaponType(weapon);


	Asset_GBXModel* model = Helpers::GetTypedAsset<Asset_GBXModel>(weapon->WeaponData->ViewModelID);
	if (!model)
	{
		Logger::log << "[UpdateCache] Can't find GBX model from ViewModelID = " << weapon->WeaponData->ViewModelID << std::endl;
		return false;
	}

#if DRAW_DEBUG_AIM
	std::string reversedGroup;
	reversedGroup.assign(model->GroupID, 4);
	std::reverse(reversedGroup.begin(), reversedGroup.end());
	Logger::log << "[UpdateCache] GBXModelTag = " << reversedGroup << std::endl;
	Logger::log << "[UpdateCache] GBXModelPath = " << model->ModelPath << std::endl;
#endif

	if (!model->ModelData || model->ModelData->NumSockets < 0 || model->ModelData->NumSockets > 256 ||
        (model->ModelData->NumSockets && !model->ModelData->Sockets))
	{
		return false;
	}

#if DRAW_DEBUG_AIM
	Logger::log << "[UpdateCache] NumSockets = " << model->ModelData->NumSockets << std::endl;
#endif

	for (int i = 0; i < model->ModelData->NumSockets; i++)
	{
		GBXSocket& socket = model->ModelData->Sockets[i];
#if DRAW_DEBUG_AIM
		Logger::log << "[UpdateCache] Socket = " << socket.SocketName << std::endl;
#endif

		if (strstr(socket.SocketName, "primary trigger"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Effects location, Num Transforms = " << socket.NumTransforms << std::endl;
#endif

			if (socket.NumTransforms <= 0 || !socket.Transforms)
			{
				Logger::log << "[UpdateCache] " << socket.SocketName << " has no transforms" << std::endl;
				break;
			}

			candidate.cookedFireOffset = socket.Transforms[0].Position;
			Transform rotation;
			Helpers::MakeTransformFromQuat(&socket.Transforms[0].QRotation, &rotation);
			candidate.cookedFireRotation = rotation.rotation;

#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Position = " << socket.Transforms[0].Position << std::endl;
			Logger::log << "[UpdateCache] Quaternion = " << socket.Transforms[0].QRotation << std::endl;
#endif
			break;
		}
	}
    candidate.valid = true;
    cachedViewModel = candidate;
    return true;
}

inline WeaponType WeaponHandler::GetWeaponType(Asset_Weapon* weapon) const
{
	WeaponType foundType = WeaponType::Unknown;
	if (strstr(weapon->WeaponAsset, "\\plasma pistol\\"))
	{
		foundType = WeaponType::PlasmaPistol;
	}
	else if (strstr(weapon->WeaponAsset, "\\sniper rifle\\"))
	{
		foundType = WeaponType::Sniper;
	}
	else if (strstr(weapon->WeaponAsset, "\\pistol\\"))
	{
		foundType = WeaponType::Pistol;
	}
	else if (strstr(weapon->WeaponAsset, "\\plasma rifle\\"))
	{
		foundType = WeaponType::PlasmaRifle;
	}
	else if (strstr(weapon->WeaponAsset, "\\shotgun\\"))
	{
		foundType = WeaponType::Shotgun;
	}
	else if (strstr(weapon->WeaponAsset, "\\assault rifle\\"))
	{
		foundType = WeaponType::AssaultRifle;
	}
	else if (strstr(weapon->WeaponAsset, "\\rocket launcher\\"))
	{
		foundType = WeaponType::RocketLauncher;
	}
	else if (strstr(weapon->WeaponAsset, "\\flamethrower\\"))
	{
		foundType = WeaponType::Flamethrower;
	}
	else if (strstr(weapon->WeaponAsset, "\\plasma_cannon\\"))
	{
		foundType = WeaponType::PlasmaCannon;
	}
	else if (strstr(weapon->WeaponAsset, "\\needler\\"))
	{
		foundType = WeaponType::Needler;
	}
	else if (strstr(weapon->WeaponAsset, "\\fuel rod\\"))
	{
		foundType = WeaponType::FuelRod;
	}
	else
	{
		Logger::log << "[UpdateCache] Unknown weapon with asset " << weapon->WeaponAsset << std::endl;
	}

	return foundType;
}

inline void WeaponHandler::TransformToMatrix4(Transform& inTransform, Matrix4& outMatrix) const
{
	// Assumes scale of 1!
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			// Not sure why get is const, you can directly set the values with setrow/setcolumn anyway
			const_cast<float*>(outMatrix.get())[x + y * 4] = inTransform.rotation[x + y * 3];
		}
	}
	outMatrix.setColumn(3, inTransform.translation);
}

Vector3 WeaponHandler::GetScopeLocation(WeaponType type) const
{
	Vector3 Scale = Game::instance.bLeftHanded ? Vector3(1.0f, -1.0f, 1.0f) : Vector3(1.0f, 1.0f, 1.0f);

	switch (type)
	{
	case WeaponType::RocketLauncher:
		return Game::instance.c_ScopeOffsetRocket->Value() * Scale;
	case WeaponType::Sniper:
		return Game::instance.c_ScopeOffsetSniper->Value() * Scale;
	case WeaponType::Unknown:
	case WeaponType::Pistol:
		return Game::instance.c_ScopeOffsetPistol->Value() * Scale;
	default:
		return Vector3(0.0f, 0.0f, 0.0f);
	}
}

namespace {
double RecoilTime() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
Immersion::Object RecoilObject(const HaloID& id) {
    return static_cast<Immersion::Object>(id.index) | (static_cast<Immersion::Object>(id.id)<<16);
}
Immersion::RecoilProfile WeaponRecoilProfile(WeaponType type, short trigger) {
    // Degrees, backward metres, exponential recovery seconds, pitch limit.
    switch (type) {
    case WeaponType::Pistol: return {3.4f,0.025f,0.20f,12.0f};
    case WeaponType::AssaultRifle: return {1.5f,0.014f,0.24f,14.0f};
    case WeaponType::Shotgun: return {7.0f,0.055f,0.25f,15.0f};
    case WeaponType::RocketLauncher: return {5.0f,0.045f,0.28f,14.0f};
    case WeaponType::Sniper: return {5.5f,0.040f,0.23f,14.0f};
    case WeaponType::Flamethrower: return {0.45f,0.005f,0.18f,8.0f};
    case WeaponType::PlasmaPistol: return trigger == 1 ? Immersion::RecoilProfile{3.0f,0.025f,0.22f,10.0f} : Immersion::RecoilProfile{1.2f,0.012f,0.20f,10.0f};
    case WeaponType::PlasmaRifle: return {1.1f,0.012f,0.22f,12.0f};
    case WeaponType::PlasmaCannon: return {1.5f,0.015f,0.22f,12.0f};
    case WeaponType::Needler: return {0.9f,0.009f,0.21f,10.0f};
    case WeaponType::FuelRod: return {4.5f,0.040f,0.25f,14.0f};
    default: return {2.0f,0.020f,0.20f,12.0f};
    }
}
}

void WeaponHandler::UpdateRecoil()
{
    Immersion::FireState state; bool supported=false;
    auto& game=Game::instance;
    if (!cachedViewModel.valid || game.bUse3DOFAiming || game.bNeedsRecentre ||
        Helpers::IsMouseVisible() || Helpers::IsCampaignLoading() || Helpers::GetCutsceneData()->bInCutscene ||
        !game.c_RecoilScale || game.c_RecoilScale->Value() <= 0 ||
        !game.PhysicalRecoilState(cachedViewModel.currentWeapon,state,supported)) {
        recoil.Reset(); return;
    }
    recoil.Context(state.unit,state.weapon);
    recoil.Advance(RecoilTime());
}

void WeaponHandler::BeginRecoilShot(const HaloID& weapon)
{
    recoilPending=false;
    UpdateRecoil();
    auto& game=Game::instance;
    if (!cachedViewModel.valid || RecoilObject(weapon) != RecoilObject(cachedViewModel.currentWeapon) ||
        game.bUse3DOFAiming || game.bNeedsRecentre || Helpers::IsMouseVisible() ||
        Helpers::IsCampaignLoading() || Helpers::GetCutsceneData()->bInCutscene ||
        !game.c_RecoilScale || game.c_RecoilScale->Value() <= 0) return;
    recoilPending=game.PhysicalRecoilState(weapon,recoilBefore,recoilSupported);
}

void WeaponHandler::EndRecoilShot(const HaloID& weapon, short trigger)
{
    const bool pending=recoilPending; recoilPending=false;
    if (!pending || RecoilObject(weapon)!=recoilBefore.weapon) return;
    Immersion::FireState after; bool supported=false;
    auto& game=Game::instance;
    if (!game.PhysicalRecoilState(weapon,after,supported) || !Immersion::FiredBetween(recoilBefore,after)) return;
    recoil.Context(after.unit,after.weapon);
    const unsigned type=static_cast<unsigned>(cachedViewModel.weaponType);
    float perWeapon=1.0f;
    if (type < game.c_WeaponRecoilScales.size() && game.c_WeaponRecoilScales[type])
        perWeapon=game.c_WeaponRecoilScales[type]->Value();
    if (!std::isfinite(perWeapon)) perWeapon=1.0f;
    perWeapon=(std::max)(0.0f,(std::min)(3.0f,perWeapon));
    auto profile=WeaponRecoilProfile(cachedViewModel.weaponType,trigger);
    profile.pitchDegrees*=perWeapon; profile.backMetres*=perWeapon;
    recoil.Shot(profile,recoilSupported,game.c_RecoilScale->Value(),
        game.c_SupportedRecoilFraction->Value(),RecoilTime());
}

Matrix4 WeaponHandler::RecoilKick(float metresToWorld) const
{
    const auto pose=recoil.Pose(RecoilObject(cachedViewModel.currentWeapon));
    Matrix4 kick;
    kick.rotateY(-pose.pitch); // Halo-local +X muzzle rises toward +Z.
    kick.translate(-pose.back*metresToWorld,0,0);
    return kick;
}

void WeaponHandler::RemoveRecoilFromCache(Matrix4& hand, Vector3& gunPosition, Matrix3& gunRotation) const
{
    const auto pose=recoil.Pose(RecoilObject(cachedViewModel.currentWeapon));
    if (pose.pitch == 0 && pose.back == 0) return;
    Matrix4 inverseKick=RecoilKick(Game::instance.MetresToWorld(1.0f));
    inverseKick.invertAffine();
    Matrix4 inverseHand=hand; inverseHand.invertAffine();
    const Matrix4 original=hand*inverseKick;
    const Matrix4 undo=original*inverseHand;
    gunPosition=undo*gunPosition;
    Matrix3 rotation;
    for (int i=0;i<3;++i) rotation.setColumn(i,&undo.get()[i*4]);
    gunRotation=rotation*gunRotation;
    hand=original;
}

void WeaponHandler::CacheFireAlignment(Matrix4 hand, Vector3 gunPosition, Matrix3 gunRotation)
{
    // Cache the muzzle relative to the firing hand, never relative to world
    // axes. A world-space conjugate feeds turning back into the next two-hand
    // correction (especially on the launcher's rotated gun/trigger bones).
    RemoveRecoilFromCache(hand,gunPosition,gunRotation);
    const Vector3 handPosition=hand*Vector3();
    Matrix3 inverseHand;
    for(int i=0;i<3;++i)inverseHand.setColumn(i,&hand.get()[i*4]);
    inverseHand.invert();
    cachedViewModel.gunOffset=inverseHand*(gunPosition-handPosition);
    cachedViewModel.fireOffset=inverseHand*(gunPosition-handPosition+gunRotation*cachedViewModel.cookedFireOffset);
    cachedViewModel.fireRotation=inverseHand*gunRotation*cachedViewModel.cookedFireRotation;
}

Matrix4 WeaponHandler::GetDominantHandTransform() const
{
    return GetUnrecoiledHandTransform()*RecoilKick();
}

Matrix4 WeaponHandler::GetUnrecoiledHandTransform() const
{
	Matrix4 controllerTransform;
	if (Game::instance.GetPhysicalSupportTransform(controllerTransform)) return controllerTransform;
	Vector3 actualControllerPos;
	Vector3 toOffHand;
	Vector3 smoothedPosition; 

	if (!Game::instance.GetCalculatedHandPositions(controllerTransform, actualControllerPos, toOffHand))
	{
		return controllerTransform;
	}

    smoothedPosition = Game::instance.GetSmoothedInput();
    Vector3 direction=smoothedPosition-actualControllerPos;
    if(!std::isfinite(direction.lengthSqr()) || direction.lengthSqr()<1e-8f) {
        // First acquisition/stale smoothing must not collapse the look-at basis.
        smoothedPosition=actualControllerPos+toOffHand;
        direction=toOffHand;
    }
    if(!std::isfinite(direction.lengthSqr()) || direction.lengthSqr()<1e-8f)return controllerTransform;
    direction.normalize();
    Vector3 upVector=controllerTransform.getForwardAxis();
    if(upVector.cross(direction).lengthSqr()<1e-4f) {
        upVector=controllerTransform.getUpAxis();
    }
	controllerTransform.lookAt(smoothedPosition, upVector);

	controllerTransform.translate(-actualControllerPos);
	controllerTransform.rotate(-90.0f, controllerTransform.getUpAxis());
	controllerTransform.rotate(-90.0f, controllerTransform.getLeftAxis());
	controllerTransform.translate(actualControllerPos);

	// Apply offset from weapon aiming here
	Matrix4 cachedRot4;

	for (int i = 0; i < 3; i++)
	{
		cachedRot4.setColumn(i, cachedViewModel.fireRotation.getColumn(i));
	}

	controllerTransform *= cachedRot4.invertAffine();
	return controllerTransform;
}

bool WeaponHandler::GetLocalWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
    if (!cachedViewModel.valid || Game::instance.PhysicalWeaponsStowed()) return false;
	HaloID playerID;
	if (!Helpers::GetLocalPlayerID(playerID))
	{
		return false;
	}

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(playerID));
	if (!player)
	{
		return false;
	}

	// TODO: Handedness
	Matrix4 controllerPos = GetDominantHandTransform();

	Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 handRotation = controllerPos.translate(-handPos);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &handRotation.get()[i * 4]);
	}

	Matrix3 finalRot;

	if (Game::instance.bUse3DOFAiming)
	{
		// 3DOF MODE: Use pure controller rotation (matching bullet direction in RelocatePlayer)
		// fireRotation is NOT updated in 3DOF mode, so skip it
		finalRot = handRotation3;

		// Position from HMD instead of controller
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		outPosition = hmdPos;
	}
	else
	{
		// Compose the hand-local muzzle rotation into the tracked hand frame.
		finalRot = handRotation3 * cachedViewModel.fireRotation;
		outPosition = handPos + handRotation * cachedViewModel.fireOffset * Game::instance.WorldToMetres(1.0f);
	}

	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);
	upDir = finalRot * Vector3(0.0f, 0.0f, 1.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_CrosshairDistance->Value());
	Game::instance.inGameRenderer.DrawLine3D(worldOutPos, aimTarget, D3DCOLOR_ARGB(255, 255, 20, 20));

	Game::instance.inGameRenderer.DrawLine3D(lastFireLocation, lastFireAim, D3DCOLOR_ARGB(255, 20, 255, 255));
#endif

	return true;
}

bool WeaponHandler::GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	bool bSuccess = GetLocalWeaponAim(outPosition, outAim, upDir);

	outPosition = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);

	return bSuccess;
}


bool WeaponHandler::GetLocalWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
    if (!cachedViewModel.valid || Game::instance.PhysicalWeaponsStowed()) return false;
	HaloID playerID;
	if (!Helpers::GetLocalPlayerID(playerID))
	{
		return false;
	}

	UnitDynamicObject* player = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(playerID));
	if (!player)
	{
		return false;
	}

	// TODO: Handedness
	Matrix4 controllerPos = GetDominantHandTransform();

	Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
	Matrix4 handRotation = controllerPos.translate(-handPos);

	Matrix3 handRotation3;

	for (int i = 0; i < 3; i++)
	{
		handRotation3.setColumn(i, &handRotation.get()[i * 4]);
	}

	Matrix3 finalRot;

	if (Game::instance.bUse3DOFAiming)
	{
		// 3DOF MODE: Use pure controller rotation (matching bullet direction in RelocatePlayer)
		// fireRotation is NOT updated in 3DOF mode, so skip it
		finalRot = handRotation3;

		// Position from HMD instead of controller
		Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
		Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);
		outPosition = hmdPos;
	}
	else
	{
		// Scope and shots share the same hand-local muzzle correction.
		finalRot = handRotation3 * cachedViewModel.fireRotation;

		Vector3 scopeOffset = GetScopeLocation(cachedViewModel.weaponType);
		Vector3 gunOffset = handPos + handRotation * cachedViewModel.gunOffset * Game::instance.WorldToMetres(1.0f);
		outPosition = gunOffset + finalRot * scopeOffset;
	}

	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);
	upDir = finalRot * Vector3(1.0f, 0.0f, 1.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_CrosshairDistance->Value());
	Game::instance.inGameRenderer.DrawLine3D(worldOutPos, aimTarget, D3DCOLOR_ARGB(255, 255, 20, 20));
#endif

	return true;
}

bool WeaponHandler::GetWorldWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
	bool bSuccess = GetLocalWeaponScope(outPosition, outAim, upDir);

	outPosition = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);

	return bSuccess;
}

bool WeaponHandler::IsSniperScope() const
{
	return cachedViewModel.weaponType == WeaponType::Sniper;
}

void WeaponHandler::RelocatePlayer(HaloID& PlayerID)
{
	// Teleport the player to the controller position so the bullet comes from there instead
	weaponFiredPlayer = static_cast<UnitDynamicObject*>(Helpers::GetDynamicObject(PlayerID));
	if (weaponFiredPlayer)
	{
		// TODO: Handedness
		Matrix4 controllerPos = GetDominantHandTransform();

		// Apply scale only to translation portion
		Vector3 translation = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
		controllerPos.translate(-translation);
		translation *= Game::instance.MetresToWorld(1.0f);
		translation += weaponFiredPlayer->position;

		controllerPos.translate(translation);

		Vector3 handPos = controllerPos * Vector3(0.0f, 0.0f, 0.0f);
		Matrix4 handRotation = controllerPos.translate(-handPos);

		Matrix3 handRotation3;

		for (int i = 0; i < 3; i++)
		{
			handRotation3.setColumn(i, &handRotation.get()[i * 4]);
		}

		// Cache the real values so we can restore them after running the original fire function
		realPlayerPosition = weaponFiredPlayer->position;
		realPlayerAim = weaponFiredPlayer->aim;

		if (Game::instance.bUse3DOFAiming)
		{
			// 3DOF MODE: Bullets originate from HMD (user's eyes)
			// Get HMD position in VR space
			Matrix4 hmdTransform = Game::instance.GetVR()->GetHMDTransform(true);
			Vector3 hmdPos = hmdTransform * Vector3(0.0f, 0.0f, 0.0f);

			// Convert to game world units and add to player base position
			Vector3 hmdPosWorld = hmdPos * Game::instance.MetresToWorld(1.0f);
			weaponFiredPlayer->position = realPlayerPosition + hmdPosWorld;

			// Aim direction uses controller rotation directly
			// Note: fireRotation is NOT updated in 3DOF mode (skipped in UpdateViewModel),
			// so we use handRotation3 directly for aim direction
			weaponFiredPlayer->aim = handRotation3 * Vector3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			// 6DOF MODE: Bullets originate from hand/gun barrel (existing behavior)
			weaponFiredPlayer->position = handPos + handRotation * cachedViewModel.fireOffset;
			weaponFiredPlayer->aim = (handRotation3 * cachedViewModel.fireRotation) * Vector3(1.0f, 0.0f, 0.0f);
		}

#if DRAW_DEBUG_AIM
		Vector3 internalFireOffset = Helpers::GetCamera().position - realPlayerPosition;
		lastFireLocation = weaponFiredPlayer->position + internalFireOffset;
		lastFireAim = lastFireLocation + weaponFiredPlayer->aim * 1.0f;
		Logger::log << "FireOffset: " << cachedViewModel.fireOffset << std::endl;
		Logger::log << "FireAim: " << cachedViewModel.fireRotation << std::endl;
		Logger::log << "Player Position: " << realPlayerPosition << std::endl;
		Logger::log << "Last fire location: " << lastFireLocation << std::endl;
#endif
	}
}

void WeaponHandler::PreFireWeapon(HaloID& WeaponID, short param2)
{
    BeginRecoilShot(WeaponID); // Sample BEFORE relocating the native muzzle.
	BaseDynamicObject* Object = Helpers::GetDynamicObject(WeaponID);

	weaponFiredPlayer = nullptr;

	// Check if the weapon is being used by the player
	HaloID PlayerID;
	bool foundPlayer = Helpers::GetLocalPlayerID(PlayerID);
	if (Object && foundPlayer && PlayerID == Object->parent)
	{
		HandleWeaponHaptics();
		Game::instance.weaponHapticsConfig.WeaponFired(cachedViewModel.weaponType);
		RelocatePlayer(PlayerID);
	}
}

void WeaponHandler::SetPlasmaPistolCharge()
{
	HaloID PlayerID;
	bool foundPlayer = Helpers::GetLocalPlayerID(PlayerID);

	if (foundPlayer && cachedViewModel.weaponType == WeaponType::PlasmaPistol)
	{
		Game::instance.weaponHapticsConfig.SetPlasmaPistolCharging();
	}
}

void WeaponHandler::HandlePlasmaPistolCharge()
{
	IVR* vr = Game::instance.GetVR();

	if (vr && cachedViewModel.weaponType == WeaponType::PlasmaPistol)
	{
		bool isCharging = Game::instance.weaponHapticsConfig.IsPlasmaPistolCharging();

		if (isCharging)
		{
			HandleWeaponHaptics();
		}
	}
}

inline void WeaponHandler::HandleWeaponHaptics() const
{
	IVR* vr = Game::instance.GetVR();
	
	if (cachedViewModel.weaponType != WeaponType::Unknown)
	{
		WeaponHapticsConfigManager hapticsManager = Game::instance.weaponHapticsConfig;
		WeaponHaptic haptic = hapticsManager.GetWeaponHaptics(cachedViewModel.weaponType);

#if HAPTICS_DEBUG
		Logger::log << "[Weapon Haptics] triggering haptics for weapon " << haptic.Description << std::endl;
#endif

		ControllerRole dominantHand = ControllerRole::Right;
		ControllerRole nondominantHand = ControllerRole::Left;
		
		if (Game::instance.bLeftHanded)
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Left is hand dominant hand. " << haptic.Description << std::endl;
#endif

			dominantHand = ControllerRole::Left;
			nondominantHand = ControllerRole::Right;
		}
		else
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Right is hand dominant hand. " << haptic.Description << std::endl;
#endif
		}

		if (Game::instance.bUseTwoHandAim || Game::instance.bCompactSupportGrip)
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Gun is in two handed mode. " << haptic.Description << std::endl;
#endif
			WeaponHapticArg dominantHaptics = haptic.TwoHand.Dominant;
			WeaponHapticArg nondominantHaptics = haptic.TwoHand.Nondominant;
			hapticsManager.HandleWeaponHaptics(vr, dominantHand, dominantHaptics);
			hapticsManager.HandleWeaponHaptics(vr, nondominantHand, nondominantHaptics);
		}
		else 
		{
#if HAPTICS_DEBUG
			Logger::log << "[Weapon Haptics] Gun is in one handed mode. " << haptic.Description << std::endl;
#endif
			WeaponHapticArg hapticArgs = haptic.OneHand;
			hapticsManager.HandleWeaponHaptics(vr, dominantHand, hapticArgs);
		}
	}
	else
	{
		Logger::log << "[Weapon Haptics] Attempted to find gun haptics for weapon type " << static_cast<int>(cachedViewModel.weaponType) << " but vrinput was null" << std::endl;
	}
}

void WeaponHandler::PostFireWeapon(HaloID& weaponID, short param2)
{
	// Restore state after firing the weapon
	if (weaponFiredPlayer)
	{
		weaponFiredPlayer->position = realPlayerPosition;
		weaponFiredPlayer->aim = realPlayerAim;
		weaponFiredPlayer = nullptr;
	}
    EndRecoilShot(weaponID,param2); // Kick only after the native firing attempt.
}

void WeaponHandler::PreThrowGrenade(HaloID& playerID)
{
	weaponFiredPlayer = nullptr;

	// Check if the weapon is being used by the player
	HaloID PlayerID;
	if (Helpers::GetLocalPlayerID(PlayerID) && PlayerID == playerID)
	{
		RelocatePlayer(PlayerID);
	}
}

void WeaponHandler::PostThrowGrenade(HaloID& playerID)
{
	if (weaponFiredPlayer)
	{
		weaponFiredPlayer->position = realPlayerPosition;
		weaponFiredPlayer->aim = realPlayerAim;
		weaponFiredPlayer = nullptr;
	}
}
