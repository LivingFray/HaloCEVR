#include "HaloCEVR/Immersion/Recoil.h"
#include <iostream>
#include <cstdlib>
#include <limits>
using namespace Immersion;
unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
bool close(float a,float b){return std::abs(a-b)<1e-4f;}
int main(){
    FireState before{1,2,100,{12,0},0.2f,0.3f},after=before;
    CHECK(!FiredBetween(before,after)); // Dry trigger / reload / no shot.
    ++after.tick;CHECK(FiredBetween(before,after)); // Includes no-ammo energy weapons.
    after=before;--after.loaded[0];CHECK(FiredBetween(before,after)); // Same tick.
    after=before;--after.loaded[1];CHECK(FiredBetween(before,after));
    after=before;after.heat+=0.1f;CHECK(FiredBetween(before,after));
    after=before;after.energyUsed+=0.1f;CHECK(FiredBetween(before,after));
    after=before;after.loaded[0]+=10;after.heat-=0.1f;CHECK(!FiredBetween(before,after));
    after.tick++;after.weapon++;CHECK(!FiredBetween(before,after));
    after.weapon=before.weapon;after.unit++;CHECK(!FiredBetween(before,after));
    Recoil one,two;one.Context(1,2);two.Context(1,2);
    const RecoilProfile ar{1.5f,0.014f,0.24f,14};
    for(unsigned i=0;i<600;++i){
        const double time=i/15.0;
        one.Shot(ar,false,1,0.35f,time);two.Shot(ar,true,1,0.35f,time);
        CHECK(close(two.Pose(2).pitch,one.Pose(2).pitch*0.35f));
        CHECK(close(two.Pose(2).back,one.Pose(2).back*0.35f));
        CHECK(one.Pose(2).pitch<=14 && one.Pose(2).back<=0.1f);
    }
    CHECK(one.Pose(2).pitch>5);CHECK(two.Pose(2).pitch<2.5f);
    const auto p=one.Pose(2);for(int eye=0;eye<100;++eye) CHECK(one.Pose(2).pitch==p.pitch);
    one.Advance(42);CHECK(one.Pose(2).pitch<0.002f);CHECK(one.Pose(22).pitch==0);
    for(int hz:{30,72,90,120,144}){
        Recoil r;r.Context(1,2);r.Shot(ar,false,1,.35f,0);
        for(int i=1;i<=hz;++i)r.Advance(double(i)/hz);
        CHECK(close(r.Pose(2).pitch,1.5f*std::exp(-1.0f/.24f)));
    }
    one.Context(1,3);CHECK(one.Pose(3).pitch==0);one.Shot(ar,false,1,.35f,50);
    one.Context(2,3);CHECK(one.Pose(3).pitch==0); // New player / respawn.
    one.Shot(ar,false,1,.35f,51);one.Advance(50);CHECK(one.Pose(3).pitch==0);
    one.Shot(ar,false,1,.35f,52);one.Advance(std::numeric_limits<double>::quiet_NaN());CHECK(one.Pose(3).pitch==0);
    one.Context(1,2);one.Shot(ar,true,1,0,0);CHECK(one.Pose(2).pitch==0);
    one.Shot(ar,false,-100,.35f,0);CHECK(one.Pose(2).pitch==0);
    one.Shot(ar,true,1,100,0);CHECK(close(one.Pose(2).pitch,1.5f));
    one.Reset();one.Context(1,2);one.Shot(ar,false,std::numeric_limits<float>::quiet_NaN(),.35f,0);
    CHECK(close(one.Pose(2).pitch,1.5f));
    std::cout<<checks<<" recoil timing, accumulation and shot-observation checks passed\n";
}
