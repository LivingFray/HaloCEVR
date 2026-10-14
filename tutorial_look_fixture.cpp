#include "HaloCEVR/Maths/Matrices.h"
#include "HaloCEVR/Immersion/LookHistory.h"
#include <sstream>
#include <iostream>
#include <cstdlib>
struct VR{
    bool tracked=true;float yaw=0;Matrix4 head;Vector2 stick;
    bool IsHMDTracked()const{return tracked;}
    Matrix4 GetHMDTransform()const{return Matrix4(head).rotateZ(-yaw);}
    float GetYawOffset()const{return yaw;}
    Vector2 GetVector2Input(int)const{return stick;}
};using IVR=VR;
namespace Helpers{bool menu=false,loading=false;bool IsMouseVisible(){return menu;}bool IsCampaignLoading(){return loading;}}
namespace Logger{std::ostringstream log;}
struct Game{
    static Game instance;VR vr;bool bUse3DOFAiming=false,bNeedsRecentre=false;float lastDeltaTime=.01f;
    struct Interactions{bool active=false;bool VehicleActive()const{return active;}}interactions;
    VR* GetVR(){return &vr;}
};Game Game::instance;
struct Engine{
    Immersion::LookHistory history;unsigned flags=0,calls=0;bool valid=false;
    unsigned RecordLook(Immersion::Point head,bool active,float dt,float x=0,float y=0){
        ++calls;valid=active;if(!active){history.Reset();return 0;}
        const auto observed=history.Update(head,true,dt)|history.UpdateStick(x,y,true,dt);
        const auto added=observed&~flags;flags|=observed;return added;
    }
};
struct InputHandler{int Look=0;Engine physicalEngine;void UpdateTutorialLook();};
// INSERT_TUTORIAL_LOOK
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    InputHandler input;auto& g=Game::instance;input.UpdateTutorialLook();CHECK(input.physicalEngine.valid);
    g.vr.head.rotateY(-5);input.UpdateTutorialLook();CHECK(input.physicalEngine.flags==0x80);
    g.vr.head.rotateY(10);input.UpdateTutorialLook();CHECK(input.physicalEngine.flags==0x180);
    // VR snap yaw alone is not actual head travel; the stick path handles it.
    g.vr.yaw=90;input.UpdateTutorialLook();CHECK(input.physicalEngine.flags==0x180);
    g.vr.stick={1,0};for(int n=0;n<9;++n)input.UpdateTutorialLook();CHECK(input.physicalEngine.flags==0x580);
    g.vr.stick={-1,0};for(int n=0;n<9;++n)input.UpdateTutorialLook();CHECK(input.physicalEngine.flags==0x780);
    for(int reason=0;reason<6;++reason){
        g.vr.tracked=reason!=0;g.bNeedsRecentre=reason==1;g.bUse3DOFAiming=reason==2;
        Helpers::menu=reason==3;Helpers::loading=reason==4;g.interactions.active=reason==5;
        input.UpdateTutorialLook();CHECK(!input.physicalEngine.valid);
    }
    g.interactions.active=false;input.UpdateTutorialLook();CHECK(input.physicalEngine.valid);
    std::cout<<checks<<" production frame-based tutorial headset/stick and reset checks passed\n";
}
