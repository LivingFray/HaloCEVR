#pragma once
#include "../Helpers/Maths.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Immersion {
// Retail gbxmodel node layout, including the engine's inverse bind matrix.
struct ModelNode {
    char name[32];
    std::int16_t sibling, child, parent, padding;
    Vector3 translation;
    Vector4 rotation;
    float distance;
    unsigned char reserved[32];
    Transform inverseBind;
};
static_assert(sizeof(ModelNode) == 0x9c, "Retail model node layout");
static_assert(sizeof(Transform) == 0x34, "Retail model matrix layout");

inline Matrix4 ModelMatrix(const Transform& t) {
    Matrix4 m;
    for (int c=0;c<3;++c) for (int r=0;r<3;++r) m[c*4+r]=t.rotation[c*3+r]*t.scale;
    m[12]=t.translation.x; m[13]=t.translation.y; m[14]=t.translation.z;
    return m;
}
inline Transform ModelTransform(const Matrix4& m) {
    Transform t{}; t.scale=1;
    for (int c=0;c<3;++c) for (int r=0;r<3;++r) t.rotation[c*3+r]=m[c*4+r];
    t.translation=m*Vector3(); return t;
}
inline bool ValidModelNodes(const ModelNode* nodes, unsigned count) {
    if (!nodes || count == 0 || count > 64) return false;
    for (unsigned i=0;i<count;++i) {
        const auto& node=nodes[i];
        if (node.parent < -1 || node.parent >= static_cast<int>(count)) return false;
        int parent=static_cast<int>(i);
        for (unsigned depth=0;parent!=-1;++depth) {
            if (depth>=count || parent<0 || parent>=static_cast<int>(count)) return false;
            parent=nodes[parent].parent;
        }
        const auto m=ModelMatrix(node.inverseBind);
        for (int v=0;v<16;++v) if (!std::isfinite(m[v])) return false;
        if (std::fabs(m.getDeterminant())<0.00001f) return false;
    }
    return true;
}
inline bool PlaceModel(const ModelNode* nodes, unsigned count, const Matrix4& pose, Transform* out) {
    if (!out || !ValidModelNodes(nodes,count)) return false;
    for (unsigned i=0;i<count;++i) out[i]=ModelTransform(pose*ModelMatrix(nodes[i].inverseBind).invertAffine());
    return true;
}
inline bool PlaceChiefHands(const ModelNode* nodes, unsigned count, const Matrix4* poses, Transform* out) {
    if (!poses || !out || !ValidModelNodes(nodes,count)) return false;
    int wrist[2]={-1,-1};
    for (unsigned i=0;i<count;++i) {
        char name[33]{}; std::memcpy(name,nodes[i].name,32);
        if (std::strstr(name,"l wrist")) wrist[0]=static_cast<int>(i);
        if (std::strstr(name,"r wrist")) wrist[1]=static_cast<int>(i);
    }
    if (wrist[0]<0 || wrist[1]<0) return false;
    const Matrix4 delta[2]={poses[0]*ModelMatrix(nodes[wrist[0]].inverseBind),
                           poses[1]*ModelMatrix(nodes[wrist[1]].inverseBind)};
    for (unsigned i=0;i<count;++i) {
        int side=-1, ancestor=static_cast<int>(i);
        while (ancestor!=-1) {
            if (ancestor==wrist[0]) { side=0; break; }
            if (ancestor==wrist[1]) { side=1; break; }
            ancestor=nodes[ancestor].parent;
        }
        if (side>=0) out[i]=ModelTransform(delta[side]*ModelMatrix(nodes[i].inverseBind).invertAffine());
        else {
            // Collapse the arm sleeve onto its wrist, as the equipped model does.
            char name[33]{}; std::memcpy(name,nodes[i].name,32);
            out[i]=ModelTransform(poses[std::strstr(name,"r ") ? 1 : 0]); out[i].scale=0;
        }
    }
    return true;
}
// Animated legs rebased onto a steady, invisible pelvis. Collapse every
// non-leg bone at the waist in the render copy; no torso/head/arm mesh remains.

inline bool PlaceOnFootBody(const ModelNode* nodes,unsigned count,const Transform* live,Transform* out) {
    if(!live || !out || !ValidModelNodes(nodes,count))return false;
    int root=-1,pelvis=-1,legs[2]={-1,-1};
    for(unsigned i=0;i<count;++i){
        char name[33]{};std::memcpy(name,nodes[i].name,32);
        if(nodes[i].parent<0)root=i;
        if(std::strstr(name,"pelvis"))pelvis=i;
        for(unsigned h=0;h<2;++h){
            const bool side=std::strstr(name,h?"r ":"l ")!=nullptr;
            if(side && std::strstr(name,"thigh"))legs[h]=i;
        }
        const auto m=ModelMatrix(live[i]);
        for(unsigned n=0;n<16;++n)if(!std::isfinite(m[n]))return false;
        if(std::fabs(m.getDeterminant())<.00001f)return false;
    }
    if(root<0 || pelvis<0 || legs[0]<0 || legs[1]<0)return false;
    const Matrix4 rootFrame=ModelMatrix(live[root])*ModelMatrix(nodes[root].inverseBind);
    const auto forward=rootFrame.getLeftAxis();
    if(forward.x*forward.x+forward.y*forward.y<.01f)return false;
    Matrix4 steady;steady.rotateZ(std::atan2(forward.y,forward.x)*57.2957795f);
    steady.translate(rootFrame*Vector3());
    if(!PlaceModel(nodes,count,steady,out))return false;
    const auto legDelta=ModelMatrix(out[pelvis])*ModelMatrix(live[pelvis]).invertAffine();
    auto descendant=[&](unsigned child,int ancestor){
        int n=static_cast<int>(child);while(n>=0 && n!=ancestor)n=nodes[n].parent;return n==ancestor;
    };
    const auto waist=out[pelvis].translation;
    for(unsigned i=0;i<count;++i){
        if(descendant(i,legs[0]) || descendant(i,legs[1]))out[i]=ModelTransform(legDelta*ModelMatrix(live[i]));
        else {
            out[i]=ModelTransform(Matrix4().translate(waist));out[i].scale=0;
            for(float& axis:out[i].rotation)axis=0;
        }
    }
    return true;
}

}
