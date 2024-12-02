<p align="center">
  <img width="460" height="215" src="./Bindings/icon.png">
</p>

# Halo: Combat Evolved VR
A full VR conversion mod for the original 2003 PC edition of _Halo: Combat Evolved_.

[Mod Download here](../../releases)

**This mod is not compatible with _The Master Chief Collection_**

## Features
* 6DoF Synced Stereo view (i.e. VR camera without any AER/3D screen nonsense)
* Tracked controllers
* 6DoF Weapon aiming
* 6DoF Grenade aiming
* Functional PiP scope for appropriate weapons
* Two handed aiming mode
* Rebindable controls (with left handed preset available)
* Motion controlled melee (uses head-aiming)
* Motion controlled flashlight (tap head)
* Motion controlled crouching
* Detached floating UI layer
* Floating crosshair
* Joystick steered vehicles
* Desktop mirror (recommended to use SteamVR's desktop display instead)

## Known issues
* Game sometimes isn't the focused window on launch and inputs/sounds may break when the game is unfocused
* First stage of the tutorial ("look around") doesn't detect headset movement, wiggle the mouse and you should get passed it
* Camera behaves weirdly briefly when entering/exiting vehicles
* Reloading a checkpoint made while in a vehicle can mess with the camera position, get out and in again to fix it
* Melee and interact use head aiming rather than controller aiming
* Crosshair only lights up red when looking at an enemy, not when pointing a gun at one
* On screen button prompts display keyboard bindings rather than VR bindings
* If the game is installed in program files the mod config file (and log file) won't generate unless the game is run as administrator
* Occasional stutters can cause motion smoothing to kick in, which is very jarring in vehicles

## Installation
0. Install _Halo: Combat Evolved for PC_ using an original installation CD + product key
1. Install the 1.10 patch
2. (Optional) Install [chimera](https://github.com/SnowyMouse/chimera) (fixes a few bugs/issues such as entities moving at 30fps)
3. If using chimera: open chimera.ini, locate the "Font Override Settings" section, and change enabled=1 to enabled=0 (failing to do this will break many UI elements in VR)
4. Download the latest version of this mod from the [releases page](../../releases)
5. Extract HaloCEVR.zip and place the files in the same directory as the halo executable (You should see a VR folder, openvr_api.dll and d3d9.dll if done correctly)
6. Launch the game once to generate a config.txt file in the VR directory
7. If setting LeftHanded=true in the config, consider selecting the left handed controller bindings in the game's SteamVR controller bindings page

## Uninstalling
1. Go play the MCC version instead
2. Delete/rename the d3d9.dll you placed in the same directory as the halo executable (this contains all of the mod code and is only pretending to be d3d9 to trick halo into loading it)

## FAQ
### Why the gearbox port and not MCC?
Short answer: skill issue.

Long answer: This mod my first time attempting to reverse engineer and mod a close sourced game engine requiring learning how to decompile, analyse and patch x86 assembly.
As you may guess, this is hard. By focusing on an older title I have a simpler project to work with as I do not have an additional 2 decades worth of updates and advances in technology to worry about.

(For example, no dual classic/aniversary graphics, no MCC launcher application separate to the halo1 game code, DirectX9 rather than DirectX11, x64, simpler code layout with few virtual functions to follow (which I struggled to analyse in Ghidra).
### Where do I even get a CD copy in \<current year\>? Can I use a cracked version?
No.

This mod is intended to only be used with legitimate copies of Halo: Combat Evolved with the official 1.10 patch. Fortunately the product keys aren't single use, so if you can find a second hand copy (or a friend willing to lend the copy they happened to archive 20 years ago) you can use that without worrying about the key being invalid. 
### Does multiplayer work?
This mod was designed for singleplayer, it is untested in multiplayer and as such some features, such as weapon aiming, may not function or only work for the host.

Also it does not work in Co-op because this version of the game does not have that mode.
### How do I turn on the flashlight?
By default tapping your head with your left hand will toggle the flashlight, you can adjust the radius this triggers at and which hand to use in config.txt. Alternatively there is an optional binding you can configure in SteamVR's controller bindings page to toggle it directly.
### How do I melee/can melee be bound to a button on my controller?
By default swinging either controller vertically with enough speed will trigger a melee attack wherever you are looking. If preferred there is a controller binding that is unset by default you can configure instead.

### How do I switch weapons?
By default, you can switch weapons using a controller binding.
Alternatively, if enabled in the config.txt file, you can switch weapons using the shoulder weapon holsters. With this setting enabled, first hover your main hand over a shoulder, then press the switch weapon binding to change weapons.

### Things feel constantly jittery in vehicles
Halo internally runs a lower tick rate (I believe it is 30fps) and only interpolates the player camera, this makes things feel jittery when driving vehicles. To fix this install [chimera](https://github.com/SnowyMouse/chimera), as they have fixed this issue along with many others. If you still experience intermittent stuttering on vehicles it may be due to the motion smoothing kicking in and locking the frame rate to 45 for a few seconds, you may experience smoother results by disabling it. 
### Does this mod support OpenXR?
No, this mod is exclusively for SteamVR since it is the only runtime to support 32bit applications to my knowledge.
### Help the camera is too high/too low (or crouch seems to be stuck on)
Stand up straight (or sit up if playing seated) and hold down the menu button for 3 seconds to recentre the camera to your current physical position.

## Compiling Source
1. Make sure you're running VS 2022 and open the solution.
2. Ensure that C++ Language Standard is ISO C++ 17 Standard (/std:c++17). This can be found under HaloCEVR Properties -> Configuration Properties -> General.
3. Set solution build configuration to release and target x86.

This should be everything that needs to be done inside of the HaloCEVR project, however it is likely you will need to compile MinHook.
1. Go to https://github.com/TsudaKageyu/minhook and clone the project.
2. Open the file and navigate to build/VC17, open this solution.
3. Set solution build configuration to release and target x86.
4. Build the libMinHook project, the result of which should be a libMinHook.x86.lib file.
5. Replace the file libMinHook.x86.lib in the HaloCEVR folder found at HaloCEVR-master\ThirdParty\MinHook\lib with this newly compiled libMinHook.x86.lib file

You should now be able to build successfully, this will generate a d3d9.dll file, using the "makerelease" bat file we can create a zip containing the d3d9.dll and the VR folder. You will need to add openvr_api.dll yourself, either by adding to the release folder so the bat file can pickup copy and zip it, or by adding it directly into your halo CE installation directory.
