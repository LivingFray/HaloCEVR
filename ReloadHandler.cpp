#include "ReloadHandler.h"
#include "Immersion/HandGripPose.h"
#include "Immersion/PhysicalInteractions.h"
#include "Immersion/OutlineZone.h"
#include "Game.h"
#include "Helpers/Assets.h"
#include "Helpers/Camera.h"
#include "Helpers/Controls.h"
#include "Helpers/Menus.h"
#include "Helpers/Cutscene.h"
#include "Logger.h"
#include <cctype>
using namespace Immersion;
namespace {
Point P(Vector3 p){return {p.x,p.y,p.z};}
Vector3 V(Point p){return {p.x,p.y,p.z};}
float Bounded(float v,float fallback,float low,float high){return (std::max)(low,(std::min)(high,std::isfinite(v)?v:fallback));}
bool Descendant(const Bone* bones,unsigned count,int node,int parent) {
    for(unsigned n=0;n<count && node>=0 && node<static_cast<int>(count);++n){
        if(node==parent)return true;
        node=bones[node].Parent;
    }return false;
}
void MovePart(const Bone* bones,unsigned count,Transform* out,int index,const Matrix4& target,bool hide=false){
    if(index<0)return;
    Matrix4 old=ModelMatrix(out[index]);
    if(std::fabs(old.getDeterminant())<0.00001f)return;
    const Matrix4 delta=target*old.invertAffine();
    for(unsigned i=0;i<count;++i)if(Descendant(bones,count,static_cast<int>(i),index)){
        out[i]=ModelTransform(delta*ModelMatrix(out[i]));if(hide)out[i].scale=0;
    }
}
void MoveHand(const Bone* bones,unsigned count,Transform* out,int wrist,int gun,const Matrix4& delta){
    for(unsigned i=0;i<count;++i)if(Descendant(bones,count,static_cast<int>(i),wrist) &&
        !Descendant(bones,count,static_cast<int>(i),gun))out[i]=ModelTransform(delta*ModelMatrix(out[i]));
    const int parent=bones[wrist].Parent;
    if(parent>=0 && parent<static_cast<int>(count) && out[parent].scale==0)out[parent].translation=out[wrist].translation;
}
void CloseReloadHand(const Bone* bones,unsigned count,Transform* out,int wrist,int gun,
    const ModelNode* rest,unsigned restCount,bool thumbOut=false){
    CloseGripHand(bones,count,out,wrist,gun,rest,restCount,1.f,thumbOut?0.f:1.f);
}

void DrawReloadPiece(const Matrix4& pose,ReloadWeaponKind kind,bool spent=false){
    // Original simple meshes; native detachable magazine nodes are used when available.
    auto& renderer=Game::instance.inGameRenderer;
    const float radius=kind==ReloadWeaponKind::Rocket ? .028f : .014f;
    const float length=kind==ReloadWeaponKind::Rocket ? .23f : kind==ReloadWeaponKind::Shotgun ? .06f : .12f;
    if(kind==ReloadWeaponKind::Shotgun || kind==ReloadWeaponKind::Rocket){
        for(unsigned n=0;n<10;++n){
            const float a=n*6.2831853f/10,b=(n+1)*6.2831853f/10;
            Vector3 q[4]={pose*Vector3(0,radius*std::cos(a),radius*std::sin(a)),pose*Vector3(length,radius*std::cos(a),radius*std::sin(a)),
                pose*Vector3(length,radius*std::cos(b),radius*std::sin(b)),pose*Vector3(0,radius*std::cos(b),radius*std::sin(b))};
            const D3DCOLOR color=spent?D3DCOLOR_ARGB(255,85,85,80):kind==ReloadWeaponKind::Shotgun?D3DCOLOR_ARGB(255,155,35,25):D3DCOLOR_ARGB(255,85,105,65);
            const D3DCOLOR colors[3]={color,color,color};Vector3 first[3]={q[0],q[1],q[2]},second[3]={q[0],q[2],q[3]};
            renderer.DrawMeshTriangle(first,colors);renderer.DrawMeshTriangle(second,colors);
            const Vector3 cap[3]={pose*Vector3(length+.015f,0,0),q[2],q[1]};
            const D3DCOLOR brass[3]={D3DCOLOR_ARGB(255,175,145,65),D3DCOLOR_ARGB(255,175,145,65),D3DCOLOR_ARGB(255,175,145,65)};
            renderer.DrawMeshTriangle(cap,brass);
        }
    }else{
        const float wide=kind==ReloadWeaponKind::Pistol?.015f:.03f;
        const Vector3 vertices[8]={{0,-wide,0},{.045f,-wide,0},{.045f,wide,0},{0,wide,0},
            {0,-wide,-length},{.045f,-wide,-length},{.045f,wide,-length},{0,wide,-length}};
        const unsigned faces[12][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
        for(const auto& f:faces){const Vector3 tri[3]={pose*vertices[f[0]],pose*vertices[f[1]],pose*vertices[f[2]]};
            const D3DCOLOR color=D3DCOLOR_ARGB(255,70+f[0]*3,76+f[0]*3,78+f[0]*3),colors[3]={color,color,color};renderer.DrawMeshTriangle(tri,colors);}
    }
}
}
void ReloadHandler::CancelTool(){tool=ReloadTool::None;stroke={};consumed=false;pumpSupported=false;}
void ReloadHandler::Suspend(){palmValid=rocketPalmValid=false;CancelTool();reloadButton.Suspend();shotPending=false;soundBank.Reset();modelWeapon=None;}
void ReloadHandler::Deactivate(){Suspend();active=false;held=owner=None;records.clear();fallingTime=0;modelWeapon=None;nodeCount=0;}
void ReloadHandler::Feedback(ReloadCue cue,Hand hand,float amplitude){
    auto& game=Game::instance;
    game.GetVR()->TriggerHapticVibration(hand==Hand::Left?ControllerRole::Left:ControllerRole::Right,0,.045f,110,amplitude);
    const auto sound=feedback.sounds[static_cast<unsigned>(cue)];
    if(audioReady && engine && engine->ValidReloadFeedbackTag(held,sound))
        soundBank.Play(sound,feedback.slices[static_cast<unsigned>(cue)],Bounded(game.c_ReloadSoundVolume->Value(),.6f,0,1));

}
ReloadState& ReloadHandler::Observe(const ReloadAmmo& ammo){auto& state=records[ammo.weapon];state.Observe(ammo);return state;}
bool ReloadHandler::BlocksNative(Object weapon)const{
    ReloadAmmo ammo;
    return active && weapon!=buttonReloadPermit && Game::instance.c_ManualReload->Value() && engine && engine->ReadReloadAmmo(weapon,ammo,false) &&
        ammo.nativeState!=1 && ammo.nativeState!=3; // Let a reload already started before activation finish.
}
bool ReloadHandler::BlocksFire(Object weapon)const{
    if(!BlocksNative(weapon))return false;
    const auto found=records.find(weapon);
    return (weapon==held && tool==ReloadTool::Action) || (found!=records.end() && found->second.BlocksFire());
}
bool ReloadHandler::BlocksNativeFire(Object weapon)const{
    if(!BlocksFire(weapon))return false;
    ReloadAmmo ammo;
    // The verified native fire entry handles an empty magazine without firing
    // a projectile. Let it produce the weapon's own dry sound, just as it already
    // does for the launcher. Automatic reload remains guarded independently.
    return !engine || !engine->ReadReloadAmmo(weapon,ammo) || ammo.loaded>0;
}
void ReloadHandler::BeforeShot(Object weapon){
    shotPending=BlocksNative(weapon) && engine->ReadReloadAmmo(weapon,beforeShot) && beforeShot.loaded>0 && beforeShot.kind!=ReloadWeaponKind::Needler;
}
void ReloadHandler::AfterShot(Object weapon){
    const bool pending=shotPending;shotPending=false;
    ReloadAmmo ammo;
    if(pending && beforeShot.weapon==weapon && BlocksNative(weapon) && engine->ReadReloadAmmo(weapon,ammo) &&
        ammo.unit==beforeShot.unit && ammo.tag==beforeShot.tag && (ammo.loaded<beforeShot.loaded || ammo.tick!=beforeShot.tick)){
        auto& state=Observe(ammo);
        if(ammo.kind==ReloadWeaponKind::Shotgun){state.needsRack=true;state.spentShell=true;}
    }
}

bool ReloadHandler::DeferPumpSound(Object weapon,Object sound)const{
    const auto found=records.find(weapon);ReloadAmmo ammo;
    return active && Game::instance.c_ManualReload->Value() && engine && engine->ReadReloadAmmo(weapon,ammo) && weapon==held && weapon==modelWeapon && found!=records.end() &&
        found->second.kind==ReloadWeaponKind::Shotgun && feedback.IsPumpSound(sound);
}
bool ReloadHandler::DeferPumpEffect(Object weapon,Object effect)const{
    const auto found=records.find(weapon);ReloadAmmo ammo;
    return active && Game::instance.c_ManualReload->Value() && engine && engine->ReadReloadAmmo(weapon,ammo) && weapon==held && weapon==modelWeapon && effect!=None && effect==feedback.chamberEffect &&
        found!=records.end() && found->second.kind==ReloadWeaponKind::Shotgun;
}
void ReloadHandler::PumpPulled(ReloadState& state){
    if(state.kind!=ReloadWeaponKind::Shotgun || !state.spentShell)return;
    // Consume once, including a last-round pump. Regripping/canceling cannot
    // eject the same shell twice; a fresh shell insertion cannot create one.
    state.spentShell=false;
    if(audioReady && engine && !feedback.chamberIsSound && engine->ValidReloadFeedbackTag(held,feedback.chamberEffect,true))
        PlayNativeReloadEffect(held,feedback.chamberEffect);
    else if(shellNode>=0){
        // Some maps have no chambering effect. Reuse their textured shell node
        // as the spent-shell preview instead of inventing a new cylinder.
        fallingPose=weaponPose;fallingPose.translate(weaponPose.getLeftAxis()*sockets.insert.x+
            weaponPose.getUpAxis()*sockets.insert.y+weaponPose.getForwardAxis()*sockets.insert.z);
        fallingKind=ReloadWeaponKind::Shotgun;fallingTime=.75f;
        fallingVelocity=weaponPose.getUpAxis()*-.75f+weaponPose.getForwardAxis()*.3f;
    }
}

bool ReloadHandler::Process(RetailWeaponEngine& native,WeaponSession& session,WeaponFrame& frame,
    const Matrix4* hands,Point head,float yaw,bool reloadDown,bool reloadValid){
    auto& game=Game::instance;engine=&native;
    NativeInventory inventory;
    if(!game.c_ManualReload->Value() || !engine->ReadNative(inventory)){Deactivate();return false;}
    active=true;
    if(owner!=inventory.unit || ownerStorage!=inventory.storage || inventory.age<ownerAge){
        records.clear();Suspend();modelWeapon=None;nodeCount=0;
    }
    owner=inventory.unit;ownerStorage=inventory.storage;ownerAge=inventory.age;
    bodyYaw=yaw;supply=Layout{game.c_LeftHanded->Value()}.Anchor(Zone::SupplyHip,head,yaw);
    const Object nowHeld=session.Held();const Hand nowOffhand=Other(session.FiringHand());
    if(held!=nowHeld || offhand!=nowOffhand){CancelTool();supportValid=false;palmValid=rocketPalmValid=false;reloadButton.Suspend();held=nowHeld;offhand=nowOffhand;}
    const auto reload=reloadButton.Update(reloadDown,reloadValid);
    ReloadAmmo ammo;
    if(held==None || session.SupportOnly() || !engine->ReadReloadAmmo(held,ammo)){CancelTool();return false;}
    // Native animation reload must not begin for a manually controlled gun.
    Helpers::GetControls().Reload=0;game.bIsReloading=false;
    if(ammo.kind==ReloadWeaponKind::Needler){
        CancelTool();
        if(!reload.pressed || !frame.grip[Index(session.FiringHand())].down)return false;
        // The permit exists only during this checked synchronous call, never as
        // a time window in which a later automatic reload could slip through.
        buttonReloadPermit=held;
        const bool started=engine->StartButtonReload(held);
        buttonReloadPermit=None;
        if(started)game.GetVR()->TriggerHapticVibration(session.FiringHand()==Hand::Left?ControllerRole::Left:ControllerRole::Right,0,.045f,110,.3f);
        return started;
    }
    auto& state=Observe(ammo);
    auto recovered=state;auto reconciledAmmo=ammo;
    if(recovered.RecoverUnseatedRounds(reconciledAmmo) && engine->CommitReloadAmmo(ammo,reconciledAmmo)){
        Logger::log<<"[ManualReload] recovered unseated rounds weapon="<<held<<" rounds="<<ammo.loaded
            <<" reserve="<<reconciledAmmo.reserve<<" rack="<<recovered.needsRack<<std::endl;
        state=recovered;ammo=reconciledAmmo;
    }
    const unsigned h=Index(offhand),main=Index(session.FiringHand());
    if(!frame.grip[main].down || frame.consumed[h]){
        if(!frame.grip[main].down && frame.grip[h].down && tool==ReloadTool::Action && ammo.kind==ReloadWeaponKind::Shotgun)
            session.RestoreSupportAfterPump(offhand);
        CancelTool();return false;
    }
    weaponPose=hands[main];offhandPose=hands[h];toolPose=hands[h];toolPose.translate(toolPose.getLeftAxis()*.045f);
    // Raise the pistol magazine relative to the tracked hand, so the grasp
    // sits lower on its body. Interaction/controller positions stay unchanged.
    if(ammo.kind==ReloadWeaponKind::Pistol)toolPose.translate(toolPose.getForwardAxis()*.035f);
    // Anatomical palm basis is captured from Chief's actual knuckles in Pose.
    // Fresh, spent and dropped previews share this orientation.
    if(ammo.kind==ReloadWeaponKind::Rocket)
        toolPose=RocketPalmGrasp(rocketPalmValid?offhandPose*rocketPalmRelative:offhandPose,1.f);
    Matrix4 inverse=weaponPose;inverse.invertAffine();
    const Point local=P(inverse*(hands[h]*Vector3()));
    const Point position=P(hands[h]*Vector3());
    const float radius=Bounded(game.c_ReloadGrabRadius->Value(),.14f,.06f,.24f);
    const float strokeLength=Bounded(game.c_ReloadStroke->Value(),.09f,.04f,.20f)*
        (ammo.kind==ReloadWeaponKind::Shotgun?.60f:1.f);
    const bool atSupply=Within(position,supply,Bounded(game.c_MagazineZoneRadius->Value(),.17f,.08f,.25f));
    // Event-only diagnostics: identify empty reserves, occupied hands or a
    // native/manual state disagreement without recording every input frame.
    if(frame.grip[h].pressed && atSupply)
        Logger::log<<"[ManualReload] supply grip weapon="<<held<<" hand="<<(offhand==Hand::Left?"L":"R")
            <<" loaded="<<ammo.loaded<<" reserve="<<ammo.reserve<<" capacity="<<ammo.capacity
            <<" magazine="<<state.magazine<<" rack="<<state.needsRack<<" native="<<ammo.nativeState
            <<" tool="<<static_cast<int>(tool)<<" occupied="<<!session.Items().Empty(offhand)
            <<" consumed="<<consumed<<std::endl;
    CacheModel(held,ammo.tag);
    if(audioReady && !soundBank.Update(*engine,held,game.lastDeltaTime))
        Logger::log<<"[ManualReloadAudio] "<<soundBank.LastIssue()<<"; haptics retained. weapon="<<held<<std::endl;
    sockets=DefaultReloadSockets(ammo.kind);
    // Pose() supplies native-node locations relative to the firing controller.
    if(socketValid[0])sockets.insert=P(magRelative*Vector3());
    if(socketValid[1])sockets.action=P(actionRelative*Vector3());
    if(ammo.kind==ReloadWeaponKind::Rocket && socketValid[1])
        sockets.insert=sockets.action+P(socketBasis*Vector3(state.open?-strokeLength:0,0,0));
    const auto atInsertion=[&](){
        if(ammo.kind==ReloadWeaponKind::Pistol)
            return NearPistolMagazinePort(local,sockets.insert,P(socketBasis.getForwardAxis()),radius);
        return Within(local,sockets.insert,(ammo.kind==ReloadWeaponKind::Shotgun || ammo.kind==ReloadWeaponKind::Rocket)?(std::max)(radius,.19f):radius);
    };
    bool acted=false;
    // B/Y operates the launcher casing. Repeated presses may close an empty
    // casing without spending ammo; a held reload piece cannot be interrupted.
    if(reload.pressed && ammo.kind==ReloadWeaponKind::Rocket && tool==ReloadTool::None){
        state.open=!state.open;
        Feedback(state.open?ReloadCue::Pull:ReloadCue::Close,session.FiringHand(),.5f);
        Logger::log<<"[ManualReload] rocket casing open="<<state.open<<" weapon="<<held<<std::endl;
        acted=true;
    }
    if(reload.pressed && MagazineReload(ammo.kind)){
        auto candidate=state;auto after=ammo;
        if(candidate.Eject(after) && engine->CommitReloadAmmo(ammo,after)){
            Logger::log<<"[ManualReload] eject weapon="<<held<<" retained="<<ammo.loaded<<" rack="<<candidate.needsRack<<std::endl;
            state=candidate;ammo=after;fallingPose=weaponPose;fallingPose.translate(weaponPose.getLeftAxis()*sockets.insert.x+weaponPose.getUpAxis()*sockets.insert.y+weaponPose.getForwardAxis()*sockets.insert.z);
            fallingTime=.75f;fallingVelocity={};fallingKind=ammo.kind;Feedback(ReloadCue::Eject,session.FiringHand());acted=true;
        }
    }
    if(consumed && !frame.grip[h].down)consumed=false;
    if(tool!=ReloadTool::None){
        consumed=frame.grip[h].down;frame.consumed[h]=true;
        if(tool==ReloadTool::Action){
            if((local-stroke.start).LengthSquared()>.36f){CancelTool();consumed=frame.grip[h].down;return acted;}
            if(!frame.grip[h].down){
                if(MagazineReload(ammo.kind) && frame.grip[h].released){
                    const bool wasPulled=stroke.pulled;
                    stroke.Update(local,strokeLength);
                    if(!wasPulled && stroke.pulled)Feedback(ReloadCue::Pull,offhand,.4f);
                }
                // Magazine-fed actions spring shut when released after a full
                // rearward pull. A shotgun still needs a complete manual pump.
                if(MagazineReload(ammo.kind) && (stroke.pulled || stroke.travel>.01f)){
                    if(stroke.pulled)state.Rack(ammo);
                    Feedback(ReloadCue::Close,offhand,.5f);acted=true;
                    Logger::log<<"[ManualReload] action ready (release) weapon="<<held<<std::endl;
                }else if(!MagazineReload(ammo.kind))Feedback(ReloadCue::Drop,offhand,.15f);
                CancelTool();return acted;
            }
            {
                const bool wasPulled=stroke.pulled;
                const bool complete=stroke.Update(local,strokeLength,ammo.kind==ReloadWeaponKind::Shotgun?.60f:.30f);
                if(!wasPulled && stroke.pulled){
                    PumpPulled(state);
                    Feedback(ReloadCue::Pull,offhand,.4f);
                    Logger::log<<"[ManualReload] action pulled weapon="<<held<<std::endl;
                }
                if(complete && (MagazineReload(ammo.kind) || state.Rack(ammo))){
                    if(MagazineReload(ammo.kind))state.Rack(ammo);
                    if(ammo.kind==ReloadWeaponKind::Shotgun)session.RestoreSupportAfterPump(offhand);
                    if(ammo.kind==ReloadWeaponKind::Pistol)stroke.pulled=false; // Magnum alone allows another free stroke while held.
                    else tool=ReloadTool::None;
                    consumed=ammo.kind!=ReloadWeaponKind::Shotgun;
                    Feedback(ReloadCue::Close,offhand,.5f);acted=true;
                    Logger::log<<"[ManualReload] action ready weapon="<<held<<std::endl;
                }
            }
        }else{
            const bool atPort=atInsertion();
            const bool moved=(position-toolStart).LengthSquared()>.0036f;
            const bool released=frame.grip[h].released;
            if(tool==ReloadTool::SpentRocket){
                if(released){if(state.spent>0)--state.spent;fallingPose=toolPose;fallingTime=.75f;fallingVelocity={};fallingKind=ReloadWeaponKind::Rocket;fallingSpent=true;CancelTool();Feedback(ReloadCue::Drop,offhand);acted=true;}
            }else if(atPort && moved){
                auto candidate=state;auto after=ammo;
                if(candidate.Insert(after,game.c_ShotgunShellRatio->Value()) && engine->CommitReloadAmmo(ammo,after)){
                    Logger::log<<"[ManualReload] insert weapon="<<held<<" rounds="<<after.loaded-ammo.loaded
                        <<" loaded="<<after.loaded<<" reserve="<<after.reserve<<" rack="<<candidate.needsRack<<std::endl;
                    state=candidate;ammo=after;tool=ReloadTool::None;consumed=frame.grip[h].down;Feedback(ReloadCue::Insert,offhand,.5f);acted=true;
                }
            }
            if(released && tool!=ReloadTool::None){fallingSpent=tool!=ReloadTool::Rocket;fallingPose=toolPose;fallingKind=ammo.kind;fallingTime=.6f;fallingVelocity={};CancelTool();Feedback(ReloadCue::Drop,offhand,.15f);}
        }
        return acted;
    }
    if(consumed){frame.consumed[h]=true;return acted;}
    // Compare the visible palm as well as the controller pivot. A Quest
    // controller's origin sits behind the fingers and can miss a small slide
    // sphere even when the visible hand is directly over the slide.
    const float actionRadius=ammo.kind==ReloadWeaponKind::Pistol && !state.needsRack?
        (std::max)(.10f,(std::min)(radius,.12f)):
        ammo.kind==ReloadWeaponKind::Shotgun?(std::max)(radius,.18f):radius;
    Matrix4 inverseBasis=socketBasis;inverseBasis.invertAffine();
    auto slideContact=[&](Point p){return NearPistolSlide(P(inverseBasis*V(p-sockets.action)),actionRadius);};
    const Point palmLocal=P(inverse*(hands[h]*V(palmFromController)));
    const bool atAction=ammo.kind==ReloadWeaponKind::Pistol?
        (slideContact(local) || (palmValid && slideContact(palmLocal))):Within(local,sockets.action,actionRadius);
    if(ammo.kind==ReloadWeaponKind::Pistol && frame.grip[h].pressed && Within(local,sockets.action,.35f))
        Logger::log<<"[ManualReload] pistol grip route="<<(atAction?"slide":frame.nearSupport[h]?"support":"outside")
            <<" controller=("<<local.x<<","<<local.y<<","<<local.z<<") palmValid="<<palmValid
            <<" palm=("<<palmLocal.x<<","<<palmLocal.y<<","<<palmLocal.z<<") slide=("
            <<sockets.action.x<<","<<sockets.action.y<<","<<sockets.action.z<<") radius="<<actionRadius<<std::endl;
    const bool supportedAtAction=frame.grip[h].down && atAction && session.Items().GetHand(offhand).hold==Hold::SupportGrip;
    const bool actionAllowed=state.needsRack || ammo.kind==ReloadWeaponKind::Pistol;
    // Pumping is available only when a shot/loading from empty requires it.
    // A loaded, ready foregrip remains support even if the hand slides back.
    const bool pumpHeld=ammo.kind==ReloadWeaponKind::Shotgun && supportedAtAction && state.needsRack;
    const bool slideHeld=MagazineReload(ammo.kind) && actionAllowed && supportedAtAction;
    // Moving a held support hand up to the slide intentionally transfers the
    // grip; it must not require a second press while the first is still held.
    if(!frame.grip[h].pressed && !pumpHeld && !slideHeld)return acted;
    if(!session.Items().Empty(offhand) && !pumpHeld && !slideHeld)return acted;
    const bool atPort=atInsertion();
    const bool shotgunSupportStart=ammo.kind==ReloadWeaponKind::Shotgun && !state.needsRack && frame.nearSupport[h] && !pumpHeld;
    const bool rocketPort=ammo.kind==ReloadWeaponKind::Rocket && state.open && state.spent>0 && atPort;
    if(rocketPort)tool=ReloadTool::SpentRocket;
    else if(ammo.kind!=ReloadWeaponKind::Rocket && actionAllowed && atAction && !shotgunSupportStart){
        tool=ReloadTool::Action;stroke={local,false,0};
        pumpSupported=ammo.kind==ReloadWeaponKind::Shotgun && pumpHeld;
        Logger::log<<"[ManualReload] action grab weapon="<<held<<" node="<<actionNode
            <<" hand="<<(offhand==Hand::Left?"L":"R")<<" native="<<socketValid[1]<<std::endl;
    }else if(atSupply && ammo.reserve>0 && ammo.loaded<ammo.capacity &&
        (!MagazineReload(ammo.kind) || !state.magazine) && (ammo.kind!=ReloadWeaponKind::Rocket || (state.open && ammo.loaded+state.spent<ammo.capacity))){
        tool=ammo.kind==ReloadWeaponKind::Shotgun ? ReloadTool::Shell : ammo.kind==ReloadWeaponKind::Rocket ? ReloadTool::Rocket : ReloadTool::Magazine;
        toolStart=position;
    }
    if(tool!=ReloadTool::None){if(!pumpSupported)session.ReleaseSupportForInteraction(offhand);frame.consumed[h]=true;consumed=true;
        Feedback(tool==ReloadTool::Action?ReloadCue::ActionGrab:tool==ReloadTool::SpentRocket?ReloadCue::Eject:ReloadCue::Grab,offhand);acted=true;
    }
    return acted;
}

void ReloadHandler::CacheModel(Object weapon,Object tag){
    if(modelWeapon==weapon && modelTag==tag)return;
    modelWeapon=weapon;modelTag=tag;nodeCount=0;palmValid=rocketPalmValid=false;socketBasis=Matrix4();
    shellNode=portNode=-1;supportValid=false;feedback={};fallingTime=0;fallingVelocity={};
    Object rocketModel=None;rocketModelAvailable=engine->ReadReloadRocketModel(weapon,rocketModel);
    if(records[weapon].kind==ReloadWeaponKind::Rocket)
        Logger::log<<"[ManualReload] rocket preview model="<<rocketModel<<" native="<<rocketModelAvailable<<std::endl;
    audioReady=InitializeReloadAudio();engine->ReadReloadFeedback(weapon,feedback);soundBank.Prime(feedback);
    Logger::log<<"[ManualReloadAudio] native="<<audioReady<<" pump sounds="<<feedback.pumpSoundCount
        <<" chamber effect="<<feedback.chamberEffect<<std::endl;
magNode=actionNode=gunNode=-1;socketValid[0]=socketValid[1]=false;
    handNodeCount=0;if(!engine->ReadReloadHandModel(weapon,handNodes,handNodeCount))handNodeCount=0;
    for(unsigned i=0;i<handNodeCount;++i){char name[33]{};std::memcpy(name,handNodes[i].name,32);
        Logger::log<<"[ManualReload] hand node "<<i<<" "<<name<<std::endl;}
    if(!engine->ReadReloadModel(weapon,nodes,nodeCount))return;
    const auto kind=records[weapon].kind;
    int bestAction=0;
    for(unsigned i=0;i<nodeCount;++i){
        char name[33]{};std::memcpy(name,nodes[i].name,32);for(char& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if(MagazineReload(kind) && (std::strstr(name,"magazine") || !std::strcmp(name,"mag") || !std::strcmp(name,"clip")))magNode=static_cast<int>(i);
        if(kind==ReloadWeaponKind::Shotgun && (std::strstr(name,"bullet") || std::strstr(name,"shell")))shellNode=static_cast<int>(i);
        if(kind==ReloadWeaponKind::Shotgun && std::strstr(name,"magazine"))portNode=static_cast<int>(i);
        const bool pump=kind==ReloadWeaponKind::Shotgun && (std::strstr(name,"pump") || std::strstr(name,"foregrip"));
        const bool cover=kind==ReloadWeaponKind::Rocket && (std::strstr(name,"cover") || std::strstr(name,"hatch") || !std::strcmp(name,"frame tubes"));
        const char* part=std::strncmp(name,"frame ",6)==0?name+6:name;
        const bool control=std::strstr(part,"release") || std::strstr(part,"button") || std::strstr(part,"trigger");
        const bool exact=!std::strcmp(part,"slide") || !std::strcmp(part,"ophandle") || !std::strcmp(part,"bolt") ||
            !std::strcmp(part,"charging handle") || !std::strcmp(part,"charging_handle") || !std::strcmp(part,"bolt_handle");
        const int score=control ? 0 : pump || cover ? 100 : MagazineReload(kind) ?
            (exact ? 100 : (std::strstr(part,"bolt") || std::strstr(part,"slide") || std::strstr(part,"charging")) ? 50 : 0) : 0;
        if(score>bestAction){bestAction=score;actionNode=static_cast<int>(i);}
        Logger::log<<"[ManualReload] node "<<weapon<<" "<<i<<" "<<name<<std::endl;
    }
    Logger::log<<"[ManualReload] parts magazine="<<magNode<<" action="<<actionNode
        <<" shell="<<shellNode<<" port="<<portNode<<" hand nodes="<<handNodeCount<<std::endl;
}

void ReloadHandler::Pose(Object weapon,const Bone* bones,Transform* out,unsigned count,int gun,int wrist){
    if(!active || !Game::instance.c_ManualReload->Value() || weapon!=held || weapon!=modelWeapon || !bones || !out || count>64 || gun<0 || gun>=static_cast<int>(count))return;
    const auto found=records.find(weapon);if(found==records.end())return;
    const auto& state=found->second;auto& game=Game::instance;
    const float scale=game.MetresToWorld(1);
    // Node names map the model's bind pose to the animation skeleton, never
    // copying bone indices or poses from another weapon.
    auto animationIndex=[&](int modelIndex){
        if(modelIndex<0 || gunNode<0)return -1;
        for(unsigned i=0;i<count;++i)if(std::strncmp(bones[i].BoneName,nodes[modelIndex].name,32)==0 && Descendant(bones,count,static_cast<int>(i),gun))return static_cast<int>(i);
        return -1;
    };
    if(gunNode<0){for(unsigned i=0;i<nodeCount;++i)if(std::strncmp(bones[gun].BoneName,nodes[i].name,32)==0)gunNode=static_cast<int>(i);}
    Matrix4 nativeGun=ModelMatrix(out[gun]);
    const auto camera=Helpers::GetCamera().position;
    const Matrix4 rootInverse=gunNode>=0?ModelMatrix(nodes[gunNode].inverseBind):Matrix4();
    auto worldToController=[&](Matrix4 world){
        const auto pos=world*Vector3();world.translate(-pos);world.translate((pos-camera)*game.WorldToMetres(1));
        Matrix4 inverse=weaponPose;inverse.invertAffine();return inverse*world;
    };
    auto toWorld=[&](Matrix4 vr){const auto p=vr*Vector3();vr.translate(-p);vr.translate(camera+p*scale);return vr;};
    socketBasis=GripRotation(worldToController(nativeGun*rootInverse));
    if(state.kind==ReloadWeaponKind::Pistol && wrist>=0 && wrist<static_cast<int>(count) && !game.bUseTwoHandAim){
        // Called before compact-support or reload skin snapping. Store a
        // controller-relative point, never the previous frame's world point.
        Matrix4 palm;
        if(LeftPalmFrame(bones,count,out,wrist,palm)){
            const auto point=(palm*Vector3()-camera)*game.WorldToMetres(1);
            Matrix4 offInverse=offhandPose;offInverse.invertAffine();
            const Point offset=P(offInverse*point);
            if(offset.Finite() && offset.LengthSquared()<.0625f){palmFromController=offset;palmValid=true;}
        }
    }
    if(state.kind==ReloadWeaponKind::Rocket && wrist>=0 && wrist<static_cast<int>(count)){
        std::array<Transform,64> bind{};
        for(unsigned i=0;i<count;++i)bind[i]=out[i];
        HandBindPose(bones,count,handNodes.data(),handNodeCount,bind.data());
        Matrix4 palm;const Matrix4 bindWrist=ModelMatrix(bind[wrist]);
        if(std::fabs(bindWrist.getDeterminant())>.00001f && LeftPalmFrame(bones,count,bind.data(),wrist,palm)){
            palm=ModelMatrix(out[wrist])*Matrix4(bindWrist).invertAffine()*palm;
            const auto world=palm*Vector3();palm.translate(-world);palm.translate((world-camera)*game.WorldToMetres(1));
            rocketPalmRelative=Matrix4(offhandPose).invertAffine()*palm;rocketPalmValid=true;
            if(tool==ReloadTool::Rocket || tool==ReloadTool::SpentRocket)toolPose=RocketPalmGrasp(palm,1.f);
        }
    }
    const float strokeLength=Bounded(game.c_ReloadStroke->Value(),.09f,.04f,.20f);
    const float travel=state.kind==ReloadWeaponKind::Rocket ? (state.open?strokeLength:0.f) :
        (tool==ReloadTool::Action?stroke.travel:0.f);
    // Model rails follow the actual gun, including its aim correction and left
    // hand mirroring. Controller X is not necessarily the model's slide axis.
    const float visualTravel=travel*(state.kind==ReloadWeaponKind::Pistol?.04f/strokeLength:
        state.kind==ReloadWeaponKind::Rifle?.065f/strokeLength:state.kind==ReloadWeaponKind::Shotgun?1.f/.60f:1.f);
    Vector3 rail=(nativeGun*rootInverse).getLeftAxis();
    const float railLength=rail.length();if(railLength>.00001f)rail*=1.f/railLength;
    const Vector3 actionShift=rail*(-visualTravel*scale);
    Vector3 latchPosition=camera+(weaponPose*V(sockets.action))*scale+actionShift;
    const int magazine=animationIndex(magNode),action=animationIndex(actionNode);
    if(state.kind==ReloadWeaponKind::Sniper && tool==ReloadTool::Magazine && wrist>=0 && wrist<static_cast<int>(count) &&
        wrist!=gun && !Descendant(bones,count,wrist,gun)){
        // Palm-up relative to the tracked offhand, preserving the reflected
        // Chief skeleton when the weapon is in the left hand.
        const Matrix4 current=ModelMatrix(out[wrist]);
        const float mirror=current.getDeterminant()<0?-1.f:1.f;
        Matrix4 desired=GripRotation(offhandPose)*GripAxes({1,0,0},{0,-mirror,0},{0,0,-1});
        Matrix4 palm;Vector3 center=current*Vector3();
        if(LeftPalmFrame(bones,count,out,wrist,palm))center=palm*Vector3();
        desired.translate(center);
        Matrix4 fallback=desired;fallback.translate(current*Vector3()-center);
        PlacePalmGrip(bones,count,out,wrist,gun,handNodes.data(),handNodeCount,desired,fallback);
    }
    if(magazine>=0){
        const Matrix4 rest=nativeGun*rootInverse*ModelMatrix(nodes[magNode].inverseBind).invertAffine();
        const auto local=worldToController(rest);const auto p=local*Vector3();
        if(p.length()<1.2f && std::isfinite(p.x)){magRelative=local;socketValid[0]=true;}
        if(!state.magazine){
            if(tool==ReloadTool::Magazine){
                Matrix4 target=toWorld(toolPose);
                if(MagazineReload(state.kind) && wrist>=0 && wrist<static_cast<int>(count)){
                    std::array<Transform,64> bind{};
                    for(unsigned i=0;i<count;++i)bind[i]=out[i];
                    HandBindPose(bones,count,handNodes.data(),handNodeCount,bind.data());
                    Matrix4 palm;const Matrix4 bindWrist=ModelMatrix(bind[wrist]);
                    if(std::fabs(bindWrist.getDeterminant())>.00001f && LeftPalmFrame(bones,count,bind.data(),wrist,palm)){
                        palm=ModelMatrix(out[wrist])*Matrix4(bindWrist).invertAffine()*palm;
                        // Thumb edge up, magazine inside the curled fingers,
                        // clear of the palm skin; grasp the lower magazine body.
                        const Matrix4 orientation=GripAxes({1,0,0},{0,0,1},{0,-1,0});
                        // Turn the sniper magazine through 90 degrees about
                        // the finger axis: feed end above the palm, rounds up.
                        target=palm*(state.kind==ReloadWeaponKind::Sniper?Matrix4().rotateX(90.f):Matrix4())*orientation;
                        const bool rifle=state.kind==ReloadWeaponKind::Rifle || state.kind==ReloadWeaponKind::Sniper;
                        if(rifle)target=target*GripRotation(rootInverse*ModelMatrix(nodes[magNode].inverseBind).invertAffine());
                        // Centre the AR magazine at the palm instead of the
                        // fingertips; retain its side-to-side and skin clearance.
                        const Vector3 pocket=state.kind==ReloadWeaponKind::Sniper?Vector3(0,0,-.025f):
                            rifle?Vector3(0,0,-.060f):Vector3(.012f,-.040f,-.022f);
                        const Vector3 origin=palm*(pocket*scale);
                        target.translate(origin-target*Vector3());
                    }
                }
                MovePart(bones,count,out,magazine,target);
            }
            else if(fallingTime>0 && MagazineReload(fallingKind))MovePart(bones,count,out,magazine,toWorld(fallingPose));
            else MovePart(bones,count,out,magazine,rest,true);
        }
    }
    const int shell=animationIndex(shellNode),port=animationIndex(portNode);
    if(state.kind==ReloadWeaponKind::Shotgun){
        const int portModel=port>=0?portNode:shellNode;
        if(portModel>=0){
            Matrix4 rest=nativeGun*rootInverse*ModelMatrix(nodes[portModel].inverseBind).invertAffine();
            Vector3 up=(nativeGun*rootInverse).getForwardAxis();const float length=up.length();
            if(length>.00001f)rest.translate(up*(.065f*scale/length));
            const Matrix4 local=worldToController(rest);const auto p=local*Vector3();
            if(p.length()<1.2f && std::isfinite(p.x)){magRelative=local;socketValid[0]=true;}
        }
        if(shell>=0){
            // The actual textured first-person shell, not a red proxy cylinder.
            // Explicitly restore the node from bind pose: animation may scale it
            // to zero between reloads, where MovePart cannot invert its matrix.
            const Matrix4 rest=nativeGun*rootInverse*ModelMatrix(nodes[shellNode].inverseBind).invertAffine();
            for(unsigned i=0;i<count;++i)if(Descendant(bones,count,static_cast<int>(i),shell)){
                const int n=[&](){for(unsigned j=0;j<nodeCount;++j)if(std::strncmp(bones[i].BoneName,nodes[j].name,32)==0)return static_cast<int>(j);return -1;}();
                if(n>=0)out[i]=ModelTransform(nativeGun*rootInverse*ModelMatrix(nodes[n].inverseBind).invertAffine());
            }
            if(tool==ReloadTool::Shell)MovePart(bones,count,out,shell,toWorld(toolPose));
            else if(fallingTime>0 && fallingKind==ReloadWeaponKind::Shotgun)MovePart(bones,count,out,shell,toWorld(fallingPose));
            else MovePart(bones,count,out,shell,rest,true);
        }
        if(wrist>=0 && wrist<static_cast<int>(count) && game.bUseTwoHandAim && tool==ReloadTool::None && !state.needsRack &&
            std::fabs(nativeGun.getDeterminant())>.00001f){
            supportRelative=Matrix4(nativeGun).invertAffine()*ModelMatrix(out[wrist]);supportValid=true;
        }
    }
    if(action>=0){
        Matrix4 rest=nativeGun*rootInverse*ModelMatrix(nodes[actionNode].inverseBind).invertAffine();
        const auto local=worldToController(rest);const auto p=local*Vector3();
        if(p.length()<1.2f && std::isfinite(p.x)){actionRelative=local;socketValid[1]=true;}
        rest.translate(actionShift);latchPosition=rest*Vector3();
        // Manual pumps/covers and empty actions cannot cycle themselves. Let
        // ordinary pistol/rifle firing animate the action while still loaded.
        if(state.kind==ReloadWeaponKind::Shotgun || state.kind==ReloadWeaponKind::Rocket || state.needsRack || tool==ReloadTool::Action)
            MovePart(bones,count,out,action,rest);
    }
    if(tool!=ReloadTool::None && wrist>=0 && wrist<static_cast<int>(count) && wrist!=gun &&
        !Descendant(bones,count,wrist,gun)){
        if(tool==ReloadTool::Action){
            if(state.kind==ReloadWeaponKind::Shotgun && supportValid){
                Matrix4 targetWrist=nativeGun*supportRelative;targetWrist.translate(actionShift);
                const Matrix4 current=ModelMatrix(out[wrist]);
                if(std::fabs(current.getDeterminant())>.00001f)
                    MoveHand(bones,count,out,wrist,gun,targetWrist*Matrix4(current).invertAffine());
            }else{
                // Fingers cross the action, back of hand faces up. Derive
                // the actual wrist rotation from Chief's knuckle plane.
                const Matrix4 basis=GripRotation(nativeGun*rootInverse);
                const bool assault=state.kind==ReloadWeaponKind::Rifle;
                // AR and sniper fingers point forward along the gun, palm
                // down. Their separate lateral handle offsets are preserved.
                const bool rifle=assault || state.kind==ReloadWeaponKind::Sniper;
                Matrix4 desired=basis*(rifle?GripAxes({1,0,0},{0,1,0},{0,0,1}):GripAxes({0,-1,0},{1,0,0},{0,0,1}));
                const Vector3 offset=rifle?Vector3(0,assault?.095f:.035f,.025f):Vector3(-.012f,0,.026f);
                desired.translate(latchPosition+(basis*offset)*scale);
                Matrix4 fallback=basis*Matrix4().rotateX(90.f).rotateZ(rifle?0.f:-90.f);
                const Vector3 wristOffset=rifle?Vector3(-.045f,0,0):Vector3(0,.045f,0);
                fallback.translate(latchPosition+(basis*(offset+wristOffset))*scale);
                PlacePalmGrip(bones,count,out,wrist,gun,handNodes.data(),handNodeCount,desired,fallback);
            }
        }
        const bool thumbShell=state.kind==ReloadWeaponKind::Shotgun && tool==ReloadTool::Shell;
        CloseReloadHand(bones,count,out,wrist,gun,handNodes.data(),handNodeCount,thumbShell);
        if(thumbShell && shell>=0){
            Matrix4 thumb;
            if(ThumbPadFrame(bones,count,out,wrist,gun,.012f*scale,thumb)){
                const Matrix4 orientation=GripRotation(rootInverse*ModelMatrix(nodes[shellNode].inverseBind).invertAffine());
                MovePart(bones,count,out,shell,thumb*orientation);
            }
        }
    }
}

void ReloadHandler::DrawNativePieces(){
    auto& game=Game::instance;auto* vr=game.GetVR();
    if(!active || !engine || !rocketModelAvailable || !game.c_ManualReload->Value() ||
        !vr || !vr->IsHMDTracked() || !game.PhysicalVisualsActive() || Helpers::IsMouseVisible() || Helpers::IsCampaignLoading())return;
    const auto* scene=Helpers::GetCutsceneData();if(scene && scene->bInCutscene)return;
    const auto origin=Helpers::GetCamera().position;
    auto nativePose=[&](Matrix4 pose){
        const auto point=pose*Vector3();pose.translate(-point);
        // Native model dimensions are already in world units, unlike our
        // procedural fallback. Convert its tracked origin, not its geometry.
        pose.translate(origin+point*game.MetresToWorld(1));return pose;
    };
    if(tool==ReloadTool::Rocket || tool==ReloadTool::SpentRocket)
        engine->DrawReloadRocket(held,nativePose(toolPose),tool==ReloadTool::SpentRocket);
    if(fallingTime>0 && fallingKind==ReloadWeaponKind::Rocket)
        engine->DrawReloadRocket(held,nativePose(fallingPose),fallingSpent);
}

void ReloadHandler::Draw(){
    auto& game=Game::instance;auto* vr=game.GetVR();
    if(!active || !game.c_ManualReload->Value() || !vr || !vr->IsHMDTracked() ||
        !game.PhysicalVisualsActive() || Helpers::IsMouseVisible() || Helpers::IsCampaignLoading())return;
    const auto* scene=Helpers::GetCutsceneData();
    if(scene && scene->bInCutscene)return;
    const float scale=game.MetresToWorld(1),dt=Bounded(game.lastDeltaTime,0,.0f,.05f);
    const auto origin=Helpers::GetCamera().position;const auto center=origin+V(supply)*scale;
    const Vector3 forward(std::cos(bodyYaw),std::sin(bodyYaw),0),right(forward.y,-forward.x,0);
    DrawOutlineZone(game.inGameRenderer,center,right,forward,.09f*scale,.003f*scale,
        D3DCOLOR_ARGB(255,85,185,205));
    if(game.uiRenderer && game.uiRenderer->GetSmallFont())game.inGameRenderer.DrawLabel3D(*game.uiRenderer->GetSmallFont(),"Magazine Zone",center,right,forward,.012f*scale,0,true);
    auto toWorld=[&](Matrix4 pose){const auto p=pose*Vector3();pose.translate(-p);pose.scale(scale);pose.translate(origin+p*scale);return pose;};
    const bool nativeMagazine=magNode>=0 && socketValid[0];
    if((tool==ReloadTool::Shell && shellNode<0) || ((tool==ReloadTool::Rocket || tool==ReloadTool::SpentRocket) && !rocketModelAvailable) || (tool==ReloadTool::Magazine && !nativeMagazine))
        DrawReloadPiece(toWorld(toolPose),records[held].kind,tool==ReloadTool::SpentRocket);
    if(fallingTime>0){
        fallingTime=(std::max)(0.0f,fallingTime-dt);const float t=.75f-fallingTime;
        fallingPose.translate(fallingVelocity*dt+Vector3(0,0,-(1.0f+9.81f*t)*dt));
        if((!nativeMagazine || !MagazineReload(fallingKind)) && !(fallingKind==ReloadWeaponKind::Shotgun && shellNode>=0) && !(fallingKind==ReloadWeaponKind::Rocket && rocketModelAvailable))DrawReloadPiece(toWorld(fallingPose),fallingKind,fallingKind==ReloadWeaponKind::Rocket?fallingSpent:true);
    }
    const auto found=records.find(held);
    if(found!=records.end() && found->second.open){
        const auto pos=origin+(weaponPose*V(sockets.insert))*scale;
        game.inGameRenderer.DrawPolygon(pos,weaponPose.getForwardAxis(),weaponPose.getUpAxis(),12,.06f*scale,D3DCOLOR_ARGB(100,100,220,150),false);
    }
}
