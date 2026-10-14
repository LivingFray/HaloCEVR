#include <iostream>
#include <cstdlib>
#define VR_PROFILE_SCOPE(x)
enum class ERenderState {SCOPE,LEFT_EYE};
namespace Helpers {
    struct Target {int renderSurface=10;} targets[2];
    struct Device {int target=10;void SetRenderTarget(int,int t){target=t;}} device;
    Target* GetRenderTargets(){return targets;}
    Device* GetDirect3DDevice9(){return &device;}
}
class Game {
public:
    struct Interactions{bool driving=false;bool HideDrivingReticle()const{return driving;}} interactions;
    bool empty=false,support=false;ERenderState state=ERenderState::LEFT_EYE;
    int realZoom=-1,crosshairSurface=20,crosshairRealSurface=0;
    bool PhysicalWeaponsStowed()const{return empty;}
    bool PhysicalSupportOnly()const{return support;}
    ERenderState GetRenderState()const{return state;}
    bool PreDrawCrosshair(short*);void PostDrawCrosshair();
};
// INSERT_PRODUCTION_HUD
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    Game g;
    for(bool driver:{false,true})for(bool empty:{false,true})for(bool support:{false,true}) {
        g.interactions.driving=driver;
        g.empty=empty;g.support=support;
        for(short anchor=0;anchor<5;++anchor) {
            const bool draw=g.PreDrawCrosshair(&anchor);
            CHECK(draw == (anchor!=4 || (!empty&&!support&&!driver)));
            if(draw)g.PostDrawCrosshair();
            CHECK(Helpers::GetRenderTargets()[1].renderSurface==10);
        }
        CHECK(g.PreDrawCrosshair(nullptr));g.PostDrawCrosshair();
    }
    std::cout<<checks<<" production HUD anchor/health preservation checks passed\n";
}
