// Actual WeaponHandler recoil, fire hooks, aim, cache correction and transforms.
#include "HaloCEVR/WeaponHandler.h"
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <array>
struct FloatProperty { float value=1;float Value()const{return value;} };
struct VectorProperty {Vector3 value{};Vector3 Value()const{return value;}};
struct VRDouble {Matrix4 GetHMDTransform(bool)const{return {};}};
struct Game {
    static Game instance;
    bool valid=true,supported=false,stowed=false,bUse3DOFAiming=false,bNeedsRecentre=false;
    bool useSupportOnly=false,aimEnabled=false,bLeftHanded=false;
    Vector3 aimTarget{1,0,0};
    VectorProperty scope;
    VectorProperty *c_ScopeOffsetRocket=&scope,*c_ScopeOffsetSniper=&scope,*c_ScopeOffsetPistol=&scope;
    Matrix4 hand,support;
    FloatProperty scale{1},fraction{.35f},weaponScale{1};
    FloatProperty* c_RecoilScale=&scale;
    FloatProperty* c_SupportedRecoilFraction=&fraction;
    std::array<FloatProperty*,12> c_WeaponRecoilScales{};
    Immersion::FireState state{0x10001,0x20002,10,{12,0},0,0};
    VRDouble vr;
    VRDouble* GetVR(){return &vr;}
    struct Haptics {void WeaponFired(WeaponType){}} weaponHapticsConfig;
    bool PhysicalRecoilState(const HaloID& id,Immersion::FireState& out,bool& supportOut)const{
        out=state;supportOut=supported;
        return valid && !stowed && (static_cast<unsigned>(id.index)|(static_cast<unsigned>(id.id)<<16))==state.weapon;
    }
    bool PhysicalWeaponsStowed()const{return stowed;}
    bool GetPhysicalSupportTransform(Matrix4& out)const{out=support;return useSupportOnly;}
    bool GetCalculatedHandPositions(Matrix4& out,Vector3& pos,Vector3& dir)const{
        out=hand;pos=hand*Vector3();dir=aimTarget-pos;
        if(dir.lengthSqr()<1e-8f)dir=hand.getLeftAxis();else dir.normalize();
        return aimEnabled;
    }
    Vector3 GetSmoothedInput()const{return aimTarget;}
    float WorldToMetres(float f)const{return f*2;}
    float MetresToWorld(float f)const{return f*.5f;}
};
Game Game::instance;
namespace Helpers {
    bool menu=false,loading=false;
    struct Cutscene {bool bInCutscene=false;} cutscene;
    bool IsMouseVisible(){return menu;}
    bool IsCampaignLoading(){return loading;}
    Cutscene* GetCutsceneData(){return &cutscene;}
    UnitDynamicObject player{};
    BaseDynamicObject weapon{};
    bool GetLocalPlayerID(HaloID& id){id={1,1};return true;}
    BaseDynamicObject* GetDynamicObject(HaloID& id){
        if(id.index==1 && id.id==1)return &player;
        if(id.index==2 && id.id==2)return &weapon;
        return nullptr;
    }
}
void WeaponHandler::HandleWeaponHaptics()const{}
// INSERT_PRODUCTION_RECOIL
struct Handler:WeaponHandler {
    using WeaponHandler::cachedViewModel;
    using WeaponHandler::recoil;
    using WeaponHandler::RecoilKick;
    using WeaponHandler::RemoveRecoilFromCache;
    using WeaponHandler::CacheFireAlignment;
    Handler(){cachedViewModel.valid=true;cachedViewModel.currentWeapon={2,2};cachedViewModel.weaponType=WeaponType::Pistol;}
};
unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
bool close(float a,float b){return std::abs(a-b)<2e-4f;}
void matrixEqual(const Matrix4& a,const Matrix4& b){for(int i=0;i<16;++i)CHECK(close(a[i],b[i]));}
Matrix3 rotation(const Matrix4& pose){
    Matrix3 out;for(int i=0;i<3;++i)out.setColumn(i,&pose.get()[i*4]);return out;
}
void vectorEqual(const Vector3& a,const Vector3& b){CHECK((a-b).length()<.0002f);}
int main(){
    auto& g=Game::instance;HaloID weapon{2,2};
    Helpers::weapon.parent={1,1};Helpers::player.position={1,2,3};Helpers::player.aim={1,0,0};
    for(unsigned type=1;type<=11;++type){
        float kicks[2]{};
        for(unsigned supported=0;supported<2;++supported){
            Handler h;h.cachedViewModel.weaponType=static_cast<WeaponType>(type);g.supported=supported!=0;
            h.PreFireWeapon(weapon,0);
            CHECK(close(Helpers::player.aim.z,0)); // First bullet has no pre-shot punishment.
            ++g.state.tick;--g.state.loaded[0];
            h.PostFireWeapon(weapon,0);kicks[supported]=h.recoil.Pose(g.state.weapon).pitch;
            CHECK(kicks[supported]>0);
            CHECK(Helpers::player.position==Vector3(1,2,3));CHECK(Helpers::player.aim==Vector3(1,0,0));
            Vector3 pos,aim,up;CHECK(h.GetLocalWeaponAim(pos,aim,up));CHECK(aim.z>0);
            const Matrix4 pose=h.GetDominantHandTransform();
            matrixEqual(pose,h.GetDominantHandTransform()); // Eyes/scope queries don't advance spring.
            matrixEqual(g.hand,h.GetUnrecoiledHandTransform()); // Grab positions never kick.
            h.PreFireWeapon(weapon,0);
            CHECK(Helpers::player.aim.z>0); // Next bullet follows visible raised muzzle.
            CHECK((Helpers::player.aim-aim).length()<0.001f);
            h.PostFireWeapon(weapon,0); // Dry fire: no tick/resource change.
            CHECK(h.recoil.Pose(g.state.weapon).pitch<=kicks[supported]);
            CHECK(h.recoil.Pose(g.state.weapon).pitch>kicks[supported]*0.98f);
        }
        CHECK(close(kicks[1],kicks[0]*.35f));
    }
    { // Offsets are removed before alignment caching, even with rotated weapons.
        Handler h;h.recoil.Context(g.state.unit,g.state.weapon);h.recoil.Shot({5,.05f,.2f,15},false,1,.35f,0);
        Matrix4 base;base.rotateX(33);base.rotateY(25);base.rotateZ(72);base.translate(10,20,30);
        Matrix4 localGun;localGun.rotateZ(-12);localGun.translate(.5f,.1f,.2f);
        Matrix4 kickedHand=base*h.RecoilKick(.5f),kickedGun=kickedHand*localGun;
        Vector3 gun=kickedGun*Vector3();Matrix3 gunRot;
        for(int i=0;i<3;++i)gunRot.setColumn(i,&kickedGun.get()[i*4]);
        h.RemoveRecoilFromCache(kickedHand,gun,gunRot);
        matrixEqual(kickedHand,base);
        const auto expected=base*localGun;
        CHECK((gun-expected*Vector3()).length()<.0001f);
        for(int col=0;col<3;++col)for(int row=0;row<3;++row)CHECK(close(gunRot[col*3+row],expected[col*4+row]));
    }
    { // Energy gun success and NPC/stowed/invalid frame are handled by actual hooks.
        Handler h;h.PreFireWeapon(weapon,1);++g.state.tick;h.PostFireWeapon(weapon,1);CHECK(h.recoil.Pose(g.state.weapon).pitch>0);
        g.stowed=true;h.UpdateRecoil();CHECK(h.recoil.Pose(g.state.weapon).pitch==0);g.stowed=false;
        HaloID npc{3,3};h.PreFireWeapon(npc,0);++g.state.tick;h.PostFireWeapon(npc,0);CHECK(h.recoil.Pose(g.state.weapon).pitch==0);
        for(bool* blocked:{&Helpers::menu,&Helpers::loading,&Helpers::cutscene.bInCutscene,&g.bUse3DOFAiming,&g.bNeedsRecentre}){
            *blocked=true;h.PreFireWeapon(weapon,0);++g.state.tick;h.PostFireWeapon(weapon,0);
            CHECK(h.recoil.Pose(g.state.weapon).pitch==0);*blocked=false;
        }
        g.valid=false;h.PreFireWeapon(weapon,0);++g.state.tick;h.PostFireWeapon(weapon,0);CHECK(h.recoil.Pose(g.state.weapon).pitch==0);g.valid=true;
        g.scale.value=0;h.PreFireWeapon(weapon,0);++g.state.tick;h.PostFireWeapon(weapon,0);CHECK(h.recoil.Pose(g.state.weapon).pitch==0);g.scale.value=1;
        g.c_WeaponRecoilScales[1]=&g.weaponScale;g.weaponScale.value=.5f;
        h.PreFireWeapon(weapon,0);++g.state.tick;h.PostFireWeapon(weapon,0);
        CHECK(close(h.recoil.Pose(g.state.weapon).pitch,3.4f*.5f*.35f));
    }
    { // Non-commuting gun/marker rotations must never carry world yaw into
      // the next supported pose. Exercise acquisition, repeated render passes,
      // smooth/snap turns, both mirrored skeletons, scope and actual fire hook.
        for(auto type:{WeaponType::RocketLauncher,WeaponType::AssaultRifle,WeaponType::Shotgun,WeaponType::Sniper,WeaponType::Pistol})
        for(bool left:{false,true}) {
            Handler h;h.cachedViewModel.weaponType=type;g.bLeftHanded=left;
            Matrix4 gunLocal,marker;
            if(type==WeaponType::RocketLauncher) {
                gunLocal.rotateX(23).rotateY(-31).rotateZ(47);
                marker.rotateY(19).rotateZ(-17);
            }
            const Matrix4 mirror=Matrix4().scale(1,left?-1.f:1.f,1);
            gunLocal=mirror*gunLocal*mirror;marker=mirror*marker*mirror;
            gunLocal.translate(.12f,left?-.04f:.04f,.08f);
            h.cachedViewModel.cookedFireRotation=rotation(marker);
            h.cachedViewModel.cookedFireOffset={.4f,0,.1f};
            const Matrix4 muzzleLocal=gunLocal*marker;
            h.CacheFireAlignment(Matrix4(),gunLocal*Vector3(),rotation(gunLocal));
            g.aimEnabled=true;
            for(float yaw:{0.f,2.f,4.f,90.f,135.f,-179.f,179.f,270.f,360.f}) {
                const Matrix4 desired=Matrix4().rotateX(12).rotateY(-25).rotateZ(yaw).translate(.3f,-.2f,-.1f);
                g.hand=desired;g.aimTarget=desired*Vector3(1,0,0);
                for(unsigned frame=0;frame<30;++frame) {
                    const Matrix4 hand=h.GetUnrecoiledHandTransform();
                    const Matrix4 gun=hand*gunLocal;
                    vectorEqual((hand*muzzleLocal).getLeftAxis(),desired.getLeftAxis());
                    vectorEqual((hand*muzzleLocal).getForwardAxis(),desired.getForwardAxis());
                    vectorEqual(hand*Vector3(),desired*Vector3());
                    h.CacheFireAlignment(hand,gun*Vector3(),rotation(gun));
                    matrixEqual(hand,h.GetUnrecoiledHandTransform());
                    const Matrix3 local=rotation(muzzleLocal);
                    for(int i=0;i<9;++i)CHECK(close(h.cachedViewModel.fireRotation[i],local[i]));
                    vectorEqual(h.cachedViewModel.gunOffset,gunLocal*Vector3());
                    vectorEqual(h.cachedViewModel.fireOffset,gunLocal*h.cachedViewModel.cookedFireOffset);
                    Vector3 pos,aim,up,scopePos,scopeAim,scopeUp;
                    CHECK(h.GetLocalWeaponAim(pos,aim,up));CHECK(h.GetLocalWeaponScope(scopePos,scopeAim,scopeUp));
                    vectorEqual(aim,desired.getLeftAxis());vectorEqual(scopeAim,aim);
                    h.PreFireWeapon(weapon,0);vectorEqual(Helpers::player.aim,aim);h.PostFireWeapon(weapon,0);
                }
            }
            // A supported shot must also keep transient kick out of the cache.
            h.recoil.Context(g.state.unit,g.state.weapon);h.recoil.Shot({5,.05f,.2f,15},false,1,.35f,0);
            const Matrix4 base=h.GetUnrecoiledHandTransform();
            const Matrix4 kicked=base*h.RecoilKick(.5f),gun=kicked*gunLocal;
            h.CacheFireAlignment(kicked,gun*Vector3(),rotation(gun));
            matrixEqual(base,h.GetUnrecoiledHandTransform());
        }
        // Coincident smoothing target and either vertical hand arrangement
        // cannot produce a zero/singular look-at matrix on initial support.
        Handler h;g.hand=Matrix4().translate(.3f,-.2f,-.1f);
        for(Vector3 direction:{Vector3(),Vector3(0,0,1),Vector3(0,0,-1),Vector3(.0001f,0,1)}) {
            g.aimTarget=g.hand*Vector3()+direction;
            const auto pose=h.GetUnrecoiledHandTransform();
            for(int i=0;i<16;++i)CHECK(std::isfinite(pose[i]));
            CHECK(close(pose.getDeterminant(),1.f));
            vectorEqual(pose*Vector3(),g.hand*Vector3());
        }
        g.aimEnabled=false;g.bLeftHanded=false;g.hand=Matrix4();
    }
    std::cout<<checks<<" production recoil/fire/aim/cache checks passed (engine doubles)\n";
}
