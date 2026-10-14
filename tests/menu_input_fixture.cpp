// Compile the real UpdateInputs and UpdateMenuInput with native/Win32 doubles.
#include "HaloCEVR/Immersion/InteractionModel.h"
#include "HaloCEVR/Helpers/Controls.h"
#include "HaloCEVR/Maths/Matrices.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <cstdlib>
using InputBindingID=unsigned long long;
enum class ControllerRole {Left,Right};
struct INPUT {unsigned type=0;struct {unsigned wScan=0,dwFlags=0;} ki;};
constexpr unsigned INPUT_KEYBOARD=1,KEYEVENTF_SCANCODE=8,KEYEVENTF_KEYUP=2;
static unsigned downs=0,ups=0;static bool injectionPass=true;
static bool validEscape=true;
unsigned SendInput(unsigned count,INPUT* event,int size){
    validEscape=validEscape && count==1 && size==sizeof(INPUT) && event->type==INPUT_KEYBOARD && event->ki.wScan==1 && (event->ki.dwFlags&KEYEVENTF_SCANCODE);
    if(!injectionPass)return 0;
    if(event->ki.dwFlags&KEYEVENTF_KEYUP)++ups;else ++downs;
    return 1;
}
#define D3DCOLOR_XRGB(r,g,b) 0u
namespace Logger {std::ostringstream log;}
namespace Helpers {
    Controls normal{};Controls_Custom custom{};
    Controls& GetControls(){return normal;}Controls_Custom& GetControlsCustom(){return custom;}
    struct Camera {Vector3 position,lookDir{1,0,0};} camera;
    Camera& GetCamera(){return camera;}
}
struct VRDouble {
    bool buttons[32]{},valid=true;
    bool IsHMDTracked()const{return true;}bool IsControllerTracked(ControllerRole)const{return true;}
    unsigned buzz=0;void TriggerHapticVibration(ControllerRole,float,float,float,float){++buzz;}
    void UpdateInputs(){}
    bool GetBoolInput(InputBindingID id){return buttons[id];}
    bool GetBoolInput(InputBindingID id,bool& changed){changed=false;return buttons[id];}
    bool TryGetBoolInput(InputBindingID id,bool& value){value=buttons[id];return valid;}
    Matrix4 GetControllerTransform(ControllerRole,bool=false){return {};}
    Vector2 GetVector2Input(InputBindingID){return {};}
};
using IVR=VRDouble;
using Camera=Helpers::Camera;
float AngleBetweenVector2(Vector2,Vector2){return 0;}
Vector2 RotateVector2(Vector2 v,float){return v;}
struct Property{bool value=false;bool Value()const{return value;}};
struct Game {
    static Game instance;VRDouble vr;
    struct Interaction{bool ghost=false;bool GhostActive()const{return ghost;}bool VehicleActive()const{return ghost;}void GrenadeControls(){}template<class... T>void UpdateVehicle(T&&...){}} interactions;
    Property physicalGrenades;Property* c_PhysicalGrenades=&physicalGrenades;
    bool bIsCustom=false,bLeftHanded=false,bIsFiring=false,bNeedsRecentre=false;
    Property holsters,movement,offset;Property* c_EnableWeaponHolsters=&holsters;
    Property* c_HandRelativeMovement=&movement;Property* c_HandRelativeOffsetRotation=&offset;
    struct Renderer {unsigned draws=0;template<class... T>void DrawPolygon(T...){++draws;}} inGameRenderer;
    float MetresToWorld(float v)const{return v*.5f;}
    VRDouble* GetVR(){return &vr;}
};
Game Game::instance;
#define ApplyBoolInput(x) controls.x = vr->GetBoolInput(x) ? 127 : 0
#define ApplyImpulseBoolInput(x) controls.x = vr->GetBoolInput(x,bHasChanged) && bHasChanged ? 127 : 0
class InputHandler {
public:
    // INSERT_PRODUCTION_MENU_STATE
    int physicalEngine=0;
    InputBindingID PhysicalLeftGrip=17,PhysicalRightGrip=18,PhysicalLeftTrigger=15;
    bool physicalWeaponsEnabled=true,physicalControlsActive=false,physicalFrameValid=true,physicalCanFire=true;
    bool simulatePhysical=true;
    Immersion::Button menuConfirm,physicalGrenadeCycle;
    bool physicalCycleArmed=false;
    std::array<bool,2> headGripConsumed{};
    InputBindingID Jump=0,SwitchGrenades=0,Interact=0,Melee=0,Flashlight=0,Grenade=0,Fire=0,MenuForward=0,Crouch=0,Zoom=0,Reload=0,SwitchWeapons=0;
    InputBindingID PhysicalRecentre=10,PhysicalInteract=19;
    InputBindingID MenuBack=10,PhysicalGrenadeCycle=11,PhysicalJump=12,Move=0,PhysicalMove=20,PhysicalRightTrigger=14,PhysicalGrenade=16;
    void UpdateInputs(bool);
    bool UpdatePhysicalWeapons(bool){if(simulatePhysical)UpdatePhysicalButtons();return simulatePhysical;}
    void UpdatePhysicalButtons();
    unsigned char UpdateFlashlight(){return 0;}
    unsigned char UpdateHolsterSwitchWeapons(){return 0;}
    unsigned char UpdateMelee(){return 0;}
    unsigned char UpdateCrouch(){return 0;}
    void UpdateHandsProximity(){}
};
// INSERT_PRODUCTION_MENU_METHODS
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
void reset(){Game::instance.vr={};Game::instance.interactions.ghost=false;Game::instance.bNeedsRecentre=false;Helpers::normal={};Helpers::custom={};downs=ups=0;injectionPass=true;}
void tick(InputHandler& input){input.UpdateInputs(false);}
void releaseEscape(InputHandler& input){input.menuEscapeReleaseTime=std::chrono::steady_clock::now()-std::chrono::milliseconds(1);tick(input);}
int main(){
    for(bool custom:{false,true}){
        reset();Game::instance.bIsCustom=custom;
        InputHandler input;input.physicalWeaponsEnabled=false;input.simulatePhysical=false;
        const auto key=input.MenuBack;auto& vr=Game::instance.vr;
        tick(input);
        // Original HandleInputs has already populated Escape. Never erase it.
        Helpers::normal.MenuBack=1;Helpers::custom.MenuBack=1;tick(input);
        CHECK(Helpers::normal.MenuBack==1);CHECK(Helpers::custom.MenuBack==1);
        Helpers::normal.MenuBack=0;Helpers::custom.MenuBack=0;
        vr.buttons[key]=true;tick(input);CHECK(downs==0);
        vr.buttons[key]=false;tick(input);CHECK(downs==1);CHECK(input.bHoldingMenu);
        tick(input);CHECK(ups==0);CHECK(downs==1);
        Helpers::normal.MenuBack=1;Helpers::custom.MenuBack=1;tick(input);
        CHECK((custom?Helpers::custom.MenuBack:Helpers::normal.MenuBack)==1);
        vr.valid=false;releaseEscape(input);CHECK(ups==1);CHECK(!input.bHoldingMenu);
        vr.valid=true;tick(input);vr.buttons[key]=true;tick(input);vr.valid=false;tick(input);
        vr.valid=true;vr.buttons[key]=false;tick(input);CHECK(downs==1);
        vr.buttons[key]=true;tick(input);input.menuHeldTime-=std::chrono::milliseconds(5100);tick(input);
        vr.buttons[key]=false;tick(input);CHECK(Game::instance.bNeedsRecentre);CHECK(downs==1);
    }
    for(bool left:{false,true}){
        reset();Game::instance.bIsCustom=false;Game::instance.bLeftHanded=left;
        InputHandler input;input.Reload=left?10:13;input.Fire=left?15:14;input.Zoom=left?14:15;
        auto& vr=Game::instance.vr;tick(input);tick(input);
        // Reload/Y is independent of menu even for left-handed guns.
        vr.buttons[input.Reload]=true;tick(input);CHECK(Helpers::normal.Reload==(left?0:127));CHECK(downs==0);
        vr.buttons[input.Reload]=false;tick(input);CHECK(Helpers::normal.Reload==(left?127:0));
        Helpers::normal.MenuBack=1;tick(input);CHECK(Helpers::normal.MenuBack==1);Helpers::normal.MenuBack=0;
        // X alone cycles on release, not on press.
        vr.buttons[11]=true;tick(input);CHECK(Helpers::normal.SwitchGrenades==0);CHECK(downs==0);
        vr.buttons[11]=false;tick(input);CHECK(Helpers::normal.SwitchGrenades==127);tick(input);CHECK(Helpers::normal.SwitchGrenades==0);
        // Hold X THEN press RT. Menu input is resolved before gameplay fire.
        vr.buttons[11]=true;tick(input);vr.buttons[14]=true;tick(input);
        CHECK(downs==1);CHECK(Helpers::normal.Fire==0);CHECK(Helpers::normal.Zoom==0);CHECK(Helpers::normal.SwitchGrenades==0);
        tick(input);CHECK(downs==1);CHECK(ups==0);
        vr.buttons[11]=false;tick(input);CHECK(Helpers::normal.SwitchGrenades==0);CHECK(input.menuRightTriggerBlocked);
        releaseEscape(input);CHECK(ups==1);CHECK(Helpers::normal.Fire==0);
        vr.buttons[14]=false;tick(input);CHECK(!input.menuRightTriggerBlocked);
        // A fresh trigger works after leaving the chord. No sticky suppression.
        vr.buttons[input.Fire]=true;tick(input);CHECK(Helpers::normal.Fire==127);
        vr.buttons[input.Fire]=false;tick(input);
        // RT already held before X does not count as a menu request.
        vr.buttons[14]=true;tick(input);vr.buttons[11]=true;tick(input);CHECK(downs==1);
        vr.buttons[14]=false;tick(input);vr.buttons[14]=true;tick(input);CHECK(downs==2);
        vr.buttons[11]=false;vr.buttons[14]=false;tick(input);CHECK(Helpers::normal.SwitchGrenades==0);
        releaseEscape(input);CHECK(ups==2);
        // Tracking/binding loss cannot fabricate a trigger edge or grenade cycle.
        vr.buttons[11]=true;tick(input);vr.valid=false;tick(input);vr.buttons[14]=true;vr.valid=true;tick(input);CHECK(downs==2);
        vr.buttons[11]=false;vr.buttons[14]=false;tick(input);CHECK(Helpers::normal.SwitchGrenades==0);
        // Injection failure is visible in the log and leaves no pressed key.
        injectionPass=false;vr.buttons[11]=true;tick(input);vr.buttons[14]=true;tick(input);
        CHECK(!input.bHoldingMenu);CHECK(Logger::log.str().find("injection failed")!=std::string::npos);
        injectionPass=true;vr.buttons[14]=false;tick(input);vr.buttons[14]=true;tick(input);CHECK(downs==3);
        vr.valid=false;releaseEscape(input);CHECK(ups==3);CHECK(!input.bHoldingMenu);
    }
    for(bool ghost:{false,true})for(bool left:{false,true}){
        reset();Game::instance.bIsCustom=false;Game::instance.bLeftHanded=left;Game::instance.interactions.ghost=ghost;
        InputHandler input;input.Reload=left?10:13;auto& vr=Game::instance.vr;tick(input);tick(input);
        if(ghost){
            vr.buttons[11]=true;tick(input);vr.buttons[14]=true;tick(input);
            CHECK(downs==0);CHECK(!input.menuRightTriggerBlocked); // X+RT remains shoot+throttle.
            vr.buttons[11]=vr.buttons[14]=false;tick(input);
        }
        vr.buttons[19]=true;tick(input);CHECK(Helpers::normal.Interact==127);
        vr.buttons[19]=false;tick(input);CHECK(Helpers::normal.Interact==0);
        vr.buttons[10]=true;tick(input);CHECK(Helpers::normal.Interact==0);CHECK(!input.recentreTap);CHECK(!Game::instance.bNeedsRecentre);
        input.recentreHeldTime-=std::chrono::milliseconds(4800);tick(input);CHECK(!Game::instance.bNeedsRecentre);
        input.recentreHeldTime-=std::chrono::milliseconds(300);tick(input);CHECK(Game::instance.bNeedsRecentre);
        CHECK(vr.buzz==1);CHECK(!input.recentreTap);CHECK(downs==0);
        Game::instance.bNeedsRecentre=false;tick(input);CHECK(!Game::instance.bNeedsRecentre && vr.buzz==1);
        vr.buttons[10]=false;tick(input);CHECK(!input.recentreTap);CHECK(downs==0); // Recenter release cannot eject or open menu.
        vr.buttons[10]=true;tick(input);vr.buttons[10]=false;tick(input);
        CHECK(input.recentreTap);CHECK(downs==(ghost?1u:0u));
        if(!ghost)CHECK(Helpers::normal.Reload==(left?127:0));
        releaseEscape(input);
        vr.buttons[10]=true;tick(input);vr.valid=false;tick(input);vr.valid=true;
        input.recentreHeldTime-=std::chrono::seconds(6);tick(input);CHECK(!Game::instance.bNeedsRecentre);CHECK(vr.buzz==1);
        vr.buttons[10]=false;tick(input);CHECK(!input.recentreTap);
    }
    CHECK(validEscape);
    std::cout<<checks<<" production X/RT menu, reload separation, trigger suppression and native Escape checks passed (Win32/VR doubles)\n";
}
