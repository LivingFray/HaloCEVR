#pragma once
#include "Maths/Vectors.h"
#include "Maths/Matrices.h"
#include "Helpers/Objects.h"
#include "Immersion/Recoil.h"

#define DRAW_DEBUG_AIM 0

enum class WeaponType
{
	Unknown,
	Pistol,
	AssaultRifle,
	Shotgun,
	RocketLauncher,
	Sniper,
	Flamethrower,
	PlasmaPistol,
	PlasmaRifle,
	PlasmaCannon,
	Needler,
	FuelRod
};

class WeaponHandler
{
public:
	void UpdateViewModel(struct HaloID& id, struct Vector3* pos, struct Vector3* facing, struct Vector3* up, struct TransformQuat* boneTransforms, struct Transform* outBoneTransforms);
	void HandlePlasmaPistolCharge();
	void SetPlasmaPistolCharge();
	void PreFireWeapon(HaloID& weaponID, short param2);
	void PostFireWeapon(HaloID& weaponID, short param2);
	void PreThrowGrenade(HaloID& playerID);
	void PostThrowGrenade(HaloID& playerID);

	bool GetLocalWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetWorldWeaponAim(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetLocalWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;
	bool GetWorldWeaponScope(Vector3& outPosition, Vector3& outAim, Vector3& upDir) const;

	bool IsSniperScope() const;
	Matrix4 GetDominantHandTransform() const;
    Matrix4 GetUnrecoiledHandTransform() const;
    void UpdateRecoil();
    bool UsesLongSupportGrip(Immersion::Object weapon) const {
        if (!cachedViewModel.valid || weapon != (cachedViewModel.currentWeapon.index |
            (static_cast<Immersion::Object>(cachedViewModel.currentWeapon.id) << 16))) return true;
        const auto type=cachedViewModel.weaponType;
        return type!=WeaponType::Pistol && type!=WeaponType::PlasmaPistol && type!=WeaponType::Needler && type!=WeaponType::PlasmaRifle;
    }

    float SupportGripAdvance(Immersion::Object weapon) const {
        if (!cachedViewModel.valid || weapon != (cachedViewModel.currentWeapon.index |
            (static_cast<Immersion::Object>(cachedViewModel.currentWeapon.id) << 16))) return 0.f;
        return cachedViewModel.weaponType==WeaponType::RocketLauncher?.12f:
            cachedViewModel.weaponType==WeaponType::Shotgun?.08f:0.f;
    }

    float SupportGripForgiveness(Immersion::Object weapon) const {
        return cachedViewModel.valid && weapon==(cachedViewModel.currentWeapon.index |
            (static_cast<Immersion::Object>(cachedViewModel.currentWeapon.id)<<16)) &&
            cachedViewModel.weaponType==WeaponType::AssaultRifle?.04f:0.f;
    }

    float SupportGripLift(Immersion::Object weapon) const {
        if (!cachedViewModel.valid || weapon != (cachedViewModel.currentWeapon.index |
            (static_cast<Immersion::Object>(cachedViewModel.currentWeapon.id) << 16))) return 0.f;
        return cachedViewModel.weaponType==WeaponType::RocketLauncher?.10f:0.f;
    }

	Vector3 localOffset;
	Vector3 localRotation;

protected:
    void BeginRecoilShot(const HaloID& weapon);
    void EndRecoilShot(const HaloID& weapon, short trigger);
    Matrix4 RecoilKick(float metresToWorld = 1.0f) const;
    void RemoveRecoilFromCache(Matrix4& hand, Vector3& gunPosition, Matrix3& gunRotation) const;
    void CacheFireAlignment(Matrix4 hand, Vector3 gunPosition, Matrix3 gunRotation);
    Immersion::Recoil recoil;
    Immersion::FireState recoilBefore;
    bool recoilPending=false, recoilSupported=false;
	void RelocatePlayer(HaloID& PlayerID);

	inline void CalculateBoneTransform(int boneIndex, struct Bone* boneArray, struct Transform& root, struct TransformQuat* boneTransforms, struct Transform& outTransform) const;
	inline void CalculateHandTransform(Vector3* pos, Matrix4& handTransform) const;
	inline void CreateEndCap(int boneIndex, const struct Bone& currentBone, struct Transform* outBoneTransforms) const;
	inline void MoveBoneToTransform(int boneIndex, const class Matrix4& newTransform, struct Transform* realTransforms, struct Transform* outBoneTransforms) const;
	inline bool UpdateCache(struct HaloID& id, struct HaloID& weaponID, struct Asset_Weapon* weapon, struct AssetData_ModelAnimations* animationData);
	inline WeaponType GetWeaponType(struct Asset_Weapon* weapon) const;
	inline void HandleWeaponHaptics() const;

	inline void TransformToMatrix4(struct Transform& inTransform, class Matrix4& outMatrix) const;

	inline Vector3 GetScopeLocation(WeaponType Type) const;


	struct ViewModelCache
	{
		HaloID currentAsset{ 0xffff, 0xffff };
        HaloID currentWeapon{ 0xffff, 0xffff };
        bool valid = false;
		int leftWristIndex = -1;
		int rightWristIndex = -1;
		int gunIndex = -1;
		int displayIndex = -1;
		Vector3 cookedFireOffset;
		Matrix3 cookedFireRotation;
		Vector3 fireOffset;
		Vector3 gunOffset;
		Matrix3 fireRotation; // Hand-local muzzle rotation; world = hand * fireRotation.
		WeaponType weaponType = WeaponType::Unknown;
		bool IsShooting = false;
		
	} cachedViewModel;

	UnitDynamicObject* weaponFiredPlayer = nullptr;
	Vector3 realPlayerPosition;
	Vector3 realPlayerAim;

	// Smoothed facing direction for 3DOF mode weapon model
	Vector3 smoothed3DOFFacingDir = Vector3(1.0f, 0.0f, 0.0f);
	bool bWasIn3DOFMode = false;

	// Track previous yaw offset to detect snap turns
	float lastYawOffset = 0.0f;

	// Debug stuff for checking where bullets are coming from/going
#if DRAW_DEBUG_AIM
	mutable Vector3 lastFireLocation;
	mutable Vector3 lastFireAim;
#endif
};

