#include "HaloCEVR/Immersion/HolsterMarker.h"
#include "HaloCEVR/Maths/Matrices.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <string>
enum class ControllerRole {Left,Right};
namespace Helpers {
    bool ready=false,loading=false;
    unsigned cutsceneReads=0,cameraReads=0;
    struct Cutscene {bool bInCutscene=false;} scene;
    struct Camera {Vector3 position{10,20,30};} camera;
    Cutscene* GetCutsceneData(){++cutsceneReads;if(!ready)throw std::runtime_error("Startup access");return &scene;}
    bool IsCampaignLoading(){return loading;}
    bool IsMouseVisible(){return false;}
    Camera& GetCamera(){++cameraReads;return camera;}
}
struct VRDouble {
    bool tracked=true;
    bool IsHMDTracked()const{return tracked;}
    bool IsControllerTracked(ControllerRole)const{return tracked;}
    Matrix4 GetHMDTransform()const{return Matrix4();}
    Matrix4 GetControllerTransform(ControllerRole hand,bool)const {
        return Matrix4().translate({0.1f,hand==ControllerRole::Left?0.2f:-0.2f,-0.3f});
    }
};
struct InputDouble {
    unsigned bodyDraws=0;void DrawOnFootBody(){++bodyDraws;}
    bool canRender=false,empty=true;unsigned draws=0;bool hands=false;
    Immersion::Object hip=Immersion::None;Matrix4 lastHip,lastShoulder;Matrix4 lastHands[2];
    Immersion::Object shoulder=Immersion::None;bool grey=false;
    bool CanRenderEmptyHands()const{return canRender&&empty;}
    bool PhysicalVisualsActive()const{return canRender;}
    std::array<Immersion::Object,2> owners{{13,12}};
    std::array<Immersion::HolsterMarker,2> HolsterMarkers()const{return {{{owners[0],false},{owners[1],!empty}}};}
    float HolsterYaw()const{return 0;}
    bool DrawBodyModels(bool h,const std::array<Immersion::HolsterMarker,2>& markers,const Matrix4* poses,const Matrix4* markerPoses) {
        ++draws;hands=h;hip=markers[1].weapon;shoulder=markers[0].weapon;grey=markers[1].equipped;lastHip=markerPoses[1];lastShoulder=markerPoses[0];lastHands[0]=poses[0];lastHands[1]=poses[1];return true;
    }
};
template<class T>struct PropertyDouble {T value{};T Value()const{return value;}};
struct Font {};
struct UIDouble { Font font; Font* GetSmallFont(){return &font;} };
struct LabelDouble {
    struct Label {std::string text;Vector3 center,right,up;bool depth;};
    std::vector<Label> labels;
    void DrawLabel3D(const Font&,const char* text,const Vector3& center,const Vector3& right,
        const Vector3& up,float,float=0,bool depth=false){labels.push_back({text,center,right,up,depth});}
};
class Game {
public:
    struct Interaction{bool VehicleActive()const{return false;}void DrawNative(){}unsigned GrenadeHandMask()const{return 0;}} interactions;
    VRDouble* vr=nullptr;bool bNeedsRecentre=false,bUse3DOFAiming=false;InputDouble inputHandler;
    float MetresToWorld(float v)const{return v*0.5f;}
    PropertyDouble<bool> body{false};PropertyDouble<bool>* c_OnFootLegs=&body;
    PropertyDouble<bool> markers{true},left{false};PropertyDouble<Vector3> rotation,offset;
    PropertyDouble<bool>* c_PhysicalHolsterMarkers=&markers,*c_LeftHanded=&left;
    PropertyDouble<Vector3>* c_ControllerRotation=&rotation,*c_ControllerOffset=&offset;
    UIDouble ui; UIDouble* uiRenderer=&ui; LabelDouble inGameRenderer;
    void QueuePhysicalHolsterLabels();
    struct Reload {unsigned draws=0;void DrawNativePieces(){++draws;}} reloadHandler;
    void DrawPhysicalBodyModels();
};
// INSERT_PRODUCTION_METHOD
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    Game g;VRDouble vr;
    g.DrawPhysicalBodyModels();CHECK(Helpers::cutsceneReads==0);
    g.vr=&vr;g.DrawPhysicalBodyModels();CHECK(Helpers::cutsceneReads==0);
    g.inputHandler.canRender=true;g.bUse3DOFAiming=true;g.DrawPhysicalBodyModels();CHECK(Helpers::cutsceneReads==0);
    g.bUse3DOFAiming=false;vr.tracked=false;g.DrawPhysicalBodyModels();CHECK(Helpers::cutsceneReads==0);
    vr.tracked=true;Helpers::ready=true;Helpers::scene.bInCutscene=true;
    g.DrawPhysicalBodyModels();CHECK(g.inputHandler.draws==0);CHECK(Helpers::cameraReads==0);
    Helpers::scene.bInCutscene=false;Helpers::loading=true;g.DrawPhysicalBodyModels();CHECK(g.inputHandler.draws==0);
    Helpers::loading=false;g.DrawPhysicalBodyModels();CHECK(g.inputHandler.draws==1);CHECK(g.inputHandler.hands);
    CHECK(g.inputHandler.hip==12);CHECK(g.inputHandler.shoulder==13);CHECK(!g.inputHandler.grey);
    CHECK((g.inputHandler.lastShoulder*Vector3()-Vector3(10.105f,20,29.785f)).length()<0.00001f);
    CHECK(g.inputHandler.lastShoulder*Vector3(1,0,0)-g.inputHandler.lastShoulder*Vector3()==Vector3(0,1,0));
    const auto down=g.inputHandler.lastHip*Vector3(1,0,0)-g.inputHandler.lastHip*Vector3();
    CHECK(down==Vector3(0,0,-1)); // The stored weapon points down.
    CHECK((g.inputHandler.lastHands[0]*Vector3()-Vector3(10.05f,20.1f,29.85f)).length()<0.00001f);
    g.inputHandler.empty=false;g.DrawPhysicalBodyModels();CHECK(!g.inputHandler.hands);CHECK(g.inputHandler.hip==12);CHECK(g.inputHandler.grey);
    g.markers.value=false;g.DrawPhysicalBodyModels();CHECK(g.inputHandler.hip==Immersion::None);CHECK(g.inputHandler.shoulder==Immersion::None);
    g.inputHandler.canRender=false;Helpers::ready=false;const auto reads=Helpers::cutsceneReads;
    g.DrawPhysicalBodyModels();CHECK(g.inputHandler.draws==3);CHECK(Helpers::cutsceneReads==reads);
    // Labels are queued once per frame, never by either eye's body-model pass.
    g.QueuePhysicalHolsterLabels();CHECK(g.inGameRenderer.labels.empty());
    g.inputHandler.canRender=true;Helpers::ready=true;g.markers.value=true;
    g.QueuePhysicalHolsterLabels();CHECK(g.inGameRenderer.labels.size()==2);
    CHECK(g.inGameRenderer.labels[0].text=="Right Shoulder");CHECK(g.inGameRenderer.labels[1].text=="Right Hip (Here)");
    CHECK(g.inGameRenderer.labels[0].center.x<g.inputHandler.lastShoulder[12]);
    g.inGameRenderer.labels.clear();g.left.value=true;g.inputHandler.owners[1]=Immersion::None;
    g.QueuePhysicalHolsterLabels();CHECK(g.inGameRenderer.labels.size()==3);
    CHECK(g.inGameRenderer.labels[0].text=="Left Shoulder");CHECK(g.inGameRenderer.labels[1].text=="Left Hip (Here)");
    CHECK(g.inGameRenderer.labels[2].text=="EMPTY");CHECK(g.inGameRenderer.labels[2].center.y>20);
    g.inGameRenderer.labels.clear();g.inputHandler.owners[0]=Immersion::None;
    g.QueuePhysicalHolsterLabels();CHECK(g.inGameRenderer.labels.size()==4);
    CHECK(g.inGameRenderer.labels[1].text=="EMPTY");CHECK(g.inGameRenderer.labels[3].text=="EMPTY");
    for(const auto& label:g.inGameRenderer.labels)CHECK(label.depth);
    g.body.value=true;g.DrawPhysicalBodyModels();CHECK(g.inputHandler.bodyDraws==1);
    g.body.value=false;g.DrawPhysicalBodyModels();CHECK(g.inputHandler.bodyDraws==1);
    for(unsigned fault=0;fault<4;++fault) {
        g.inGameRenderer.labels.clear();g.markers.value=fault!=0;Helpers::loading=fault==1;
        Helpers::scene.bInCutscene=fault==2;vr.tracked=fault!=3;
        g.QueuePhysicalHolsterLabels();CHECK(g.inGameRenderer.labels.empty());
    }
    std::cout<<checks<<" production body-model lifecycle/placement checks passed\n";
}
