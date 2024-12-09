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
* Shoulder weapon holsters (for switching weapons)
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
* Some people report a minor distortion or warping effect when tilting their head side to side.

## Installation
0. Install _Halo: Combat Evolved for PC_ (not Custom Edition) using an original installation CD + product key.  IMPORTANT: Install in a directory OTHER THAN Program Files (example: C:\HaloVRMod\Halo). Installing in Program Files potentailly causes numerous permissions-related issues.
1. Install the 1.10 patch for PC (not Custom Edition)
2. (Optional) Install [chimera](https://github.com/SnowyMouse/chimera) (fixes a few bugs/issues such as entities moving at 30fps).  Grab the RELEASE files from [releases](https://github.com/SnowyMouse/chimera/releases/download/1.0.0r1021/chimera-1.0.0r1021.10144368.7z).  Copy chimera.ini, strings.dll, and the fonts folder from the chimera zip after unzipping and place into your halo game folder in the same location as halo.exe.
3. If using chimera: open chimera.ini, locate the "Font Override Settings" section, and change enabled=1 to enabled=0 (failing to do this will break many UI elements in VR)
4. Download the latest version of this mod from the [releases page](../../releases)
5. Extract HaloCEVR.zip and place the files in the same directory as the halo executable (You should see a VR folder, openvr_api.dll and d3d9.dll if done correctly - if you do not see these files your antivirus may be interfering)
6. Launch the game once to generate a config.txt file in the VR directory
7. If setting LeftHanded=true in the config, consider selecting the left handed controller bindings in the game's SteamVR controller bindings page

## Uninstalling
1. Go play the MCC version instead
2. Delete/rename the d3d9.dll you placed in the same directory as the halo executable (this contains all of the mod code and is only pretending to be d3d9 to trick halo into loading it).  For example, you could rename it to d3d9-backup.dll.

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
### Does this work with Custom Edition?
No.
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
By default tapping your head with your left hand will toggle the flashlight, you can adjust the radius this triggers at and which hand to use in config.txt. Alternatively there is an optional binding you can configure in SteamVR's controller bindings page to toggle it directly.
### How do I melee/can melee be bound to a button on my controller?
By default swinging either controller vertically with enough speed will trigger a melee attack wherever you are looking. If preferred there is a controller binding that is unset by default you can configure instead.

### How do I switch weapons?
By default, you can switch weapons using the shoulder weapon holsters. To do this, hover your dominant hand over a shoulder and press the Switch Weapon controller binding (can be configured in SteamVR's bindings).
If you prefer, you can adjust the settings for the weapon holsters or disable them in config.txt to switch weapons using only the controller binding.

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
### Textures seem to pop in too much.  Any fix?
Try using chimera (highly recommended anyway) and create a file called chimera_preferences.txt in your Halo directory.  In that text file, insert the following lines:
```
chimera_model_detail 8
chimera_lod 1
```
Then in your chimera.ini file, find the line that starts with ;exec= and change it to:
```
exec=path\to\your\chimera_preferences.txt
```
Example:
```
exec=C:\HaloVRMod\Halo\chimera_preferences.txt
```
Note that deleting the semicolon is what makes this line active.  It's telling chimera to load the text commands you wrote that control the level of detail. If others share chimera commands in the future you can place them here.
You can test that the commands are being executed by changing the name of the VR mod dll file, d3d9.dll, to something else temporarily and booting the game up in flat screen.  The commands will flash on the screen if this step worked.
#### How can I make sounds even more immersive?
Check out DSOAL for 3D sound ingame: https://github.com/ThreeDeeJay/dsoal/releases/tag/0.9.6, specific download zip link: https://github.com/ThreeDeeJay/dsoal/releases/download/0.9.6/DSOAL+HRTF.zip 
-Extract to a location on your computer. Inside the zip, find the Win32 folder, open it, and copy and paste all Win32 files into your Halo folder
-One of the files will be called alsoft.ini which is a configuration file for DSOAL.

Path: Halo>alsoft.ini

Copy and Paste into alsoft.ini:
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

You should now be able to build successfully, this will generate a d3d9.dll file, using the "makerelease" bat file we can create a zip containing the d3d9.dll and the VR folder. You will need to add openvr_api.dll yourself, either by adding to the release folder so the bat file can pickup copy and zip it, or by adding it directly into your halo CE installation directory.
