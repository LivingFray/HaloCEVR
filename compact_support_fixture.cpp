#include "HaloCEVR/WeaponHandler.h"
#include "HaloCEVR/Immersion/HandGripPose.h"
#include <iostream>
#include <cstdlib>
using namespace Immersion;
struct Bone {char BoneName[32]{};int Parent=-1;};
enum class ControllerRole {Left,Right};
struct VR {
    Matrix4 hands[2];bool facingAvailable=false;Vector3 facing{1,0,0};
    Matrix4 GetControllerTransform(ControllerRole h,bool=true){return hands[unsigned(h)];}
    Matrix4 GetRawControllerTransform(ControllerRole h,bool=true){return hands[unsigned(h)];}
    bool TryGetControllerFacing(ControllerRole,Vector3& out){out=facing;return facingAvailable;}
};
struct Game {
    static Game instance;
    bool bCompactSupportGrip=true,bUseTwoHandAim=false,bUse3DOFAiming=false,bLeftHanded=false;
    VR vr;VR* GetVR(){return &vr;}
    float MetresToWorld(float x){return x*.5f;}
};
Game Game::instance;
struct InputHandler {bool GetCalculatedHandPositions(Matrix4&,Vector3&,Vector3&);};
// INSERT_PRODUCTION_AIM
struct Handler:WeaponHandler {
    Handler(){cachedViewModel.valid=true;cachedViewModel.currentWeapon={1,1};cachedViewModel.leftWristIndex=1;cachedViewModel.rightWristIndex=2;cachedViewModel.gunIndex=3;}
    void Type(WeaponType t){cachedViewModel.weaponType=t;}
    void Pose(Bone* boneArray,Transform* outBoneTransforms,const Matrix4& handTransform){
        const int count=10,wrist=cachedViewModel.rightWristIndex;
        // INSERT_PRODUCTION_SUPPORT_POSE
    }
};
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    auto& g=Game::instance;Handler handler;InputHandler input;
    for(auto type:{WeaponType::Pistol,WeaponType::Needler,WeaponType::PlasmaPistol,WeaponType::PlasmaRifle}){
        handler.Type(type);CHECK(!handler.UsesLongSupportGrip(0x10001));
        CHECK(handler.UsesLongSupportGrip(0x10002)); // Stale/other gun cache cannot opt in.
        for(bool left:{false,true})for(bool facing:{false,true}){
            g.bLeftHanded=left;g.vr.facingAvailable=facing;
            const auto main=left?0u:1u,off=1u-main;
            g.vr.hands[main]=Matrix4().rotateZ(23).translate(.3f,0,-.2f);
            Matrix4 original;Vector3 pos,dir;
            g.vr.hands[off]=Matrix4().translate(.7f,.1f,-.1f);
            const bool calculated=input.GetCalculatedHandPositions(original,pos,dir);const auto firstDir=dir;
            g.vr.hands[off]=Matrix4().rotateX(87).translate(.5f,-.9f,.8f);
            Matrix4 moved;CHECK(input.GetCalculatedHandPositions(moved,pos,dir)==calculated);
            for(int i=0;i<16;++i)CHECK(std::fabs(original[i]-moved[i])<.00001f);
            if(calculated)CHECK(dir==firstDir); // Offhand never steers compact gun.
            Bone bones[10]{};Transform out[10];
            const char* names[]={"root","frame l wriste","frame r wriste","frame gun","frame l index low","frame l index mid","frame l index tip","frame l pinky low","frame l pinky mid","frame l pinky tip"};
            const int parents[]={-1,0,0,2,1,4,5,1,7,8};
            for(unsigned i=0;i<10;++i){std::strcpy(bones[i].BoneName,names[i]);bones[i].Parent=parents[i];out[i]=ModelTransform(Matrix4());}
            for(unsigned f=0;f<2;++f)for(unsigned j=0;j<3;++j)out[4+f*3+j]=ModelTransform(Matrix4().translate(.04f+j*.016f,f?.02f:-.02f,0));
            out[2]=ModelTransform(Matrix4().translate(4,5,6));out[3]=ModelTransform(Matrix4().rotateZ(14).translate(4.2f,5,6));
            const auto gun=out[3],firing=out[2];handler.Pose(bones,out,g.vr.hands[main]);
            CHECK(std::memcmp(&gun,&out[3],sizeof(gun))==0);CHECK(std::memcmp(&firing,&out[2],sizeof(firing))==0);
            CHECK((out[1].translation-out[2].translation).length()<.07f); // At grip, not barrel.
            if(type==WeaponType::Pistol){
                Matrix4 palm;CHECK(LeftPalmFrame(bones,10,out,1,palm));
                Matrix4 basis=GripRotation(g.vr.hands[main]);if(left)basis=basis*Matrix4().scale(1,-1,1);
                const auto offset=Matrix4(basis).invertAffine()*(palm*Vector3()-out[2].translation);
                CHECK((offset-Vector3(.05f,.0075f,-.02f)).length()<.0001f);
                CHECK(palm.getForwardAxis().dot(basis.getForwardAxis())<-.999f); // Support palm faces up.
            }
        }
    }
    for(auto type:{WeaponType::AssaultRifle,WeaponType::Shotgun,WeaponType::Sniper,WeaponType::RocketLauncher}){
        handler.Type(type);CHECK(handler.UsesLongSupportGrip(0x10001));
    }
    g.bUseTwoHandAim=true;g.bLeftHanded=false;g.vr.facingAvailable=false;
    Matrix4 pose;Vector3 pos,dir;g.vr.hands[0]=Matrix4().translate(.5f,0,0);
    CHECK(input.GetCalculatedHandPositions(pose,pos,dir));const auto old=dir;
    g.vr.hands[0]=Matrix4().translate(.4f,.3f,0);CHECK(input.GetCalculatedHandPositions(pose,pos,dir));
    CHECK((dir-old).length()>.1f); // Long guns retain offhand-directed aiming.
    std::cout<<checks<<" production compact-support pose and firing-hand aim checks passed\n";
}
