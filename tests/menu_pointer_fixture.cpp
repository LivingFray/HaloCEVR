#include "ThirdParty/OpenVR/include/openvr.h"
#include "HaloCEVR/Immersion/MenuPointer.h"
#include "HaloCEVR/Maths/Vectors.h"
#include <iostream>
#include <cstdlib>
struct SystemDouble {
    bool valid=true;
    vr::TrackedDeviceIndex_t GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole r) {
        return valid ? (r==vr::TrackedControllerRole_LeftHand?1:2) : vr::k_unTrackedDeviceIndexInvalid;
    }
};
struct OverlayDouble {
    vr::VROverlayIntersectionParams_t last{};
    bool hit=true;
    bool ComputeOverlayIntersection(vr::VROverlayHandle_t,vr::VROverlayIntersectionParams_t* p,vr::VROverlayIntersectionResults_t* r) {
        last=*p;r->vUVs={{0.2f,0.3f}};return hit;
    }
};
class OpenVR {
public:
    SystemDouble* vrSystem=nullptr;
    int* vrInput=nullptr;
    OverlayDouble* vrOverlay=nullptr;
    bool bMouseVisible=true,bMouseDown=false,bMousePositionValid=false;
    Vector2 mousePos;
    Immersion::MenuPointer menuPointer;
    struct Binding{unsigned id=0;};std::array<Binding,7> physicalBindings{};
    std::array<bool,2> down{},valid{{true,true}};
    vr::TrackedDevicePose_t renderPoses[vr::k_unMaxTrackedDeviceCount]{};
    vr::VROverlayHandle_t uiOverlay=3;
    bool TryGetBoolInput(unsigned id,bool& result) {result=down[id];return valid[id];}
    void UpdateMenuPointer();
};
// INSERT_PRODUCTION_METHOD
unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main() {
    for(unsigned hand=0;hand<2;++hand) {
        OpenVR o;SystemDouble system;OverlayDouble overlay;int input=0;
        o.vrSystem=&system;o.vrOverlay=&overlay;o.vrInput=&input;
        o.physicalBindings[2].id=0;o.physicalBindings[3].id=1;
        auto& pose=o.renderPoses[hand+1];pose.bDeviceIsConnected=pose.bPoseIsValid=true;
        pose.mDeviceToAbsoluteTracking.m[0][3]=2.0f;
        pose.mDeviceToAbsoluteTracking.m[1][3]=1.5f;
        pose.mDeviceToAbsoluteTracking.m[2][2]=1.0f;
        o.down[hand]=true;o.UpdateMenuPointer();CHECK(!o.bMouseDown);
        CHECK(o.bMousePositionValid);CHECK(std::abs(o.mousePos.y-0.7f)<1e-5f);
        CHECK(overlay.last.eOrigin==vr::TrackingUniverseStanding);
        CHECK(overlay.last.vSource.v[0]==2.0f && overlay.last.vSource.v[1]==1.5f);
        CHECK(overlay.last.vDirection.v[2]==-1.0f);
        o.down[hand]=false;o.UpdateMenuPointer();o.down[hand]=true;o.UpdateMenuPointer();CHECK(o.bMouseDown);
        overlay.hit=false;o.UpdateMenuPointer();CHECK(!o.bMouseDown && !o.bMousePositionValid);
        overlay.hit=true;o.UpdateMenuPointer();CHECK(!o.bMouseDown);
        o.down[hand]=false;o.UpdateMenuPointer();o.down[hand]=true;o.UpdateMenuPointer();CHECK(o.bMouseDown);
        pose.bPoseIsValid=false;o.UpdateMenuPointer();CHECK(!o.bMouseDown && !o.bMousePositionValid);
        system.valid=false;o.UpdateMenuPointer();CHECK(!o.bMouseDown);
    }
    std::cout<<checks<<" production controller-to-menu checks passed (OpenVR doubles)\n";
}
