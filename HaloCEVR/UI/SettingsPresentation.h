#pragma once
#include <string>
#include <cctype>
namespace SettingsPresentation {
inline bool Visible(const std::string& key,bool physicalWeapons,bool gripFlashlight,bool threeDOF){
    // Preserve saved keys and unsupported-game fallback behavior, but do not
    // offer controls superseded by this mode's grip/holster/flashlight logic.
    if(key=="StylishRecoilUnknown" || key=="StylishOnFootBody")return false;
    if(physicalWeapons && !threeDOF){
        for(const char* old:{"ToggleGrip","TwoHandDistance","SwapHandDistance","EnableWeaponHolsters",
            "LeftShoulderHolsterDistance","LeftShoulderHolsterOffset","RightShoulderHolsterDistance","RightShoulderHolsterOffset"})
            if(key==old)return false;
    }
    if(gripFlashlight){
        for(const char* old:{"OffhandHandFlashlight","LeftHandFlashlight","RightHandFlashlight"})if(key==old)return false;
    }
    if(!threeDOF && key.rfind("3DOF",0)==0)return false;
    return true;
}
inline std::string Label(std::string key){
    if(key.rfind("Stylish",0)==0)key.erase(0,7);
    if(key=="OnFootLegs")return "Show Legs";
    if(key=="GhostAimSensitivity")return "Ghost Aim Sensitivity";
    std::string out;
    for(unsigned i=0;i<key.size();++i){
        const auto c=static_cast<unsigned char>(key[i]);
        if(i && std::isupper(c) && (std::islower(static_cast<unsigned char>(key[i-1])) ||
            (i+1<key.size() && std::islower(static_cast<unsigned char>(key[i+1])) && std::isupper(static_cast<unsigned char>(key[i-1])))))out+=' ';
        out+=static_cast<char>(c);
    }
    return out;
}
}
