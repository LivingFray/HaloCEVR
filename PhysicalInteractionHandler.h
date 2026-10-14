#pragma once
#include "Immersion/PhysicalInteractions.h"
#include "Helpers/Camera.h"
struct CameraFrustum;
namespace Immersion {class RetailWeaponEngine;}
struct Bone;
class PhysicalInteractionHandler {
public:
    void AttachEngine(Immersion::RetailWeaponEngine& native){engine=&native;}
    void Grenades(Immersion::RetailWeaponEngine& engine,const Immersion::WeaponSession& session,
        Immersion::WeaponFrame& frame,const Matrix4* hands,Immersion::Point head,float yaw);
    void SuspendGrenades();
    void OnRecentre(){passengerYaw=passengerPitch=passengerSeatYaw=0;exitBlend=false;exitRequested=false;haveSeat=false;seatMotion.Reset();seatYawMotion.Reset();seatAnchor.Reset();SuspendGrenades();}
    bool GrenadeControls();
    bool BeforeThrow(Immersion::Object unit);
    void AfterThrow(Immersion::Object unit);
    void UpdateVehicle(Immersion::RetailWeaponEngine& engine,bool inVehicle,const bool* triggers,bool valid,Vector2 move={});
    bool VehicleCamera(float& yaw,float& pitch,Vector2 move,Vector2 look);
    void BeginView(CameraFrustum& first,CameraFrustum& second);
    void EndView();
    void ApplyViewRotation(Matrix4& head)const;
    void PrepareVehicleEye(CameraFrustum& culling,CameraFrustum& raster)const;
    bool HideDrivingReticle()const{return VehicleActive() && vehicle.kind==Immersion::VehicleSeat::WarthogDriver;}
    void Draw();
    void DrawNative();
    void HideGrenadeHand(const Bone* bones,Transform* transforms,unsigned count,int gun,int wrist);
    Vector3 VehicleAim()const{return reticleReady?reticleAim:Vector3(vehicle.aim.x,vehicle.aim.y,vehicle.aim.z).normalize();}
    Immersion::Object VehicleBodyOwner()const{return viewSaved && VehicleActive()?vehicle.unit:Immersion::None;}
    bool ViewOverrideActive()const{return viewSaved;}
    bool GhostActive()const{return vehicle.kind==Immersion::VehicleSeat::Ghost;}
    bool VehicleActive()const{return vehicle.kind!=Immersion::VehicleSeat::None;}
    bool GrenadesActive()const{return grenadeActive;}
    bool BlocksMelee(unsigned hand)const{return VehicleActive() || (hand<2 && ((GrenadeHandMask()&(1u<<hand)) || throwCooldown[hand]>0));}
    unsigned GrenadeHandMask()const{return grenadeActive?((grenades.held[0]>=0?1u:0u)|(grenades.held[1]>=0?2u:0u)):0u;}
private:
    Vector3 ViewOrigin()const;
    void RefreshVehicle();
    Immersion::RetailWeaponEngine* engine=nullptr;
    Immersion::PhysicalGrenades grenades;
    std::array<Immersion::ThrowVelocityHistory,2> velocityHistory;
    std::array<float,2> throwCooldown{};
    Immersion::GrenadeInventory inventory;
    Immersion::Object thrown=Immersion::None;
    bool grenadeActive=false,grenadeHook=false;
    float chestYaw=0;
    std::array<Immersion::Point,2> chest{},chestGrip{};
    Immersion::VehicleState vehicle;
    Immersion::SeatMotion seatMotion,seatYawMotion;
    Immersion::SeatAnchor seatAnchor;
    const char* lastGrenadeStatus=nullptr;
    const char* lastVehicleStatus=nullptr;
    bool vehicleInputsValid=false,haveSeat=false,viewSaved=false;
    bool exitRequested=false,exitInteractDown=false,exitBlend=false;
    float exitRequestAge=0,exitBlendAge=0;
    float passengerYaw=0,passengerPitch=0,passengerSeatYaw=0;
    Vector3 exitSeatLocal{},lastViewOrigin{},exitBlendStart{};
    float vehicleYaw=0,nativeYaw=0,lastNativeYaw=0;
    Vector3 seatedHead{},seatEye{},viewOrigin{},reticleAim{1,0,0};
    bool reticleReady=false;
    Camera savedCamera{};
    Vector3 savedFirstPosition{},savedSecondPosition{};
    bool savedFirstPlayer=false,savedSecondPlayer=false;
    CameraFrustum* firstView=nullptr;CameraFrustum* secondView=nullptr;
};
