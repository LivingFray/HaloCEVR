// run.py inserts the actual UpdateGripFlashlight method at the marker below.
// Engine/VR doubles test routing and geometry, NOT native Halo behavior.
#include "HaloCEVR/Immersion/InteractionModel.h"
#include "HaloCEVR/Maths/Matrices.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
enum class ControllerRole { Left, Right };
using InputBindingID = unsigned long long;
struct FloatProperty { float value=0.14f; float Value() const {return value;} };
struct BoolProperty { bool value=false; bool Value() const {return value;} };
struct VRDouble {
    bool tracked[2]{true,true}; bool grip[2]{};
    Vector3 positions[2]{{0,0,-0.6f},{0,0,-0.6f}};
    Matrix4 head;
    unsigned vibrations[2]{};
    bool IsHMDTracked()const{return true;}
    bool IsControllerTracked(ControllerRole h) const {return tracked[static_cast<unsigned>(h)];}
    Matrix4 GetHMDTransform() const {return head;}
    Matrix4 GetRawControllerTransform(ControllerRole h) const {return Matrix4().translate(positions[static_cast<unsigned>(h)]);}
    bool inputValid=true;
    bool TryGetBoolInput(InputBindingID id,bool& down) const {down=grip[id-1];return inputValid;}
    void TriggerHapticVibration(ControllerRole h,float,float,float,float) {++vibrations[static_cast<unsigned>(h)];}
};
using IVR=VRDouble;
struct Game {
    static Game instance;
    struct Interaction{unsigned mask=0;unsigned GrenadeHandMask()const{return mask;}} interactions;
    bool bNeedsRecentre=false;
    bool bLeftHanded=false;
    BoolProperty layout;
    BoolProperty* c_LeftHanded=&layout;
    FloatProperty radius;
    FloatProperty* c_GripHeadFlashlightRadius=&radius;
    VRDouble vr;
    IVR* GetVR() {return &vr;}
};
Game Game::instance;
namespace Helpers {
    struct Player {float health=1;} player;
    struct Cutscene {bool bInCutscene=false;} cutscene;
    bool menu=false, loading=false;
    const Cutscene* GetCutsceneData() {return &cutscene;}
    const Player* GetLocalPlayer() {return &player;}
    bool IsMouseVisible() {return menu;}
    bool IsCampaignLoading() {return loading;}
}
class InputHandler {
public:
    unsigned char UpdateGripFlashlight();
    InputBindingID PhysicalLeftGrip=1,PhysicalRightGrip=2;
    Immersion::HeadTouch headTouch;float flashlightPulse=0;
    std::array<Immersion::Button,2> headGripButtons;
    std::array<bool,2> headGripConsumed{};
    std::chrono::steady_clock::time_point lastHeadTouchUpdate{};
};
// Game.h includes Windows headers before the production method. Reproduce
// their legacy empty macro here: using "near" as a local variable must fail
// this portable fixture too, instead of slipping through until an MSVC build.
#define near
// INSERT_PRODUCTION_METHOD
#undef near
static unsigned checks=0;
#define CHECK(x) do {++checks; if(!(x)) {std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}} while(false)
static unsigned char tick(InputHandler& input,float dt=.05f){
    input.lastHeadTouchUpdate=std::chrono::steady_clock::now()-std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(dt));
    return input.UpdateGripFlashlight();
}
int main(){
    for(bool leftHanded:{false,true})for(unsigned hand=0;hand<2;++hand)for(float yaw:{0.f,90.f,180.f}){
        Game::instance.vr={};auto& vr=Game::instance.vr;InputHandler input;
        Game::instance.layout.value=leftHanded;Game::instance.bLeftHanded=!leftHanded;
        const float side=leftHanded?-.12f:.12f;
        vr.head.rotateZ(yaw).translate({1,2,3});tick(input);
        vr.tracked[1-hand]=false; // Other controller occlusion must not disable the gesture.
        vr.positions[hand]=vr.head*Vector3(-.04f,side,-.03f);tick(input);
        vr.grip[hand]=true;CHECK(tick(input)==127);CHECK(vr.vibrations[hand]==1);CHECK(input.headGripConsumed[hand]);
        vr.grip[hand]=false;CHECK(tick(input)==127); // Survives native 30-Hz sampling.
        tick(input);CHECK(tick(input)==0);CHECK(!input.headGripConsumed[hand]);
        vr.grip[hand]=true;CHECK(tick(input)==127);CHECK(vr.vibrations[hand]==2); // Fresh squeeze without leaving temple.
        for(int i=0;i<12;++i){tick(input);}CHECK(vr.vibrations[hand]==2);CHECK(tick(input)==0);
        vr.grip[hand]=false;tick(input);vr.positions[hand]=vr.head*Vector3(-.04f,-side,-.03f);vr.grip[hand]=true;
        CHECK(tick(input)==0);CHECK(vr.vibrations[hand]==2); // Opposite temple remains inactive.
    }
    for(unsigned hand=0;hand<2;++hand){
        Game::instance.layout.value=false;
        Game::instance.vr={};auto& vr=Game::instance.vr;InputHandler input;tick(input);
        vr.positions[hand]={-.04f,.12f,-.03f};Game::instance.interactions.mask=1u<<hand;vr.grip[hand]=true;
        CHECK(tick(input)==0);CHECK(vr.vibrations[hand]==0);Game::instance.interactions.mask=0;
        vr.grip[hand]=false;tick(input);vr.tracked[hand]=false;vr.grip[hand]=true;CHECK(tick(input)==0);
        vr.tracked[hand]=true;CHECK(tick(input)==0); // Recovery while held is not a fresh gesture.
        vr.grip[hand]=false;tick(input);vr.grip[hand]=true;CHECK(tick(input)==127);
        Helpers::menu=true;CHECK(tick(input)==0);Helpers::menu=false;CHECK(tick(input)==0);
        Helpers::player.health=0;CHECK(tick(input)==0);Helpers::player.health=1;
        Helpers::loading=true;CHECK(tick(input)==0);Helpers::loading=false;
        Helpers::cutscene.bInCutscene=true;CHECK(tick(input)==0);Helpers::cutscene.bInCutscene=false;
        Game::instance.bNeedsRecentre=true;CHECK(tick(input)==0);Game::instance.bNeedsRecentre=false;
    }
    std::cout<<"Production flashlight method with engine doubles: "<<checks<<" checks passed\n";
}
