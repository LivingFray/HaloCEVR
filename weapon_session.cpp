#include "../HaloCEVR/Immersion/WeaponSession.h"
#include <cstdlib>
#include <iostream>
using namespace Immersion;
struct Engine : WeaponEngine {
    NativeInventory state{100,0x10000,50,{11,22,None,None},11};
    bool valid=true, rejectSelect=false, rejectDrop=false, deleteDrop=false;
    unsigned selections=0,drops=0,pickups=0;
    bool rejectPickup=false,partialPickup=false;
    bool Pickup(Object unit,Object weapon) override {
        ++pickups;
        if(rejectPickup || unit!=state.unit || state.current!=None || state.Contains(weapon) ||
           std::count_if(state.weapons.begin(),state.weapons.end(),[](Object id){return id!=None;})>=2) return false;
        *std::find(state.weapons.begin(),state.weapons.end(),None)=weapon;
        if(partialPickup) return false;
        state.current=weapon;return true;
    }
    bool Read(NativeInventory& out) const override {out=state;return valid;}
    bool Select(Object unit,Object weapon) override {
        ++selections;
        if(rejectSelect || unit!=state.unit || (weapon!=None && !state.Contains(weapon))) return false;
        state.current=weapon; return true;
    }
    NativeDrop Drop(Object unit,Object weapon,const DropPose&) override {
        ++drops;
        if(rejectDrop || unit!=state.unit || weapon!=state.current) return NativeDrop::Rejected;
        for(auto& w:state.weapons) if(w==weapon) w=None;
        state.current=None;
        return deleteDrop ? NativeDrop::RemovedWithoutWorldObject : NativeDrop::WorldObject;
    }
};
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
WeaponFrame draw(Hand hand,int slot) {
    WeaponFrame f;f.grip[Index(hand)]={true,true,false};f.holster[Index(hand)]=slot;return f;
}
int main() {
    for(Hand hand:{Hand::Left,Hand::Right}) {
        Engine e;WeaponSession s;
        CHECK(s.Update(e,{}).count==1);CHECK(e.state.current==None);
        CHECK(s.Items().SlotOwner(1)==11);CHECK(s.Items().SlotOwner(0)==22);
        auto f=draw(hand,1);s.Update(e,f);
        CHECK(s.Held()==11);CHECK(s.FiringHand()==hand);CHECK(s.CanFire(f));CHECK(e.state.current==11);
        CHECK(s.Items().SlotOwner(1)==11); // Reserved during a draw.
        f={};f.grip[Index(hand)]={false,false,true};f.holster[Index(hand)]=1;
        s.Update(e,f);CHECK(s.Held()==None);CHECK(e.state.Contains(11));CHECK(e.drops==0);
        s.Update(e,draw(hand,0));CHECK(s.Held()==22);
        f={};f.grip[Index(hand)]={false,false,true};
        s.Update(e,f);CHECK(e.drops==1);CHECK(!e.state.Contains(22));CHECK(s.Items().SlotOwner(0)==None);
        s.Update(e,f);CHECK(e.drops==1); // No double drop on stale release input.
        s.Update(e,draw(hand,1));f={};f.grip[Index(hand)]={false,false,true};f.holster[Index(hand)]=0;
        s.Update(e,f);CHECK(s.Items().SlotOwner(0)==11);CHECK(s.Items().SlotOwner(1)==None);
    }
    // Both release orders, support-only hold, and physical trigger regrip.
    for(Hand hand:{Hand::Left,Hand::Right}) {
        Engine e;WeaponSession s;s.Update(e,{});s.Update(e,draw(hand,1));
        Hand off=Other(hand);WeaponFrame f;
        f.grip[Index(hand)].down=true;f.grip[Index(off)]={true,true,false};f.nearSupport[Index(off)]=true;
        s.Update(e,f);CHECK(s.Items().Supported(11));CHECK(s.CanFire(f));
        f={};f.grip[Index(hand)]={false,false,true};f.grip[Index(off)].down=true;
        s.Update(e,f);CHECK(s.SupportOnly());CHECK(s.Held()==11);CHECK(e.drops==0);CHECK(!s.CanFire(f));
        f.grip[Index(hand)]={true,true,false};f.nearTrigger[Index(hand)]=true;
        s.Update(e,f);CHECK(!s.SupportOnly());CHECK(s.Items().Supported(11));CHECK(s.CanFire(f));
        f={};f.grip[0].released=true;f.grip[1].released=true;
        s.Update(e,f);CHECK(s.Held()==None);CHECK(e.drops==1);CHECK(!e.state.Contains(11));
    }
    { // Failed mutations never release portable ownership or reserve a fake hand.
        Engine e;WeaponSession s;s.Update(e,{});e.rejectSelect=true;
        s.Update(e,draw(Hand::Right,1));CHECK(s.Held()==None);CHECK(s.Items().SlotOwner(1)==11);
        e.rejectSelect=false;s.Update(e,draw(Hand::Right,1));e.rejectSelect=true;
        WeaponFrame f;f.grip[1].released=true;f.holster[1]=1;
        s.Update(e,f);CHECK(s.Held()==11);CHECK(e.state.Contains(11));
        e.rejectSelect=false;e.rejectDrop=true;f.holster[1]=-1;
        s.Update(e,f);CHECK(s.Held()==11);CHECK(!s.CanFire(f));CHECK(s.Items().SlotOwner(1)==11);
        e.rejectDrop=false;e.deleteDrop=true;s.Update(e,f);
        CHECK(s.Held()==None);CHECK(s.Items().SlotOwner(1)==None);
    }
    { // Native pickups/checkpoint state reconcile without deleting or duplicating guns.
        Engine e;WeaponSession s;s.Update(e,{});s.Update(e,draw(Hand::Right,1));
        e.state.weapons[0]=33;e.state.current=33;
        s.Update(e,{});CHECK(s.Held()==None);CHECK(s.Items().SlotOwner(1)==33);CHECK(e.drops==0);
        s.Update(e,draw(Hand::Left,1));e.state.age=1;
        s.Update(e,{});CHECK(s.Held()==None);CHECK(e.drops==0);
        e.valid=false;auto n=e.selections;s.Update(e,draw(Hand::Right,1));CHECK(e.selections==n);
    }
    { // Head gesture wins; attempting a second gun cannot replace the first.
        Engine e;WeaponSession s;s.Update(e,{});auto f=draw(Hand::Left,1);f.consumed[0]=true;
        s.Update(e,f);CHECK(s.Held()==None);
        s.Update(e,draw(Hand::Right,1));s.Update(e,draw(Hand::Left,0));
        CHECK(s.Held()==11);CHECK(s.Items().Empty(Hand::Left));CHECK(e.state.Contains(22));
    }
    for (Hand hand : {Hand::Left,Hand::Right}) {
        // A scripted grant keeps the existing hip/shoulder identity and held hand.
        Engine e;e.state.weapons={11,None,None,None};WeaponSession s;s.Update(e,{});
        s.Update(e,draw(hand,1));e.state.weapons[1]=22;e.state.current=22;
        auto events=s.Update(e,{});CHECK(events.count==1);CHECK(events.entries[0].event==WeaponEvent::Reconciled);
        CHECK(s.Held()==11);CHECK(e.state.current==11);CHECK(s.Items().SlotOwner(1)==11);CHECK(s.Items().SlotOwner(0)==22);
        std::swap(e.state.weapons[0],e.state.weapons[1]);s.Update(e,{});
        CHECK(s.Held()==11);CHECK(s.Items().SlotOwner(1)==11);CHECK(s.Items().SlotOwner(0)==22);
        WeaponFrame f;f.grip[Index(hand)].released=true;s.Update(e,f);
        CHECK(s.Held()==None);CHECK(e.drops==1);CHECK(s.Items().SlotOwner(0)==22);
        // Walking past the target, or already holding grip, cannot acquire it.
        f={};f.pickupTarget=33;s.Update(e,f);CHECK(e.pickups==0);
        f.grip[Index(hand)].down=true;s.Update(e,f);CHECK(e.pickups==0);
        f.grip[Index(hand)].pressed=true;f.holster[Index(hand)]=0;s.Update(e,f);
        CHECK(e.pickups==1);CHECK(s.Held()==33);CHECK(s.FiringHand()==hand);CHECK(e.state.current==33);
        CHECK(s.Items().SlotOwner(0)==22);CHECK(s.Items().SlotOwner(1)==None); // New item has no home yet.
        f={};f.grip[Index(hand)].down=true;s.Update(e,f);CHECK(s.Held()==33); // Not auto-stowed on next update.
        f.grip[Index(hand)]={false,false,true};f.holster[Index(hand)]=1;s.Update(e,f);
        CHECK(s.Items().SlotOwner(1)==33);CHECK(s.Items().SlotOwner(0)==22);CHECK(e.drops==1);
        f={};f.pickupTarget=44;f.grip[Index(hand)]={true,true,false};s.Update(e,f);
        CHECK(s.Held()==None);CHECK(!e.state.Contains(44));CHECK(s.Items().SlotOwner(1)==33); // Full inventory; no swap.
        f.holster[Index(hand)]=1;s.Update(e,f);CHECK(s.Held()==33);CHECK(!e.state.Contains(44)); // Body draw remains usable to free a home.
    }
    { // A scripted grant arriving during release must not swallow the release.
        Engine e;e.state.weapons={11,None,None,None};WeaponSession s;s.Update(e,{});s.Update(e,draw(Hand::Right,1));
        e.state.weapons[1]=22;e.state.current=22;WeaponFrame f;f.grip[1].released=true;
        s.Update(e,f);CHECK(e.drops==1);CHECK(s.Held()==None);CHECK(s.Items().SlotOwner(0)==22);
    }
    { // Empty hands/inventory can take a floor gun; simultaneous grips acquire only once.
        Engine e;e.state.weapons.fill(None);e.state.current=None;WeaponSession s;s.Update(e,{});
        WeaponFrame f;f.pickupTarget=55;f.grip[0]=f.grip[1]={true,true,false};
        s.Update(e,f);CHECK(e.pickups==1);CHECK(s.Held()==55);CHECK(s.Items().Empty(Hand::Right));
        f={};f.grip[0].released=true;s.Update(e,f);CHECK(e.drops==1);CHECK(!e.state.Contains(55));
    }
    { // Partial native acquisition gets reconciled next tick, never orphaned.
        Engine e;e.state.weapons={11,None,None,None};WeaponSession s;s.Update(e,{});e.partialPickup=true;
        WeaponFrame f;f.pickupTarget=22;f.grip[1]={true,true,false};s.Update(e,f);
        CHECK(s.Held()==None);CHECK(e.state.Contains(22));s.Update(e,{});
        CHECK(s.Items().SlotOwner(1)==11);CHECK(s.Items().SlotOwner(0)==22);CHECK(e.state.current==None);
    }
    { // Failed reconciliation leaves the old homes available for a later retry.
        Engine e;e.state.weapons={11,None,None,None};WeaponSession s;s.Update(e,{});
        e.state.weapons[1]=22;e.state.current=22;e.rejectSelect=true;s.Update(e,{});
        CHECK(s.Items().SlotOwner(1)==11);CHECK(s.Items().SlotOwner(0)==None);
        e.rejectSelect=false;s.Update(e,{});CHECK(s.Items().SlotOwner(0)==22);
    }
    std::cout<<checks<<" native-session transaction checks passed (engine double, not Halo)\n";
}
