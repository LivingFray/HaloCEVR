#pragma once
#include "../Maths/Vectors.h"
#include <cstdint>

namespace Immersion {
// Opaque, closed geometry in the same coordinates as its interaction anchor.
// The rim and side wall make crossing the surface legible in stereo.
template<class Renderer,class Color>
void DrawSolidZone(Renderer& renderer,Vector3 center,Vector3 right,Vector3 up,
                   float radius,float thickness,Color face,Color rim,Color side){
    if(radius<=0 || thickness<=0)return;
    const auto normal=right.cross(up).normalize();
    const auto front=center+normal*(thickness*.5f),back=center-normal*(thickness*.5f);
    auto triangle=[&](Vector3 a,Vector3 b,Vector3 c,Color color){
        const Vector3 p[3]={a,b,c};const Color colors[3]={color,color,color};
        renderer.DrawMeshTriangle(p,colors);
    };
    constexpr unsigned segments=32;
    for(unsigned i=0;i<segments;++i){
        const float a=6.2831853f*i/segments,b=6.2831853f*(i+1)/segments;
        const auto u=(right*std::cos(a)+up*std::sin(a))*radius;
        const auto v=(right*std::cos(b)+up*std::sin(b))*radius;
        const auto innerU=u*.85f,innerV=v*.85f;
        triangle(front,front+innerU,front+innerV,face);
        triangle(front+innerU,front+u,front+v,rim);
        triangle(front+innerU,front+v,front+innerV,rim);
        triangle(back,back+v,back+u,side);
        triangle(front+u,back+u,back+v,side);
        triangle(front+u,back+v,front+v,side);
    }
}
}
