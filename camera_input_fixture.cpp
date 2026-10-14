// Actual camera input functions: prevent headset/cinematic feedback and wrap yaw.
#include "HaloCEVR/Immersion/PhysicalInteractions.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
namespace Logger{std::ostringstream log;}
struct UnitDynamicObject{struct {unsigned id=0xffff;} parent;};
#define VR_PROFILE_SCOPE(x)
using InputBindingID=unsigned long long;
struct VRDouble{
    Vector2 stick{};Matrix4 head;float yaw=0;
    Vector2 GetVector2Input(InputBindingID){return stick;}
    float GetYawOffset()const{return yaw;}void SetYawOffset(float v){yaw=v;}
    Matrix4 GetHMDTransform()const{return Matrix4(head).rotateZ(-yaw);}
};
using IVR=VRDouble;
struct InputHandler{
    char lastSnapState=0;InputBindingID Look=0;unsigned vehicleCalls=0;
    void UpdateCamera(float&,float&);void UpdateCameraForVehicles(float& y,float& p){++vehicleCalls;y=.1f;p=.2f;}
};
namespace Helpers{UnitDynamicObject player;bool loading=false;
    void* GetLocalPlayer(){return &player;}bool IsCampaignLoading(){return loading;}
    struct Scene{bool bInCutscene=false;} scene;Scene* GetCutsceneData(){return &scene;}
    struct Camera{Vector3 lookDir{1,0,0};} camera;Camera& GetCamera(){return camera;}}
template<class T>struct Prop{T v;T Value()const{return v;}};
struct Game{
    static Game instance;VRDouble vr;InputHandler inputHandler;Immersion::VehicleExitGuard vehicleExit;
    struct Interactions{bool active=false;bool VehicleActive()const{return active;}} interactions;
    Prop<bool> snap{false};Prop<float> amount{45},smooth{90};
    Prop<bool>* c_SnapTurn=&snap;Prop<float>* c_SnapTurnAmount=&amount,*c_SmoothTurnAmount=&smooth;
    bool bInVehicle=false,bHasWeapon=true,bDetectedChimera=false;Vector3 LastLookDir{1,0,0};float lastDeltaTime=.01f;
    IVR* GetVR(){return &vr;}void UpdateCamera(float&,float&);
};
Game Game::instance;
// INSERT_CAMERA_INPUT
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    auto& g=Game::instance;float y=0,p=0;
    for(bool chimera:{false,true})for(float sign:{-1.f,1.f}){
        g.bDetectedChimera=chimera;g.vr.head=Matrix4().rotateZ(sign*179);
        Helpers::camera.lookDir=g.LastLookDir=Matrix4().rotateZ(-sign*179).getLeftAxis();
        g.UpdateCamera(y,p);CHECK(std::fabs(y+sign*2.f*3.14159265f/180.f)<.00001f);CHECK(p==0);
    }
    for(bool vehicle:{false,true}){
        g.interactions.active=vehicle;Helpers::scene.bInCutscene=true;g.vr.stick={1,1};
        const auto yaw=g.vr.yaw;const auto calls=g.inputHandler.vehicleCalls;y=p=100;g.UpdateCamera(y,p);
        CHECK(y==0 && p==0);CHECK(g.vr.yaw==yaw);CHECK(g.inputHandler.vehicleCalls==calls);
    }
    Helpers::scene.bInCutscene=false;g.UpdateCamera(y,p);CHECK(y==.1f && p==.2f);
    g.interactions.active=false;g.bInVehicle=false;g.vr.stick={};
    for(float fps:{60.f,72.f,90.f,120.f,144.f})for(bool chimera:{false,true}){
        g.vehicleExit.Reset();g.bDetectedChimera=chimera;g.vr.head=Matrix4().rotateZ(73).rotateY(25);g.vr.yaw=0;
        Helpers::player.parent.id=1;g.UpdateCamera(y,p); // Establish actual seat ownership.
        Helpers::player.parent.id=0xffff;
        unsigned frames=0;
        while(frames<unsigned(fps)){
            Helpers::camera.lookDir=g.LastLookDir=Matrix4().rotateZ(frames%2?170:-170).rotateY(frames%2?60:-60).getLeftAxis();
            g.UpdateCamera(y,p);CHECK(y==0 && p==0); // Animated reference never gets fed back.
            g.UpdateCamera(y,p);CHECK(y==0 && p==0); // Extra input hooks do not spend the cooldown.
            g.vehicleExit.Advance(1.f/fps);++frames;
        }
        g.vehicleExit.Advance(.01f);g.UpdateCamera(y,p);CHECK(y==0 && p==0);
        CHECK(Helpers::camera.lookDir.dot(g.vr.GetHMDTransform().getLeftAxis())>.9999f);
        CHECK(g.LastLookDir.dot(Helpers::camera.lookDir)>.9999f);
        g.UpdateCamera(y,p);CHECK(std::fabs(y)<.00001f && std::fabs(p)<.00001f);
        Helpers::player.parent.id=1;g.UpdateCamera(y,p);Helpers::player.parent.id=0xffff;g.UpdateCamera(y,p);
        CHECK(g.vehicleExit.Suppressing());Helpers::player.parent.id=1;g.UpdateCamera(y,p);CHECK(!g.vehicleExit.Suppressing());
        Helpers::loading=true;g.UpdateCamera(y,p);CHECK(!g.vehicleExit.Suppressing());Helpers::loading=false;
    }
    std::cout<<checks<<" production cinematic input and wrapped yaw checks passed\n";
}
