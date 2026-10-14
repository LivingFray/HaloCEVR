// The real production Copy/Field/ObjectBytes/Read code is inserted by run.py.
// Synthetic memory tests reject stale IDs and malformed layouts without Halo.
#include "HaloCEVR/Immersion/WeaponSession.h"
#include "HaloCEVR/Immersion/LookHistory.h"
#include "HaloCEVR/Immersion/Recoil.h"
#include "HaloCEVR/Immersion/ManualReload.h"
#include "HaloCEVR/Immersion/ReloadCue.h"
#include "HaloCEVR/Immersion/HolsterMarker.h"
#include "HaloCEVR/Immersion/NativeModelPose.h"
#include "HaloCEVR/Immersion/PhysicalInteractions.h"
#include "HaloCEVR/Immersion/ARAccuracy.h"
#include "HaloCEVR/Immersion/HandGripPose.h"
#include <map>
#define __cdecl
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include "HaloCEVR/Immersion/RetailProfile.h"
namespace Logger {std::ostringstream log;}
using SIZE_T=std::size_t;
static std::map<std::uintptr_t,std::vector<unsigned char>> memory;
static std::uintptr_t mutateAfterCopy=0;
void* GetCurrentProcess(){return nullptr;}
bool ReadProcessMemory(void*,const void* source,void* target,std::size_t size,SIZE_T* done){
    const auto address=reinterpret_cast<std::uintptr_t>(source);*done=0;
    for(auto& region:memory)if(address>=region.first && address-region.first<=region.second.size() &&
        size<=region.second.size()-(address-region.first)){
        std::memcpy(target,region.second.data()+address-region.first,size);*done=size;
        if(address==mutateAfterCopy && size){region.second[address-region.first+size-1]^=0x40;mutateAfterCopy=0;}
        return true;
    }
    return false;
}
template<class T>void put(std::uintptr_t address,T value){
    for(auto& region:memory)if(address>=region.first && address-region.first+sizeof(T)<=region.second.size()){
        std::memcpy(region.second.data()+address-region.first,&value,sizeof(value));return;
    }
    std::abort();
}
struct Game {
    static Game instance;
    struct Interactions{Immersion::Object owner=Immersion::None;Immersion::Object VehicleBodyOwner()const{return owner;}} interactions;
};
Game Game::instance;
namespace Immersion {
class RetailWeaponEngine {
public:
    std::vector<ARSpreadPatch> accuracyShots;Object accuracyReportedTag=None;
    bool ReadARSpread(Object,ARSpreadPatch&,float)const;
    bool WriteARSpread(ARSpreadPatch&,bool);
    void BeginAccuracyShot(Object,short,float);
    void EndAccuracyShot();
    float AccuracyReticleScale(float)const;
    bool interactionAvailable=true,vehicleAvailable=true;
    mutable const char* grenadeReadStatus="",*vehicleReadStatus="";
    const Transform* VehicleBodyNodes(Object,const Transform*,Object)const;
    bool ReadGrenades(GrenadeInventory&)const;
    bool SelectGrenade(Object,unsigned);
    Object AttachedGrenade(Object)const;
    bool FinishGrenade(Object,const TrackedGrenadeRelease&,float,float);
    bool ReadVehicle(VehicleState&)const;
    bool IsScorpionDriver()const;
    bool DrawOnFootBody()const;
    bool DrawGrenade(unsigned,const Matrix4&,bool)const;
    bool DrawInteractionHands(const Matrix4*,unsigned,unsigned,Matrix4* =nullptr)const;
    bool available=true,codeValid=true,pickupEnabled=true;
    bool recoilAvailable=true;
    bool reloadAvailable=true;
    mutable bool readMayResume=false;
    mutable const char* readStatus="";
    LookHistory lookHistory;
    Object lookUnit=None;
    std::uint32_t lookData=0,previousLookFlags=0;
    bool Read(NativeInventory&)const;
    bool ReadNative(NativeInventory&)const;
    bool ReadFireState(Object,FireState&)const;
    bool ReadReloadAmmo(Object,ReloadAmmo&,bool current=true)const;
    bool StartButtonReload(Object);
    bool CommitReloadAmmo(const ReloadAmmo&,const ReloadAmmo&);
    bool ReadReloadModel(Object,std::array<ModelNode,64>&,unsigned&)const;
    bool ReadReloadRocketModel(Object,Object&)const;
    bool DrawReloadRocket(Object,const Matrix4&,bool)const;
    bool ReadReloadHandModel(Object,std::array<ModelNode,64>&,unsigned&)const;
    bool ReadReloadFeedback(Object,ReloadFeedbackAssets&)const;
    bool ReadReloadPcm(Object,Object,ReloadPcm&)const;
    bool ValidReloadFeedbackTag(Object,Object,bool effect=false)const;
    bool HideNativeWeapon()const;
    bool DrawBodyModels(bool,const std::array<HolsterMarker,2>&,const Matrix4*,const Matrix4*)const;
    mutable std::array<Object,2> lastMarkerObjects{None,None};
    bool viewModelRefreshPending=false;
    bool visualGuardsInstalled=true; mutable unsigned lastBodyStatus=~0u;
    Object stowedUnit=None; std::uint32_t stowedStorage=0,stowedAge=0;
    bool CheckCode()const{return codeValid;}
    std::uint32_t RecordLook(Point,bool,float,float=0,float=0);
    bool NewLooseWeapon(const NativeInventory&,Object) const;
    bool HandleWorldItem(Object,Object);
    Object PickupTarget() const;
    bool Pickup(Object,Object);
    bool Select(Object,Object);
    bool SynchronizeViewModel(bool atPose=false);
    bool ViewModelReady(Object)const;
    bool ViewModelWeapon(Object&,Object&,Object&)const;

};
namespace {
bool rejectStore=false;
bool Store(std::uintptr_t address,const void* source,std::size_t size){
    if(rejectStore)return false;
    for(auto& region:memory)if(address>=region.first && address-region.first<=region.second.size() &&
        size<=region.second.size()-(address-region.first)){
        std::memcpy(region.second.data()+address-region.first,source,size);return true;
    }
    return false;
}
// INSERT_PRODUCTION_READ_HELPERS
bool canUse=true,addSuccess=true,selectSuccess=true;
unsigned offered=0,added=0;int offeredAction=0;
bool CanUseWeapon(Object,Object){return canUse;}
void OfferPickup(Object player,Object item,int action){
    ++offered;offeredAction=action;
    if(player==(13u<<16)){put<Object>(0x121024,item);put<std::int16_t>(0x121028,static_cast<std::int16_t>(action));}
}
bool AddWeapon(Object,Object item){
    ++added;if(!addSuccess)return false;
    for(unsigned i=0;i<4;++i){Object id;ReadAt(0x1302f8+4*i,id);
        if(id==None){put<Object>(0x1302f8+4*i,item);return true;}}
    return false;
}
}
unsigned refreshes=0,readyCalls=0;
bool fingerprintValid=true;
bool MatchFingerprint(const CodeFingerprint&){return fingerprintValid;}
bool ModelDrawReady(){return fingerprintValid;}
void RefreshViewModel(int player) {
    if(player!=0)std::abort();
    ++refreshes;
    std::int16_t slot=-1;ReadAt(0x1302f2,slot);
    Object w=None;if(slot>=0)ReadAt(0x1302f8+slot*4,w);
    put<Object>(0x200008,w);put<unsigned char>(0x200000,w!=None);
    put<Object>(0x201d8e,w); // Native remap/animation rebuild simulated.
}
void ReadyWeapon(Object){
    ++readyCalls;
    if(!selectSuccess)return;
    std::int16_t desired=-1;ReadAt(0x1302f4,desired);put<std::int16_t>(0x1302f2,desired);
}
unsigned poseCalls=0,hudCalls=0,visibilityCalls=0;
RetailWeaponEngine* visualOwner=nullptr;
void originalPose(){++poseCalls;put<unsigned char>(0x200000,1);}
void originalWeaponHUD(void*){++hudCalls;}
void SetNativeVisibility(int player,bool visible){if(player!=0 || visible)std::abort();++visibilityCalls;put<unsigned char>(0x200000,0);}
// INSERT_PRODUCTION_VEHICLE_DRAW_HELPER
// INSERT_PRODUCTION_VISUAL_GUARDS
static unsigned buttonReloads=0;
void StartNativeReload(Immersion::Object){++buttonReloads;}
// INSERT_PRODUCTION_READ
std::vector<Object> drawnModels;
std::vector<Vector3> drawnPoints;
void* ObjectLighting(Object){return reinterpret_cast<void*>(1);}
std::vector<bool> drawnGreys;
std::vector<float> drawnScales;
std::array<Transform,64> lastBodyNodes{};
void DrawNativeModel(Object model,const Transform* nodes,Object,std::uintptr_t,void*,bool marker=false,bool grey=false){
    drawnGreys.push_back(marker && grey);
    drawnScales.push_back((Immersion::ModelMatrix(nodes[0])*Vector3(1,0,0)-nodes[0].translation).length());
    drawnModels.push_back(model);drawnPoints.push_back(nodes[0].translation);
    if(model==21){unsigned count=0;ReadAt(0x2710b8,count);std::copy(nodes,nodes+(std::min)(count,64u),lastBodyNodes.begin());}
}
// INSERT_PRODUCTION_VISUAL
// INSERT_PRODUCTION_PICKUP
void PositionObject(Object id,const Point* p,const Point*,const Point*){
    std::array<unsigned char,0x1f4> object{};std::uintptr_t address=0;
    if(ObjectBytes(id,object,address,5))Store(address+0x5c,p,sizeof(*p));
}
bool ValidatePosition(Object,const Point*){return true;}
// INSERT_PRODUCTION_INTERACTION_ENGINE
// INSERT_PRODUCTION_ACCURACY

}
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
constexpr std::uint32_t unit=(42u<<16)|10u,weapon=(43u<<16)|11u,loose=(44u<<16)|12u,localPlayer=13u<<16;
void setup(){
    Immersion::rejectStore=false;
    memory.clear();Immersion::offered=Immersion::added=0;Immersion::canUse=Immersion::addSuccess=Immersion::selectSuccess=true;
    for(auto r:std::map<std::uintptr_t,std::size_t>{{0x6b145c,4},{0x150000,0x50},{0x6f1d20,4},{0x87a480,4},{0x8603b0,4},
        {0x100000,0x38},{0x110000,2048*12},{0x120000,0x38},{0x121000,0x200},{0x130000,0x550},{0x140000,0x340},{0x160000,0x340}})
        memory[r.first].resize(r.second);
    put<std::uint32_t>(0x6b145c,0x150000);
    put<std::uint32_t>(0x150010,unit);
    put<std::uint32_t>(0x8603b0,0x100000);put<std::uint16_t>(0x100020,2048);put<std::uint16_t>(0x100022,12);
    put<std::uint32_t>(0x100034,0x110000);
    put<std::uint16_t>(0x110000+10*12,42);put<std::uint16_t>(0x110006+10*12,0x550);put<std::uint32_t>(0x110008+10*12,0x130000);
    put<std::uint16_t>(0x110000+11*12,43);put<std::uint16_t>(0x110006+11*12,0x340);put<std::uint32_t>(0x110008+11*12,0x140000);
    put<std::uint16_t>(0x1400b4,2);put<std::uint32_t>(0x140000,123);
    put<std::uint16_t>(0x110000+12*12,44);put<std::uint16_t>(0x110006+12*12,0x340);put<std::uint32_t>(0x110008+12*12,0x160000);
    put<std::uint16_t>(0x1600b4,2);put<std::uint32_t>(0x160000,456);put<std::uint32_t>(0x160010,0x800);
    put<std::uint32_t>(0x16011c,Immersion::None);put<std::uint32_t>(0x160200,Immersion::None);
    put<std::uint32_t>(0x87a480,0x120000);put<std::uint16_t>(0x120020,16);put<std::uint16_t>(0x120022,0x200);
    put<std::uint16_t>(0x120030,1);put<std::uint32_t>(0x120034,0x121000);put<std::uint16_t>(0x121000,13);
    put<std::uint32_t>(0x121034,unit);put<std::uint32_t>(0x130218,13u<<16);put<float>(0x1300e0,1.0f);
    put<std::uint32_t>(0x13011c,0xffffffff);put<std::uint32_t>(0x1302f8,weapon);
    for(unsigned i=1;i<4;++i)put<std::uint32_t>(0x1302f8+4*i,0xffffffff);
}
void checkARAccuracy(){
    using namespace Immersion;RetailWeaponEngine engine;
    auto prepare=[&](const char* path="weapons\\assault rifle\\assault rifle"){
        setup();engine.accuracyShots.clear();engine.available=true;
        memory[0x87bc14].resize(4);memory[0x210000].resize(0x2000);memory[0x220000].resize(0x508);
        memory[0x230000].resize(0x114);memory[0x240000].resize(std::strlen(path)+1);
        std::memcpy(memory[0x240000].data(),path,std::strlen(path)+1);
        put<std::uint32_t>(0x87bc14,0x210000);constexpr auto tag=0x210000+123*0x20;
        put<std::uint32_t>(tag,0x77656170);put<std::uint32_t>(tag+0xc,123);
        put<std::uint32_t>(tag+0x14,0x220000);put<std::uint32_t>(tag+0x10,0x240000);
        put<std::uint32_t>(0x2204fc,1);put<std::uint32_t>(0x220500,0x230000);
        put<std::uint32_t>(0x14011c,unit);
        put(0x230078,std::array<float,3>{.02f,.04f,.12f});
    };
    auto angle=[&](){std::array<float,3> v{};CHECK(ReadAt(0x230078,v));return v;};
    for(float factor:{.35f,.65f,1.f}){
        prepare();const auto snapshot=memory;
        CHECK(std::fabs(engine.AccuracyReticleScale(factor)-factor)<.0001f);
        engine.BeginAccuracyShot(weapon,0,factor);
        CHECK(std::fabs(angle()[2]-.12f*factor)<.0001f);
        // Reentrant AI fire must see native values; nested local fire must
        // not multiply the accuracy improvement again.
        engine.BeginAccuracyShot(loose,0,factor);CHECK(angle()[2]==.12f);
        engine.EndAccuracyShot();CHECK(std::fabs(angle()[2]-.12f*factor)<.0001f);
        engine.BeginAccuracyShot(weapon,0,factor);CHECK(std::fabs(angle()[2]-.12f*factor)<.0001f);
        engine.EndAccuracyShot();engine.EndAccuracyShot();engine.EndAccuracyShot();
        CHECK(memory==snapshot); // No ammo/damage/rate, map or tag residue.
    }
    for(const char* path:{"weapons\\pistol\\pistol","weapons\\shotgun\\shotgun","weapons\\custom\\assault rifle"}){
        prepare(path);const auto snapshot=memory;CHECK(engine.AccuracyReticleScale(.65f)==1);
        engine.BeginAccuracyShot(weapon,0,.65f);CHECK(memory==snapshot);engine.EndAccuracyShot();
    }
    for(int blocked=0;blocked<8;++blocked){
        prepare();
        if(blocked==0)put<std::uint32_t>(0x6f1d20,1); // Multiplayer.
        if(blocked==1)put<std::uint32_t>(0x14011c,unit+1); // NPC / stale owner.
        if(blocked==2)put<std::uint16_t>(0x110000+11*12,1); // Stale datum.
        if(blocked==3)put<std::uint32_t>(0x2204fc,3); // Unexpected trigger block.
        if(blocked==4)put<float>(0x230078,std::numeric_limits<float>::quiet_NaN());
        if(blocked==5)put<std::uint32_t>(0x220500,0xfffffff0);
        if(blocked==6)engine.available=false;
        if(blocked==7)put<std::int16_t>(0x1302f2,-1); // Holstered.
        const auto snapshot=memory;CHECK(engine.AccuracyReticleScale(.65f)==1);
        engine.BeginAccuracyShot(weapon,0,.65f);engine.EndAccuracyShot();CHECK(memory==snapshot);
    }
    prepare();const auto snapshot=memory;rejectStore=true;
    engine.BeginAccuracyShot(weapon,0,.65f);engine.EndAccuracyShot();CHECK(memory==snapshot);rejectStore=false;
    engine.BeginAccuracyShot(weapon,1,.65f);engine.EndAccuracyShot();CHECK(memory==snapshot);
    CHECK(ARSpreadScale(-1)==.35f && ARSpreadScale(10)==1.f);
}
void checkReloadTransactions(){
    using namespace Immersion;
    RetailWeaponEngine engine;
    auto prepare=[&](const char* path="weapons\\assault rifle\\assault rifle") {
        setup();engine.stowedUnit=None;
        memory[0x87bc14].resize(4);memory[0x210000].resize(0x2000);memory[0x220000].resize(0x500);
        memory[0x230000].resize(0x70);memory[0x240000].resize(std::strlen(path)+1);
        std::memcpy(memory[0x240000].data(),path,std::strlen(path)+1);
        put<std::uint32_t>(0x87bc14,0x210000);
        constexpr auto tag=0x210000+123*0x20;
        put<std::uint32_t>(tag,0x77656170);put<std::uint32_t>(tag+0xc,123);put<std::uint32_t>(tag+0x14,0x220000);put<std::uint32_t>(tag+0x10,0x240000);
        put<std::uint32_t>(0x2204f0,1);put<std::uint32_t>(0x2204f4,0x230000);put<std::int16_t>(0x23000a,60);
        put<std::uint32_t>(0x14011c,unit);put<std::int16_t>(0x1402b6,240);put<std::int16_t>(0x1402b8,17);
    };
    ReloadAmmo before,after,checked;
    for(const auto* path:{"weapons\\assault rifle\\assault rifle","weapons\\pistol\\pistol","weapons\\shotgun\\shotgun",
        "weapons\\sniper rifle\\sniper rifle","weapons\\rocket launcher\\rocket launcher"}){
        prepare(path);CHECK(engine.ReadReloadAmmo(weapon,before));CHECK(before.loaded==17 && before.reserve==240 && before.capacity==60);
        CHECK(before.unit==unit && before.weapon==weapon && before.tag==123);
        after=before;after.reserve+=after.loaded;after.loaded=0;CHECK(engine.CommitReloadAmmo(before,after));
        CHECK(engine.ReadReloadAmmo(weapon,checked));CHECK(checked.loaded==0 && checked.reserve==257);
        CHECK(!engine.CommitReloadAmmo(before,after)); // A stale transaction cannot replay.
        before=checked;after=before;after.loaded=60;after.reserve-=60;CHECK(engine.CommitReloadAmmo(before,after));
        CHECK(engine.ReadReloadAmmo(weapon,checked));CHECK(checked.loaded==60 && checked.reserve==197);
        std::int16_t selection=-1;CHECK(ReadAt(0x1302f2,selection));CHECK(selection==0); // Ammo changes never select/rebuild a weapon.
    }
    for(const auto* path:{"weapons\\plasma pistol\\plasma pistol","weapons\\plasma rifle\\plasma rifle","weapons\\custom\\custom"}){
        prepare(path);CHECK(!engine.ReadReloadAmmo(weapon,before));
    }
    prepare("weapons\\needler\\needler");CHECK(engine.ReadReloadAmmo(weapon,before));CHECK(before.kind==ReloadWeaponKind::Needler);
    CHECK(engine.StartButtonReload(weapon));CHECK(buttonReloads==1);
    for(int state:{1,3}){put<std::int16_t>(0x1402b0,state);CHECK(!engine.StartButtonReload(weapon));}
    put<std::int16_t>(0x1402b0,0);put<std::int16_t>(0x1402b6,0);CHECK(!engine.StartButtonReload(weapon));
    put<std::int16_t>(0x1402b6,240);put<std::int16_t>(0x1402b8,60);CHECK(!engine.StartButtonReload(weapon));
    prepare();CHECK(!engine.StartButtonReload(weapon));CHECK(buttonReloads==1);
    prepare();CHECK(engine.ReadReloadAmmo(weapon,before));after=before;++after.loaded;CHECK(!engine.CommitReloadAmmo(before,after));
    after=before;--after.reserve;++after.loaded;rejectStore=true;CHECK(!engine.CommitReloadAmmo(before,after));rejectStore=false;
    CHECK(engine.ReadReloadAmmo(weapon,checked));CHECK(checked.loaded==before.loaded && checked.reserve==before.reserve);
    for(int state:{1,3}){
        put<std::int16_t>(0x1402b0,state);CHECK(engine.ReadReloadAmmo(weapon,before));after=before;--after.reserve;++after.loaded;
        CHECK(!engine.CommitReloadAmmo(before,after));
    }
    put<std::int16_t>(0x1402b0,2);CHECK(engine.ReadReloadAmmo(weapon,before));after=before;--after.reserve;++after.loaded;CHECK(engine.CommitReloadAmmo(before,after));
    prepare();CHECK(engine.ReadReloadAmmo(weapon,before));after=before;--after.reserve;++after.loaded;
    after.weapon=loose;CHECK(!engine.CommitReloadAmmo(before,after));after.weapon=weapon;
    put<std::uint16_t>(0x110000+11*12,44);CHECK(!engine.CommitReloadAmmo(before,after)); // Salt/generation changed.
    prepare();engine.reloadAvailable=false;CHECK(!engine.ReadReloadAmmo(weapon,before));engine.reloadAvailable=true;
    engine.stowedUnit=unit;engine.stowedStorage=0x130000;engine.stowedAge=0;
    CHECK(!engine.ReadReloadAmmo(weapon,before));CHECK(engine.ReadReloadAmmo(weapon,before,false)); // Guard stowed inventory too.
    prepare();put<std::int16_t>(0x23000a,0);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();put<std::int16_t>(0x1402b8,61);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();put<std::int16_t>(0x1402b6,-1);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();put<std::uint32_t>(0x6f1d20,1);CHECK(!engine.ReadReloadAmmo(weapon,before)); // Multiplayer.
    prepare();put<std::uint32_t>(0x14011c,unit+1);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();put<std::uint32_t>(0x210000+123*0x20+0xc,124);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();memory[0x240000]=std::vector<unsigned char>(256,'x');CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();memory.erase(0x240000);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare();put<std::int16_t>(0x1402b0,4);CHECK(!engine.ReadReloadAmmo(weapon,before));
    prepare(); // Read the same native hands model used by accepted unarmed rendering.
    std::array<ModelNode,64> handNodes{};unsigned handCount=99;
    CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));CHECK(handCount==0);
    memory[0x746fa0].resize(4);put<std::uint32_t>(0x746fa0,0x250000);
    memory[0x250000].resize(0x184);put<unsigned>(0x25017c,1);put<unsigned>(0x250180,0x260000);
    memory[0x260000].resize(16);put<Object>(0x26000c,77);
    const auto modelTag=0x210000+77*0x20;
    put<unsigned>(modelTag,0x6d6f6432);put<Object>(modelTag+0xc,77);put<unsigned>(modelTag+0x14,0x270000);
    memory[0x270000].resize(0xc0);put<unsigned>(0x2700b8,2);put<unsigned>(0x2700bc,0x280000);
    memory[0x280000].resize(2*sizeof(ModelNode));
    for(unsigned n=0;n<2;++n){ModelNode node{};node.parent=static_cast<std::int16_t>(n)-1;
        std::strcpy(node.name,n?"bip01 l finger1":"bip01 l wrist");node.inverseBind=ModelTransform(Matrix4());
        put(0x280000+n*sizeof(ModelNode),node);
    }
    CHECK(engine.ReadReloadHandModel(weapon,handNodes,handCount));CHECK(handCount==2);
    put<unsigned>(0x25017c,0);CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));
    put<unsigned>(0x25017c,17);CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));
    put<unsigned>(0x25017c,1);put<unsigned>(0x2700b8,65);CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));
    put<unsigned>(0x2700b8,2);put<Object>(modelTag+0xc,78);CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));
    put<Object>(modelTag+0xc,77);put<std::uint32_t>(0x14011c,unit+1);CHECK(!engine.ReadReloadHandModel(weapon,handNodes,handCount));
    prepare("weapons\\shotgun\\shotgun");
    auto makeTag=[&](Object id,unsigned group,unsigned address){const auto t=0x210000+id*0x20;
        put<unsigned>(t,group);put<Object>(t+0xc,id);put<unsigned>(t+0x14,address);};
    makeTag(77,0x616e7472,0x270000);put<Object>(0x220478,77);
    memory[0x270000].resize(0x80);memory[0x280000].resize(3*0xb4);memory[0x290000].resize(3*20);
    put<unsigned>(0x270074,3);put<unsigned>(0x270078,0x280000);put<unsigned>(0x270054,3);put<unsigned>(0x270058,0x290000);
    const char* names[]={"fire-1","reload","ready"};
    for(unsigned i=0;i<3;++i){
        std::array<char,32> name{};std::strcpy(name.data(),names[i]);put(0x280000+i*0xb4,name);
        put<std::int16_t>(0x28003c+i*0xb4,i);put<Object>(0x29000c+i*20,80+i);makeTag(80+i,0x736e6421,0x300000+i*0x1000);
    }
    makeTag(90,0x65666665,0x330000);put<Object>(0x230054,90);
    ReloadFeedbackAssets assets;CHECK(engine.ReadReloadFeedback(weapon,assets));
    CHECK(assets.IsPumpSound(80));CHECK(!assets.IsPumpSound(81));CHECK(assets.chamberEffect==90);
    CHECK(assets.sounds[static_cast<unsigned>(ReloadCue::Pull)]==80);
    CHECK(assets.sounds[static_cast<unsigned>(ReloadCue::Insert)]==81);
    CHECK(assets.sounds[static_cast<unsigned>(ReloadCue::Grab)]==None);
    CHECK(engine.ValidReloadFeedbackTag(weapon,80));CHECK(!engine.ValidReloadFeedbackTag(weapon,90));CHECK(engine.ValidReloadFeedbackTag(weapon,90,true));
    // Only validated decoded PCM is copied. Reject compressed/stale/unloaded
    // data before allocation or playback; retain channel/rate boundaries.
    memory[0x300000].resize(0xa4);put<std::uint16_t>(0x300006,0);put<std::uint16_t>(0x30006c,0);
    put<unsigned>(0x300098,1);put<unsigned>(0x30009c,0x340000);memory[0x340000].resize(0x48);
    put<unsigned>(0x34003c,1);put<unsigned>(0x340040,0x350000);memory[0x350000].resize(0x7c);
    put<std::uint16_t>(0x350028,1);put<std::int16_t>(0x35002a,-1);put<unsigned>(0x350030,0x360000);
    put<unsigned>(0x350038,4000);memory[0x360000].resize(4000);
    for(unsigned n=0;n<2000;++n)put<std::int16_t>(0x360000+n*2,static_cast<std::int16_t>(n));
    ReloadPcm pcm;CHECK(engine.ReadReloadPcm(weapon,80,pcm));CHECK(pcm.rate==22050);CHECK(pcm.channels==1);
    CHECK(pcm.samples.size()==2000);CHECK(pcm.samples[1999]==1999);
    mutateAfterCopy=0x360000;CHECK(!engine.ReadReloadPcm(weapon,80,pcm));CHECK(pcm.samples.empty());
    CHECK(engine.ReadReloadPcm(weapon,80,pcm)); // Same allocation/header, now a stable snapshot.
    put<std::uint16_t>(0x300006,1);put<std::uint16_t>(0x30006c,1);
    CHECK(engine.ReadReloadPcm(weapon,80,pcm));CHECK(pcm.rate==44100);CHECK(pcm.channels==2);
    put<unsigned>(0x350038,4002);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));CHECK(pcm.samples.empty());
    put<unsigned>(0x350038,4000);put<std::uint16_t>(0x350028,3);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<std::uint16_t>(0x350028,1);put<unsigned>(0x350038,10000000);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<unsigned>(0x350038,4000);put<unsigned>(0x350030,0);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<unsigned>(0x350030,0x360000);put<std::int16_t>(0x35002a,1);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<std::int16_t>(0x35002a,-1);put<unsigned>(0x34003c,257);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<unsigned>(0x34003c,1);put<unsigned>(0x300098,9);CHECK(!engine.ReadReloadPcm(weapon,80,pcm));
    put<unsigned>(0x300098,1);
    put<Object>(0x210000+80*0x20+0xc,81);CHECK(!engine.ValidReloadFeedbackTag(weapon,80));
    CHECK(engine.ReadReloadFeedback(weapon,assets));CHECK(!assets.IsPumpSound(80)); // Stale sound datums rejected.
    put<unsigned>(0x270054,257);CHECK(!engine.ReadReloadFeedback(weapon,assets));put<unsigned>(0x270054,3);
    put<unsigned>(0x270074,1025);CHECK(!engine.ReadReloadFeedback(weapon,assets));put<unsigned>(0x270074,3);
    put<std::int16_t>(0x28003c,300);CHECK(engine.ReadReloadFeedback(weapon,assets));CHECK(!assets.IsPumpSound(80));
    put<Object>(0x14011c,unit+1);CHECK(!engine.ReadReloadFeedback(weapon,assets));

    prepare("weapons\\rocket launcher\\rocket launcher");
    makeTag(77,0x616e7472,0x270000);put<Object>(0x220478,77);
    memory[0x270000].resize(0x80);memory[0x280000].resize(2*0xb4);memory[0x290000].resize(2*20);
    put<unsigned>(0x270074,2);put<unsigned>(0x270078,0x280000);put<unsigned>(0x270054,2);put<unsigned>(0x270058,0x290000);
    for(unsigned i=0;i<2;++i){
        std::array<char,32> name{};std::strcpy(name.data(),i?"first-person reload-full":"first-person reload-empty");
        put(0x280000+i*0xb4,name);put<std::int16_t>(0x28003c+i*0xb4,i);
        put<Object>(0x29000c+i*20,80+i);makeTag(80+i,0x736e6421,0x300000+i*0x1000);
    }
    CHECK(engine.ReadReloadFeedback(weapon,assets));
    unsigned phase=0;for(auto cue:{ReloadCue::Pull,ReloadCue::Eject,ReloadCue::Insert,ReloadCue::Close}){
        const auto i=static_cast<unsigned>(cue);CHECK(assets.sounds[i]==80);
        CHECK(assets.slices[i].part==phase++);CHECK(assets.slices[i].parts==4);
    }
    // Resolve primary trigger projectile -> model, not a guessed datum or the full launcher.
    memory[0x220000].resize(0x508);memory[0x370000].resize(0x114);memory[0x380000].resize(0x40);
    memory[0x390000].resize(0xc4);memory[0x3a0000].resize(sizeof(ModelNode));
    put<unsigned>(0x2204fc,1);put<unsigned>(0x220500,0x370000);put<Object>(0x3700a0,88);
    makeTag(88,0x70726f6a,0x380000);put<Object>(0x380034,89);makeTag(89,0x6d6f6432,0x390000);
    put<unsigned>(0x3900b8,1);put<unsigned>(0x3900bc,0x3a0000);
    ModelNode rocketNode{};rocketNode.parent=-1;rocketNode.inverseBind=ModelTransform(Matrix4());put(0x3a0000,rocketNode);
    Object rocketModel=None;CHECK(engine.ReadReloadRocketModel(weapon,rocketModel));CHECK(rocketModel==89);
    memory[0x7c3108].resize(2);put<std::int16_t>(0x7c3108,0);
    drawnModels.clear();drawnGreys.clear();drawnScales.clear();drawnPoints.clear();
    const auto pose=Matrix4().rotateZ(32).translate(1,2,3);
    CHECK(engine.DrawReloadRocket(weapon,pose,false));CHECK(engine.DrawReloadRocket(weapon,pose,true));
    CHECK(drawnModels==std::vector<Object>({89,89}));CHECK(drawnGreys==std::vector<bool>({false,true}));
    CHECK(drawnPoints[0]==Vector3(1,2,3));CHECK(std::fabs(drawnScales[0]-1.f)<.0001f);
    CHECK(engine.ReadReloadAmmo(weapon,before));CHECK(before.loaded==17 && before.reserve==240);
    for(unsigned count:{0u,3u,0xffffffffu}){put<unsigned>(0x2204fc,count);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));CHECK(rocketModel==None);}
    put<unsigned>(0x2204fc,1);put<Object>(0x210000+88*0x20+0xc,99);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));
    put<Object>(0x210000+88*0x20+0xc,88);put<unsigned>(0x210000+88*0x20,0x77656170);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));
    put<unsigned>(0x210000+88*0x20,0x70726f6a);put<unsigned>(0x220500,0xfffffff0);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));
    put<unsigned>(0x220500,0x370000);put<unsigned>(0x3900b8,65);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));
    put<unsigned>(0x3900b8,1);rocketNode.parent=0;put(0x3a0000,rocketNode);CHECK(!engine.ReadReloadRocketModel(weapon,rocketModel));
    rocketNode.parent=-1;put(0x3a0000,rocketNode);drawnModels.clear();
    put<std::int16_t>(0x7c3108,-1);CHECK(!engine.DrawReloadRocket(weapon,pose,false));put<std::int16_t>(0x7c3108,0);
    fingerprintValid=false;CHECK(!engine.DrawReloadRocket(weapon,pose,false));fingerprintValid=true;
    Matrix4 invalid=pose;invalid[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!engine.DrawReloadRocket(weapon,invalid,false));
    engine.stowedUnit=unit;engine.stowedStorage=0x130000;engine.stowedAge=0;CHECK(!engine.DrawReloadRocket(weapon,pose,false));
    CHECK(drawnModels.empty());
    drawnModels.clear();drawnPoints.clear();drawnGreys.clear();drawnScales.clear();

}
void checkOnFootBody(){
    using namespace Immersion;setup();RetailWeaponEngine engine;
    memory[0x87bc14].resize(4);put<unsigned>(0x87bc14,0x210000);memory[0x210000].resize(0x2000);
    put<Object>(0x130000,20);memory[0x270000].resize(0x508);memory[0x271000].resize(0xc0);
    for(unsigned id:{20u,21u}){put<unsigned>(0x210000+id*0x20,id==20?0x62697064:0x6d6f6432);
        put<Object>(0x21000c+id*0x20,id);put<unsigned>(0x210014+id*0x20,id==20?0x270000:0x271000);}
    put<Object>(0x270034,21);put<unsigned>(0x2710b8,8);put<unsigned>(0x2710bc,0x272000);
    memory[0x272000].resize(8*sizeof(ModelNode));memory[0x130000].resize(0x1000);
    put<std::uint16_t>(0x110006+10*12,0x1000);put<std::int16_t>(0x1301f2,0x700);
    const char* names[]={"root","pelvis","head","l upperarm","r upperarm","l thigh","r thigh","spine"};
    for(unsigned n=0;n<8;++n){ModelNode node{};node.parent=n==0?-1:n==1?0:1;std::strcpy(node.name,names[n]);
        node.inverseBind=ModelTransform(Matrix4().translate({0,0,-float(n)*.1f}));put(0x272000+n*sizeof(node),node);
        put(0x130700+n*sizeof(Transform),ModelTransform(Matrix4().translate({10,20,30+float(n)*.1f})));}
    memory[0x7c3108].resize(2);put<std::int16_t>(0x7c3108,0);
    const auto before=memory[0x130000];const auto tags=memory[0x272000];drawnModels.clear();
    CHECK(engine.DrawOnFootBody());CHECK(drawnModels.size()==1 && drawnModels[0]==21);
    for(unsigned i:{0u,1u,2u,3u,4u,7u})CHECK(lastBodyNodes[i].scale==0);
    CHECK(lastBodyNodes[5].scale==1 && lastBodyNodes[6].scale==1);
    CHECK(memory[0x130000]==before && memory[0x272000]==tags);
    for(unsigned fault=0;fault<6;++fault){
        memory[0x130000]=before;engine.interactionAvailable=true;engine.visualGuardsInstalled=true;put<std::int16_t>(0x7c3108,0);
        if(fault==0)put<Object>(0x13011c,123);
        if(fault==1)put<float>(0x1300e0,0);
        if(fault==2)put<std::int16_t>(0x7c3108,1);
        if(fault==3)engine.interactionAvailable=false;
        if(fault==4)engine.visualGuardsInstalled=false;
        if(fault==5)put<std::int16_t>(0x1301f2,0xfff);
        CHECK(!engine.DrawOnFootBody());CHECK(drawnModels.size()==1);
    }
}
void checkPhysicalNative(){
    using namespace Immersion;setup();RetailWeaponEngine engine;
    GrenadeInventory inv;
    put<unsigned char>(0x13031e,4);put<unsigned char>(0x13031f,2);put<unsigned char>(0x13031c,0);
    CHECK(engine.ReadGrenades(inv));CHECK(inv.count[0]==4 && inv.count[1]==2);
    CHECK(!engine.SelectGrenade(unit+0x10000,1));CHECK(engine.SelectGrenade(unit,1));
    CHECK(engine.ReadGrenades(inv) && inv.selected==1);CHECK(!engine.SelectGrenade(unit,2));
    std::int16_t grenadeCommand=-1;ReadAt(0x150032,grenadeCommand);CHECK(grenadeCommand==1);
    put<Object>(0x150010,unit+1);CHECK(!engine.SelectGrenade(unit,0));put<Object>(0x150010,unit);
    CHECK(engine.ReadGrenades(inv) && inv.selected==1);
    put<unsigned char>(0x13028d,2);CHECK(!engine.SelectGrenade(unit,0));
    const Object projectile=(51u<<16)|14u;
    memory[0x190000].resize(0x300);put<std::uint16_t>(0x110000+14*12,51);put<std::uint16_t>(0x110006+14*12,0x300);
    put<std::uint32_t>(0x110008+14*12,0x190000);put<std::uint16_t>(0x1900b4,5);put<Object>(0x19011c,unit);
    put<Object>(0x130294,projectile);CHECK(engine.AttachedGrenade(unit)==projectile);CHECK(engine.AttachedGrenade(unit+1)==None);
    TrackedGrenadeRelease release{unit,1,0,{}};release.pose.position={.2f,.3f,.4f};release.pose.velocity={3,0,0};
    CHECK(!engine.FinishGrenade(projectile,release,2,7)); // Still attached: never detach ourselves.
    put<Object>(0x19011c,None);put<unsigned char>(0x13028d,3);put<Point>(0x130068,{.1f,0,0});
    CHECK(engine.FinishGrenade(projectile,release,2,7));Point velocity,pos;ReadAt(0x190068,velocity);ReadAt(0x19005c,pos);
    CHECK(std::fabs(velocity.x-(7.f/30+.1f))<.0001f);CHECK(pos.x==.2f && pos.y==.3f && pos.z==.4f);
    CHECK(!engine.FinishGrenade(projectile+0x10000,release,2,7));release.pose.position={50,0,0};CHECK(!engine.FinishGrenade(projectile,release,2,7));
    CHECK(engine.ReadGrenades(inv) && inv.count[0]==4 && inv.count[1]==2);
    put<Object>(0x130218,99);CHECK(!engine.ReadGrenades(inv));put<Object>(0x130218,localPlayer);
    engine.interactionAvailable=false;CHECK(!engine.ReadGrenades(inv));engine.interactionAvailable=true;
    // Synthetic seated biped with live world-space skeletal matrices.
    memory[0x87bc14].resize(4);put<unsigned>(0x87bc14,0x210000);memory[0x210000].resize(0x2000);
    auto tag=[&](Object id,unsigned group,unsigned data,unsigned size){
        put<unsigned>(0x210000+id*0x20,group);put<Object>(0x21000c+id*0x20,id);put<unsigned>(0x210014+id*0x20,data);memory[data].resize(size);
    };
    tag(20,0x62697064,0x270000,0x508);tag(21,0x6d6f6432,0x271000,0xc0);tag(22,0x76656869,0x273000,0x300);
    put<Object>(0x130000,20);put<Object>(0x270034,21);put<unsigned>(0x2710b8,4);put<unsigned>(0x2710bc,0x272000);
    memory[0x272000].resize(4*sizeof(ModelNode));memory[0x130000].resize(0x1000);put<std::uint16_t>(0x110006+10*12,0x1000);put<std::int16_t>(0x1301f2,0x700);
    for(unsigned n=0;n<4;++n){ModelNode node{};node.parent=n?0:-1;
        std::strcpy(node.name,n==0?"root":n==1?"bip01 head":n==2?"bip01 l hand":"bip01 r hand");
        node.inverseBind=ModelTransform(Matrix4());put(0x272000+n*sizeof(node),node);
        put(0x130700+n*sizeof(Transform),ModelTransform(Matrix4().translate({10.f,float(n),30.f})));
    }
    const Object car=(50u<<16)|13u;memory[0x180000].resize(0x400);put<std::uint16_t>(0x110000+13*12,50);
    put<std::uint16_t>(0x110006+13*12,0x400);put<unsigned>(0x110008+13*12,0x180000);put<Object>(0x180000,22);put<std::uint16_t>(0x1800b4,1);
    put<Object>(0x13011c,car);put<std::int16_t>(0x1302f0,0);put<Point>(0x180074,{1,0,0});put<Point>(0x180080,{0,0,1});put<Point>(0x13023c,{1,0,0});
    put<unsigned>(0x2732e4,3);put<unsigned>(0x2732e8,0x274000);memory[0x274000].resize(3*0x11c);
    for(unsigned i=0;i<3;++i)std::strcpy(reinterpret_cast<char*>(memory[0x274000].data()+i*0x11c+4),i==0?"W-driver":i==1?"W-gunner":"W-passenger");
    put<unsigned>(0x210010+22*0x20,0x275000);memory[0x275000].resize(256);std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\warthog\\warthog");
    VehicleState seat;CHECK(engine.ReadVehicle(seat));CHECK(seat.kind==VehicleSeat::WarthogDriver);CHECK(seat.eye.x==10 && seat.eye.y==1 && seat.eye.z==30);
    CHECK(seat.handleValid[0] && seat.handleValid[1]);CHECK(seat.handles[0][13]==2 && seat.handles[1][13]==3);
    // Rendered vehicle nodes deliberately differ from raw simulation position.
    // Cancel a translated/rotated root bind; never add a second interpolation.
    tag(23,0x6d6f6432,0x276000,0xc0);put<Object>(0x273034,23);
    put<unsigned>(0x2760b8,1);put<unsigned>(0x2760bc,0x277000);memory[0x277000].resize(sizeof(ModelNode));
    memory[0x180000].resize(0x800);put<std::uint16_t>(0x110006+13*12,0x800);put<std::int16_t>(0x1801f2,0x500);
    ModelNode chassis{};chassis.parent=-1;std::strcpy(chassis.name,"frame hull");
    const auto bind=Matrix4().rotateZ(12).translate({.1f,-.2f,.3f});
    chassis.inverseBind=ModelTransform(Matrix4(bind).invertAffine());put(0x277000,chassis);
    const auto rendered=Matrix4().rotateZ(37).translate({10.2f,20.1f,30.05f});
    put(0x180500,ModelTransform(rendered*bind));put<Point>(0x18005c,{10,20,30});
    CHECK(engine.ReadVehicle(seat) && seat.renderedPoseValid);
    for(unsigned i=0;i<16;++i)CHECK(std::fabs(seat.renderedBasis[i]-rendered[i])<.00001f);
    CHECK(std::fabs(seat.renderedFacing.x-rendered.getLeftAxis().x)<.00001f);
    const auto nativeCar=memory[0x180000];put<std::int16_t>(0x1801f2,0x7f0);
    CHECK(engine.ReadVehicle(seat) && !seat.renderedPoseValid);memory[0x180000]=nativeCar;
    engine.interactionAvailable=false;CHECK(engine.ReadVehicle(seat));engine.interactionAvailable=true;
    engine.vehicleAvailable=false;CHECK(!engine.ReadVehicle(seat));engine.vehicleAvailable=true;CHECK(engine.ReadVehicle(seat));
    memory[0x7c3108].resize(2);put<std::int16_t>(0x7c3108,0);
    const auto bodyBefore=memory[0x130000];
    memory[0x280000].resize(4*sizeof(Transform));
    for(unsigned i=0;i<4;++i)put(0x280000+i*sizeof(Transform),ModelTransform(Matrix4().rotateX(20).translate({11.f,float(i)+.25f,31.f})));
    const auto submittedBefore=memory[0x280000];const auto* submitted=reinterpret_cast<const Transform*>(0x280000);
    visualOwner=&engine;Game::instance.interactions.owner=unit;
    const auto* hidden=PrepareVehicleModelDraw(21,submitted,unit);CHECK(hidden!=submitted);
    CHECK(memory[0x130000]==bodyBefore && memory[0x280000]==submittedBefore);
    CHECK(hidden[1].scale==0 && hidden[0].scale==1 && hidden[2].scale==1 && hidden[3].scale==1);
    for(float axis:hidden[1].rotation)CHECK(axis==0);
    CHECK(hidden[2].translation.y==2.25f && hidden[3].translation.y==3.25f); // Render pose, not simulation pose.
    CHECK(PrepareVehicleModelDraw(21,submitted,unit+0x10000)==submitted); // Other player/AI.
    CHECK(PrepareVehicleModelDraw(22,submitted,unit)==submitted); // Attached weapon/marker.
    Game::instance.interactions.owner=None;CHECK(PrepareVehicleModelDraw(21,submitted,unit)==submitted); // Outside seat eye render.
    Game::instance.interactions.owner=unit;
    put<std::int16_t>(0x7c3108,1);CHECK(PrepareVehicleModelDraw(21,submitted,unit)==submitted);put<std::int16_t>(0x7c3108,0);
    CHECK(PrepareVehicleModelDraw(21,nullptr,unit)==nullptr);
    CHECK(PrepareVehicleModelDraw(21,reinterpret_cast<const Transform*>(0x280001),unit)==reinterpret_cast<const Transform*>(0x280001));
    put<float>(0x280000+sizeof(Transform)+40,std::numeric_limits<float>::quiet_NaN());
    CHECK(PrepareVehicleModelDraw(21,submitted,unit)==submitted);memory[0x280000]=submittedBefore;
    visualOwner=nullptr;CHECK(PrepareVehicleModelDraw(21,submitted,unit)==submitted);
    CHECK(!engine.ReadGrenades(inv)); // Never handle grenades in a vehicle.
    CHECK(!engine.DrawOnFootBody());
    put<std::int16_t>(0x1302f0,1);CHECK(engine.ReadVehicle(seat) && seat.kind==VehicleSeat::WarthogTurret);CHECK(seat.renderedHeadValid);
    put<std::int16_t>(0x1302f0,2);CHECK(!engine.ReadVehicle(seat)); // Passenger is untouched.
    put<std::int16_t>(0x1302f0,0);std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\ghost\\ghost");
    CHECK(engine.ReadVehicle(seat) && seat.kind==VehicleSeat::Ghost);
    CHECK(!engine.IsScorpionDriver());
    std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\scorpion\\scorpion");
    CHECK(engine.IsScorpionDriver());CHECK(!engine.ReadVehicle(seat)); // Controls-only; no new tank camera.
    put<std::int16_t>(0x1302f0,2);CHECK(!engine.IsScorpionDriver());put<std::int16_t>(0x1302f0,0);
    engine.vehicleAvailable=false;CHECK(!engine.IsScorpionDriver());engine.vehicleAvailable=true;
    put<unsigned>(0x6f1d20,1);CHECK(!engine.IsScorpionDriver());put<unsigned>(0x6f1d20,0);
    std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\pelican\\pelican");
    CHECK(engine.ReadVehicle(seat) && seat.kind==VehicleSeat::PelicanPassenger);
    std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\c gun turret\\c gun turret");
    CHECK(engine.ReadVehicle(seat) && seat.kind==VehicleSeat::ShadeTurret);
    CHECK(seat.renderedHeadValid);
    const auto headBind=Matrix4().rotateZ(37).translate({0,0,.3f});
    ModelNode headNode{};ReadAt(0x272000+sizeof(ModelNode),headNode);
    const auto originalHeadNode=headNode;
    headNode.inverseBind=ModelTransform(Matrix4(headBind).invertAffine());put(0x272000+sizeof(ModelNode),headNode);
    put(0x130700+sizeof(Transform),ModelTransform(Matrix4().rotateZ(90).translate({10,1,30})*headBind));
    CHECK(engine.ReadVehicle(seat) && seat.renderedHeadValid);
    CHECK(std::fabs(seat.renderedHeadFacing.x)<.0001f && seat.renderedHeadFacing.y>.999f);
    CHECK(std::fabs(seat.renderedFacing.x-rendered.getLeftAxis().x)>.1f); // Root/chassis is not the head frame.
    put(0x272000+sizeof(ModelNode),originalHeadNode);
    put(0x130700+sizeof(Transform),ModelTransform(Matrix4().translate({10,1,30})));
    put<std::int16_t>(0x1302f0,1);CHECK(engine.ReadVehicle(seat) && seat.kind==VehicleSeat::ShadeTurret);
    put<std::int16_t>(0x1302f0,2);CHECK(!engine.ReadVehicle(seat)); // Do not convert passenger seats.
    put<std::int16_t>(0x1302f0,0);
    std::strcpy(reinterpret_cast<char*>(memory[0x275000].data()),"vehicles\\ghost\\ghost");
    put<std::int16_t>(0x1301f2,0xff0);CHECK(!engine.ReadVehicle(seat)); // Node array crosses allocation.
    put<std::int16_t>(0x1301f2,-1);CHECK(!engine.ReadVehicle(seat));put<std::int16_t>(0x1301f2,0x700);
    put<std::uint16_t>(0x110000+13*12,51);CHECK(!engine.ReadVehicle(seat));put<std::uint16_t>(0x110000+13*12,50);
    put<unsigned>(0x6f1d20,1);CHECK(!engine.ReadVehicle(seat));put<unsigned>(0x6f1d20,0);
    put<float>(0x1300e0,0);CHECK(engine.ReadVehicle(seat));
    put<std::uint16_t>(0x130106,4);CHECK(!engine.ReadVehicle(seat));put<std::uint16_t>(0x130106,0);
    // Native grenade root can be translated/rotated in its own model. The
    // visible root must still land exactly on the chest/hand target, without
    // inheriting model placement offsets from a scaled bind pose.
    put<float>(0x1300e0,1);put<Object>(0x13011c,None);
    memory[0x746fa0].resize(4);put<unsigned>(0x746fa0,0x290000);memory[0x290000].resize(0x200);
    put<unsigned>(0x290128,2);put<unsigned>(0x29012c,0x291000);memory[0x291000].resize(2*0x44);
    for(unsigned type=0;type<2;++type){
        const unsigned proj=24+type,model=26+type,data=0x292000+type*0x1000,modelData=0x294000+type*0x1000,nodeData=0x296000+type*0x1000;
        tag(proj,0x70726f6a,data,0x40);tag(model,0x6d6f6432,modelData,0xc0);put<Object>(data+0x34,model);
        put<Object>(0x291040+type*0x44,proj);put<unsigned>(modelData+0xb8,1);put<unsigned>(modelData+0xbc,nodeData);
        memory[nodeData].resize(sizeof(ModelNode));ModelNode root{};std::strcpy(root.name,"root");root.parent=-1;
        root.inverseBind=ModelTransform(Matrix4().rotateY(30).translate(.3f,-.4f,.2f).invertAffine());put(nodeData,root);
        for(float scale:{.32f,.5f}){
            Matrix4 target=Matrix4().scale(scale).rotateZ(40).translate(5,6,7);
            const auto beforeModel=memory[nodeData];CHECK(engine.DrawGrenade(type,target,false));
            CHECK((drawnPoints.back()-Vector3(5,6,7)).length()<.0001f);CHECK(std::fabs(drawnScales.back()-scale)<.0001f);
            CHECK(memory[nodeData]==beforeModel);
        }
    }
}

int main(){
    checkARAccuracy();
    checkPhysicalNative();checkOnFootBody();
    checkReloadTransactions();
    Immersion::RetailWeaponEngine engine;Immersion::NativeInventory out;
    setup();
    Immersion::FireState fire;
    CHECK(!engine.ReadFireState(weapon,fire)); // Wrong/missing parent rejected.
    put<std::uint32_t>(0x14011c,unit);
    put<std::uint32_t>(0x1402d0,1234);
    put<std::int16_t>(0x1402b8,55);put<std::int16_t>(0x1402c4,7);
    put<float>(0x14023c,0.3f);put<float>(0x140240,0.2f);
    CHECK(engine.ReadFireState(weapon,fire));
    CHECK(fire.unit==unit && fire.weapon==weapon && fire.tick==1234);
    CHECK(fire.loaded[0]==55 && fire.loaded[1]==7 && fire.heat==0.3f && fire.energyUsed==0.2f);
    CHECK(!engine.ReadFireState(loose,fire));
    CHECK(!engine.ReadFireState(weapon+(1u<<16),fire));
    put<std::uint16_t>(0x110006+11*12,0x2cc);CHECK(!engine.ReadFireState(weapon,fire));
    put<std::uint16_t>(0x110006+11*12,0x340);
    engine.recoilAvailable=false;CHECK(!engine.ReadFireState(weapon,fire));engine.recoilAvailable=true;
    put<float>(0x14023c,std::numeric_limits<float>::quiet_NaN());CHECK(!engine.ReadFireState(weapon,fire));
    setup();CHECK(engine.Read(out));CHECK(out.unit==unit);CHECK(out.current==weapon);CHECK(out.weapons[1]==Immersion::None);
    put<std::uint32_t>(0x130118,0x9999000f);CHECK(engine.Read(out));CHECK(out.current==weapon); // Child is not selection.
    put<std::int16_t>(0x1302f2,-1);CHECK(engine.Read(out));CHECK(out.current==Immersion::None);CHECK(out.Contains(weapon));
    setup();put<std::uint16_t>(0x110000+11*12,44);CHECK(!engine.Read(out)); // Reused index / old generation.
    setup();put<std::uint16_t>(0x110006+11*12,0x200);CHECK(!engine.Read(out));
    setup();put<std::uint16_t>(0x1400b4,5);CHECK(!engine.Read(out)); // A projectile is not a weapon.
    setup();put<std::uint16_t>(0x100022,16);CHECK(!engine.Read(out));
    setup();put<std::uint16_t>(0x100020,10);CHECK(!engine.Read(out));
    setup();put<std::uint32_t>(0x130218,14u<<16);CHECK(!engine.Read(out));
    setup();put<std::uint16_t>(0x121002,1);CHECK(!engine.Read(out));
    setup();put<std::uint32_t>(0x6f1d20,0x777777);CHECK(!engine.Read(out)); // Multiplayer dispatch active.
    setup();put<float>(0x1300e0,0);CHECK(engine.Read(out));
    put<std::uint16_t>(0x130106,4);CHECK(!engine.Read(out));CHECK(engine.readMayResume);
    CHECK(std::strstr(engine.readStatus,"dead"));
    setup();put<float>(0x1300e0,std::numeric_limits<float>::quiet_NaN());CHECK(!engine.Read(out));
    setup();put<std::uint32_t>(0x13011c,0x123456);CHECK(!engine.Read(out)); // Vehicle parent.
    setup();put<std::int16_t>(0x1302f2,4);CHECK(!engine.Read(out));
    setup();put<std::int16_t>(0x1302f4,-2);CHECK(!engine.Read(out));
    setup();put<std::uint32_t>(0x1302fc,weapon);CHECK(!engine.Read(out)); // Duplicate inventory handles.
    setup();memory.erase(0x140000);CHECK(!engine.Read(out)); // Unreadable weapon memory.
    setup();engine.available=false;CHECK(!engine.Read(out));
    setup();engine.available=true;put<std::uint16_t>(0x121000,0);CHECK(!engine.Read(out));
    setup();put<std::uint16_t>(0x120030,0);CHECK(!engine.Read(out)); // Native allocation count, verified in retail binary.
    setup();engine.available=true;
    auto look=[&](float degrees,bool valid=true){float angle=degrees*3.14159265f/180;return engine.RecordLook({std::cos(angle),0,std::sin(angle)},valid,1.0f/90);};
    auto flags=[](){std::uint32_t value=0;SIZE_T done;ReadProcessMemory(nullptr,reinterpret_cast<void*>(0x150000),&value,4,&done);return value;};
    put<std::uint32_t>(0x150000,0x20);CHECK(look(0)==0);CHECK(look(5)==0x80);CHECK(flags()==0xa0);
    CHECK(look(10)==0);CHECK(flags()==0xa0);CHECK(look(0)==0x100);CHECK(flags()==0x1a0);
    put<std::uint32_t>(0x150000,0);CHECK(look(-5)==0);CHECK(flags()==0); // Native script reset rebaselines.
    CHECK(look(-10)==0x100);CHECK(flags()==0x100);
    CHECK(look(0,false)==0);CHECK(look(0)==0);CHECK(look(5)==0x80);
    engine.codeValid=false;put<std::uint32_t>(0x150000,0x20);CHECK(look(-5)==0);CHECK(flags()==0x20);
    engine.codeValid=true;put<std::uint32_t>(0x6f1d20,1);CHECK(look(10)==0);CHECK(flags()==0x20);
    put<std::uint32_t>(0x6f1d20,0);CHECK(look(0)==0); // Campaign return starts from a fresh sample.
    memory.erase(0x150000);CHECK(look(5)==0); // Unreadable history never writes.
    setup();engine.RecordLook({},false,0);
    put<std::uint32_t>(0x1302f8,Immersion::None);put<std::int16_t>(0x1302f2,-1);put<std::int16_t>(0x1302f4,-1);
    CHECK(engine.Read(out));CHECK(out.current==Immersion::None);
    CHECK(look(0)==0);CHECK(look(5)==0x80);CHECK(flags()==0x80); // Unarmed cryo player can record a look.
    setup();engine.RecordLook({},false,0);
    put<std::uint32_t>(0x13011c,0x123456); // Parented cryotube, not eligible for weapon handling.
    CHECK(!engine.Read(out));CHECK(look(0)==0);CHECK(look(5)==0x80);CHECK(flags()==0x80);
    put<std::int16_t>(0x1302f2,4); // Inventory transition also must not block tutorial history.
    CHECK(look(-5)==0x100);CHECK(flags()==0x180);
    put<std::uint32_t>(0x150000,0);engine.RecordLook({},false,0);
    for(int n=0;n<9;++n)engine.RecordLook({1,0,0},true,.01f,1,0);
    CHECK(flags()==0x400); // Right-stick right works without mouse/head movement.
    for(int n=0;n<9;++n)engine.RecordLook({1,0,0},true,.01f,-1,1);
    CHECK(flags()==0x680);
    for(int n=0;n<9;++n)engine.RecordLook({1,0,0},true,.01f,0,-1);
    CHECK((flags()&0x780)==0x780); // Same all-four native tutorial predicate.
    put<std::uint32_t>(0x150010,unit+1);put<std::uint32_t>(0x150000,0);
    CHECK(look(15)==0);CHECK(flags()==0); // Refuse stale control ownership.
    setup();CHECK(engine.Read(out));CHECK(out.player==localPlayer);
    CHECK(engine.HandleWorldItem(localPlayer,loose));CHECK(Immersion::offered==1);CHECK(Immersion::added==0);
    CHECK(Immersion::offeredAction==7);CHECK(engine.Read(out));CHECK(!out.Contains(loose));
    CHECK(engine.PickupTarget()==loose);CHECK(!engine.Pickup(unit,loose)); // Already holding a gun.
    put<std::int16_t>(0x1302f2,-1);CHECK(engine.Pickup(unit,loose));CHECK(Immersion::added==1);
    CHECK(engine.Read(out));CHECK(out.Contains(weapon));CHECK(out.Contains(loose));CHECK(out.current==loose);
    CHECK(engine.PickupTarget()==Immersion::None);CHECK(!engine.Pickup(unit,loose));CHECK(Immersion::added==1);
    setup();put<std::int16_t>(0x1302f2,-1);put<std::uint32_t>(0x1302f8,Immersion::None);
    CHECK(engine.HandleWorldItem(localPlayer,loose));CHECK(engine.Pickup(unit,loose)); // Completely unarmed.
    setup();engine.pickupEnabled=false;CHECK(!engine.HandleWorldItem(localPlayer,loose));CHECK(engine.PickupTarget()==Immersion::None);
    engine.pickupEnabled=true;
    setup();CHECK(!engine.HandleWorldItem(14u<<16,loose));CHECK(Immersion::offered==0); // Other player.
    setup();put<std::uint32_t>(0x6f1d20,1);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Multiplayer.
    setup();put<std::uint32_t>(0x160000,123);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Same-tag ammo scavenging preserved.
    setup();put<std::uint16_t>(0x1600b4,3);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Equipment/grenade pickup preserved.
    setup();put<std::uint32_t>(0x16011c,unit);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Not a world object.
    setup();put<std::uint32_t>(0x160200,unit);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Native recent-drop cooldown.
    setup();put<std::uint32_t>(0x160010,0);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Native not-pickable flag.
    setup();put<std::uint16_t>(0x110000+12*12,45);CHECK(!engine.HandleWorldItem(localPlayer,loose)); // Stale generation.
    setup();Immersion::canUse=false;CHECK(engine.HandleWorldItem(localPlayer,loose));CHECK(Immersion::offered==0);
    setup();engine.codeValid=false;CHECK(engine.HandleWorldItem(localPlayer,loose));CHECK(Immersion::offered==0);engine.codeValid=true;
    setup();put<std::int16_t>(0x1302f2,-1);CHECK(engine.HandleWorldItem(localPlayer,loose));
    CHECK(!engine.Pickup(unit,weapon));CHECK(Immersion::added==0); // Wrong prompt target.
    put<std::int16_t>(0x121028,3);CHECK(engine.PickupTarget()==Immersion::None);CHECK(!engine.Pickup(unit,loose));
    put<std::int16_t>(0x121028,7);Immersion::addSuccess=false;CHECK(!engine.Pickup(unit,loose));CHECK(engine.Read(out));CHECK(!out.Contains(loose));
    setup();put<std::int16_t>(0x1302f2,-1);CHECK(engine.HandleWorldItem(localPlayer,loose));
    Immersion::selectSuccess=false;CHECK(!engine.Pickup(unit,loose));CHECK(engine.Read(out));CHECK(out.Contains(loose)); // Partial commit survives.
    setup(); // Full inventory with a distinct second gun: prompt, no forced swap.
    memory[0x170000]=memory[0x160000];put<std::uint32_t>(0x170000,789);
    const auto third=(45u<<16)|13u;put<std::uint16_t>(0x110000+13*12,45);
    put<std::uint16_t>(0x110006+13*12,0x340);put<std::uint32_t>(0x110008+13*12,0x170000);
    put<std::uint32_t>(0x1302fc,third);put<std::int16_t>(0x1302f2,-1);
    CHECK(engine.HandleWorldItem(localPlayer,loose));CHECK(Immersion::offeredAction==6);
    CHECK(engine.PickupTarget()==loose);CHECK(!engine.Pickup(unit,loose));CHECK(Immersion::added==0);
    CHECK(engine.Read(out));CHECK(out.Contains(weapon));CHECK(out.Contains(third));CHECK(!out.Contains(loose));
    // First-person rendering is a separate state, with live effects even if
    // native ready(-1) already removed its weapon ID. Never patch at startup.
    setup();Immersion::refreshes=0;
    CHECK(!engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==0);
    memory[0x6b2d98].resize(4);memory[0x200000].resize(0x1ea0);
    put<std::uint32_t>(0x6b2d98,0x200000);
    put<std::uint32_t>(0x200004,unit+1);
    CHECK(!engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==0);
    put<std::uint32_t>(0x200004,unit);put<std::uint32_t>(0x200008,Immersion::None);
    CHECK(!engine.ViewModelReady(weapon));
    Immersion::fingerprintValid=false;
    CHECK(!engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==0);
    Immersion::fingerprintValid=true;
    CHECK(engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==1);
    CHECK(engine.ViewModelReady(weapon));
    CHECK(engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==1); // No per-frame restart.
    put<std::int16_t>(0x1302f2,-1);
    CHECK(engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==2);
    CHECK(!engine.ViewModelReady(weapon));CHECK(!engine.ViewModelReady(Immersion::None));
    unsigned char visible=1;Immersion::ReadAt(0x200000,visible);CHECK(!visible);
    // A render/engine tick can retain or select an inventory weapon while the
    // physical hand is empty. Stowing must not repeatedly clear its FP datum.
    put<std::int16_t>(0x1302f2,0);
    CHECK(engine.Select(unit,Immersion::None));
    const auto baseline=Immersion::refreshes;Immersion::visualOwner=&engine;
    for (int tick=0;tick<300;++tick) {
        put<std::uint32_t>(0x200008,weapon);put<unsigned char>(0x200000,1);
        CHECK(engine.Read(out));CHECK(out.current==Immersion::None);
        CHECK(engine.ReadNative(out));CHECK(out.current==weapon);
        CHECK(engine.HideNativeWeapon());CHECK(engine.SynchronizeViewModel(true));
        CHECK(Immersion::refreshes==baseline);
        Immersion::OnPose();Immersion::OnWeaponHUD(nullptr);
        CHECK(Immersion::poseCalls==0 && Immersion::hudCalls==0);
        CHECK(Immersion::visibilityCalls==static_cast<unsigned>(tick+1));
        Immersion::ReadAt(0x200000,visible);CHECK(!visible);

    }
    CHECK(engine.Select(unit,weapon));CHECK(!engine.HideNativeWeapon());
    Immersion::OnPose();Immersion::OnWeaponHUD(nullptr);
    CHECK(Immersion::poseCalls==1 && Immersion::hudCalls==1);

    CHECK(engine.Read(out));CHECK(out.current==weapon);
    CHECK(engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==baseline+1);
    put<std::uint32_t>(0x1302fc,loose); // AR/pistol repeatedly select different native slots.
    for(int i=0;i<24;++i) {
        const auto id=i%2?weapon:loose;
        put<std::int16_t>(0x1302f2,i%2?0:1);
        CHECK(!engine.ViewModelReady(id));CHECK(engine.SynchronizeViewModel(true));CHECK(engine.ViewModelReady(id));
    }
    CHECK(engine.Select(unit,Immersion::None));
    CHECK(engine.Select(unit,loose));
    put<Immersion::Object>(0x200008,loose); // Native ready reports correct ID already.
    put<unsigned>(0x201d8e,0xdeadbeef); // Stale skeleton remap from former weapon.
    const auto beforeReset=Immersion::refreshes;
    CHECK(!engine.ViewModelReady(loose));
    CHECK(!engine.SynchronizeViewModel()); // Input is not allowed to rebuild early.
    CHECK(Immersion::refreshes==beforeReset);
    Immersion::OnPose();
    CHECK(Immersion::refreshes==beforeReset+1);CHECK(engine.ViewModelReady(loose));
    unsigned remap=0;Immersion::ReadAt(0x201d8e,remap);CHECK(remap==loose);
    for (int frame=0;frame<30;++frame) Immersion::OnPose();
    CHECK(Immersion::refreshes==beforeReset+1);
    CHECK(engine.Select(unit,Immersion::None));CHECK(engine.Select(unit,weapon));
    put<Immersion::Object>(0x200008,weapon);put<unsigned>(0x201d8e,0xdeadbeef);
    CHECK(!engine.ViewModelReady(weapon));Immersion::OnPose();
    CHECK(engine.ViewModelReady(weapon));Immersion::ReadAt(0x201d8e,remap);CHECK(remap==weapon);
    // Reproduction: the input command still names a valid holstered gun.
    // Halo retains that slot (0x471c29), submits it in each action (0x471ed3),
    // then writes unit+2f4 from control data (0x563a69). R1.6.1 only changed the
    // unit, so this simulated tick undid the draw and restarted its model.
    const auto tick=[&]() {
        std::int16_t command=-1,current=-1;
        Immersion::ReadAt(0x150030,command);Immersion::ReadAt(0x1302f2,current);
        if(command>=0)put<std::int16_t>(0x1302f4,command);
        if(command>=0 && current!=command)Immersion::ReadyWeapon(unit);
        Immersion::OnPose();
    };
    put<std::uint32_t>(0x150014,0x1234abcd); // Neighbor input fields must survive.
    put<std::uint16_t>(0x150032,1);put<std::uint16_t>(0x150034,2);
    for(unsigned draw=0;draw<8;++draw) {
        const auto id=draw%2 ? weapon : loose;
        const short slot=draw%2 ? 0 : 1;
        CHECK(engine.Select(unit,Immersion::None));CHECK(engine.Select(unit,id));
        const auto readyCount=Immersion::readyCalls,resetCount=Immersion::refreshes;
        for(unsigned frame=0;frame<120;++frame) {
            tick();CHECK(engine.Read(out));CHECK(out.current==id);
            CHECK(engine.ViewModelReady(id));CHECK(Immersion::readyCalls==readyCount);
            CHECK(Immersion::refreshes==resetCount+1);
            CHECK(out.weapons[0]==weapon && out.weapons[1]==loose);
        }
        std::int16_t command=-1;Immersion::ReadAt(0x150030,command);CHECK(command==slot);
    }
    unsigned flagsAfter=0;Immersion::ReadAt(0x150014,flagsAfter);CHECK(flagsAfter==0x1234abcd);
    std::uint16_t grenade=0,zoom=0;Immersion::ReadAt(0x150032,grenade);Immersion::ReadAt(0x150034,zoom);
    CHECK(grenade==1 && zoom==2);
    // Bad owner/pointer/slot: do not partially mutate the live biped.
    for(unsigned fault=0;fault<3;++fault) {
        put<std::uint32_t>(0x150010,fault==0 ? unit+0x10000 : unit);
        put<std::uint32_t>(0x6b145c,fault==1 ? 0xdeadbeef : 0x150000);
        put<std::int16_t>(0x150030,fault==2 ? 4 : 0);
        CHECK(!engine.Select(unit,loose));CHECK(engine.Read(out));CHECK(out.current==weapon);
        std::int16_t desired=-1;Immersion::ReadAt(0x1302f4,desired);CHECK(desired==0);
    }
    put<std::uint32_t>(0x6b145c,0x150000);put<std::uint32_t>(0x150010,unit);put<std::int16_t>(0x150030,0);
    Immersion::selectSuccess=false;CHECK(!engine.Select(unit,loose));Immersion::selectSuccess=true;
    std::int16_t command=-1,desired=-1;Immersion::ReadAt(0x150030,command);Immersion::ReadAt(0x1302f4,desired);
    CHECK(command==0 && desired==0); // Failed transaction restores both choices.
    memory[0x87bc14].resize(4);memory[0x210000].resize(0x2000);memory[0x220000].resize(0x500);
    put<std::uint32_t>(0x87bc14,0x210000);
    constexpr std::uintptr_t tag=0x210000+123*0x20;
    put<std::uint32_t>(tag,0x77656170);put<std::uint32_t>(tag+0xc,123);put<std::uint32_t>(tag+0x14,0x220000);
    put<std::uint32_t>(0x220478,0x432100ab);
    Immersion::Object w,t,anim;
    CHECK(engine.ViewModelWeapon(w,t,anim));CHECK(w==weapon && t==123 && anim==0x432100ab);
    put<std::uint32_t>(tag+0xc,124);CHECK(!engine.ViewModelWeapon(w,t,anim));
    put<std::uint32_t>(tag+0xc,123);put<std::uint32_t>(0x200008,loose);
    CHECK(!engine.ViewModelWeapon(w,t,anim)); // Never use holstered child object as model source.
    const auto calls=Immersion::refreshes;engine.available=false;
    CHECK(!engine.SynchronizeViewModel(true));CHECK(Immersion::refreshes==calls);
    engine.available=true;
    // Actual render adapter: loaded map models, independent hand transforms,
    // and the stored gun, without touching native inventory or FP animation.
    memory[0x7c3108].resize(4);put<std::int16_t>(0x7c3108,0);
    memory[0x746fa0].resize(4);put<std::uint32_t>(0x746fa0,0x230000);
    memory[0x230000].resize(0x190);put<std::uint32_t>(0x23017c,1);put<std::uint32_t>(0x230180,0x240000);
    memory[0x240000].resize(16);put<std::uint32_t>(0x24000c,77);
    for(unsigned id:{77u,80u}) {
        const auto modelTag=0x210000+id*0x20;
        const std::uint32_t modelData=0x250000+(id-77)*0x10000;
        const std::uint32_t nodeData=modelData+0x1000;
        put<std::uint32_t>(modelTag,0x6d6f6432);put<std::uint32_t>(modelTag+0xc,id);put<std::uint32_t>(modelTag+0x14,modelData);
        memory[modelData].resize(0xc0);put<unsigned>(modelData+0xb8,3);put<std::uint32_t>(modelData+0xbc,nodeData);
        memory[nodeData].resize(3*sizeof(Immersion::ModelNode));
        for(unsigned n=0;n<3;++n) {
            Immersion::ModelNode node{};node.parent=n==0?-1:0;
            std::strcpy(node.name,n==1?"l wrist":n==2?"r wrist":"root");
            node.inverseBind=Immersion::ModelTransform(Matrix4());put(nodeData+n*sizeof(node),node);
        }
    }
    put<std::uint32_t>(0x220034,80);
    Matrix4 hands[2]{Matrix4().translate({1,2,3}),Matrix4().translate({4,5,6})};
    Matrix4 markerPoses[2]{Matrix4().translate({1,3,5}),Matrix4().rotateY(90).translate({7,8,9})};
    std::array<Immersion::HolsterMarker,2> markers{{{Immersion::None,false},{weapon,false}}};
    Immersion::NativeInventory saved,after;CHECK(engine.ReadNative(saved));
    CHECK(engine.DrawBodyModels(true,markers,hands,markerPoses));
    CHECK(Immersion::drawnModels==std::vector<Immersion::Object>({77,80}));
    CHECK(Immersion::drawnPoints[1]==Vector3(7,8,9));
    CHECK(engine.ReadNative(after));CHECK(after.current==saved.current && after.weapons==saved.weapons);
    CHECK(std::fabs(Immersion::drawnScales[1]-0.30f)<0.00001f); // Unknown/big world model.
    // Both homes survive drawing: the equipped home renders the same model in grey.
    markers[0]={weapon,true};markers[1]={Immersion::None,false};
    Immersion::drawnModels.clear();Immersion::drawnGreys.clear();Immersion::drawnPoints.clear();
    CHECK(engine.DrawBodyModels(false,markers,hands,markerPoses));
    CHECK(Immersion::drawnModels==std::vector<Immersion::Object>({80}));CHECK(Immersion::drawnGreys[0]);
    CHECK(Immersion::drawnPoints[0]==Vector3(1,3,5));
    CHECK(std::fabs(Immersion::drawnScales.back()-0.18f)<0.00001f);
    markers[0]={Immersion::None,false};markers[1]={weapon,false};
    // Salt/tag mismatch or invalid render context must fail before native draw.
    Immersion::drawnModels.clear();put<std::uint32_t>(0x210000+80*0x20+0xc,81);
    CHECK(!engine.DrawBodyModels(false,markers,hands,markerPoses));CHECK(Immersion::drawnModels.empty());
    put<std::int16_t>(0x7c3108,-1);
    CHECK(!engine.DrawBodyModels(true,markers,hands,markerPoses));CHECK(Immersion::drawnModels.empty());
    std::cout<<checks<<" production native-reader/pickup checks passed (synthetic memory)\n";
}
