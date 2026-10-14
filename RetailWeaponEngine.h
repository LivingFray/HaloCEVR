#pragma once
#include "WeaponSession.h"
#include "LookHistory.h"
#include "HolsterMarker.h"
#include "Recoil.h"
#include "ManualReload.h"
#include "ReloadCue.h"
#include "NativeModelPose.h"
#include "PhysicalInteractions.h"
#include "ARAccuracy.h"
class Matrix4;

namespace Immersion {
// Only the supplied retail Halo PC 1.0.10.0621 image is currently verified.
// No scan fallback into a similar function on an unsupported edition.
class RetailWeaponEngine final : public WeaponEngine {
public:
    bool Initialize();
    void BeginAccuracyShot(Object weapon,short trigger,float scale);
    void EndAccuracyShot();
    float AccuracyReticleScale(float scale) const;
    bool ReadGrenades(GrenadeInventory& out) const;
    bool SelectGrenade(Object unit,unsigned type);
    Object AttachedGrenade(Object unit) const;
    bool FinishGrenade(Object projectile,const TrackedGrenadeRelease& release,float strength,float maxWorldSpeed);
    bool ReadVehicle(VehicleState& out) const;
    bool IsScorpionDriver() const;
    const Transform* VehicleBodyNodes(Object model,const Transform* submitted,Object owner) const;
    const char* VehicleReadStatus() const { return vehicleReadStatus; }
    const char* GrenadeReadStatus() const { return grenadeReadStatus; }
    bool DrawOnFootBody() const;
    bool DrawGrenade(unsigned type,const Matrix4& pose,bool grey) const;
    bool DrawInteractionHands(const Matrix4* poses,unsigned mask,unsigned closedMask,Matrix4* palms=nullptr) const;
    bool Available() const { return available; }
    bool Read(NativeInventory& out) const override;
    bool ReadNative(NativeInventory& out) const;
    bool ReadFireState(Object weapon, FireState& out) const;
    bool ReadReloadAmmo(Object weapon, ReloadAmmo& out, bool current=true) const;
    bool StartButtonReload(Object weapon);
    bool CommitReloadAmmo(const ReloadAmmo& before, const ReloadAmmo& after);
    bool ReadReloadModel(Object weapon, std::array<ModelNode,64>& nodes, unsigned& count) const;
    bool ReadReloadRocketModel(Object weapon,Object& model) const;
    bool DrawReloadRocket(Object weapon,const Matrix4& pose,bool spent) const;
    bool ReadReloadHandModel(Object weapon, std::array<ModelNode,64>& nodes, unsigned& count) const;
    bool ReadReloadFeedback(Object weapon, ReloadFeedbackAssets& assets) const;
    bool ReadReloadPcm(Object weapon,Object sound,ReloadPcm& pcm) const;
    bool ValidReloadFeedbackTag(Object weapon,Object tag,bool effect=false) const;
    const char* ReadStatus() const { return readStatus; }
    bool ReadMayResume() const { return readMayResume; }
    std::uint32_t RecordLook(Point forward, bool valid, float elapsed, float stickX=0, float stickY=0);
    bool Select(Object unit, Object weapon) override;
    // Called from validated campaign input; stowing never rebuilds the model.
    bool SynchronizeViewModel(bool atPose = false);
    bool ViewModelReady(Object held) const;
    bool ViewModelWeapon(Object& weapon, Object& tag, Object& animations) const;
    NativeDrop Drop(Object unit, Object weapon, const DropPose& pose) override;
    bool InstallPickupGuard();
    bool InstallVisualGuards();
    bool HideNativeWeapon() const;
    void SetPickupEnabled(bool enabled) {
        pickupEnabled = enabled && pickupGuardInstalled;
        if (!enabled) { stowedUnit = None; viewModelRefreshPending = false; }
    }
    // Render-only access. Never selects a weapon or changes the inventory.
    bool DrawBodyModels(bool hands, const std::array<HolsterMarker,2>& markers,
        const Matrix4* handPoses, const Matrix4* markerPoses) const;
    Object PickupTarget() const;
    bool Pickup(Object unit, Object weapon) override;
    bool HandleWorldItem(Object player, Object item);
private:
    bool ReadARSpread(Object weapon,ARSpreadPatch& patch,float scale) const;
    bool WriteARSpread(ARSpreadPatch& patch,bool apply);
    std::vector<ARSpreadPatch> accuracyShots;
    Object accuracyReportedTag=None;
    bool CheckCode() const;
    void CheckInteractionCode();
    bool NewLooseWeapon(const NativeInventory& player, Object item) const;
    bool pickupGuardInstalled = false, pickupEnabled = false;
    bool available = false;
    bool interactionAvailable = false, vehicleAvailable = false;
    mutable bool readMayResume = false;
    mutable const char* vehicleReadStatus = "seat not sampled";
    mutable const char* grenadeReadStatus = "inventory not sampled";
    bool recoilAvailable = false;
    bool reloadAvailable = false;
    bool visualGuardsInstalled = false;
    mutable unsigned lastBodyStatus = ~0u;
    mutable std::array<Object,2> lastMarkerObjects{None,None};
    bool viewModelRefreshPending = false;
    Object stowedUnit = None;
    std::uint32_t stowedStorage = 0, stowedAge = 0;
    mutable const char* readStatus = "Campaign inventory has not been read.";
    LookHistory lookHistory;
    Object lookUnit = None;
    std::uint32_t lookData = 0, previousLookFlags = 0;
};
}
