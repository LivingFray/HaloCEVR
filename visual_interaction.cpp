#include "../HaloCEVR/Immersion/HandMesh.h"
#include "../HaloCEVR/Immersion/HolsterMesh.h"
#include "../HaloCEVR/Immersion/MenuPointer.h"
#include "../HaloCEVR/Immersion/RetailProfile.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
using namespace Immersion;
unsigned checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr<<__LINE__<<": " #x "\n"; std::exit(1); } } while(false)
int main(int argc,char** argv) {
    for (bool left : {false,true}) {
        Layout layout{left};
        const std::array<Point,2> zones{layout.Offset(Zone::Shoulder),layout.Offset(Zone::WeaponHip)};
        const float radius[2]{0.20f,0.16f};
        CHECK(std::abs(zones[0].x+0.15f)<1e-5f);
        CHECK(std::abs(zones[1].z+0.43f)<1e-5f);
        CHECK(std::abs(zones[0].z+0.06f)<1e-5f);
        CHECK(!Within(zones[0],{-0.04f,left?0.12f:-0.12f,-0.03f},0.14f));
        CHECK(std::abs(zones[1].x-0.22f)<1e-5f);
        for (unsigned h=0; h<2; ++h) for (unsigned slot=0; slot<2; ++slot) {
            HolsterHover hover;
            CHECK(hover.Update(h,zones[slot],zones,radius)==int(slot));
            for (int i=0; i<50; ++i) CHECK(hover.Update(h,zones[slot],zones,radius)==-1);
            CHECK(hover.Update(h,zones[slot]+Point{radius[slot]+0.02f,0,0},zones,radius)==-1);
            CHECK(hover.Update(h,zones[slot],zones,radius)==-1);
            CHECK(hover.Update(h,zones[slot]+Point{radius[slot]+0.04f,0,0},zones,radius)==-1);
            CHECK(hover.Update(h,zones[slot],zones,radius)==int(slot));
            hover.Reset(); CHECK(hover.Update(h,zones[slot],zones,radius)==int(slot));
        }
    }
    for (unsigned hand=0; hand<2; ++hand) {
        MenuPointer pointer; std::array<MenuRay,2> rays{};
        rays[hand]={true,true,true,0.25f,0.75f};
        CHECK(!pointer.Update(rays).down); // Menu opens with trigger held.
        rays[hand].down=false; auto p=pointer.Update(rays);
        CHECK(p.positioned && p.x==0.25f && p.y==0.25f);
        rays[hand].down=true; CHECK(pointer.Update(rays).down);
        CHECK(pointer.Update(rays).down);
        rays[hand].hit=false; CHECK(!pointer.Update(rays).down);
        rays[hand].hit=true; CHECK(!pointer.Update(rays).down); // No reclick while held.
        rays[hand].down=false; pointer.Update(rays);
        rays[hand].down=true; CHECK(pointer.Update(rays).down);
        rays[hand].valid=false; CHECK(!pointer.Update(rays).down);
        rays[hand].valid=true; CHECK(!pointer.Update(rays).down);
        rays[hand].u=std::numeric_limits<float>::quiet_NaN(); CHECK(!pointer.Update(rays).positioned);
        pointer.Reset();rays[hand].u=0.5f;CHECK(!pointer.Update(rays).down);
    }
    for (bool grip : {false,true}) for (bool left : {false,true}) {
        auto joints=FallbackHand(left,grip); CHECK(ValidHand(joints));
        std::vector<HandVertex> mesh;BuildHandMesh(joints,left,mesh);
        CHECK(!mesh.empty() && mesh.size()%3==0 && mesh.size()<6000);
        for (const auto& v:mesh) {
            CHECK(v.position.Finite() && v.position.LengthSquared()<0.06f);
            CHECK((v.color>>24)==255);
        }
        if(argc>1 && !left && !grip) {
            std::ofstream file(argv[1]);
            for (const auto& v:mesh) file<<v.position.x<<' '<<v.position.y<<' '<<v.position.z<<' '<<v.color<<'\n';
        }
        joints[7].x=std::numeric_limits<float>::infinity();
        CHECK(!ValidHand(joints));mesh.clear();BuildHandMesh(joints,left,mesh);CHECK(mesh.empty());
    }
    for (bool occupied : {false,true}) {
        std::vector<HandVertex> mesh;BuildHolsterMesh(occupied,mesh);
        CHECK(!mesh.empty() && mesh.size()%3==0 && mesh.size()<1000);
        for (const auto& v : mesh) {
            CHECK(v.position.Finite() && v.position.LengthSquared()<0.05f);
            CHECK((v.color>>24)==255);
        }
        if(argc>2 && occupied) {
            std::ofstream file(argv[2]);
            for(const auto& v:mesh)file<<v.position.x<<' '<<v.position.y<<' '<<v.position.z<<' '<<v.color<<'\n';
        }
    }
    CHECK(!ValidHand(HandJoints{}));
    auto a=FallbackHand(false,false),b=FallbackHand(true,false);
    for (unsigned i=0;i<a.size();++i) CHECK(a[i].x==b[i].x && a[i].y==-b[i].y && a[i].z==b[i].z);
    CHECK(RetailVisibilityEntry.length==0x4d && RetailVisibilityEntry.rva==0x93e50);
    std::cout<<checks<<" holster-hover, menu-input and glove-mesh checks passed\n";
}
