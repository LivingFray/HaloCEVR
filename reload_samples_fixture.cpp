#include "HaloCEVR/ReloadAudio.h"
#include "HaloCEVR/Immersion/ReloadSoundSegments.h"
#include <deque>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstdlib>
using WORD=unsigned short;using DWORD=unsigned long;using LPSTR=char*;using HWAVEOUT=void*;
struct WAVEHDR {LPSTR lpData=nullptr;DWORD dwBufferLength=0,dwFlags=0;};
struct WAVEFORMATEX {WORD wFormatTag=0,nChannels=0;DWORD nSamplesPerSec=0,nAvgBytesPerSec=0;WORD nBlockAlign=0,wBitsPerSample=0;};
constexpr int MMSYSERR_NOERROR=0,WAVE_FORMAT_PCM=1,WAVE_MAPPER=-1,CALLBACK_NULL=0,WHDR_DONE=1;
static unsigned checks=0,opens=0,writes=0,closes=0,resets=0;static bool rejectOpen=false,rejectWrite=false;
static WAVEHDR* submitted=nullptr;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(false)
int waveOutOpen(HWAVEOUT* handle,int,const WAVEFORMATEX* f,int,int,int){
    CHECK(f->wBitsPerSample==16);CHECK(f->nBlockAlign==f->nChannels*2);++opens;
    if(rejectOpen)return 1;
    *handle=reinterpret_cast<void*>(1);return 0;
}
int waveOutClose(HWAVEOUT){++closes;return 0;}
int waveOutReset(HWAVEOUT){++resets;if(submitted)submitted->dwFlags|=WHDR_DONE;return 0;}
int waveOutPrepareHeader(HWAVEOUT,WAVEHDR* h,unsigned){h->dwFlags=0;return 0;}
int waveOutUnprepareHeader(HWAVEOUT,WAVEHDR* h,unsigned){if(submitted==h && !(h->dwFlags&WHDR_DONE))return 1;submitted=nullptr;return 0;}
int waveOutWrite(HWAVEOUT,WAVEHDR* h,unsigned){submitted=h;++writes;return rejectWrite?1:0;}
// INSERT_PRODUCTION_SAMPLES
int main(){
    Immersion::ReloadPcm pcm;pcm.rate=22050;pcm.channels=1;pcm.samples.assign(2205,2000);
    CHECK(QueueReloadPcm(pcm,.5f));CHECK(writes==1);CHECK(prepared);CHECK(playing.samples[100]==1000);
    const auto* storage=playing.samples.data();CHECK(QueueReloadPcm(pcm,1));CHECK(writes==1);CHECK(playing.samples.data()==storage);
    UpdateReloadPcm();CHECK(writes==1);submitted->dwFlags|=WHDR_DONE;UpdateReloadPcm();CHECK(writes==2);
    CHECK(playing.samples[100]==2000);StopReloadPcm();CHECK(!output);CHECK(!prepared);CHECK(playing.samples.empty());CHECK(queue.empty());
    CHECK(QueueReloadPcm(pcm,0));CHECK(!output);pcm.channels=3;CHECK(!QueueReloadPcm(pcm,1));
    pcm.channels=1;pcm.rate=44100;CHECK(QueueReloadPcm(pcm,1));CHECK(rate==44100);CHECK(opens==2);
    pcm.rate=22050;CHECK(QueueReloadPcm(pcm,1));submitted->dwFlags|=WHDR_DONE;UpdateReloadPcm();CHECK(rate==22050);CHECK(opens==3);
    StopReloadPcm();rejectOpen=true;CHECK(!QueueReloadPcm(pcm,1));CHECK(!output);CHECK(queue.empty());rejectOpen=false;
    rejectWrite=true;CHECK(!QueueReloadPcm(pcm,1));CHECK(!prepared);CHECK(playing.samples.empty());StopReloadPcm();CHECK(resets>0);CHECK(closes>0);
    rejectWrite=false;
    // Limit whole clips without hard clipping or amplifying quieter recordings.
    pcm.channels=2;pcm.samples.resize(4410);
    for(unsigned f=0;f<2205;++f){pcm.samples[2*f]=f%2?10000:-10000;pcm.samples[2*f+1]=pcm.samples[2*f]/2;}
    const auto original=pcm.samples;CHECK(QueueReloadPcm(pcm,.6f));CHECK(pcm.samples==original);
    double power=0;
    for(unsigned f=0;f<2205;++f){
        CHECK(std::abs(int(playing.samples[2*f])-2*playing.samples[2*f+1])<=1);
        CHECK(std::abs(int(playing.samples[2*f]))<=7200);
    }
    for(auto sample:playing.samples)power+=double(sample)*sample;
    CHECK(std::sqrt(power/playing.samples.size())<=2701);StopReloadPcm();
    pcm.channels=1;pcm.samples.assign(4410,0);
    for(unsigned i=1000;i<1044;++i)pcm.samples[i]=i%2?24000:-24000;
    CHECK(QueueReloadPcm(pcm,1));CHECK(playing.samples[1001]==12000);CHECK(playing.samples[1000]==-12000);StopReloadPcm();
    pcm.samples.assign(4410,32767);const auto count=writes;CHECK(!QueueReloadPcm(pcm,1));CHECK(writes==count);CHECK(!output);
    std::cout<<checks<<" production PCM queue, level-limiting and buffer-lifetime checks passed (WinMM doubles)\n";
}
