#pragma once
#include "ReloadCue.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace Immersion {
struct ReloadSoundRange {std::size_t begin=0,end=0;};
// Gross decoder/heap noise is not a reload cue. Check each channel in 20 ms
// windows so a corrupt tail cannot hide behind a long silent prefix. This is
// a conservative corruption guard, not proof that arbitrary PCM is correct.
inline bool PlausibleReloadPcm(const ReloadPcm& pcm){
    if(pcm.rate<8000 || pcm.rate>48000 || pcm.channels<1 || pcm.channels>2 ||
        pcm.samples.empty() || pcm.samples.size()%pcm.channels || pcm.samples.size()>pcm.rate*pcm.channels*8)return false;
    const std::size_t frames=pcm.samples.size()/pcm.channels,window=(std::max)(1u,pcm.rate/50);
    for(std::size_t begin=0;begin<frames;begin+=window)for(unsigned c=0;c<pcm.channels;++c){
        const auto end=(std::min)(frames,begin+window);double sum=0,power=0;std::size_t clipped=0;
        for(auto f=begin;f<end;++f){const int value=pcm.samples[f*pcm.channels+c];
            sum+=value;power+=double(value)*value;if(std::abs(value)>=32760)++clipped;}
        const auto n=end-begin;
        if(n>=pcm.rate/200 && (std::fabs(sum/n)>6000 || power/n>14000.0*14000.0 || clipped*20>n))return false;
    }
    return true;
}
// Locate authored actions by their quiet gaps. Do not divide a recording into
// equal time chunks: that can put the next action into the previous gesture.
inline std::vector<ReloadSoundRange> ReloadSoundRanges(const ReloadPcm& pcm,unsigned parts){
    if(parts<1 || parts>4 || pcm.rate<8000 || pcm.rate>48000 || pcm.channels<1 || pcm.channels>2 ||
        pcm.samples.empty() || pcm.samples.size()%pcm.channels)return {};
    const std::size_t frames=pcm.samples.size()/pcm.channels,window=(std::max)(1u,pcm.rate/200);
    std::vector<float> energy;
    for(std::size_t begin=0;begin<frames;begin+=window){
        double sum=0;const auto end=(std::min)(frames,begin+window);
        for(auto f=begin;f<end;++f)for(unsigned c=0;c<pcm.channels;++c){const double v=pcm.samples[f*pcm.channels+c];sum+=v*v;}
        energy.push_back(static_cast<float>(std::sqrt(sum/((end-begin)*pcm.channels))));
    }
    const float peak=*std::max_element(energy.begin(),energy.end());
    if(peak<16)return {};
    const float threshold=(std::max)(12.f,peak*.045f);
    std::vector<ReloadSoundRange> groups;
    for(std::size_t i=0;i<energy.size();++i)if(energy[i]>=threshold){
        const auto begin=i;while(i+1<energy.size() && energy[i+1]>=threshold)++i;
        ReloadSoundRange range{begin*window,(std::min)(frames,(i+1)*window)};
        if(!groups.empty() && range.begin-groups.back().end<pcm.rate*.055f)groups.back().end=range.end;
        else groups.push_back(range);
    }
    // Ignore isolated clicks shorter than 10 ms when larger actions exist.
    if(groups.size()>parts)groups.erase(std::remove_if(groups.begin(),groups.end(),[&](const ReloadSoundRange& r){
        return r.end-r.begin<pcm.rate/100;
    }),groups.end());
    // Some rocket recordings combine extraction/insertion into one middle
    // action. Share that isolated cue between the two gestures; never substitute
    // the complete reload. Other weapons retain their exact segmentation rules.
    if(parts==4 && groups.size()==3)groups.insert(groups.begin()+2,groups[1]);
    if(groups.size()<parts)return {}; // Never replay a whole sequence as fallback.
    while(groups.size()>parts){
        std::size_t closest=0;
        for(std::size_t i=1;i+1<groups.size();++i)
            if(groups[i+1].begin-groups[i].end<groups[closest+1].begin-groups[closest].end)closest=i;
        groups[closest].end=groups[closest+1].end;groups.erase(groups.begin()+closest+1);
    }
    const std::size_t padding=pcm.rate/100;
    for(auto& r:groups){r.begin=r.begin>padding?r.begin-padding:0;r.end=(std::min)(frames,r.end+padding);}
    return groups;
}
inline bool SliceReloadSound(const ReloadPcm& source,ReloadSoundSlice slice,ReloadPcm& result){
    result={};const auto ranges=ReloadSoundRanges(source,slice.parts);
    if(slice.part>=ranges.size())return false;
    result.rate=source.rate;result.channels=source.channels;
    const auto range=ranges[slice.part];
    result.samples.assign(source.samples.begin()+range.begin*source.channels,source.samples.begin()+range.end*source.channels);
    // A 2 ms edge fade avoids cut clicks, without altering the interior recording.
    const std::size_t frames=result.samples.size()/result.channels,fade=(std::min)(frames/2,std::size_t(result.rate/500));
    for(std::size_t i=0;i<fade;++i)for(unsigned c=0;c<result.channels;++c){
        result.samples[i*result.channels+c]=static_cast<std::int16_t>(result.samples[i*result.channels+c]*float(i)/fade);
        const auto last=(frames-1-i)*result.channels+c;
        result.samples[last]=static_cast<std::int16_t>(result.samples[last]*float(i)/fade);
    }
    return true;
}
}
