// run.py inserts the production OpenVR registration/read/update methods and binding state.
#include "ThirdParty/OpenVR/include/openvr.h"
#include <array>
#include <cstdint>
#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <limits>
#include "HaloCEVR/Maths/Vectors.h"
using InputBindingID=vr::VRActionHandle_t;
namespace Logger {std::ostringstream log;}
#define VR_PROFILE_SCOPE(x)
struct VRInputDouble {
    std::map<std::string,InputBindingID> actions;
    std::map<std::pair<InputBindingID,vr::VRInputValueHandle_t>,vr::InputDigitalActionData_t> samples;
    bool valid=true,sourceValid=true;
    std::map<std::pair<InputBindingID,vr::VRInputValueHandle_t>,vr::InputAnalogActionData_t> analog;
    std::map<std::pair<InputBindingID,vr::VRInputValueHandle_t>,vr::EVRInputError> analogFailures;
    unsigned analogReads=0;
    vr::EVRInputError GetAnalogActionData(InputBindingID id,vr::InputAnalogActionData_t* out,std::uint32_t,vr::VRInputValueHandle_t source){
        ++analogReads;
        const auto failure=analogFailures.find({id,source});
        if(failure!=analogFailures.end())return failure->second;
        auto it=analog.find({id,source});if(it==analog.end())return vr::VRInputError_InvalidHandle;
        *out=it->second;return vr::VRInputError_None;
    }
    vr::EVRInputError GetActionHandle(const char* name,InputBindingID* out){
        auto i=actions.find(name);if(i==actions.end()){*out=0;return vr::VRInputError_NameNotFound;}
        *out=i->second;return vr::VRInputError_None;
    }
    vr::EVRInputError GetInputSourceHandle(const char* name,vr::VRInputValueHandle_t* out){
        if(!sourceValid)return vr::VRInputError_NameNotFound;
        *out=std::string(name)=="/user/hand/left"?11:12;return vr::VRInputError_None;
    }
    vr::EVRInputError GetDigitalActionData(InputBindingID id,vr::InputDigitalActionData_t* out,
                                          std::uint32_t,vr::VRInputValueHandle_t source){
        auto i=samples.find({id,source});if(i==samples.end()){*out={};return vr::VRInputError_None;}
        *out=i->second;return vr::VRInputError_None;
    }
    vr::EVRInputError UpdateActionState(vr::VRActiveActionSet_t*,std::uint32_t,std::uint32_t){
        return valid?vr::VRInputError_None:vr::VRInputError_NoData;
    }
    void sample(InputBindingID action,vr::VRInputValueHandle_t hand,bool active,bool down){
        auto& value=samples[{action,hand}];value.bActive=active;value.bState=down;
    }
};
class OpenVR {
public:
    VRInputDouble* vrInput=nullptr;
    bool inputStateValid=false;
    vr::VRActiveActionSet_t actionSets[2]{};
    void SetActiveActionSet(int,const std::string&){}
    void UpdateInputs();
    InputBindingID RegisterBoolInput(std::string,std::string);
    bool TryGetBoolInput(InputBindingID,bool&);
    InputBindingID RegisterVector2Input(std::string,std::string);
    Vector2 GetVector2Input(InputBindingID);
    // INSERT_PRODUCTION_BINDING_STATE
};
// INSERT_PRODUCTION_METHODS
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    const char* names[]={"PhysicalLeftGrip","PhysicalRightGrip","PhysicalLeftTrigger","PhysicalRightTrigger","PhysicalJump","PhysicalGrenade","PhysicalGrenadeCycle","PhysicalRecentre","PhysicalInteract"};
    const char* aliases[]={"/actions/default/in/SwapWeaponHand","/actions/left handed/in/SwapWeaponHand",
                          "/actions/left handed/in/Fire","/actions/default/in/Fire","/actions/default/in/Grenade","/actions/default/in/Jump","/actions/default/in/SwitchGrenades","/actions/default/in/MenuBack","/actions/default/in/Interact"};
    for(unsigned slot=0;slot<9;++slot){
        VRInputDouble input;OpenVR vr;vr.vrInput=&input;
        const auto primaryName=std::string("/actions/default/in/")+names[slot];
        input.actions[primaryName]=100+slot;input.actions[aliases[slot]]=200+slot;
        const auto id=vr.RegisterBoolInput("default",names[slot]);const auto hand=(slot==0||slot==2||slot==6||slot==7)?11:12;
        CHECK(id==100+slot);CHECK(vr.physicalBindings[slot].source==hand);
        input.sample(200+slot,hand,true,true);vr.UpdateInputs();bool down=false;
        CHECK(vr.TryGetBoolInput(id,down));CHECK(down); // Old binding works with optional primary inactive.
        input.sample(id,hand,true,false);vr.UpdateInputs();
        CHECK(!vr.TryGetBoolInput(id,down));CHECK(!down); // Change source: suspend this whole sample.
        CHECK(!vr.TryGetBoolInput(id,down)); // Flashlight's earlier read cannot consume the suspension.
        vr.UpdateInputs();CHECK(vr.TryGetBoolInput(id,down));CHECK(!down); // Released primary beats held alias.
        input.sample(id,hand,true,true);vr.UpdateInputs();CHECK(vr.TryGetBoolInput(id,down));CHECK(down);
        input.valid=false;vr.UpdateInputs();CHECK(!vr.TryGetBoolInput(id,down));CHECK(!down);
        input.valid=true;input.samples.clear();input.sample(id,hand==11?12:11,true,true);
        vr.UpdateInputs();CHECK(!vr.TryGetBoolInput(id,down));CHECK(!down); // Other hand is never a fallback.
        input.actions.erase(primaryName);const auto fallbackID=vr.RegisterBoolInput("default",names[slot]);
        CHECK(fallbackID==200+slot);input.sample(fallbackID,hand,true,true);vr.UpdateInputs();
        CHECK(vr.TryGetBoolInput(fallbackID,down));CHECK(down);
        input.sourceValid=false;vr.RegisterBoolInput("default",names[slot]);vr.UpdateInputs();
        CHECK(!vr.TryGetBoolInput(fallbackID,down));CHECK(!down);
    }
    VRInputDouble input;OpenVR vr;vr.vrInput=&input;input.actions["/actions/default/in/Reload"]=7;
    CHECK(vr.RegisterBoolInput("default","Reload")==7);input.sample(7,0,true,true);vr.UpdateInputs();
    bool down=false;CHECK(vr.TryGetBoolInput(7,down));CHECK(down);CHECK(!vr.TryGetBoolInput(0,down));
    input.actions["/actions/default/in/Move"]=601;
    CHECK(vr.RegisterVector2Input("default","Move")==601);
    CHECK(vr.RegisterVector2Input("default","Unknown")==0);
    auto& regular=input.analog[{601,0}];regular.bActive=true;regular.x=.75f;regular.y=-.5f;
    const auto good=vr.GetVector2Input(601);CHECK(good.x==.75f && good.y==-.5f);
    auto absent=vr.GetVector2Input(0);CHECK(absent.x==0 && absent.y==0);
    regular.bActive=false;absent=vr.GetVector2Input(601);CHECK(absent.x==0 && absent.y==0);
    regular.bActive=true;regular.x=std::numeric_limits<float>::quiet_NaN();absent=vr.GetVector2Input(601);CHECK(absent.x==0 && absent.y==0);
    const auto diagnosticStart=Logger::log.str().size();
    for(int repeat=0;repeat<1000;++repeat){absent=vr.GetVector2Input(999);CHECK(absent.x==0 && absent.y==0);}
    const auto diagnostic=Logger::log.str().substr(diagnosticStart);
    CHECK(diagnostic.find("Analog action 999 result=3")!=std::string::npos);
    CHECK(diagnostic.find('\n')==diagnostic.size()-1); // One error message, not one per frame.
    auto& recovered=input.analog[{999,0}];recovered.bActive=true;recovered.x=.4f;
    CHECK(vr.GetVector2Input(999).x==.4f);CHECK(Logger::log.str().find("Analog action 999 result=0")!=std::string::npos);

    // Reproduce R1.31: new handles can register successfully but fail when read
    // against an older active manifest/binding. Both old presets and action sets
    // must still select the requested physical controller, never firing hand.
    for(bool leftPreset:{false,true})for(unsigned activeSet:{0u,1u})for(unsigned mode=0;mode<4;++mode){
        VRInputDouble old;OpenVR routed;routed.vrInput=&old;
        const char* legacy[]={"/actions/default/in/Move","/actions/left handed/in/Move",
                              "/actions/default/in/Look","/actions/left handed/in/Look"};
        for(unsigned n=0;n<4;++n)old.actions[legacy[n]]=101+n;
        auto setAxis=[&](InputBindingID action,vr::VRInputValueHandle_t hand,float x,float y){
            auto& a=old.analog[{action,hand}];a.bActive=true;a.x=x;a.y=y;
        };
        const auto leftAlias=101+activeSet+(leftPreset?2:0);
        const auto rightAlias=101+activeSet+(leftPreset?0:2);
        setAxis(leftAlias,11,.65f,-.45f);setAxis(rightAlias,12,-.35f,.8f);
        for(unsigned hand=0;hand<2;++hand){
            if(mode!=0)old.actions[hand?"/actions/default/in/PhysicalLook":"/actions/default/in/PhysicalMove"]=601+hand;
            if(mode==1)old.analogFailures[{601+hand,11+hand}]=vr::VRInputError_InvalidHandle;
            if(mode==2)old.analog[{601+hand,11+hand}].bActive=false;
            if(mode==3)setAxis(601+hand,11+hand,hand?-.2f:.3f,hand?.6f:-.7f);
        }
        const auto move=routed.RegisterVector2Input("default","PhysicalMove");
        const auto look=routed.RegisterVector2Input("default","PhysicalLook");
        CHECK(move && look && move!=look);CHECK(move!=leftAlias && look!=rightAlias);
        routed.UpdateInputs();
        const auto l=routed.GetVector2Input(move),r=routed.GetVector2Input(look);
        CHECK(l.x==(mode==3?.3f:.65f) && l.y==(mode==3?-.7f:-.45f));
        CHECK(r.x==(mode==3?-.2f:-.35f) && r.y==(mode==3?.6f:.8f));
        const auto logSize=Logger::log.str().size();
        for(int frame=0;frame<120;++frame){
            routed.UpdateInputs();const auto left=routed.GetVector2Input(move),right=routed.GetVector2Input(look);
            CHECK(left.x==l.x && left.y==l.y && right.x==r.x && right.y==r.y);
        }
        CHECK(Logger::log.str().size()==logSize);
        // The regular on-foot action must retain its original unrestricted meaning.
        setAxis(101,0,leftPreset?-.9f:.9f,.25f);
        CHECK(routed.RegisterVector2Input("default","Move")==101);
        CHECK(routed.GetVector2Input(101).x==(leftPreset?-.9f:.9f));
        // Missing the left source cannot route to the right stick, even though
        // it has a valid Move/Look action and a nonzero vector.
        old.analog.erase({leftAlias,11});old.analog.erase({601,11});
        auto zero=routed.GetVector2Input(move);CHECK(zero.x==0 && zero.y==0);
        CHECK(routed.GetVector2Input(look).y==r.y);
        const auto noSourceLog=Logger::log.str().size();
        for(int frame=0;frame<100;++frame){zero=routed.GetVector2Input(move);CHECK(zero.x==0 && zero.y==0);}
        CHECK(Logger::log.str().size()==noSourceLog);
        old.valid=false;routed.UpdateInputs();const auto reads=old.analogReads;
        zero=routed.GetVector2Input(look);CHECK(zero.x==0 && zero.y==0);CHECK(old.analogReads==reads);
        old.valid=true;routed.UpdateInputs();CHECK(routed.GetVector2Input(look).y==r.y);
        old.sourceValid=false;routed.RegisterVector2Input("default","PhysicalLook");
        const auto sourceReads=old.analogReads;zero=routed.GetVector2Input(look);
        CHECK(zero.x==0 && zero.y==0);CHECK(old.analogReads==sourceReads);
        routed.vrInput=nullptr;zero=routed.GetVector2Input(move);CHECK(zero.x==0 && zero.y==0);
    }
    { // A centered active primary wins over a moving old alias. Do not sum routes.
        VRInputDouble fresh;OpenVR routed;routed.vrInput=&fresh;
        fresh.actions["/actions/default/in/PhysicalMove"]=701;fresh.actions["/actions/default/in/Move"]=702;
        const auto move=routed.RegisterVector2Input("default","PhysicalMove");routed.UpdateInputs();
        auto& primary=fresh.analog[{701,11}];primary.bActive=true;
        auto& alias=fresh.analog[{702,11}];alias.bActive=true;alias.x=.9f;alias.y=.2f;
        const auto centered=routed.GetVector2Input(move);CHECK(centered.x==0 && centered.y==0);
        primary.x=.25f;CHECK(routed.GetVector2Input(move).x==.25f);
        primary.x=std::numeric_limits<float>::infinity();CHECK(routed.GetVector2Input(move).x==.9f);
        alias.y=std::numeric_limits<float>::quiet_NaN();const auto invalid=routed.GetVector2Input(move);
        CHECK(invalid.x==0 && invalid.y==0);
    }
    std::cout<<checks<<" production OpenVR physical-binding checks passed (VRInput double)\n";
}
