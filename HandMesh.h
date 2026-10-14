#pragma once
#include "InteractionModel.h"

namespace Immersion {
// Original, asset-independent glove geometry. Coordinates/joints are metres;
// callers transform the fallback from controller space, or supply tracked joints.
struct HandVertex { Point position; std::uint32_t color; };
inline Point Cross(Point a, Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Point Unit(Point a, Point fallback={1,0,0}) {
    const float n = a.LengthSquared();
    return a.Finite() && std::isfinite(n) && n > 0.000001f ? a*(1.0f/std::sqrt(n)) : fallback;
}
using HandJoints = std::array<Point,26>;
inline HandJoints FallbackHand(bool left, bool grip) {
    HandJoints j{};
    const float side = left ? -1.0f : 1.0f;
    j[1] = {-0.045f,0,0};
    const float lengths[4] = {0.068f,0.078f,0.073f,0.056f};
    for (unsigned f=0; f<4; ++f) {
        const unsigned start = 6+5*f;
        const float y = (0.027f-f*0.018f)*side;
        j[start] = {-0.027f,y*0.55f,0};
        j[start+1] = {0.037f,y,0};
        for (unsigned b=2; b<5; ++b) {
            const float t = (b-1)/3.0f;
            const float angle = grip ? t*2.2f : t*0.40f;
            j[start+b] = j[start+b-1]+Point{std::cos(angle),0,-std::sin(angle)}*(lengths[f]/3.0f);
        }
    }
    j[2] = {-0.024f,0.022f*side,-0.007f};
    j[3] = {-0.002f,0.044f*side,-0.012f};
    j[4] = {0.026f,0.050f*side,-0.023f};
    j[5] = {0.047f,(grip ? 0.026f : 0.052f)*side,-0.034f};
    return j;
}
inline bool ValidHand(const HandJoints& joints) {
    for (const auto p : joints) if (!p.Finite() || (p-joints[1]).LengthSquared() > 0.16f) return false;
    return (joints[12]-joints[1]).LengthSquared() > 0.0004f &&
        (joints[7]-joints[22]).LengthSquared() > 0.0001f;
}
inline void BuildHandMesh(const HandJoints& j, bool left, std::vector<HandVertex>& out) {
    if (!ValidHand(j)) return;
    const Point forward = Unit(j[12]-j[1]);
    const Point across = Unit(j[7]-j[22],{0,1,0});
    const Point normal = Unit(Cross(forward,across)*(left ? -1.0f : 1.0f),{0,0,1});
    const Point side = Unit(Cross(normal,forward),{0,1,0});
    auto ellipsoid = [&](Point centre, Point x, Point y, Point z, Point size, std::uint32_t color) {
        // Smooth vertex lighting baked into diffuse colors, independent of the
        // weapon shader/material left active by Halo's render pass.
        auto vertex = [&](unsigned lat, unsigned lon) {
            const float a = 3.141592654f*lat/4, b = 6.283185307f*lon/8;
            const Point n = x*(std::sin(a)*std::cos(b))+y*(std::sin(a)*std::sin(b))+z*std::cos(a);
            const Point p = centre+x*(size.x*std::sin(a)*std::cos(b))+
                y*(size.y*std::sin(a)*std::sin(b))+z*(size.z*std::cos(a));
            const float shade = 0.72f+0.28f*(std::max)(0.0f,n.Dot(Unit({-0.3f,0.4f,0.85f})));
            std::uint32_t shaded = 0xff000000u;
            for (unsigned shift=0; shift<=16; shift+=8)
                shaded |= static_cast<std::uint32_t>(((color>>shift)&255)*shade)<<shift;
            return HandVertex{p,shaded};
        };
        for (unsigned a=0; a<4; ++a) for (unsigned b=0; b<8; ++b) {
            const auto p=vertex(a,b), q=vertex(a+1,b), r=vertex(a+1,b+1), s=vertex(a,b+1);
            if (a != 3) { out.push_back(p); out.push_back(q); out.push_back(r); }
            if (a != 0) { out.push_back(p); out.push_back(r); out.push_back(s); }
        }
    };
    const Point knuckles=(j[7]+j[12]+j[17]+j[22])*0.25f;
    const Point palm=(j[1]+knuckles)*0.5f;
    const float length=(std::max)(0.042f,std::sqrt((knuckles-j[1]).LengthSquared())*0.5f+0.011f);
    // A continuous, padded glove body reaching the knuckles, rather than
    // thin segments separated by gaps. Original geometry, not game assets.
    ellipsoid(palm,forward,side,normal,{length,0.041f,0.023f},0xff505a53u);
    ellipsoid(palm+normal*0.019f,forward,side,normal,{length*0.78f,0.034f,0.009f},0xff89985cu);
    ellipsoid(j[1]-forward*0.007f,forward,side,normal,{0.025f,0.034f,0.024f},0xff6c7951u);
    auto finger = [&](unsigned start, unsigned end, float radius) {
        for (unsigned b=start; b<end; ++b) {
            const Point axis = Unit(j[b+1]-j[b]);
            const float length = std::sqrt((j[b+1]-j[b]).LengthSquared());
            const Point width = Unit(Cross(normal,axis),side), depth = Unit(Cross(axis,width),normal);
            ellipsoid((j[b]+j[b+1])*0.5f,axis,width,depth,{length*0.5f+0.006f,radius,radius},0xff566058u);
            ellipsoid(j[b],axis,width,depth,{radius,radius,radius},0xff566058u);
            if (b == start) ellipsoid(j[b]+normal*radius,axis,width,depth,{0.011f,radius,0.003f},0xff718050u);
        }
    };
    finger(2,5,0.013f);
    for (unsigned f=0; f<4; ++f) finger(7+f*5,10+f*5,f == 3 ? 0.010f : 0.012f);
}
}
