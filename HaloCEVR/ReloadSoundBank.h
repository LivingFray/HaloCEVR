#pragma once
#include "ReloadAudio.h"
#include "Immersion/ReloadSoundSegments.h"
#include <map>
#include <deque>
// Own PCM copies, never pointers into Halo's asynchronous decoder.
class ReloadSoundBank {
    struct Clip {
        Immersion::ReloadPcm pcm,snapshot;
        std::array<bool,5> parts{};
        float warmingAge=0,retryAfter=0,pollAfter=0,stableAge=0;
        unsigned stableReads=0;
    };
    struct Pending {Immersion::Object sound;Immersion::ReloadSoundSlice slice;float volume,age;};
    std::map<Immersion::Object,Clip> clips;
    std::deque<Pending> pending;
    Immersion::Object warming=Immersion::None;
    unsigned issues=0,reported=0;
    const char* lastIssue="none";
    void Issue(unsigned bit,const char* text){if(!(reported&bit)){issues|=bit;lastIssue=text;}}
    bool Submit(const Clip& clip,Immersion::ReloadSoundSlice slice,float volume){
        Immersion::ReloadPcm step;
        if(!SliceReloadSound(clip.pcm,slice,step)){Issue(1,"recording has no separable cue");return false;}
        if(!QueueReloadPcm(step,volume)){Issue(2,"Windows PCM output queue unavailable");return false;}
        return true;
    }
public:
    const char* LastIssue()const{return lastIssue;}
    void Reset(){clips.clear();pending.clear();warming=Immersion::None;issues=reported=0;lastIssue="none";StopReloadAudio();}
    void Prime(const Immersion::ReloadFeedbackAssets& assets){
        Reset();
        for(unsigned i=0;i<assets.sounds.size();++i)if(assets.sounds[i]!=Immersion::None){
            auto& clip=clips[assets.sounds[i]];
            const unsigned parts=assets.slices[i].parts;
            if(parts>=1 && parts<=4)clip.parts[parts]=true;
        }
    }
    void Play(Immersion::Object sound,Immersion::ReloadSoundSlice slice,float volume){
        if(sound==Immersion::None || volume<=0 || pending.size()>=6)return;
        const auto found=clips.find(sound);if(found==clips.end())return;
        if(pending.empty() && !found->second.pcm.samples.empty()){
            Submit(found->second,slice,volume);return;
        }
        // A fresh gesture may retry a cold/evicted native asset. Expired cues
        // are not replayed later as a burst after the user's gesture has ended.
        found->second.retryAfter=0;
        pending.push_back({sound,slice,volume,0});
    }
    template<class Engine> bool Update(Engine& engine,Immersion::Object weapon,float dt){
        UpdateReloadPcm();dt=std::isfinite(dt)?(std::max)(0.f,(std::min)(.05f,dt)):0.f;
        for(auto& item:clips)item.second.retryAfter=(std::max)(0.f,item.second.retryAfter-dt);
        for(auto& item:clips){
            auto& clip=item.second;
            if(!clip.pcm.samples.empty() || clip.retryAfter>0 || (warming!=Immersion::None && warming!=item.first))continue;
            if(warming==Immersion::None){
                warming=item.first;clip.warmingAge=clip.stableAge=0;clip.stableReads=0;clip.snapshot={};clip.pollAfter=.02f;
                // Request the game's normal decode at -80 dB. No full reload
                // clip is used as an audible fallback. Never accept an old
                // cache pointer BEFORE requesting this sound's decode.
                if(!PlayNativeReloadSound(weapon,item.first,.0001f)){
                    clip.retryAfter=1;warming=Immersion::None;Issue(4,"native decode request unavailable; retrying");
                }
            }else{
                clip.warmingAge+=dt;clip.pollAfter-=dt;
                if(clip.pollAfter<=0){
                    clip.pollAfter=.02f;Immersion::ReloadPcm candidate;
                    bool valid=engine.ReadReloadPcm(weapon,item.first,candidate) && !candidate.samples.empty();
                    if(valid && !Immersion::PlausibleReloadPcm(candidate)){
                        valid=false;Issue(32,"native PCM contains implausible levels; discarded and retrying");
                    }
                    if(valid)for(unsigned parts=1;parts<=4;++parts)if(clip.parts[parts] && Immersion::ReloadSoundRanges(candidate,parts).size()!=parts)valid=false;
                    if(valid){
                        const bool same=candidate.rate==clip.snapshot.rate && candidate.channels==clip.snapshot.channels && candidate.samples==clip.snapshot.samples;
                        if(same)++clip.stableReads;
                        else{clip.snapshot=std::move(candidate);clip.stableReads=1;clip.stableAge=clip.warmingAge;}
                        // Sample contents, not just the allocation header, must
                        // remain identical for at least 60 ms across three reads.
                        if(clip.stableReads>=3 && clip.warmingAge-clip.stableAge>=.06f){
                            clip.pcm=std::move(clip.snapshot);clip.snapshot={};
                            StopNativeReloadSound();warming=Immersion::None;clip.warmingAge=0;
                        }
                    }else{clip.snapshot={};clip.stableReads=0;clip.stableAge=clip.warmingAge;}
                }
                if(warming==item.first && clip.warmingAge>=3.f){
                    clip.retryAfter=1;clip.warmingAge=0;clip.snapshot={};clip.stableReads=0;
                    StopNativeReloadSound();warming=Immersion::None;
                    Issue(8,"native recording unstable, incomplete or inseparable; retrying");
                }
            }
            break; // At most one native buffer copy/load request per tick.
        }
        for(auto& cue:pending)cue.age+=dt;
        for(auto i=pending.begin();i!=pending.end();){
            const auto clip=clips.find(i->sound);
            if(clip==clips.end() || i->age>1.f){Issue(16,"cue expired waiting for native recording");i=pending.erase(i);continue;}
            if(clip->second.pcm.samples.empty())break;
            Submit(clip->second,i->slice,i->volume);i=pending.erase(i);
        }
        if(issues){reported|=issues;issues=0;return false;}return true;
    }
};
