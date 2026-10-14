#pragma once
// Engine-independent rules. Mutate a candidate copy and commit it ONLY when
// the engine adapter confirms the corresponding native operation succeeded.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Immersion {
enum class Hand : unsigned { Left, Right };
inline unsigned Index(Hand hand) { return static_cast<unsigned>(hand); }
inline Hand Other(Hand hand) { return hand == Hand::Left ? Hand::Right : Hand::Left; }
using Object = std::uint32_t; // Full native datum: index AND generation/salt.
constexpr Object None = 0xffffffffu;

struct Point {
    float x = 0, y = 0, z = 0;
    Point operator-(Point b) const { return {x-b.x, y-b.y, z-b.z}; }
    Point operator+(Point b) const { return {x+b.x, y+b.y, z+b.z}; }
    Point operator*(float k) const { return {x*k, y*k, z*k}; }
    float Dot(Point b) const { return x*b.x + y*b.y + z*b.z; }
    float LengthSquared() const { return Dot(*this); }
    bool Finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};
inline bool Within(Point a, Point b, float radius) {
    return a.Finite() && b.Finite() && std::isfinite(radius) && radius > 0 &&
        (a-b).LengthSquared() <= radius*radius;
}

enum class Zone { Shoulder, WeaponHip, SupplyHip, PlasmaChest, FragChest };
struct Layout {
    bool leftHanded = false; // Persistent preference, NOT current holding hand.
    // Metres: forward, left, up relative to a calibrated head-height origin.
    Point Offset(Zone zone) const {
        Point p;
        switch (zone) {
        case Zone::Shoulder: p = {-0.15f, -0.24f, -0.06f}; break;
        case Zone::WeaponHip: p = {0.22f, -0.16f, -0.43f}; break;
        case Zone::SupplyHip: p = {0.14f, 0.25f, -0.43f}; break;
        case Zone::PlasmaChest: p = {0.06f, 0.06f, -0.24f}; break;
        case Zone::FragChest: p = {0.06f, -0.10f, -0.24f}; break;
        }
        if (leftHanded) p.y = -p.y;
        return p;
    }
    Point Anchor(Zone zone, Point origin, float bodyYawRadians) const {
        const Point p = Offset(zone);
        const float c = std::cos(bodyYawRadians), s = std::sin(bodyYawRadians);
        return origin + Point{p.x*c-p.y*s, p.x*s+p.y*c, p.z};
    }
};

// One pulse on entry, with a 3 cm exit margin to avoid buzzing at the edge.
// Independent state for each hand; callers reset on tracking/menu interruptions.
class HolsterHover {
public:
    int Update(unsigned hand, Point position, const std::array<Point,2>& zones,
               const float (&radii)[2]) {
        if (hand >= 2 || !position.Finite()) return -1;
        int& previous = inside[hand];
        if (previous >= 0 && Within(position,zones[previous],radii[previous]+0.03f)) return -1;
        previous = -1;
        float nearest = 1.0e10f;
        for (unsigned slot=0; slot<2; ++slot) {
            const float d = (position-zones[slot]).LengthSquared();
            if (Within(position,zones[slot],radii[slot]) && d < nearest) {
                previous = static_cast<int>(slot); nearest = d;
            }
        }
        return previous;
    }
    void Reset() { inside = {-1,-1}; }
private:
    std::array<int,2> inside{{-1,-1}};
};

// Tracks raw physical input edges. Lost input does not synthesize releases.
// The first valid sample after suspension is a baseline, never an action.
struct Edges { bool down = false, pressed = false, released = false; };
class Button {
public:
    Edges Update(bool down, bool valid = true) {
        if (!valid) { initialized = false; return {}; }
        Edges result{down, initialized && down && !previous,
            initialized && !down && previous};
        previous = down; initialized = true;
        return result;
    }
    void Suspend() { initialized = false; }
private:
    bool initialized = false, previous = false;
};

enum class Hold { Empty, TriggerGrip, SupportGrip, OtherItem };
struct HandState { Hold hold = Hold::Empty; Object object = None; };
enum class ReleaseKind { Nothing, StillHeld, Stowed, Dropped };
struct Release { ReleaseKind kind = ReleaseKind::Nothing; Object object = None; int slot = -1; };

class Inventory {
public:
    const HandState& GetHand(Hand h) const { return hands[Index(h)]; }
    Object SlotOwner(unsigned slot) const { return slot < 2 ? homes[slot] : None; }
    bool Empty(Hand h) const { return GetHand(h).hold == Hold::Empty; }
    bool InHand(Object object) const {
        if (object == None) return false;
        for (const auto& h : hands)
            if (h.object == object && (h.hold == Hold::TriggerGrip || h.hold == Hold::SupportGrip)) return true;
        return false;
    }
    bool SeedHolster(unsigned slot, Object object) {
        if (slot >= 2 || object == None || homes[slot] != None || Known(object)) return false;
        homes[slot] = object; return true;
    }
    bool Draw(unsigned slot, Hand h) {
        if (slot >= 2 || !Empty(h) || homes[slot] == None || InHand(homes[slot])) return false;
        hands[Index(h)] = {Hold::TriggerGrip, homes[slot]};
        return true; // Home remains reserved while drawn.
    }
    bool Pickup(Object object, Hand h, bool promptVisible) {
        if (!promptVisible || !Empty(h) || object == None || Known(object)) return false;
        hands[Index(h)] = {Hold::TriggerGrip, object};
        return true; // No forced swap even if both home slots are occupied.
    }
    bool GrabSupport(Hand h, Object object) {
        const auto& other = GetHand(Other(h));
        if (!Empty(h) || object == None || other.object != object || other.hold != Hold::TriggerGrip) return false;
        hands[Index(h)] = {Hold::SupportGrip, object}; return true;
    }
    bool RegrabTrigger(Hand h, Object object) {
        const auto& other = GetHand(Other(h));
        if (!Empty(h) || object == None || other.object != object || other.hold != Hold::SupportGrip) return false;
        hands[Index(h)] = {Hold::TriggerGrip, object}; return true;
    }
    bool OccupyWithItem(Hand h, Object object) {
        if (!Empty(h) || object == None || Known(object)) return false;
        hands[Index(h)] = {Hold::OtherItem, object}; return true;
    }
    bool ClearItem(Hand h, Object object) {
        if (GetHand(h).hold != Hold::OtherItem || GetHand(h).object != object) return false;
        hands[Index(h)] = {}; return true;
    }
    bool CanFire(Hand h) const { return GetHand(h).hold == Hold::TriggerGrip; }
    bool Supported(Object object) const {
        return object != None && hands[0].object == object && hands[1].object == object &&
            ((hands[0].hold == Hold::TriggerGrip && hands[1].hold == Hold::SupportGrip) ||
             (hands[1].hold == Hold::TriggerGrip && hands[0].hold == Hold::SupportGrip));
    }
    Release ReleaseGrip(Hand hand, int destinationSlot = -1) {
        auto& h = hands[Index(hand)];
        if (h.hold != Hold::TriggerGrip && h.hold != Hold::SupportGrip) return {};
        const Object object = h.object;
        h = {};
        if (InHand(object)) return {ReleaseKind::StillHeld, object, -1};
        if (destinationSlot >= 0 && destinationSlot < 2 &&
            (homes[destinationSlot] == None || homes[destinationSlot] == object)) {
            ForgetHome(object);
            homes[destinationSlot] = object;
            return {ReleaseKind::Stowed, object, destinationSlot};
        }
        ForgetHome(object);
        return {ReleaseKind::Dropped, object, -1};
    }
    void RemoveInvalidObject(Object object) {
        if (object == None) return;
        ForgetHome(object);
        for (auto& h : hands) if (h.object == object) h = {};
    }
    void Reset() { hands = {}; homes = {None, None}; }
private:
    bool Known(Object object) const {
        if (homes[0] == object || homes[1] == object) return true;
        for (const auto& h : hands) if (h.object == object) return true;
        return false;
    }
    void ForgetHome(Object object) { for (auto& id : homes) if (id == object) id = None; }
    std::array<HandState, 2> hands{};
    std::array<Object, 2> homes{None, None};
};

// Either physical hand can touch either temple; weapon occupancy is irrelevant.
// Output is a one-update pulse. Both hands together produce only one toggle.
class HeadTouch {
public:
    bool Update(const std::array<bool,2>& nearHead,
                const std::array<bool,2>& gripping,
                const std::array<bool,2>& pressed, float dt, bool valid = true) {
        if (!valid || !std::isfinite(dt) || dt < 0 || dt > 0.25f) {
            blocked = {true, true}; dwell = {}; return false;
        }
        bool toggle = false;
        for (unsigned h = 0; h < 2; ++h) {
            if (!nearHead[h]) { blocked[h] = false; dwell[h] = 0; continue; }
            if (!gripping[h]) { blocked[h] = false; dwell[h] = 0; continue; }
            if (blocked[h]) continue;
            dwell[h] += dt;
            if (pressed[h] || dwell[h] >= 0.22f) {
                blocked[h] = true; toggle = true;
            }
        }
        if (toggle) { // Prevent staggered two-hand contact from toggling back.
            for (unsigned h = 0; h < 2; ++h) if (nearHead[h]) blocked[h] = true;
        }
        return toggle;
    }
private:
    std::array<bool,2> blocked{};
    std::array<float,2> dwell{};
};

enum class GrenadeKind { Frag, Plasma };
enum class GrenadePhase { Absent, Safe, PinHeld, Armed };
struct GrenadeRelease { bool valid = false, armed = false; GrenadeKind kind = GrenadeKind::Frag; Hand hand = Hand::Right; Point velocity; };
class Grenade {
public:
    bool Draw(GrenadeKind kind, Hand hand) {
        if (phase != GrenadePhase::Absent) return false;
        type = kind; owner = hand; phase = GrenadePhase::Safe; return true;
    }
    bool ArmWithTrigger(Hand hand, bool freshPress) {
        if (phase != GrenadePhase::Safe || type != GrenadeKind::Plasma || hand != owner || !freshPress) return false;
        phase = GrenadePhase::Armed; return true; // One haptic/sound confirmation.
    }
    bool GrabPin(Hand hand, Point handInGrenadeSpace, bool freeHand, bool nearPin) {
        if (phase != GrenadePhase::Safe || type != GrenadeKind::Frag || hand == owner ||
            !freeHand || !nearPin || !handInGrenadeSpace.Finite()) return false;
        pinStart = handInGrenadeSpace; phase = GrenadePhase::PinHeld; return true;
    }
    bool PullPin(Point handInGrenadeSpace, bool pinGripHeld, float thresholdMetres = 0.08f) {
        if (phase != GrenadePhase::PinHeld) return false;
        if (!pinGripHeld) { phase = GrenadePhase::Safe; return false; }
        if (!handInGrenadeSpace.Finite() || !std::isfinite(thresholdMetres) || thresholdMetres < 0.02f) return false;
        if ((handInGrenadeSpace-pinStart).LengthSquared() < thresholdMetres*thresholdMetres) return false;
        phase = GrenadePhase::Armed; return true;
    }
    GrenadeRelease ReleaseGrip(Hand hand, Point releaseVelocity, bool trackingValid) {
        if (phase == GrenadePhase::Absent || hand != owner || !trackingValid || !releaseVelocity.Finite()) return {};
        // Input velocity is metres/second, already converted into world axes.
        // The engine adapter owns conversion to Halo velocity units.
        const float speed2 = releaseVelocity.LengthSquared();
        if (!std::isfinite(speed2)) return {};
        if (speed2 > 225.0f) releaseVelocity = releaseVelocity * (15.0f / std::sqrt(speed2));
        GrenadeRelease release{true, phase == GrenadePhase::Armed, type, owner, releaseVelocity};
        phase = GrenadePhase::Absent; return release;
    }
    bool ReturnSafeToHolster(Hand hand, GrenadeKind holster) {
        if (phase != GrenadePhase::Safe || hand != owner || type != holster) return false;
        phase = GrenadePhase::Absent; return true;
    }
    GrenadePhase Phase() const { return phase; }
private:
    GrenadeKind type = GrenadeKind::Frag;
    GrenadePhase phase = GrenadePhase::Absent;
    Hand owner = Hand::Right;
    Point pinStart;
};

// An action is measured in WEAPON-LOCAL coordinates: moving both hands together
// cannot cock the gun. Reaching the end of the outward stroke is not enough;
// the handle/pump must return before chambering completes.
class Stroke {
public:
    bool Begin(Point origin, Point direction, float distance) {
        const float n2 = direction.LengthSquared();
        if (!origin.Finite() || !direction.Finite() || !std::isfinite(n2) || n2 < 0.0001f ||
            !std::isfinite(distance) || distance < 0.01f || distance > 0.5f) return false;
        start = origin; axis = direction * (1.0f/std::sqrt(n2)); travel = distance;
        active = true; outward = false; return true;
    }
    bool Update(Point point, bool gripHeld) {
        if (!active) return false;
        if (!gripHeld || !point.Finite()) { active = false; return false; }
        const float d = (point-start).Dot(axis);
        if (!outward) { outward = d >= travel; return false; }
        if (d <= travel*0.2f && d >= -travel*0.5f) { active = false; return true; }
        return false;
    }
private:
    Point start, axis;
    float travel = 0;
    bool active = false, outward = false;
};

enum class ReloadKind { Magazine, Shotgun, Rockets, Native, Battery };
enum class PayloadKind { Magazine, Shell, Rocket };
struct Payload {
    PayloadKind kind = PayloadKind::Magazine;
    Object weapon = None;
    int rounds = 0;
    bool valid = false;
    std::uint64_t token = 0;
};
class Reload {
public:
    bool Initialize(Object weapon, ReloadKind kind, int capacity, int rounds, int reserve) {
        if (weapon == None || capacity < 1 || capacity > 10000 || rounds < 0 || rounds > capacity || reserve < 0) return false;
        id = weapon; type = kind; cap = capacity; loaded = rounds; spare = reserve;
        magazineIn = true; actionOpen = false; ready = rounds > 0;
        outstanding.clear(); return true;
    }
    bool CanFire() const { return id != None && loaded > 0 && ready && !actionOpen && (type != ReloadKind::Magazine || magazineIn); }
    bool ConfirmShot() {
        if (!CanFire()) return false;
        --loaded;
        if (loaded == 0 || type == ReloadKind::Shotgun) ready = false;
        return true;
    }
    Payload EjectMagazine() {
        if (id == None || type != ReloadKind::Magazine || !magazineIn) return {};
        Payload result = Issue(PayloadKind::Magazine, loaded);
        loaded = 0; magazineIn = false;
        // Preserve ready for a tactical reload; empty-before-eject remains false.
        return result;
    }
    Payload DrawSupply() {
        if (id == None || spare <= 0) return {};
        PayloadKind kind;
        int amount;
        if (type == ReloadKind::Magazine && !magazineIn) { kind = PayloadKind::Magazine; amount = (std::min)(cap, spare); }
        else if (type == ReloadKind::Shotgun && loaded < cap) { kind = PayloadKind::Shell; amount = 1; }
        else if (type == ReloadKind::Rockets && actionOpen && loaded < cap) { kind = PayloadKind::Rocket; amount = 1; }
        else return {};
        spare -= amount;
        return Issue(kind, amount);
    }
    bool Insert(Payload& payload) {
        if (!payload.valid || payload.weapon != id || payload.rounds < 0) return false;
        const auto issued = std::find_if(outstanding.begin(), outstanding.end(),
            [&](const Payload& p) { return p.token == payload.token && p.weapon == payload.weapon &&
                p.kind == payload.kind && p.rounds == payload.rounds; });
        if (issued == outstanding.end()) return false; // Stale/copied/replayed payload.
        const bool magazine = type == ReloadKind::Magazine && !magazineIn && payload.kind == PayloadKind::Magazine && payload.rounds <= cap;
        const bool shell = type == ReloadKind::Shotgun && payload.kind == PayloadKind::Shell && payload.rounds == 1 && loaded < cap;
        const bool rocket = type == ReloadKind::Rockets && actionOpen && payload.kind == PayloadKind::Rocket && payload.rounds == 1 && loaded < cap;
        if (!magazine && !shell && !rocket) return false;
        loaded += payload.rounds;
        if (magazine) magazineIn = true;
        outstanding.erase(issued);
        payload = {}; // Adapter clears the physical hand only after successful commit.
        return true;
    }
    bool CompleteRackOrPump() {
        if (id == None || loaded == 0 || actionOpen ||
            (type != ReloadKind::Magazine && type != ReloadKind::Shotgun) ||
            (type == ReloadKind::Magazine && !magazineIn)) return false;
        ready = true; return true;
    }
    bool OpenCasing() {
        if (id == None || type != ReloadKind::Rockets || actionOpen) return false;
        actionOpen = true; return true;
    }
    bool CloseCasing() {
        if (id == None || type != ReloadKind::Rockets || !actionOpen) return false;
        actionOpen = false; ready = loaded > 0; return true;
    }
    bool WantsNativeReload() const { return id != None && type == ReloadKind::Native; }
    int Loaded() const { return loaded; }
    int Reserve() const { return spare; }
    bool NeedsRack() const { return !ready && (type == ReloadKind::Magazine || type == ReloadKind::Shotgun); }
private:
    Payload Issue(PayloadKind kind, int rounds) {
        Payload result{kind, id, rounds, true, nextToken++};
        outstanding.push_back(result);
        return result;
    }
    std::vector<Payload> outstanding;
    std::uint64_t nextToken = 1;
    Object id = None;
    ReloadKind type = ReloadKind::Battery;
    int cap = 0, loaded = 0, spare = 0;
    bool magazineIn = true, actionOpen = false, ready = false;
};

enum class TriggerAction { None, FireWeapon, ArmPlasma, Interact };
inline TriggerAction ResolveTrigger(Hold hold, bool plasmaHeld, bool freshPress, bool interactionInReach) {
    if (plasmaHeld) return freshPress ? TriggerAction::ArmPlasma : TriggerAction::None;
    if (hold == Hold::TriggerGrip) return TriggerAction::FireWeapon;
    if (hold == Hold::Empty && freshPress && interactionInReach) return TriggerAction::Interact;
    return TriggerAction::None;
}

enum class GripAction { None, Retain, Manipulate, Flashlight, Pickup, Holster, Support };
inline GripAction ResolveGrip(bool alreadyHolding, bool manipulationLatched,
    bool headGesture, bool promptVisible, bool holsterEligible, bool supportEligible) {
    // Flashlight is additive for occupied hands: the adapter retains their item.
    if (manipulationLatched) return GripAction::Manipulate;
    if (headGesture) return GripAction::Flashlight;
    if (alreadyHolding) return GripAction::Retain;
    if (promptVisible) return GripAction::Pickup;
    if (holsterEligible) return GripAction::Holster;
    if (supportEligible) return GripAction::Support;
    return GripAction::None;
}
inline float RecoilScale(const Inventory& inventory, Object weapon,
    float oneHand = 1.0f, float twoHands = 0.35f) {
    if (!std::isfinite(oneHand)) oneHand = 1.0f;
    if (!std::isfinite(twoHands)) twoHands = 0.35f;
    oneHand = (std::max)(0.0f, (std::min)(oneHand, 5.0f));
    twoHands = (std::max)(0.0f, (std::min)(twoHands, oneHand));
    return inventory.Supported(weapon) ? twoHands : oneHand;
}
} // namespace Immersion
