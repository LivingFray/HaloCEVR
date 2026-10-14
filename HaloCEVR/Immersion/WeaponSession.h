#pragma once
#include "InteractionModel.h"

namespace Immersion {
struct NativeInventory {
    Object unit = None;
    std::uint32_t storage = 0, age = 0;
    std::array<Object,4> weapons{None,None,None,None};
    Object current = None;
    Object player = None;
    bool Contains(Object id) const {
        return id != None && std::find(weapons.begin(), weapons.end(), id) != weapons.end();
    }
};
struct DropPose { Point position, forward{1,0,0}, up{0,0,1}, velocity; };
enum class NativeDrop { Rejected, WorldObject, RemovedWithoutWorldObject };
class WeaponEngine {
public:
    virtual ~WeaponEngine() = default;
    virtual bool Read(NativeInventory& out) const = 0;
    virtual bool Select(Object unit, Object weapon) = 0;
    virtual NativeDrop Drop(Object unit, Object weapon, const DropPose& pose) = 0;
    virtual bool Pickup(Object, Object) { return false; }
};
enum class WeaponEvent { Initialized, Reconciled, PickedUp, Drawn, Supported, TriggerRegripped,
    SupportReleased, TriggerReleased, Stowed, Dropped, Removed, Rejected };
inline const char* WeaponEventName(WeaponEvent event) {
    switch (event) {
        case WeaponEvent::Initialized: return "initialized";
        case WeaponEvent::Reconciled: return "inventory-reconciled";
        case WeaponEvent::PickedUp: return "grip-picked-up";
        case WeaponEvent::Drawn: return "drawn";
        case WeaponEvent::Supported: return "supported";
        case WeaponEvent::TriggerRegripped: return "trigger-regripped";
        case WeaponEvent::SupportReleased: return "support-released";
        case WeaponEvent::TriggerReleased: return "trigger-released";
        case WeaponEvent::Stowed: return "stowed";
        case WeaponEvent::Dropped: return "dropped";
        case WeaponEvent::Removed: return "native-removed-without-world-object";
        case WeaponEvent::Rejected: return "native-action-rejected";
        default: return "unknown";
    }
}
struct WeaponFrame {
    Object pickupTarget = None;
    std::array<Edges,2> grip{};
    std::array<bool,2> consumed{};
    std::array<int,2> holster{-1,-1};
    std::array<DropPose,2> release{};
    // Computed in weapon/controller space by the input adapter.
    std::array<bool,2> nearSupport{}, nearTrigger{};
};
struct WeaponEvents {
    struct Entry { WeaponEvent event; Hand hand; Object weapon; };
    std::array<Entry,8> entries{};
    unsigned count = 0;
    void Add(WeaponEvent event, Hand hand, Object weapon) {
        if (count < entries.size()) entries[count++] = {event,hand,weapon};
    }
};

// R1: one native weapon can be held, with either/both hands. Two holster homes.
// Dual rendering and temporary third-weapon pickup are separate integrations.
// Every native mutation is checked before committing the portable ownership.
class WeaponSession {
public:
    void Reset() { inventory.Reset(); observed = {}; active = false; firingHand = Hand::Right; }
    bool Active() const { return active; }
    const Inventory& Items() const { return inventory; }
    Hand FiringHand() const { return firingHand; }
    Object Held() const {
        for (Hand hand : {Hand::Left,Hand::Right})
            if (!inventory.Empty(hand)) return inventory.GetHand(hand).object;
        return None;
    }
    bool SupportOnly() const { return Held() != None && !inventory.CanFire(firingHand); }
    bool CanFire(const WeaponFrame& frame) const {
        return active && inventory.CanFire(firingHand) && frame.grip[Index(firingHand)].down;
    }
    bool Owns(Object weapon) const { return active && observed.Contains(weapon); }
    // An intentional action/pump grip takes precedence over support aiming.
    void ReleaseSupportForInteraction(Hand hand) {
        if (inventory.GetHand(hand).hold==Hold::SupportGrip) inventory.ReleaseGrip(hand);
    }
    void RestoreSupportAfterPump(Hand hand) {
        if (active && !SupportOnly() && inventory.Empty(hand)) inventory.GrabSupport(hand,Held());
    }

    WeaponEvents Update(WeaponEngine& engine, const WeaponFrame& frame) {
        WeaponEvents events;
        NativeInventory now;
        if (!engine.Read(now)) return events;
        // Only a new player/checkpoint starts a new ownership session. Native
        // grants and inventory slot reordering must preserve surviving homes.
        if (!active || now.unit != observed.unit || now.storage != observed.storage ||
            now.age < observed.age) {
            Inventory candidate;
            // Latest layout: current/first weapon at hip (1), second at shoulder (0).
            const unsigned homes[2] = {1,0};
            unsigned home = 0;
            if (now.current != None) {
                if (!now.Contains(now.current) || !candidate.SeedHolster(homes[home++],now.current)) return events;
            }
            for (Object id : now.weapons) {
                if (id == None || id == now.current) continue;
                if (home >= 2 || !candidate.SeedHolster(homes[home++],id)) return events;
            }
            if (!engine.Select(now.unit,None)) return events;
            NativeInventory after;
            if (!engine.Read(after) || after.unit != now.unit || after.storage != now.storage ||
                after.weapons != now.weapons || after.current != None) return events;
            inventory = candidate; observed = after; active = true;
            events.Add(WeaponEvent::Initialized,Hand::Right,None);
            return events; // A press during initialization cannot also draw.
        }
        if (now.weapons != observed.weapons) {
            Inventory candidate = inventory;
            for (Object id : observed.weapons) if (!now.Contains(id)) candidate.RemoveInvalidObject(id);
            for (Object id : now.weapons) {
                if (id == None || observed.Contains(id)) continue;
                const unsigned home = candidate.SlotOwner(1) == None ? 1 : 0;
                if (!candidate.SeedHolster(home,id)) return events;
            }
            Object expected = None;
            for (Hand hand : {Hand::Left,Hand::Right})
                if (!candidate.Empty(hand)) expected = candidate.GetHand(hand).object;
            if (!engine.Select(now.unit,expected)) return events;
            NativeInventory after;
            if (!engine.Read(after) || after.unit != now.unit || after.storage != now.storage ||
                after.weapons != now.weapons || after.current != expected) return events;
            inventory = candidate; observed = after; now = after;
            events.Add(WeaponEvent::Reconciled,Hand::Right,None);
            // Process this frame's genuine release/press as well; a grant must
            // not swallow the only grip release and leave a weapon stuck held.
        }
        observed.age = now.age;
        // Suppress Halo's automatic selection of the other still-holstered gun.
        if (now.current != Held() && !engine.Select(now.unit,Held())) return events;

        // Release both hands before processing any acquisitions. Two releases
        // in one sample must produce exactly one world drop, not two copies.
        for (Hand hand : {Hand::Left,Hand::Right}) {
            const unsigned h = Index(hand);
            if (!frame.grip[h].released || inventory.Empty(hand)) continue;
            Inventory candidate = inventory;
            const auto released = candidate.ReleaseGrip(hand,frame.holster[h]);
            if (released.kind == ReleaseKind::StillHeld) {
                const bool wasTrigger = inventory.CanFire(hand);
                inventory = candidate;
                events.Add(wasTrigger ? WeaponEvent::TriggerReleased : WeaponEvent::SupportReleased,hand,released.object);
            } else if (released.kind == ReleaseKind::Stowed) {
                if (engine.Select(now.unit,None)) {
                    inventory = candidate;
                    events.Add(WeaponEvent::Stowed,hand,released.object);
                } else events.Add(WeaponEvent::Rejected,hand,released.object);
            } else if (released.kind == ReleaseKind::Dropped) {
                const auto result = engine.Drop(now.unit,released.object,frame.release[h]);
                if (result != NativeDrop::Rejected) {
                    inventory = candidate;
                    events.Add(result == NativeDrop::WorldObject ? WeaponEvent::Dropped : WeaponEvent::Removed,
                        hand,released.object);
                } else events.Add(WeaponEvent::Rejected,hand,released.object);
            }
        }
        for (Hand hand : {Hand::Left,Hand::Right}) {
            const unsigned h = Index(hand);
            if (!frame.grip[h].pressed || frame.consumed[h] || !inventory.Empty(hand)) continue;
            Inventory candidate = inventory;
            const Object held = Held();
            if (held != None) {
                if (SupportOnly() && frame.nearTrigger[h] && candidate.RegrabTrigger(hand,held)) {
                    inventory = candidate; firingHand = hand;
                    events.Add(WeaponEvent::TriggerRegripped,hand,held);
                } else if (!SupportOnly() && frame.nearSupport[h] && candidate.GrabSupport(hand,held)) {
                    inventory = candidate;
                    events.Add(WeaponEvent::Supported,hand,held);
                }
                continue; // R1 never silently swaps a held gun for the other one.
            }
            const int slot = frame.holster[h];
            const bool fullHomes = candidate.SlotOwner(0) != None && candidate.SlotOwner(1) != None;
            const bool atOwnedHome = slot >= 0 && slot < 2 && candidate.SlotOwner(static_cast<unsigned>(slot)) != None;
            // With full homes, allow deliberately drawing a gun at its body
            // anchor so the player can drop it to make room for the floor gun.
            if (frame.pickupTarget != None && !(fullHomes && atOwnedHome)) {
                const Object weapon = frame.pickupTarget;
                if (candidate.Pickup(weapon,hand,true) && engine.Pickup(now.unit,weapon)) {
                    inventory = candidate; firingHand = hand;
                    events.Add(WeaponEvent::PickedUp,hand,weapon);
                } else events.Add(WeaponEvent::Rejected,hand,weapon);
                // With room to acquire, a visible item wins over a nearby home.
                // A rejected world pickup never falls through into a draw.
                break;
            }
            if (slot < 0 || slot >= 2 || !candidate.Draw(static_cast<unsigned>(slot),hand)) continue;
            const Object weapon = candidate.GetHand(hand).object;
            if (engine.Select(now.unit,weapon)) {
                inventory = candidate; firingHand = hand;
                events.Add(WeaponEvent::Drawn,hand,weapon);
            } else events.Add(WeaponEvent::Rejected,hand,weapon);
        }
        NativeInventory after;
        // Do not absorb an unexpected partial native mutation: reconcile it
        // next frame instead of losing track of a newly granted/removed gun.
        if (engine.Read(after)) {
            bool matches = true;
            for (Object id : after.weapons)
                if (id != None && !inventory.InHand(id) && inventory.SlotOwner(0) != id && inventory.SlotOwner(1) != id) matches = false;
            for (unsigned slot=0; slot<2; ++slot)
                if (inventory.SlotOwner(slot) != None && !after.Contains(inventory.SlotOwner(slot))) matches = false;
            for (Hand hand : {Hand::Left,Hand::Right})
                if (!inventory.Empty(hand) && !after.Contains(inventory.GetHand(hand).object)) matches = false;
            if (matches) observed = after;
        }
        return events;
    }
private:
    Inventory inventory;
    NativeInventory observed;
    Hand firingHand = Hand::Right;
    bool active = false;
};
} // namespace Immersion
