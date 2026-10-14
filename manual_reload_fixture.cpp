#include "HaloCEVR/Immersion/OutlineZone.h"
// run.py compiles the complete production ReloadHandler with engine/VR doubles.
#include "HaloCEVR/ReloadHandler.h"
#include "HaloCEVR/Immersion/HandGripPose.h"
#include "HaloCEVR/Immersion/RetailWeaponEngine.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cctype>
using namespace Immersion;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
struct Bone {char BoneName[32]{};std::int16_t LeftLeaf=-1,RightLeaf=-1,Parent=-1;char pad[26]{};};
enum class ControllerRole {Left,Right};
template<class T>struct Property {T value;T Value()const{return value;}};
using D3DCOLOR=std::uint32_t;
#define D3DCOLOR_ARGB(a,r,g,b) ((D3DCOLOR(a)<<24)|(D3DCOLOR(r)<<16)|(D3DCOLOR(g)<<8)|D3DCOLOR(b))
struct Font {};
struct VRDouble {
    unsigned buzz[2]{};bool tracked=true;
    bool IsHMDTracked()const{return tracked;}
    void TriggerHapticVibration(ControllerRole hand,float,float,float,float){++buzz[static_cast<unsigned>(hand)];}
};
struct RenderDouble {
    unsigned triangles=0,lines=0,labels=0,circles=0;Vector3 labelCenter;std::string label;
    void DrawMeshTriangle(const Vector3 (&v)[3],const D3DCOLOR (&)[3]){
        for(const auto& p:v)CHECK(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z));
        ++triangles;
    }
    void DrawLine3D(const Vector3&,const Vector3&,D3DCOLOR,bool=true,float=.05f){++lines;}
    void DrawLabel3D(const Font&,const char* text,const Vector3& center,const Vector3&,const Vector3&,float,float=0,bool=true){++labels;label=text;labelCenter=center;}
    void DrawPolygon(const Vector3&,const Vector3&,const Vector3&,int,float,D3DCOLOR,bool=true,float=1){++circles;}
};
struct UI {Font font;Font* GetSmallFont(){return &font;}};
struct Game {
    static Game instance;
    Property<bool> enabled{true},layout{false};Property<int> ratio{1};
    Property<float> radius{.14f},stroke{.09f},supply{.17f},volume{.6f};
    Property<bool>* c_ManualReload=&enabled;Property<bool>* c_LeftHanded=&layout;
    Property<int>* c_ShotgunShellRatio=&ratio;
    Property<float>* c_ReloadGrabRadius=&radius;Property<float>* c_ReloadStroke=&stroke;
    Property<float>* c_MagazineZoneRadius=&supply;
    Property<float>* c_ReloadSoundVolume=&volume;
    bool bIsReloading=false,visuals=true,bUseTwoHandAim=false;float lastDeltaTime=1.f/90;
    VRDouble vr;RenderDouble inGameRenderer;UI ui;UI* uiRenderer=&ui;
    VRDouble* GetVR(){return &vr;}bool PhysicalVisualsActive()const{return visuals;}
    float MetresToWorld(float value)const{return value*.5f;}
    float WorldToMetres(float value)const{return value*2;}
};
Game Game::instance;
namespace Logger {std::ostringstream log;}
static std::vector<ReloadCue> cues;
bool InitializeReloadAudio(){return true;}
bool PlayNativeReloadSound(Object,Object,float volume){CHECK(volume<=.0001f);return true;} // Inaudible decoder warm-up, not a played cue.
static unsigned pumpEffects=0;
bool PlayNativeReloadEffect(Object,Object){++pumpEffects;return true;}
void StopReloadAudio(){}
void StopNativeReloadSound(){}
void UpdateReloadPcm(){}
void StopReloadPcm(){}
bool QueueReloadPcm(const ReloadPcm& pcm,float){cues.push_back(static_cast<ReloadCue>(pcm.samples[pcm.samples.size()/2]-100));return true;}
namespace Helpers {
    struct Camera {Vector3 position{10,20,30};} camera;
    struct Controls {unsigned char Reload=0;} controls;
    struct Cutscene {bool bInCutscene=false;} scene;
    bool menu=false,loading=false;
    Camera& GetCamera(){return camera;}Controls& GetControls(){return controls;}
    const Cutscene* GetCutsceneData(){return &scene;}
    bool IsMouseVisible(){return menu;}bool IsCampaignLoading(){return loading;}
}
struct EngineDouble {
    NativeInventory inventory{100,0x10000,100,{11,22,None,None},11};
    std::map<Object,ReloadAmmo> ammo;
    bool stowed=false,valid=true,rejectCommit=false,model=false,allowDrop=false;
    int refillOnPickup=0; // Fault injection: native ammo can change outside manual insertion.
    unsigned commits=0,selections=0,drops=0,buttonReloads=0;
    bool rocketModel=false;std::vector<bool> rocketDraws;std::vector<Matrix4> rocketPoses;
    std::array<ModelNode,64> nodes{},hands{};unsigned nodeCount=0,handCount=0;
} native;
bool RetailWeaponEngine::ReadNative(NativeInventory& out)const{out=native.inventory;return native.valid;}
bool RetailWeaponEngine::Read(NativeInventory& out)const{out=native.inventory;if(native.stowed)out.current=None;return native.valid;}
bool RetailWeaponEngine::Select(Object unit,Object weapon){
    if(unit!=native.inventory.unit || (weapon!=None&&!native.inventory.Contains(weapon)))return false;
    ++native.selections;native.stowed=weapon==None;if(!native.stowed)native.inventory.current=weapon;return true;
}
NativeDrop RetailWeaponEngine::Drop(Object unit,Object weapon,const DropPose&){
    ++native.drops;
    if(!native.allowDrop || unit!=native.inventory.unit || native.stowed || native.inventory.current!=weapon)
        return NativeDrop::Rejected;
    for(auto& slot:native.inventory.weapons)if(slot==weapon)slot=None;
    native.inventory.current=None;native.stowed=true;return NativeDrop::WorldObject;
}
bool RetailWeaponEngine::Pickup(Object unit,Object weapon){
    if(!native.allowDrop || unit!=native.inventory.unit || !native.stowed || native.inventory.Contains(weapon) ||
        native.ammo.find(weapon)==native.ammo.end())return false;
    auto slot=std::find(native.inventory.weapons.begin(),native.inventory.weapons.end(),None);
    if(slot==native.inventory.weapons.end())return false;
    *slot=weapon;auto& ammo=native.ammo[weapon];
    const int rounds=(std::min)(native.refillOnPickup,(std::min)(ammo.reserve,ammo.capacity-ammo.loaded));
    ammo.loaded+=rounds;ammo.reserve-=rounds;
    return Select(unit,weapon);
}
bool RetailWeaponEngine::ReadReloadAmmo(Object weapon,ReloadAmmo& out,bool current)const {
    const auto item=native.ammo.find(weapon);out={};
    if(!native.valid || !native.inventory.Contains(weapon) || item==native.ammo.end() ||
        (current && (native.stowed || native.inventory.current!=weapon)))return false;
    out=item->second;out.unit=native.inventory.unit;return ValidReloadAmmo(out) && out.nativeState>=0 && out.nativeState<=3;
}
bool RetailWeaponEngine::CommitReloadAmmo(const ReloadAmmo& before,const ReloadAmmo& after){
    ReloadAmmo now;
    if(native.rejectCommit || !ReadReloadAmmo(before.weapon,now) || now.loaded!=before.loaded || now.reserve!=before.reserve ||
        now.unit!=before.unit || (now.nativeState!=0 && now.nativeState!=2) ||
        after.loaded+after.reserve!=before.loaded+before.reserve)return false;
    native.ammo[before.weapon]=after;++native.commits;return true;
}
bool RetailWeaponEngine::ReadReloadModel(Object,std::array<ModelNode,64>& out,unsigned& count)const {
    out=native.nodes;count=native.nodeCount;return native.model;
}
bool RetailWeaponEngine::ReadReloadRocketModel(Object weapon,Object& model)const{
    model=native.rocketModel && native.ammo[weapon].kind==ReloadWeaponKind::Rocket?777:None;return model!=None;
}
bool RetailWeaponEngine::DrawReloadRocket(Object,const Matrix4& pose,bool spent)const{
    native.rocketDraws.push_back(spent);native.rocketPoses.push_back(pose);return native.rocketModel;
}
bool RetailWeaponEngine::ReadReloadHandModel(Object,std::array<ModelNode,64>& out,unsigned& count)const {
    out=native.hands;count=native.handCount;return count!=0;
}
bool RetailWeaponEngine::ReadReloadPcm(Object,Object sound,ReloadPcm& pcm)const{
    pcm.rate=22050;pcm.channels=1;pcm.samples.assign(4410,static_cast<std::int16_t>(sound));return true;
}
bool RetailWeaponEngine::ReadReloadFeedback(Object,ReloadFeedbackAssets& assets)const{
    for(unsigned i=0;i<assets.sounds.size();++i)assets.sounds[i]=100+i;
    assets.chamberEffect=200;assets.pumpSounds[0]=104;assets.pumpSoundCount=1;return true;
}
bool RetailWeaponEngine::ValidReloadFeedbackTag(Object weapon,Object tag,bool)const{return native.inventory.Contains(weapon) && tag!=None;}
static ReloadHandler* reloadUnderTest=nullptr;
bool RetailWeaponEngine::StartButtonReload(Object weapon){
    ReloadAmmo ammo;
    if(!ReadReloadAmmo(weapon,ammo) || ammo.kind!=ReloadWeaponKind::Needler || ammo.loaded>=ammo.capacity || ammo.reserve<=0 ||
        (ammo.nativeState!=0 && ammo.nativeState!=2))return false;
    CHECK(reloadUnderTest && !reloadUnderTest->BlocksNative(weapon));
    ++native.buttonReloads;native.ammo[weapon].nativeState=1;return true;
}
// INSERT_PRODUCTION_RELOAD
struct Fixture {
    RetailWeaponEngine engine;WeaponSession session;ReloadHandler reload;
    Button buttons[2];Matrix4 hands[2];unsigned main,off;WeaponFrame frame;
    Fixture(ReloadWeaponKind kind,int loaded,int capacity,unsigned hand=1,bool mirrored=false):main(hand),off(1-hand){
        native={};reloadUnderTest=&reload;cues.clear();native.ammo[11]={100,11,123,kind,loaded,100,capacity,0,10};
        auto& g=Game::instance;g.enabled.value=true;g.layout.value=mirrored;g.ratio.value=1;g.visuals=true;g.vr={};g.inGameRenderer={};
        Helpers::menu=Helpers::loading=Helpers::scene.bInCutscene=false;
        session.Update(engine,{});WeaponFrame draw;draw.holster[main]=1;draw.grip[main]={true,true,false};session.Update(engine,draw);
        CHECK(session.Held()==11);CHECK(Index(session.FiringHand())==main);
        hands[main].translate(.5f,0,-.1f);at({0,.7f,0});
        for(unsigned n=0;n<100;++n)tick(false); // Let the real sound bank confirm stable decoded samples.
    }
    ReloadAmmo& ammo(){return native.ammo[11];}
    void at(Point p){hands[off]=Matrix4().translate(p.x,p.y,p.z);}
    void relative(Point p){const auto world=hands[main]*Vector3(p.x,p.y,p.z);at({world.x,world.y,world.z});}
    Point supply()const{return Layout{Game::instance.layout.value}.Offset(Zone::SupplyHip);}
    ReloadSockets sockets()const{return DefaultReloadSockets(native.ammo.at(11).kind);}
    bool step(bool mainGrip,bool grip,bool b=false,bool consumed=false,int home=-1,Object pickup=None){
        ++native.inventory.age;frame={};
        frame.grip[main]=buttons[main].Update(mainGrip);frame.grip[off]=buttons[off].Update(grip);frame.consumed[off]=consumed;
        frame.holster[main]=home;frame.pickupTarget=pickup;
        Matrix4 inverse=hands[main];const auto p=inverse.invertAffine()*(hands[off]*Vector3());
        frame.nearSupport[off]=p.x>=-.04f && p.x<=.65f && p.y*p.y+p.z*p.z<=.04f;
        const bool result=reload.Process(engine,session,frame,hands,{},0,b,true);
        session.Update(engine,frame);return result;
    }
    bool tick(bool grip,bool b=false,bool consumed=false){
        const bool result=step(true,grip,b,consumed);CHECK(session.Held()==11);return result;
    }
    void eject(){tick(false,true);tick(false,false);}
    void supplyGrab(){at(supply());tick(false);CHECK(tick(true));CHECK(frame.consumed[off]);}
    void insert(bool rocket=false){relative(sockets().insert);tick(true);if(rocket)tick(false);}
    void rack(){
        tick(false);relative(sockets().action);const bool grabbed=tick(true);CHECK(grabbed || ammo().kind==ReloadWeaponKind::Shotgun);
        relative(sockets().action+Point{-.12f,0,0});tick(true);
        relative(sockets().action);CHECK(tick(true));tick(false);
    }
    void cover(){tick(false);CHECK(tick(false,true));tick(false,false);}
    void removeRocket(){tick(false);relative(sockets().insert);CHECK(tick(true));at({.7f,.4f,-.1f});CHECK(tick(false));}
    void shot(){reload.BeforeShot(11);--ammo().loaded;++ammo().tick;reload.AfterShot(11);}
};
int main(){
    for(auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Rifle,ReloadWeaponKind::Sniper})
    for(unsigned hand:{0u,1u})for(int rounds:{0,5}){
        Fixture f(kind,rounds,12,hand);const int reserve=f.ammo().reserve;const unsigned effects=pumpEffects;
        for(unsigned repeat=0;repeat<3;++repeat){
            if(kind!=ReloadWeaponKind::Pistol && rounds>0){
                // Loaded rifles never steal support or consume grip at the handle.
                f.tick(false);f.relative(f.sockets().action);CHECK(!f.tick(true));
                CHECK(!f.frame.consumed[f.off]);CHECK(f.session.Items().Supported(11));
                f.relative(f.sockets().action+Point{-.12f,0,0});CHECK(!f.tick(true));
                f.relative(f.sockets().action);CHECK(!f.tick(true));
                CHECK(f.session.Items().Supported(11));CHECK(cues.empty());f.tick(false);
                CHECK(f.ammo().loaded==rounds && f.ammo().reserve==reserve);
                CHECK(native.commits==0 && !f.reload.BlocksFire(11));continue;
            }
            f.rack();CHECK(f.ammo().loaded==rounds && f.ammo().reserve==reserve);
            CHECK(native.commits==0 && pumpEffects==effects); // Free cycling never ejects live ammunition.
            CHECK(f.reload.BlocksFire(11)==(rounds==0));
        }
        CHECK(!f.reload.BlocksNativeFire(11)); // Empty clicks pass; ready guns fire.
        if(rounds>0){f.shot();if(kind==ReloadWeaponKind::Shotgun)CHECK(f.reload.BlocksNativeFire(11));}
    }
    for(unsigned hand:{0u,1u}){
        Fixture f(ReloadWeaponKind::Shotgun,6,12,hand);
        f.relative(f.sockets().action);f.tick(true);f.relative(f.sockets().action+Point{-.08f,0,0});f.tick(true);
        f.relative(f.sockets().action);f.tick(true);CHECK(f.session.Items().Supported(11));
        for(int n=0;n<20;++n){f.tick(true);CHECK(!f.reload.BlocksNativeFire(11));}
        // A ready shotgun is support-only even with a long deliberate stroke.
        const auto effects=pumpEffects;
        f.relative(f.sockets().action+Point{-.025f,0,0});f.tick(true);CHECK(!f.reload.BlocksNativeFire(11));
        f.relative(f.sockets().action+Point{-.08f,0,0});f.tick(true);f.relative(f.sockets().action);f.tick(true);
        CHECK(f.ammo().loaded==6 && f.ammo().reserve==100 && native.commits==0);
        CHECK(f.session.Items().Supported(11));CHECK(!f.reload.BlocksNativeFire(11));
        CHECK(pumpEffects==effects);
        f.shot();CHECK(f.reload.BlocksNativeFire(11));f.rack();CHECK(!f.reload.BlocksNativeFire(11));
    }
    for(unsigned hand:{0u,1u})for(int rounds:{0,10}){
        Fixture f(ReloadWeaponKind::Needler,rounds,20,hand);
        while(f.ammo().loaded>0){f.shot();CHECK(!f.reload.BlocksNativeFire(11));}
        for(int n=0;n<120;++n){f.tick(false);CHECK(f.reload.BlocksNative(11));CHECK(!f.reload.BlocksNativeFire(11));}
        CHECK(native.buttonReloads==0);CHECK(f.tick(false,true));CHECK(native.buttonReloads==1);
        CHECK(!f.reload.BlocksNative(11)); // Native animation/end hook must finish.
        for(int n=0;n<20;++n){f.tick(false,true);}CHECK(native.buttonReloads==1);
        f.ammo().nativeState=0;f.ammo().loaded=10;CHECK(!f.reload.BlocksNativeFire(11));
        CHECK(f.reload.BlocksNative(11)); // Permit is not retained; native reload never inherits a manual chamber lock.
        f.tick(false);CHECK(f.tick(false,true));CHECK(native.buttonReloads==2);
        f.ammo().nativeState=0;f.ammo().reserve=0;f.tick(false);CHECK(!f.tick(false,true));CHECK(f.reload.BlocksNative(11));
        CHECK(native.commits==0); // No manual ammo writes or replacement animation.
    }
    {Fixture f(ReloadWeaponKind::Shotgun,0,12);const auto effects=pumpEffects;
        for(int n=0;n<10;++n){CHECK(!f.reload.BlocksNativeFire(11));f.reload.BeforeShot(11);++f.ammo().tick;f.reload.AfterShot(11);}
        f.rack();CHECK(pumpEffects==effects);CHECK(f.ammo().loaded==0 && native.commits==0);
    }

    for(const auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Rifle,ReloadWeaponKind::Sniper})
    for(unsigned hand:{0u,1u})for(bool mirrored:{false,true})for(int loaded:{0,7}){
        Fixture f(kind,loaded,12,hand,mirrored);CHECK(f.reload.BlocksNative(11));CHECK(!f.reload.BlocksNative(999));
        CHECK(f.reload.BlocksFire(11)==(loaded==0));f.eject();CHECK(f.ammo().loaded==0);CHECK(f.ammo().reserve==100+loaded);CHECK(f.reload.BlocksFire(11));
        f.supplyGrab();CHECK(f.ammo().reserve==100+loaded);f.insert();CHECK(f.ammo().loaded==12);CHECK(f.ammo().reserve==88+loaded);
        CHECK(f.reload.BlocksFire(11)==(loaded==0));
        if(loaded==0)f.rack();
        CHECK(!f.reload.BlocksFire(11));
        CHECK(native.drops==0);CHECK(native.selections==2);CHECK(Game::instance.vr.buzz[f.off]==(loaded==0?5u:2u));
        CHECK(Game::instance.vr.buzz[f.main]==1); // Ejection acts on the firing hand; every physical manipulation uses the offhand.
        const std::vector<ReloadCue> expectedCues=loaded==0 ? std::vector<ReloadCue>{ReloadCue::Eject,ReloadCue::Grab,ReloadCue::Insert,ReloadCue::ActionGrab,ReloadCue::Pull,ReloadCue::Close} : std::vector<ReloadCue>{ReloadCue::Eject,ReloadCue::Grab,ReloadCue::Insert};
        CHECK(cues==expectedCues);
        f.reload.Draw();CHECK(Game::instance.inGameRenderer.label=="Magazine Zone");CHECK(Game::instance.inGameRenderer.lines==0);CHECK(Game::instance.inGameRenderer.triangles>=96);
        const auto expected=Helpers::camera.position+Vector3(.14f,mirrored?-.25f:.25f,-.43f)*.5f;
        CHECK((Game::instance.inGameRenderer.labelCenter-expected).length()<.0001f);
    }
    { // Discard/cancel a fresh magazine without consuming reserve; failed write is reversible.
        Fixture f(ReloadWeaponKind::Rifle,7,60);f.eject();f.supplyGrab();f.at({.7f,.6f,.1f});f.tick(false);
        CHECK(f.ammo().reserve==107);CHECK(f.ammo().loaded==0);f.supplyGrab();native.rejectCommit=true;f.insert();
        CHECK(f.ammo().loaded==0);CHECK(f.reload.BlocksFire(11));native.rejectCommit=false;f.tick(true);CHECK(f.ammo().loaded==60);
        CHECK(f.ammo().reserve==47);CHECK(!f.reload.BlocksFire(11));for(int n=0;n<100;++n)f.tick(true);CHECK(native.commits==2);
    }
    { // A blocked B write leaves both physical and native magazine untouched.
        Fixture f(ReloadWeaponKind::Pistol,4,12);native.rejectCommit=true;f.eject();CHECK(!f.reload.BlocksFire(11));CHECK(f.ammo().loaded==4);
        native.rejectCommit=false;f.eject();CHECK(f.reload.BlocksFire(11));
    }
    for(auto kind:{ReloadWeaponKind::Rifle,ReloadWeaponKind::Pistol,ReloadWeaponKind::Sniper})
    for(unsigned hand:{0u,1u})for(int loaded:{0,5})for(int refill:{1,60}){
        // Eject -> drop -> menu -> pickup -> stow/draw. An external/native refill
        // must not strand an absent physical magazine with a full ammo counter.
        const int capacity=kind==ReloadWeaponKind::Rifle?60:kind==ReloadWeaponKind::Pistol?12:8;
        Fixture f(kind,loaded,capacity,hand,hand==0);native.allowDrop=true;native.refillOnPickup=refill;
        native.ammo[22]={100,22,456,ReloadWeaponKind::Pistol,6,40,12,0,10};
        f.eject();const int total=f.ammo().reserve;
        f.step(false,false);CHECK(f.session.Held()==None);CHECK(!native.inventory.Contains(11));
        f.reload.Suspend();f.step(false,false); // Pausing cannot silently seat a magazine.
        f.step(true,false,false,false,-1,11);CHECK(f.session.Held()==11);
        CHECK(f.ammo().loaded>0);CHECK(f.ammo().loaded+f.ammo().reserve==total);
        f.tick(false);CHECK(f.ammo().loaded==0);CHECK(f.ammo().reserve==total);CHECK(f.reload.BlocksFire(11));
        f.step(false,false,false,false,1);CHECK(f.session.Held()==None);
        f.step(true,false,false,false,0);CHECK(f.session.Held()==22);
        f.step(false,false,false,false,0);CHECK(f.session.Held()==None);
        f.step(true,false,false,false,1);CHECK(f.session.Held()==11);
        f.supplyGrab();f.insert();CHECK(f.ammo().loaded==capacity);CHECK(f.ammo().reserve==total-capacity);
        CHECK(f.reload.BlocksFire(11)==(loaded==0));if(loaded==0)f.rack();
        CHECK(!f.reload.BlocksFire(11));CHECK(native.ammo[22].loaded==6);CHECK(native.ammo[22].reserve==40);
    }
    for(unsigned hand:{0u,1u})for(float yaw:{-35.f,70.f}){
        Fixture f(ReloadWeaponKind::Shotgun,2,12,hand);native.model=true;native.nodeCount=4;native.handCount=14;
        const char* names[]={"root","frame l wriste","frame r wriste","frame gun","frame bullet","frame pump","frame magazine",
            "frame l index low","frame l index mid","frame l pinky low","frame l pinky mid","frame l thumb low","frame l thumb mid","frame l thumb tip"};
        const int parents[]={-1,0,0,2,3,3,3,1,7,1,9,1,11,12};
        const Vector3 positions[]={{},{},{},{},{0,0,.02f},{.18f,0,0},{.04f,0,.01f},
            {.04f,-.018f,0},{.056f,-.018f,0},{.04f,.018f,0},{.056f,.018f,0},{.015f,-.03f,0},{.03f,-.04f,0},{.045f,-.05f,0}};
        Bone bones[14];Transform out[14];
        for(unsigned i=0;i<14;++i){std::strcpy(bones[i].BoneName,names[i]);bones[i].Parent=parents[i];
            std::strcpy(native.hands[i].name,names[i]);native.hands[i].parent=parents[i];
            native.hands[i].inverseBind=ModelTransform(Matrix4().translate(-positions[i]));}
        for(unsigned i=0;i<4;++i){native.nodes[i]=native.hands[i+3];native.nodes[i].parent=i?0:-1;}
        f.reload.Suspend();f.tick(false);f.supplyGrab();
        const auto cam=Helpers::camera.position;const float mirror=hand==0?-1.f:1.f;
        const Matrix4 off=Matrix4().scale(1,mirror,1).rotateZ(yaw).translate(cam+(f.hands[f.off]*Vector3())*.5f);
        const Matrix4 gun=Matrix4().scale(1,mirror,1).rotateY(13).translate(cam+(f.hands[f.main]*Vector3())*.5f);
        for(unsigned i=0;i<14;++i)out[i]=ModelTransform((i==1 || i>=7?off:gun)*Matrix4().translate(positions[i]));
        out[4].scale=0;const auto mainHand=out[2],mainGun=out[3];
        f.reload.Pose(11,bones,out,14,3,1);
        CHECK(out[4].scale!=0);CHECK(std::memcmp(&mainHand,&out[2],sizeof(mainHand))==0);
        CHECK(std::memcmp(&mainGun,&out[3],sizeof(mainGun))==0);
        for(unsigned i=11;i<14;++i)CHECK((out[i].translation-off*positions[i]).length()<.0001f);
        CHECK((out[8].translation-off*positions[8]).length()>.01f); // Index curled, thumb extended.
        Matrix4 pad;CHECK(ThumbPadFrame(bones,14,out,1,3,.006f,pad));
        CHECK((out[4].translation-pad*Vector3()).length()<.0001f);
        CHECK(ModelMatrix(out[4]).getLeftAxis().dot(pad.getLeftAxis())>.999f);
        const auto first=out[4];f.reload.Pose(11,bones,out,14,3,1);
        CHECK((out[4].translation-first.translation).length()<.0001f); // Stereo cannot add curl/offset.
        // Release cancels the held shell; it no longer follows the thumb.
        f.at({.7f,.4f,-.1f});f.tick(false);for(int n=0;n<100;++n)f.reload.Draw();
        f.reload.Pose(11,bones,out,14,3,1);CHECK(out[4].scale==0);CHECK(f.ammo().loaded==2);
    }
    for(unsigned hand:{0u,1u}){
        // All reserve moved into a native-loaded clip: zero reserve must not
        // strand an absent mag. Failed writes/running native reloads retry,
        // without changing the physical state or duplicating a single round.
        Fixture f(ReloadWeaponKind::Rifle,0,60,hand);f.ammo().reserve=60;f.eject();
        f.ammo().loaded=60;f.ammo().reserve=0;f.ammo().nativeState=1;
        f.tick(false);CHECK(f.ammo().loaded==60);CHECK(f.ammo().reserve==0);CHECK(native.commits==1);
        f.ammo().nativeState=0;native.rejectCommit=true;
        f.tick(false);CHECK(f.ammo().loaded==60);CHECK(f.ammo().reserve==0);CHECK(f.reload.BlocksFire(11));
        native.rejectCommit=false;f.supplyGrab();CHECK(f.ammo().loaded==0);CHECK(f.ammo().reserve==60);
        f.insert();CHECK(f.ammo().loaded==60);CHECK(f.ammo().reserve==0);CHECK(f.reload.BlocksFire(11));
        f.rack();CHECK(!f.reload.BlocksFire(11));CHECK(native.commits==3);
    }
    { // Recovery while a preview is already held still inserts exactly once.
        Fixture f(ReloadWeaponKind::Rifle,53,60);f.eject();f.supplyGrab();
        f.ammo().loaded=60;f.ammo().reserve-=60;f.insert();
        CHECK(f.ammo().loaded==60);CHECK(f.ammo().reserve==93);CHECK(!f.reload.BlocksFire(11));
        for(int n=0;n<10;++n)f.tick(true);
        CHECK(native.commits==3);
        CHECK(std::count(cues.begin(),cues.end(),ReloadCue::Eject)==1);
        CHECK(std::count(cues.begin(),cues.end(),ReloadCue::Insert)==1);
    }
    { // Truly exhausted ammunition remains empty; new scavenged reserve can be grabbed.
        Fixture f(ReloadWeaponKind::Rifle,0,60);f.ammo().reserve=0;f.eject();f.at(f.supply());
        CHECK(!f.tick(true));CHECK(native.commits==1);CHECK(f.ammo().reserve==0);CHECK(f.reload.BlocksFire(11));
        CHECK(Logger::log.str().find("reserve=0 capacity=60 magazine=0")!=std::string::npos);
        f.ammo().reserve=15;f.supplyGrab();f.insert();f.rack();CHECK(f.ammo().loaded==15);CHECK(f.ammo().reserve==0);
    }
    { // Native-loaded rounds in an actually seated magazine are never moved.
        Fixture f(ReloadWeaponKind::Rifle,0,60);f.ammo().loaded=60;f.ammo().reserve=40;f.tick(false);
        CHECK(f.ammo().loaded==60);CHECK(f.ammo().reserve==40);CHECK(native.commits==0);
    }
    { // Menu/tracking suspension cancels only the transient offhand payload.
        Fixture f(ReloadWeaponKind::Rifle,0,60);f.eject();f.supplyGrab();f.reload.Suspend();f.relative(f.sockets().insert);f.tick(true);
        CHECK(f.ammo().loaded==0);CHECK(f.ammo().reserve==100);f.supplyGrab();f.insert();f.rack();CHECK(!f.reload.BlocksFire(11));
        Game::instance.enabled.value=false;f.tick(false);CHECK(!f.reload.BlocksNative(11));CHECK(!f.reload.BlocksFire(11));
        Game::instance.enabled.value=true;f.tick(false);CHECK(!f.reload.BlocksFire(11));
    }
    for(unsigned hand:{0u,1u})for(int ratio:{-3,1,2,3,4,9}){
        Fixture f(ReloadWeaponKind::Shotgun,0,12,hand);Game::instance.ratio.value=ratio;
        const int rounds=(std::max)(1,(std::min)(4,ratio));f.supplyGrab();f.insert();CHECK(f.ammo().loaded==rounds);CHECK(f.ammo().reserve==100-rounds);
        CHECK(f.reload.BlocksFire(11));f.rack();CHECK(!f.reload.BlocksFire(11));
        f.shot();CHECK(f.reload.BlocksFire(11));if(f.ammo().loaded==0){f.supplyGrab();f.insert();}f.rack();CHECK(!f.reload.BlocksFire(11));
    }
    { // Pump without releasing an existing two-hand hold; restoration supports the next shot/pump too.
        Fixture f(ReloadWeaponKind::Shotgun,8,12);f.relative(f.sockets().action);f.tick(true);CHECK(f.session.Items().Supported(11));
        for(int n=0;n<3;++n){
            f.shot();CHECK(f.reload.BlocksFire(11));CHECK(f.tick(true));CHECK(f.session.Items().Supported(11));
            f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);f.relative(f.sockets().action);CHECK(f.tick(true));
            CHECK(!f.reload.BlocksFire(11));CHECK(f.session.Items().Supported(11));
        }
    }
    for(unsigned hand:{0u,1u})for(int initiallyLoaded:{0,1}){
        Fixture f(ReloadWeaponKind::Rocket,initiallyLoaded,2,hand);f.cover();CHECK(f.reload.BlocksFire(11));
        for(int n=initiallyLoaded;n<2;++n){
            f.at(f.supply());f.tick(false);CHECK(!f.tick(true)); // Spent tube must come out first.
            f.removeRocket();f.supplyGrab();f.insert();
            CHECK(f.ammo().loaded==n+1);CHECK(f.ammo().reserve==100-(n+1-initiallyLoaded));
            CHECK(f.reload.BlocksFire(11));CHECK(f.frame.consumed[f.off]);
            for(int i=0;i<5;++i)f.tick(true);
            CHECK(f.ammo().loaded==n+1);
        }
        CHECK(std::count(cues.begin(),cues.end(),ReloadCue::Eject)==2-initiallyLoaded);
        f.cover();CHECK(!f.reload.BlocksFire(11)); // B closes only after BOTH insertions.
        f.shot();f.cover();f.removeRocket();f.supplyGrab();
        native.rejectCommit=true;f.insert();CHECK(f.ammo().loaded==1);CHECK(f.reload.BlocksFire(11));
        native.rejectCommit=false;CHECK(f.tick(true));CHECK(f.ammo().loaded==2);CHECK(f.reload.BlocksFire(11));
        f.cover();CHECK(!f.reload.BlocksFire(11));
    }
    for(unsigned hand:{0u,1u}){ // Cancel/reopen with one live round; no need to remove it.
        Fixture f(ReloadWeaponKind::Rocket,1,2,hand);f.cover();f.removeRocket();f.supplyGrab();
        f.at({1,1,1});f.tick(false);CHECK(f.ammo().loaded==1);CHECK(f.ammo().reserve==100);
        f.cover();CHECK(!f.reload.BlocksFire(11));f.cover();f.supplyGrab();f.insert();
        CHECK(f.ammo().loaded==2);CHECK(f.ammo().reserve==99);CHECK(f.reload.BlocksFire(11));f.cover();CHECK(!f.reload.BlocksFire(11));
    }
    for(unsigned hand:{0u,1u}){ // Both pieces use native rendering only in the correct pass.
        Fixture f(ReloadWeaponKind::Rocket,1,2,hand);native.rocketModel=true;f.reload.Deactivate();f.tick(false);
        f.cover();f.relative(f.sockets().insert);CHECK(f.tick(true));f.reload.Draw();CHECK(native.rocketDraws.empty());
        CHECK(Game::instance.inGameRenderer.triangles==96);f.reload.DrawNativePieces();CHECK(native.rocketDraws==std::vector<bool>{true});
        CHECK(std::fabs(native.rocketPoses.back().getLeftAxis().length()-1.f)<.0001f); // Native geometry is not halved.
        auto checkRocketPose=[&](){
            // Local +X of the rocket now points left (+Y) across the controller;
            // turning the controller carries this offset without orbiting its origin.
            const auto& pose=native.rocketPoses.back();const auto& handPose=f.hands[f.off];
            CHECK((pose.getLeftAxis()-handPose.getUpAxis()).length()<.0001f);
            CHECK((pose.getUpAxis()+handPose.getLeftAxis()).length()<.0001f);
            CHECK((pose.getForwardAxis()-handPose.getForwardAxis()).length()<.0001f);
            CHECK((pose*Vector3()-(Helpers::GetCamera().position+(handPose*Vector3()+handPose.getLeftAxis()*.018f-handPose.getForwardAxis()*.035f)*.5f)).length()<.0001f);
        };
        checkRocketPose();
        {const auto p=f.hands[f.off]*Vector3();f.hands[f.off]=Matrix4().rotateX(23).rotateY(35).rotateZ(71).translate(p);}
        f.tick(true);f.reload.DrawNativePieces();checkRocketPose();
        f.at({1,1,1});f.tick(false);for(int i=0;i<100;++i)f.reload.Draw();native.rocketDraws.clear();
        f.supplyGrab();f.reload.DrawNativePieces();CHECK(native.rocketDraws==std::vector<bool>{false});checkRocketPose();
        f.at({1,1,1});f.tick(false);native.rocketDraws.clear();f.reload.DrawNativePieces();CHECK(native.rocketDraws==std::vector<bool>{false}); // Dropped fresh round remains fresh.
        checkRocketPose();
        Helpers::menu=true;f.reload.DrawNativePieces();CHECK(native.rocketDraws.size()==1);Helpers::menu=false;
    }
    for(unsigned hand:{0u,1u}){ // Short, forgiving pump; backward-only/release still cannot chamber.
        Fixture f(ReloadWeaponKind::Shotgun,2,12,hand);f.shot();
        f.relative(f.sockets().action+Point{0,.16f,0});CHECK(f.tick(true)); // Wider foregrip capture.
        f.relative(f.sockets().action+Point{-.040f,.16f,0});f.tick(true);
        CHECK(f.reload.BlocksFire(11));
        f.relative(f.sockets().action+Point{-.056f,.16f,0});f.tick(true);
        CHECK(f.reload.BlocksFire(11)); // Must move forward after full rearward pull.
        f.relative(f.sockets().action+Point{-.025f,.16f,0});CHECK(f.tick(true));
        CHECK(!f.reload.BlocksFire(11));CHECK(f.ammo().loaded==1);CHECK(f.ammo().reserve==100);
    }
    for(unsigned hand:{0u,1u}){ // Rocket names taken directly from the supplied native log.
        Fixture f(ReloadWeaponKind::Rocket,1,2,hand);native.model=true;native.nodeCount=5;
        const char* names[]={"frame gun","frame body","frame foregrip","frame tubes","frame push_bar"};
        Bone bones[5];Transform out[5];
        const Vector3 parts[]={{},{.1f,0,0},{.2f,0,0},{.08f,0,.06f},{.12f,0,.06f}};
        for(unsigned i=0;i<5;++i){
            std::strcpy(native.nodes[i].name,names[i]);std::strcpy(bones[i].BoneName,names[i]);
            native.nodes[i].parent=bones[i].Parent=i?0:-1;
            native.nodes[i].inverseBind=ModelTransform(Matrix4().translate(-parts[i]));
        }
        f.reload.Deactivate();f.tick(false);
        const auto camera=Helpers::camera.position;
        Matrix4 gun=Matrix4().rotateZ(32).scale(1,hand==0?-1.f:1.f,1).translate(camera+(f.hands[f.main]*Vector3())*.5f);
        auto pose=[&](){for(unsigned i=0;i<5;++i)out[i]=ModelTransform(gun*Matrix4().translate(parts[i]));f.reload.Pose(11,bones,out,5,0);};
        pose();const auto closed=out[3].translation;
        f.cover();pose();
        CHECK((out[3].translation-closed-gun.getLeftAxis()*-.045f).length()<.00001f);
        CHECK((out[2].translation-gun*parts[2]).length()<.00001f); // Foregrip stays fixed.
        Matrix4 inverse=f.hands[f.main];const auto local=inverse.invertAffine()*((out[3].translation-camera)*2);
        f.relative({local.x,local.y,local.z});CHECK(f.tick(true));CHECK(f.frame.consumed[f.off]);
        f.at({1,1,1});CHECK(f.tick(false));f.supplyGrab();
        f.relative({local.x,local.y,local.z});CHECK(f.tick(true));CHECK(f.ammo().loaded==2);CHECK(f.ammo().reserve==99);
        CHECK(f.reload.BlocksFire(11));f.cover();CHECK(!f.reload.BlocksFire(11));pose();CHECK((out[3].translation-closed).length()<.00001f);
    }
    { // Ordinary support grabs, head gestures and unsupported native weapons retain their controls.
        Fixture f(ReloadWeaponKind::Rifle,20,60);f.relative({.35f,0,0});f.tick(true);CHECK(f.session.Items().Supported(11));
        f.tick(false);f.eject();f.at(f.supply());f.tick(true,false,true);CHECK(!f.frame.nearSupport[f.off]);CHECK(native.commits==1);
        f.ammo().kind=ReloadWeaponKind::Native;Helpers::controls.Reload=127;f.tick(false,true);CHECK(Helpers::controls.Reload==127);CHECK(!f.reload.BlocksNative(11));
    }
    { // Stroke discrimination: lateral motion/common hand motion and incomplete strokes cannot rack.
        Fixture f(ReloadWeaponKind::Sniper,0,4);f.eject();f.supplyGrab();f.insert();f.tick(false);f.relative(f.sockets().action);f.tick(true);
        f.relative(f.sockets().action+Point{0,.2f,0});f.tick(true);f.relative(f.sockets().action);f.tick(true);CHECK(f.reload.BlocksFire(11));
        f.relative(f.sockets().action+Point{-.04f,0,0});f.tick(true);f.relative(f.sockets().action);f.tick(true);CHECK(f.reload.BlocksFire(11));
        f.hands[f.main].translate(.5f,0,0);f.hands[f.off].translate(.5f,0,0);f.tick(true);CHECK(f.reload.BlocksFire(11));
        f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);f.relative(f.sockets().action);f.tick(true);CHECK(!f.reload.BlocksFire(11));
        const auto finishedCues=cues.size();
        f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);f.relative(f.sockets().action);f.tick(true);
        CHECK(cues.size()==finishedCues); // Required charging ends the latch, no repeated free strokes.
        f.tick(false);CHECK(!f.reload.BlocksFire(11));
    }
    { // Native reload already in progress at activation must finish; dry fire does not latch a pump.
        Fixture f(ReloadWeaponKind::Shotgun,8,12);f.ammo().nativeState=1;CHECK(!f.reload.BlocksNative(11));f.ammo().nativeState=0;
        f.reload.BeforeShot(11);f.reload.AfterShot(11);CHECK(!f.reload.BlocksFire(11));
        native.valid=false;CHECK(!f.reload.BlocksNative(11));f.tick(false);CHECK(!f.reload.BlocksFire(11));
    }
    { // Native names map across different model/animation indices. Detached magazine
      // descendants move together; neither gun nor either wrist can be moved/hidden.
        Fixture f(ReloadWeaponKind::Pistol,6,12);
        native.model=true;native.nodeCount=5;
        const char* names[]={"root","gun","magazine","round","slide"};
        const int parents[]={-1,0,1,2,1};
        const Vector3 rest[]={{},{},{0,0,-.04f},{0,0,-.03f},{.025f,.025f,.03f}};
        for(unsigned n=0;n<5;++n){
            auto& node=native.nodes[n];std::strcpy(node.name,names[n]);node.parent=static_cast<std::int16_t>(parents[n]);
            node.inverseBind=ModelTransform(Matrix4().translate(-rest[n]));
        }
        f.reload.Deactivate();f.tick(false);
        Bone bones[7];const char* animationNames[]={"root","l wrist","r wrist","gun","magazine","round","slide"};
        const int animationParents[]={-1,0,0,2,3,4,3};
        Transform out[7],baseline[7];
        for(unsigned n=0;n<7;++n){
            std::strcpy(bones[n].BoneName,animationNames[n]);bones[n].Parent=static_cast<std::int16_t>(animationParents[n]);
            baseline[n]=ModelTransform(Matrix4().translate(Helpers::camera.position+Vector3(.25f,0,-.05f)));
        }
        for(unsigned n=4;n<7;++n)baseline[n].translation+=rest[n-2];
        auto pose=[&](){std::copy(std::begin(baseline),std::end(baseline),std::begin(out));f.reload.Pose(11,bones,out,7,3);};
        pose();CHECK(out[6].translation==baseline[6].translation); // Bolt is at model rest.
        f.eject();for(int n=0;n<100;++n)f.reload.Draw();pose();
        CHECK(out[4].scale==0 && out[5].scale==0);
        for(unsigned n:{0u,1u,2u,3u})CHECK(std::memcmp(&out[n],&baseline[n],sizeof(Transform))==0);
        f.supplyGrab();pose();CHECK(out[4].scale!=0);CHECK(out[5].scale!=0);
        const auto handPos=f.hands[f.off]*Vector3(.045f,0,.035f);
        CHECK((out[4].translation-(Helpers::camera.position+handPos*.5f)).length()<.0001f);
        CHECK(std::fabs((out[5].translation-out[4].translation).length()-.01f)<.0001f);
        f.insert();pose();CHECK(out[4].translation==baseline[4].translation);CHECK(out[5].translation==baseline[5].translation);
        Game::instance.enabled.value=false;
        std::copy(std::begin(baseline),std::end(baseline),std::begin(out));out[6].translation.x+=.2f;
        const auto animated=out[6].translation;f.reload.Pose(11,bones,out,7,3);CHECK(out[6].translation==animated);
        CHECK(!Descendant(bones,7,1,3));CHECK(Descendant(bones,7,5,3));
    }
    for(auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Rifle,ReloadWeaponKind::Sniper})
    for(unsigned hand:{0u,1u})for(bool fast:{false,true}){
        // Full pull + release springs magazine-fed actions forward; even if the
        // threshold is first crossed in the release sample. No extra ammo added.
        Fixture f(kind,0,12,hand);f.eject();f.supplyGrab();f.insert();f.tick(false);
        f.relative(f.sockets().action);CHECK(f.tick(true));
        f.relative(f.sockets().action+Point{-.12f,0,0});
        if(!fast){f.tick(true);CHECK(f.reload.BlocksFire(11));}
        CHECK(f.tick(false));CHECK(!f.reload.BlocksFire(11));CHECK(f.ammo().loaded==12);CHECK(native.commits==2);
        CHECK(cues.back()==ReloadCue::Close);CHECK(!f.session.Items().Supported(11));
    }
    for(auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Shotgun}){
        Fixture f(kind,0,12);if(kind==ReloadWeaponKind::Pistol)f.eject();f.supplyGrab();f.insert();f.tick(false);
        f.relative(f.sockets().action);CHECK(f.tick(true));
        f.relative(f.sockets().action+Point{kind==ReloadWeaponKind::Shotgun?-.12f:-.04f,0,0});f.tick(true);f.tick(false);
        CHECK(f.reload.BlocksFire(11)); // Short pulls fail; pumps never spring shut.
        f.rack();CHECK(!f.reload.BlocksFire(11));
    }
    for(auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Rifle})for(unsigned hand:{0u,1u}){
        // Actual node names from the user's R1.9 log. A later slide_release must
        // never override slide, and ophandle must be recognized on the rifle.
        Fixture f(kind,0,12,hand);native.model=true;native.nodeCount=4;
        const char* names[]={"frame gun","frame magazine",kind==ReloadWeaponKind::Pistol?"frame slide":"frame ophandle","frame slide_release"};
        const Vector3 rest[]={{},{0,0,-.045f},{.04f,.03f,.035f},{.01f,.4f,0}};
        for(unsigned n=0;n<4;++n){
            auto& node=native.nodes[n];std::strcpy(node.name,names[n]);node.parent=n==0?-1:0;
            node.inverseBind=ModelTransform(Matrix4().translate(-rest[n]));
        }
        Bone bones[11];const char* animationNames[]={"root","bip01 l wrist","bip01 r wrist","frame gun","frame magazine",names[2],names[3],
            "bip01 l finger1","bip01 l finger11","bip01 l finger12","bip01 r finger1"};
        const int parents[]={-1,0,0,2,3,3,3,1,7,8,2};
        for(unsigned n=0;n<11;++n){std::strcpy(bones[n].BoneName,animationNames[n]);bones[n].Parent=static_cast<std::int16_t>(parents[n]);}
        native.handCount=4;
        for(unsigned n=0;n<4;++n){
            auto& node=native.hands[n];std::strcpy(node.name,animationNames[n==0?1:n+6]);node.parent=static_cast<std::int16_t>(n)-1;
            node.inverseBind=ModelTransform(Matrix4().translate(-.015f*n,0,0));
        }
        f.reload.Deactivate();f.tick(false);
        Transform out[11],baseline[11];
        const float mirror=hand==0?-1.f:1.f;
        auto pose=[&](){
            const auto cam=Helpers::camera.position;
            for(unsigned n=0;n<11;++n)baseline[n]=ModelTransform(Matrix4().translate(cam));
            baseline[1]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.off]*Vector3())*.5f));
            baseline[2]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.main]*Vector3())*.5f));
            const Matrix4 gun=ModelMatrix(baseline[2]);baseline[3]=baseline[2];
            for(unsigned n=1;n<4;++n)baseline[n+3]=ModelTransform(gun*Matrix4().translate(rest[n]));
            for(unsigned n=7;n<10;++n)baseline[n]=ModelTransform(ModelMatrix(baseline[1])*Matrix4().translate(.015f*(n-6),0,0));
            baseline[10]=ModelTransform(ModelMatrix(baseline[2])*Matrix4().translate(.02f,0,0));
            std::copy(std::begin(baseline),std::end(baseline),std::begin(out));
            f.reload.Pose(11,bones,out,11,3,1);
            for(unsigned n:{0u,2u,3u,6u,10u})CHECK(std::memcmp(&out[n],&baseline[n],sizeof(Transform))==0);
        };
        pose();f.eject();f.supplyGrab();pose();
        CHECK((out[8].translation-out[7].translation).length()>.0149f);
        CHECK((out[8].translation-out[7].translation).length()<.0151f);
        CHECK(std::fabs(out[8].translation.y-out[7].translation.y)>.004f); // Actual native fingers curl.
        const auto firstFinger=out[9];pose();CHECK(std::memcmp(&firstFinger,&out[9],sizeof(Transform))==0); // No stereo/frame accumulation.
        f.relative({0,0,-.09f});f.tick(true);CHECK(f.ammo().loaded==12);f.tick(false);pose();
        CHECK(std::memcmp(&out[8],&baseline[8],sizeof(Transform))==0); // Release restores native animation.
        const Point action{.08f,.06f*mirror,.07f};
        f.relative(action+Point{0,.07f,0});CHECK(f.tick(true));CHECK(!f.session.Items().Supported(11));pose();
        const Vector3 wristOffset=kind==ReloadWeaponKind::Rifle?Vector3(-.045f,.095f*mirror,.025f):Vector3(-.012f,.045f*mirror,.026f);
        Vector3 palm=out[1].translation-wristOffset*.5f;
        CHECK((palm-out[5].translation).length()<.0001f); // Snaps to selected part despite grab margin.
        const auto snappedWrist=out[1];
        f.relative(action+Point{-.12f,.14f,.05f});{const auto pos=f.hands[f.off]*Vector3();f.hands[f.off]=Matrix4().rotateZ(60).translate(pos);}f.tick(true);pose();
        for(unsigned r=0;r<9;++r)CHECK(std::fabs(out[1].rotation[r]-snappedWrist.rotation[r])<.0001f);
        palm=out[1].translation-wristOffset*.5f;
        CHECK((palm-out[5].translation).length()<.0001f); // Lateral tracker motion cannot peel hand off slide.
        const float distance=kind==ReloadWeaponKind::Pistol?.04f:.065f;
        CHECK(std::fabs(out[5].translation.x-(baseline[5].translation.x-distance*.5f))<.0001f);
        CHECK(f.tick(false));CHECK(!f.reload.BlocksFire(11));pose();
        CHECK(std::memcmp(&out[1],&baseline[1],sizeof(Transform))==0);
        CHECK(out[5].translation==baseline[5].translation);
        CHECK(Logger::log.str().find("action grab weapon=11 node=2")!=std::string::npos);
        f.ammo().loaded=0;f.relative(action+Point{.02f,.02f,0});
        {const auto pos=f.hands[f.off]*Vector3();f.hands[f.off]=Matrix4().rotateZ(-80).translate(pos);}
        f.tick(false);CHECK(f.tick(true));pose();
        CHECK((out[1].translation-snappedWrist.translation).length()<.0001f);
        for(unsigned r=0;r<9;++r)CHECK(std::fabs(out[1].rotation[r]-snappedWrist.rotation[r])<.0001f);

    }

    for(unsigned hand:{0u,1u}){
        Fixture f(ReloadWeaponKind::Shotgun,2,12,hand);pumpEffects=0;
        CHECK(f.reload.DeferPumpSound(11,104));CHECK(!f.reload.DeferPumpSound(11,999));
        CHECK(f.reload.DeferPumpEffect(11,200));CHECK(!f.reload.DeferPumpEffect(22,200));
        f.ammo().nativeState=3;CHECK(f.reload.DeferPumpSound(11,104));CHECK(f.reload.DeferPumpEffect(11,200));f.ammo().nativeState=0;
        f.shot();CHECK(pumpEffects==0);f.tick(false);f.relative(f.sockets().action);f.tick(true);
        f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);CHECK(pumpEffects==1);
        f.tick(true);f.tick(false);f.rack();CHECK(pumpEffects==1); // Regrip cannot duplicate the casing.
        f.shot();CHECK(f.ammo().loaded==0);f.rack();CHECK(pumpEffects==2);CHECK(f.reload.BlocksFire(11));
        f.supplyGrab();f.insert();f.tick(false);f.rack();CHECK(pumpEffects==2);CHECK(!f.reload.BlocksFire(11));
        Game::instance.enabled.value=false;CHECK(!f.reload.DeferPumpEffect(11,200));CHECK(!f.reload.DeferPumpSound(11,104));
    }
    { // Actual anatomical names, native firing-hand grasp, both physical hands.
        Bone bones[8];const char* names[]={"root","bip01 l wrist","bip01 r wrist","frame gun",
            "bip01 l index0","bip01 l index1","bip01 r index0","bip01 r index1"};
        const int parents[]={-1,0,0,2,1,4,2,6};ModelNode rest[8]{};
        for(unsigned i=0;i<8;++i){std::strcpy(bones[i].BoneName,names[i]);bones[i].Parent=parents[i];
            std::strcpy(rest[i].name,names[i]);rest[i].parent=parents[i];rest[i].inverseBind=ModelTransform(Matrix4());}
        rest[5].inverseBind=rest[7].inverseBind=ModelTransform(Matrix4().translate(-.02f,0,0));
        for(float mirror:{-1.f,1.f}){
            Transform out[8];for(auto& t:out)t=ModelTransform(Matrix4());
            out[1]=ModelTransform(Matrix4().scale(1,mirror,1).translate(1,2,3));
            out[2]=ModelTransform(Matrix4().scale(1,mirror,1).translate(4,5,6));
            out[4]=out[1];out[5]=ModelTransform(ModelMatrix(out[1])*Matrix4().translate(.02f,0,0));
            out[6]=ModelTransform(ModelMatrix(out[2])*Matrix4().rotateZ(80));
            out[7]=ModelTransform(ModelMatrix(out[6])*Matrix4().translate(.02f,0,0));
            const auto source=out[7];CloseReloadHand(bones,8,out,1,3,rest,8);
            CHECK(std::memcmp(&out[7],&source,sizeof(source))==0);
            CHECK(std::fabs(out[5].translation.x-out[4].translation.x)<.013f);
            CHECK(std::fabs(out[5].translation.y-out[4].translation.y)>.015f);
        }
    }
    for(auto kind:{ReloadWeaponKind::Pistol,ReloadWeaponKind::Shotgun})for(unsigned hand:{0u,1u}){
        Fixture f(kind,kind==ReloadWeaponKind::Shotgun?2:0,12,hand);native.model=true;native.nodeCount=4;
        const char* names[]={"frame gun",kind==ReloadWeaponKind::Shotgun?"frame bullet":"frame magazine",
            kind==ReloadWeaponKind::Shotgun?"frame pump":"frame slide","frame magazine port"};
        const Vector3 part[]={{},{0,0,.02f},{.18f,0,0},{.04f,0,.01f}};
        Bone bones[7];const int parents[]={-1,0,0,2,3,3,3};
        std::strcpy(bones[0].BoneName,"root");std::strcpy(bones[1].BoneName,"bip01 l wrist");std::strcpy(bones[2].BoneName,"bip01 r wrist");
        for(unsigned n=0;n<7;++n)bones[n].Parent=parents[n];
        for(unsigned n=0;n<4;++n){std::strcpy(bones[n+3].BoneName,names[n]);std::strcpy(native.nodes[n].name,names[n]);
            native.nodes[n].parent=n==0?-1:0;native.nodes[n].inverseBind=ModelTransform(Matrix4().translate(-part[n]));}
        f.reload.Deactivate();f.tick(false);
        const auto cam=Helpers::camera.position;
        const Matrix4 gun=Matrix4().rotateZ(24).scale(1,hand==0?-1.f:1.f,1).translate(cam+(f.hands[f.main]*Vector3())*.5f);
        const Matrix4 clean=gun*Matrix4().translate(.17f,.025f,-.035f);
        Transform out[7];
        auto pose=[&](bool animated=false){
            for(auto& t:out)t=ModelTransform(Matrix4().translate(cam));
            out[1]=ModelTransform(animated?Matrix4(clean).translate(0,.1f,0):clean);
            out[2]=out[3]=ModelTransform(gun);
            for(unsigned n=1;n<4;++n)out[n+3]=ModelTransform(gun*Matrix4().translate(part[n]));
            if(kind==ReloadWeaponKind::Shotgun)out[4].scale=0; // Hidden native shell becomes visible while held.
            f.reload.Pose(11,bones,out,7,3,1);
        };
        Game::instance.bUseTwoHandAim=true;pose();
        auto controllerPoint=[&](Vector3 world){Matrix4 inv=f.hands[f.main];const auto p=inv.invertAffine()*((world-cam)*2);return Point{p.x,p.y,p.z};};
        if(kind==ReloadWeaponKind::Shotgun){
            f.supplyGrab();pose();CHECK(out[4].scale!=0);const auto palm=f.hands[f.off]*Vector3(.045f,0,0);
            CHECK((out[4].translation-(cam+palm*.5f)).length()<.00001f);
            const auto port=controllerPoint(gun*part[3])+Point{0,0,.065f};
            f.relative(port+Point{0,.15f,0});CHECK(f.tick(true));CHECK(f.ammo().loaded==3);f.tick(false);pose();CHECK(out[4].scale==0);
            f.shot();
        }else{f.eject();f.supplyGrab();const auto port=controllerPoint(gun*part[1]);f.relative(port);f.tick(true);f.tick(false);}
        const auto action=controllerPoint(gun*part[2]);f.relative(action);f.tick(false);CHECK(f.tick(true));pose(true);
        f.relative(action+Point{-.12f,0,0});f.tick(true);pose(true);
        const Vector3 delta=out[5].translation-gun*part[2];const auto rail=gun.getLeftAxis();
        const float travel=kind==ReloadWeaponKind::Pistol?.04f:.09f;
        CHECK((delta-rail*(-travel*.5f)).length()<.00001f); // No sideways slide/pump drift with an angled model.
        if(kind==ReloadWeaponKind::Shotgun)CHECK((out[1].translation-(clean*Vector3()+delta)).length()<.00001f);
        Game::instance.bUseTwoHandAim=false;
    }
    for(unsigned hand:{0u,1u}){
        Fixture f(ReloadWeaponKind::Pistol,6,12,hand);
        f.relative({0,.06f,-.02f});CHECK(!f.tick(true));CHECK(f.session.Items().Supported(11));
        CHECK(!f.reload.BlocksFire(11));const auto ammo=f.ammo();
        // Hold the same grip and deliberately move from the handle to the slide.
        f.relative(f.sockets().action);CHECK(f.tick(true));CHECK(f.frame.consumed[f.off]);
        CHECK(!f.session.Items().Supported(11));CHECK(f.reload.BlocksFire(11));
        f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);CHECK(f.tick(false));
        CHECK(!f.reload.BlocksFire(11));CHECK(f.ammo().loaded==ammo.loaded);CHECK(native.commits==0);
        // A handle press must not steal the slide, including an empty weapon.
        f.ammo().loaded=0;f.relative({0,.05f,-.02f});CHECK(!f.tick(true));CHECK(f.session.Items().Supported(11));
    }
    for(unsigned hand:{0u,1u}){
        Fixture f(ReloadWeaponKind::Pistol,5,12,hand);f.eject();f.supplyGrab();
        f.relative(f.sockets().insert+Point{.24f,0,-.25f});f.tick(true);CHECK(f.ammo().loaded==0);
        // Controller remains >19 cm from the well, allowing physical clearance.
        f.relative(f.sockets().insert+Point{.14f,0,-.13f});CHECK(f.tick(true));
        CHECK(f.ammo().loaded==12);CHECK(native.commits==2);CHECK(!f.reload.BlocksFire(11));
        f.tick(true);CHECK(native.commits==2); // Holding nearby cannot insert twice.
    }
    for(unsigned hand:{0u,1u}){
        Fixture f(ReloadWeaponKind::Pistol,6,12,hand);
        f.relative(f.sockets().action+Point{.13f,.09f,0});CHECK(f.tick(true));
        CHECK(f.reload.BlocksFire(11));CHECK(!f.session.Items().Supported(11));
        f.relative(f.sockets().action+Point{.01f,.09f,0});f.tick(true);f.tick(false);
        CHECK(!f.reload.BlocksFire(11));CHECK(f.ammo().loaded==6);CHECK(native.commits==0);
    }
    for(unsigned hand:{0u,1u})for(bool supported:{false,true}){
        Fixture f(ReloadWeaponKind::Pistol,6,12,hand);
        if(supported){f.relative({.10f,.015f,-.04f});f.tick(true);CHECK(f.session.Items().Supported(11));}
        // The controller is below/outside the slide zone while the visible
        // palm is over it. Simulate real tracked skin BEFORE support snapping.
        f.relative(f.sockets().action+Point{-.16f,.08f,-.05f});f.tick(supported);
        Bone bones[6]{};Transform out[6];
        const char* names[]={"root","frame l wriste","frame r wriste","frame gun","frame l index low","frame l pinky low"};
        const int parents[]={-1,0,0,2,1,1};
        const float mirror=hand==0?-1.f:1.f;
        for(unsigned i=0;i<6;++i){std::strcpy(bones[i].BoneName,names[i]);bones[i].Parent=parents[i];out[i]=ModelTransform(Matrix4());}
        const auto cam=Helpers::camera.position;
        out[1]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.off]*Vector3())*.5f));
        out[2]=out[3]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.main]*Vector3())*.5f));
        out[4]=ModelTransform(ModelMatrix(out[1])*Matrix4().translate(.03f,-.01f,.02f));
        out[5]=ModelTransform(ModelMatrix(out[1])*Matrix4().translate(.03f,.01f,.02f));
        f.reload.Pose(11,bones,out,6,3,1);
        CHECK(f.tick(true));CHECK(f.reload.BlocksFire(11));CHECK(!f.session.Items().Supported(11));
        CHECK(f.ammo().loaded==6);CHECK(native.commits==0);
        // Releasing restores firing; no ammunition was manufactured or spent.
        f.tick(false);CHECK(!f.reload.BlocksFire(11));
    }
    for(auto kind:{ReloadWeaponKind::Rifle,ReloadWeaponKind::Sniper})for(unsigned hand:{0u,1u}){
        Fixture f(kind,0,60,hand);native.model=true;native.nodeCount=3;
        const char* modelNames[]={"frame gun","frame magazine","frame ophandle"};
        const Vector3 positions[]={{},{0,0,-.04f},{.06f,.025f,.035f}};
        for(unsigned i=0;i<3;++i){std::strcpy(native.nodes[i].name,modelNames[i]);native.nodes[i].parent=i?0:-1;
            native.nodes[i].inverseBind=ModelTransform(Matrix4().translate(-positions[i]));}
        f.reload.Deactivate();f.tick(false);
        Bone bones[8]{};Transform out[8];
        const char* names[]={"root","frame l wriste","frame r wriste","frame gun","frame magazine","frame ophandle","frame l index low","frame l pinky low"};
        const int parents[]={-1,0,0,2,3,3,1,1};
        for(unsigned i=0;i<8;++i){std::strcpy(bones[i].BoneName,names[i]);bones[i].Parent=parents[i];}
        native.handCount=8;
        for(unsigned i=0;i<8;++i){
            std::strcpy(native.hands[i].name,names[i]);native.hands[i].parent=parents[i];
            native.hands[i].inverseBind=ModelTransform(Matrix4().translate(i==6?Vector3(-.03f,.012f,0):i==7?Vector3(-.03f,-.012f,0):Vector3()));
        }
        f.reload.Deactivate();f.tick(false);
        const float mirror=hand==0?-1.f:1.f;
        auto pose=[&](){
            const auto cam=Helpers::camera.position;
            for(auto& t:out)t=ModelTransform(Matrix4());
            out[1]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.off]*Vector3())*.5f));
            out[2]=out[3]=ModelTransform(Matrix4().scale(1,mirror,1).translate(cam+(f.hands[f.main]*Vector3())*.5f));
            out[4]=ModelTransform(ModelMatrix(out[3])*Matrix4().translate(positions[1]));
            out[5]=ModelTransform(ModelMatrix(out[3])*Matrix4().translate(positions[2]));
            out[6]=ModelTransform(ModelMatrix(out[1])*Matrix4().translate(.03f,-.012f,0));
            out[7]=ModelTransform(ModelMatrix(out[1])*Matrix4().translate(.03f,.012f,0));
            const auto gun=out[3];f.reload.Pose(11,bones,out,8,3,1);CHECK(std::memcmp(&gun,&out[3],sizeof(gun))==0);
        };
        pose();f.eject();f.supplyGrab();pose();
        Matrix4 palm;CHECK(LeftPalmFrame(bones,8,out,1,palm));
        const auto magPoint=Matrix4(palm).invertAffine()*out[4].translation;
        CHECK((magPoint-Vector3(0,0,kind==ReloadWeaponKind::Sniper?-.0125f:-.030f)).length()<.0001f);
        if(kind==ReloadWeaponKind::Sniper){
            CHECK(palm.getForwardAxis().z<-.999f); // Palm up; back of hand down.
            CHECK(palm.getLeftAxis().x>.999f); // Fingers forward.
            CHECK(ModelMatrix(out[4]).getForwardAxis().dot(-palm.getForwardAxis())>.999f); // Magazine feed end above the palm.
        }
        f.relative({0,0,-.08f});f.tick(true);f.tick(false);
        f.relative({.12f,.05f*mirror,.07f});CHECK(f.tick(true));pose();
        CHECK(LeftPalmFrame(bones,8,out,1,palm));
        CHECK(palm.getLeftAxis().x>.999f);CHECK(palm.getForwardAxis().z>.999f); // Fingers forward, back up.
        CHECK((palm*Vector3()-out[5].translation-Vector3(0,(kind==ReloadWeaponKind::Sniper?.0175f:.0475f)*mirror,.0125f)).length()<.0001f);
    }
    for(unsigned hand:{0u,1u})for(int loaded:{0,6}){
        // Free slides work without an empty reload, never duplicate/discard ammo,
        // and prevent shooting while either hand is holding the moving slide.
        Fixture f(ReloadWeaponKind::Pistol,loaded,12,hand);
        const auto original=f.ammo();f.relative(f.sockets().action);CHECK(f.tick(true));
        CHECK(f.reload.BlocksFire(11));
        f.relative(f.sockets().action+Point{-.04f,0,0});f.tick(true);f.tick(false);
        CHECK(f.reload.BlocksFire(11)==(loaded==0));CHECK(f.ammo().loaded==original.loaded);CHECK(native.commits==0);
        f.relative(f.sockets().action);f.tick(true);
        for(int cycle=0;cycle<3;++cycle){
            f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);
            f.relative(f.sockets().action);f.tick(true);CHECK(f.reload.BlocksFire(11));
        }
        f.tick(false);CHECK(f.reload.BlocksFire(11)==(loaded==0));
        CHECK(f.ammo().loaded==original.loaded);CHECK(f.ammo().reserve==original.reserve);CHECK(native.commits==0);
        f.eject();f.relative(f.sockets().action);CHECK(f.tick(true));
        f.relative(f.sockets().action+Point{-.12f,0,0});f.tick(true);f.tick(false);
        CHECK(f.reload.BlocksFire(11));CHECK(f.ammo().loaded==0); // A missing magazine stays missing.
    }
    std::cout<<checks<<" production manual-reload interaction, ammo and drawing checks passed (engine/VR doubles)\n";
}
