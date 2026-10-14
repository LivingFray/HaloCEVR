HALO VR - RESTART R1.31b
14 October 2026

Source update on the user-confirmed R1.31a build.
Windows/MSVC compilation and in-game headset testing have NOT been done here.

BUILD / INSTALL
1. Extract the complete HaloCEVR-Stylish-Restart-R1.31b-Source.zip into a NEW
   folder. Keep your working R1.30 source/DLL backup. ThirdParty is included.
2. Open PowerShell inside HaloCEVR-Stylish-Restart-R1.31b-Source and run:
   powershell -NoProfile -ExecutionPolicy Bypass -File .\Build-Restart-R1.ps1
3. After a successful build, close Halo. Copy the CONTENTS of
   artifacts\Restart-R1.31b\weapon-test into the folder containing halo.exe.
   Replace d3d9.dll AND all five JSON files inside VR\OpenVR.
   Keep VR\config.txt. This updates your existing HaloCEVR 1.4.0 installation.
4. Vehicle sticks now fall back to the original Move/Look actions on the correct
   physical controller if the new bindings are unavailable. Existing compatible
   personal bindings work; the included defaults are still provided.
5. Confirm VR\inject.log begins with [Stylish Restart R1.31b]. About displays
   Restart R1.31b; Guide contains ten pages including TANK / PELICAN.
The script stages files only; it never installs or launches Halo automatically.
For compilation errors, keep the full output. For a runtime issue, keep
VR\inject.log and VR\StylishCrash.log plus native\d3d9.pdb from the matching build.

CHANGES IN R1.31b
- Flashlight touch zone follows the saved Left Handed setting: left temple in
  right-handed mode, right temple in left-handed mode. Either hand can activate
  it, including while holding a weapon. Switching the firing hand alone does
  not move the zone. Existing radius, haptics and grenade exclusion remain.
- Guide and settings descriptions updated. R1.31a steering/reload fixes retained.

RETAINED R1.31a CHANGES
- Fix the R1.31 vehicle-stick regression shown in the supplied log. New analog
  handles registered but failed during reads with OpenVR InvalidHandle (3).
  Read failures/inactive actions now fall back to original Move/Look bindings,
  restricted to the correct physical left/right controller in either preset.
  An active centered input remains centered; aliases are never added together.
- Physical stick diagnostics report route changes only, instead of repeating
  errors every frame. The log also reports the selected action-manifest path.
- Remove optional free charging on the AR and sniper. Their handles engage when
  the chamber needs charging after running empty. Successful charging releases
  the latch; required empty reloads still work in either hand. Magnum free slide
  and required shotgun pumps remain available. Guide/settings text updated.

RETAINED R1.31 CHANGES
- On foot: legs only, OFF by default. Enable Basic > Show Legs if wanted.
  Torso, head and arms are removed from the optional body draw; walking legs
  remain animated. Tracked hands still work normally. The new OnFootLegs config
  key deliberately does not inherit the old torso option's true value.
- Ghost: LEFT stick moves forward/back and strafes left/right. RIGHT stick aims
  up/down/left/right and turns. LT OR RT fires; holding both does not double fire.
  A/X no longer fire the Ghost. Short Y still opens its menu; hold Y 5s recenters.
- Ghost Aim Sensitivity is independent of Turret Sensitivity. Default 1.0,
  compared with the old shared 0.55; adjustable range 0.1 to 2.5.
- Scorpion driver: RT main cannon; LT machine gun (native secondary weapon).
  A no longer fires the machine gun. Its existing camera behavior is retained.
- VR Settings titles omit Stylish and use readable spacing. Legacy grip toggle,
  hand-swap distance, native shoulder-switch zones and old head-touch flashlight
  controls are hidden when their replacements are enabled. Inapplicable 3DOF
  tuning is hidden in 6DOF. Saved keys are retained for compatibility/fallback.
- Shotgun: no optional free pumping while ready. Still pump after a shot and
  after loading from empty. Only the Magnum retains optional free cycling.
- Pelican: discover/refresh seats during rendering as well as native input.
  This handles scripts that suspend gameplay input during an intro. Right stick
  looks around with independent view yaw/pitch; scripts still control flight.
  The shared path applies to Pelican seats across missions, including Silent
  Cartographer, 343 Guilty Spark, Truth and Reconciliation and Assault on the
  Control Room. These individual scenes still require in-game confirmation.
- Exiting vehicles/turrets: suspend headset-to-native yaw/pitch corrections for
  one elapsed second after detachment, then synchronize native/Chimera view
  references before resuming. Headset rendering and physical head tracking stay
  live. Position handoff uses frame-rate-independent exponential smoothing.
  Inspired by AJBats' draft explanation supplied by Ryan; no external code copied.
- Guide updated for all changed controls, including a new TANK / PELICAN page.

QUICK CONTROLS
On foot: hold grip to hold; release in a holster to stow, elsewhere to drop.
Either hand can hold a weapon. Free-hand grip at its support point reduces recoil.
Right-handed: right hip + right shoulder weapons; left hip Magazine Zone.
Left-handed mode mirrors weapon/ammo holsters. Plasma stays left chest, frag right.
B interacts. Reload with B (right-handed) or a short Y press (left-handed).
Hold physical left Y five seconds to recenter. X held + RT opens the menu,
except the Ghost where short Y opens it. Right A jumps. Crouch physically.
Flashlight: fresh grip with either hand at the left temple in right-handed mode,
or the right temple in left-handed mode. Works even while holding a gun.
World Scale 0.80 remains the recommended Quest 3 starting point; recenter on foot.

VEHICLES
Warthog driver: left stick steering, RT forward, LT reverse.
Warthog turret / Shade: right stick aims, RT fires; Turret Sensitivity tunes both.
Ghost: left stick movement/strafe; right stick aim/turn; either trigger fires.
Scorpion: RT main cannon, LT machine gun; existing movement/aim controls retained.
Pelican: headset and right-stick look; scripted flight.
Fixed vehicle stick actions remain on the same physical hands with either preset.

MANUAL RELOADS
Magnum/AR/sniper: reload button ejects; free-hand grip at Magazine Zone takes a
magazine; bring it to the well. If it ran empty, pull the slide/charging handle
back, then return or release. Only the Magnum can cycle freely while ready;
AR/sniper handles engage only when charging is required after running empty.
Shotgun: take shell from Magazine Zone and insert at top port. Pump back/forward
after each shot and after loading from empty. Shell Ratio: 1..4, reserve limited.
Launcher: reload button opens/closes casing. Grip/remove/discard spent rockets;
take fresh rockets from Magazine Zone, insert one or two, then close with button.
Needler: reload button only; no auto-reload. Plasma batteries are unchanged.

VALIDATION / LIMITS
Portable fixtures exercise the production camera/input/reload/render adapters
with synthetic native memory, VR and Windows API doubles. No real game/headset
or Windows DLL compilation is available here. Camera comfort, actual mesh
appearance and each mission intro need headset testing.
Campaign retail 1.0.10.0621 only. Custom Edition/multiplayer retain original paths.
Independent dual-weapon rendering/firing and manual steering remain pending.
R1.29's native-visibility rollback is retained; no new PVS override is introduced.
The earlier missing-cliff visibility issue is not claimed solved.
No game executable, maps, models, sounds or other Halo assets are in this ZIP.
Upstream HaloCEVR credit: LivingFray and contributors (README/LICENSE retained).
