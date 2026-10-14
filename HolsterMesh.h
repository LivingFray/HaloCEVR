#pragma once
#include "HandMesh.h"

namespace Immersion {
// Universal open weapon cradle. Origin is the SAME grip hotspot used by input;
// x forward, y left, z up, in metres. No bundled Halo/other-mod art assets.
inline void BuildHolsterMesh(bool occupied, std::vector<HandVertex>& out) {
    auto box=[&](Point centre,Point half,std::uint32_t color) {
        const Point corners[8]={
            {-half.x,-half.y,-half.z},{half.x,-half.y,-half.z},
            {half.x,half.y,-half.z},{-half.x,half.y,-half.z},
            {-half.x,-half.y,half.z},{half.x,-half.y,half.z},
            {half.x,half.y,half.z},{-half.x,half.y,half.z}};
        const unsigned faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
        const float shade[6]={0.58f,1.0f,0.76f,0.85f,0.68f,0.93f};
        for (unsigned f=0;f<6;++f) {
            std::uint32_t c=0xff000000u;
            for (unsigned shift=0;shift<=16;shift+=8)
                c|=static_cast<std::uint32_t>(((color>>shift)&255)*shade[f])<<shift;
            for (unsigned v : {0u,1u,2u,0u,2u,3u}) out.push_back({centre+corners[faces[f][v]],c});
        }
    };
    // Rounded look comes from inset layers; the open mouth stays at z=0.
    box({-0.045f,0,-0.045f},{0.012f,0.066f,0.092f},0xff65724au); // back plate
    box({0.015f,-0.057f,-0.072f},{0.060f,0.010f,0.065f},0xff4c5742u);
    box({0.015f, 0.057f,-0.072f},{0.060f,0.010f,0.065f},0xff4c5742u);
    box({0.067f,0,-0.083f},{0.009f,0.057f,0.054f},0xff596747u);
    box({0.013f,0,-0.137f},{0.063f,0.067f,0.010f},0xff39413bu); // floor
    // Reinforced open rim, two straps and buckle; no flat blue target ring.
    for(float y : {-0.057f,0.057f})
        box({0.011f,y,-0.007f},{0.065f,0.013f,0.009f},0xff899783u);
    box({0.070f,0,-0.023f},{0.011f,0.062f,0.008f},0xff899783u);
    for(float y : {-0.033f,0.033f})
        box({0.078f,y,-0.091f},{0.004f,0.009f,0.043f},0xff272e2du);
    box({0.083f,0,-0.085f},{0.006f,0.022f,0.016f},0xffa3a899u);
    box({0.090f,0,-0.085f},{0.002f,0.014f,0.006f},occupied ? 0xff61becbu : 0xff464d49u);
}
}
