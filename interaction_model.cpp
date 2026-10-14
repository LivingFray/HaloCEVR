#include "../HaloCEVR/Immersion/InteractionModel.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
using namespace Immersion;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
static bool close(float a, float b) { return std::fabs(a-b) < 0.00001f; }

void InventoryCases() {
    for (auto firing : {Hand::Left, Hand::Right}) {
        const auto support = Other(firing);
        Inventory i;
        CHECK(i.SeedHolster(0, 0x12340000));
        CHECK(i.SeedHolster(1, 0x12350001));
        CHECK(!i.SeedHolster(1, 99));
        CHECK(i.Draw(0, firing));
        CHECK(i.SlotOwner(0) == 0x12340000);
        CHECK(!i.Draw(0, support));
        CHECK(i.GrabSupport(support, 0x12340000));
        CHECK(i.Supported(0x12340000));
        CHECK(i.CanFire(firing) && !i.CanFire(support));
        CHECK(close(RecoilScale(i, 0x12340000), 0.35f));
        CHECK(i.ReleaseGrip(firing).kind == ReleaseKind::StillHeld);
        CHECK(!i.CanFire(firing) && !i.CanFire(support));
        CHECK(!i.Supported(0x12340000));
        CHECK(i.GetHand(support).hold == Hold::SupportGrip);
        CHECK(i.RegrabTrigger(firing, 0x12340000));
        CHECK(i.ReleaseGrip(support).kind == ReleaseKind::StillHeld);
        CHECK(close(RecoilScale(i, 0x12340000), 1.0f));
        CHECK(i.ReleaseGrip(firing, 0).kind == ReleaseKind::Stowed);
        CHECK(i.Draw(0, firing));
        CHECK(i.Draw(1, support));
        CHECK(i.CanFire(firing) && i.CanFire(support));
        CHECK(!i.Supported(0x12340000)); // Two guns != support.
        CHECK(!i.Pickup(20, firing, true));
        auto drop = i.ReleaseGrip(firing);
        CHECK(drop.kind == ReleaseKind::Dropped && drop.object == 0x12340000);
        CHECK(i.SlotOwner(0) == None);
        CHECK(i.Pickup(20, firing, true));
        CHECK(i.ReleaseGrip(firing, 0).kind == ReleaseKind::Stowed);
        // A rejected engine pickup or drop must not mutate the live model.
        auto candidate = i;
        CHECK(candidate.ReleaseGrip(support).kind == ReleaseKind::Dropped);
        CHECK(candidate.SlotOwner(1) == None);
        CHECK(i.SlotOwner(1) == 0x12350001 && !i.Empty(support));
        i.RemoveInvalidObject(0x99990001); // Same index, different generation.
        CHECK(!i.Empty(support));
        i.RemoveInvalidObject(0x12350001);
        CHECK(i.Empty(support) && i.SlotOwner(1) == None);
    }
    Inventory i;
    CHECK(i.SeedHolster(0,1) && i.SeedHolster(1,2));
    CHECK(!i.Pickup(3,Hand::Left,false));
    CHECK(i.Pickup(3,Hand::Left,true)); // Temporary third weapon allowed.
    auto result = i.ReleaseGrip(Hand::Left,0);
    CHECK(result.kind == ReleaseKind::Dropped && result.object == 3);
    CHECK(i.SlotOwner(0)==1 && i.SlotOwner(1)==2);
    CHECK(i.Draw(0,Hand::Right));
    CHECK(i.GrabSupport(Hand::Left,1));
    CHECK(i.ReleaseGrip(Hand::Left).kind==ReleaseKind::StillHeld);
    CHECK(i.ReleaseGrip(Hand::Right).kind==ReleaseKind::Dropped);
    CHECK(i.ReleaseGrip(Hand::Left).kind==ReleaseKind::Nothing); // No duplicate drop.
    CHECK(i.SlotOwner(0)==None);
    CHECK(i.OccupyWithItem(Hand::Right,90));
    CHECK(!i.CanFire(Hand::Right) && !i.Pickup(1,Hand::Right,true));
    CHECK(!i.ClearItem(Hand::Right,91) && i.ClearItem(Hand::Right,90));
}

void LayoutAndInputs() {
    const Layout right{}, left{true};
    for (auto zone : {Zone::Shoulder, Zone::WeaponHip, Zone::SupplyHip, Zone::PlasmaChest, Zone::FragChest}) {
        auto r=right.Offset(zone), l=left.Offset(zone);
        CHECK(close(r.x,l.x) && close(r.z,l.z) && close(r.y,-l.y));
        auto a=right.Anchor(zone,{2,3,4},1.5707963268f);
        CHECK(close(a.x,2-r.y) && close(a.y,3+r.x) && close(a.z,4+r.z));
    }
    Button b;
    CHECK(!b.Update(false).pressed);
    CHECK(b.Update(true).pressed);
    CHECK(!b.Update(true).pressed);
    CHECK(b.Update(false).released);
    CHECK(!b.Update(true,false).released);
    CHECK(!b.Update(true).pressed); // Tracking resume while gripping.
    b.Suspend();
    CHECK(!b.Update(false).released); // No synthesized drop.
    CHECK(b.Update(true).pressed);
    CHECK(ResolveGrip(false,false,false,true,true,true)==GripAction::Pickup);
    CHECK(ResolveGrip(true,false,true,true,true,true)==GripAction::Flashlight);
    CHECK(ResolveGrip(true,false,false,true,true,true)==GripAction::Retain);
    CHECK(ResolveGrip(false,true,true,true,true,true)==GripAction::Manipulate);
    CHECK(ResolveTrigger(Hold::Empty,false,true,true)==TriggerAction::Interact);
    CHECK(ResolveTrigger(Hold::TriggerGrip,false,true,true)==TriggerAction::FireWeapon);
    CHECK(ResolveTrigger(Hold::SupportGrip,false,true,true)==TriggerAction::None);
    CHECK(ResolveTrigger(Hold::OtherItem,true,true,true)==TriggerAction::ArmPlasma);
    CHECK(ResolveTrigger(Hold::OtherItem,true,false,true)==TriggerAction::None);
    CHECK(ResolveTrigger(Hold::OtherItem,false,true,true)==TriggerAction::None);
}

void FlashlightCases() {
    HeadTouch touch;
    CHECK(!touch.Update({false,false},{false,true},{false,false},0.01f));
    CHECK(touch.Update({true,false},{true,true},{true,false},0.01f));
    for (int n=0;n<200;++n) CHECK(!touch.Update({true,false},{true,true},{false,false},0.01f));
    CHECK(!touch.Update({true,false},{false,true},{false,false},0.01f));
    CHECK(touch.Update({true,false},{true,true},{true,false},0.01f)); // Fresh squeeze re-arms at the temple.
    CHECK(!touch.Update({false,false},{true,true},{false,false},0.01f));
    CHECK(touch.Update({true,false},{true,true},{true,false},0.01f));
    CHECK(!touch.Update({false,false},{true,true},{false,false},0.01f));
    for(int n=0;n<21;++n) CHECK(!touch.Update({false,true},{true,true},{false,false},0.01f));
    CHECK(touch.Update({false,true},{true,true},{false,false},0.02f)); // Holding weapon, deliberate touch.
    CHECK(!touch.Update({false,false},{false,false},{false,false},0.01f));
    CHECK(touch.Update({true,true},{true,true},{true,true},0.01f)); // One bool pulse, not two.
    CHECK(!touch.Update({true,true},{true,true},{true,true},0.01f));
    CHECK(!touch.Update({false,false},{false,false},{false,false},0.01f,false));
    CHECK(!touch.Update({true,true},{true,true},{true,true},0.01f)); // Resume at head is blocked.
    CHECK(!touch.Update({false,false},{false,false},{false,false},0.01f));
    CHECK(touch.Update({false,true},{false,true},{false,true},0.01f));
}

void GrenadeCases() {
    for (auto hand : {Hand::Left,Hand::Right}) {
        Grenade g;
        CHECK(g.Draw(GrenadeKind::Plasma,hand));
        CHECK(!g.Draw(GrenadeKind::Frag,Other(hand)));
        CHECK(!g.GrabPin(Other(hand),{},true,true));
        CHECK(!g.ArmWithTrigger(Other(hand),true));
        CHECK(!g.ArmWithTrigger(hand,false));
        CHECK(g.ArmWithTrigger(hand,true));
        CHECK(!g.ArmWithTrigger(hand,true)); // One confirmation only.
        CHECK(!g.ReturnSafeToHolster(hand,GrenadeKind::Plasma));
        CHECK(!g.ReleaseGrip(hand,{4,0,2},false).valid);
        CHECK(g.Phase()==GrenadePhase::Armed);
        auto release=g.ReleaseGrip(hand,{4,0,2},true);
        CHECK(release.valid && release.armed && release.hand==hand);
        CHECK(close(release.velocity.x,4) && close(release.velocity.z,2));
        CHECK(!g.ReleaseGrip(hand,{},true).valid);
        CHECK(g.Draw(GrenadeKind::Frag,hand));
        CHECK(!g.ArmWithTrigger(hand,true));
        CHECK(!g.GrabPin(hand,{},true,true));
        CHECK(!g.GrabPin(Other(hand),{},false,true));
        CHECK(g.GrabPin(Other(hand),{},true,true));
        CHECK(!g.PullPin({0.02f,0,0},true));
        CHECK(!g.PullPin({0.12f,0,0},false));
        CHECK(g.Phase()==GrenadePhase::Safe);
        CHECK(g.GrabPin(Other(hand),{},true,true));
        CHECK(!g.PullPin({},true)); // Both hands move together in world.
        CHECK(g.PullPin({0.09f,0,0},true));
        CHECK(!g.PullPin({0.09f,0,0},true));
        release=g.ReleaseGrip(hand,{},true);
        CHECK(release.valid && release.armed && release.velocity.LengthSquared()==0); // Armed low-speed drop.
        CHECK(g.Draw(GrenadeKind::Frag,hand));
        CHECK(!g.ReturnSafeToHolster(hand,GrenadeKind::Plasma));
        CHECK(g.ReturnSafeToHolster(hand,GrenadeKind::Frag));
        CHECK(g.Draw(GrenadeKind::Plasma,hand));
        release=g.ReleaseGrip(hand,{100,0,0},true);
        CHECK(release.valid && !release.armed && close(release.velocity.x,15));
    }
}

void ReloadCases() {
    // A mag reload has the same ammo/chamber rules for pistol, AR and sniper.
    for (int capacity : {12,60,4}) {
        Reload r;
        CHECK(r.Initialize(42,ReloadKind::Magazine,capacity,capacity,capacity*2));
        CHECK(r.ConfirmShot());
        const auto old=r.EjectMagazine();
        CHECK(old.valid && old.rounds==capacity-1 && !r.CanFire());
        CHECK(!r.EjectMagazine().valid);
        auto candidate=r;
        auto rejected=candidate.DrawSupply();
        CHECK(rejected.valid && r.Reserve()==capacity*2); // Failed native request: do not commit.
        auto mag=r.DrawSupply();
        CHECK(mag.valid && mag.rounds==capacity && r.Reserve()==capacity);
        CHECK(r.Insert(mag) && !mag.valid);
        CHECK(r.CanFire() && !r.NeedsRack()); // Tactical reload preserves ready action.
        CHECK(!r.Insert(mag)); // No duplication.
        for(int n=0;n<capacity;++n) CHECK(r.ConfirmShot());
        CHECK(!r.CanFire() && r.NeedsRack());
        auto empty=r.EjectMagazine();
        CHECK(empty.valid && empty.rounds==0);
        mag=r.DrawSupply();
        CHECK(r.Insert(mag));
        CHECK(!r.CanFire() && r.NeedsRack());
        CHECK(r.CompleteRackOrPump() && r.CanFire());
        CHECK(r.Loaded()+r.Reserve()+old.rounds == capacity*2-1); // Capacity+1 shots used; old mag preserved.
    }
    Reload shotgun;
    CHECK(shotgun.Initialize(5,ReloadKind::Shotgun,12,2,3));
    CHECK(shotgun.ConfirmShot() && !shotgun.CanFire());
    CHECK(shotgun.CompleteRackOrPump() && shotgun.ConfirmShot());
    CHECK(!shotgun.CanFire() && !shotgun.CompleteRackOrPump());
    auto shell=shotgun.DrawSupply();
    CHECK(shell.valid && shell.rounds==1 && shotgun.Reserve()==2);
    CHECK(shotgun.Insert(shell) && !shotgun.CanFire());
    CHECK(shotgun.CompleteRackOrPump() && shotgun.CanFire());
    shell=shotgun.DrawSupply();
    CHECK(shotgun.Insert(shell) && shotgun.CanFire()); // Top-up doesn't unchamber.
    CHECK(shotgun.Loaded()==2 && shotgun.Reserve()==1);
    Reload rocket;
    CHECK(rocket.Initialize(6,ReloadKind::Rockets,2,0,3));
    CHECK(!rocket.DrawSupply().valid);
    CHECK(rocket.OpenCasing());
    auto one=rocket.DrawSupply();
    CHECK(rocket.Insert(one) && !rocket.CanFire());
    auto two=rocket.DrawSupply();
    CHECK(rocket.Insert(two) && !rocket.DrawSupply().valid);
    CHECK(rocket.CloseCasing() && rocket.CanFire());
    CHECK(rocket.ConfirmShot());
    CHECK(rocket.OpenCasing() && !rocket.CanFire());
    one=rocket.DrawSupply();
    CHECK(rocket.Insert(one) && rocket.Loaded()==2 && rocket.Reserve()==0);
    CHECK(rocket.CloseCasing() && rocket.ConfirmShot() && rocket.ConfirmShot());
    CHECK(!rocket.CanFire());
    Reload a,b;
    CHECK(a.Initialize(7,ReloadKind::Magazine,12,0,5));
    CHECK(b.Initialize(8,ReloadKind::Magazine,12,0,5));
    a.EjectMagazine(); b.EjectMagazine();
    auto ammo=a.DrawSupply();
    CHECK(ammo.rounds==5 && a.Reserve()==0);
    CHECK(!b.Insert(ammo) && ammo.valid); // Wrong weapon cannot consume payload.
    CHECK(a.Insert(ammo) && a.Loaded()==5 && !a.CanFire());
    CHECK(a.CompleteRackOrPump() && a.CanFire());
    auto issued=a.EjectMagazine();
    auto replay=issued;
    CHECK(a.Insert(issued));
    auto real=a.EjectMagazine();
    CHECK(!a.Insert(replay)); // A copied payload can't create a second magazine.
    auto forged=real;
    forged.rounds=12;
    CHECK(!a.Insert(forged));
    CHECK(a.Insert(real));
    CHECK(a.Initialize(7,ReloadKind::Native,20,1,10));
    CHECK(a.WantsNativeReload() && !a.EjectMagazine().valid && !a.DrawSupply().valid);
    CHECK(a.Initialize(7,ReloadKind::Battery,100,100,0));
    CHECK(!a.WantsNativeReload() && !a.DrawSupply().valid);
}

void StrokeCases() {
    Stroke s;
    CHECK(!s.Begin({},{},0.1f));
    CHECK(s.Begin({},{-1,0,0},0.1f));
    CHECK(!s.Update({0,0.2f,0},true)); // Lateral movement is not a rack.
    CHECK(!s.Update({-0.11f,0,0},true));
    CHECK(s.Update({},true));
    CHECK(!s.Update({},true));
    CHECK(s.Begin({},{-1,0,0},0.1f));
    CHECK(!s.Update({-0.11f,0,0},true));
    CHECK(!s.Update({},false)); // Dropped pump grip doesn't complete cycle.
    CHECK(!s.Update({},true));
    const float nan=std::numeric_limits<float>::quiet_NaN();
    CHECK(!s.Begin({},{-1,0,0},nan));
    CHECK(!Within({nan,0,0},{},0.2f));
}
int main() {
    InventoryCases(); LayoutAndInputs(); FlashlightCases(); GrenadeCases(); ReloadCases(); StrokeCases();
    std::cout << "Interaction model: " << checks << " checks passed\n";
}
