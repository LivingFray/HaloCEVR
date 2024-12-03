#include "WeaponHandler.h"
#include "Helpers/Objects.h"
#include "Helpers/Camera.h"
#include "Helpers/Assets.h"
#include "Helpers/Maths.h"
#include "Logger.h"
#include "Game.h"

// This is a working decomp of the game's original logic for updating the view model's skeleton
// Only kept here for reference when working on the replacement function below
static void ReferenceUpdateViewModelImpl(HaloID& id, Vector3* pos, Vector3* facing, Vector3* up, TransformQuat* boneTransforms, Transform* outBoneTransforms)
{
	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	AssetData_ModelAnimations* animationData = viewModel->Data;
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
	Vector3& camPos = Helpers::GetCamera().position;

	pos->x = camPos.x;
	pos->y = camPos.y;
	pos->z = camPos.z;

	facing->x = 1.0f;
	facing->y = 0.0f;
	facing->z = 0.0f;

	up->x = 0.0f;
	up->y = 0.0f;
	up->z = 1.0f;

	Asset_ModelAnimations* viewModel = Helpers::GetTypedAsset<Asset_ModelAnimations>(id);
	if (!viewModel)
	{
		Logger::log << "[UpdateViewModel] Can't get view model asset" << std::endl;
		return;
	}

	AssetData_ModelAnimations* animationData = viewModel->Data;
	Bone* boneArray = animationData->BoneArray;

	Transform root;
	Helpers::MakeTransformFromXZ(up, facing, &root);
	root.translation = *pos;

	const bool bShouldUpdateCache = cachedViewModel.currentAsset != id;
	if (bShouldUpdateCache)
	{
		UpdateCache(id, animationData);
	}

	Matrix4 handTransform;
	CalculateHandTransform(pos, handTransform);

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

			if (boneIndex == cachedViewModel.displayIndex && Game::instance.c_LeftHanded->Value())
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
			if (currentBone.Parent == 0 || boneArray[currentBone.Parent].Parent == 0)
			{
				// Hide arms/root
				outBoneTransforms[boneIndex].scale = 0.0f;
			}
			else if (boneIndex == cachedViewModel.rightWristIndex)
			{
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

							if (Game::instance.c_LeftHanded->Value())
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
			else if (boneIndex == cachedViewModel.leftWristIndex)
			{
				if (!Game::instance.bUseTwoHandAim)
				{
					Matrix4 newTransform = Game::instance.GetVR()->GetControllerTransform(Game::instance.c_LeftHanded->Value() ? ControllerRole::Right : ControllerRole::Left, true);
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

					if (Game::instance.c_LeftHanded->Value())
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
			else if (boneIndex == cachedViewModel.gunIndex)
			{
				Vector3& gunPos = outBoneTransforms[boneIndex].translation;
				Matrix3 gunRot = outBoneTransforms[boneIndex].rotation;

				if (Game::instance.c_LeftHanded->Value())
				{
					gunRot = gunRot * Matrix3(
						1.0f, 0.0f, 0.0f,
						0.0f, -1.0f, 0.0f,
						0.0f, 0.0f, 1.0f
					);
				}

				Vector3 handPos = handTransform * Vector3(0.0f, 0.0f, 0.0f);
				Matrix4 handRotation = handTransform.translate(-handPos);
				Matrix3 handRotation3;

				for (int i = 0; i < 3; i++)
				{
					handRotation3.setColumn(i, &handRotation.get()[i * 4]);
				}

				Matrix3 inverseHand = handRotation3;
				inverseHand.invert();

				cachedViewModel.fireOffset = (gunPos - handPos) + (gunRot * cachedViewModel.cookedFireOffset);
				cachedViewModel.fireOffset = inverseHand * cachedViewModel.fireOffset;

				cachedViewModel.gunOffset = (gunPos - handPos);
				cachedViewModel.gunOffset = inverseHand * cachedViewModel.gunOffset;

				cachedViewModel.fireRotation = cachedViewModel.cookedFireRotation * gunRot * inverseHand;
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
	Matrix4 newRotation4 = Game::instance.c_LeftHanded->Value() ? newTransform * Matrix4().scale(1.0f, -1.0f, 1.0f) : newTransform;
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

void WeaponHandler::UpdateCache(HaloID& id, AssetData_ModelAnimations* animationData)
{
#if DRAW_DEBUG_AIM
	Logger::log << "[UpdateCache] Swapped weapons, recaching " << id << std::endl;
#endif
	cachedViewModel.currentAsset = id;
	cachedViewModel.leftWristIndex = -1;
	cachedViewModel.rightWristIndex = -1;
	cachedViewModel.gunIndex = -1;
	cachedViewModel.displayIndex = -1;

	Bone* boneArray = animationData->BoneArray;

	for (int i = 0; i < animationData->NumBones; i++)
	{
		Bone& CurrentBone = boneArray[i];

		if (cachedViewModel.leftWristIndex == -1 && strstr(CurrentBone.BoneName, "l wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Left Wrist @ " << i << std::endl;
#endif
			cachedViewModel.leftWristIndex = i;
		}
		else if (cachedViewModel.rightWristIndex == -1 && strstr(CurrentBone.BoneName, "r wrist"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Right Wrist @ " << i << std::endl;
#endif
			cachedViewModel.rightWristIndex = i;
		}
		else if (cachedViewModel.gunIndex == -1 && strstr(CurrentBone.BoneName, "gun"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			cachedViewModel.gunIndex = i;
		}
		else if (cachedViewModel.gunIndex == -1 && strstr(CurrentBone.BoneName, "body"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Gun @ " << i << std::endl;
#endif
			cachedViewModel.gunIndex = i;
		}
		else if (cachedViewModel.displayIndex == -1 && strstr(CurrentBone.BoneName, "display"))
		{
#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Found Display @ " << i << std::endl;
#endif
			cachedViewModel.displayIndex = i;
		}
#if DRAW_DEBUG_AIM
		else
		{
			Logger::log << "[UpdateCache] Skipped Bone " << CurrentBone.BoneName << std::endl;
		}
#endif
	}

	cachedViewModel.fireOffset = Vector3();
	cachedViewModel.cookedFireOffset = Vector3();
	cachedViewModel.cookedFireRotation = Matrix3();
	cachedViewModel.gunOffset = Vector3();

	// weapon model can be found from this chain:
	// player->WeaponID (DynamicObject)->WeaponID (weapon Asset)->WeaponData->ViewModelID (GBX Asset)
	// The local offset of the fire VFX (i.e. end of the barrel) is found in the "primary trigger" socket

	BaseDynamicObject* player = Helpers::GetLocalPlayer();
	if (!player)
	{
		Logger::log << "[UpdateCache] Can't find local player" << std::endl;
		return;
	}

	BaseDynamicObject* weaponObj = Helpers::GetDynamicObject(player->weapon);
	if (!weaponObj)
	{
		Logger::log << "[UpdateCache] Can't find weapon from WeaponID " << player->weapon << std::endl;
		Logger::log << "[UpdateCache] Player Tag = " << player->tagID << std::endl;
		return;
	}

	Asset_Weapon* weapon = Helpers::GetTypedAsset<Asset_Weapon>(weaponObj->tagID);
	if (!weapon)
	{
		Logger::log << "[UpdateCache] Can't find weapon asset from TagID " << weaponObj->tagID << std::endl;
		return;
	}

	if (!weapon->WeaponData)
	{
		Logger::log << "[UpdateCache] Can't find weapon data in weapon asset " << weaponObj->tagID << std::endl;
		Logger::log << "[UpdateCache] Weapon Type = " << weapon->GroupID << std::endl;
		Logger::log << "[UpdateCache] Weapon Path = " << weapon->WeaponAsset << std::endl;
		return;
	}


	Asset_GBXModel* model = Helpers::GetTypedAsset<Asset_GBXModel>(weapon->WeaponData->ViewModelID);
	if (!model)
	{
		Logger::log << "[UpdateCache] Can't find GBX model from ViewModelID = " << weapon->WeaponData->ViewModelID << std::endl;
		return;
	}

#if DRAW_DEBUG_AIM
	std::string reversedGroup;
	reversedGroup.assign(model->GroupID, 4);
	std::reverse(reversedGroup.begin(), reversedGroup.end());
	Logger::log << "[UpdateCache] GBXModelTag = " << reversedGroup << std::endl;
	Logger::log << "[UpdateCache] GBXModelPath = " << model->ModelPath << std::endl;
#endif

	if (strstr(model->ModelPath, "\\pistol\\"))
	{
		cachedViewModel.scopeType = ScopedWeaponType::Pistol;
	}
	else if (strstr(model->ModelPath, "\\sniper rifle\\"))
	{
		cachedViewModel.scopeType = ScopedWeaponType::Sniper;
	}
	else if (strstr(model->ModelPath, "\\rocket launcher\\"))
	{
		cachedViewModel.scopeType = ScopedWeaponType::Rocket;
	}
	else
	{
		cachedViewModel.scopeType = ScopedWeaponType::Unknown;
	}

	if (!model->ModelData)
	{
		return;
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

			if (socket.NumTransforms == 0)
			{
				Logger::log << "[UpdateCache] " << socket.SocketName << " has no transforms" << std::endl;
				break;
			}

			cachedViewModel.cookedFireOffset = socket.Transforms[0].Position;
			Transform rotation;
			Helpers::MakeTransformFromQuat(&socket.Transforms[0].QRotation, &rotation);
			cachedViewModel.cookedFireRotation = rotation.rotation;

#if DRAW_DEBUG_AIM
			Logger::log << "[UpdateCache] Position = " << socket.Transforms[0].Position << std::endl;
			Logger::log << "[UpdateCache] Quaternion = " << socket.Transforms[0].QRotation << std::endl;
#endif
			break;
		}
	}
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

Vector3 WeaponHandler::GetScopeLocation(ScopedWeaponType type) const
{
	Vector3 Scale = Game::instance.c_LeftHanded->Value() ? Vector3(1.0f, -1.0f, 1.0f) : Vector3(1.0f, 1.0f, 1.0f);

	switch (type)
	{
	case ScopedWeaponType::Rocket:
		return Game::instance.c_ScopeOffsetRocket->Value() * Scale;
	case ScopedWeaponType::Sniper:
		return Game::instance.c_ScopeOffsetSniper->Value() * Scale;
	case ScopedWeaponType::Unknown:
	case ScopedWeaponType::Pistol:
		return Game::instance.c_ScopeOffsetPistol->Value() * Scale;
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

Matrix4 WeaponHandler::GetDominantHandTransform() const
{
	ControllerRole dominant = Game::instance.c_LeftHanded->Value() ? ControllerRole::Left : ControllerRole::Right;
	ControllerRole nonDominant = Game::instance.c_LeftHanded->Value() ? ControllerRole::Right : ControllerRole::Left;

	Matrix4 controllerTransform = Game::instance.GetVR()->GetControllerTransform(dominant, true);
	static Vector3 smoothedPosition = controllerTransform * Vector3(0.0f, 0.0f, 0.0f);

	Vector3 poseDirection;
	bool bHasPoseData = Game::instance.GetVR()->TryGetControllerFacing(dominant, poseDirection);

	// When 2h aiming point the main hand at the offhand 
	if (Game::instance.bUseTwoHandAim || bHasPoseData)
	{
		Matrix4 aimingTransform = Game::instance.GetVR()->GetRawControllerTransform(dominant, true);
		Matrix4 offHandTransform = Game::instance.GetVR()->GetRawControllerTransform(nonDominant, true);

		const Vector3 actualControllerPos = controllerTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 mainHandPos = aimingTransform * Vector3(0.0f, 0.0f, 0.0f);
		const Vector3 offHandPos = Game::instance.bUseTwoHandAim ? offHandTransform * Vector3(0.0f, 0.0f, 0.0f) : mainHandPos + poseDirection;
		const Vector3 toOffHand = (offHandPos - mainHandPos).normalize();

		Vector3 upVector = controllerTransform.getForwardAxis();

		// In the unlikely event the player decides to put their hand directly above their other hand, avoid a DIV/0 error when doing the lookat
		if (upVector.dot(toOffHand) == 1.0f)
		{
			upVector += controllerTransform.getUpAxis() * 0.001f;
		}

		/*
		Matrix3 rot;
		for (int i = 0; i < 3; i++)
		{
			offHandTransform.setColumn(i, &rot.get()[i * 4]);
		}
		*/

		float userInput = 0.0f;
		short zoom = Helpers::GetInputData().zoomLevel; 
		
		if (zoom == -1)
		{
			userInput = Game::instance.c_WeaponSmoothingAmountNoZoom->Value(); 
		}
		else if (zoom == 0)
		{
			userInput = Game::instance.c_WeaponSmoothingAmountOneZoom->Value();
		}
		else if (zoom == 1)
		{
			userInput = Game::instance.c_WeaponSmoothingAmountTwoZoom->Value();
		}

		float clampedValue = std::clamp(userInput, 0.0f, 1.0f);

		float maxSmoothing = 20.0f;		//20 is already a bit ridiculous but just incase people need that much smoothing. 
		float speedRampup = 10.0f;		//This helps control the slowdown curve of the interpolation

		float t = (clampedValue * maxSmoothing) * Game::instance.lastDeltaTime;

		// Apply the smoothing using linear interpolation with the adjusted deltaTime
		smoothedPosition = Helpers::Lerp(smoothedPosition, actualControllerPos + toOffHand, exp(-t * speedRampup));
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
	}

	return controllerTransform;
}

bool WeaponHandler::GetLocalWeaponAim(Vector3& outPosition, Vector3& outAim) const
{
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

	Matrix3 finalRot = cachedViewModel.fireRotation * handRotation3;

	outPosition = handPos + handRotation * cachedViewModel.fireOffset * Game::instance.WorldToMetres(1.0f);
	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_UIOverlayDistance->Value());
	Game::instance.inGameRenderer.DrawLine3D(worldOutPos, aimTarget, D3DCOLOR_ARGB(255, 255, 20, 20));

	Game::instance.inGameRenderer.DrawLine3D(lastFireLocation, lastFireAim, D3DCOLOR_ARGB(255, 20, 255, 255));
#endif

	return true;
}

bool WeaponHandler::GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim) const
{
	bool bSuccess = GetLocalWeaponAim(outPosition, outAim);

	outPosition = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);

	return bSuccess;
}


bool WeaponHandler::GetLocalWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const
{
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

	Matrix3 finalRot = cachedViewModel.fireRotation * handRotation3;

	Vector3 scopeOffset = GetScopeLocation(cachedViewModel.scopeType);

	Vector3 gunOffset = handPos + handRotation * cachedViewModel.gunOffset * Game::instance.WorldToMetres(1.0f);

	outPosition = gunOffset + finalRot * scopeOffset;
	outAim = finalRot * Vector3(1.0f, 0.0f, 0.0f);
	upDir = finalRot * Vector3(0.0f, 0.0f, 1.0f);

#if DRAW_DEBUG_AIM
	// N.b. - This function is in local (i.e. vr) coordinate space, convert to world for debug to be correct
	Vector3 worldOutPos = Helpers::GetCamera().position + outPosition * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldOutPos, finalRot, 0.02f);
	Vector3 worldHandPos = Helpers::GetCamera().position + handPos * Game::instance.MetresToWorld(1.0f);
	Game::instance.inGameRenderer.DrawCoordinate(worldHandPos, handRotation3, 0.015f, false);

	Vector3 aimTarget = worldOutPos + outAim * Game::instance.MetresToWorld(Game::instance.c_UIOverlayDistance->Value());
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
	return cachedViewModel.scopeType == ScopedWeaponType::Sniper;
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
		// What are the other aims for??
		realPlayerAim = weaponFiredPlayer->aim;

		weaponFiredPlayer->position = handPos + handRotation * cachedViewModel.fireOffset;
		weaponFiredPlayer->aim = (cachedViewModel.fireRotation * handRotation3) * Vector3(1.0f, 0.0f, 0.0f);

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
	BaseDynamicObject* Object = Helpers::GetDynamicObject(WeaponID);

	weaponFiredPlayer = nullptr;

	// Check if the weapon is being used by the player
	HaloID PlayerID;
	if (Object && Helpers::GetLocalPlayerID(PlayerID) && PlayerID == Object->parent)
	{
		RelocatePlayer(PlayerID);
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
