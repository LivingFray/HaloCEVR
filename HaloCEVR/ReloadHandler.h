#pragma once
#include "Immersion/ManualReload.h"
#include "Immersion/NativeModelPose.h"
#include "Immersion/WeaponSession.h"
#include "ReloadAudio.h"
#include "ReloadSoundBank.h"
#include <map>
namespace Immersion { class RetailWeaponEngine; }
struct Bone;
class ReloadHandler {
public:
    bool Process(Immersion::RetailWeaponEngine& engine, Immersion::WeaponSession& session,
        Immersion::WeaponFrame& frame, const Matrix4* hands, Immersion::Point head, float yaw,
        bool reloadDown, bool reloadValid);
    void Suspend();
    void Deactivate();
    bool BlocksNative(Immersion::Object weapon) const;
    bool BlocksFire(Immersion::Object weapon) const; // Manual chamber/part state.
    bool BlocksNativeFire(Immersion::Object weapon) const; // Empty attempts retain Halo's dry click.
    void BeforeShot(Immersion::Object weapon);
    void AfterShot(Immersion::Object weapon);
    void Draw();
    void DrawNativePieces(); // Only from the native first-person model pass.
    bool DeferPumpSound(Immersion::Object weapon,Immersion::Object sound) const;
    bool DeferPumpEffect(Immersion::Object weapon,Immersion::Object effect) const;
    bool HandBusy(Immersion::Hand hand)const{return active && tool!=Immersion::ReloadTool::None && hand==offhand;}
    bool PumpSupportsWeapon() const {return active && tool==Immersion::ReloadTool::Action && pumpSupported;}

    void Pose(Immersion::Object weapon, const Bone* bones, Transform* out, unsigned count, int gun, int wrist=-1);
private:
    void CancelTool();
    void Feedback(Immersion::ReloadCue cue, Immersion::Hand hand, float amplitude=0.3f);
    Immersion::ReloadState& Observe(const Immersion::ReloadAmmo& ammo);
    void PumpPulled(Immersion::ReloadState& state);
    void CacheModel(Immersion::Object weapon, Immersion::Object tag);
    Immersion::RetailWeaponEngine* engine=nullptr;
    bool active=false, consumed=false;
    Immersion::Object owner=Immersion::None, held=Immersion::None;
    std::uint32_t ownerStorage=0, ownerAge=0;
    std::map<Immersion::Object,Immersion::ReloadState> records;
    Immersion::Button reloadButton;
    Immersion::Hand offhand=Immersion::Hand::Left;
    Immersion::ReloadTool tool=Immersion::ReloadTool::None;
    Immersion::ReloadStroke stroke;
    Immersion::Object buttonReloadPermit=Immersion::None;
    Immersion::ReloadAmmo beforeShot;
    bool shotPending=false;
    Immersion::Point toolStart{}, supply{};
    Matrix4 weaponPose, offhandPose, toolPose, fallingPose;
    Immersion::Point palmFromController{};
    bool palmValid=false,rocketPalmValid=false;
    Matrix4 rocketPalmRelative;
    float bodyYaw=0, fallingTime=0;
    Vector3 fallingVelocity{};
    Immersion::ReloadWeaponKind fallingKind=Immersion::ReloadWeaponKind::Native;
    Immersion::ReloadSockets sockets;
    Immersion::Object modelWeapon=Immersion::None, modelTag=Immersion::None;
    std::array<Immersion::ModelNode,64> nodes{};
    unsigned nodeCount=0;
    int magNode=-1, actionNode=-1, gunNode=-1;
    bool socketValid[2]{};
    Matrix4 magRelative, actionRelative, supportRelative, socketBasis;
    bool supportValid=false,pumpSupported=false;
    int shellNode=-1,portNode=-1;
    Immersion::ReloadFeedbackAssets feedback;
    bool audioReady=false,rocketModelAvailable=false,fallingSpent=true;
    ReloadSoundBank soundBank;

    std::array<Immersion::ModelNode,64> handNodes{};
    unsigned handNodeCount=0;
};
