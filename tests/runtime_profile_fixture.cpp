// Compiled by runtime_profile.py against a temporary, read-only-derived image.
// Production fingerprint/gating AND the production crouch patch are inserted.
#include "HaloCEVR/Immersion/RetailProfile.h"
#include <array>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <iterator>
using byte=unsigned char;
static std::vector<byte> image;
static constexpr std::uintptr_t Base=0x400000;
namespace Logger {std::ostringstream log;}
static bool Copy(std::uintptr_t address,void* target,std::size_t size){
    if(address<Base || address-Base>image.size() || size>image.size()-(address-Base))return false;
    std::memcpy(target,image.data()+address-Base,size);return true;
}
namespace Immersion {
// INSERT_MATCH
class RetailWeaponEngine {
public:bool available=true,interactionAvailable=false,vehicleAvailable=false;
    void CheckInteractionCode();
};
// INSERT_GATE
}
struct Property {float value=.15f;float Value()const{return value;}};
struct Game {static Game instance;Property crouch;Property* c_CrouchHeight=&crouch;};
Game Game::instance;
namespace Hooks {
struct {struct {std::uintptr_t Address=0;} CrouchHeight;} o;
void SetBytes(std::uintptr_t address,int length,const byte* source){std::memcpy(reinterpret_cast<void*>(address),source,length);}
void P_DisableCrouchCamera();
}
// INSERT_CROUCH_PATCH
static unsigned checks=0;
#define CHECK(x)do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int main(int argc,char** argv){
    CHECK(argc==2);std::ifstream in(argv[1],std::ios::binary);CHECK(bool(in));
    image={std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};CHECK(image.size()==0x491000);
    CHECK(Immersion::MatchFingerprint(Immersion::RetailStructureVisibilityEntry));
    CHECK(Immersion::MatchFingerprint(Immersion::RetailStructurePvsBranch));
    const auto pristine=image;Immersion::RetailWeaponEngine engine;
    engine.CheckInteractionCode();CHECK(engine.interactionAvailable && engine.vehicleAvailable);
    Hooks::o.CrouchHeight.Address=reinterpret_cast<std::uintptr_t>(image.data()+0x168ff0);
    Hooks::P_DisableCrouchCamera();
    CHECK(!Immersion::MatchFingerprint(Immersion::RetailSeatCamera));
    CHECK(Immersion::MatchFingerprint({Immersion::RetailSeatCamera.rva,Immersion::RetailSeatCamera.length,Immersion::RetailSeatCameraCrouchDisabledFNV}));
    engine.CheckInteractionCode();CHECK(engine.interactionAvailable && engine.vehicleAvailable);
    // MinHook's throw-entry detour leaves the verified release INTERIOR alone.
    for(unsigned n=0;n<5;++n)image[0x16e440+n]=n?0:0xe9;
    engine.CheckInteractionCode();CHECK(engine.interactionAvailable && engine.vehicleAvailable);
    const auto patched=image;
    // Every unrecognised change still fails closed. No blanket skipped bytes.
    for(unsigned offset=0;offset<Immersion::RetailSeatCamera.length;++offset){
        image=patched;image[Immersion::RetailSeatCamera.rva+offset]^=0x40;
        engine.CheckInteractionCode();CHECK(engine.interactionAvailable && !engine.vehicleAvailable);
    }
    image=patched;image[Immersion::RetailGrenadeAttach.rva+10]^=0x40;
    engine.CheckInteractionCode();CHECK(!engine.interactionAvailable && engine.vehicleAvailable);
    image=patched;image[Immersion::RetailGrenadeCommand.rva+10]^=0x40;
    engine.CheckInteractionCode();CHECK(!engine.interactionAvailable && engine.vehicleAvailable);
    image=patched;image[Immersion::RetailObjectNodes.rva+10]^=0x40;
    engine.CheckInteractionCode();CHECK(engine.interactionAvailable && !engine.vehicleAvailable);
    image=pristine;Game::instance.crouch.value=-1;
    Hooks::o.CrouchHeight.Address=reinterpret_cast<std::uintptr_t>(image.data()+0x168ff0);
    Hooks::P_DisableCrouchCamera();CHECK(image==pristine);
    engine.CheckInteractionCode();CHECK(engine.interactionAvailable && engine.vehicleAvailable);
    engine.available=false;engine.CheckInteractionCode();CHECK(!engine.interactionAvailable && !engine.vehicleAvailable);
    CHECK(Logger::log.str().find("seat_camera_vr=1")!=std::string::npos);
    std::cout<<checks<<" runtime-patched profile checks passed (actual crouch patch and production feature gates)\n";
}
