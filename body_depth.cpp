#include "HaloCEVR/Immersion/RenderProjection.h"
#include "HaloCEVR/Immersion/OutlineZone.h"
#include <vector>
#include <iostream>
#include <cstdlib>
struct Projection{float _11=1.4f,_22=1.6f,_13=.2f,_23=.3f,_33=-1.1f,_43=-.2f;};
struct Renderer{std::vector<Vector3> vertices;void DrawMeshTriangle(const Vector3 (&p)[3],const unsigned (&)[3]){for(auto v:p)vertices.push_back(v);}};
unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(){
    const Projection original{},p=Immersion::FirstPersonDepth(original);
    CHECK(p._11==original._11 && p._22==original._22 && p._13==0 && p._23==0);
    auto depth=[&](float distance){return (-distance*p._33+p._43)/distance;};
    // Native first-person projection's independently calculated clip depths:
    // equal-position hand and marker must tie; nearer passes, farther fails.
    constexpr double near=.01171875,far=1024;
    for(float distance:{.04f,.08f,.12f,.2f,.4f,.8f}){
        const double nativeDepth=((far+near)-2*far*near/distance)/(far-near);
        CHECK(std::fabs(depth(distance)-nativeDepth)<.000001);
        CHECK(depth(distance-.005f)<nativeDepth && depth(distance+.005f)>nativeDepth);
        CHECK(std::fabs((-distance*original._33+original._43)/distance-nativeDepth)>.01);
    }
    Renderer renderer;const Vector3 center(1,2,3),right(0,-1,0),up(1,0,0);
    Immersion::DrawOutlineZone(renderer,center,right,up,.09f,.003f,unsigned(1));
    CHECK(renderer.vertices.size()==96*3);
    for(auto v:renderer.vertices){const float radius=(v-center).length();
        CHECK(std::fabs(v.z-center.z)<.000001f);CHECK(radius>=.08699f && radius<=.09001f);
    }
    std::cout<<checks<<" body-outline geometry and native first-person depth checks passed\n";
}
