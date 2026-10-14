#include "HaloCEVR/ReloadSoundBank.h"
#include <iostream>
#include <cstdlib>
using namespace Immersion;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
static Object warming=None;
static unsigned nativeStops=0;
static std::vector<ReloadPcm> played;
bool PlayNativeReloadSound(Object,Object sound,float volume){CHECK(volume<=.0001f);warming=sound;return true;}
void StopNativeReloadSound(){warming=None;++nativeStops;}
void StopReloadAudio(){warming=None;played.clear();}
void UpdateReloadPcm(){}
bool QueueReloadPcm(const ReloadPcm& pcm,float){played.push_back(pcm);return true;}
ReloadPcm recording(unsigned channels,unsigned parts){
    ReloadPcm p;p.rate=22050;p.channels=channels;p.samples.resize(22050*channels*(parts==4?3:2));
    const unsigned start[]={1000,13500,32000,48000},end[]={5200,20000,41000,60000};
    for(unsigned n=0;n<parts;++n)for(unsigned f=start[n];f<end[n];++f)for(unsigned c=0;c<channels;++c)
        p.samples[f*channels+c]=static_cast<std::int16_t>((n+1)*2000*(f%2?1:-1));
    return p;
}
int main(){
    for(unsigned channels:{1u,2u})for(unsigned parts:{1u,2u,3u,4u}){
        auto pcm=recording(channels,parts);const auto ranges=ReloadSoundRanges(pcm,parts);CHECK(ranges.size()==parts);
        for(unsigned n=0;n<parts;++n){
            ReloadPcm cut;CHECK(SliceReloadSound(pcm,{n,parts},cut));CHECK(cut.channels==channels);CHECK(cut.rate==22050);
            CHECK(cut.samples.size()<pcm.samples.size());CHECK(cut.samples.size()%channels==0);
            int peak=0;for(auto sample:cut.samples)peak=(std::max)(peak,std::abs(int(sample)));
            CHECK(peak==int((n+1)*2000));CHECK(cut.samples.front()==0);CHECK(cut.samples.back()==0);
        }
        ReloadPcm cut;CHECK(!SliceReloadSound(pcm,{parts,parts},cut));
    }
    for(unsigned channels:{1u,2u}){
        auto pcm=recording(channels,3);const auto ranges=ReloadSoundRanges(pcm,4);CHECK(ranges.size()==4);
        CHECK(ranges[1].begin==ranges[2].begin && ranges[1].end==ranges[2].end);
        for(unsigned n=0;n<4;++n){ReloadPcm cut;CHECK(SliceReloadSound(pcm,{n,4},cut));
            int peak=0;for(auto x:cut.samples)peak=(std::max)(peak,std::abs(int(x)));
            CHECK(peak==int((n==0?1:n==3?3:2)*2000));}
        CHECK(ReloadSoundRanges(recording(channels,2),4).empty());
    }
    auto flat=recording(1,1);CHECK(ReloadSoundRanges(flat,2).empty()); // No equal-time/full-clip fallback.
    flat.samples.assign(4000,0);CHECK(ReloadSoundRanges(flat,1).empty());
    flat.channels=3;CHECK(ReloadSoundRanges(flat,1).empty());
    struct Engine {
        bool available=true,corrupt=false;unsigned reads=0,changingUntil=0,partialUntil=0;
        bool ReadReloadPcm(Object,Object sound,ReloadPcm& out){
            CHECK(warming==sound);++reads;
            if(!available)return false;
            out=recording(1,reads<=partialUntil?1:3);
            if(reads<=changingUntil)out.samples[1500]+=static_cast<std::int16_t>(reads*10);
            if(corrupt)for(unsigned i=42000;i<44000;++i)out.samples[i]=i%2?30000:-30000;
            return true;
        }
    } engine;
    ReloadSoundBank bank;ReloadFeedbackAssets assets;assets.sounds[0]=80;assets.sounds[2]=80;
    assets.slices[0]={0,3};assets.slices[2]={2,3};bank.Prime(assets);
    bank.Play(80,{0,3},.6f);CHECK(played.empty());CHECK(bank.Update(engine,11,.02f));
    CHECK(warming==80);CHECK(engine.reads==0); // A decode request must precede any cache read.
    for(unsigned n=0;n<3;++n){bank.Update(engine,11,.02f);CHECK(played.empty());}
    for(unsigned n=0;n<6;++n)bank.Update(engine,11,.02f);
    CHECK(played.size()==1);CHECK(warming==None);CHECK(nativeStops>0);
    bank.Play(80,{1,3},.6f);bank.Play(80,{2,3},.6f);CHECK(played.size()==3);
    for(unsigned i=0;i<3;++i){int peak=0;for(auto sample:played[i].samples)peak=(std::max)(peak,std::abs(int(sample)));CHECK(peak==int((i+1)*2000));}
    { // Four rocket gestures use a warmed, owned clip through the actual bank.
        struct RocketEngine {unsigned parts;bool ReadReloadPcm(Object,Object,ReloadPcm& out){out=recording(1,parts);return true;}} rocket{4};
        for(unsigned groups:{4u,3u}){
            rocket.parts=groups;ReloadFeedbackAssets a;
            for(unsigned i=0;i<4;++i){a.sounds[i]=88;a.slices[i]={i,4};}
            ReloadSoundBank rocketBank;rocketBank.Prime(a);rocketBank.Play(88,{0,4},.6f);
            for(unsigned n=0;n<12;++n)rocketBank.Update(rocket,11,.02f);
            for(unsigned i=1;i<4;++i)rocketBank.Play(88,{i,4},.6f);
            CHECK(played.size()==4);rocketBank.Reset();
        }
        // Restore the earlier bank's playback assertions.
        bank.Play(80,{0,3},.6f);bank.Play(80,{1,3},.6f);bank.Play(80,{2,3},.6f);
    }
    const auto reads=engine.reads;bank.Update(engine,11,.02f);CHECK(engine.reads==reads); // Playback owns its copy.
    bank.Reset();CHECK(played.empty());bank.Prime(assets);engine.available=false;
    bank.Play(80,{0,3},.6f);bool reported=false;
    for(unsigned i=0;i<165;++i)if(!bank.Update(engine,11,.02f))reported=true;
    CHECK(reported);CHECK(played.empty());CHECK(warming==None);
    engine.available=true;bank.Play(80,{0,3},.6f);
    for(unsigned i=0;i<12;++i)bank.Update(engine,11,.02f);
    CHECK(played.size()==1); // Expired cues never replay as a burst.

    bank.Prime(assets);engine={};engine.partialUntil=5;bank.Play(80,{0,3},.6f);
    for(unsigned i=0;i<6;++i){bank.Update(engine,11,.02f);CHECK(played.empty());}
    for(unsigned i=0;i<10;++i)bank.Update(engine,11,.02f);
    CHECK(played.size()==1);CHECK(warming==None);
    bank.Play(80,{2,3},.6f);CHECK(played.size()==2);

    // Every snapshot contains all cues and has the same format/length, but
    // samples are still changing in place. A header-only check misses this.
    bank.Prime(assets);engine={};engine.changingUntil=5;bank.Play(80,{1,3},.6f);
    for(unsigned i=0;i<8;++i){bank.Update(engine,11,.02f);CHECK(played.empty());}
    for(unsigned i=0;i<8;++i)bank.Update(engine,11,.02f);
    CHECK(played.size()==1);CHECK(warming==None);

    // Stable garbage is still garbage. Reject the hot tail, then recover on
    // clean decoded samples without a weapon switch or replaying stale cues.
    bank.Prime(assets);engine={};engine.corrupt=true;bank.Play(80,{1,3},.6f);reported=false;
    for(unsigned i=0;i<12;++i)if(!bank.Update(engine,11,.02f))reported=true;
    CHECK(reported);CHECK(played.empty());engine.corrupt=false;
    for(unsigned i=0;i<10;++i)bank.Update(engine,11,.02f);
    CHECK(played.size()==1);CHECK(warming==None);

    for(unsigned channels:{1u,2u}){
        auto pcm=recording(channels,3);CHECK(PlausibleReloadPcm(pcm));
        for(unsigned i=32000;i<33000;++i)pcm.samples[i*channels]=i%2?30000:-30000;
        CHECK(!PlausibleReloadPcm(pcm));
        pcm=recording(channels,3);for(unsigned i=32000;i<33000;++i)pcm.samples[i*channels]=12000;
        CHECK(!PlausibleReloadPcm(pcm)); // Sustained DC / common uninitialised heap patterns.
        pcm=recording(channels,3);for(unsigned i=32000;i<33000;++i)pcm.samples[i*channels]=i%2?32767:-32768;
        CHECK(!PlausibleReloadPcm(pcm));
    }
    std::cout<<checks<<" native recording segmentation, stable-snapshot and corruption-recovery checks passed\n";
}
