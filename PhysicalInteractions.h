#pragma once
#include "WeaponSession.h"
#include "NativeModelPose.h"

namespace Immersion {
struct GrenadeInventory {
    Object unit=None; std::uint32_t storage=0;
    std::array<unsigned,2> count{}; // Native order: frag, plasma.
    unsigned state=0; int selected=-1; std::uint32_t age=0;
};
struct TrackedGrenadeRelease {
    Object unit=None; unsigned type=0, hand=0; DropPose pose{};
};
// One native grenade animation at a time, with one queued second-hand release.
// A release never overwrites the throw already owned by the native animation.
class PhysicalGrenades {
public:
    std::array<int,2> held{{-1,-1}};
    TrackedGrenadeRelease release{};
    bool pending=false, issued=false,acknowledged=false; unsigned countAtIssue=0; float elapsed=0;
    void Reset(){held={{-1,-1}};pending=issued=acknowledged=queued=false;elapsed=0;owner=None;storage=age=0;}
    void Observe(const GrenadeInventory& inventory){
        if(owner!=inventory.unit || storage!=inventory.storage || inventory.age<age){Reset();owner=inventory.unit;storage=inventory.storage;}
        age=inventory.age;
    }
    unsigned Reserved(unsigned type)const{
        unsigned n=pending && release.type==type?1u:0u;
        if(queued && nextRelease.type==type)++n;
        for(int t:held)if(t==static_cast<int>(type))++n;
        return n;
    }
    bool Grab(unsigned hand,unsigned type,const GrenadeInventory& inventory){
        if(hand>=2 || type>=2 || held[hand]>=0 || inventory.unit!=owner ||
            inventory.count[type]<=Reserved(type))return false;
        held[hand]=static_cast<int>(type);return true;
    }
    bool Release(unsigned hand,bool returned,const DropPose& pose){
        if(hand>=2 || held[hand]<0)return false;
        const unsigned type=static_cast<unsigned>(held[hand]);
        held[hand]=-1;
        if(returned || !pose.position.Finite() || !pose.velocity.Finite())return false;
        if(pending){
            if(queued)return false;
            nextRelease={owner,type,hand,pose};queued=true;return true;
        }
        release={owner,type,hand,pose};pending=true;issued=acknowledged=false;elapsed=0;return true;
    }
    void Finish(){
        pending=issued=acknowledged=false;elapsed=0;
        if(queued){release=nextRelease;queued=false;pending=true;}
    }
    void Tick(float dt){if(pending && std::isfinite(dt) && dt>0){elapsed+=dt;if(elapsed>2.5f){pending=issued=acknowledged=queued=false;elapsed=0;}}}
private:Object owner=None;std::uint32_t storage=0,age=0;
    bool queued=false;TrackedGrenadeRelease nextRelease{};
};
// Short release window preserves a natural apex release without retaining an
// old backswing after the player pauses. Samples are already world velocity.
class ThrowVelocityHistory {
public:
    void Reset(){samples={};next=0;}
    void Add(Point v,float dt){
        if(!v.Finite() || !std::isfinite(dt) || dt<=0 || dt>.1f){Reset();return;}
        for(auto& s:samples)s.age+=dt;
        samples[next]={v,0};next=(next+1)%samples.size();
    }
    Point Release(Point current)const{
        Point result=current.Finite()?current:Point{};
        for(const auto& s:samples)if(s.age<=.075f && s.velocity.LengthSquared()>result.LengthSquared())result=s.velocity;
        return result;
    }
private:
    struct Sample{Point velocity{};float age=1;};
    std::array<Sample,16> samples{};unsigned next=0;
};
// Boost the tracked launch response, including existing saved strength values.
// A resting hand still drops; direction and proportional effort are retained.
constexpr float GrenadeLaunchBoost=2.f;
constexpr float GrenadeMaximumMetresPerSecond=24.f;
inline Point GrenadeVelocity(Point controllerWorldPerSecond,Point playerPerTick,float strength,float maximumWorldSpeed){
    if(!controllerWorldPerSecond.Finite() || !playerPerTick.Finite() || !std::isfinite(strength) ||
        !std::isfinite(maximumWorldSpeed) || maximumWorldSpeed<=0)return {};
    Point v=controllerWorldPerSecond*(GrenadeLaunchBoost*(std::max)(0.f,(std::min)(strength,3.f)));
    const float speed=std::sqrt(v.LengthSquared());
    if(speed>maximumWorldSpeed)v=v*(maximumWorldSpeed/speed);
    return v*(1.f/30.f)+playerPerTick;
}
enum class VehicleSeat { None, WarthogDriver, WarthogTurret, Ghost, ShadeTurret, PelicanPassenger };
// AJBats' reported exit feedback loop: let native dismount rotation settle
// before deriving aim corrections from it. Time-based at all headset rates.
class VehicleExitGuard {
public:
    void Reset(){seated=false;remaining=0;sync=false;}
    void Observe(bool attached,bool valid=true){
        if(!valid){Reset();return;}
        if(seated && !attached){remaining=1.f;sync=true;}
        if(attached){remaining=0;sync=false;}
        seated=attached;
    }
    void Advance(float dt){if(std::isfinite(dt) && dt>0)remaining=(std::max)(0.f,remaining-(std::min)(dt,.25f));}
    bool Suppressing()const{return remaining>0 || sync;}
    bool TakeSync(){if(remaining>0 || !sync)return false;sync=false;return true;}
private:
    bool seated=false,sync=false;float remaining=0;
};
inline bool IsTurretSeat(VehicleSeat seat){return seat==VehicleSeat::WarthogTurret || seat==VehicleSeat::ShadeTurret;}
inline float TurretStick(float value){
    if(!std::isfinite(value))return 0;
    const float magnitude=(std::min)(1.f,std::fabs(value));
    if(magnitude<=.15f)return 0;
    const float t=(magnitude-.15f)/.85f;
    return std::copysign(t*t,value);
}
// Keep chest capture close to the visible model in the forward direction;
// release-to-return remains generous. Left/offhand tools take priority below.
inline bool GrenadeGripContains(Point hand,Point center,float yaw,unsigned physicalHand){
    const auto d=hand-center;const float c=std::cos(yaw),s=std::sin(yaw);
    const float forward=(d.x*c+d.y*s)/(physicalHand==0?.065f:.09f);
    const float side=(-d.x*s+d.y*c)/.10f,up=d.z/.10f;
    return forward*forward+side*side+up*up<=1.f;
}
// Widen only the scope render; a fraction of the excess magnification retains
// a useful zoom at both sniper stages while 1 restores the native lens.
inline float ComfortableScopeFov(float nativeFov,float unzoomedFov,float strength){
    if(!std::isfinite(nativeFov) || nativeFov<=0 || nativeFov>=3.0f)return nativeFov;
    if(!std::isfinite(unzoomedFov) || unzoomedFov<=nativeFov || unzoomedFov>=3.0f)return nativeFov;
    strength=std::isfinite(strength)?(std::max)(.15f,(std::min)(strength,1.f)):.5f;
    const float base=std::tan(unzoomedFov*.5f),nativeZoom=base/std::tan(nativeFov*.5f);
    return 2.f*std::atan(base/(1.f+(nativeZoom-1.f)*strength));
}
struct VehicleState {
    Object unit=None, vehicle=None; int seat=-1; VehicleSeat kind=VehicleSeat::None;
    Point eye{},forward{},up{},aim{},position{},velocity{};
    // Sampled from the native model nodes in the render frame. Chimera
    // interpolates these before drawing, but leaves object position untouched.
    Matrix4 renderedBasis;
    Point renderedFacing{};
    bool renderedPoseValid=false;
    // A turret gunner rotates relative to its chassis. The head
    // bind correction supplies actual rendered yaw rather than chassis yaw.
    Point renderedHeadFacing{};
    bool renderedHeadValid=false;
    std::array<Matrix4,2> handles{};
    std::array<bool,2> handleValid{};
};
inline float WrapRadians(float angle){
    if(!std::isfinite(angle))return 0;
    return std::remainder(angle,6.28318530718f);
}
inline Matrix4 VehicleBasis(const VehicleState& seat) {
    if(seat.renderedPoseValid)return seat.renderedBasis;
    Vector3 forward(seat.forward.x,seat.forward.y,seat.forward.z),up(seat.up.x,seat.up.y,seat.up.z);
    forward.normalize();up-=forward*up.dot(forward);up.normalize();
    const auto left=up.cross(forward);
    Matrix4 result;
    for(unsigned i=0;i<3;++i){result[i]=forward[i];result[4+i]=left[i];result[8+i]=up[i];}
    result.translate({seat.position.x,seat.position.y,seat.position.z});return result;
}
// Native seat ownership becomes valid before the climb-in animation finishes.
// Settle against the moving head in VEHICLE space, then ignore small idle
// motion. A displacement can reopen settling during the first three seconds
// for delayed entry. After that, turret/body animation cannot move the anchor.
class SeatAnchor {
public:
    void Reset(){ready=locked=false;age=quiet=total=0;}
    bool Locked()const{return locked;}
    Vector3 Update(Vector3 headLocal,float dt,float worldPerMetre){
        if(!ready){position=previous=headLocal;ready=true;return position;}
        if(!std::isfinite(dt) || dt<=0)return position;
        dt=(std::min)(dt,.1f);total+=dt;
        // Only entry may recapture the head. Turret aiming and suspension
        // animations must never reopen a settled seat later in the ride.
        if(total>=3.f){locked=true;return position;}
        if(locked && (headLocal-position).lengthSqr()>.12f*.12f*worldPerMetre*worldPerMetre){
            locked=false;age=quiet=0;
        }
        if(!locked){
            age+=dt;
            quiet=(headLocal-previous).lengthSqr()<=.012f*.012f*worldPerMetre*worldPerMetre?quiet+dt:0.f;
            position+=(headLocal-position)*(1.f-std::exp(-dt/.07f));
            if(age>=.75f && quiet>=.30f && (headLocal-position).lengthSqr()<.02f*.02f*worldPerMetre*worldPerMetre)locked=true;
        }
        previous=headLocal;return position;
    }
private:
    bool ready=false,locked=false;float age=0,quiet=0,total=0;Vector3 position{},previous{};
};
// One simulation-tick interpolation buffer for native motion. The old filter
// advanced by velocity every render frame then pulled back toward a repeated
// 30 Hz snapshot: that produces a sawtooth even at constant driving speed.
// No HMD pose enters this buffer. Stops hold; teleports and stalls reset it.
class SeatMotion {
public:
    void Reset(){count=0;clock=0;}
    Vector3 Update(Vector3 target,Vector3 /*velocity*/,float dt){
        if(!count || !std::isfinite(dt) || dt<=0 || dt>.1f || (target-samples[count-1].position).lengthSqr()>4.f){
            Reset();samples[count++]={0,target};return target;
        }
        clock+=dt;
        if((target-samples[count-1].position).lengthSqr()>1.e-12f){
            // A stationary interval is a hold, not a slow journey beginning
            // at the last movement many seconds ago.
            if(clock-samples[count-1].time>2.f/30.f)Push({clock-1.f/30.f,samples[count-1].position});
            Push({clock,target});
        }
        const float renderTime=clock-1.f/30.f;
        while(count>2 && samples[1].time<=renderTime){
            for(unsigned i=1;i<count;++i){samples[i-1]=samples[i];}
            --count;
        }
        if(count<2 || renderTime>=samples[count-1].time)return samples[count-1].position;
        if(renderTime<=samples[0].time)return samples[0].position;
        const float fraction=(renderTime-samples[0].time)/(samples[1].time-samples[0].time);
        return samples[0].position+(samples[1].position-samples[0].position)*fraction;
    }
private:
    struct Sample{float time=0;Vector3 position{};};
    std::array<Sample,16> samples{};unsigned count=0;float clock=0;
    void Push(Sample sample){
        if(count==samples.size()){for(unsigned i=1;i<count;++i){samples[i-1]=samples[i];}
            --count;}
        samples[count++]=sample;
    }
};
inline bool HideSeatedHead(const ModelNode* nodes,unsigned count,Transform* transforms){
    if(!transforms || !ValidModelNodes(nodes,count))return false;
    int head=-1;
    for(unsigned i=0;i<count;++i){char name[33]{};std::memcpy(name,nodes[i].name,32);
        if(std::strstr(name,"head")){head=static_cast<int>(i);break;}}
    if(head<0)return false;
    const auto anchor=transforms[head].translation;
    for(unsigned i=0;i<count;++i){int parent=static_cast<int>(i);
        while(parent>=0 && parent!=head)parent=nodes[parent].parent;
        if(parent==head){
            transforms[i].scale=0;
            for(float& axis:transforms[i].rotation)axis=0;
            transforms[i].translation=anchor;
        }}
    return true;
}
// Palm X is the finger direction. Rocket +X runs across the palm (+Y),
// rather than into it. Keep the barrel clear of the palm skin.
inline Matrix4 RocketPalmGrasp(const Matrix4& palm,float scale){
    Matrix4 across;across[0]=0;across[1]=1;across[4]=-1;across[5]=0;
    Matrix4 result=palm*across;
    result.translate((palm.getLeftAxis()*.018f-palm.getForwardAxis()*.035f)*scale);
    return result;
}
}
