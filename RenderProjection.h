#pragma once
namespace Immersion {
// Retail 1.0.10.0621 rasterizer_set_frustum_z (0x518f40) calls the
// projection override at 0x50c9a0. Native first-person model paths use these
// near/far values (0x69c664/0x69c668). Preserve the per-eye x/y projection.
// No live pointer, hook, world-depth clear or game asset is needed here.
template<class Matrix> Matrix FirstPersonDepth(Matrix projection){
    constexpr float nearPlane=.01171875f,farPlane=1024.f;
    projection._13=projection._23=0.f;
    projection._33=-(farPlane+nearPlane)/(farPlane-nearPlane);
    projection._43=-(2.f*farPlane*nearPlane)/(farPlane-nearPlane);
    return projection;
}
}
