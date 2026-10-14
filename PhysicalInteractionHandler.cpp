#include "PhysicalInteractionHandler.h"
#include "Game.h"
#include "Immersion/HandGripPose.h"
#include "Helpers/Assets.h"
#include "Helpers/Controls.h"
#include "Helpers/Menus.h"
#include "Helpers/Cutscene.h"
#include "Logger.h"
using namespace Immersion;
namespace {
Point P(Vector3 p){return {p.x,p.y,p.z};}
Vector3 V(Point p){return {p.x,p.y,p.z};}
ControllerRole Role(unsigned h){return h?ControllerRole::Right:ControllerRole::Left;}
float Bound(float v,float fallback,float low,float high){return (std::max)(low,(std::min)(high,std::isfinite(v)?v:fallback));}
bool Usable(bool scriptedSeat=false){auto& g=Game::instance;auto* v=g.GetVR();const auto* scene=Helpers::GetCutsceneData();
    return v && !g.bUse3DOFAiming && !g.bNeedsRecentre && v->IsHMDTracked() && !Helpers::IsMouseVisible() &&
        !Helpers::IsCampaignLoading() && (scriptedSeat || !(scene && scene->bInCutscene));}
void Buzz(unsigned h,float strength=.3f){Game::instance.GetVR()->TriggerHapticVibration(Role(h),0,.04f,110,strength);}
}
void PhysicalInteractionHandler::SuspendGrenades(){
    // Previews never consume inventory. Losing tracking/menu returns previews,
    // and cancels unissued requests; an already-started native throw is allowed
    // to finish natively, without teleporting it using stale hand tracking.
    grenades.Reset();throwCooldown={};for(auto& history:velocityHistory)history.Reset();grenadeActive=false;thrown=None;grenadeHook=false;
}
void PhysicalInteractionHandler::Grenades(RetailWeaponEngine& native,const WeaponSession& session,
    WeaponFrame& frame,const Matrix4*,Point head,float yaw){
    auto& g=Game::instance;engine=&native;
    if(!g.c_PhysicalGrenades->Value() || !Usable()){SuspendGrenades();return;}
    const bool readable=engine->ReadGrenades(inventory);
    const char* status=engine->GrenadeReadStatus();
    if(status!=lastGrenadeStatus){Logger::log<<"[PhysicalGrenades] "<<status<<std::endl;lastGrenadeStatus=status;}
    if(!readable){SuspendGrenades();return;}
    grenadeActive=true;chestYaw=yaw;grenades.Observe(inventory);
    const bool wasPending=grenades.pending;grenades.Tick(g.lastDeltaTime);
    if(wasPending && !grenades.pending)Logger::log<<"[PhysicalGrenades] native throw timed out; no inventory write"<<std::endl;
    for(auto& cooldown:throwCooldown)cooldown=(std::max)(0.f,cooldown-Bound(g.lastDeltaTime,0,0,.1f));
    if(grenades.pending)throwCooldown[grenades.release.hand]=.35f;
    // Explicit physical sides, independent of the saved weapon-handedness layout.
    chest[0]=Layout{false}.Anchor(Zone::FragChest,head,yaw);
    chest[1]=Layout{false}.Anchor(Zone::PlasmaChest,head,yaw);
    const float gripLift=Bound(g.c_GrenadeGripHeightOffset->Value(),.06f,0.f,.15f);
    for(unsigned type=0;type<2;++type)chestGrip[type]=chest[type]+Point{0,0,gripLift};
    for(unsigned h=0;h<2;++h){
        const auto controller=P(g.GetVR()->GetRawControllerTransform(Role(h))*Vector3());
        if(grenades.held[h]>=0){
            velocityHistory[h].Add(frame.release[h].velocity,g.lastDeltaTime);
            frame.consumed[h]=true;frame.nearSupport[h]=frame.nearTrigger[h]=false;
            if(frame.grip[h].released){
                const unsigned type=grenades.held[h];
                const bool returned=Within(controller,chestGrip[type],.17f);
                auto release=frame.release[h];release.velocity=velocityHistory[h].Release(release.velocity);
                const bool queued=grenades.Release(h,returned,release);velocityHistory[h].Reset();
                if(queued)throwCooldown[h]=.35f;
                Buzz(h,returned?.2f:.4f);
                Logger::log<<"[PhysicalGrenades] "<<(queued?"throw queued":returned?"returned":"native throw busy; returned")
                    <<" hand="<<h<<" type="<<type<<std::endl;
            }
        }else if(frame.grip[h].pressed && !frame.consumed[h] && session.Items().Empty(static_cast<Hand>(h)) && !g.ReloadHandBusy(static_cast<Hand>(h))){
            // Nearby chest/hip capture spheres can overlap. Prefer the closest
            // visible occupied weapon or magazine zone, not blanket chest priority.
            float otherDistance=1.e10f;
            const Layout weaponLayout{g.c_LeftHanded->Value()};
            if(frame.holster[h]>=0 && session.Items().SlotOwner(static_cast<unsigned>(frame.holster[h]))!=None){
                const auto zone=weaponLayout.Anchor(frame.holster[h]==0?Zone::Shoulder:Zone::WeaponHip,head,yaw);
                otherDistance=(controller-zone).LengthSquared();
            }
            if(session.Held()!=None && g.c_ManualReload->Value())
                otherDistance=(std::min)(otherDistance,(controller-weaponLayout.Anchor(Zone::SupplyHip,head,yaw)).LengthSquared());
            int nearest=-1;float nearestDistance=otherDistance;
            for(unsigned type=0;type<2;++type){
                const float distance=(controller-chestGrip[type]).LengthSquared();
                if(inventory.count[type]>grenades.Reserved(type) && GrenadeGripContains(controller,chestGrip[type],yaw,h) && distance<nearestDistance){
                    nearest=static_cast<int>(type);nearestDistance=distance;
                }
            }
            if(nearest>=0 && grenades.Grab(h,static_cast<unsigned>(nearest),inventory)){
                const unsigned type=static_cast<unsigned>(nearest);
                velocityHistory[h].Reset();
                if(!grenades.pending)engine->SelectGrenade(inventory.unit,type);
                frame.consumed[h]=true;frame.nearSupport[h]=frame.nearTrigger[h]=false;Buzz(h);
                Logger::log<<"[PhysicalGrenades] grabbed hand="<<h<<" type="<<type<<std::endl;
            }
        }
    }
}
bool PhysicalInteractionHandler::GrenadeControls(){
    if(!grenadeActive || !engine)return false;
    auto& controls=Helpers::GetControls();controls.Grenade=0;
    if(grenades.pending){
        controls.SwitchGrenades=0;
        GrenadeInventory now;
        if(!engine->ReadGrenades(now) || now.unit!=grenades.release.unit){grenades.Finish();return true;}
        if(!grenades.issued){
            if(engine->SelectGrenade(now.unit,grenades.release.type)){
                grenades.issued=true;grenades.countAtIssue=now.count[grenades.release.type];controls.Grenade=127;
                Logger::log<<"[PhysicalGrenades] native throw input type="<<grenades.release.type<<" count="<<grenades.countAtIssue<<std::endl;
            }
        }else{
            if(now.state==1 || now.state==2 || now.count[grenades.release.type]<grenades.countAtIssue)grenades.acknowledged=true;
            // Keep the press across render/input polling until acknowledged.
            // Never re-press during state 3 if the release callback was missed.
            if(!grenades.acknowledged)controls.Grenade=127;
        }
    }
    return true;
}
bool PhysicalInteractionHandler::BeforeThrow(Object unit){
    grenadeHook=false;thrown=None;
    if(!engine || !grenadeActive || !grenades.pending || !grenades.issued || grenades.release.unit!=unit)return false;
    thrown=engine->AttachedGrenade(unit);grenadeHook=thrown!=None;
    return grenadeHook; // Suppress the legacy weapon-hand relocation only for our throw.
}
void PhysicalInteractionHandler::AfterThrow(Object unit){
    if(!grenadeHook || !engine || !grenades.pending || grenades.release.unit!=unit)return;
    auto& g=Game::instance;
    const float handSpeed=g.WorldToMetres(std::sqrt(grenades.release.pose.velocity.LengthSquared()));
    const float strength=Bound(g.c_GrenadeThrowStrength->Value(),1.3f,.25f,3.f);
    const bool moved=engine->FinishGrenade(thrown,grenades.release,
        strength,g.MetresToWorld(GrenadeMaximumMetresPerSecond));
    Logger::log<<"[PhysicalGrenades] native release hand="<<grenades.release.hand<<" projectile="<<thrown
        <<" tracked velocity applied="<<moved<<" hand_mps="<<handSpeed
        <<" launch_mps="<<(std::min)(GrenadeMaximumMetresPerSecond,handSpeed*strength*GrenadeLaunchBoost)<<std::endl;
    throwCooldown[grenades.release.hand]=.35f;
    grenades.Finish();grenadeHook=false;thrown=None;
}
void PhysicalInteractionHandler::RefreshVehicle(){
    VehicleState next;
    const bool readable=engine && Game::instance.c_FirstPersonVehicles->Value() && engine->ReadVehicle(next);
    if(engine && engine->VehicleReadStatus()!=lastVehicleStatus){
        lastVehicleStatus=engine->VehicleReadStatus();Logger::log<<"[PhysicalVehicles] "<<lastVehicleStatus<<std::endl;
    }
    if(!readable){
        if(haveSeat && Usable(true)){
            exitBlend=true;exitBlendAge=0;exitBlendStart=lastViewOrigin;
            Logger::log<<"[PhysicalVehicles] exit camera handoff started"<<std::endl;
        }
        exitRequested=false;exitInteractDown=false;
        vehicle={};seatMotion.Reset();seatYawMotion.Reset();seatAnchor.Reset();haveSeat=false;vehicleInputsValid=false;reticleReady=false;
        return;
    }
    const bool changed=next.vehicle!=vehicle.vehicle || next.unit!=vehicle.unit || next.seat!=vehicle.seat;
    vehicle=next;
    if(changed){
        exitRequested=exitBlend=false;exitInteractDown=Helpers::GetControls().Interact!=0;
        haveSeat=false;reticleReady=false;seatMotion.Reset();seatYawMotion.Reset();seatAnchor.Reset();
        passengerYaw=passengerPitch=passengerSeatYaw=0;
        Logger::log<<"[PhysicalVehicles] seat="<<vehicle.seat<<" kind="<<static_cast<int>(vehicle.kind)
            <<" native handle anchors="<<vehicle.handleValid[0]<<","<<vehicle.handleValid[1]<<std::endl;
    }
}
void PhysicalInteractionHandler::UpdateVehicle(RetailWeaponEngine& native,bool inVehicle,const bool* triggers,bool valid,Vector2 move){
    engine=&native;RefreshVehicle();
    if(!VehicleActive()){
        if(inVehicle && native.IsScorpionDriver()){
            auto& c=Helpers::GetControls();const bool usable=valid && Usable();
            c.Fire=usable && triggers[1]?127:0;
            c.Grenade=usable && triggers[0]?127:0; // Native secondary weapon, formerly A.
            c.Jump=c.Zoom=c.SwitchGrenades=c.Reload=c.SwitchWeapons=c.Melee=0;
        }
        return;
    }
    const bool exitButton=Helpers::GetControls().Interact!=0;
    if(haveSeat && exitButton && !exitInteractDown && vehicle.kind!=VehicleSeat::PelicanPassenger){
        exitRequested=true;exitRequestAge=0;
        exitSeatLocal=Matrix4(VehicleBasis(vehicle)).invertAffine()*seatEye;
    }
    exitInteractDown=exitButton;
    vehicleInputsValid=valid && Usable();
    auto& c=Helpers::GetControls();c.Grenade=c.SwitchGrenades=c.SwitchWeapons=c.Reload=0;
    if(vehicle.kind==VehicleSeat::Ghost){
        c.Forwards=vehicleInputsValid?Bound(move.y,0,-1,1):0.f;
        c.Left=vehicleInputsValid?-Bound(move.x,0,-1,1):0.f;
        c.Fire=vehicleInputsValid && (triggers[0] || triggers[1])?127:0;
        c.Jump=c.Zoom=c.Melee=0;
    }else if(vehicle.kind==VehicleSeat::WarthogDriver){
        c.Forwards=vehicleInputsValid?float(triggers[1])-float(triggers[0]):0.f;c.Left=0;
        c.Fire=c.Jump=c.Zoom=c.Melee=0;
    }else if(vehicle.kind==VehicleSeat::PelicanPassenger){
        c.Forwards=c.Left=0;c.Fire=c.Jump=c.Zoom=0;
    }else {
        c.Fire=vehicleInputsValid && triggers[1]?127:0;
        c.Left=0;c.Melee=0;
        if(IsTurretSeat(vehicle.kind))c.Forwards=0;
    }
    if(!vehicleInputsValid){c.Forwards=c.Left=0;c.Fire=0;}
}
bool PhysicalInteractionHandler::VehicleCamera(float& yaw,float& pitch,Vector2 move,Vector2 look){
    if(!VehicleActive())return false;
    yaw=pitch=0;
    if(!vehicleInputsValid || !Usable() || vehicle.kind==VehicleSeat::PelicanPassenger || exitRequested)return true;
    const float dt=Bound(Game::instance.lastDeltaTime,1.f/90.f,0.f,.05f);
    if(IsTurretSeat(vehicle.kind) || vehicle.kind==VehicleSeat::Ghost){
        const auto* property=vehicle.kind==VehicleSeat::Ghost?Game::instance.c_GhostSensitivity:Game::instance.c_TurretSensitivity;
        const float sensitivity=Bound(property->Value(),vehicle.kind==VehicleSeat::Ghost?1.f:.55f,.1f,vehicle.kind==VehicleSeat::Ghost?2.5f:1.5f);
        yaw=-TurretStick(look.x)*1.8f*sensitivity*dt;
        pitch=TurretStick(look.y)*1.2f*sensitivity*dt;
    }else{
        // Steering comes ONLY from left-stick X. Head pose and right-stick
        // position cannot pull the wheel/vehicle away from this command.
        const float forward=std::atan2(vehicle.forward.y,vehicle.forward.x);
        const float aim=std::atan2(vehicle.aim.y,vehicle.aim.x);
        const float desired=forward-Bound(move.x,0,-1,1)*.95f;
        yaw=Bound(WrapRadians(desired-aim),0,-2.5f*dt,2.5f*dt);
        const float currentPitch=std::atan2(vehicle.aim.z,std::sqrt(vehicle.aim.x*vehicle.aim.x+vehicle.aim.y*vehicle.aim.y));
        pitch=Bound(-currentPitch,0,-1.5f*dt,1.5f*dt);
    }
    return true;
}
Vector3 PhysicalInteractionHandler::ViewOrigin()const{
    if(!haveSeat)return Helpers::GetCamera().position;
    Matrix4 rotate;rotate.rotateZ((vehicleYaw+passengerYaw)*57.2957795f);
    return seatEye-rotate*seatedHead*Game::instance.MetresToWorld(1);
}
void PhysicalInteractionHandler::BeginView(CameraFrustum& first,CameraFrustum& second){
    EndView();auto& g=Game::instance;auto* vr=g.GetVR();
    // Cinematics can stop native gameplay input entirely. Discover and refresh
    // seats here too, so all Pelican map intros use the same stable path.
    RefreshVehicle();
    if(!VehicleActive()){
        if(!exitBlend)return;
        if(!Usable(true)){exitBlend=false;return;}
        const Vector3 target=first.position;
        // Do not drag a view across checkpoint loads or teleports.
        if((target-exitBlendStart).length()>g.MetresToWorld(8.f)){exitBlend=false;passengerPitch=0;return;}
        const float dt=Bound(g.lastDeltaTime,0,0,.1f);
        exitBlendAge+=dt;
        const float alpha=1.f-std::exp(-dt/.18f);
        exitBlendStart+=(target-exitBlendStart)*alpha;
        const Vector3 origin=exitBlendStart;
        passengerPitch*=std::exp(-dt/.18f);
        // Keep smoothing through the full native dismount, rather than snap
        // back to an animated camera after a fixed 0.35-second position blend.
        if(exitBlendAge>=1.f && (target-origin).length()<g.MetresToWorld(.01f))exitBlend=false;
        if(exitBlendAge>=2.f){exitBlend=false;passengerPitch=0;}
        savedCamera=Helpers::GetCamera();savedFirstPosition=first.position;savedSecondPosition=second.position;
        savedFirstPlayer=first.drawPlayer;savedSecondPlayer=second.drawPlayer;firstView=&first;secondView=&second;viewSaved=true;
        Helpers::GetCamera().position=first.position=second.position=origin;
        first.drawPlayer=second.drawPlayer=false;
        return;
    }
    if(!Usable(vehicle.kind==VehicleSeat::PelicanPassenger) || !engine)return;
    const Vector3 rawAim=V(vehicle.aim).normalize();
    if(!reticleReady || vehicle.kind!=VehicleSeat::Ghost || reticleAim.dot(rawAim)<.5f){
        reticleAim=rawAim;reticleReady=true;
    }else{
        // Aim is sampled at simulation rate. Only the Ghost HUD direction is
        // filtered; native projectile aim, steering and the HMD remain direct.
        const float alpha=1.f-std::exp(-Bound(g.lastDeltaTime,1.f/90.f,0.f,.1f)/.06f);
        reticleAim=(reticleAim+(rawAim-reticleAim)*alpha).normalize();
    }
    const bool shade=vehicle.kind==VehicleSeat::ShadeTurret;
    const bool headSeat=IsTurretSeat(vehicle.kind);
    const bool renderPose=vehicle.renderedPoseValid || (headSeat && vehicle.renderedHeadValid);
    const Point facing=headSeat && vehicle.renderedHeadValid?vehicle.renderedHeadFacing:
        (vehicle.renderedPoseValid?vehicle.renderedFacing:(IsTurretSeat(vehicle.kind)?vehicle.aim:vehicle.forward));
    if(exitRequested){exitRequestAge+=Bound(g.lastDeltaTime,0,0,.1f);if(exitRequestAge>2.f)exitRequested=false;}
    const bool passenger=vehicle.kind==VehicleSeat::PelicanPassenger;
    const auto chassisFacing=VehicleBasis(vehicle).getLeftAxis();
    const float chassisYaw=std::atan2(chassisFacing.y,chassisFacing.x);
    if(passenger && !haveSeat)passengerSeatYaw=WrapRadians(std::atan2(facing.y,facing.x)-chassisYaw);
    const float nextYaw=exitRequested && haveSeat?vehicleYaw:passenger?WrapRadians(chassisYaw+passengerSeatYaw):std::atan2(facing.y,facing.x);
    if(!haveSeat){
        Logger::log<<"[PhysicalVehicles] cockpit visibility: native near/far="<<first.zNear<<","<<first.zFar
            <<" raster="<<second.zNear<<","<<second.zFar<<"; matched current-eye frusta / native visibility"<<std::endl;
        const auto head=vr->GetHMDTransform();const auto look=head.getLeftAxis();
        const float delta=WrapRadians(nextYaw-std::atan2(look.y,look.x));
        vr->SetYawOffset(vr->GetYawOffset()-delta*57.2957795f);
        Matrix4 inverse;inverse.rotateZ(-nextYaw*57.2957795f);
        seatedHead=inverse*(vr->GetHMDTransform()*Vector3());
        // Vehicle parent/seat is set before the climb-in animation ends. The
        // local anchor must settle, not freeze this first outside head sample.
        seatAnchor.Reset();seatMotion.Reset();seatYawMotion.Reset();vehicleYaw=nativeYaw=lastNativeYaw=nextYaw;haveSeat=true;
        seatYawMotion.Update({nextYaw,0,0},{},g.lastDeltaTime);
        Logger::log<<"[PhysicalVehicles] camera source="<<(headSeat?"rendered turret head / no frozen seat anchor":renderPose?"rendered chassis / no second interpolation":"simulation fallback")
            <<" seat="<<vehicle.seat<<std::endl;
    }else {
        // Interpolate wrapped native yaw on the same 30 Hz timeline as
        // translation. Latest headset rotation is still applied by OpenVR.
        nativeYaw+=WrapRadians(nextYaw-lastNativeYaw);lastNativeYaw=nextYaw;
        const float smoothYaw=renderPose?nativeYaw:seatYawMotion.Update({nativeYaw,0,0},{},g.lastDeltaTime).x;
        const float delta=WrapRadians(smoothYaw-vehicleYaw);
        vr->SetYawOffset(vr->GetYawOffset()-delta*57.2957795f);
        vehicleYaw=WrapRadians(smoothYaw);
    }
    if(passenger){
        // Render-time stick look stays available while a mission script locks
        // gameplay input. It changes only the view, never the flying Pelican.
        vr->UpdateInputs(); // Scripts may not call the native input hook during the ride.
        const auto look=g.VehicleLookInput();const float dt=Bound(g.lastDeltaTime,0,0,.05f);
        const float yawDelta=TurretStick(look.x)*90.f*dt;
        passengerYaw=WrapRadians(passengerYaw-yawDelta/57.2957795f);
        passengerPitch=Bound(passengerPitch+TurretStick(look.y)*60.f*dt,0,-65.f,65.f);
        vr->SetYawOffset(vr->GetYawOffset()+yawDelta);
    }
    const float lift=Bound(g.c_VehicleEyeHeight->Value(),.18f,0.f,.40f)+
        (vehicle.kind==VehicleSeat::Ghost?Bound(g.c_GhostEyeLift->Value(),.18f,0.f,.45f):
            shade?Bound(g.c_ShadeEyeLift->Value(),.18f,0.f,.45f):0.f);
    // A turret gunner's head moves relative to its chassis while aiming. Its
    // rendered head pose is authoritative; freezing chassis-local coordinates
    // strands the camera beside Chief. Other seats keep their stable anchor.
    Vector3 target=V(vehicle.eye);
    if(!headSeat){
        const Matrix4 basis=VehicleBasis(vehicle);
        const auto headLocal=Matrix4(basis).invertAffine()*V(vehicle.eye);
        const bool wasLocked=seatAnchor.Locked();
        const auto seatLocal=seatAnchor.Update(headLocal,g.lastDeltaTime,g.MetresToWorld(1));
        if(!wasLocked && seatAnchor.Locked())Logger::log<<"[PhysicalVehicles] seat anchor settled seat="<<vehicle.seat
            <<" head_error_m="<<g.WorldToMetres((headLocal-seatLocal).length())<<std::endl;
        target=basis*seatLocal;
    }
    target+=Vector3(0,0,g.MetresToWorld(lift));
    if(exitRequested)target=VehicleBasis(vehicle)*exitSeatLocal;
    // The rendered chassis and Chief's head share the SAME interpolation
    // phase. Filtering one of them again makes the cockpit oscillate around
    // the viewer, especially with Chimera at non-30-Hz headset refresh rates.
    seatEye=(renderPose || headSeat)?target:seatMotion.Update(target,V(vehicle.velocity)*30.f,g.lastDeltaTime);
    viewOrigin=ViewOrigin();lastViewOrigin=viewOrigin;savedCamera=Helpers::GetCamera();viewSaved=true;
    savedFirstPosition=first.position;savedSecondPosition=second.position;
    savedFirstPlayer=first.drawPlayer;savedSecondPlayer=second.drawPlayer;firstView=&first;secondView=&second;
    Helpers::GetCamera().position=viewOrigin;
    first.position=second.position=viewOrigin;
    // Keep the native seated body; its actual render submission hides the head.
    first.drawPlayer=second.drawPlayer=true;
}
void PhysicalInteractionHandler::ApplyViewRotation(Matrix4& head)const{
    if(!viewSaved || (!exitBlend && vehicle.kind!=VehicleSeat::PelicanPassenger) || std::fabs(passengerPitch)<.001f)return;
    const auto position=head*Vector3();
    const auto forward=head.getLeftAxis();Vector3 side(-forward.y,forward.x,0);
    if(side.lengthSqr()<.01f)return;
    side.normalize();head.translate(-position).rotate(-passengerPitch,side).translate(position);
}
void PhysicalInteractionHandler::EndView(){if(viewSaved){Helpers::GetCamera()=savedCamera;firstView->position=savedFirstPosition;secondView->position=savedSecondPosition;
    firstView->drawPlayer=savedFirstPlayer;secondView->drawPlayer=savedSecondPlayer;viewSaved=false;}}
void PhysicalInteractionHandler::Draw(){
    // Chest previews, including empty grey previews, are submitted through the
    // native model pass in DrawNative: the same projection as Chief's hands.
}
void PhysicalInteractionHandler::PrepareVehicleEye(CameraFrustum& culling,CameraFrustum& raster)const{
    if(!viewSaved || !VehicleActive())return;
    // Both paths must describe the same current eye. Preserve the ordinary
    // native clipping range; discard only an invalid/oblique old camera plane.
    if(!std::isfinite(raster.zNear) || raster.zNear<=0)raster.zNear=.00390625f;
    if(!std::isfinite(raster.zFar) || raster.zFar<=raster.zNear)raster.zFar=1024.f;
    raster.unk1={};culling=raster;
    // Preserve native portal/object visibility. No PVS/debug override and
    // no culling-FOV expansion are installed by this build.
}
void PhysicalInteractionHandler::DrawNative(){
    if(!engine || !Usable())return;
    if(VehicleActive())return; // Native world draw supplies the seated body once.
    auto& g=Game::instance;auto* vr=g.GetVR();Matrix4 poses[2];unsigned mask=0,closed=0;
    for(unsigned h=0;h<2;++h){
        if(!vr->IsControllerTracked(Role(h)))continue;
        Matrix4 hand=vr->GetControllerTransform(Role(h),true);
        const auto p=hand*Vector3();hand.translate(-p);
        const auto rotation=g.c_ControllerRotation->Value();
        hand.rotateZ(rotation.z);hand.rotateY(rotation.y);hand.rotateX(rotation.x);
        const auto offset=hand*g.c_ControllerOffset->Value();
        hand.translate(Helpers::GetCamera().position+p*g.MetresToWorld(1)+offset);poses[h]=hand;
        if(grenadeActive && grenades.held[h]>=0){
            mask|=1u<<h;closed|=1u<<h;
        }
    }
    Matrix4 palms[2]={poses[0],poses[1]};
    engine->DrawInteractionHands(poses,mask,closed,palms);
    if(grenadeActive && !VehicleActive())for(unsigned h=0;h<2;++h)if(grenades.held[h]>=0 && (mask&(1u<<h))){
        Matrix4 grenade=palms[h];
        // Keep the grenade itself unreflected for the actual native right hand.
        if(grenade.getDeterminant()<0)for(unsigned i=0;i<3;++i)grenade[4+i]=-grenade[4+i];
        grenade.translate(-grenade.getForwardAxis()*.025f*g.MetresToWorld(1));
        grenade*=Matrix4().scale(.5f);
        engine->DrawGrenade(static_cast<unsigned>(grenades.held[h]),grenade,false);
    }
    if(grenadeActive && !VehicleActive())for(unsigned type=0;type<2;++type){
        Matrix4 pose;pose.translate(Helpers::GetCamera().position+V(chest[type])*g.MetresToWorld(1));
        pose*=Matrix4().scale(.32f);engine->DrawGrenade(type,pose,inventory.count[type]<=grenades.Reserved(type));
    }
}
void PhysicalInteractionHandler::HideGrenadeHand(const Bone* bones,Transform* out,unsigned count,int gun,int wrist){
    const unsigned offhand=Game::instance.bLeftHanded?1:0;
    if(!grenadeActive || grenades.held[offhand]<0 || !bones || !out || count>64 || wrist<0)return;
    for(unsigned i=0;i<count;++i)if(HandDescendant(bones,count,i,wrist) && !HandDescendant(bones,count,i,gun)){
        out[i].scale=0;out[i].translation=out[wrist].translation;
    }
}
