<p align="center">
  <img width="460" height="215" src="./Extras/Bindings/icon.png">
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
* Shoulder weapon holsters (for switching weapons)
* Swapping weapons between hands
* Detached floating UI layer
* Floating crosshair
* Joystick steered vehicles
* Desktop mirror (recommended to use SteamVR's desktop display instead)

## Known issues
* Game sometimes isn't the focused window on launch and inputs/sounds may break when the game is unfocused
* First stage of the tutorial ("look around") doesn't detect headset movement, wiggle the mouse (physically or via steamvr's desktop view) and you should get past it
* Camera behaves weirdly briefly when entering/exiting vehicles
* Reloading a checkpoint made while in a vehicle can mess with the camera position, get out and in again to fix it
* Melee and interact use head aiming rather than controller aiming
* Crosshair only lights up red when looking at an enemy, not when pointing a gun at one
* On screen button prompts display keyboard bindings rather than VR bindings
* If the game is installed in program files the mod config file (and log file) won't generate unless the game is run as administrator
* Occasional stutters can cause motion smoothing to kick in, which is very jarring in vehicles
* Some people report a minor distortion or warping effect when tilting their head side to side.

## Installation
0. Install _Halo: Combat Evolved for PC_ (not Custom Edition) using an original installation CD + product key.  **IMPORTANT: Install in a directory OTHER THAN Program Files (example: C:\HaloVRMod\Halo). Installing in Program Files potentailly causes numerous permissions-related issues**.
1. Install the 1.0.10 patch for PC (not Custom Edition)
2. (Optional) Install [chimera](https://github.com/SnowyMouse/chimera) (fixes a few bugs/issues such as entities moving at 30fps).  Grab the RELEASE files from [releases](https://github.com/SnowyMouse/chimera/releases).  Copy chimera.ini, strings.dll, and the fonts folder from the chimera zip after unzipping and place into your halo game folder in the same location as halo.exe.
3. If using chimera: open chimera.ini, locate the "Font Override Settings" section, and change enabled=1 to enabled=0 (failing to do this will break many UI elements in VR)
4. Download the latest version of this mod from the [releases page](../../releases)
5. Extract HaloCEVR.zip and place the files in the same directory as the halo executable (You should see a VR folder, openvr_api.dll and d3d9.dll if done correctly - if you do not see these files your antivirus may be interfering)
6. Launch the game once to generate a config.txt file in the VR directory
7. If setting LeftHanded=true in the config, consider selecting the left handed controller bindings in the game's SteamVR controller bindings page if you want the sticks to also be swapped

## Uninstalling
1. Go play the MCC version instead
2. Delete/rename the d3d9.dll you placed in the same directory as the halo executable (this contains all of the mod code and is only pretending to be d3d9 to trick halo into loading it).  For example, you could rename it to d3d9-backup.dll.

## Installing Graphical and Audio Enhancements
### dsoal 

Dsoal allows you to turn on hardware acceleration and EAX in Halo without requisite hardware to use environmental sound effects like reverb and HRTF, providing realistic, locatable 3D sound with high accuracy for virtual reality. Highly recommended, and you do not need high-fidelity headphones to enjoy this.

Note: Chimera is required for reverb on first person sounds. First person sounds have no stereo/HRTF/location effects.

1) Download and extract from https://github.com/ThreeDeeJay/dsoal/releases/download/0.9.6/DSOAL+HRTF.zip 
2) Open the DSOAL+HRTF\Win32\ folder.
3) Copy & paste all files from Win32\ (alsoft.ini, dsoal-aldrv.dll, dsound.dll) into your Halo\ install folder where halo.exe is located.
5) Edit alsoft.ini and copy & paste the settings shown below:
6) ```hrtf-mode = full``` may or may not be CPU intensive, remove if you experience new performance issues.
7) Launch Halo and go to Settings >> Audio Setup. Make sure Hardware Acceleration is set to Yes and Environmental Sound to EAX.

```
[general]
channels=stereo
frequency=48000
stereo-mode=headphones
cf_level=0
sources=512
sample-type=float32
hrtf=true
period_size=960
hrtf-mode = full

[reverb]
boost=-6
```
### Chimera

Chimera increases animation framerate, corrects fog, enables anisotropic filtering, enables reverb for first person sounds (requires dsoal), and boosts polygon count, object limit, and draw distance. But is highly recommended for bug fixes and QOL changes alone.

See installation instructions above. If you installed chimera, there is an enhancement not enabled by default. You can execute a console command ```chimera_model_detail 8``` to reduce pop-in and low detail models (lods). There are several ways to do this. 

* You can bring up the in-game console with tilde ~ key, but it helps to disable the VR mod by temporarily by renaming d3d9.dll to something like d3d9aa.dll so you can see the console. 

* Or, executed console commands are saved and read from a text file. You can create this file yourself:
1) Navigate to (Your User Folder)\Documents\My Games\Halo\chimera\ and create a text file named preferences.txt
2) Edit your preferences.txt and add ```chimera_model_detail 8```

* Or, If you want this command and other preferences to be portable and travel with your Halo installation:

1) Create a new text file named chimera_preferences.txt in the Halo\ folder alongside chimera.ini
2) Edit Halo\chimera.ini
3) Adding ```chimera_model_detail 8``` into chimera.ini will not work.
4) chimera.ini disables lines with a semi-colon ```;```
5) Find ```exec=``` and remove any semi-colon on that line, such as ```;exec=```.
6) Set the value to ```exec=./chimera_preferences.txt```
7) Add ```chimera_model_detail 8``` into chimera_preferences.txt

### Halo Refined

A Community project that restores many graphical effects present in the original Xbox version by replacing Halo's map files. Fixes issues such as invisible bumpmapping, missing specular, broken transparency effects, etc. Also provides some high res asset replacements, most notably an HD HUD. 

Halo Refined is poorly tested with HaloCEVR but seems to work, recommended for experimental use. Please keep backups of your original .map files before installing. If you are having any issues with HaloCEVR while using Refined's maps, restore the original files and retest before reporting your issue.

Recent versions require chimera. Refined is designed with dgVoodoo2 and CEnshine in mind to fix even more effects, but these are tools and modifications can't be used with HaloCEVR. 

1) Download and extract the latest distribution for Retail, not Custom Edition
2) Backup all files in Halo\maps\ by copying them to a different location
3) Replace files in Halo\maps\
4) (Optional) Read Halo\maps\info.txt for project information

Download, updated as of the time of writing:
https://www.proxeninc.net/Halo/Refined/

Mirror, outdated as of the time of writing:
http://vaporeon.io/hosted/halo/refined/


## FAQ
### Why the gearbox port and not MCC?
Short answer: skill issue.

Long answer: This mod my first time attempting to reverse engineer and mod a close sourced game engine requiring learning how to decompile, analyse and patch x86 assembly.
As you may guess, this is hard. By focusing on an older title I have a simpler project to work with as I do not have an additional 2 decades worth of updates and advances in technology to worry about.

(For example, no dual classic/aniversary graphics, no MCC launcher application separate to the halo1 game code, DirectX9 rather than DirectX11, x64, simpler code layout with few virtual functions to follow (which I struggled to analyse in Ghidra).
### Where do I even get a CD copy in \<current year\>? Can I use a cracked version?
No.

This mod is intended to only be used with legitimate copies of Halo: Combat Evolved with the official 1.10 patch. Fortunately the product keys aren't single use, so if you can find a second hand copy (or a friend/person on the _internet_ willing to lend the copy they happened to _archive_ 20 years ago) you can use that without worrying about the key being invalid. 
### Does multiplayer work?
This mod was designed for singleplayer, it is untested in multiplayer and as such some features, such as weapon aiming, may not function or only work for the host.

Also it does not work in Co-op because this version of the game does not have that mode.
### Does this work with Custom Edition?
Not currently.
### Does this work with MCC Edition?
No.
### Does this work with SPV3?
No, SPV3 depends on Halo Custom Edition which is presently incompatible.
### Does this work with other mods?
Maybe, if they only need the PC edition and do not change the underlying structure of the game in a way that interferes with the VR mod. The focus of this mod is making sure the base game works in VR.
### Help! The game launches in VR but I cannot interact with the main menu! My buttons do not work!
Make sure the game window on your desktop is in focus (switch back to desktop view and click on the window).  If that does not work make sure you're actually using a SteamVR runtime.
### Help! I've tried EVERYTHING to the letter and something is still wrong.
A very few users have reported reinstalling SteamVR helps.  Not sure why.  Triple check your anti-virus is not blocking anything.  If you still have trouble join the flat2VR discord, find the hvr-join channel, and post about your issue, including as many details as possible about your system and what you have done to install.  The community may be able to help you.
### Help! I get to the tutorial and am stuck when I am supposed to look around.
Either play through this initial part of the game in flat screen, or walk over to your computer and wiggle the mouse and it should get you past this point.
### Which config.txt file do I use to make changes to VR settings?  What if I am making changes but don't see changes in game?
The one found in the "VR" folder after install and after you run the game once in VR. If you are not seeing changes you make take effect in game, then chances are you disregarded the instruction not to install in Program Files.  You can try running in administrator mode to make the edits but safer to just install Halo and the mod in a new location.
### How do I turn on the flashlight?
By default tapping your head with an empty hand will toggle the flashlight, you can adjust the radius this triggers at and which hand to use in config.txt. Alternatively there is an optional binding you can configure in SteamVR's controller bindings page to toggle it directly.
### How do I melee/can melee be bound to a button on my controller?
By default swinging either controller vertically with enough speed will trigger a melee attack wherever you are looking. If preferred there is a controller binding that is unset by default you can configure instead.

### How do I switch weapons?
By default, you can switch weapons using the shoulder weapon holsters. To do this, hover your dominant hand over a shoulder and press the Switch Weapon controller binding (can be configured in SteamVR's bindings).
If you prefer, you can adjust the settings for the weapon holsters or disable them in config.txt to switch weapons using only the controller binding.

### How can I swap my weapon between my hands?
You can swap your weapon into the opposite hand by bringing your hands close together and using the `SwapWeaponHand` binding (This can be configured in SteamVR's bindings). By default, this is bound to the grip button on your non-dominant hand and changes hands depending on whether the weapon is currently in your left or right hand. Whilst holding a weapon in your left hand the `left handed` SteamVR Action Set Bindings will be used. The distance required between hands can be configured in the `config.txt` file or the feature can be disabled.

### How do I activate smooth turning?
Go to the VR config.txt file and change SnapTurn = false.  You can also adjust turning speed with SmoothTurnAmount.
### How do I activate hand-directed movement?
By default movement is in the direction the player's head is facing. If you would like to use hand-directed movement, open the VR config.txt file, and find these lines:
```//[Int] Movement is relative to hand orientation, rather than head, 0 = off, 1 = left, 2 = right (Default Value: 0)
HandRelativeMovement = 0

//[Float] Hand direction rotational offset in degrees used for hand-relative movement (Default Value: -20)
HandRelativeOffsetRotation = -20
```
Change HandRelativeMovement to 1 for movement to follow your left hand direction or 2 for movement to follow your right hand direction.
### How do I turn the crosshair off?
Open the VR config.txt file and change "ShowCrosshair" to false - "ShowCrosshair=false"
### Things feel constantly jittery in vehicles
Halo internally runs a lower tick rate (I believe it is 30fps) and only interpolates the player camera, this makes things feel jittery when driving vehicles. To fix this install [chimera](https://github.com/SnowyMouse/chimera), as they have fixed this issue along with many others. If you still experience intermittent stuttering on vehicles it may be due to the motion smoothing kicking in and locking the frame rate to 45 for a few seconds, you may experience smoother results by disabling it. 
### Does this mod support OpenXR?
No, this mod is exclusively for SteamVR since it is the only runtime to support 32bit applications to my knowledge.  This means you need to choose the SteamVR runtime in your VR software if available. For example, with Oculus Link you need to make sure you have SteamVR running, or with WMR you need to use the SteamVR runtime.
### Help the camera is too high/too low (or crouch seems to be stuck on)
Stand up straight (or sit up if playing seated) and hold down the menu button for 3 seconds to recentre the camera to your current physical position.
### How do I crouch?
By default you crouch in real life. If you do not prefer that, change the option in the config.txt file in the VR folder, specifically set CrouchHeight = -1.0 and bind a button to crouch in the SteamVR Input menu.
### I wish (insert action) was bound to a different key
Use SteamVR input settings to change your bindings to whatever you want. You can also search for other peoples' bindings for your controller in the bindings menu.  You can learn how to use SteamVR input by watching this video by HoriZon, who also has bindings available for Quest controllers: https://www.youtube.com/watch?v=rdlCu7IjbGI.  It's useful in a bunch of games!
### I know C++ coding or will learn to help develop the mod.  How do I help?
Thank you! See compiling source directions below and submit a Pull Request on Github explaining the changes you have made and impact on other files.  Be sure to thoroughly test any changes before submitting them. After others and I test the changes, they may be incorporated into the mod release.

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
