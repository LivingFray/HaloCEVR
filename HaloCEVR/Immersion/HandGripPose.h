#pragma once
#include "NativeModelPose.h"
#include <cctype>


namespace Immersion {
inline std::array<char,33> HandBoneKey(const char* name) {
    std::array<char,33> key{};unsigned n=0;
    for(unsigned i=0;i<32 && name[i];++i) {
        const auto c=static_cast<unsigned char>(name[i]);
        if(std::isalnum(c))key[n++]=static_cast<char>(std::tolower(c));
    }
    return key; // Includes Halo's "middlelow" / "middle low" spelling difference.
}
template<class B> bool HandDescendant(const B* bones,unsigned count,int node,int parent) {
    for(unsigned i=0;i<count && node>=0 && node<static_cast<int>(count);++i) {
        if(node==parent)return true;
        node=bones[node].Parent;
    }
    return false;
}
inline Matrix4 GripAxes(Vector3 fingers,Vector3 across,Vector3 back) {
    Matrix4 m;
    for(unsigned i=0;i<3;++i){m[i]=fingers[i];m[4+i]=across[i];m[8+i]=back[i];}
    return m;
}
inline Matrix4 GripRotation(Matrix4 m) {
    for(unsigned c=0;c<3;++c) {
        Vector3 v(m[c*4],m[c*4+1],m[c*4+2]);
        const float n=v.length();if(n>.00001f)v*=1.f/n;
        for(unsigned r=0;r<3;++r)m[c*4+r]=v[r];
    }
    m.translate(-(m*Vector3()));return m;
}
template<class B> float NativeHandSign(const B* bones,int wrist) {
    const auto key=HandBoneKey(bones[wrist].BoneName);
    return std::strstr(key.data(),"rwrist")?-1.f:1.f;
}
// The palm frame is determined from actual knuckles, not the controller's
// guessed roll: X towards fingers, Y index-to-pinky, Z back of the hand.
// Right native anatomy has the opposite cross-product sign; a reflected left
// reload skeleton still keeps its original left-hand sign.
template<class B> bool LeftPalmFrame(const B* bones,unsigned count,const Transform* bind,int wrist,Matrix4& palm) {
    int index=-1,pinky=-1;
    for(unsigned i=0;i<count;++i)if(bones[i].Parent==wrist) {
        const auto key=HandBoneKey(bones[i].BoneName);
        if(std::strstr(key.data(),"index") || std::strstr(key.data(),"finger1"))index=i;
        if(std::strstr(key.data(),"pinky") || std::strstr(key.data(),"little") || std::strstr(key.data(),"finger4"))pinky=i;
    }
    if(index<0 || pinky<0)return false;
    const Vector3 knuckles=(bind[index].translation+bind[pinky].translation)*.5f;
    Vector3 fingers=knuckles-bind[wrist].translation,across=bind[pinky].translation-bind[index].translation;
    if(fingers.lengthSqr()<1e-9f || across.lengthSqr()<1e-9f)return false;
    fingers.normalize();across-=fingers*across.dot(fingers);
    if(across.lengthSqr()<1e-9f)return false;
    across.normalize();
    const float mirror=ModelMatrix(bind[wrist]).getDeterminant()<0?-1.f:1.f;
    palm=GripAxes(fingers,across,fingers.cross(across)*(mirror*NativeHandSign(bones,wrist)));
    palm.translate(bind[wrist].translation+(knuckles-bind[wrist].translation)*.72f);
    return true;
}
template<class B> bool HandBindPose(const B* bones,unsigned count,const ModelNode* rest,unsigned restCount,Transform* bind) {
    bool complete=true;
    std::array<std::array<char,33>,64> keys{};restCount=(std::min)(restCount,64u);
    for(unsigned n=0;n<restCount;++n)keys[n]=HandBoneKey(rest[n].name);
    for(unsigned i=0;i<count;++i) {
        bool found=false;const auto key=HandBoneKey(bones[i].BoneName);
        for(unsigned n=0;n<restCount;++n)if(key==keys[n]) {
            bind[i]=ModelTransform(ModelMatrix(rest[n].inverseBind).invertAffine());found=true;break;
        }
        complete&=found;
    }
    return complete;
}
template<class B> void MoveGripHand(const B* bones,unsigned count,Transform* out,int wrist,int gun,const Matrix4& target) {
    const Matrix4 old=ModelMatrix(out[wrist]);if(std::fabs(old.getDeterminant())<.00001f)return;
    const Matrix4 delta=target*Matrix4(old).invertAffine();
    for(unsigned i=0;i<count;++i)if(HandDescendant(bones,count,i,wrist) && !HandDescendant(bones,count,i,gun))
        out[i]=ModelTransform(delta*ModelMatrix(out[i]));
    const int p=bones[wrist].Parent;
    if(p>=0 && p<static_cast<int>(count) && out[p].scale==0)out[p].translation=out[wrist].translation;
}
// Rebuild each finger from its own parent and bind length. Copying the trigger
// hand left the index open and never made a fist. Rotations are absolute from
// this frame's baseline, so stereo rendering cannot accumulate extra curl.
template<class B> void CloseGripHand(const B* bones,unsigned count,Transform* out,int wrist,int gun,
    const ModelNode* rest,unsigned restCount,float amount=1.f,float thumbAmount=1.f) {
    if(count>64 || wrist<0 || wrist>=static_cast<int>(count))return;
    std::array<Transform,64> bind{};
    for(unsigned i=0;i<count;++i)bind[i]=out[i];
    HandBindPose(bones,count,rest,restCount,bind.data());
    Matrix4 palm;const bool anatomical=LeftPalmFrame(bones,count,bind.data(),wrist,palm);
    for(unsigned depth=1;depth<=4;++depth)for(unsigned i=0;i<count;++i) {
        if(static_cast<int>(i)==wrist || !HandDescendant(bones,count,i,wrist) || HandDescendant(bones,count,i,gun))continue;
        unsigned level=0;int p=i;
        while(p>=0 && p<static_cast<int>(count) && p!=wrist && level<count){p=bones[p].Parent;++level;}
        if(p!=wrist || level!=depth)continue;
        const auto key=HandBoneKey(bones[i].BoneName);
        const bool thumb=std::strstr(key.data(),"thumb") || std::strstr(key.data(),"finger0");
        if(!thumb && !std::strstr(key.data(),"finger") && !std::strstr(key.data(),"index") &&
            !std::strstr(key.data(),"middle") && !std::strstr(key.data(),"ring") &&
            !std::strstr(key.data(),"pinky") && !std::strstr(key.data(),"little"))continue;
        p=bones[i].Parent;if(p<0 || p>=static_cast<int>(count))continue;
        const Matrix4 joint=ModelMatrix(bind[i]),parent=ModelMatrix(bind[p]);
        if(std::fabs(joint.getDeterminant())<.00001f || std::fabs(parent.getDeterminant())<.00001f)continue;
        Matrix4 local=Matrix4(parent).invertAffine()*joint;
        Vector3 hinge(0,0,1);
        if(anatomical) {
            Matrix4 inverse=GripRotation(joint).invertAffine();
            hinge=(inverse*(thumb ? palm.getLeftAxis() : palm.getUpAxis()))*NativeHandSign(bones,wrist);
            if(thumb)hinge=-hinge;
        }
        const float angle=thumb?(depth==1?32.f:depth==2?55.f:30.f):(depth==1?55.f:depth==2?78.f:42.f);
        const Matrix4 target=ModelMatrix(out[p])*local*Matrix4().rotate(angle*amount*(thumb?thumbAmount:1.f),hinge);
        MoveGripHand(bones,count,out,i,gun,target);
    }
}
// Use the actual extended thumb's distal pad, not a guessed controller/finger
// gap. Both wielding hands use this skeleton with the existing reflected pose.
template<class B> bool ThumbPadFrame(const B* bones,unsigned count,const Transform* out,int wrist,int gun,
    float clearance,Matrix4& target){
    if(count>64 || wrist<0 || wrist>=static_cast<int>(count))return false;
    Matrix4 palm;if(!LeftPalmFrame(bones,count,out,wrist,palm))return false;
    int tip=-1;unsigned deepest=0;
    for(unsigned i=0;i<count;++i){
        const auto key=HandBoneKey(bones[i].BoneName);
        if((!std::strstr(key.data(),"thumb") && !std::strstr(key.data(),"finger0")) ||
            !HandDescendant(bones,count,i,wrist) || HandDescendant(bones,count,i,gun))continue;
        unsigned depth=0;int p=i;
        while(p>=0 && p<static_cast<int>(count) && p!=wrist && depth<count){p=bones[p].Parent;++depth;}
        if(p==wrist && depth>deepest){tip=i;deepest=depth;}
    }
    if(tip<0 || deepest<2)return false;
    const int parent=bones[tip].Parent;
    Vector3 along=out[tip].translation-out[parent].translation;
    if(along.lengthSqr()<1e-9f)return false;
    along.normalize();Vector3 back=palm.getForwardAxis();back-=along*back.dot(along);
    if(back.lengthSqr()<1e-9f)return false;
    back.normalize();const float mirror=palm.getDeterminant()<0?-1.f:1.f;
    target=GripAxes(along,back.cross(along)*mirror,back);
    target.translate((out[tip].translation+out[parent].translation)*.5f-back*clearance);
    return true;
}
template<class B> void PlacePalmGrip(const B* bones,unsigned count,Transform* out,int wrist,int gun,
    const ModelNode* rest,unsigned restCount,const Matrix4& desiredPalm,const Matrix4& fallbackWrist) {
    std::array<Transform,64> bind{};if(count>64)return;
    for(unsigned i=0;i<count;++i)bind[i]=out[i];
    HandBindPose(bones,count,rest,restCount,bind.data());
    Matrix4 palm,target=fallbackWrist;
    const Matrix4 w=ModelMatrix(bind[wrist]);
    if(std::fabs(w.getDeterminant())>.00001f && LeftPalmFrame(bones,count,bind.data(),wrist,palm)) {
        const Matrix4 relative=Matrix4(w).invertAffine()*palm;
        target=desiredPalm*Matrix4(relative).invertAffine();
    }
    MoveGripHand(bones,count,out,wrist,gun,target);
}
}
