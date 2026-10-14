#include "RetailWeaponEngine.h"
#include "RetailProfile.h"
#include "NativeModelPose.h"
#include "HandGripPose.h"
#include "../Game.h"
#include "../Helpers/Version.h"
#include "../Helpers/DX9.h"
#include "../DirectXWrappers/IDirect3DDevice9ExWrapper.h"
#include <cstring>
#include <MinHook.h>

namespace Immersion {
namespace {
constexpr std::uintptr_t Base = 0x400000;
// This adapter never uses unchecked Helpers::GetDynamicObject lookups.
bool Copy(std::uintptr_t address, void* target, std::size_t size) {
    if (address < 0x10000 || address > UINT32_MAX || size > UINT32_MAX-address) return false;
    SIZE_T done = 0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),target,size,&done)
        && done == size;
}
template<class T> bool ReadAt(std::uintptr_t address, T& target) { return Copy(address,&target,sizeof(target)); }
template<class T, std::size_t N> T Field(const std::array<unsigned char,N>& bytes, unsigned offset) {
    T value{}; if (offset+sizeof(T) <= N) std::memcpy(&value,bytes.data()+offset,sizeof(value)); return value;
}
bool Store(std::uintptr_t address, const void* data, std::size_t size) {
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD|PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)) ||
        address < reinterpret_cast<std::uintptr_t>(region.BaseAddress) ||
        size > region.RegionSize-(address-reinterpret_cast<std::uintptr_t>(region.BaseAddress))) return false;
    std::memcpy(reinterpret_cast<void*>(address),data,size); return true;
}
template<std::size_t N>
bool ObjectBytes(Object id, std::array<unsigned char,N>& bytes, std::uintptr_t& address, unsigned expectedType) {
    if (id == None || (id & 0xffffu) == 0xffffu) return false;
    std::uint32_t tablePtr = 0;
    std::array<unsigned char,0x38> table{};
    if (!ReadAt(0x8603b0,tablePtr) || !ReadAt(tablePtr,table)) return false;
    const auto capacity = Field<std::uint16_t>(table,0x20);
    if (!capacity || capacity > 8192 || Field<std::uint16_t>(table,0x22) != 12 || (id & 0xffffu) >= capacity) return false;
    const auto elements = Field<std::uint32_t>(table,0x34);
    std::array<unsigned char,12> entry{};
    if (!ReadAt(static_cast<std::uintptr_t>(elements)+(id & 0xffffu)*12,entry) ||
        Field<std::uint16_t>(entry,0) != (id >> 16) || Field<std::uint16_t>(entry,6) < N) return false;
    address = Field<std::uint32_t>(entry,8);
    return ReadAt(address,bytes) && Field<std::uint16_t>(bytes,0xb4) == expectedType;
}
bool UnitBytes(Object id, std::array<unsigned char,0x328>& bytes, std::uintptr_t& address) {
    return ObjectBytes(id,bytes,address,0);
}
bool WeaponBytes(Object id, std::array<unsigned char,0x2cc>& bytes, std::uintptr_t& address) {
    return ObjectBytes(id,bytes,address,2);
}
void RefreshViewModel(int player) {
    using Function=void(__cdecl*)(int);
    reinterpret_cast<Function>(0x493c60)(player);
}
void StartNativeReload(Object weapon) {
    // Same verified retail entry/ABI already used by Hooks::H_ReloadStart.
    // Go through that hook: only a synchronous explicit-button permit lets it in.
    using Function=void(__cdecl*)(Object,short,bool);
    reinterpret_cast<Function>(0x4c35b0)(weapon,0,false);
}
using ReadyFunction = void(__cdecl*)(Object,bool);
void ReadyWeapon(Object unit) { reinterpret_cast<ReadyFunction>(0x56d6e0)(unit,true); }
using DropFunction = bool(__cdecl*)(Object,bool);
using WorldItemFunction = void(__cdecl*)(Object,Object);
RetailWeaponEngine* pickupOwner = nullptr;
WorldItemFunction originalWorldItem = nullptr;
using PoseFunction = void(__cdecl*)();
using WeaponHUDFunction = void(__cdecl*)(void*);
PoseFunction originalPose = nullptr;
WeaponHUDFunction originalWeaponHUD = nullptr;
RetailWeaponEngine* visualOwner = nullptr;

void SetNativeVisibility(int player, bool visible) {
    const std::uintptr_t function = 0x493e50;
    const int state = visible ? 1 : 0;
    __asm {
        push edi
        mov edi, player
        push state
        call function
        add esp, 4
        pop edi
    }
}
void __cdecl OnPose() {
    if (visualOwner && visualOwner->HideNativeWeapon()) {
        // Native teardown removes first-person lights, particles and sounds.
        // Guard this specific no-argument pose entry, not the register-ABI
        // visibility function shared with startup and other game systems.
        SetNativeVisibility(0,false);
        return;
    }
    // Refresh at the pose boundary, after input has finished the inventory
    // transaction, before sampling the newly selected animation skeleton.
    if (visualOwner) visualOwner->SynchronizeViewModel(true);
    originalPose();
}
void __cdecl OnWeaponHUD(void* player) {
    if (visualOwner && visualOwner->HideNativeWeapon()) return;
    originalWeaponHUD(player);
}
void __cdecl OnWorldItem(Object player, Object item) {
    if (!pickupOwner || !pickupOwner->HandleWorldItem(player,item)) originalWorldItem(player,item);
}

bool MatchFingerprint(const CodeFingerprint& fingerprint) {
    std::array<unsigned char,1024> bytes{};
    if (fingerprint.length > bytes.size() || !Copy(Base+fingerprint.rva,bytes.data(),fingerprint.length)) return false;
    std::uint32_t hash = 2166136261u;
    for (unsigned i=0; i<fingerprint.length; ++i) hash = (hash^bytes[i])*16777619u;
    return hash == fingerprint.fnv1a;
}

// 0x4d6fc0: EAX=model, ECX=nodes, eleven stack arguments; owner is
// argument nine ([entry ESP+0x24]). Preserve all entry registers/flags and
// replace only ECX. No live object/tag data is modified.
void* originalModelDraw=nullptr;
bool modelDrawHookInstalled=false;
bool ModelDrawReady(){return modelDrawHookInstalled || MatchFingerprint(RetailModelDraw);}
const Transform* __cdecl PrepareVehicleModelDraw(Object model,const Transform* nodes,Object owner){
    if(!visualOwner || owner==None || Game::instance.interactions.VehicleBodyOwner()!=owner)return nodes;
    return visualOwner->VehicleBodyNodes(model,nodes,owner);
}
void __declspec(naked) OnModelDraw(){
    __asm {
        pushfd
        pushad
        mov edx, [esp+0x48]
        push edx
        push ecx
        push eax
        call PrepareVehicleModelDraw
        add esp, 12
        mov [esp+24], eax
        popad
        popfd
        jmp originalModelDraw
    }
}
bool InstallModelDrawGuard(){
    if(modelDrawHookInstalled)return true;
    if(!MatchFingerprint(RetailModelDraw))return false;
    void* entry=reinterpret_cast<void*>(Base+RetailModelDraw.rva);
    if(MH_CreateHook(entry,reinterpret_cast<void*>(&OnModelDraw),&originalModelDraw)!=MH_OK)return false;
    if(MH_EnableHook(entry)!=MH_OK){MH_RemoveHook(entry);originalModelDraw=nullptr;return false;}
    modelDrawHookInstalled=true;return true;
}

bool TagData(Object id, std::uint32_t group, std::uint32_t& data) {
    std::uint32_t tags=0; std::array<unsigned char,0x20> asset{};
    if (id==None || (id&0xffffu)==0xffffu || !ReadAt(0x87bc14,tags) ||
        !ReadAt(static_cast<std::uintptr_t>(tags)+(id&0xffffu)*0x20,asset) ||
        Field<Object>(asset,0xc)!=id || Field<std::uint32_t>(asset,0)!=group) return false;
    data=Field<std::uint32_t>(asset,0x14); return data>=0x10000;
}
bool ModelNodes(Object id, std::array<ModelNode,64>& nodes, unsigned& count) {
    std::uint32_t data=0, address=0; count=0;
    return TagData(id,0x6d6f6432u,data) && ReadAt(data+0xb8,count) && count>0 && count<=64 &&
        ReadAt(data+0xbc,address) && Copy(address,nodes.data(),count*sizeof(ModelNode)) &&
        ValidModelNodes(nodes.data(),count);
}
void* ObjectLighting(Object unit) {
    const std::uintptr_t function=0x50ea00;
    void* result;
    __asm {
        push esi
        mov esi, unit
        push 0x7f7fffff
        call function
        add esp, 4
        mov result, eax
        pop esi
    }
    return result;
}
void DrawNativeModel(Object model, const Transform* nodes, Object owner, std::uintptr_t body,
                     void* lighting, bool marker = false, bool grey = false) {
    const std::uintptr_t function=0x4d6fc0;
    const auto colors=body+0x1b8, functions=body+0x134;
    // First-person assets have a zero disappearance cutoff; world models need
    // a positive screen-size/detail metric or Halo can cull them (notably AR).
    const std::uint32_t detail = marker ? 0x7f7fffffu : 0u;
    auto* device = reinterpret_cast<IDirect3DDevice9ExWrapper*>(Helpers::GetDirect3DDevice9());
    const bool previousGrey = device->bGreyHolsterMarker;
    device->bGreyHolsterMarker = grey;
    // Same first-person model submission as 0x4926d5. Null cloak data uses
    // the engine default; materials/textures come from the user's map.
    __asm {
        push 8
        push 0
        push owner
        push 0
        push 0
        push 0x7c3114
        push lighting
        push functions
        push colors
        push 0
        push detail
        mov eax, model
        mov ecx, nodes
        call function
        add esp, 44
    }
    // This native path flushes its model submission before returning (0x4d7288).
    device->bGreyHolsterMarker = previousGrey;
}

// Nonstandard register conventions verified from this retail executable.
// Keep inline assembly confined to these small x86 wrappers.
void PositionObject(Object id, const Point* position, const Point* forward, const Point* up) {
    const std::uintptr_t function = 0x4f51c0;
    __asm {
        push edi
        mov edi, position
        push up
        push forward
        push id
        call function
        add esp, 12
        pop edi
    }
}
bool ValidatePosition(Object id, const Point* previous) {
    const std::uintptr_t function = 0x4f7b70;
    unsigned char result;
    __asm {
        push -1
        push id
        mov ecx, previous
        call function
        add esp, 8
        mov result, al
    }
    return result != 0;
}
bool AddWeapon(Object unit, Object item) {
    const std::uintptr_t function = 0x56d400;
    unsigned char result;
    __asm {
        push 1
        mov ecx, unit
        mov eax, item
        call function
        add esp, 4
        mov result, al
    }
    return result != 0;
}
bool CanUseWeapon(Object unit, Object item) {
    const std::uintptr_t function = 0x56da00;
    unsigned char result;
    __asm {
        push esi
        push edi
        mov esi, unit
        mov edi, item
        call function
        mov result, al
        pop edi
        pop esi
    }
    return result != 0;
}
void OfferPickup(Object player, Object item, int action) {
    const std::uintptr_t function = 0x478e00;
    __asm {
        push ebx
        push -1
        push action
        mov eax, player
        mov ebx, item
        call function
        add esp, 8
        pop ebx
    }
}
}

bool RetailWeaponEngine::CheckCode() const {
    for (const auto& fingerprint : RetailCode) if (!MatchFingerprint(fingerprint)) return false;
    return true;
}
void RetailWeaponEngine::CheckInteractionCode() {
    // Initialize runs AFTER HaloCEVR enables its hooks and crouch-camera patch.
    // Accept only the exact original OR our exact known patched camera routine.
    const bool attach=MatchFingerprint(RetailGrenadeAttach);
    const bool release=MatchFingerprint(RetailGrenadeRelease);
    const bool command=MatchFingerprint(RetailGrenadeCommand);
    const bool nodes=MatchFingerprint(RetailObjectNodes);
    const bool camera=MatchFingerprint(RetailSeatCamera);
    const bool vrCamera=MatchFingerprint({RetailSeatCamera.rva,RetailSeatCamera.length,RetailSeatCameraCrouchDisabledFNV});
    interactionAvailable=available && attach && release && command;
    vehicleAvailable=available && nodes && (camera || vrCamera);
    Logger::log << "[PhysicalInteractions] profile base=" << available
        << " grenade_attach=" << attach << " grenade_release=" << release << " grenade_command=" << command
        << " object_nodes=" << nodes << " seat_camera=" << camera << " seat_camera_vr=" << vrCamera << std::endl;
    Logger::log << "[PhysicalInteractions] grenades=" << interactionAvailable << " vehicles=" << vehicleAvailable << std::endl;
}
bool RetailWeaponEngine::Initialize() {
    available = false;
    interactionAvailable = vehicleAvailable = false;
    recoilAvailable = false;
    reloadAvailable = false;
    if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != Base ||
        std::strcmp(Helpers::GetGameTypeString(),"halor") != 0 ||
        std::strcmp(Helpers::GetVersionString(),"01.00.10.0621") != 0) return false;
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS32 pe{};
    if (!ReadAt(Base,dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000 ||
        !ReadAt(Base+dos.e_lfanew,pe) || pe.Signature != IMAGE_NT_SIGNATURE ||
        pe.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 || pe.FileHeader.TimeDateStamp != 1400006738u ||
        pe.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || pe.OptionalHeader.SizeOfImage != 0x491000) return false;
    available = CheckCode();
    CheckInteractionCode();
    recoilAvailable = available && MatchFingerprint(RetailFireMagazines) &&
        MatchFingerprint(RetailFireSuccess) && MatchFingerprint(RetailFireEnergy);
    if (available && !recoilAvailable) Logger::log << "[StylishRecoil] Firing observation profile differs; extra recoil disabled." << std::endl;
    reloadAvailable=available && MatchFingerprint(RetailReloadLayout);
    if (available && !reloadAvailable) Logger::log << "[ManualReload] Reload layout differs; native reload retained." << std::endl;
    return available;
}

bool RetailWeaponEngine::ReadNative(NativeInventory& out) const {
    out = {};
    readMayResume = false;
    auto fail = [&](const char* reason) { readStatus = reason; return false; };
    if (!available) return fail("Retail profile inactive: stock controls.");
    // The campaign has no multiplayer game-engine dispatch table. Require a
    // live local biped as well; the title screen also has a null game engine.
    std::uint32_t gameEngine = 1, playerTable = 0;
    if (!ReadAt(0x6f1d20,gameEngine)) return fail("Game mode unavailable: stock controls.");
    if (gameEngine != 0) return fail("Multiplayer: stock controls; physical inventory is campaign-only.");
    readMayResume = true;
    if (!ReadAt(0x87a480,playerTable)) return fail("Campaign player table unavailable: stock controls.");
    std::array<unsigned char,0x38> table{},player{};
    if (!ReadAt(playerTable,table) || Field<std::uint16_t>(table,0x22) != 0x200 ||
        !Field<std::uint16_t>(table,0x30) || !Field<std::uint16_t>(table,0x20) ||
        Field<std::uint16_t>(table,0x20) > 16) return fail("Campaign player table not ready or layout mismatch: stock controls.");
    const auto players = Field<std::uint32_t>(table,0x34);
    if (!ReadAt(players,player) || !Field<std::uint16_t>(player,0) || Field<std::uint16_t>(player,2) != 0) return fail("Local campaign player unavailable: stock controls.");
    const Object unit = Field<Object>(player,0x34);
    std::uintptr_t address = 0;
    std::array<unsigned char,0x328> body{};
    if (!UnitBytes(unit,body,address)) return fail("Campaign biped unavailable; waiting for player recovery.");
    if (!std::isfinite(Field<float>(body,0xe0))) return fail("Campaign vitality unreadable; interactions paused.");
    // Native death is an explicit damage flag, not a guessed health threshold.
    // A depleted vitality sample must not switch a still-controlled unit back
    // to the original grip/reload/throw bindings.
    if (Field<std::uint16_t>(body,0x106) & 4u) return fail("Campaign biped dead; waiting for checkpoint/respawn.");
    if (Field<Object>(body,0x11c) != None) return fail("Campaign biped parented; on-foot interactions paused.");
    if (Field<Object>(body,0x218) != (static_cast<Object>(Field<std::uint16_t>(player,0))<<16))
        return fail("Campaign player ownership changing; interactions paused.");
    const int current = Field<std::int16_t>(body,0x2f2);
    const int desired = Field<std::int16_t>(body,0x2f4);
    if (current < -1 || current >= 4 || desired < -1 || desired >= 4) return fail("Native weapon slot invalid: stock controls.");
    out.unit = unit; out.storage = static_cast<std::uint32_t>(address); out.age = Field<std::uint32_t>(body,0xc);
    out.player = static_cast<Object>(Field<std::uint16_t>(player,0))<<16;
    for (unsigned i=0; i<4; ++i) {
        out.weapons[i] = Field<Object>(body,0x2f8+i*4);
        if (out.weapons[i] == None) continue;
        std::array<unsigned char,0x2cc> weapon{}; std::uintptr_t weaponAddress = 0;
        if (!WeaponBytes(out.weapons[i],weapon,weaponAddress)) return fail("Native weapon object validation failed: stock controls.");
        for (unsigned j=0; j<i; ++j) if (out.weapons[i] == out.weapons[j]) return fail("Duplicate inventory object: stock controls.");
    }
    out.current = current < 0 ? None : out.weapons[current];
    if (current >= 0 && out.current == None) return fail("Native current weapon slot is empty: stock controls.");
    readStatus = "Campaign inventory verified.";
    return true;
}

bool RetailWeaponEngine::Read(NativeInventory& out) const {
    if (!ReadNative(out)) return false;
    if (out.unit == stowedUnit && out.storage == stowedStorage && out.age >= stowedAge)
        out.current = None;
    return true;
}

bool RetailWeaponEngine::ReadFireState(Object weapon, FireState& out) const {
    out = {};
    if (!recoilAvailable) return false;
    NativeInventory native;
    std::array<unsigned char,0x2d4> bytes{}; std::uintptr_t address=0;
    if (!Read(native) || native.current != weapon || weapon == None ||
        !ObjectBytes(weapon,bytes,address,2) || Field<Object>(bytes,0x11c) != native.unit) return false;
    out.unit=native.unit; out.weapon=weapon;
    out.tick=Field<std::uint32_t>(bytes,0x2d0);
    // Retail magazines are 12 bytes (not Custom Edition's 16).
    out.loaded={Field<std::int16_t>(bytes,0x2b8),Field<std::int16_t>(bytes,0x2c4)};
    out.heat=Field<float>(bytes,0x23c); out.energyUsed=Field<float>(bytes,0x240);
    return std::isfinite(out.heat) && std::isfinite(out.energyUsed);
}

bool RetailWeaponEngine::ReadReloadAmmo(Object weapon, ReloadAmmo& out, bool current) const {
    out={};NativeInventory native;std::uintptr_t address=0;
    std::array<unsigned char,0x2d4> object{};
    if (!reloadAvailable || !ReadNative(native) || !native.Contains(weapon) ||
        (current && (native.current!=weapon || HideNativeWeapon())) || !ObjectBytes(weapon,object,address,2)) return false;
    if (current && Field<Object>(object,0x11c)!=native.unit) return false;
    const Object tag=Field<Object>(object,0);
    std::uint32_t data=0,tags=0,name=0,magazines=0,count=0;
    if (!TagData(tag,0x77656170u,data) || !ReadAt(0x87bc14,tags) ||
        !ReadAt(static_cast<std::uintptr_t>(tags)+(tag&0xffffu)*0x20+0x10,name)) return false;
    std::array<char,256> path{};
    if (!Copy(name,path.data(),path.size())) {
        // A short valid tag path may finish immediately before an unreadable page.
        for (unsigned i=0;i<path.size();++i) {
            if (!ReadAt(static_cast<std::uintptr_t>(name)+i,path[i])) return false;
            if (!path[i]) break;
        }
    }
    if (!std::memchr(path.data(),0,path.size())) return false;
    out.kind=ReloadWeaponKindForPath(path.data());
    if (out.kind==ReloadWeaponKind::Native || !ReadAt(data+0x4f0,count) || count<1 || count>2 ||
        !ReadAt(data+0x4f4,magazines)) return false;
    std::int16_t capacity=0;
    if (!ReadAt(magazines+0xa,capacity)) return false;
    out.unit=native.unit;out.weapon=weapon;out.tag=tag;out.capacity=capacity;
    out.reserve=Field<std::int16_t>(object,0x2b6);out.loaded=Field<std::int16_t>(object,0x2b8);
    out.nativeState=Field<std::int16_t>(object,0x2b0);
    out.tick=Field<std::uint32_t>(object,0x2d0);
    return ValidReloadAmmo(out) && out.nativeState>=0 && out.nativeState<=3;
}

bool RetailWeaponEngine::StartButtonReload(Object weapon) {
    ReloadAmmo ammo;
    if(!ReadReloadAmmo(weapon,ammo) || ammo.kind!=ReloadWeaponKind::Needler ||
        ammo.loaded>=ammo.capacity || ammo.reserve<=0 || (ammo.nativeState!=0 && ammo.nativeState!=2))return false;
    StartNativeReload(weapon);
    return true;
}

bool RetailWeaponEngine::CommitReloadAmmo(const ReloadAmmo& before, const ReloadAmmo& after) {
    ReloadAmmo current;
    if (!ValidReloadAmmo(after) || !ReadReloadAmmo(before.weapon,current) ||
        current.unit!=before.unit || current.tag!=before.tag || current.kind!=before.kind ||
        current.capacity!=before.capacity || current.loaded!=before.loaded || current.reserve!=before.reserve ||
        after.unit!=before.unit || after.weapon!=before.weapon || after.tag!=before.tag ||
        after.kind!=before.kind || after.capacity!=before.capacity ||
        before.loaded+before.reserve!=after.loaded+after.reserve) return false;
    std::array<unsigned char,0x2cc> bytes{};std::uintptr_t address=0;
    if (!WeaponBytes(before.weapon,bytes,address) || Field<Object>(bytes,0)!=before.tag ||
        Field<Object>(bytes,0x11c)!=before.unit || Field<std::int16_t>(bytes,0x2b6)!=before.reserve ||
        Field<std::int16_t>(bytes,0x2b8)!=before.loaded) return false;
    const auto state=Field<std::int16_t>(bytes,0x2b0);
    if (state!=0 && state!=2) return false; // Never race an already-running native reload.
    const std::array<std::int16_t,2> oldAmmo{static_cast<std::int16_t>(before.reserve),static_cast<std::int16_t>(before.loaded)};
    const std::array<std::int16_t,2> newAmmo{static_cast<std::int16_t>(after.reserve),static_cast<std::int16_t>(after.loaded)};
    if (!Store(address+0x2b6,newAmmo.data(),sizeof(newAmmo))) return false;
    ReloadAmmo checked;
    if (ReadReloadAmmo(before.weapon,checked) && checked.unit==before.unit && checked.tag==before.tag &&
        checked.reserve==after.reserve && checked.loaded==after.loaded) return true;
    // Restore only the same still-owned allocation and still-unmodified values.
    std::uintptr_t rollback=0;
    if (WeaponBytes(before.weapon,bytes,rollback) && rollback==address &&
        Field<Object>(bytes,0)==before.tag && Field<Object>(bytes,0x11c)==before.unit &&
        Field<std::int16_t>(bytes,0x2b6)==after.reserve && Field<std::int16_t>(bytes,0x2b8)==after.loaded)
        Store(address+0x2b6,oldAmmo.data(),sizeof(oldAmmo));
    return false;
}

bool RetailWeaponEngine::ReadReloadModel(Object weapon, std::array<ModelNode,64>& nodes, unsigned& count) const {
    ReloadAmmo ammo;std::uint32_t data=0;Object model=None;
    return ReadReloadAmmo(weapon,ammo) && TagData(ammo.tag,0x77656170u,data) &&
        ReadAt(data+0x468,model) && ModelNodes(model,nodes,count);
}

bool RetailWeaponEngine::ReadReloadRocketModel(Object weapon,Object& model) const {
    model=None;ReloadAmmo ammo;std::uint32_t data=0,count=0,triggers=0,projectileData=0;
    Object projectile=None,candidate=None;
    // HEK Weapon.triggers: 0x4fc; WeaponTrigger.projectile datum: 0xa0;
    // Object.model datum: 0x34. See research/rocket-reload-R1.19.txt.
    if(!ReadReloadAmmo(weapon,ammo) || ammo.kind!=ReloadWeaponKind::Rocket ||
        !TagData(ammo.tag,0x77656170u,data) || !ReadAt(std::uintptr_t(data)+0x4fc,count) || count<1 || count>2 ||
        !ReadAt(std::uintptr_t(data)+0x500,triggers) || triggers<0x10000 ||
        !ReadAt(std::uintptr_t(triggers)+0xa0,projectile) || !TagData(projectile,0x70726f6au,projectileData) ||
        !ReadAt(std::uintptr_t(projectileData)+0x34,candidate))return false;
    std::array<ModelNode,64> nodes{};unsigned nodesCount=0;
    if(!ModelNodes(candidate,nodes,nodesCount))return false;
    model=candidate;return true;
}

bool RetailWeaponEngine::ReadReloadHandModel(Object weapon, std::array<ModelNode,64>& nodes, unsigned& count) const {
    count=0;ReloadAmmo ammo;
    std::uint32_t globals=0,firstPerson=0,entries=0;Object model=None;
    return ReadReloadAmmo(weapon,ammo) && ReadAt(0x746fa0,globals) && globals!=0 &&
        ReadAt(globals+0x17c,entries) && entries>0 && entries<=16 &&
        ReadAt(globals+0x180,firstPerson) && firstPerson!=0 && ReadAt(firstPerson+0xc,model) &&
        ModelNodes(model,nodes,count);
}

bool RetailWeaponEngine::ValidReloadFeedbackTag(Object weapon,Object tag,bool effect) const {
    ReloadAmmo ammo;std::uint32_t data=0;
    return ReadReloadAmmo(weapon,ammo) && (TagData(tag,effect?0x65666665u:0x736e6421u,data) ||
        (effect && TagData(tag,0x736e6421u,data)));
}
bool RetailWeaponEngine::ReadReloadFeedback(Object weapon,ReloadFeedbackAssets& assets) const {
    assets={};ReloadAmmo ammo;Object graph=None;std::uint32_t data=0,animations=0,count=0,sounds=0,soundCount=0;
    if(!ReadReloadAmmo(weapon,ammo) || !TagData(ammo.tag,0x77656170u,data))return false;
    std::uint32_t magazines=0;
    if(ReadAt(data+0x4f4,magazines)){
        Object effect=None;
        if(ReadAt(magazines+0x54,effect) && ValidReloadFeedbackTag(weapon,effect,true)){
            assets.chamberEffect=effect;std::uint32_t soundData=0;
            assets.chamberIsSound=TagData(effect,0x736e6421u,soundData);
        }
    }
    if(!ReadAt(data+0x478,graph) || !TagData(graph,0x616e7472u,data) ||
        !ReadAt(data+0x74,count) || count>1024 || !ReadAt(data+0x78,animations) ||
        !ReadAt(data+0x54,soundCount) || soundCount>256 || !ReadAt(data+0x58,sounds))return false;
    std::array<int,static_cast<unsigned>(ReloadCue::Count)> scores{};
    for(unsigned i=0;i<count;++i){
        std::array<unsigned char,0x40> animation{};
        if(!ReadAt(animations+i*0xb4,animation))return false;
        char name[33]{};std::memcpy(name,animation.data(),32);
        for(char& c:name)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
        const int index=Field<std::int16_t>(animation,0x3c);Object sound=None;std::uint32_t soundData=0;
        if(index<0 || static_cast<unsigned>(index)>=soundCount || !ReadAt(sounds+index*20+0xc,sound) ||
            !TagData(sound,0x736e6421u,soundData))continue;
        // Graph sound references and animation names are read from the current map.
        // Never guess datum IDs or borrow a different gun's assets.
        const bool reload=std::strstr(name,"reload")!=nullptr,ready=std::strstr(name,"ready")!=nullptr;
        const bool chamber=std::strstr(name,"chamber") || std::strstr(name,"pump") || std::strstr(name,"cock");
        const bool fire=std::strstr(name,"fire") && !std::strstr(name,"misfire");
        if(ammo.kind==ReloadWeaponKind::Shotgun && (fire || chamber) && !assets.IsPumpSound(sound) &&
            assets.pumpSoundCount<assets.pumpSounds.size())assets.pumpSounds[assets.pumpSoundCount++]=sound;
        auto choose=[&](ReloadCue cue,int score,unsigned part=0,unsigned parts=1){
            const auto n=static_cast<unsigned>(cue);if(score>scores[n]){
                scores[n]=score;assets.sounds[n]=sound;assets.slices[n]={part,parts};
            }
        };
        if(reload && MagazineReload(ammo.kind)){
            const unsigned parts=ammo.kind==ReloadWeaponKind::Rifle?3u:2u;
            const int reloadScore=std::strstr(name,"reload-full")?20:10;
            choose(ReloadCue::Eject,reloadScore,0,parts);choose(ReloadCue::Insert,reloadScore,1,parts);
            if(ammo.kind==ReloadWeaponKind::Rifle)choose(ReloadCue::Pull,10,2,3);
        }
        if(reload && ammo.kind==ReloadWeaponKind::Rocket){
            const int score=std::strstr(name,"reload-empty")?30:std::strstr(name,"reload-full")?20:10;
            choose(ReloadCue::Pull,score,0,4);choose(ReloadCue::Eject,score,1,4);
            choose(ReloadCue::Insert,score,2,4);choose(ReloadCue::Close,score,3,4);
        }
        if(ready && ammo.kind==ReloadWeaponKind::Pistol){
            choose(ReloadCue::Pull,10,0,2);choose(ReloadCue::Close,10,1,2);
        }else if(ready && ammo.kind==ReloadWeaponKind::Sniper)choose(ReloadCue::Pull,10);
        if(ammo.kind==ReloadWeaponKind::Shotgun){
            if(reload){
                const bool loop=std::strstr(name,"loop") || std::strstr(name,"insert");
                choose(ReloadCue::Insert,loop?30:std::strstr(name,"reload-full")?20:std::strstr(name,"reload-empty")?5:10);
            }
            if(chamber || fire){choose(ReloadCue::Pull,20,0,2);choose(ReloadCue::Close,20,1,2);}
        }else if(chamber)choose(ReloadCue::Pull,20);
        // A custom map may supply genuinely separate step clips.
        if(std::strstr(name,"insert") || std::strstr(name,"mag-in"))choose(ReloadCue::Insert,40);
        if(std::strstr(name,"eject") || std::strstr(name,"mag-out"))choose(ReloadCue::Eject,40);
        if(std::strstr(name,"close"))choose(ReloadCue::Close,40);
        Logger::log<<"[ManualReloadAudio] weapon="<<weapon<<" animation="<<name<<" sound="<<sound<<std::endl;
    }
    if(ammo.kind==ReloadWeaponKind::Shotgun && assets.chamberIsSound){
        assets.sounds[static_cast<unsigned>(ReloadCue::Pull)]=assets.chamberEffect;
        assets.sounds[static_cast<unsigned>(ReloadCue::Close)]=assets.chamberEffect;
        assets.slices[static_cast<unsigned>(ReloadCue::Pull)]={0,2};
        assets.slices[static_cast<unsigned>(ReloadCue::Close)]={1,2};
    }
    return true;
}

// Copy only the engine's decoded PCM cache. No map/tag writes, extraction to
// disk, compressed-data guessing, or borrowed sound datum IDs.
bool RetailWeaponEngine::ReadReloadPcm(Object weapon,Object sound,ReloadPcm& pcm) const {
    pcm={};ReloadAmmo ammo;std::uint32_t data=0,count=0,ranges=0;
    if(!ReadReloadAmmo(weapon,ammo) || !TagData(sound,0x736e6421u,data))return false;
    std::uint16_t rate=0,encoding=0;
    if(!ReadAt(data+6,rate) || rate>1 || !ReadAt(data+0x6c,encoding) || encoding>1 ||
        !ReadAt(data+0x98,count) || count==0 || count>8 || !ReadAt(data+0x9c,ranges) || !ranges)return false;
    for(unsigned r=0;r<count;++r){
        std::uint32_t n=0,perms=0;
        if(!ReadAt(ranges+r*0x48+0x3c,n) || n>256 || !ReadAt(ranges+r*0x48+0x40,perms))return false;
        for(unsigned i=0;i<n;++i){
            std::array<unsigned char,0x54> perm{};const auto address=perms+i*0x7c;
            if(!ReadAt(address,perm))return false;
            // Stock weapon FX are Xbox ADPCM; Halo PC expands this to signed
            // little-endian PCM16 at +30, with decoded byte length at +38.
            // Other codecs fail closed instead of treating Ogg/compressed bytes as PCM.
            if(Field<std::uint16_t>(perm,0x28)!=1 || Field<std::int16_t>(perm,0x2a)!=-1)continue;
            const auto cache=Field<std::uint32_t>(perm,0x30),bytes=Field<std::uint32_t>(perm,0x38);
            const unsigned channels=encoding+1,sampleRate=rate?44100:22050;
            if(!cache || bytes<channels*2 || bytes>sampleRate*channels*2*8 || bytes%(channels*2))continue;
            ReloadPcm candidate;candidate.rate=sampleRate;candidate.channels=channels;candidate.samples.resize(bytes/2);
            if(!Copy(cache,candidate.samples.data(),bytes))continue;
            // The decoder can fill an allocation in place without changing its
            // header. Reject a torn snapshot, even when pointer/size still match.
            // Two bulk reads avoid hundreds of small process-memory calls on
            // a VR input frame. Both allocations share the eight-second bound.
            std::vector<std::int16_t> verify(candidate.samples.size());
            if(!Copy(cache,verify.data(),bytes) || verify!=candidate.samples)return false;
            std::array<unsigned char,0x54> checked{};ReloadAmmo current;
            if(!ReadAt(address,checked) || checked!=perm || !ReadReloadAmmo(weapon,current) || current.unit!=ammo.unit ||
                current.tag!=ammo.tag || !ValidReloadFeedbackTag(weapon,sound))return false;
            pcm=std::move(candidate);return true;
        }
    }
    return false;
}

bool RetailWeaponEngine::HideNativeWeapon() const {
    NativeInventory native;
    return pickupEnabled && ReadNative(native) && native.unit == stowedUnit &&
        native.storage == stowedStorage && native.age >= stowedAge;
}

namespace { bool LocalBiped(Object&,std::array<unsigned char,0x328>&,std::uintptr_t&); }
std::uint32_t RetailWeaponEngine::RecordLook(Point forward, bool valid, float elapsed,float stickX,float stickY) {
    Object unit=None,controlled=None;std::uintptr_t address=0;std::array<unsigned char,0x328> body{};
    std::uint32_t data = 0, flags = 0;
    // Cryotube attachment and weapon slots are irrelevant to look history.
    // Still require the verified executable and live local campaign ownership.
    if (!available || !valid || !LocalBiped(unit,body,address) || !ReadAt(0x6b145c,data) ||
        !ReadAt(data,flags) || !ReadAt(data+0x10,controlled) || controlled!=unit) {
        lookHistory.Reset(); lookUnit = None; lookData = 0; previousLookFlags = 0; return 0;
    }
    if (lookUnit != unit || lookData != data || (previousLookFlags != 0 && flags == 0)) lookHistory.Reset();
    lookUnit = unit; lookData = data; previousLookFlags = flags;
    const auto headset=lookHistory.Update(forward,true,elapsed);
    const auto observed = (headset|lookHistory.UpdateStick(stickX,stickY,true,elapsed)) & 0x780u;
    const auto added = observed & ~flags;
    if (!added || !CheckCode()) return 0;
    // Preserve other action flags and the game's own resets. No script return,
    // control lock, or tutorial completion variable is patched.
    const auto merged = flags | added;
    if (!Store(data,&merged,sizeof(merged))) return 0;
    previousLookFlags = merged;
    return added;
}

bool RetailWeaponEngine::Select(Object unit, Object weapon) {
    NativeInventory before;
    if (!ReadNative(before) || before.unit != unit || (weapon != None && !before.Contains(weapon)) || !CheckCode()) return false;
    // Native ready(-1) is not a durable holster state: Halo's next update selects
    // an inventory weapon again. Keep the native slot stable, hide its rendering
    // and effects, and expose an empty hand to the physical ownership model.
    if (weapon == None) {
        stowedUnit = before.unit; stowedStorage = before.storage; stowedAge = before.age;
        viewModelRefreshPending = false;
        return true;
    }
    std::array<unsigned char,0x328> body{}; std::uintptr_t address = 0;
    if (!UnitBytes(unit,body,address)) return false;
    const auto slot = static_cast<std::int16_t>(std::find(before.weapons.begin(),before.weapons.end(),weapon)-before.weapons.begin());
    const auto previousDesired = Field<std::int16_t>(body,0x2f4);
    // The unit slot alone is transient. Retail player_control_update (0x471ae0)
    // keeps its own selection and copies it into every new player action. A
    // valid old slot wins over our unit write on the next simulation tick.
    // Bind the command to the same owned biped before changing either slot.
    std::uint32_t controls=0;
    std::array<unsigned char,0x50> control{};
    if (!ReadAt(0x6b145c,controls) || !ReadAt(controls,control) ||
        Field<Object>(control,0x10)!=unit) return false;
    const auto previousCommand=Field<std::int16_t>(control,0x30);
    if (previousCommand < -1 || previousCommand >= 4 ||
        !Store(controls+0x30,&slot,sizeof(slot))) return false;
    if (!Store(address+0x2f4,&slot,sizeof(slot))) {
        Store(controls+0x30,&previousCommand,sizeof(previousCommand));
        return false;
    }
    if (before.current != weapon) ReadyWeapon(unit);
    NativeInventory after;
    if (ReadNative(after) && after.unit == unit && after.weapons == before.weapons && after.current == weapon) {
        // An equal FP datum does not prove its remap/animation state is current.
        // Also refresh when drawing the same native gun after virtual stowing.
        viewModelRefreshPending = viewModelRefreshPending || before.current != weapon || stowedUnit == unit;
        stowedUnit = None;
        if (previousCommand!=slot)
            Logger::log << "[PhysicalWeapons] selection command " << previousCommand << " -> " << slot
                << " object=" << weapon << std::endl;
        return true;
    }
    if (UnitBytes(unit,body,address)) Store(address+0x2f4,&previousDesired,sizeof(previousDesired));
    // Never restore into a control block that has been replaced/rebound.
    std::uint32_t currentControls=0; Object currentUnit=None;
    if (ReadAt(0x6b145c,currentControls) && currentControls==controls &&
        ReadAt(controls+0x10,currentUnit) && currentUnit==unit)
        Store(controls+0x30,&previousCommand,sizeof(previousCommand));
    return false;
}

bool RetailWeaponEngine::SynchronizeViewModel(bool atPose) {
    if (!pickupEnabled) return true;
    NativeInventory native;
    std::uint32_t globals=0;
    std::array<unsigned char,12> fp{};
    if (!ReadNative(native) || !ReadAt(0x6b2d98,globals) || !ReadAt(globals,fp) ||
        Field<Object>(fp,4) != native.unit) return false;
    // A stowed weapon retains its native model identity. The pose guard performs
    // the native visibility teardown without clearing/remapping the skeleton.
    if (HideNativeWeapon()) return true;
    const Object old = Field<Object>(fp,8);
    if (old == native.current && !viewModelRefreshPending) return true;
    viewModelRefreshPending = true;
    if (!atPose) return false;
    if (!MatchFingerprint(RetailViewModelRefresh)) return false;
    RefreshViewModel(0);
    if (!ReadAt(globals,fp) || Field<Object>(fp,4) != native.unit ||
        Field<Object>(fp,8) != native.current) return false;
    viewModelRefreshPending = false;
    Logger::log << "[PhysicalVisuals] pose-boundary model reset " << old << " -> " << native.current << std::endl;
    return true;
}

bool RetailWeaponEngine::ViewModelReady(Object held) const {
    NativeInventory native;
    std::uint32_t globals=0;
    std::array<unsigned char,12> fp{};
    return !viewModelRefreshPending && held != None && Read(native) && native.current == held &&
        ReadAt(0x6b2d98,globals) && ReadAt(globals,fp) &&
        Field<Object>(fp,4) == native.unit && Field<Object>(fp,8) == held;
}

bool RetailWeaponEngine::ViewModelWeapon(Object& weapon, Object& tag, Object& animations) const {
    weapon=tag=animations=None;
    NativeInventory native;
    std::array<unsigned char,0x2cc> object{};
    std::uintptr_t address=0;
    if (!Read(native) || !ViewModelReady(native.current) ||
        !WeaponBytes(native.current,object,address)) return false;
    const Object id=Field<Object>(object,0);
    std::uint32_t tags=0;
    std::array<unsigned char,0x20> asset{};
    if (id == None || !ReadAt(0x87bc14,tags) ||
        !ReadAt(static_cast<std::uintptr_t>(tags)+(id & 0xffffu)*0x20,asset) ||
        Field<Object>(asset,0xc) != id || Field<std::uint32_t>(asset,0) != 0x77656170u) return false;
    const auto data=Field<std::uint32_t>(asset,0x14);
    if (!ReadAt(static_cast<std::uintptr_t>(data)+0x478,animations) || animations == None) return false;
    weapon=native.current; tag=id; return true;
}

bool RetailWeaponEngine::DrawBodyModels(bool hands, const std::array<HolsterMarker,2>& markers,
    const Matrix4* handPoses, const Matrix4* markerPoses) const {
    NativeInventory native;
    std::int16_t renderPlayer=-1;
    if (!visualGuardsInstalled || !ReadNative(native) || !ReadAt(0x7c3108,renderPlayer) || renderPlayer!=0 ||
        !ModelDrawReady() || !MatchFingerprint(RetailObjectLighting)) return false;
    std::array<unsigned char,0x328> body{}; std::uintptr_t bodyAddress=0;
    if (!UnitBytes(native.unit,body,bodyAddress)) return false;
    std::array<ModelNode,64> nodes{};
    std::array<Transform,64> transforms{};
    unsigned count=0;
    bool drawn[2]{}, handsDrawn=false;
    void* lighting=nullptr;
    if (hands && handPoses) {
        std::uint32_t globals=0, firstPerson=0, entries=0;
        Object model=None;
        if (ReadAt(0x746fa0,globals) && ReadAt(globals+0x17c,entries) && entries>0 &&
            ReadAt(globals+0x180,firstPerson) && ReadAt(firstPerson+0xc,model) &&
            ModelNodes(model,nodes,count) && PlaceChiefHands(nodes.data(),count,handPoses,transforms.data())) {
            lighting=ObjectLighting(native.unit);
            if (lighting) { DrawNativeModel(model,transforms.data(),native.unit,bodyAddress,lighting); handsDrawn=true; }
        }
    }
    for (unsigned home=0; markerPoses && home<2; ++home) {
        const auto& marker=markers[home];
        if (!native.Contains(marker.weapon)) continue;
        std::array<unsigned char,0x2cc> weapon{}; std::uintptr_t address=0;
        std::uint32_t data=0; Object model=None;
        if (WeaponBytes(marker.weapon,weapon,address) && TagData(Field<Object>(weapon,0),0x77656170u,data) &&
            ReadAt(data+0x34,model) && ModelNodes(model,nodes,count)) {
            std::uint32_t tags=0, path=0;
            char name[256]{};
            if (ReadAt(0x87bc14,tags) && ReadAt(tags+(Field<Object>(weapon,0)&0xffffu)*0x20+0x10,path)) {
                // Bounded byte reads also work when a short path ends at a page boundary.
                for (unsigned i=0;i<sizeof(name)-1 && path>=0x10000;++i)
                    if (!ReadAt(path+i,name[i]) || name[i]=='\0') break;
            }
            Matrix4 size; size.scale(HolsterMarkerScale(name,home));
            if (!PlaceModel(nodes.data(),count,markerPoses[home]*size,transforms.data())) continue;
            if (!lighting) lighting=ObjectLighting(native.unit);
            if (lighting) {
                DrawNativeModel(model,transforms.data(),marker.weapon,bodyAddress,lighting,true,marker.equipped);
                drawn[home]=true;
            }
        }
    }
    const std::array<Object,2> ids{markers[0].weapon,markers[1].weapon};
    const unsigned status=(hands?1u:0u)|(handsDrawn?2u:0u)|(drawn[0]?4u:0u)|(drawn[1]?8u:0u)|
        (markers[0].equipped?16u:0u)|(markers[1].equipped?32u:0u);
    if (status!=lastBodyStatus || ids!=lastMarkerObjects) {
        lastBodyStatus=status;
        lastMarkerObjects=ids;
        Logger::log << "[PhysicalVisuals] native body models: hands=" << hands << "/" << handsDrawn
            << " shoulder=" << ids[0] << "/" << drawn[0] << "/grey=" << markers[0].equipped
            << " hip=" << ids[1] << "/" << drawn[1] << "/grey=" << markers[1].equipped << std::endl;
    }
    return drawn[0] || drawn[1];
}

bool RetailWeaponEngine::DrawReloadRocket(Object weapon,const Matrix4& pose,bool spent) const {
    Object model=None;NativeInventory inventory;std::int16_t renderPlayer=-1;
    if(!visualGuardsInstalled || !ReadNative(inventory) || inventory.current!=weapon ||
        !ReadAt(0x7c3108,renderPlayer) || renderPlayer!=0 ||
        !ModelDrawReady() || !MatchFingerprint(RetailObjectLighting) ||
        !ReadReloadRocketModel(weapon,model))return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(pose[i]))return false;
    std::array<unsigned char,0x328> body{};std::uintptr_t address=0;
    std::array<ModelNode,64> nodes{};std::array<Transform,64> transforms{};unsigned count=0;
    if(!UnitBytes(inventory.unit,body,address) || !ModelNodes(model,nodes,count) ||
        !PlaceModel(nodes.data(),count,pose,transforms.data()))return false;
    void* lighting=ObjectLighting(inventory.unit);if(!lighting)return false;
    // Render geometry/materials only: never spawn a live projectile, trail or effect.
    DrawNativeModel(model,transforms.data(),weapon,address,lighting,true,spent);return true;
}

NativeDrop RetailWeaponEngine::Drop(Object unit, Object weapon, const DropPose& pose) {
    NativeInventory before;
    if (!pose.position.Finite() || !pose.forward.Finite() || !pose.up.Finite() || !pose.velocity.Finite() ||
        !Read(before) || before.unit != unit || before.current != weapon || weapon == None || !CheckCode()) return NativeDrop::Rejected;
    std::array<unsigned char,0x328> body{}; std::uintptr_t address = 0;
    if (!UnitBytes(unit,body,address)) return NativeDrop::Rejected;
    const Point playerVelocity = Field<Point>(body,0x68);
    const bool reportedSuccess = reinterpret_cast<DropFunction>(0x56dec0)(unit,true);
    NativeInventory after;
    if (!Read(after) || after.unit != unit || after.Contains(weapon)) return NativeDrop::Rejected;
    // Even a native failure code must not resurrect an object already removed.
    (void)reportedSuccess;
    Select(unit,None);
    std::array<unsigned char,0x2cc> item{};
    if (!WeaponBytes(weapon,item,address) || Field<Object>(item,0x11c) != None) return NativeDrop::RemovedWithoutWorldObject;
    const Point previous = Field<Point>(item,0x5c);
    const float maximumReach = Game::instance.MetresToWorld(3.0f);
    if (Within(previous,pose.position,maximumReach) && pose.forward.LengthSquared() > 0.5f && pose.up.LengthSquared() > 0.5f) {
        PositionObject(weapon,&pose.position,&pose.forward,&pose.up);
        if (!ValidatePosition(weapon,&previous)) PositionObject(weapon,&previous,nullptr,nullptr);
    }
    // SteamVR velocity was converted to Halo world units/second by IVR.
    // Halo physics stores displacement per fixed 30 Hz simulation tick.
    Point velocity = pose.velocity;
    const float maxSpeed = Game::instance.MetresToWorld(8.0f);
    const float speed2 = velocity.LengthSquared();
    if (!std::isfinite(speed2)) velocity = {};
    else if (speed2 > maxSpeed*maxSpeed) velocity = velocity*(maxSpeed/std::sqrt(speed2));
    velocity = velocity*(1.0f/30.0f) + (playerVelocity.Finite() ? playerVelocity : Point{});
    if (WeaponBytes(weapon,item,address)) {
        Store(address+0x68,&velocity,sizeof(velocity));
        return NativeDrop::WorldObject;
    }
    return NativeDrop::RemovedWithoutWorldObject;
}

bool RetailWeaponEngine::InstallPickupGuard() {
    if (pickupGuardInstalled) return true;
    if (!available || !CheckCode() || !MatchFingerprint(RetailPickupEntry)) return false;
    void* entry = reinterpret_cast<void*>(Base+RetailPickupEntry.rva);
    if (MH_CreateHook(entry,reinterpret_cast<void*>(&OnWorldItem),reinterpret_cast<void**>(&originalWorldItem)) != MH_OK) return false;
    pickupOwner = this;
    if (MH_EnableHook(entry) != MH_OK) {
        pickupOwner = nullptr; MH_RemoveHook(entry); originalWorldItem = nullptr; return false;
    }
    pickupGuardInstalled = true;
    return true;
}

bool RetailWeaponEngine::InstallVisualGuards() {
    if (visualGuardsInstalled) return true;
    if (!available || !MatchFingerprint(RetailPoseEntry) ||
        !MatchFingerprint(RetailWeaponHUDEntry) || !MatchFingerprint(RetailVisibilityEntry)) return false;
    void* pose = reinterpret_cast<void*>(0x492430);
    void* hud = reinterpret_cast<void*>(0x4b1e20);
    if (MH_CreateHook(pose,reinterpret_cast<void*>(&OnPose),reinterpret_cast<void**>(&originalPose)) != MH_OK) return false;
    if (MH_CreateHook(hud,reinterpret_cast<void*>(&OnWeaponHUD),reinterpret_cast<void**>(&originalWeaponHUD)) != MH_OK) {
        MH_RemoveHook(pose); return false;
    }
    visualOwner = this;
    if (MH_EnableHook(pose) != MH_OK || MH_EnableHook(hud) != MH_OK) {
        MH_DisableHook(pose); MH_DisableHook(hud);
        MH_RemoveHook(pose); MH_RemoveHook(hud); visualOwner = nullptr; return false;
    }
    visualGuardsInstalled = true;
    const bool bodyGuard=InstallModelDrawGuard();
    Logger::log<<"[PhysicalVehicles] native visibility restored; experimental PVS override removed"<<std::endl;
    vehicleAvailable=vehicleAvailable && bodyGuard;
    Logger::log << "[PhysicalVehicles] native head-mask render guard=" << bodyGuard << std::endl;
    Logger::log << "[PhysicalVisuals] native body renderer profile="
        << (ModelDrawReady() && MatchFingerprint(RetailObjectLighting)) << std::endl;
    Logger::log << "[PhysicalVisuals] stable native model / stowed-effect / weapon-HUD guards active" << std::endl;
    return true;
}

bool RetailWeaponEngine::NewLooseWeapon(const NativeInventory& player, Object item) const {
    if (player.Contains(item)) return false;
    std::array<unsigned char,0x2cc> weapon{}; std::uintptr_t address = 0;
    if (!WeaponBytes(item,weapon,address) || Field<Object>(weapon,0x11c) != None ||
        Field<Object>(weapon,0x200) == player.unit || !(Field<std::uint32_t>(weapon,0x10) & 0x800u)) return false;
    const auto tag = Field<Object>(weapon,0);
    if (tag == None) return false;
    for (Object id : player.weapons) {
        if (id == None) continue;
        if (!WeaponBytes(id,weapon,address)) return false;
        // Same-type ammunition scavenging remains native; it does not acquire
        // another inventory weapon. Grenades/equipment never enter this path.
        if (Field<Object>(weapon,0) == tag) return false;
    }
    return true;
}

bool RetailWeaponEngine::HandleWorldItem(Object player, Object item) {
    NativeInventory now;
    if (!pickupEnabled || !Read(now) || now.player != player || !NewLooseWeapon(now,item)) return false;
    const auto count = std::count_if(now.weapons.begin(),now.weapons.end(),[](Object id) { return id != None; });
    if (count > 2) return false;
    // This hook is ONLY the native player/world-item contact handler. Direct
    // mission grants still call Halo's unchanged add-to-inventory routine.
    // Offer the actual native prompt even when no gun is selected: the stock
    // swap test requires a selected gun, which physical holstering removes.
    if (CheckCode() && CanUseWeapon(now.unit,item)) OfferPickup(player,item,count < 2 ? 7 : 6);
    return true;
}

Object RetailWeaponEngine::PickupTarget() const {
    NativeInventory now;
    if (!pickupEnabled || !Read(now)) return None;
    std::uint32_t table = 0, players = 0;
    std::array<unsigned char,0x38> player{};
    if (!ReadAt(0x87a480,table) || !ReadAt(table+0x34,players) || !ReadAt(players,player) ||
        (static_cast<Object>(Field<std::uint16_t>(player,0))<<16) != now.player ||
        Field<Object>(player,0x34) != now.unit) return None;
    const auto action = Field<std::int16_t>(player,0x28);
    const Object item = Field<Object>(player,0x24);
    return (action == 6 || action == 7) && NewLooseWeapon(now,item) ? item : None;
}

bool RetailWeaponEngine::Pickup(Object unit, Object weapon) {
    NativeInventory before;
    if (weapon == None || PickupTarget() != weapon || !Read(before) || before.unit != unit ||
        before.current != None || !CheckCode() ||
        std::count_if(before.weapons.begin(),before.weapons.end(),[](Object id) { return id != None; }) >= 2) return false;
    // Mode 1 takes a free native slot. Never use the mode that drops/replaces
    // other weapons; two occupied homes require a deliberate drop first.
    const bool result = AddWeapon(unit,weapon);
    NativeInventory after;
    if (!Read(after) || after.unit != unit || after.storage != before.storage || !after.Contains(weapon)) return false;
    for (Object id : before.weapons) if (id != None && !after.Contains(id)) return false;
    // Native state is authoritative even if a routine reports failure after
    // committing. A failed selection is reconciled and safely stowed next tick.
    (void)result;
    return Select(unit,weapon);
}
#include "RetailInteractions.inl"
#include "RetailAccuracy.inl"

}
