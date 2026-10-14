#include "HaloCEVR/Immersion/InteractionModel.h"
#include <cstring>
// Windows RPC headers define small as char. Exercise the production header
// with that macro present even in the portable Linux test environment.
#define small char
#include "HaloCEVR/Immersion/HolsterMarker.h"
#undef small
#include <iostream>
#include <cstdlib>
#include <map>
using DWORD=unsigned;
constexpr unsigned D3DRS_ALPHABLENDENABLE=1,D3DRS_ALPHATESTENABLE=2,D3DRS_FOGENABLE=3;
constexpr unsigned FALSE=0;
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#include "HaloCEVR/Immersion/GreyMarkerDraw.h"
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
struct Shader { unsigned refs=1;void Release(){--refs;} };
struct Device {
    std::map<unsigned,DWORD> state{{1,1},{2,1},{3,1}};
    Shader* current=nullptr;bool failRead=false,failSet=false;unsigned reads=0,writes=0;
    int GetPixelShader(Shader** shader){++reads;*shader=current;if(current)++current->refs;return 0;}
    int SetPixelShader(Shader* shader){if(failSet)return -1;++writes;current=shader;return 0;}
    int GetRenderState(unsigned name,DWORD* value){++reads;if(failRead)return -1;*value=state[name];return 0;}
    int SetRenderState(unsigned name,DWORD value){++writes;state[name]=value;return 0;}
};
int main(){
    using namespace Immersion;
    CHECK(std::strcmp(HolsterLabel(0,false),"Right Shoulder")==0);
    CHECK(std::strcmp(HolsterLabel(1,false),"Right Hip (Here)")==0);
    CHECK(std::strcmp(HolsterLabel(0,true),"Left Shoulder")==0);
    CHECK(std::strcmp(HolsterLabel(1,true),"Left Hip (Here)")==0);
    for(const char* path:{"weapons\\assault rifle\\assault rifle","weapons\\rocket launcher\\rocket launcher",
        "weapons\\plasma rifle\\plasma rifle","weapons\\sniper rifle\\sniper rifle","weapons\\shotgun\\shotgun"}) {
        CHECK(std::fabs(HolsterMarkerScale(path,1)-0.30f)<1e-6f);
        CHECK(HolsterMarkerScale(path,0)<HolsterMarkerScale(path,1));
    }
    for(const char* path:{"weapons\\plasma pistol\\plasma pistol","weapons\\pistol\\pistol","weapons\\needler\\needler"})
        CHECK(std::fabs(HolsterMarkerScale(path,1)-0.65f)<1e-6f);
    CHECK(HolsterMarkerScale(nullptr,1)==0.30f);CHECK(HolsterMarkerScale("custom\\weapon",1)==0.30f);
    for(float yaw:{0.0f,1.5f,3.0f}) {
        const Point head{1,2,3};const auto a=HolsterMarkerAnchor(0,Layout{false},head,yaw);
        const auto b=HolsterMarkerAnchor(0,Layout{true},head,yaw);
        CHECK((a-b).LengthSquared()<1e-7f); // Center marker isn't a moved grab anchor.
        CHECK((a-Layout{false}.Anchor(Zone::Shoulder,head,yaw)).LengthSquared()>0.1f);
        CHECK((HolsterMarkerAnchor(1,Layout{true},head,yaw)-Layout{true}.Anchor(Zone::WeaponHip,head,yaw)).LengthSquared()<1e-7f);
    }
    Device device;Shader normal,grey;device.current=&normal;const auto state=device.state;
    { GreyMarkerDraw<Device,Shader> scope(&device,&grey);
        CHECK(device.current==&grey);CHECK(device.state[1]==0 && device.state[2]==0 && device.state[3]==0);
        CHECK(normal.refs==2);
    }
    CHECK(device.current==&normal);CHECK(device.state==state);CHECK(normal.refs==1);
    const auto reads=device.reads,writes=device.writes;
    { GreyMarkerDraw<Device,Shader> ordinary(&device,nullptr); }
    CHECK(device.reads==reads && device.writes==writes); // Held gun/HUD/hands untouched.
    device.failRead=true;
    { GreyMarkerDraw<Device,Shader> scope(&device,&grey); }
    CHECK(device.current==&normal && device.state==state && normal.refs==1);
    device.failRead=false;device.failSet=true;
    { GreyMarkerDraw<Device,Shader> scope(&device,&grey); }
    CHECK(device.current==&normal && device.state==state && normal.refs==1);
    device.failSet=false;device.current=nullptr;
    { GreyMarkerDraw<Device,Shader> scope(&device,&grey);CHECK(device.current==&grey); }
    CHECK(device.current==nullptr && device.state==state); // Fixed-function state restored too.
    std::cout<<checks<<" marker size/placement and grey-render restoration checks passed\n";
}
