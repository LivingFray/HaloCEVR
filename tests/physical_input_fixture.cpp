// run.py inserts the real production InputHandler physical-weapon methods.
#include "HaloCEVR/Immersion/WeaponSession.h"
#include "HaloCEVR/Immersion/Recoil.h"
#include "HaloCEVR/Maths/Matrices.h"
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <chrono>
using InputBindingID=unsigned long long;
enum class ControllerRole { Left,Right };
struct HaloID {std::uint16_t index,id;};
template<class T> struct Property {T value{};T Value() const{return value;}};
struct VRDouble {
    bool tracked[2]{true,true},valid=true;
    bool buttons[10]{};
    Vector3 wristOffset{};Matrix4 handRotation;
    Matrix4 head;
    Vector3 hand[2]{{0,0,-0.6f},{0,0,-0.6f}};
    unsigned haptics[2]{};
    bool IsControllerTracked(ControllerRole h) const{return tracked[static_cast<unsigned>(h)];}
    bool TryGetBoolInput(InputBindingID id,bool& down) const{down=buttons[id];return valid;}
    Matrix4 GetHMDTransform() const{return head;}
    Matrix4 GetRawControllerTransform(ControllerRole h) const{return Matrix4().translate(hand[static_cast<unsigned>(h)]);}
    Matrix4 GetControllerTransform(ControllerRole h,bool) const{return Matrix4(handRotation).translate(hand[static_cast<unsigned>(h)]+wristOffset);}
    Vector3 GetControllerVelocity(ControllerRole) const{return {1,0,0};}
    void TriggerHapticVibration(ControllerRole h,float,float,float,float){++haptics[static_cast<unsigned>(h)];}
};
struct Engine : Immersion::WeaponEngine {
    Immersion::NativeInventory state{100,0x10000,50,{11,22,Immersion::None,Immersion::None},11};
    bool resumable=true; bool ReadMayResume()const{return resumable;}
    bool valid=true,rejectSelect=false;unsigned drops=0;Immersion::DropPose droppedPose;
    bool Initialize(){return true;}
    bool SynchronizeViewModel(){return true;}
    bool InstallPickupGuard(){return true;}
    bool InstallVisualGuards(){return true;}
    bool pickupEnabled=false;Immersion::Object target=Immersion::None;
    void SetPickupEnabled(bool enabled){pickupEnabled=enabled;}
    Immersion::Object PickupTarget()const{return pickupEnabled?target:Immersion::None;}
    bool Pickup(Immersion::Object unit,Immersion::Object weapon)override{
        if(unit!=state.unit||weapon!=target||state.current!=Immersion::None)return false;
        auto slot=std::find(state.weapons.begin(),state.weapons.end(),Immersion::None);
        if(slot==state.weapons.end())return false;
        *slot=weapon;state.current=weapon;target=Immersion::None;return true;
    }
    const char* ReadStatus()const{return "Native context unavailable: stock controls.";}
    bool Read(Immersion::NativeInventory& out) const override{out=state;return valid;}
    bool ReadFireState(Immersion::Object id,Immersion::FireState& out)const {
        out={};out.unit=state.unit;out.weapon=id;
        return valid && id!=Immersion::None && state.current==id;
    }
    bool Select(Immersion::Object unit,Immersion::Object weapon) override {
        if(rejectSelect || unit!=state.unit || (weapon!=Immersion::None && !state.Contains(weapon)))return false;
        state.current=weapon;return true;
    }
    Immersion::NativeDrop Drop(Immersion::Object unit,Immersion::Object weapon,const Immersion::DropPose& pose) override {
        if(unit!=state.unit || weapon!=state.current)return Immersion::NativeDrop::Rejected;
        ++drops;droppedPose=pose;for(auto& w:state.weapons)if(w==weapon)w=Immersion::None;
        state.current=Immersion::None;return Immersion::NativeDrop::WorldObject;
    }
};
class InputHandler;
struct InteractionDouble {
    template<class T>void AttachEngine(T&){}
    unsigned GrenadeHandMask()const{return 0;}
    void SuspendGrenades(){}
    template<class... T>void Grenades(T&&...){}
    void GrenadeControls(){}
    bool VehicleActive()const{return false;}
};
struct Game {
    static Game instance;
    InteractionDouble interactions;
    Property<bool> enabled{true},markers{false},layout{false},grenades{true};
    Property<bool>* c_PhysicalGrenades=&grenades;
    Property<float> shoulder{0.20f},hip{0.16f};
    Property<bool>* c_PhysicalWeapons=&enabled;
    Property<bool>* c_PhysicalHolsterMarkers=&markers;
    Property<bool>* c_LeftHanded=&layout;
    Property<float>* c_PhysicalShoulderRadius=&shoulder;
    Property<float>* c_PhysicalHipRadius=&hip;
    bool bUse3DOFAiming=false,bNeedsRecentre=false,bLeftHanded=false,bUseTwoHandAim=false,bCompactSupportGrip=false;
    VRDouble vr;
    InputHandler* input=nullptr;
    VRDouble* GetVR(){return &vr;}
    float MetresToWorld(float value)const{return value*0.5f;}
    Matrix4 GetHeldWeaponTransform()const;
    bool longGrip=true;float supportAdvance=0,supportLift=0;
    float SupportGripForgiveness(Immersion::Object)const{return supportForgiveness;}
    float supportForgiveness=0;
    float SupportGripLift(Immersion::Object)const{return supportLift;}
    float SupportGripAdvance(Immersion::Object)const{return supportAdvance;}
    bool UsesLongSupportGrip(Immersion::Object)const{return longGrip;}
    // INSERT_PRODUCTION_RELOAD_ACCESSORS
    void SetPumpSupportedForTest(bool value){reloadHandler.pumpSupported=value;}
    void SuspendManualReload(){}
    void DeactivateManualReload(){}
    bool reloadDown=false;bool reloadValid=false;
    bool ProcessManualReload(Engine&,Immersion::WeaponSession&,Immersion::WeaponFrame&,const Matrix4*,Immersion::Point,float,bool down,bool valid){reloadDown=down;reloadValid=valid;return false;}
    struct Renderer {template<class... Args>void DrawPolygon(Args...) {}} inGameRenderer;
protected:
    // Match Game.h: production callers must use its public forwarding methods.
    struct ReloadDouble {
        bool pumpSupported=false;
        bool PumpSupportsWeapon()const{return pumpSupported;}
        bool DeferPumpSound(Immersion::Object,Immersion::Object)const{return false;}
        bool DeferPumpEffect(Immersion::Object,Immersion::Object)const{return false;}
    } reloadHandler;
};
Game Game::instance;
namespace Logger {std::ostringstream log;}
namespace Helpers {
    bool loading=false,menu=false;
    struct Cutscene{bool bInCutscene=false;} cutscene;
    struct Camera{Vector3 position{10,20,30};} camera;
    struct Controls{unsigned char Fire=0,Interact=0,PickupWeapon=0,Jump=0,Grenade=0,SwitchGrenades=0,Reload=0,SwitchWeapons=0,Melee=0;} controls;
    bool IsCampaignLoading(){return loading;}
    bool IsMouseVisible(){return menu;}
    Cutscene* GetCutsceneData(){return &cutscene;}
    Camera& GetCamera(){return camera;}
    Controls& GetControls(){return controls;}
}
#define D3DCOLOR_ARGB(a,r,g,b) 0u
class InputHandler {
public:
    void InitializePhysicalWeapons();
    void ReportPhysicalStatus(const char*);
    void UpdatePhysicalButtons();
    Immersion::Button physicalGrenadeCycle;
    bool physicalCycleArmed=false,menuChordUsed=false,menuRightTriggerBlocked=false;
    std::string physicalStatus;
    bool ShouldBlockPhysicalFire(const HaloID&)const;
    bool PhysicalRecoilState(const HaloID&,Immersion::FireState&,bool&)const;
    bool GetSupportOnlyTransform(Matrix4&)const;
    bool UpdatePhysicalWeapons(bool);
    void UpdateRegisteredInputs(){++registrations;}
    bool physicalWeaponsEnabled=false,physicalControlsActive=false,physicalFrameValid=false;
    bool physicalCanFire=false,physicalTriggerReady=false;
    bool physicalInputWarning=false;
    std::chrono::steady_clock::time_point lastPhysicalInputUpdate{};
    Engine physicalEngine;
    Immersion::WeaponSession physicalSession;
    std::array<Immersion::Button,2> physicalGripButtons;
    Immersion::HolsterHover holsterHover;
    Matrix4 supportRelativeTransform,lastHeldTransform;
    bool haveHeldTransform=false,haveHolsterYaw=false;
    float holsterYaw=0;
    InputBindingID PhysicalLeftGrip=1,PhysicalRightGrip=2,PhysicalLeftTrigger=3,PhysicalRightTrigger=4;
    bool recentreTap=false;
    InputBindingID PhysicalJump=5,PhysicalGrenade=6,PhysicalGrenadeCycle=7;
    InputBindingID Reload=0;
    std::array<bool,2> headGripConsumed{};
    unsigned registrations=0;
};
Matrix4 Game::GetHeldWeaponTransform()const{
    Matrix4 result;
    if(input && input->GetSupportOnlyTransform(result))return result;
    return vr.GetControllerTransform(bLeftHanded?ControllerRole::Left:ControllerRole::Right,true);
}
// INSERT_PRODUCTION_METHODS
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
void tick(InputHandler& input){input.physicalControlsActive=input.UpdatePhysicalWeapons(false);}
void initialize(InputHandler& input,bool left=false){
    auto& g=Game::instance;g.vr={};g.input=&input;g.layout.value=left;g.bLeftHanded=left;g.bUseTwoHandAim=false;
    input.InitializePhysicalWeapons();tick(input);CHECK(input.physicalSession.Active());CHECK(input.physicalSession.Held()==Immersion::None);
}
void draw(InputHandler& input,unsigned hand,unsigned slot=1){
    auto& g=Game::instance;auto p=Immersion::Layout{g.layout.value}.Offset(slot==0?Immersion::Zone::Shoulder:Immersion::Zone::WeaponHip);
    g.vr.hand[hand]={p.x,p.y,p.z};g.vr.buttons[hand+1]=true;tick(input);
}
int main(){
    CHECK(!Game::instance.ManualPumpSupportsWeapon());
    Game::instance.SetPumpSupportedForTest(true);CHECK(Game::instance.ManualPumpSupportsWeapon());
    Game::instance.SetPumpSupportedForTest(false);CHECK(!Game::instance.ManualPumpSupportsWeapon());
    for(bool longGun:{false,true})for(unsigned main:{0u,1u}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;g.longGrip=longGun;
        g.vr.hand[main]={.3f,0,-.1f};
        for(const auto distance:{-.08f,.09f,.16f,.30f,.55f,.80f}){
            g.vr.buttons[2-main]=false;tick(input);
            g.vr.hand[1-main]={.3f+distance,0,-.1f};g.vr.buttons[2-main]=true;tick(input);
            const bool expect=longGun ? (distance>=.20f && distance<=.65f) : (distance>=-.03f && (distance-.10f)*(distance-.10f)+.075f*.075f<=.16f*.16f);
            CHECK(g.bUseTwoHandAim==(expect && longGun));
            CHECK(g.bCompactSupportGrip==(expect && !longGun));
        }
        g.longGrip=true;
    }

    for(unsigned main:{0u,1u})for(float radius:{0.f,.04f}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;
        g.longGrip=true;g.supportForgiveness=radius;g.vr.hand[main]={.3f,0,-.1f};
        g.vr.hand[1-main]={.49f,.16f,-.1f};g.vr.buttons[2-main]=true;tick(input);
        CHECK(g.bUseTwoHandAim==(radius>.0f));
        g.supportForgiveness=0;
    }
    for(unsigned main:{0u,1u})for(float advance:{.08f,.12f}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;
        g.longGrip=true;g.supportAdvance=advance;g.vr.hand[main]={.3f,0,-.1f};
        for(float distance:{.21f,.25f,.30f,.36f,.65f,.72f,.79f}){
            g.vr.buttons[2-main]=false;tick(input);
            g.vr.hand[1-main]={.3f+distance,0,-.1f};g.vr.buttons[2-main]=true;tick(input);
            CHECK(g.bUseTwoHandAim==(distance>=.20f+advance && distance<=.65f+advance));
        }
        g.supportAdvance=0;
    }

    for(unsigned main:{0u,1u}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;
        g.supportAdvance=.12f;g.supportLift=.10f;g.longGrip=true;g.vr.hand[main]={.3f,0,-.1f};
        for(float height:{-.15f,-.03f,0.f,.04f,.10f,.20f,.30f}){
            g.vr.buttons[2-main]=false;tick(input);
            g.vr.hand[1-main]={.8f,0,-.1f+height};g.vr.buttons[2-main]=true;tick(input);
            CHECK(g.bUseTwoHandAim==(height>=.02f && height<=.24f));
        }
        g.supportLift=g.supportAdvance=0;
    }

    for(unsigned main:{0u,1u})for(float yaw:{0.f,90.f}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;
        g.longGrip=false;
        // Real wrist/controller origins differ. Compact support must still be
        // ahead/below the physical firing controller, never behind its wrist.
        g.vr.handRotation=Matrix4().rotateZ(yaw);
        g.vr.wristOffset=g.vr.handRotation*Vector3(-.12f,0,0);
        g.vr.hand[main]={.3f,0,-.1f};
        for(const Vector3 p:{Vector3(.10f,0,-.075f),Vector3(.23f,0,-.075f),Vector3(0,0,-.17f),Vector3(-.08f,0,-.075f),Vector3(.10f,.3f,-.075f)}){
            g.vr.buttons[2-main]=false;tick(input);
            g.vr.hand[1-main]=g.vr.hand[main]+g.vr.handRotation*p;
            g.vr.buttons[2-main]=true;tick(input);
            CHECK(g.bCompactSupportGrip==(p.x>=0 && p.y<.2f));CHECK(!g.bUseTwoHandAim);
        }
        // Reload follows the active weapon hand, independently of menu input.
        input.Reload=main==0?8:9;
        g.vr.buttons[input.Reload]=true;input.recentreTap=g.bLeftHanded;tick(input);CHECK(g.reloadDown);CHECK(g.reloadValid);
        g.vr.buttons[input.Reload]=false;input.recentreTap=false;tick(input);CHECK(!g.reloadDown);
        // The menu chord gates right-hand firing until the physical RT releases.
        g.vr.buttons[main+3]=true;input.menuRightTriggerBlocked=true;tick(input);
        CHECK(input.physicalCanFire==(main==0));
        CHECK(Helpers::controls.Fire==(main==0?127:0));
        input.menuRightTriggerBlocked=false;tick(input);CHECK(input.physicalCanFire);
        g.longGrip=true;
    }

    for(unsigned hand:{0u,1u}) {
        InputHandler input;initialize(input);draw(input,hand);
        auto& g=Game::instance;auto& vr=g.vr;
        Immersion::FireState state;bool supported=true;
        CHECK(input.PhysicalRecoilState({11,0},state,supported));CHECK(!supported);
        CHECK(!input.PhysicalRecoilState({22,0},state,supported));
        vr.hand[hand]={0.3f,0,-0.1f};vr.hand[1-hand]={0.55f,0,-0.1f};tick(input);
        vr.buttons[2-hand]=true;tick(input);
        CHECK(input.PhysicalRecoilState({11,0},state,supported));CHECK(supported);
        vr.buttons[2-hand]=false;tick(input);
        CHECK(input.PhysicalRecoilState({11,0},state,supported));CHECK(!supported);
        vr.buttons[2-hand]=true;vr.hand[1-hand]={1,0,-0.1f};tick(input);
        CHECK(input.PhysicalRecoilState({11,0},state,supported));CHECK(!supported); // Grip away from gun.
        vr.valid=false;tick(input);CHECK(!input.PhysicalRecoilState({11,0},state,supported));
    }
    for(unsigned hand:{0u,1u})for(unsigned slot:{0u,1u}) {
        InputHandler input;initialize(input);auto& vr=Game::instance.vr;
        const auto p=Immersion::Layout{}.Offset(slot==0?Immersion::Zone::Shoulder:Immersion::Zone::WeaponHip);
        vr.hand[hand]={p.x,p.y,p.z};tick(input);CHECK(vr.haptics[hand]==(slot==0?1u:0u));
        for(int i=0;i<10;++i)tick(input);CHECK(vr.haptics[hand]==(slot==0?1u:0u));
        vr.hand[hand]={0.6f,0,0};tick(input);
        vr.hand[hand]={p.x,p.y,p.z};tick(input);CHECK(vr.haptics[hand]==(slot==0?2u:0u));
        CHECK(input.physicalSession.Held()==Immersion::None);
    }
    for(bool layout:{false,true})for(unsigned hand:{0u,1u}){
        InputHandler input;initialize(input,layout);auto& g=Game::instance;
        draw(input,hand);CHECK(input.physicalSession.Held()==11);CHECK(g.bLeftHanded==(hand==0));CHECK(g.layout.value==layout);
        CHECK(g.vr.haptics[hand]==1);g.vr.hand[hand]={0.4f,0,-0.1f};
        g.vr.buttons[hand+3]=true;tick(input);CHECK(Helpers::controls.Fire==127);
        g.vr.buttons[hand+1]=false;tick(input);CHECK(input.physicalEngine.drops==1);
        CHECK(input.physicalSession.Held()==Immersion::None);CHECK(Helpers::controls.Fire==0);
        CHECK(std::abs(input.physicalEngine.droppedPose.position.x-10.2f)<1e-4f);
    }
    { // Already-held trigger at draw cannot fire until released and pressed.
        InputHandler input;initialize(input);auto& vr=Game::instance.vr;
        vr.buttons[4]=true;draw(input,1);CHECK(!input.physicalCanFire);CHECK(Helpers::controls.Fire==0);
        vr.buttons[4]=false;tick(input);vr.buttons[4]=true;tick(input);CHECK(Helpers::controls.Fire==127);
        CHECK(!input.ShouldBlockPhysicalFire({11,0}));
        vr.valid=false;tick(input);CHECK(input.ShouldBlockPhysicalFire({11,0}));CHECK(!input.ShouldBlockPhysicalFire({99,0}));
        vr.buttons[2]=false;vr.valid=true;tick(input);CHECK(input.physicalEngine.drops==0);
        CHECK(!input.physicalCanFire); // No fabricated release on input recovery.
        vr.buttons[2]=true;tick(input);vr.hand[1]={0.4f,0,-0.2f};vr.buttons[2]=false;tick(input);CHECK(input.physicalEngine.drops==1);
    }
    for(unsigned main:{0u,1u}){
        InputHandler input;initialize(input);draw(input,main);auto& g=Game::instance;const unsigned off=1-main;
        g.vr.hand[main]={0.2f,0,-0.2f};g.vr.hand[off]={0.4f,0,-0.2f};tick(input);
        g.vr.buttons[off+1]=true;tick(input);CHECK(g.bUseTwoHandAim);
        g.vr.buttons[main+1]=false;tick(input);CHECK(input.physicalSession.SupportOnly());CHECK(!input.physicalCanFire);
        CHECK(input.physicalEngine.drops==0);g.vr.hand[off].x=0.5f;
        Matrix4 transform;CHECK(input.GetSupportOnlyTransform(transform));CHECK(std::abs((transform*Vector3()).x-0.3f)<1e-4f);
        g.vr.hand[main].x=0.3f;g.vr.buttons[main+1]=true;tick(input);
        CHECK(!input.physicalSession.SupportOnly());CHECK(g.bUseTwoHandAim);
        g.vr.buttons[1]=g.vr.buttons[2]=false;tick(input);CHECK(input.physicalEngine.drops==1);
    }
    { // Menu/tracking gaps and head gestures never become a drop or draw.
        InputHandler input;initialize(input);input.headGripConsumed[1]=true;draw(input,1);CHECK(input.physicalSession.Held()==Immersion::None);
        input.headGripConsumed[1]=false;Game::instance.vr.buttons[2]=false;tick(input);draw(input,1);
        Helpers::menu=true;tick(input);Game::instance.vr.buttons[2]=false;Helpers::menu=false;tick(input);CHECK(input.physicalEngine.drops==0);
        Game::instance.vr.buttons[2]=true;tick(input);Game::instance.vr.tracked[0]=false;tick(input);
        Game::instance.vr.buttons[2]=false;Game::instance.vr.tracked[0]=true;tick(input);CHECK(input.physicalEngine.drops==0);
        Game::instance.vr.buttons[2]=true;tick(input);
        input.lastPhysicalInputUpdate-=std::chrono::seconds(1);Game::instance.vr.buttons[2]=false;tick(input);
        CHECK(input.physicalEngine.drops==0); // No fabricated drop after a stopped input loop.
    }
    { // A custom/scripted inventory larger than R1's two homes retains stock input.
        InputHandler input;initialize(input);draw(input,1);
        input.physicalEngine.state.weapons[2]=33;tick(input);
        CHECK(!input.physicalControlsActive);CHECK(!input.physicalSession.Active());
        CHECK(input.physicalEngine.drops==0);CHECK(input.physicalEngine.state.current==11);
    }
    { // Missing actions must never claim unsupported/multiplayer or unarmed input.
        InputHandler input;auto& g=Game::instance;g.vr={};g.input=&input;g.vr.valid=false;
        input.InitializePhysicalWeapons();input.physicalEngine.valid=false;tick(input);
        CHECK(!input.physicalControlsActive);CHECK(!input.physicalSession.Active());
        input.physicalEngine.valid=true;input.physicalEngine.state.weapons.fill(Immersion::None);
        input.physicalEngine.state.current=Immersion::None;tick(input);CHECK(!input.physicalControlsActive);
        input.physicalEngine.state.weapons[0]=11;input.physicalEngine.state.current=11;
        tick(input);CHECK(!input.physicalControlsActive);CHECK(input.physicalEngine.state.current==11);
        g.vr.valid=true;input.physicalEngine.rejectSelect=true;tick(input);
        CHECK(!input.physicalControlsActive);CHECK(input.physicalEngine.state.current==11);
        input.physicalEngine.rejectSelect=false;tick(input);
        CHECK(input.physicalControlsActive);CHECK(input.physicalEngine.state.current==Immersion::None);
        CHECK(input.physicalSession.Items().SlotOwner(1)==11);
    }
    for (unsigned hand : {0u,1u}) {
        InputHandler input;input.physicalEngine.state.weapons.fill(Immersion::None);
        input.physicalEngine.state.current=Immersion::None;initialize(input);
        auto& vr=Game::instance.vr;input.physicalEngine.target=33;
        Helpers::controls.Interact=Helpers::controls.PickupWeapon=127;tick(input);
        CHECK(input.physicalSession.Held()==Immersion::None);
        CHECK(Helpers::controls.Interact==0);CHECK(Helpers::controls.PickupWeapon==0);
        vr.buttons[hand+3]=true;vr.buttons[hand+1]=true;tick(input);
        CHECK(input.physicalSession.Held()==33);CHECK(!input.physicalCanFire);CHECK(vr.haptics[hand]==1);
        vr.buttons[hand+3]=false;tick(input);vr.buttons[hand+3]=true;tick(input);CHECK(input.physicalCanFire);
        vr.buttons[hand+1]=false;vr.hand[hand]={0.5f,0,0};tick(input);
        CHECK(input.physicalSession.Held()==Immersion::None);CHECK(input.physicalEngine.pickupEnabled);
        input.physicalEngine.valid=false;tick(input);CHECK(!input.physicalEngine.pickupEnabled);
    }
    for(unsigned hand:{0u,1u}) {
        InputHandler input;initialize(input);draw(input,hand);
        auto& g=Game::instance;auto& e=input.physicalEngine;
        g.vr.hand[hand]={.5f,0,0};g.vr.buttons[hand+3]=true;tick(input);
        CHECK(input.physicalSession.Held()==11);
        e.valid=false;
        for(unsigned i=0;i<120;++i){
            Helpers::controls.Grenade=Helpers::controls.Fire=Helpers::controls.Reload=127;
            Helpers::controls.Interact=Helpers::controls.PickupWeapon=Helpers::controls.SwitchWeapons=127;
            tick(input);CHECK(input.physicalControlsActive);CHECK(!input.physicalFrameValid);
            CHECK(input.physicalSession.Held()==11);CHECK(!input.physicalEngine.pickupEnabled);
            CHECK(Helpers::controls.Grenade==0 && Helpers::controls.Fire==0 && Helpers::controls.Reload==0);
            CHECK(Helpers::controls.SwitchWeapons==0 && Helpers::controls.PickupWeapon==0);
        }
        g.vr.buttons[hand+1]=false;e.valid=true;tick(input);
        CHECK(input.physicalSession.Held()==11 && e.drops==0);CHECK(!input.physicalCanFire);
        CHECK(input.physicalSession.Items().SlotOwner(0)==22);
        // Death/checkpoint can replace the biped/inventory. Resume initializes
        // fresh homes once, without a stale release, old weapon or auto-shot.
        e.valid=false;tick(input);e.state.unit=101;e.state.storage+=0x10000;e.state.age=0;
        e.state.weapons={33,44,Immersion::None,Immersion::None};e.state.current=33;e.valid=true;tick(input);
        CHECK(input.physicalControlsActive);CHECK(input.physicalSession.Held()==Immersion::None);
        CHECK(input.physicalSession.Items().SlotOwner(1)==33);CHECK(input.physicalSession.Items().SlotOwner(0)==44);
        CHECK(e.drops==0 && Helpers::controls.Fire==0);
        // A real unsupported-mode change still relinquishes ownership.
        e.resumable=false;e.valid=false;tick(input);CHECK(!input.physicalControlsActive);CHECK(!input.physicalSession.Active());
    }
    for (bool left : {false,true}) {
        InputHandler input;initialize(input,left);auto& vr=Game::instance.vr;
        vr.buttons[5]=true;tick(input);CHECK(Helpers::controls.Jump==127);CHECK(Helpers::controls.Grenade==0);
        vr.buttons[5]=false;vr.buttons[6]=true;tick(input);CHECK(Helpers::controls.Jump==0);CHECK(Helpers::controls.Grenade==0);
        // Read/adapter failure must not restore stick throwing behind our back.
        Game::instance.grenades.value=false;tick(input);CHECK(Helpers::controls.Grenade==127);
        Game::instance.grenades.value=true;tick(input);CHECK(Helpers::controls.Grenade==0);
        vr.buttons[7]=true;tick(input);CHECK(Helpers::controls.SwitchGrenades==0);
        vr.buttons[7]=false;tick(input);CHECK(Helpers::controls.SwitchGrenades==127);
        tick(input);CHECK(Helpers::controls.SwitchGrenades==0);
        vr.buttons[7]=true;tick(input);input.menuChordUsed=true;
        vr.buttons[7]=false;tick(input);CHECK(Helpers::controls.SwitchGrenades==0);
    }
    std::cout<<checks<<" production physical-input checks passed (engine/VR doubles)\n";
}
