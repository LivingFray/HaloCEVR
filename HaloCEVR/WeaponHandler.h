#pragma once
#include "Maths/Vectors.h"
#include "Maths/Matrices.h"
#include "Helpers/Objects.h"
#include "../ThirdParty/nlohmann/json.hpp"
#define DRAW_DEBUG_AIM 0

enum class ScopedWeaponType
{
	Unknown,
	Pistol,
	Rocket,
	Sniper,
	Custom
};

class WeaponHandler
{
public:
	void LoadCustomWeapons();
	void UpdateViewModel(struct HaloID& id, struct Vector3* pos, struct Vector3* facing, struct Vector3* up, struct TransformQuat* boneTransforms, struct Transform* outBoneTransforms);
	void PreFireWeapon(HaloID& weaponID, short param2);
	void PostFireWeapon(HaloID& weaponID, short param2);
	void PreThrowGrenade(HaloID& playerID);
	void PostThrowGrenade(HaloID& playerID);

	bool GetLocalWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetLocalWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetWorldWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;

	bool IsSniperScope() const;

	Vector3 localOffset;
	Vector3 localRotation;

protected:
	void RelocatePlayer(HaloID& PlayerID);

	inline void CalculateBoneTransform(int boneIndex, struct Bone* boneArray, struct Transform& root, struct TransformQuat* boneTransforms, struct Transform& outTransform) const;
	inline void CalculateHandTransform(Vector3* pos, Matrix4& handTransform) const;
	inline void CreateEndCap(int boneIndex, const struct Bone& currentBone, struct Transform* outBoneTransforms) const;
	inline void MoveBoneToTransform(int boneIndex, const class Matrix4& newTransform, struct Transform* realTransforms, struct Transform* outBoneTransforms) const;
	inline void UpdateCache(struct HaloID& id, struct AssetData_ModelAnimations* animationData);

	inline void TransformToMatrix4(struct Transform& inTransform, class Matrix4& outMatrix) const;

	inline Vector3 GetScopeLocation(ScopedWeaponType Type) const;

	Matrix4 GetDominantHandTransform() const;

	struct ViewModelCache
	{
		HaloID currentAsset{ 0, 0 };
		int leftWristIndex = -1;
		int rightWristIndex = -1;
		int gunIndex = -1;
		int displayIndex = -1;
		int leftUpperArmIndex = -1;
		int rightUpperArmIndex = -1;
		Vector3 cookedFireOffset;
		Matrix3 cookedFireRotation;
		Vector3 fireOffset;
		Vector3 gunOffset;
		Matrix3 fireRotation;
		ScopedWeaponType scopeType = ScopedWeaponType::Unknown;
		int customId = -1;
	} cachedViewModel;

	struct WeaponData
	{
		// (Partial) name of the model associated with this weapon
		std::string modelName;
		// (Partial) name of the left wrist bone
		std::string leftWrist = "l wrist";
		// (Partial) name of the right wrist bone
		std::string rightWrist = "r wrist";
		// (Partial) name of the weapon's main bone
		std::string frame = "gun";
		// (Partial) name of the weapon's ammo display (if it has one), this bone will be mirrored when in the left hand
		std::string display = "display";
		// Offset to display the scope view at, relative to the controller
		Vector3 scopeOffset;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeaponData, modelName, leftWrist, rightWrist, frame, display, scopeOffset)
	};

	std::vector<WeaponData> customWeapons;

	UnitDynamicObject* weaponFiredPlayer = nullptr;
	Vector3 realPlayerPosition;
	Vector3 realPlayerAim;

	// Debug stuff for checking where bullets are coming from/going
#if DRAW_DEBUG_AIM
	mutable Vector3 lastFireLocation;
	mutable Vector3 lastFireAim;
#endif
};

