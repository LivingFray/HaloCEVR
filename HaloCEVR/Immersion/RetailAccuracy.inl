// Included after RetailInteractions.inl; shares checked native readers.
// Schema: SnowyMouse/invader, src/tag/hek/definition/weapon.json.
// Weapon +0x4fc trigger block; 0x114-byte trigger has minimum error at
// +0x78 and error-angle bounds at +0x7c/+0x80. Only the local campaign AR
// primary shot borrows these values; restore before native fire returns.
bool RetailWeaponEngine::ReadARSpread(Object weapon,ARSpreadPatch& patch,float scale) const {
    patch={};NativeInventory native;std::array<unsigned char,0x2cc> body{};std::uintptr_t address=0;
    if(!available || !ReadNative(native) || native.current!=weapon || !native.Contains(weapon) ||
        !WeaponBytes(weapon,body,address) || Field<Object>(body,0x11c)!=native.unit)return false;
    char path[256]{};std::uint32_t count=0;patch.tag=Field<Object>(body,0);
    if(!TagPath(patch.tag,path) || std::strcmp(path,"weapons\\assault rifle\\assault rifle")!=0 ||
        !TagData(patch.tag,0x77656170u,patch.data) || !ReadAt(patch.data+0x4fc,count) || count!=1 ||
        !ReadAt(patch.data+0x500,patch.trigger) || !ReadAt(patch.trigger+0x78,patch.original))return false;
    for(unsigned i=0;i<3;++i){
        const float angle=patch.original[i];
        if(!std::isfinite(angle) || angle<0 || angle>.7853982f)return false;
        patch.tuned[i]=angle*ARSpreadScale(scale);
    }
    return patch.original[2]>=patch.original[1] && patch.original[2]>0;
}
bool RetailWeaponEngine::WriteARSpread(ARSpreadPatch& patch,bool apply){
    if(patch.tag==None || apply==patch.applied)return true;
    std::uint32_t data=0,count=0,trigger=0;std::array<float,3> current{};
    if(!TagData(patch.tag,0x77656170u,data) || data!=patch.data ||
        !ReadAt(data+0x4fc,count) || count!=1 || !ReadAt(data+0x500,trigger) || trigger!=patch.trigger ||
        !ReadAt(trigger+0x78,current) || current!=(apply?patch.original:patch.tuned))return false;
    const auto& desired=apply?patch.tuned:patch.original;
    if(!Store(trigger+0x78,desired.data(),sizeof(desired)))return false;
    patch.applied=apply;return true;
}
void RetailWeaponEngine::BeginAccuracyShot(Object weapon,short trigger,float scale){
    // A nested AI/other-weapon callback sees the original data; a nested local
    // AR shot starts from the original cone, never the parent's scaled cone.
    bool restored=true;
    if(!accuracyShots.empty())restored=WriteARSpread(accuracyShots.back(),false);
    ARSpreadPatch patch;
    if(restored && trigger==0 && ARSpreadScale(scale)<1.f && ReadARSpread(weapon,patch,scale)){
        if(WriteARSpread(patch,true) && accuracyReportedTag!=patch.tag){
            accuracyReportedTag=patch.tag;
            Logger::log<<"[ARAccuracy] local campaign AR spread scale="<<ARSpreadScale(scale)
                <<" cone_deg="<<patch.original[1]*57.2957795f<<".."<<patch.original[2]*57.2957795f
                <<" -> "<<patch.tuned[1]*57.2957795f<<".."<<patch.tuned[2]*57.2957795f
                <<"; damage/rate/range-limit unchanged"<<std::endl;
        }
    }
    accuracyShots.push_back(patch);
}
void RetailWeaponEngine::EndAccuracyShot(){
    if(accuracyShots.empty())return;
    if(!WriteARSpread(accuracyShots.back(),false))
        Logger::log<<"[ARAccuracy] shot restore skipped: tag changed during native fire"<<std::endl;
    accuracyShots.pop_back();
    if(!accuracyShots.empty())WriteARSpread(accuracyShots.back(),true);
}
float RetailWeaponEngine::AccuracyReticleScale(float scale) const {
    if(ARSpreadScale(scale)>=1.f)return 1.f;
    NativeInventory native;ARSpreadPatch patch;
    return ReadNative(native) && ReadARSpread(native.current,patch,scale)?ARSpreadScale(scale):1.f;
}
