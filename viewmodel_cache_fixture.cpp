#include "HaloCEVR/WeaponHandler.h"
#include "HaloCEVR/Helpers/Assets.h"
#include "HaloCEVR/Helpers/Maths.h"
#include <cstring>
#include <sstream>
#include <iostream>
#include <cstdlib>
namespace Logger {std::ostringstream log;}
namespace Helpers {
    alignas(Asset_GBXModel) unsigned char tags[512]{};
    Asset_Generic* GetAssetArray(){return reinterpret_cast<Asset_Generic*>(tags);}
    void MakeTransformFromQuat(const Vector4*,Transform* out){*out={};out->rotation[0]=out->rotation[4]=out->rotation[8]=1;}
}
// INSERT_PRODUCTION_CACHE
struct Handler:WeaponHandler {
    using WeaponHandler::UpdateCache;
    using WeaponHandler::cachedViewModel;
};
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    CHECK(Helpers::GetTypedAsset<Asset_GBXModel>({0xffff,0xffff})==nullptr);
    Handler h;HaloID animation{1,42},weaponID{10,50};
    Bone bones[4]{};
    const char* names[]={"root","l wrist","r wrist","gun"};
    for(int i=0;i<4;++i) {
        std::strcpy(bones[i].BoneName,names[i]);
        bones[i].Parent=i-1;bones[i].LeftLeaf=i<3?i+1:-1;bones[i].RightLeaf=-1;
    }
    AssetData_ModelAnimations animations{};animations.NumBones=4;animations.BoneArray=bones;
    AssetData_Weapon data{};data.ViewModelID={2,100};
    Asset_Weapon weapon{};char name[]="weapons\\pistol\\pistol";
    weapon.WeaponAsset=name;weapon.WeaponData=&data;
    auto* model=Helpers::GetTypedAsset<Asset_GBXModel>(data.ViewModelID);
    AssetData_GBXModel modelData{};model->ModelData=&modelData;
    CHECK(!h.cachedViewModel.valid);
    CHECK(h.UpdateCache(animation,weaponID,&weapon,&animations));
    CHECK(h.cachedViewModel.valid && h.cachedViewModel.rightWristIndex==2);
    CHECK(h.cachedViewModel.currentAsset==animation && h.cachedViewModel.currentWeapon==weaponID);
    CHECK(h.cachedViewModel.weaponType==WeaponType::Pistol);
    HaloID next{3,44},otherWeapon{11,50};
    model->ModelData=nullptr;
    CHECK(!h.UpdateCache(next,otherWeapon,&weapon,&animations));
    CHECK(h.cachedViewModel.currentAsset==animation && h.cachedViewModel.currentWeapon==weaponID);
    model->ModelData=&modelData;
    data.ViewModelID={0xffff,0xffff};CHECK(!h.UpdateCache(next,otherWeapon,&weapon,&animations));
    data.ViewModelID={2,100};
    CHECK(h.UpdateCache(next,otherWeapon,&weapon,&animations));
    CHECK(h.cachedViewModel.currentAsset==next && h.cachedViewModel.currentWeapon==otherWeapon);
    CHECK(!h.UpdateCache(animation,weaponID,nullptr,&animations));
    bones[2].Parent=2;CHECK(!h.UpdateCache(animation,weaponID,&weapon,&animations));
    bones[2].Parent=1;bones[3].LeftLeaf=0;CHECK(!h.UpdateCache(animation,weaponID,&weapon,&animations));
    bones[3].LeftLeaf=-1;animations.NumBones=65;CHECK(!h.UpdateCache(animation,weaponID,&weapon,&animations));
    animations.NumBones=4;std::memset(bones[2].BoneName,'x',32);
    CHECK(!h.UpdateCache(animation,weaponID,&weapon,&animations));
    const auto datum=otherWeapon.index|(static_cast<Immersion::Object>(otherWeapon.id)<<16);
    for(const auto type:{WeaponType::RocketLauncher,WeaponType::Shotgun,WeaponType::AssaultRifle,WeaponType::Pistol}){
        h.cachedViewModel.valid=true;h.cachedViewModel.weaponType=type;
        CHECK(h.SupportGripAdvance(datum)==(type==WeaponType::RocketLauncher?.12f:type==WeaponType::Shotgun?.08f:0.f));
        CHECK(h.SupportGripLift(datum)==(type==WeaponType::RocketLauncher?.10f:0.f));
        CHECK(h.SupportGripLift(datum+1)==0.f);
        CHECK(h.SupportGripAdvance(datum+1)==0.f); // Never reuse the previous gun's zone after switching.
        h.cachedViewModel.valid=false;CHECK(h.SupportGripAdvance(datum)==0.f);
    }
    std::cout<<checks<<" production weapon-model cache checks passed\n";
}
