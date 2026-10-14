#include "../HaloCEVR/Immersion/LookHistory.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Immersion;
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
Point direction(float yaw,float pitch){
    yaw*=3.14159265f/180;pitch*=3.14159265f/180;
    return {std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
}
int main(){
    LookHistory h;
    auto tick=[&](float yaw,float pitch){return h.Update(direction(yaw,pitch),true,1.0f/90);};
    CHECK(tick(0,0)==0);CHECK(tick(0,5)==0x80);CHECK(tick(0,0)==0x100);
    CHECK(tick(5,0)==0x200);CHECK(tick(0,0)==0x400);
    h.Reset();CHECK(tick(0,0)==0);
    for(int i=0;i<100;++i)CHECK(tick((i%2)?0.2f:-0.2f,0)==0); // Alternating jitter cannot complete the test.
    h.Reset();CHECK(tick(0,0)==0);CHECK(tick(0,1)==0);CHECK(tick(0,2)==0);CHECK(tick(0,4)==0x80);
    h.Reset();CHECK(tick(179,0)==0);CHECK(tick(-179,0)==0);CHECK(tick(-177,0)==0x200);
    CHECK(h.Update(direction(0,0),false,0.01f)==0);CHECK(tick(0,0)==0);
    CHECK(h.Update(direction(0,5),true,1.0f)==0);CHECK(tick(0,5)==0);
    CHECK(tick(0,80)==0);CHECK(tick(0,81)==0); // Recenter/pose discontinuity is not a deliberate look.
    CHECK(h.Update({std::numeric_limits<float>::quiet_NaN(),0,0},true,0.01f)==0);
    CHECK(h.Update({},true,0.01f)==0);CHECK(tick(0,0)==0);
    h.Reset();
    for(int n=0;n<100;++n)CHECK(h.UpdateStick(.2f,-.2f,true,.01f)==0);
    CHECK(h.UpdateStick(1,0,true,.04f)==0);CHECK(h.UpdateStick(1,0,true,.04f)==0x400);
    CHECK(h.UpdateStick(-1,0,true,.08f)==0x200);
    CHECK(h.UpdateStick(0,1,true,.08f)==0x80);CHECK(h.UpdateStick(0,-1,true,.08f)==0x100);
    CHECK(h.UpdateStick(1,1,false,.1f)==0);CHECK(h.UpdateStick(1,1,true,1.f)==0);
    CHECK(h.UpdateStick(1,0,true,.04f)==0);CHECK(h.UpdateStick(-1,0,true,.04f)==0);
    h.Reset();CHECK(h.UpdateStick(-1,0,true,.04f)==0);
    std::cout<<checks<<" headset look-history checks passed\n";
}
