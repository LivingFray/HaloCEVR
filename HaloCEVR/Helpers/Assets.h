#pragma once
#include "Objects.h"

struct Asset_Base
{
public:
	char GroupID[4];
};


struct Asset_Generic : public Asset_Base
{
	char pad[28];
};
static_assert(sizeof(Asset_Generic) == 0x20);


struct Sound
{
public:
	char* N00000551; //0x0000
}; //Size: 0x0004

struct Bone
{
public:
	char BoneName[32]; //0x0000
	int16_t LeftLeaf; //0x0020
	int16_t RightLeaf; //0x0022
	int16_t Parent; //0x0024
	char pad_0026[26]; //0x0026
}; //Size: 0x0040

struct Animation
{
public:
	char N00000429[32]; //0x0000
	char pad_0020[12]; //0x0020
	int32_t NumBones; //0x002C
	char pad_0030[20]; //0x0030
	float N00000433; //0x0044
	char pad_0048[108]; //0x0048
}; //Size: 0x00B4

struct AssetData_ModelAnimations
{
public:
	char pad_0000[76]; //0x0000
	void* N00000324; //0x004C
	char pad_0050[8]; //0x0050
	void* N00000327; //0x0058
	char pad_005C[12]; //0x005C
	int32_t NumBones; //0x0068
	Bone* BoneArray; //0x006C
	char pad_0070[4]; //0x0070
	int32_t NumAnimations; //0x0074
	Animation* AnimationArray; //0x0078
	char pad_007C[20]; //0x007C
	int32_t NumSounds; //0x0090
	Sound* Sounds; //0x0094
	char pad_0098[4]; //0x0098
}; //Size: 0x161C

struct Asset_ModelAnimations
{
public:
	char pad_0000[12]; //0x0000
	HaloID SelfID; //0x000C
	char* Shader; //0x0010
	AssetData_ModelAnimations* Data; //0x0014
	char pad_0018[8]; //0x0018
};

struct AssetData_Weapon
{
	char pad_0000[1128]; //0x0000
	HaloID ViewModelID; //0x0468
	char pad_046C[12]; //0x046C
	HaloID ViewAnimationID; //0x0478
	char pad_047C[136];
};

struct Asset_Weapon : public Asset_Base
{
	char N00000D51[4]; //0x0004
	char N00000D52[4]; //0x0008
	char pad_000C[4]; //0x000C
	char* WeaponAsset; //0x0010
	AssetData_Weapon* WeaponData; //0x0014
	char pad_0018[8]; //0x0018
};

struct GBXSocketTransform
{
	char pad_0000[4]; //0x0000
	Vector3 Position; //0x0004
	Vector4 QRotation; //0x0010
};

struct GBXSocket
{
	char SocketName[52]; //0x0000
	int32_t NumTransforms; //0x0034
	GBXSocketTransform* Transforms; //0x0038
	char pad_003C[4]; //0x003C
};

struct AssetData_GBXModel
{
	char pad_0000[172]; //0x0000
	int32_t NumSockets; //0x00AC
	GBXSocket* Sockets; //0x00B0
};

struct Asset_GBXModel : public Asset_Base
{
	char pad_0004[12]; //0x0004
	char* ModelPath; //0x0010
	AssetData_GBXModel* ModelData; //0x0014
	char pad_0018[8]; //0x0018
};

namespace Helpers
{
	Asset_Generic* GetAssetArray();

	template<typename T>
	T* GetTypedAsset(HaloID ID)
	{
		if (ID.id == 0xffff || ID.index == 0xffff)
		{
			return nullptr;
		}

		return reinterpret_cast<T*>(&GetAssetArray()[ID.index]);
	}
}