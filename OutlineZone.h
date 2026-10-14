#pragma once
#include "../Maths/Vectors.h"
namespace Immersion {
template<class Renderer,class Color>
void DrawOutlineZone(Renderer& renderer,Vector3 center,Vector3 right,Vector3 up,
                     float radius,float width,Color color){
    if(radius<=0 || width<=0 || width>=radius)return;
    constexpr unsigned segments=48;
    const Color colors[3]={color,color,color};
    for(unsigned i=0;i<segments;++i){
        const float a=6.2831853f*i/segments,b=6.2831853f*(i+1)/segments;
        const auto u=right*std::cos(a)+up*std::sin(a),v=right*std::cos(b)+up*std::sin(b);
        const Vector3 first[3]={center+u*(radius-width),center+u*radius,center+v*radius};
        const Vector3 second[3]={center+u*(radius-width),center+v*radius,center+v*(radius-width)};
        renderer.DrawMeshTriangle(first,colors);renderer.DrawMeshTriangle(second,colors);
    }
}
}
