#include "ReloadAudio.h"
#include "Immersion/ReloadSoundSegments.h"
#include <Windows.h>
#include <mmsystem.h>
#include <deque>
#include <algorithm>
#include <cmath>
#pragma comment(lib,"winmm.lib")
namespace {
HWAVEOUT output=nullptr;
WAVEHDR header{};
bool prepared=false,playbackFailed=false;
unsigned rate=0,channels=0;
Immersion::ReloadPcm playing;
std::deque<Immersion::ReloadPcm> queue;
bool ReleaseBuffer(){
    if(prepared){if(waveOutUnprepareHeader(output,&header,sizeof(header))!=MMSYSERR_NOERROR)return false;prepared=false;}
    playing={};header={};return true;
}
}
void StopReloadPcm(){
    queue.clear();if(output){waveOutReset(output);if(!ReleaseBuffer())return;waveOutClose(output);output=nullptr;}
    rate=channels=0;
}
void UpdateReloadPcm(){
    playbackFailed=false;
    if(prepared){if(!(header.dwFlags&WHDR_DONE))return;if(!ReleaseBuffer())return;}
    if(queue.empty())return;
    const auto& next=queue.front();
    if(output && (rate!=next.rate || channels!=next.channels)){waveOutClose(output);output=nullptr;}
    if(!output){
        WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=static_cast<WORD>(next.channels);
        format.nSamplesPerSec=next.rate;format.wBitsPerSample=16;format.nBlockAlign=static_cast<WORD>(next.channels*2);
        format.nAvgBytesPerSec=next.rate*format.nBlockAlign;
        if(waveOutOpen(&output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){output=nullptr;queue.clear();playbackFailed=true;return;}
        rate=next.rate;channels=next.channels;
    }
    playing=std::move(queue.front());queue.pop_front();header={};
    header.lpData=reinterpret_cast<LPSTR>(playing.samples.data());header.dwBufferLength=static_cast<DWORD>(playing.samples.size()*2);
    if(waveOutPrepareHeader(output,&header,sizeof(header))!=MMSYSERR_NOERROR){playing={};playbackFailed=true;return;}
    prepared=true;
    if(waveOutWrite(output,&header,sizeof(header))!=MMSYSERR_NOERROR){waveOutReset(output);ReleaseBuffer();playbackFailed=true;}
}
bool QueueReloadPcm(const Immersion::ReloadPcm& pcm,float volume){
    if(pcm.rate<8000 || pcm.rate>48000 || pcm.channels<1 || pcm.channels>2 || pcm.samples.empty() ||
        pcm.samples.size()%pcm.channels || pcm.samples.size()>pcm.rate*pcm.channels*8 || !std::isfinite(volume) || queue.size()>=6)return false;
    if(volume<=0)return true;
    if(!Immersion::PlausibleReloadPcm(pcm))return false;
    // Attenuate the whole cue uniformly, never amplify or hard-clip it. This
    // preserves its waveform/stereo balance while bounding sudden loud bursts.
    int peak=0;double power=0;
    for(const auto sample:pcm.samples){peak=(std::max)(peak,std::abs(int(sample)));power+=double(sample)*sample;}
    const double rms=std::sqrt(power/pcm.samples.size());
    const float ceiling=(std::min)(1.f,(std::min)(peak?12000.f/peak:1.f,rms>0?float(4500.0/rms):1.f));
    auto scaled=pcm;volume=(std::min)(1.f,volume)*ceiling;
    for(auto& sample:scaled.samples)sample=static_cast<std::int16_t>(sample*volume);
    queue.push_back(std::move(scaled));UpdateReloadPcm();return !playbackFailed;
}
