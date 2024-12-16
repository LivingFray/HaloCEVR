<p align="center">
  <img width="460" height="215" src="./Bindings/icon.png">
</p>

# Halo: Combat Evolved VR
A full VR conversion mod for the original 2003 PC edition of _Halo: Combat Evolved_.

[Mod Download here](../../releases)

**This mod is not compatible with _The Master Chief Collection_.**

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
* Detached floating UI layer
* Floating crosshair
* Joystick steered vehicles
* Desktop mirror (recommended to use SteamVR's desktop display instead)

## Known issues
* Game sometimes isn't the focused window on launch and inputs/sounds may break when the game is unfocused
* First stage of the tutorial ("look around") doesn't detect headset movement (wiggle the mouse and you should get past it)
* Camera behaves weirdly briefly when entering/exiting vehicles
* Reloading a checkpoint made while in a vehicle can mess with the camera position (get out and in again to fix it)
* Melee and interact use head aiming rather than controller aiming
* Crosshair only lights up red when looking at an enemy, not when pointing a gun at one
* On screen button prompts display keyboard bindings rather than VR bindings
* If Halo is installed in Program Files, the mod's config file (and log file) won't generate unless Halo is run as administrator
* Occasional stutters can cause motion smoothing to kick in, which is very jarring in vehicles
* Some people report a minor distortion or warping effect when tilting their head side to side.

## Installation
0. Install _Halo: Combat Evolved for PC_ (not Custom Edition) using your original installation media and product key.  
  > [!IMPORTANT] 
  > Installing in Program Files causes numerous permissions-related issues. You should install to a directory outside Program Files. <br> (i.e. `C:\HaloVRMod\Halo`).

1. Install the 1.10 patch for Halo PC (not Custom Edition)
2. Install [Chimera](https://github.com/SnowyMouse/chimera), which fixes issues like entities moving at 30 FPS. (Optional, but recommended)
  > Grab the RELEASE files from releases [(direct link)](https://github.com/SnowyMouse/chimera/releases/download/1.0.0r1021/chimera-1.0.0r1021.10144368.7z).
  > Copy chimera.ini, strings.dll, and the fonts folder from the Chimera zip after unzipping, and place into your Halo game folder in the same location as halo.exe.
3. If using Chimera, disable the font override. (Failing to do this will break many UI elements in VR.)
  > Open chimera.ini, and locate the "Font override settings" section. Where it says `; Enable overriding Chimera's fonts` change `enabled=1` to `enabled=0`.
4. Download the latest version of this mod from the [releases page](../../releases).
  > Make sure you download HaloCEVR.zip, not the source code.
5. Extract HaloCEVR.zip, and place the files in the same directory as the halo.exe executable.
  > You should see a VR folder, openvr_api.dll, and d3d9.dll if everything is correct. Copy these files. If you do not see these files, your antivirus may be interfering. 
6. Launch the game once to generate a config.txt file in the VR directory.
7. If setting LeftHanded=true in the config, consider selecting the left handed controller bindings in the game's SteamVR controller bindings page for a better experience. 

## Uninstalling
0. Just play the MCC version instead!
1. Delete or rename the d3d9.dll next to halo.exe you placed in the same directory as the Halo executable. For example, you could rename it to d3d9-backup.dll. (This contains all of the mod code, and is only pretending to be D3D9 to trick Halo into loading it.)

## FAQ
### Why the Gearbox port and not MCC?
Short answer: skill issue.

Long answer: This mod's my first time attempting to reverse engineer and mod a closed source game engine requiring learning how to decompile, analyse and patch x86 assembly.
As you may guess, this is hard. By focusing on an older title I have a simpler project to work with as I do not have an additional 2 decades worth of updates and advances in technology to worry about.

(For example, no dual classic/aniversary graphics, no MCC launcher application separate to the halo1 game code, DirectX9 rather than DirectX11, x64, simpler code layout with few virtual functions to follow (which I struggled to analyse in Ghidra).
### Where do I even get a CD copy in \<current year\>? Can I use a cracked version?
No, you can't use a cracked version.

This mod is intended to only be used with legitimate copies of Halo: Combat Evolved with the official 1.10 patch. Fortunately, the product keys aren't single use. If you can find a second hand copy, or a friend willing to lend the copy they happened to archive 20 years ago, you can use that without worrying about the key being invalid. 
### Does multiplayer work?
This mod was designed for singleplayer. It is untested in multiplayer and as such some features, such as weapon aiming, may not function or only work for the host.

Also, the original PC version of Halo doesn't have a co-op mode, so there is no co-op support either. 
### Does this work with Custom Edition?
No.
### Does this work with the MCC Edition?
No. See above.
### Does this work with SPV3?
No. SPV3 depends on Halo Custom Edition, which is presently incompatible.
### Does this work with other mods?
Maybe. The focus of this mod is making sure the base game works in VR. If the mod only needs the PC edition, and do not change the underlying structure of the game in a way that interferes with the VR mod, it might work. 
### Help! The game launches in VR, but I cannot interact with the main menu! My buttons do not work!
Make sure the game window on your desktop is in focus (switch back to desktop view and click on the window).  If that does not work, make sure you're actually using a SteamVR runtime.
### Help! I've followed EVERYTHING to the letter and something is still wrong.
A very few users have reported reinstalling SteamVR helps.  Not sure why.  Triple check your anti-virus is not blocking anything.  If you still have trouble join the Flat2VR discord, find the hvr-join channel, and post about your issue, including as many details as possible about your system and what you have done to install.  The community may be able to help you.
### Help! I get to the tutorial and I'm stuck when I am supposed to look around.
Walk over to your computer and wiggle the mouse, or play through this initial part of the game in flat screen. 
### Which config.txt file do I use to make changes to VR settings? 
The one found in the "VR" folder, which is created after install and after you've run the game once in VR. 
### I'm changing the config.txt, but the changes don't take effect in-game?
If you aren't seeing changes you make take effect in game, chances are you disregarded the instruction not to install in Program Files. Try running Halo in Administrator mode to make the edits, but it's safer to just install Halo and the mod in a new location.
### How do I turn on the flashlight?
By default, tapping your head with your left hand will toggle the flashlight. You can adjust the radius this triggers at, and which hand to use, in config.txt. Alternatively, there is an optional binding you can configure in SteamVR's controller bindings page to toggle the flashlight directly.
### How do I melee? / Can melee be bound to a button on my controller?
By default, swinging either controller vertically with enough speed will trigger a melee attack where you are looking. If you prefer a button, there is a controller binding that is unset by default which you can configure in the SteamVR bindings.
### How do I switch weapons with the holsters enabled?
Hover your dominant hand over a shoulder and press the Switch Weapon controller binding. (can be configured in SteamVR's bindings)
If you prefer, you can adjust the settings for the weapon holsters in config.txt.
### How do I activate smooth turning?
Open the VR config.txt file, and change "SnapTurn" to false - `SnapTurn = false`.  You can also adjust the turning speed with SmoothTurnAmount.
### How do I activate hand-directed movement?
By default, movement is in the direction the player's head is facing. If you would like to use hand-directed movement, open the VR config.txt file, and find these lines:
```
//[Int] Movement is relative to hand orientation, rather than head, 0 = off, 1 = left, 2 = right (Default Value: 0)
HandRelativeMovement = 0

//[Float] Hand direction rotational offset (in degrees). Used for hand-relative movement (Default Value: -20)
HandRelativeOffsetRotation = -20
```
Change HandRelativeMovement to 1 for movement to follow your left hand direction, or 2 for movement to follow your right hand direction.
### How do I turn the crosshair off?
Open the VR config.txt file and change "ShowCrosshair" to false - `ShowCrosshair = false`
### Things feel constantly jittery in vehicles...
Halo internally runs a lower tick rate (i.e. 30 FPS) and only visually interpolates the first-person player camera. This makes things feel jittery when driving vehicles, as they only move at the lower tick rate. 
To fix this, install [Chimera](https://github.com/SnowyMouse/chimera), as they have fixed this issue along with many others. If you still experience intermittent stuttering on vehicles, it may be due to the motion smoothing kicking in and locking the frame rate to 45 for a few seconds. You may experience smoother results by disabling it.
### Does this mod support OpenXR?
No. This mod is exclusively for SteamVR, since it is the only runtime to support 32-bit applications to my knowledge.  This means you need to choose the SteamVR runtime in your VR software, if available. For example, with Oculus Link, you need to make sure you have SteamVR running, or with WMR you need to use the SteamVR runtime.
### Help, the camera is too high/too low (or crouch seems to be stuck on)!
Stand up straight (or sit up if playing seated) and hold down the menu button for 3 seconds to recentre the camera to your current physical position.
### How do I crouch?
By default, physically crouching will allow you to crouch. You can adjust this behaviour in config.txt. 
Alternatively, press Down on the right controller (the one you use to turn) - your view won't change, but you will be able to fit through small spaces.
### I wish (insert action) was bound to a different key...
Use SteamVR input settings to change your bindings to whatever you want! You can also search for other peoples' bindings for your controller in the bindings menu.  You can learn how to use SteamVR input by watching this video by HoriZon, who also has custom bindings available for Quest controllers: https://www.youtube.com/watch?v=rdlCu7IjbGI.  It's useful in a bunch of games!
### Things seem to pop in and out a lot...
Chimera lowers the LOD level to fix some visual bugs in the original game with high resolutions. Fortunately, these bugs aren't a huge problem in VR, so we can improve the LOD by creating a Chimera preferences file.  

Create a file called chimera_preferences.txt in your Halo directory.  In that text file, insert the following lines:
```
chimera_model_detail 8
```
Then in your chimera.ini file, find the line that starts with `;exec=` and change it to:
```
exec=path\to\your\chimera_preferences.txt
```
Example:
```
exec=C:\HaloVRMod\Halo\chimera_preferences.txt
```
Note that deleting the semicolon is what makes this line take effect. It's telling Chimera to load the text commands you wrote. If other useful Chimera commands are shared by the community in future, you can place them in that file too.
You can check that the commands are being executed by temporarily removing the VR mod (i.e. by changing the name of the VR mod file, d3d9.dll, to something else) and booting the game up in flat screen mode. The text commands will flash on the screen if this step worked.
### Can I make the game sound even more immersive?
Using [DSOAL](https://github.com/ThreeDeeJay/dsoal/releases/tag/0.9.6), you can have full directional 3D sound in-game.
1. Download it from here: [DSOAL+HRTF.zip](https://github.com/ThreeDeeJay/dsoal/releases/download/0.9.6/DSOAL+HRTF.zip)
2. Extract to a location on your computer or open the zip file. Inside the zip, find the Win32 folder. Open it, and you should see `dsound.dll` and some other files. Copy and paste those files into your Halo folder next to the halo.exe. 
3. One of the files will be called `alsoft.ini`, which is a configuration file for DSOAL. For best results, use these community-provided settings for DSOAL. Copy and paste the following into alsoft.ini:
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

[reverb]
boost=-6
```
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

You should now be able to build successfully. This will generate a d3d9.dll file, and using the "makerelease" bat file we can create a zip containing the d3d9.dll and the VR folder. You will need to add openvr_api.dll yourself, either by adding to the release folder so the bat file can pickup a copy and zip it, or by adding it directly into your Halo CE installation directory.
