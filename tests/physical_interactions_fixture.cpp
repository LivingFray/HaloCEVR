#include "HaloCEVR/Immersion/SolidZone.h"
// Compile the complete production interaction adapter against small VR/native
// doubles. No game memory or headset is used by these checks.
#include "HaloCEVR/PhysicalInteractionHandler.h"
#include "HaloCEVR/Helpers/Renderer.h"
#include "HaloCEVR/Immersion/RetailWeaponEngine.h"
#include "HaloCEVR/Immersion/HandGripPose.h"
#include "HaloCEVR/Helpers/Controls.h"
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <limits>
struct Bone{char BoneName[32]{};int Parent=-1;};
enum class ControllerRole{Left,Right};
#define D3DCOLOR_ARGB(a,r,g,b) unsigned(0)
namespace Logger{std::ostringstream log;}
namespace Helpers{
    Camera camera{};Controls controls{};bool menu=false,loading=false;
    struct Cutscene{bool bInCutscene=false;} scene;
    Camera& GetCamera(){return camera;}
    Controls& GetControls(){return controls;}
    bool IsMouseVisible(){return menu;}
    bool IsCampaignLoading(){return loading;}
    Cutscene* GetCutsceneData(){return &scene;}
}
template<class T>struct Property{T value;T Value()const{return value;}};
struct VRDouble{
    bool hmdTracked=true;bool tracked[2]{true,true};float yaw=0;
    Matrix4 head;std::array<Matrix4,2> hands{};std::array<Vector3,2> velocity{};unsigned buzz[2]{};
    unsigned inputUpdates=0;void UpdateInputs(){++inputUpdates;}
    bool IsHMDTracked()const{return hmdTracked;}
    bool IsControllerTracked(ControllerRole h)const{return tracked[unsigned(h)];}
    Matrix4 GetHMDTransform(bool=false)const{return Matrix4(head).rotateZ(-yaw);}
    Matrix4 GetRawControllerTransform(ControllerRole h,bool=false)const{return Matrix4(hands[unsigned(h)]).rotateZ(-yaw);}
    Matrix4 GetControllerTransform(ControllerRole h,bool=false)const{return GetRawControllerTransform(h);}
    Vector3 GetControllerVelocity(ControllerRole h)const{return velocity[unsigned(h)];}
    float GetYawOffset()const{return yaw;}
    void SetYawOffset(float v){yaw=v;}
    void TriggerHapticVibration(ControllerRole h,float,float,float,float){++buzz[unsigned(h)];}
};
using IVR=VRDouble;
struct Game{
    static Game instance;PhysicalInteractionHandler interactions;VRDouble vr;
    bool bUse3DOFAiming=false,bNeedsRecentre=false,bLeftHanded=false;
    float lastDeltaTime=.01f;
    Property<bool> grenades{true},vehicles{true},leftHanded{false},manualReload{true};Property<float> strength{1.3f},eyeHeight{.18f},ghostLift{.18f},shadeLift{.18f},gripHeight{.06f},turretSensitivity{.55f},ghostSensitivity{1.f};
    Property<float>* c_GhostSensitivity=&ghostSensitivity;
    Vector2 passengerLook{};Vector2 VehicleLookInput(){return passengerLook;}
    Property<float>* c_GhostEyeLift=&ghostLift,*c_TurretSensitivity=&turretSensitivity,*c_ShadeEyeLift=&shadeLift,*c_GrenadeGripHeightOffset=&gripHeight;
    Property<bool>* c_PhysicalGrenades=&grenades,*c_FirstPersonVehicles=&vehicles;
    Property<float> melee{4.f};Property<float>* c_LeftHandMeleeSwingSpeed=&melee,*c_RightHandMeleeSwingSpeed=&melee;
    Property<float>* c_GrenadeThrowStrength=&strength,*c_VehicleEyeHeight=&eyeHeight;
    Property<bool>* c_LeftHanded=&leftHanded,*c_ManualReload=&manualReload;
    Property<Vector3> rotation{{}},offset{{}};
    Property<Vector3>* c_ControllerRotation=&rotation,*c_ControllerOffset=&offset;
    bool reloadBusy[2]{};
    bool ReloadHandBusy(Immersion::Hand hand)const{return reloadBusy[unsigned(hand)];}
    VRDouble* GetVR(){return &vr;}
    float MetresToWorld(float v)const{return v*.5f;}
    float WorldToMetres(float v)const{return v*2.f;}
    struct Renderer{
        std::vector<Vector3> vertices;
        void DrawMeshTriangle(const Vector3 (&p)[3],const unsigned (&)[3]){for(auto v:p)vertices.push_back(v);}
    } inGameRenderer;
};
Game Game::instance;
namespace Immersion{
    GrenadeInventory inventory{42,0x100000,{4,3},0,0};VehicleState vehicle{};
    bool scorpion=false,valid=true;
    bool RetailWeaponEngine::IsScorpionDriver()const{return valid && scorpion;}
    Object attached=None;unsigned selections=0,finishes=0,draws=0,handsDrawn=0;
    TrackedGrenadeRelease finished{};float finishedStrength=0,finishedCap=0;
    bool RetailWeaponEngine::ReadGrenades(GrenadeInventory& out)const{out=inventory;return valid;}
    bool RetailWeaponEngine::SelectGrenade(Object unit,unsigned type){
        if(!valid || unit!=inventory.unit || type>=2 || inventory.state==1 || inventory.state==2)return false;
        inventory.selected=static_cast<int>(type);++selections;return true;
    }
    Object RetailWeaponEngine::AttachedGrenade(Object unit)const{return unit==inventory.unit && inventory.state==2?attached:None;}
    bool RetailWeaponEngine::FinishGrenade(Object projectile,const TrackedGrenadeRelease& r,float strength,float cap){
        if(projectile!=attached || inventory.state!=3)return false;
        ++finishes;finished=r;finishedStrength=strength;finishedCap=cap;return true;
    }
    bool RetailWeaponEngine::ReadVehicle(VehicleState& out)const{out=vehicle;return valid && vehicle.kind!=VehicleSeat::None;}
    std::vector<Matrix4> grenadePoses;std::vector<bool> grenadeGrey;
    bool RetailWeaponEngine::DrawGrenade(unsigned,const Matrix4& pose,bool grey)const{++draws;grenadePoses.push_back(pose);grenadeGrey.push_back(grey);return true;}
    bool RetailWeaponEngine::DrawInteractionHands(const Matrix4*,unsigned mask,unsigned,Matrix4*)const{handsDrawn=mask;return true;}
    bool RetailWeaponEngine::Read(NativeInventory&)const{return false;}
    bool RetailWeaponEngine::Select(Object,Object){return false;}
    NativeDrop RetailWeaponEngine::Drop(Object,Object,const DropPose&){return NativeDrop::Rejected;}
    bool RetailWeaponEngine::Pickup(Object,Object){return false;}
}
// INSERT_PRODUCTION_INTERACTIONS
class InputHandler{public:unsigned char UpdateMelee();};
// INSERT_PRODUCTION_MELEE
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
using namespace Immersion;
void grenadeTests(){
    RetailWeaponEngine engine;WeaponSession session;auto& g=Game::instance;auto& adapter=g.interactions;
    auto run=[&](WeaponFrame& frame){Matrix4 poses[2]={g.vr.GetControllerTransform(ControllerRole::Left),g.vr.GetControllerTransform(ControllerRole::Right)};
        adapter.Grenades(engine,session,frame,poses,{},0);};
    for(unsigned hand=0;hand<2;++hand)for(unsigned type=0;type<2;++type){
        adapter.SuspendGrenades();inventory={42,0x100000,{4,3},0,0};g.vr={};Helpers::controls={};
        const auto home=Layout{false}.Anchor(type?Zone::PlasmaChest:Zone::FragChest,{},0);
        g.vr.hands[hand]=Matrix4().translate({home.x,home.y,home.z});
        WeaponFrame frame;frame.grip[hand]={true,true,false};run(frame);
        CHECK(adapter.GrenadeHandMask()==(1u<<hand));CHECK(frame.consumed[hand]);CHECK(inventory.count[type]==(type?3u:4u));
        CHECK(inventory.selected==static_cast<int>(type));
        InputHandler input;g.vr.velocity[hand]={0,0,5};CHECK(input.UpdateMelee()==0);
        g.vr.velocity[1-hand]={0,0,5};CHECK(input.UpdateMelee()==127);g.vr.velocity[1-hand]={};
        grenadePoses.clear();grenadeGrey.clear();g.inGameRenderer.vertices.clear();
        // Render sampling later in the frame must not silently move the grab
        // anchor away from the matching ring/mesh (both sides, either hand).
        g.vr.head.translate({.03f,.02f,.01f});adapter.Draw();adapter.DrawNative();
        CHECK(handsDrawn==(1u<<hand));CHECK(grenadePoses.size()==3);
        CHECK(std::fabs(grenadePoses[0].getLeftAxis().length()-.5f)<.0001f);
        CHECK(std::fabs(grenadePoses[1].getLeftAxis().length()-.32f)<.0001f);
        for(unsigned t=0;t<2;++t){
            CHECK(g.inGameRenderer.vertices.empty());
            const auto anchor=Layout{false}.Anchor(t?Zone::PlasmaChest:Zone::FragChest,{},0);
            CHECK((grenadePoses[t+1]*Vector3()-Helpers::camera.position-Vector3(anchor.x,anchor.y,anchor.z)*g.MetresToWorld(1)).length()<.0001f);
        }
        // Return directly: no native throw, no count mutation.
        frame={};frame.grip[hand]={false,false,true};run(frame);adapter.GrenadeControls();
        CHECK(adapter.GrenadeHandMask()==0);CHECK(!Helpers::controls.Grenade);CHECK(!adapter.BeforeThrow(42));
        frame={};frame.grip[hand]={true,true,false};run(frame);
        g.vr.hands[hand]=Matrix4().translate({.7f,hand?.3f:-.3f,0});
        frame={};frame.grip[hand]={true,false,false};frame.release[hand].velocity={5,2,3};run(frame);
        frame={};frame.grip[hand]={false,false,true};frame.release[hand].position={.35f,.15f,0};run(frame);
        CHECK(adapter.GrenadeHandMask()==0);CHECK(input.UpdateMelee()==0);CHECK(adapter.GrenadeControls());CHECK(Helpers::controls.Grenade==127);
        CHECK(!adapter.BeforeThrow(99)); // Never relocate an AI grenade.
        CHECK(adapter.GrenadeControls());CHECK(Helpers::controls.Grenade==127); // Wait for native acknowledgement.
        inventory.state=1;adapter.GrenadeControls();CHECK(Helpers::controls.Grenade==0);
        inventory.state=2;attached=56;CHECK(adapter.BeforeThrow(42));inventory.state=3;
        adapter.GrenadeControls();CHECK(Helpers::controls.Grenade==0);
        const auto before=finishes;adapter.AfterThrow(42);CHECK(finishes==before+1);
        CHECK(finished.hand==hand && finished.type==type);CHECK(finishedStrength==1.3f && finishedCap==g.MetresToWorld(24));CHECK(finished.pose.velocity.x==5); // Apex sample.
        CHECK(!adapter.BeforeThrow(42));adapter.GrenadeControls();CHECK(Helpers::controls.Grenade==0);
        CHECK(inventory.count[type]==(type?3u:4u)); // Adapter never fabricates/decrements ammo.
    }
    adapter.SuspendGrenades();inventory={42,0x100000,{0,0},0,0};
    WeaponFrame empty;run(empty);grenadePoses.clear();grenadeGrey.clear();g.inGameRenderer.vertices.clear();
    adapter.Draw();adapter.DrawNative();CHECK(grenadePoses.size()==2);CHECK(grenadeGrey==std::vector<bool>({true,true}));CHECK(g.inGameRenderer.vertices.empty());
    struct HeldEngine:WeaponEngine{
        NativeInventory inventory{42,0x100000,0,{11,None,None,None},11,1};
        bool Read(NativeInventory& out)const override{out=inventory;return true;}
        bool Select(Object,Object weapon)override{inventory.current=weapon;return true;}
        NativeDrop Drop(Object,Object,const DropPose&)override{return NativeDrop::Rejected;}
    } heldEngine;
    for(unsigned main=0;main<2;++main){
        session.Reset();heldEngine.inventory.current=11;session.Update(heldEngine,{});
        WeaponFrame draw;draw.grip[main]={true,true,false};draw.holster[main]=1;session.Update(heldEngine,draw);
        CHECK(session.Held()==11 && unsigned(session.FiringHand())==main);
        adapter.SuspendGrenades();inventory={42,0x100000,{4,3},0,0};g.vr={};
        const unsigned off=1-main;g.vr.hands[off].translate({.06f,.10f,-.24f});
        WeaponFrame frame;frame.grip[main]={true,false,false};frame.grip[off]={true,true,false};frame.nearSupport[off]=true;
        run(frame);session.Update(heldEngine,frame);CHECK(adapter.GrenadeHandMask()==(1u<<off));
        CHECK(session.Held()==11 && session.CanFire(frame));CHECK(!session.Items().Supported(11));
        // The firing hand cannot also acquire a grenade.
        g.vr.hands[main]=Matrix4().translate({.06f,-.10f,-.24f});
        frame.grip[main]={true,true,false};frame.grip[off]={true,false,false};run(frame);
        CHECK(adapter.GrenadeHandMask()==(1u<<off));
    }
    session.Reset();
    for(unsigned hand=0;hand<2;++hand)for(unsigned type=0;type<2;++type){
        adapter.SuspendGrenades();inventory={42,0x100000,{4,3},0,0};g.vr={};
        const auto preview=Layout{false}.Anchor(type?Zone::PlasmaChest:Zone::FragChest,{},0);
        const auto center=preview+Point{0,0,.06f};
        WeaponFrame frame;run(frame);grenadePoses.clear();adapter.DrawNative();
        CHECK(grenadePoses.size()==2);
        CHECK((grenadePoses[type]*Vector3()-Vector3(preview.x,preview.y,preview.z)*g.MetresToWorld(1)-Helpers::camera.position).length()<.0001f);
        // Upper visible portion now captures; the corresponding low point no longer does.
        g.vr.hands[hand]=Matrix4().translate({center.x,center.y,center.z+.05f});
        frame.grip[hand]={true,true,false};run(frame);CHECK(adapter.GrenadeHandMask()==(1u<<hand));
        frame.grip[hand]={false,false,true};run(frame);CHECK(adapter.GrenadeHandMask()==0);
        CHECK(adapter.GrenadeControls() && Helpers::controls.Grenade==0); // Raised return does not throw.
        frame={};g.vr.hands[hand]=Matrix4().translate({preview.x,preview.y,preview.z-.06f});
        frame.grip[hand]={true,true,false};run(frame);CHECK(adapter.GrenadeHandMask()==0);
    }
    // Closer chest capture spheres overlap. Whichever grenade is nearer must
    // win for either physical hand, rather than fixed frag-before-plasma order.
    for(unsigned hand=0;hand<2;++hand)for(unsigned type=0;type<2;++type){
        adapter.SuspendGrenades();inventory={42,0x100000,{4,3},0,0};g.vr={};
        g.vr.hands[hand].translate({.06f,type?-.01f:-.03f,-.24f});
        WeaponFrame grab;grab.grip[hand]={true,true,false};run(grab);
        CHECK(adapter.GrenadeHandMask()==(1u<<hand));CHECK(inventory.selected==static_cast<int>(type));
    }
    adapter.SuspendGrenades();inventory={42,0x100000,{4,3},0,0};g.vr={};g.vr.hands[0].translate({.06f,.10f,-.24f});
    g.reloadBusy[0]=true;WeaponFrame frame;frame.grip[0]={true,true,false};run(frame);CHECK(adapter.GrenadeHandMask()==0);
    g.reloadBusy[0]=false;frame.consumed[0]=true;run(frame);CHECK(adapter.GrenadeHandMask()==0);
    frame.consumed[0]=false;run(frame);CHECK(adapter.GrenadeHandMask()==1);
    Helpers::menu=true;run(frame);CHECK(adapter.GrenadeHandMask()==0);CHECK(!adapter.BeforeThrow(42));Helpers::menu=false;
    adapter.SuspendGrenades();
}
void vehicleTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;
    for(auto kind:{VehicleSeat::WarthogDriver,VehicleSeat::WarthogTurret,VehicleSeat::Ghost,VehicleSeat::ShadeTurret}){
        g.vr={};Helpers::camera.position={10,20,30};vehicle={};vehicle.unit=42;vehicle.vehicle=50;vehicle.kind=kind;vehicle.seat=0;
        vehicle.position={10,20,29};vehicle.eye={10,20,30};vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};
        bool triggers[2]{};adapter.UpdateVehicle(engine,true,triggers,true);CHECK(adapter.VehicleActive());
        CHECK(adapter.HideDrivingReticle()==(kind==VehicleSeat::WarthogDriver));
        CHECK(adapter.VehicleBodyOwner()==None);
        CameraFrustum first{},second{};first.position={1,2,3};second.position={4,5,6};first.drawPlayer=second.drawPlayer=false;
        adapter.BeginView(first,second);CHECK((first.position-Vector3(10,20,(kind==VehicleSeat::Ghost || kind==VehicleSeat::ShadeTurret)?30.18f:30.09f)).length()<.0001f);
        CHECK(first.drawPlayer && second.drawPlayer);CHECK(adapter.VehicleBodyOwner()==42);
        const auto base=first.position;adapter.EndView();
        vehicle.aim={.98f,.2f,0};adapter.BeginView(first,second);
        const float response=adapter.VehicleAim().y;adapter.EndView();
        CHECK(response>0 && (kind==VehicleSeat::Ghost?response<.1f:response>.19f));
        vehicle.aim={1,0,0};
        for(unsigned i=0;i<100;++i){adapter.BeginView(first,second);adapter.EndView();}CHECK((first.position-Vector3(1,2,3)).length()<.0001f);
        CHECK(!first.drawPlayer && !second.drawPlayer);CHECK(adapter.VehicleBodyOwner()==None);
        CHECK((Helpers::camera.position-Vector3(10,20,30)).length()<.0001f);
        // Late headset lean belongs to OpenVR. Native head animation must not
        // move the base camera, including repeated stereo frame submission.
        g.vr.head.translate({.1f,.1f,.1f});vehicle.eye={10.01f,19.995f,30.01f};
        adapter.BeginView(first,second);
        const auto nativeHeadDelta=IsTurretSeat(kind)?Vector3(.01f,-.005f,.01f):Vector3();
        CHECK((first.position-base-nativeHeadDelta).length()<.0001f);adapter.EndView();
        float yaw=7,pitch=7;CHECK(adapter.VehicleCamera(yaw,pitch,{},{}));CHECK(yaw==0 && pitch==0);
        handsDrawn=0;adapter.BeginView(first,second);adapter.DrawNative();adapter.EndView();CHECK(handsDrawn==0);
        const Vector2 move(-1,1),look(-1,1);
        adapter.VehicleCamera(yaw,pitch,move,look);CHECK(yaw>0);
        CHECK((IsTurretSeat(kind) || kind==VehicleSeat::Ghost)?pitch>0:pitch==0);
        float yaw2=0,pitch2=0;
        adapter.VehicleCamera(yaw2,pitch2,IsTurretSeat(kind)?move:Vector2(),IsTurretSeat(kind)?Vector2():look);
        if(kind==VehicleSeat::Ghost){CHECK(yaw2==yaw && pitch2==pitch);}
        else CHECK(yaw2==0 && pitch2==0);
        g.vr.hands[0].translate({-1,2,-1});g.vr.hands[1].rotateY(90);g.vr.head.rotateZ(70);g.vr.tracked[0]=false;
        adapter.UpdateVehicle(engine,true,triggers,true);adapter.VehicleCamera(yaw2,pitch2,move,look);
        CHECK(std::fabs(yaw-yaw2)<.0001f && std::fabs(pitch-pitch2)<.0001f);
        triggers[1]=true;Helpers::controls.Forwards=.7f;adapter.UpdateVehicle(engine,true,triggers,true);
        CHECK(Helpers::controls.Left==0);
        if(kind==VehicleSeat::WarthogDriver){CHECK(Helpers::controls.Forwards==1 && !Helpers::controls.Fire);
            triggers[1]=false;triggers[0]=true;adapter.UpdateVehicle(engine,true,triggers,true);CHECK(Helpers::controls.Forwards==-1);}
        else {CHECK(Helpers::controls.Fire==127);CHECK(Helpers::controls.Forwards==0);}
        if(kind==VehicleSeat::Ghost){
            for(bool lt:{false,true})for(bool rt:{false,true})for(Vector2 stick:{Vector2(-1,1),Vector2(1,-1),Vector2()}){
                triggers[0]=lt;triggers[1]=rt;adapter.UpdateVehicle(engine,true,triggers,true,stick);
                CHECK(Helpers::controls.Fire==((lt||rt)?127:0));CHECK(Helpers::controls.Forwards==stick.y);CHECK(Helpers::controls.Left==-stick.x);
                adapter.VehicleCamera(yaw2,pitch2,stick,{});CHECK(yaw2==0 && pitch2==0);
            }
            adapter.VehicleCamera(yaw2,pitch2,{},look);const float before=yaw2;
            g.turretSensitivity.value=1.5f;adapter.VehicleCamera(yaw2,pitch2,{},look);CHECK(yaw2==before);
            g.ghostSensitivity.value=2.f;adapter.VehicleCamera(yaw2,pitch2,{},look);CHECK(std::fabs(yaw2-2*before)<.00001f);
            g.turretSensitivity.value=.55f;g.ghostSensitivity.value=1.f;
        }
        // Chassis translation follows the seat, without animated-head movement.
        vehicle.position.x+=3;vehicle.eye.x+=3;adapter.BeginView(first,second);CHECK(std::fabs(first.position.x-base.x-3-nativeHeadDelta.x)<.0001f);adapter.EndView();
        Helpers::menu=true;adapter.UpdateVehicle(engine,true,triggers,true);CHECK(Helpers::controls.Forwards==0 && Helpers::controls.Fire==0);
        adapter.VehicleCamera(yaw,pitch,move,look);CHECK(yaw==0 && pitch==0);
        adapter.BeginView(first,second);CHECK(adapter.VehicleBodyOwner()==None);CHECK(!first.drawPlayer);Helpers::menu=false;
        vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);CHECK(!adapter.VehicleActive());CHECK(!adapter.VehicleCamera(yaw,pitch,{},{}));
    }
}
void seatEntryTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;
    // Start outside, as Halo sets the parent before its climb-in animation.
    // Also test a delayed entry which outlasts the initial settling window.
    for(auto kind:{VehicleSeat::WarthogDriver,VehicleSeat::WarthogTurret,VehicleSeat::Ghost,VehicleSeat::ShadeTurret})for(bool delayed:{false,true}){
        g.vr={};g.vr.head.translate({.2f,-.3f,.1f});g.lastDeltaTime=.01f;
        Helpers::camera.position={20,10,4};vehicle={};vehicle.unit=42;vehicle.vehicle=50;vehicle.kind=kind;vehicle.seat=IsTurretSeat(kind)?2:0;
        vehicle.position={20,10,4};vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};
        const Vector3 outside(-.5f,.6f,.5f),inside(.15f,0,.65f);
        auto placeHead=[&](Vector3 local){const auto w=VehicleBasis(vehicle)*local;vehicle.eye={w.x,w.y,w.z};};
        placeHead(outside);bool triggers[2]{};adapter.UpdateVehicle(engine,true,triggers,true);
        CameraFrustum first{},second{};first.position={7,8,9};second.position={4,5,6};
        auto draw=[&](){adapter.BeginView(first,second);
            const Vector3 eye=first.position+(g.vr.GetHMDTransform(true)*Vector3())*g.MetresToWorld(1);
            adapter.EndView();CHECK((first.position-Vector3(7,8,9)).length()<.0001f);return eye;};
        draw();
        if(delayed)for(int i=0;i<130;++i)draw();
        for(unsigned i=1;i<=100;++i){
            const float alpha=(std::min)(1.f,float(i)/60);
            const float turn=float(i)*.003f;
            vehicle.forward=vehicle.aim={std::cos(turn),std::sin(turn),0};
            placeHead(outside+(inside-outside)*alpha);draw();
        }
        Vector3 eye;
        for(unsigned i=0;i<100;++i)eye=draw();
        const auto expected=VehicleBasis(vehicle)*inside+Vector3(0,0,g.MetresToWorld((kind==VehicleSeat::Ghost || kind==VehicleSeat::ShadeTurret)?.36f:.18f));
        CHECK((eye-expected).length()<g.MetresToWorld(.025f));
        const auto steady=eye;
        for(unsigned i=0;i<60;++i){placeHead(inside+Vector3(i%2?.002f:-.002f,0,0));eye=draw();CHECK((eye-steady).length()<(IsTurretSeat(kind)?.0021f:g.MetresToWorld(.002f)));}
        // Real physical head lean remains immediate after the seat is settled.
        placeHead(inside);draw();
        const auto leanBefore=g.vr.GetHMDTransform(true)*Vector3();g.vr.head.translate({.08f,0,.03f});
        const auto leanAfter=g.vr.GetHMDTransform(true)*Vector3();
        CHECK((draw()-steady-(leanAfter-leanBefore)*g.MetresToWorld(1)).length()<.0001f);
        vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);CHECK(adapter.VehicleBodyOwner()==None);
    }
}
void movingSeatTests(){
    // Simulation at 30 Hz; independent render rates and render phases. A
    // constant-speed ride must not repeatedly accelerate/brake between ticks.
    for(float fps:{60.f,72.f,90.f,120.f,144.f}) {
        SeatMotion filter;const float dt=1.f/fps;float previous=0,maxStep=0,minStep=100;
        for(unsigned frame=0;frame<unsigned(fps*5);++frame){
            const float t=frame*dt;
            const float nativeTime=std::floor(t*30.f+.0001f)/30.f;
            const auto smooth=filter.Update({nativeTime*3,0,0},{3,0,0},dt);
            if(frame>unsigned(fps)){
                CHECK(std::fabs(smooth.x-(t-1.f/30.f)*3)<3*dt+.002f);
                const float step=smooth.x-previous;CHECK(step>=-.0001f);
                maxStep=(std::max)(maxStep,step);minStep=(std::min)(minStep,step);
            }
            previous=smooth.x;
        }
        CHECK(maxStep<3*dt*1.8f);CHECK(minStep>3*dt*.4f);
        const auto end=filter.Update({15,0,0},{},dt);
        Vector3 stopped;
        for(int i=0;i<200;++i)stopped=filter.Update({15,0,0},{},dt);
        CHECK(stopped.x==15 && end.x<=15);
        CHECK(filter.Update({100,0,0},{},dt).x==100);
    }
    SeatAnchor anchor;
    for(int i=0;i<500;++i)anchor.Update({.2f,0,.7f},.01f,.5f);
    CHECK(anchor.Locked());
    for(int i=0;i<500;++i){
        const auto p=anchor.Update({.2f+std::sin(i*.1f)*.3f,0,.7f+std::cos(i*.3f)*.15f},.01f,.5f);
        CHECK((p-Vector3(.2f,0,.7f)).length()<.00001f);CHECK(anchor.Locked());
    }
    anchor.Reset();CHECK((anchor.Update({1,0,1},.01f,.5f)-Vector3(1,0,1)).length()<.00001f);
}
void renderedSeatTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;
    for(auto kind:{VehicleSeat::WarthogDriver,VehicleSeat::WarthogTurret,VehicleSeat::Ghost,VehicleSeat::ShadeTurret})for(float fps:{72.f,90.f,120.f}){
        g.vr={};g.lastDeltaTime=1.f/fps;Helpers::camera.position={};vehicle={};
        vehicle.unit=42;vehicle.vehicle=50;vehicle.seat=0;vehicle.kind=kind;
        vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};vehicle.renderedPoseValid=true;
        const Vector3 anchor(.15f,.1f,.65f);bool triggers[2]{};
        CameraFrustum first{},second{};
        for(unsigned frame=0;frame<unsigned(5*fps);++frame){
            const float t=frame/fps,sim=std::floor(t*30)/30;
            vehicle.position={sim*3,0,0}; // Old camera followed these steps.
            vehicle.renderedBasis=Matrix4().rotateZ(t*12).translate({t*3,0,0});
            const auto facing=vehicle.renderedBasis.getLeftAxis();vehicle.renderedFacing={facing.x,facing.y,facing.z};
            const auto head=vehicle.renderedBasis*anchor;vehicle.eye={head.x,head.y,head.z};
            adapter.UpdateVehicle(engine,true,triggers,true);adapter.BeginView(first,second);
            const auto eye=first.position+(g.vr.GetHMDTransform(true)*Vector3())*g.MetresToWorld(1);
            CHECK((eye-head-Vector3(0,0,g.MetresToWorld((kind==VehicleSeat::Ghost || kind==VehicleSeat::ShadeTurret)?.36f:.18f))).length()<.00003f);
            CHECK((first.position-second.position).length()<.000001f);adapter.EndView();
        }
        vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);
    }
}
void shadeOrbitTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;
    for(auto kind:{VehicleSeat::ShadeTurret,VehicleSeat::WarthogTurret})for(float fps:{72.f,90.f,120.f}){
        g.vr={};g.vr.head.translate({.2f,-.1f,.1f});g.lastDeltaTime=1/fps;
        Helpers::camera.position={2,3,4};vehicle={};vehicle.kind=kind;vehicle.unit=42;vehicle.vehicle=60;vehicle.seat=0;
        vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};vehicle.position={10,20,30};
        vehicle.renderedPoseValid=true;vehicle.renderedBasis=Matrix4().translate({10,20,30});
        vehicle.renderedFacing={1,0,0}; // Pedestal/root never turns.
        vehicle.renderedHeadValid=true;bool triggers[2]{};CameraFrustum first{},second{};
        for(unsigned frame=0;frame<unsigned(fps*8);++frame){
            // Turn after the old anchor's three-second freeze. Cross +/-pi.
            const float angle=(std::max)(0.f,frame/fps-3.1f)*1.5f;
            vehicle.renderedHeadFacing={std::cos(angle),std::sin(angle),0};
            vehicle.eye={10+.2f*std::cos(angle),20+.2f*std::sin(angle),30.8f};
            adapter.UpdateVehicle(engine,true,triggers,true);adapter.BeginView(first,second);
            const auto eye=first.position+(g.vr.GetHMDTransform(true)*Vector3())*g.MetresToWorld(1);
            const Vector3 expected(vehicle.eye.x,vehicle.eye.y,vehicle.eye.z+g.MetresToWorld(kind==VehicleSeat::ShadeTurret?.36f:.18f));
            CHECK((eye-expected).length()<.0001f);
            const auto look=g.vr.GetHMDTransform(true).getLeftAxis();
            CHECK(look.dot(Vector3(std::cos(angle),std::sin(angle),0))>.9999f);
            CHECK((first.position-second.position).length()<.0001f);adapter.EndView();
        }
        // Actual headset lean remains immediate and independent of the stick.
        g.vr.head.translate({.1f,0,0});adapter.BeginView(first,second);
        const auto leanEye=first.position+(g.vr.GetHMDTransform(true)*Vector3())*g.MetresToWorld(1);
        const Vector3 nativeEye(vehicle.eye.x,vehicle.eye.y,vehicle.eye.z+g.MetresToWorld(kind==VehicleSeat::ShadeTurret?.36f:.18f));
        CHECK(std::fabs((leanEye-nativeEye).length()-g.MetresToWorld(.1f))<.0001f);adapter.EndView();
        g.shadeLift.value=0;adapter.BeginView(first,second);
        const auto lower=first.position+(g.vr.GetHMDTransform(true)*Vector3())*g.MetresToWorld(1);
        CHECK(std::fabs(leanEye.z-lower.z-g.MetresToWorld(kind==VehicleSeat::ShadeTurret?.18f:0.f))<.0001f);adapter.EndView();g.shadeLift.value=.18f;
        vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);
    }
}
void transitionTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;bool triggers[2]{};
    for(auto kind:{VehicleSeat::WarthogDriver,VehicleSeat::Ghost,VehicleSeat::WarthogTurret,VehicleSeat::ShadeTurret,VehicleSeat::PelicanPassenger})
    for(bool earlyDetach:{false,true}){
        adapter.OnRecentre();g.vr={};g.lastDeltaTime=.01f;Helpers::controls={};Helpers::camera.position={10,20,30};
        Helpers::scene.bInCutscene=kind==VehicleSeat::PelicanPassenger;
        vehicle={};vehicle.unit=42;vehicle.vehicle=50;vehicle.kind=kind;vehicle.seat=0;
        vehicle.position={10,20,29};vehicle.eye={10,20,30};vehicle.forward=vehicle.aim=vehicle.renderedHeadFacing={1,0,0};vehicle.up={0,0,1};
        vehicle.renderedHeadValid=IsTurretSeat(kind);
        adapter.UpdateVehicle(engine,true,triggers,true);CameraFrustum a{},b{};a.position=b.position=Helpers::camera.position;
        adapter.BeginView(a,b);CHECK(adapter.ViewOverrideActive());const auto original=a.position;const auto yaw=g.vr.yaw;adapter.EndView();
        if(kind!=VehicleSeat::PelicanPassenger){
            Helpers::controls.Interact=127;adapter.UpdateVehicle(engine,true,triggers,true);
            vehicle.renderedHeadFacing={-1,0,0};vehicle.eye={10.8f,20.7f,30.5f}; // Dismount animation whips its head.
            adapter.BeginView(a,b);CHECK((a.position-original).length()<.0001f);CHECK(g.vr.yaw==yaw);adapter.EndView();
        }else{
            float turn=1,pitch=1;adapter.VehicleCamera(turn,pitch,{1,1},{1,1});CHECK(turn==0 && pitch==0);
        }
        a.position=b.position={10.6f,20,30.1f};Helpers::camera.position=a.position;const auto target=a.position;
        vehicle.kind=VehicleSeat::None;
        if(!earlyDetach)adapter.UpdateVehicle(engine,false,triggers,true);
        adapter.BeginView(a,b);CHECK(adapter.ViewOverrideActive());CHECK(!a.drawPlayer);CHECK(!adapter.VehicleActive());
        CHECK((a.position-original).length()<.04f);CHECK((a.position-b.position).length()<.0001f);adapter.EndView();
        for(unsigned i=0;i<220;++i){adapter.BeginView(a,b);adapter.EndView();}
        CHECK(!adapter.ViewOverrideActive());CHECK((a.position-target).length()<.0001f);CHECK(g.vr.yaw==yaw);
        Helpers::scene.bInCutscene=false;
    }
}
void vehicleVisibilityTests(){
    auto& g=Game::instance;auto& adapter=g.interactions;RetailWeaponEngine engine;
    for(auto kind:{VehicleSeat::WarthogDriver,VehicleSeat::WarthogTurret,VehicleSeat::Ghost,VehicleSeat::ShadeTurret}){
        g.vr={};Helpers::camera={};vehicle={};vehicle.unit=42;vehicle.vehicle=50;vehicle.kind=kind;vehicle.seat=0;
        vehicle.eye={0,0,1};vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};
        bool triggers[2]{};adapter.UpdateVehicle(engine,true,triggers,true);
        CameraFrustum base1{},base2{};adapter.BeginView(base1,base2);
        for(float nativeNear:{0.f,.01f,2.f})for(float fov:{1.2f,1.8f,2.8f})for(float yaw:{-.9f,0.f,.9f})for(float pitch:{-.7f,0.f,.7f})for(int eye:{0,1}){
            CameraFrustum raster{};raster.position={3.f,eye?.012f:-.012f,1.2f};
            const auto rotation=Matrix4().rotateY(pitch*57.29578f).rotateZ(yaw*57.29578f);
            raster.facingDirection=rotation.getLeftAxis();raster.upDirection=rotation.getUpAxis();
            raster.fov=fov;raster.zNear=nativeNear;raster.zFar=850;raster.unk1={1,2,3,4};raster.drawPlayer=true;
            const auto original=raster;CameraFrustum culling{};adapter.PrepareVehicleEye(culling,raster);
            CHECK((raster.position-original.position).length()==0);
            CHECK((raster.facingDirection-original.facingDirection).length()==0);
            CHECK((raster.upDirection-original.upDirection).length()==0);
            CHECK(raster.fov==fov && raster.zFar==850 && raster.zNear==(nativeNear>0?nativeNear:.00390625f));
            CHECK(raster.unk1.x==0 && raster.unk1.y==0 && raster.unk1.z==0 && raster.unk1.w==0);
            CHECK((culling.position-raster.position).length()==0);
            CHECK((culling.facingDirection-raster.facingDirection).length()==0);
            CHECK(culling.fov==raster.fov);
            CHECK(culling.zNear==raster.zNear && culling.zFar==raster.zFar);
        }
        adapter.EndView();vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);
        CHECK(!adapter.HideDrivingReticle());
        CameraFrustum cull{},raster{};cull.fov=1.1f;raster.zNear=0;raster.fov=1.3f;
        adapter.PrepareVehicleEye(cull,raster);CHECK(cull.fov==1.1f && raster.zNear==0 && raster.fov==1.3f);
    }
}
void rulesTests(){
    shadeOrbitTests();
    for(float yaw:{0.f,1.2f,-2.f}){
        Point center{.2f,-.1f,.6f};Point ahead{std::cos(yaw)*.08f,std::sin(yaw)*.08f,0};
        CHECK(GrenadeGripContains(center,center,yaw,0));
        CHECK(!GrenadeGripContains(center+ahead,center,yaw,0));
        CHECK(GrenadeGripContains(center+ahead,center,yaw,1));
        CHECK(!GrenadeGripContains(center+Point{0,0,.11f},center,yaw,0));
    }
    CHECK(TurretStick(.1f)==0);CHECK(TurretStick(-.1f)==0);
    CHECK(TurretStick(1.f)==1 && TurretStick(-1.f)==-1);
    CHECK(TurretStick(.5f)>.1f && TurretStick(.5f)<.2f);
    const float base=1.22f;
    for(float zoom:{2.f,8.f}){
        const float lens=2.f*std::atan(std::tan(base/2)/zoom);
        CHECK(std::fabs(ComfortableScopeFov(lens,base,1)-lens)<.00001f);
        const float reduced=ComfortableScopeFov(lens,base,.5f);
        CHECK(reduced>lens && reduced<base);
        CHECK(std::fabs(std::tan(base/2)/std::tan(reduced/2)-(1+(zoom-1)*.5f))<.0001f);
        CHECK(ComfortableScopeFov(lens,base,.15f)>reduced);
    }
    CHECK(ComfortableScopeFov(1.3f,1.2f,.5f)==1.3f);

    vehicleVisibilityTests();
    renderedSeatTests();
    movingSeatTests();
    SeatMotion seatMotion;
    CHECK((seatMotion.Update({1,2,3},{},.01f)-Vector3(1,2,3)).length()<.0001f);
    const auto softened=seatMotion.Update({1,2,3.03f},{},.01f);
    CHECK(softened.z==3); // Render timeline is one simulation tick behind.
    seatMotion.Reset();seatMotion.Update({},{},.01f);
    CHECK(seatMotion.Update({.03f,0,0},{3,0,0},.01f).x==0);
    CHECK((seatMotion.Update({5,0,0},{},.01f)-Vector3(5,0,0)).length()<.0001f);
    VehicleState car;car.forward={0,1,0};car.up={0,0,1};car.position={5,6,7};
    const auto basis=VehicleBasis(car);CHECK((basis*Vector3(1,0,0)-Vector3(5,7,7)).length()<.0001f);
    std::array<ModelNode,5> nodes{};std::array<Transform,5> bodyPose{};
    const char* names[]={"root","bip01 head","helmet","l upperarm","r upperarm"};
    for(unsigned i=0;i<5;++i){std::strcpy(nodes[i].name,names[i]);nodes[i].parent=i==0?-1:i==2?1:0;
        nodes[i].inverseBind=ModelTransform(Matrix4());bodyPose[i]=ModelTransform(Matrix4().translate({float(i),1,2}));}
    CHECK(HideSeatedHead(nodes.data(),5,bodyPose.data()));CHECK(bodyPose[1].scale==0 && bodyPose[2].scale==0);
    for(float axis:bodyPose[1].rotation)CHECK(axis==0);
    CHECK(bodyPose[3].scale==1 && bodyPose[4].scale==1 && bodyPose[4].translation.x==4);
    nodes[0].parent=4;CHECK(!HideSeatedHead(nodes.data(),5,bodyPose.data()));

    PhysicalGrenades grenades;GrenadeInventory inventory{42,100,{1,1},0,0};grenades.Observe(inventory);
    CHECK(grenades.Grab(0,0,inventory));CHECK(!grenades.Grab(1,0,inventory));CHECK(grenades.Grab(1,1,inventory));
    DropPose pose;pose.velocity={5,0,0};CHECK(grenades.Release(0,false,pose));CHECK(grenades.Release(1,false,pose));
    CHECK(grenades.release.type==0 && grenades.release.hand==0);CHECK(grenades.Reserved(1)==1);
    grenades.Finish();CHECK(grenades.pending && grenades.release.type==1 && grenades.release.hand==1);
    grenades.Tick(3);CHECK(!grenades.pending);CHECK(grenades.Grab(0,0,inventory));inventory.unit=99;grenades.Observe(inventory);CHECK(grenades.held[0]<0);
    Point v=GrenadeVelocity({1,0,0},{.1f,0,0},2,7);CHECK(std::fabs(v.x-(4.f/30+.1f))<.0001f);
    v=GrenadeVelocity({300,0,0},{.1f,0,0},2,7);CHECK(std::fabs(v.x-(7.f/30+.1f))<.0001f);
    CHECK(GrenadeVelocity({0,0,0},{},2,7).LengthSquared()==0);
    // Real game metres/world/tick conversion: existing saved 1.3 gains
    // twice the launch velocity until capped, without a minimum throw speed.
    const float worldPerMetre=1.f/3.048f;
    for(float metres:{0.f,.2f,2.f,6.f,12.f,100.f}){
        const Point inherited{.03f,-.02f,.01f};
        const auto launched=GrenadeVelocity({metres*worldPerMetre,0,0},inherited,1.3f,24*worldPerMetre);
        const float actual=(launched.x-inherited.x)*30/worldPerMetre;
        CHECK(std::fabs(actual-(std::min)(24.f,metres*2.6f))<.0001f);
        CHECK(launched.y==inherited.y && launched.z==inherited.z);
    }
    ThrowVelocityHistory history;history.Add({5,0,0},.01f);history.Add({},.01f);CHECK(history.Release({}).x==5);
    for(int i=0;i<12;++i)history.Add({},.01f);
    CHECK(history.Release({}).LengthSquared()==0);
    history.Add({6,0,0},.01f);history.Add({},1);CHECK(history.Release({}).LengthSquared()==0);
    for(int yaw=-180;yaw<=180;yaw+=10)for(int pitch=-90;pitch<=90;pitch+=10){
        Matrix4 palm;palm.rotateY(float(pitch)).rotateZ(float(yaw)).translate({1,2,3});
        const auto rocket=RocketPalmGrasp(palm,1);CHECK(std::fabs(rocket.getLeftAxis().dot(palm.getLeftAxis()))<.0001f);
        CHECK(rocket.getLeftAxis().dot(palm.getUpAxis())>.999f);CHECK((rocket*Vector3()-palm*Vector3()).length()<.05f);
    }
}
void renderOnlyPassengerTests(){
    auto& g=Game::instance;RetailWeaponEngine engine;bool triggers[2]{};
    for(float fps:{60.f,90.f,120.f})for(int seat:{0,1,2,7}){
        auto& adapter=g.interactions;adapter.OnRecentre();adapter.AttachEngine(engine);
        g.vr={};g.lastDeltaTime=1.f/fps;g.passengerLook={};Helpers::scene.bInCutscene=true;
        vehicle={};vehicle.unit=42;vehicle.vehicle=50;vehicle.kind=VehicleSeat::PelicanPassenger;vehicle.seat=seat;
        vehicle.position={10,20,29};vehicle.eye={10,20,30};vehicle.forward=vehicle.aim={1,0,0};vehicle.up={0,0,1};
        CameraFrustum a{},b{};a.position=b.position=Helpers::camera.position={10,20,30};
        // No UpdateVehicle/input callback occurs during this scripted intro.
        adapter.BeginView(a,b);CHECK(adapter.VehicleActive() && adapter.ViewOverrideActive());adapter.EndView();
        g.passengerLook={1,1};const auto start=g.vr.yaw;
        for(unsigned n=0;n<unsigned(fps);++n){adapter.BeginView(a,b);adapter.EndView();}
        CHECK(std::fabs(g.vr.yaw-start-90.f)<.01f);CHECK(g.vr.inputUpdates>=unsigned(fps));
        adapter.BeginView(a,b);Matrix4 head=g.vr.GetHMDTransform();const auto pos=head*Vector3();adapter.ApplyViewRotation(head);
        CHECK(head.getLeftAxis().z>.85f);CHECK((head*Vector3()-pos).length()<.00001f);adapter.EndView();
        CHECK(vehicle.position.x==10 && vehicle.position.y==20); // Never steer/move the script's vehicle.
        g.passengerLook={};vehicle.kind=VehicleSeat::None;adapter.UpdateVehicle(engine,false,triggers,true);
        Helpers::scene.bInCutscene=false;
    }
    g.interactions.OnRecentre();vehicle.kind=VehicleSeat::None;scorpion=true;
    for(bool lt:{false,true})for(bool rt:{false,true}){
        triggers[0]=lt;triggers[1]=rt;Helpers::controls.Grenade=127;Helpers::controls.Jump=127;
        g.interactions.UpdateVehicle(engine,true,triggers,true);
        CHECK(Helpers::controls.Fire==(rt?127:0));CHECK(Helpers::controls.Grenade==(lt?127:0));CHECK(!Helpers::controls.Jump && !Helpers::controls.Zoom);
        CHECK(!g.interactions.VehicleActive()); // Preserve the existing tank camera.
    }
    Helpers::menu=true;g.interactions.UpdateVehicle(engine,true,triggers,true);CHECK(!Helpers::controls.Fire && !Helpers::controls.Grenade);Helpers::menu=false;scorpion=false;
}
int main(){renderOnlyPassengerTests();transitionTests();rulesTests();grenadeTests();vehicleTests();seatEntryTests();std::cout<<checks<<" production grenade/vehicle/rocket-grasp checks passed (VR/native doubles)\n";}
