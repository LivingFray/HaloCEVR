# Stylish Halo VR — restart specification

Baseline: LivingFray/HaloCEVR tag `1.4.0`, commit
`ada2d3ba2965bbb9c6d6d653456b0ba0a897cf63`. The earlier Stage 1 recoil
experiment is preserved separately; it is not this branch's baseline.

## Current checkpoint: R1.31b

Ryan confirmed R1.31a in-game before requesting this final adjustment.
The flashlight zone now mirrors with the saved `LeftHanded` layout: left temple
for right-handed mode, right temple for left-handed mode. Either physical hand
still works. The current firing hand does not change the zone. Radius, haptic
feedback, input validity and grenade-hand exclusion are retained.

The production gesture fixture covers both layouts, either controller and
rotated/translated headset poses; the Guide's updated text is checked against
the bundled font. No other gameplay code changes in this release.
Build output: `artifacts/Restart-R1.31b/weapon-test`.

## Previous checkpoint: R1.31a

The R1.31 log shows 71,446 analog input InvalidHandle errors. PhysicalMove and
PhysicalLook registered successfully, but their reads failed; button actions
were already using the original fixed-hand fallback. This hotfix adds the
corresponding physical-controller-restricted fallback for analog sticks, using
both original Move/Look action sets. New action reads are validated at use time,
not just registration. A centered active route wins; opposite-hand input is
never substituted and aliases are never summed. Route/error diagnostics log
changes only, and startup logs the chosen action-manifest path and result.

AR/sniper handles now engage only when charging is required after running empty.
A completed charging stroke ends the latch. Their loaded support grips are not
stolen by touching the handle. Magnum free slide remains; required shotgun
pumps remain. Both firing hands and tactical/empty reloads are tested.

R1.31 camera recovery, legs-only default-off, Ghost/Scorpion mappings and settings
cleanup are retained. Build output: `artifacts/Restart-R1.31a/weapon-test`.
See `research/steering-input-R1.31a.txt` for the evidence and validation limits.

## Previous checkpoint: R1.31

Ryan confirmed the R1.30 changes working in-game. This update keeps that baseline.

- `OnFootLegs` / Show Legs replaces the torso option and defaults OFF even with
  an R1.30 config. Only thigh/calf/foot descendants render; torso, pelvis, arms
  and head nodes collapse in a draw-local copy. Animated legs remain optional.
- Ghost uses fixed physical left-stick movement/strafe, right-stick aim/turn,
  and either trigger to fire. `GhostAimSensitivity` defaults to 1.0 independently
  of turret sensitivity (still 0.55). Short Y still opens the Ghost menu.
- Scorpion driver controls map physical RT to Fire and LT to Grenade (its native
  secondary machine gun, previously bound to A). Passenger and other seats are
  excluded; the existing Scorpion camera path is retained.
- Settings labels omit the Stylish prefix and are spaced for readability.
  Superseded native grip/swap/holster and flashlight settings are hidden when
  their replacement mechanics are enabled. Saved keys remain compatible.
- Ready shotguns no longer start optional pumps. Required pumps after a shot or
  an empty reload remain, as do free Magnum/AR/sniper actions and dry-fire sounds.
- Seat discovery also runs during rendering, so Pelican intros do not depend on
  scripted gameplay input being enabled. Right-stick view yaw/pitch is applied
  locally once per rendered frame with fresh VR action sampling. The mission
  script retains flight control. This is a common seat path, not a map-name hack.
- Exit corrections are suspended for one elapsed second, then the native and
  Chimera reference directions are synchronized to the HMD before resuming.
  This follows AJBats' draft approach quoted by Ryan; no external code was copied.
  Current native parent state is checked at input and render boundaries. Live
  head tracking stays active. Position uses time-based EMA until convergence.
- Guide updated to ten pages including Ghost, Scorpion, Pelican and legs controls.

Build: `Build-Restart-R1.ps1`; deploy `artifacts/Restart-R1.31/weapon-test` including
ALL FIVE binding JSONs. New fixed physical vehicle stick actions are included in
all four controller presets. Old custom SteamVR bindings may need reselection.
Source/portable checks only here: Windows compilation, headset comfort and the
Silent Cartographer / 343 Guilty Spark / Truth and Reconciliation intros still
need game testing. Existing visibility rollback is retained; no new PVS hook.

## Previous checkpoint: R1.30

Ghost gets independent vertical/horizontal aim from either stick, RT/LT throttle,
and physical A/X fire. Warthog gunner follows the rendered head like the Shade.
B stays on the physical right controller for interaction regardless of weapon
handedness. Physical left Y held for five seconds recenters; its short release
remains left-handed reload. Ghost short Y opens the menu because X+RT now fires
and accelerates there; the existing X+RT menu chord remains elsewhere.

The flashlight accepts a fresh left-temple squeeze without requiring the other
controller to be visible, and holds the game input for 120 ms. Belt labels and
EMPTY text use the same depth-tested projection as the magazine label. Optional
`StylishOnFootBody` draws the native torso/animated legs with arms/head hidden;
the render copy steadies the torso and retains the live leg motion.

Vehicle exit freezes the animated head portion of the seated view, then blends
position back to walking over 0.35 seconds. Pelican seats use the stable chassis
anchor; cinematic input is not fed back into yaw. Wrapping turn differences and
avoiding a second cinematic rotation address identified transition hazards.
These paths are tested with synthetic poses, not proven in a running game here.

AR/sniper actions and shotgun pumps can operate freely without ammo penalties.
A supported shotgun starts a free pump only after deliberate backward travel.
Native dry-fire attempts remain audible when empty; loaded uncharged weapons
still cannot fire. Needler keeps its native reload animation, admitted only by
an explicit B / short left-handed Y reload, with no lingering auto-reload permit.

VR Settings > Guide adds nine readable, paged controls/reload guides using the
bundled font and existing UI. No image or game asset needs a separate install.

See `research/controls-camera-reload-R1.30.txt` and `RESTART-R1-README.txt`.
Build output is `artifacts/Restart-R1.30/weapon-test`. Portable regressions pass;
Windows/MSVC compilation, actual audio/visuals and headset comfort remain to test.
The R1.29 native-visibility rollback is retained; no new PVS override is installed.

## Previous checkpoint: R1.29

Removes the R1.28 experimental visibility detour after vehicle/enemy flicker and
low FPS were reported in every supported seat. Native visibility is used; the
old saved setting is ignored and cannot reactivate the deleted hook. The older
missing-cliff issue is not claimed resolved by this rollback.

Shade uses Chief's rendered head position and bind-corrected head yaw rather
than a frozen pedestal-local anchor. Physical head lean remains direct and
pitch/roll are not forced by the turret. An additional 0.18 m of Shade-only lift
is adjustable with `StylishShadeEyeLift`.

Both grenade capture/return centers move up 0.06 m without moving their meshes;
`StylishGrenadeGripHeightOffset` allows adjustment. Existing capture radii,
reload priority, either-hand handling and throw physics are retained.

See `research/vehicle-regression-R1.29.txt` and `RESTART-R1-README.txt`.
Build output is `artifacts/Restart-R1.29/weapon-test`. Portable checks pass;
Windows/MSVC, real FPS and headset validation remain untested here.

## Previous checkpoint: R1.28

Adds frame-based headset/right-stick tutorial look history for the cryotube,
independent of inventory and on-foot camera eligibility. The native script and
its resets remain in control; physical directional input is required.

Adds Shade first-person/head masking and curved, slower shared turret aim.
Ghost gets its own additional eye lift and a lightly smoothed head-origin
reticle. The cockpit visibility pass can temporarily use the engine's bounded
PVS branch with current-eye frusta; this replaces R1.27's speculative FOV margin.
The missing-cliff diagnosis is still unconfirmed in-game.

Sniper magazine feed end rotates above the palm, compact alien-weapon support
hands move forward, AR support capture expands, chest capture is narrower and
valid manual reload grips take priority over a new grenade grab. Scope excess
magnification defaults to half strength. World-scale help recommends 0.80 from
the user's testing without overwriting saved preferences.

See `RESTART-R1-README.txt` and `research/tutorial-vehicles-R1.28.txt`. New Basic
settings: `StylishGhostEyeLift`, `StylishTurretSensitivity`,
`StylishScopeMagnification`, `StylishVehicleVisibilityFix`.
Source-only output: `artifacts/Restart-R1.28/weapon-test`. Portable regression
checks pass; Windows/MSVC and headset/GPU validation remain with the user.

## Previous checkpoint: R1.27

Restores the small native grenade holster previews, grey when empty, and removes
the R1.26 chest pucks. Plasma shifts 4 cm right in both the preview and interaction
anchor. The magazine zone is a thin 18 cm outline with its label in the center.
Its depth, and the procedural reload-piece fallback, use the native first-person
projection override instead of the world projection. The native grenade models
already share Chief's hand rendering path; empty models keep native depth tests.

Vehicle motion remains the R1.26 rendered-chassis implementation. After each
stereo eye is positioned, a regular cockpit near plane replaces inherited clip
state and the culling frustum receives a bounded visibility margin. The raster
frustum, and custom overlay matrix extraction, retain the exact headset FOV.
The Warthog driver arrow is hidden; turret/Ghost crosshairs remain available.
This is a targeted visibility correction, not a confirmed diagnosis of the
footage's missing map sections. A repeat of that route in-game is required.

See research/holster-depth-visibility-R1.27.txt for evidence, hypotheses and limits.
Portable regression checks pass; Windows/MSVC and headset/GPU validation have
not been performed. Build output is artifacts/Restart-R1.27/weapon-test.

## Previous checkpoint: R1.26

The supplied R1.25 log shows a slow grenade drop followed by the combined
player-context rejection, stock controls, then successful reinitialization.
It does not identify which condition failed or establish that the DLL unloaded.
R1.26 uses Halo's native dead flag and preserves a suspended campaign session
across failed player reads. No inventory/fire action runs while invalid. On
recovery, surviving homes remain; a replacement biped/checkpoint is reconciled.
Unsupported game modes still release control. Failures now have distinct reasons.

Camera translation and yaw use the live rendered chassis root (root bind offset
removed), with turret facing from the rendered seated body root. Chimera's
preframe interpolation changes node matrices, not object position; combining
those two sources with another camera filter can cause cockpit-relative shake.
The camera now uses the same pose as the model with no extra buffer, retaining
entry-only anchor settling and immediate HMD movement. Invalid/custom vehicle
roots retain the previous simulation fallback. Native terrain motion remains.

The magazine pad is now a closed 18 cm by 3 cm puck. Empty grenade sockets are
closed 8 cm by 2.5 cm pucks with type-colored rims. Both use opaque geometry,
world depth and explicit viewport depth range; the magazine label sits on its
top face and respects depth. Empty chest sockets no longer draw ghost grenades.
Input anchors have not moved. Existing AR/world-scale and gameplay settings stay.

Regression tests pass; Windows/MSVC compilation and GPU/headset behavior remain
unverified. See research/recovery-camera-depth-R1.26.txt and RESTART-R1-README.txt.

## Previous checkpoint: R1.25

The supplied R1.24 log confirms repeated seat-anchor recapture in the turret;
the video shows the camera inside the driver seat but shaking during movement.
Head-anchor recapture is now confined to the first three seconds of entry.
The native seat position and unwrapped chassis/turret yaw use a one-tick
(33 ms) interpolation buffer. This replaces per-render velocity advancement
followed by correction toward a stale 30 Hz native snapshot, which produced
sawtooth motion even at constant speed. Stops hold, large discontinuities snap,
and entry/seat changes/recentering reset the buffers. HMD rotation and lean
remain applied directly by OpenVR; only native vehicle movement is delayed.
This covers Warthog driver, turret and Ghost. Terrain motion is not eliminated.

The magazine marker is a solid 18 cm disk with procedural radial grooves,
concentric shading and a cyan rim. It uses the existing solid mesh pass with
world-matching depth. Its centered label also respects world depth. Native
hands/grenades can correctly occlude it; the other holster badges retain their
existing overlay behavior. No external texture or relocated grab zone is needed.

Basic setting StylishWorldScale now controls perceived world size (0.75..1.25,
default 1). Try 0.90 for roughly 10% smaller scenery/characters. This changes
the shared metres/world conversion and its inverse, including headset eye
separation, head/controller translation, model offsets and tracked throws.
Grip zones remain in real controller metres. The runtime-provided FOV stays
unchanged. Adjust on foot and recenter; the default preserves existing scale.
Flat mirror footage cannot establish whether the original stereo size is wrong.

Local campaign AR primary shots use 0.65 times the current map's three spread
angles (minimum plus error-angle bounds). The native reticle overlay uses the
same multiplier. Basic setting StylishARSpreadScale accepts 0.35 to 1, default
0.65; 1 restores native spread/reticle size. Native shot-scoped tag restoration
leaves other actors/weapons unchanged, including nested fire callbacks. Exact
retail/campaign, owner, datum, weapon path, trigger count and angle validation
fail closed. No permanent tag/map/save edit. Damage, fire rate, recoil, ammo,
projectile velocity, range limit and Magnum behavior are unchanged. Effective
mid-range damage improves through more hits, not a damage or range-cap buff.
The reticle remains Halo's native graphic; it is not a calibrated dispersion plot.

Portable regressions pass, including native 30 Hz movement sampled at 60/72/90/
120/144 Hz, stops, teleports, entry/recenter/exit paths, post-entry head animation,
marker depth state, AR scoped restoration, nested local/NPC fire, unrelated
weapon exclusion and malformed native data. Windows/MSVC compilation, GPU
occlusion, actual shot dispersion and headset comfort need user testing.
See research/stability-accuracy-R1.25.txt and RESTART-R1-README.txt.

## Previous checkpoint: R1.24

R1.23's supplied video shows the view outside the Warthog while the log
confirms both the seat path and native head mask are active. The source locked
the first head sample as soon as vehicle ownership became valid. This can
freeze the camera at the climb-in position. The local seat anchor now follows
the head during entry, settles over a minimum 0.75 seconds with 0.30 seconds
of quiet motion, then ignores small idle animation. A later displacement over
12 cm reopens settling so delayed entry cannot leave it permanently outside.
The existing 25 ms chassis filter, live HMD tracking and render restoration
remain. This applies to driver, turret and Ghost seats without native-offset
or draw-hook changes. A settled-anchor/head-error log supports headset checks.

Chest anchors rise 8 cm, and each moves 5 cm toward centre: separation 20 cm
instead of 30 cm. Mesh sizes and forward offset remain R1.23. Overlapping
capture zones now choose the nearest available grenade, respecting closer
weapon/magazine interactions and already-occupied hands.

Tracked launch velocity doubles at the SAME saved strength setting; cap rises
from 14 to 24 m/s. Default/saved 1.3 therefore takes effect without replacing
config.txt. Gentle drops still have no minimum launch speed. Direction, player
velocity inheritance, counts, fuse, sticking and native release timing remain.
Release logs record hand speed and capped requested launch speed in m/s.

Production-method portable regressions cover moving/rotating seat entry,
delayed entry, quiet locked anchors, physical head lean, exit/restoration,
nearest-grenade overlap and metre/world/tick velocity conversion. Windows/MSVC
compilation and headset behaviour remain unverified here. Existing exact retail
profile data is carried forward unchanged; no new native address is introduced.
See research/physical-interactions-R1.24.txt and RESTART-R1-README.txt.

## Previous checkpoint: R1.23

R1.22's supplied log confirms grenade acquisition/return, native release and
tracked velocity. This revision targets visual fit and vehicle usability.
Chest grenade meshes are 60% smaller and held meshes 50% smaller than R1.22.
Both chest anchors move 10 cm inward. Input owns the anchor snapshot shared by
the mesh and ring; render-time HMD sampling cannot mutate the grab zones.
Native grenade bind-root translation is removed before placement. Right native
finger chains use the opposite curl sign from left anatomy, retaining the
existing mirrored-left-skeleton reload poses.

Manual vehicle steering is removed for now. Left-stick X steers the Warthog
and Ghost, independent of hands/head/right stick. Warthog keeps RT/LT throttle;
Ghost keeps left-stick Y throttle and RT fire. Turret uses right-stick XY and
RT fire. Driver steering recenters toward vehicle heading with a neutral stick.

Camera placement captures head position in vehicle-local space ONCE per seat
entry/recenter. Chassis movement then drives the seat; Chief's animated head
does not. Extra eye height defaults to 18 cm (StylishVehicleEyeHeight, Basic UI).
A 25 ms filter smooths only native seat motion/yaw; live headset tracking is
still applied directly by OpenVR. Camera/player flags restore after the frame.

The native model submission receives a headless COPY of its own render nodes
for the local seated biped only. It replaces the old duplicate body draw. Head
scale AND basis collapse in that copy. Live bones/tags, other bipeds, attached
weapons and out-of-seat renders remain unchanged. The hook checks exact retail
model-draw entry bytes before installation; its own detour does not invalidate
subsequent grenade/holster model checks. Vehicle mode disables if this guard
cannot install, leaving the independent grenade profile intact.

Portable production-code tests cover both native hand anatomies, shared
mesh/grab anchors and scale, translated grenade binds, input mapping, head
animation isolation, camera restoration and owner-specific render copying.
The exact retail EXE/profile and the R1.22 runtime-crouch regression pass.
Windows/MSVC build, rendered body fit and headset comfort are not verified
here. See research/physical-interactions-R1.23.txt and RESTART-R1-README.txt.

## Previous checkpoint: R1.22

The supplied R1.21 log confirms correct build loading but rejects the shared
interaction profile. HaloCEVR's P_DisableCrouchCamera has already changed six
bytes inside RetailSeatCamera by this stage of startup. R1.22 accepts both
exact whole-routine hashes, separates grenade/vehicle gates, and logs each
fingerprint result. Physical input blocks stick grenades even during transient
inventory read failure. Unknown bytes still fail closed. The camera patch's
unaligned uint32 loads/stores are replaced with memcpy, preserving exact bytes.

Vehicle steering no longer needs grip, a grip binding, or proximity to animated
hand anchors. Both hands in a broad forward seated posture acquire a neutral
wheel/handlebar pose; lowering a hand restores right-stick fallback. The seat
view yaw is removed from input, including turret yaw, preventing feedback from
virtual rotation. Tracking/menu/recenter resets it. RT/LT Warthog driving and
Ghost movement-stick/RT firing are unchanged. The new Basic setting is
StylishHandVehicleSteering, default true.

Instead of latching separate first-person gloves, the vehicle pass renders a
copy of Chief's actual seated biped skeleton. Only head descendants are collapsed
in that copy, leaving native steering arms and torso. The ordinary local-player
world draw remains suppressed, avoiding double bodies. No live skeleton, map
tag, or vehicle animation data is modified. Passenger remains on its native path.

Portable tests cover production handlers/readers and headless-body ownership,
no-grip steering, camera/turn stability and existing controls/reloads. An extra
read-only EXE regression compiles the actual crouch patch and actual startup
gates against a temporary image buffer; it reproduces and fixes the R1.21 failure.
Windows/MSVC and headset validation remain outstanding. See
research/physical-interactions-R1.22.txt and RESTART-R1-README.txt.

## Previous checkpoint: R1.21

Adds either-hand chest grenades, physical release velocity, and seated
first-person Warthog driver/turret and Ghost driving. Latest requested grenade
rules supersede the old unused pin/arming prototype in InteractionModel.h:
no pin and no trigger to ready, grip release throws, matching chest release
returns. Plasma stays left chest and frag stays right, regardless of handedness.
Native counts/fuse/sticking remain authoritative. The pending request waits
for the native animation's existing release hook, captures its attached datum
before the call, then moves only the validated detached local-player projectile
and applies bounded hand velocity. It does not spawn objects or edit map tags.
The native control command and both unit type bytes are updated together to
prevent simulation from restoring an old grenade selection. Acknowledged throws
cannot re-press the grenade input while waiting for the hook. The grenade hand is excluded from swing melee through release/follow-through.
One additional release can queue; all requests have bounded lifetimes and ownership guards.

The seat reader uses the live biped's world-space skeletal head/hand transforms,
validated parent vehicle and native seat labels. Passenger/unsupported vehicles
retain their paths. Entry captures a headset offset so OpenVR adds physical
head movement only once; camera overrides are restored after rendering. Vehicle
heading carries the view with a level world-up basis, while the native aiming
command comes from wheel/handle gestures or unlatched right-stick fallback.
Warthog RT/LT is forward/reverse; turret and Ghost RT remains fire. Ghost retains
native stick throttle/strafe. Tracking/menu interruptions neutralize controls.

Rocket +X now follows the anatomical palm's across-finger axis. The pose uses
Chief's bind knuckles transformed through the current wrist, including mirrored
wielding. The captured release shares this basis and palm clearance. R1.20's
hand-local muzzle correction and the confirmed reload mechanics remain intact.

New portable tests compile the entire production interaction handler and the
additional native readers against controlled doubles. The supplied retail EXE
is verified read-only, including new grenade command/attach/release and skeletal
seat fingerprints. Windows/MSVC compilation, actual vehicle anchors and grenade
launch timing still need headset validation. See RESTART-R1-README.txt for
controls/install and research/physical-interactions-R1.21.txt for native evidence.

## Previous checkpoint: R1.20

The user's R1.19 video/log confirms the reload flow; this revision addresses
rocket preview orientation and the supported launcher's turn-dependent twist.
Both spent/fresh tool poses rotate +90 degrees around controller-local Z before
capture on release. Native and fallback draws therefore retain the same pose
without rotating interaction zones or adding rotation during either eye pass.

The old fire alignment was `cooked * gunWorld * inverse(handWorld)`. It was
then post-multiplied into the next supported hand pose as though it were a
local correction. Non-commuting gun/marker rotations carry world yaw into that
feedback path. Cache now stores `inverse(handWorld) * gunWorld * cooked`;
reticle, scope and shot compose `handWorld * localFire`. This is a shared
coordinate-space correction, not a rocket-specific fixed aim tilt. Position
offsets keep their prior formula and transient recoil is still removed before
caching. The look-at builder also handles parallel/antiparallel up vectors and
coincident first-acquisition targets without a collapsed basis.

Production-method regression checks cover rotated local gun/trigger frames,
mirrored skeletons, repeated update/render cycles, yaw changes, reticle/scope/
shot agreement and unchanged recoil. Restoring the old multiplication order
makes the new test fail. Held/dropped native rocket tests assert local axes,
scale and unchanged origin for both hands. Engine/VR doubles are used; a
Windows build and headset test remain necessary. R1.19 reload mechanics and
audio, and the confirmed shotgun behavior, are retained. Grenades are next.

## Previous checkpoint: R1.19

R1.18 shotgun behavior passed the user's headset test. Rocket insertion now
leaves the casing open; B/Y explicitly closes it after one or both replacements.
Ammo still commits one round per insertion and firing stays blocked while open.

The supplied R1.18 log detects native rocket reload-empty/full recordings, but
the mapper assigned no rocket cues. The empty recording is preferred for four
steps: open, extract, insert, close. Quiet-gap segmentation accepts four groups
or shares the middle of three groups for extraction/insertion. Fewer groups
remain haptic-only; a full clip is never replayed per step. Existing decoding,
owned-buffer lifetimes and loudness limits remain. Grab-spent routes Eject.

The first-person launcher contains tubes/body/foregrip/push_bar, with no
separate round node. The current weapon's primary-trigger projectile reference
is followed to its gbxmodel. Both held and discarded previews render only its
geometry/materials in the existing native first-person model pass, preserving
native geometry scale. The same model is greyed for spent rounds; a separate
spent model is not claimed. No projectile object or attached effects spawn.
Tag salts/classes, ownership, render context, model hierarchy and existing draw
fingerprints are checked. Missing assets retain the procedural preview.
See research/rocket-reload-R1.19.txt for the new read-only tag-field references.

The rocket support acquisition cylinder moves 10 cm up along the gun's local
up axis; its low edge rejects below-centre grips. Forward reach is retained.
Actual aiming remains hand-directed with no fixed rotation correction. All
other support zones and R1.18 shotgun behavior are unchanged.

Tests compile production handlers against engine/VR/native-memory doubles:
two-round open-casing cycles, write refusal/cancel, both hands, segmented cues,
native mesh routes and draw guards, stale/missing references, and grip heights.
Windows/MSVC, actual sound segmentation and headset mesh/grip fit need user
validation. Physical grenades remain the next feature phase.

## Previous checkpoint: R1.18

R1.17 is the user-confirmed baseline. The final supplied log shows the rocket
model has frame gun/body/foregrip/tubes/push_bar and the old handler selected
no action node. R1.18 maps frame tubes and uses the moving casing position
as the forgiving loading target. B/Y toggles the casing when no reload piece
is held. Grip/release removes one spent casing; supply-to-port insertion
commits one conserved round and automatically closes. Empty launchers need
two cycles for two rounds. Closing to cancel does not spend ammo. No physical
cover gesture remains, so spent removal cannot be routed into cover pulling.

Rocket and shotgun support zones advance 12/8 cm, with their prior width.
The model cache must match the weapon datum before applying an offset.
Shotgun strokes use 60% of the configured distance, return to within 60%
of that shorter stroke, and acquire within at least 18 cm of the pump.
Visual rail travel remains unchanged. Backward-only/release cannot chamber;
existing support restoration, shell ejection and cue ordering are retained.

Sniper magazine grip uses an anatomical palm-up pose and palm-attached native
magazine. The action clasp points forward like the AR rather than across the
receiver, preserving the sniper's lateral handle offset and reflected hands.
No pistol/AR pose or reload-audio changes. X+RT and B/Y routing are retained.

Tests cover both hands, ammo conservation/refused writes, reload cancellation,
actual rocket node mapping and port/pose agreement, short pump thresholds,
reflected native sniper hand bind poses, support zones and stale caches.
Windows/MSVC and real headset validation remain on the user's machine.
Physical grenade throwing is still the next feature phase.

## Previous checkpoint: R1.17

R1.16 passed the user's gameplay checks. The new log confirms R1.16 and stock
shotgun shell/hand nodes, but contains no audio samples or output errors to
prove the exact cause of the intermittent loud distortion.

The sound bank previously accepted a native buffer before requesting its
decode and checked complete cue ranges without checking sample stability.
A stable allocation header does not establish that an asynchronous decoder
has finished writing its contents. The reader now compares a second bounded
copy; the bank requests decoding first and requires identical sample contents
across at least three polls spanning 60 ms. Copies remain owned by the bank
and WinMM playback until their existing buffer lifecycle releases them.

Conservative per-channel 20 ms checks reject sustained extreme RMS, DC or
clipping. Rejected data retries and the old pending-cue expiry remains. Whole
cue attenuation limits peaks to 12000 and RMS to 4500 PCM16 units BEFORE the
user volume multiplier, never boosts quiet data and does not hard-clip or
alter stereo ratios. These guards only affect manual reload PCM playback;
normal gunfire and game sound mixing are untouched. A cold clip can wait for
validation; unsupported or persistently suspect data remains haptic-only.

The shell grasp closes non-thumb fingers while rebuilding the thumb at its
extended bind pose. The native shell attaches to a frame constructed from
the actual distal thumb bones/palm, offset 12 mm off the pad and preserving
the native shell rotation. The hand reflection supplies left-hand wielding.
Missing/custom thumb bones retain the prior visual fallback. Finger posing
is idempotent and never changes the gun, firing hand or insertion hit test.

Tests inject changing audio under an unchanged allocation header, partial
cues, stable noise, failed decode, stereo level limiting and queued-buffer
lifetimes. The complete production shell pose is tested with native-node
mapping, mirrored/rotated hands, finger closure, thumb extension and release.
Existing R1.16 controls/reload/recoil tests pass. No new hook addresses or
assets, bindings or settings. Windows/MSVC and real headset validation remain
on the user's machine. Physical grenade throwing is the next feature phase.

## Previous checkpoint: R1.16

The user reports that everything else in R1.15 passed. The supplied log ends
AR reload progress at an eject (53 retained rounds), then drop, pause and
re-pickup; later stow/draw attempts never show another AR insertion. The log
has no ammo snapshot at the failed supply grab, so it cannot prove the exact
native transition responsible.

A production-handler regression reproduces a deadlock when native ammo is
refilled outside the manual insertion path while the physical magazine is
still absent. Recovery returns these unseated rounds to reserve through the
existing validated, ammo-conserving commit, retains the magazine-out and
original chamber state, and retries if the native write is temporarily blocked.
Both hands and all three magazine-fed manual weapons use the same recovery.
The tests cover actual portable session drop/pickup/stow/switch transactions
with injected native refill, partial/full refill, zero reserve, native reload
in progress, refused writes and a magazine preview already held. Native ammo
in a seated magazine is untouched; exhausted ammo is not manufactured.

The AR magazine shifts 4 cm back to the anatomical palm centre, retaining its
R1.15 orientation, lateral centre and skin clearance. The successful charging
palm, compact support, menu X+RT and B/Y reload inputs are unchanged. Grip-event
supply diagnostics distinguish loaded/reserve/manual state without frame spam.
No new settings, native hooks, game assets or binding changes. The Windows DLL
and actual headset behavior remain for the user's build/test.

## Previous checkpoint: R1.15

R1.14 footage shows AR magazine clipping; the user reports that the Magnum
support hit test is still behind the controller despite its improved palm-up
skin. Compact support acquisition now uses raw controller origins for both
hands, centred forward/below the firing controller. It no longer compares a
raw offhand origin to the offset firing wrist. Existing slide priority and
skin pose are retained, and long-gun support acquisition is unchanged.

The AR magazine is centred across the knuckles and shifted away from the palm
and towards the fingers. The AR action palm is shifted another 6 cm left in
the gun frame; mirrored placement, orientation and rail movement are retained.

Per the user's revised control choice, physical campaign menu access is hold
left X then tap right RT, independent of wielding hand. Reload remains B for
a right-hand weapon and Y for a left-hand weapon. X-only grenade cycling is
deferred until release and cancelled for a menu chord. RT suppression is
resolved before gameplay input, retained through early X release, and cleared
only when RT releases. Input gaps do not fabricate menu/cycle presses.

The existing native Escape handoff was being overwritten by the later VR
MenuBack assignment. Native MenuBack now passes through unchanged. Escape is
held for 80 ms across native polling with checked injection and key-up service
independent of controller availability. Disabled/unsupported contexts retain
the original menu binding and hold-to-recentre behavior.

Portable tests compile the production menu/input pass and cover both wielding
hands, preserved Y reload, chord/cycle separation, trigger gating, key release,
input loss, injection failure and raw-controller support with a displaced wrist.
No new native hooks or addresses, game assets, settings or binding actions.
Windows/MSVC compilation and headset fit still require the user's machine.

## Previous checkpoint: R1.14

Continues the saved R1.13 code and the user's latest footage/log. Magnum
insertion has a minimum 18 cm radius along a 7 cm approach below its well;
a held reload magazine and supply separation are still required. Magnum
support is a palm-up cup farther forward/below the firing wrist, skin-only
and mirrored; it does not affect aim or supported recoil.

The slide test uses a short capsule along its native gun axis with a height
gate, checking both controller origin and the tracked anatomical palm. Palm
sampling runs before support skinning, so it cannot read the snapped support
position. Acquisition precedes support and also permits a held support grip
to move up to the slide. Near-slide grip presses log routing and local contact
positions for any remaining headset-only alignment problem.

The AR magazine now anchors in the anatomical palm pocket with additional
clearance, retaining the magazine node's native orientation. Its charging
clasp faces along the rifle while palm-down at the fixed action anchor.
Other compact/long-gun poses, audio and native hooks remain unchanged.

Portable production-code checks cover both hands, forgiving insertion without
repeat ammo commits, fresh/support-to-slide contact through the visible palm,
AR magazine/action transforms and forward/upward Magnum support palms.
Full Windows/MSVC build and in-headset visual validation are not available here.

## Previous checkpoint: R1.13

Built on the exact saved R1.12 source ZIP (SHA256
`13daa4a8969af832ab55c5aecd368c14acadc63ec48b3496e559b438a90a453a`).
The user confirmed the segmented recordings work; their footage shows inverted
AR wrists, a poorly seated Magnum magazine, and rifle-style pistol support.

Reload wrists now use an anatomical palm frame from wrist/index/pinky bind
bones. Fingers curl from their own bind hierarchy rather than copying the
partly open firing hand. AR/sniper palms face down at the handle, with added
left-side clearance. Magnum palms face down across the slide. The Magnum
magazine is placed in the real hand's finger pocket with its thumb edge up.
These poses mirror with wielding hand and do not move gun/firing-hand bones.

Compact support (Magnum, Needler, Plasma Pistol, Plasma Rifle) is a handle cup,
with recoil and haptics support independent of two-hand aim steering. The
firing controller alone aims these guns. Other guns retain offhand steering.
A supported Magnum can transfer directly to the slide while grip stays held;
slide acquisition has a height gate so a handle grip does not steal it.
The magazine-zone label is centred in the arc.

The audio bank keeps R1.12's segment rules but waits for all required parts
before accepting a decoded recording. Incomplete buffers/timeouts are retried;
expired cues cannot replay in a later burst. Per-poll decoded-buffer logging
was removed and readiness/output failures are distinguished. Native hooks,
addresses and memory-writing paths are unchanged.

Portable checks cover anatomical palm orientation and finger lengths, mirrored
hands, production support-skin code and aim routing, support-to-slide transfer,
ammo preservation, late native audio decode and the prior regression suite.
MSVC and in-headset appearance remain unverified here. Existing config values
and bindings are retained. Physical grenades remain the next feature phase.

## Previous checkpoint: R1.12

R1.11a compiled and ran in the user's supplied log. This update adds fixed
native-part action wrist poses, a lower apparent Magnum magazine grasp and
free Magnum slide movement. The free slide is usable with any ammo state,
blocks fire while held and never changes ammunition; required empty reloads
still need a complete rearward pull. All hand offsets follow the weapon frame.

Full reload recordings no longer play at each step. Sound selection stores
ordered slice indices, and a bounded read-only adapter copies Halo's decoded
stock Xbox ADPCM PCM cache. Quiet gaps split the copied recording into the
requested two or three actions. Segments use a bounded asynchronous WinMM
queue on Windows default playback output, with no synthesized/bundled audio,
map writes or raw game-cache pointers retained. Unsupported codecs/ambiguous
recordings retain haptics and log a diagnostic without a full-clip fallback.
Shotgun sound-only chamber tags cannot duplicate the native pump recording.

Portable checks cover both hands, loaded/empty/mag-out free slides, ammo
conservation, fixed wrist poses under tracker rotation, native selection,
PCM bounds/ownership, silence segmentation, FIFO playback and buffer lifetime.
Native x86 hooks/addresses are unchanged. The real PCM content/cut points and
final wrist appearance need headset validation; MSVC is unavailable here.

## Previous checkpoint: R1.11a

Compilation hotfix for the three C2248 errors in the user's R1.11 MSVC build.
InputHandler and the native audio/effect callbacks now query public Game
forwarding methods; reloadHandler remains protected. No reload, sound, pose,
input mapping or configuration behavior is changed.

The portable input/audio fixtures now preserve protected-member access and
compile the forwarding methods from Game.h, so the earlier direct accesses
would fail these checks. Portable checks pass; a complete Windows/MSVC rebuild
and the R1.11 headset checks are still required.

## Previous checkpoint: R1.11

The user confirmed R1.10 reload mechanics; remaining issues were hand closure,
Magnum slide drift, native shotgun pumping/ejection, shell appearance/port height,
and synthetic audio. R1.11 maps the native firing-hand grasp onto the offhand
using matching anatomical/Biped nodes and the two bind poses. It translates
actions along the model's gun axis and retains the shotgun support grip and
clean supported wrist offset during pumping. Neither pose adjustment writes
back into the aiming cache.

The shotgun frame bullet supplies its native textured shell preview even when
animation hides its bone. Its native magazine node anchors the loading port,
raised 6.5 cm, with a minimum 19 cm insertion radius. One spent-shell flag is
consumed per rearward stroke after a shot; repeated pulls cannot duplicate it.
An empty pump completes without enabling fire or granting ammunition.

Generated Windows WAV cues were removed. A checked animation-graph reader
selects the current weapon's native sound references. Three additional verified
retail entry points support sound playback/stop and weapon-effect dispatch.
Preserving x86 register/stack shims defer only the owned manual shotgun's
first-person pump sound and chamber effect; firing-effect blast calls, AI,
multiplayer, other guns, disabled controls and invalid ownership pass through.
Manual effects are replayed through the original trampoline once per spent shell.
If a map has no chamber effect, its shell node supplies a falling preview.
No shared tags, sound data or map resources are modified.

Authored native sound clips are used as supplied, not extracted into discrete
samples. Grabs/drops remain haptic-only where no step clip exists. Clip content,
particle timing and visual appearance require a headset test with loaded maps.
Portable production fixtures cover native and fallback hand posing, rotated
rails, hidden shell bones, raised port tolerance, both hands, last-round/regrip
pumping, native sound selection, hook guards/rollback and audio lifetime. All 30
on-disk retail fingerprints match. No MSVC or in-game test is claimed.

## Previous checkpoint: R1.10

R1.9 AR/pistol magazine transfers were confirmed working by the user; empty
reloads could not acquire the action. The user's log identifies the AR node
as `frame ophandle` and the pistol slide as `frame slide`. The old substring
matcher missed the former and overwrote the latter with `frame slide_release`.
R1.10 recognizes the handle and ranks exact part names over aliases, rejecting
release buttons. Existing magazine transactions and the AR reload sequence stay.

Full backward pulls may spring closed on release for magazine-fed guns. The
shotgun still needs a complete backward/forward pump. Action grips snap the
native offhand wrist to the animated part; the hand stays constrained to its
travel until release. Magazine/action grips close Chief's existing finger bones
from the hands-model bind pose, without moving the firing wrist or gun hierarchy.
No grip transform is written into the aiming cache.

Reload steps emit a haptic pulse on the manipulating hand and an original short
mechanical cue. `StylishReloadSoundVolume` defaults to 0.6 (0 mutes), using the
Windows default output endpoint. Fixed-buffer asynchronous playback requires
no bundled game samples, external files or new engine detours. Sound failure is
logged once and cannot block reloads.

Long-gun support grips begin 20 cm ahead of the firing grip, with a 14 cm lateral
radius. Pistol/plasma-pistol/Needler keep their close support geometry. Manual
action grips still run before support acquisition. SupplyHip moves from
(0.22, 0.16, -0.43) to (0.14, 0.25, -0.43) metres (forward, left, up), mirrored by
the saved layout. Legacy SwapHandDistance/TwoHandDistance are not used here.

Portable production fixtures cover the logged bone names, each firing hand,
spring release and incomplete pulls, action latch and finger pose isolation,
magazine transaction conservation, support geometry and audio buffer lifetime.
The 27 existing retail fingerprints still match. No MSVC build or VR runtime
test was possible here; see RESTART-R1-README.txt for the focused headset checks.

## Previous checkpoint: R1.9

R1.8 recoil, settings and physical holsters are confirmed working by the user.
R1.9 integrates manual reloading for the pistol, assault rifle, sniper, shotgun
and rocket launcher in the verified retail campaign. Needler reload and plasma
behavior remain native. Either firing hand works; the supply zone follows the
saved handedness layout. A cyan arc labeled Magazine Zone marks the supply hip.

B (right-hand gun) / Y (left-hand gun) ejects detachable magazines. A free offhand
grips a supply magazine and inserts it into the gun. Only an empty-before-reload
gun needs the charging handle/slide pulled back and returned. Shotguns load at
the top port and require a manual pump after firing or loading from empty.
StylishShotgunShellRatio defaults to 1, clamped to 1–4; each insertion consumes
exactly the number of rounds added from reserve.

The rocket launcher uses a pull-open cover, individual spent-casing removal,
individual fresh rockets released at the port, and a pull-closed cover. Two
missing rounds require two fresh rockets. Remaining live rockets are retained.
The launcher cannot fire while open. Tactical magazine rounds return to reserve;
a dropped fresh supply preview does not destroy ammo. Full salted weapon IDs,
validated local ownership and checked ammo transactions prevent stale writes.

Existing native reload hooks gate auto/manual animation reload only for these
owned campaign guns. An already-running native reload may finish on activation.
Ammo changes do not select a weapon, rebuild a model, or mutate shared tag data.
The accepted selection repair, hand/holster rendering, recoil, flashlight and HUD
are unchanged. Native detachable parts are mapped by node name to the current
animation skeleton, with simple preview meshes and controller-space fallback
ports when a part is not available. Unknown skeletons never reuse another gun's
bone indices. Part alignment and native hook behavior still need headset testing.

See RESTART-R1-README.txt for controls, build/install and settings. Portable tests
compile the production reload controller and native ammo adapter against engine
and VR doubles; all 27 retail fingerprint blocks match the supplied executable.
This source has not been built with MSVC or run in Halo/VR here. Independent dual
guns, physical grenades and vehicle controls remain future work.

## Previous checkpoint: R1.8

R1.7 weapon selection and holsters are confirmed working by the user. R1.8 adds
per-shot physical recoil for the currently held campaign gun. Both hands must
hold that same gun to receive the default 65% reduction in the added kick.
Automatic fire accumulates kick; pitch and backward travel are bounded and
recover exponentially. Pistol, AR, shotgun, sniper, rockets, plasma guns,
Needler, flamethrower and fuel rod have separate profiles. Confirmed retail
firing state prevents dry trigger calls from adding recoil. The existing hook
is reused; the firing observations are read-only and separately fingerprinted.

The same transform drives the visible weapon, scope, reticle and subsequent
bullet aim. The kick is removed before alignment caching and excluded from
physical grip geometry. HMD/camera pose is not kicked. Timing is independent
of frame rate, and render queries never advance the spring. A different gun,
player, stow, invalid input, menu, cutscene or unsupported mode clears the kick.
Stock multiplayer and 3DOF are outside this campaign physical recoil feature.

`VR/config.txt` and VR Settings > All Settings expose `StylishRecoilScale`
(default 1), `StylishSupportedRecoilFraction` (0.35), and weapon multipliers
such as `StylishRecoilAssaultRifle` (1). Defaults are registered at startup;
existing settings are preserved. The first R1.8 installation needs a DLL
rebuild. Subsequent tuning needs no compilation. See RESTART-R1-README.txt.

The hip caption now reads Right Hip (Here), or Left Hip (Here) with the saved
left-handed layout. The flashlight touch target is exclusively the LEFT temple,
regardless of layout; either hand can grip there, including while holding a gun.
The accepted holster positions, selection fix and visuals remain in place.

Portable production-code fixtures and exact retail fingerprints pass. This
source release has not been compiled with MSVC or run in a headset here.
Runtime recoil feel is the remaining validation. Manual reloads, independent
dual guns, physical grenades, grenade/magazine holster meshes and vehicles remain
pending. They are not part of this checkpoint.

## Previous checkpoint: R1.7

The R1.6.1 headset log contains 1,072 model resets. The new repair synchronizes
the local player's persistent weapon-selection command with the requested unit
slot. Previously, Halo could reapply a valid old selection every simulation tick,
undoing a draw and forcing another model rebuild. Selection validates the control
block's full owner datum and restores both command and desired slot if it fails.
The existing pose-boundary remap refresh remains for actual draw transitions.

The shoulder's center-waist indicator is horizontal, 4 cm closer to the body and
17% smaller than R1.6.1. The actual shoulder grip zone and the hip placement are
unchanged. Empty homes show a compact outlined EMPTY badge. Small labels read
Right Shoulder / Right Hip, or Left Shoulder / Left Hip with the saved handedness
preference. Equipped homes still show the accepted grey weapon silhouettes.
Labels use the existing UI font, queued once per frame for both eyes; no external
textures, fonts or game models are added. Existing Chief hands and HUD remain.

The portable regression reproduces the old selection being undone by a simulated
native control tick and verifies stable selection with R1.7. Both-handed layouts,
empty versus reserved homes, rendering setup and existing tests pass. These are
synthetic engine/device tests, not Windows compilation or in-game confirmation.
See `research/r17-selection-command.md` and `RESTART-R1-README.txt`.

## Status and scope

This is a development checkpoint, not the complete requested mod. The portable
interaction model can be tested without Halo. Passing those tests does not prove
that Halo renders, fires, drops or reloads objects correctly. Engine integration
status is tracked separately below. No Windows DLL or headset test is claimed.

The target is the user's PC HaloCEVR installation with Quest 3 controllers.
Campaign is the initial validation target. Multiplayer synchronization is not
implemented by this checkpoint.

R1 source implementation connects one native held weapon, either/both hands,
two mirrored holster homes, native stow/drop transactions and support-only
retention. The R0 head-grip flashlight remains integrated. Retail engine
mutations require the verified executable profile and code fingerprints.
Action inputs distinguish an inactive/failed sample from a deliberate release.
Portable tests cover the session transactions and production input/reader code
against doubles; native execution and headset visuals remain unverified.

R1.2 uses grip for loose weapon pickup and right A to jump in physical mode.
It still uses native reloads and grenade throws (temporarily right-stick up).
It does not implement a third held weapon, dual independent guns, manual
reloading, physical grenades, new recoil, vehicles or the final crouch control.
Use `Build-Restart-R1.ps1` and follow `RESTART-R1-README.txt` for this test.

R1.1 repairs activation/fallback handling and connects genuine headset motion
to the retail campaign's look-history flags used by tutorial scripts. The
starting/current gun is assigned to the hip first; the second goes to the
shoulder. Old SteamVR bindings can supply fixed-hand grip/fire fallback.
These changes have portable regression coverage but still need a Windows build
and headset test. Multiplayer retains stock controls in this checkpoint.

R1.2 blocks automatic acquisition of a different loose weapon at the local
campaign player/world-item contact handler. Native ammo scavenging, equipment
and direct scripted grants are retained. A fresh grip press takes the offered
weapon into that hand if the other gun is stowed and native inventory has room.
Two occupied inventory slots reject pickup without swapping. Newly gripped
weapons receive a home only when stowed; surviving homes and held hands persist
across native grants and inventory reordering. The hip anchor moves forward
12 cm and inward 6 cm, and the shoulder forward 11 cm. New fixed-hand button
actions include old-binding fallbacks where the original action is on that
physical hand. Inactive optional grenade binding does not disable jump.
R1.2 needs a Windows build and in-headset validation; portable and exact-binary
checks are not a substitute for that test.

### R1.3 — holster reach, empty hands and tutorial UI

Restores the original shoulder anchor (-0.15, -0.24, -0.22 metres relative to
head height/yaw), raises the R1.2 hip anchor by 13 cm to (0.15, -0.16, -0.52),
and retains mirrored layout and configured radii. Either hand receives a short
entry pulse; a 3 cm exit margin prevents buzzing at the boundary.

Independent, original procedural glove meshes render when physical weapons are
stowed, including before the first gun. SteamVR skeletal joints drive them when
available, with a controller-relative open-hand fallback. They are not extracted
Chief meshes. Equipped weapons continue using their existing native hands.
Holster item meshes listed below are still pending.

Menu pointing now explicitly intersects either tracked controller ray with the
SteamVR UI overlay and sends fresh-trigger clicks. Lost hits/tracking cancel the
click and require a new press. Right A also supplies native MenuForward (including
old personal bindings), with normal right-A jump retained during gameplay. Real
mouse input is no longer erased when VR has no pointer/click. Dialogs which do
not expose mouse visibility can use right A to confirm the focused button.

The exact retail first-person visibility transition is fingerprinted and hooked
so stowed weapons request native effect shutdown, not just a skipped mesh draw.
A confirmed stow also invokes the shutdown, covering the native early-out on no
selected weapon. This targets the floating plasma-pistol glows without disabling
world plasma effects. No saved inventory or weapon-tag visibility is modified.

Portable tests and exact-executable verification passed. R1.3 has not been built
with MSVC or tested in a headset here; visual alignment, native effects and tutorial
confirmation remain runtime checks.

### R1.3.1 — startup recovery after reported crash

The supplied R1.3 log reached OpenVR initialization and the first menu updates;
it contains no fault address. The exact crashing instruction is unconfirmed.
The empty-glove draw now validates live campaign context before touching map
state. A production-method fixture fails on the R1.3 ordering and passes on
R1.3.1. Early UI callbacks also require initialized VR/menu objects.

This recovery build uses the original SteamVR overlay events and removes the
extra action polling during rendering. The R1.3 direct intersection helper is
retained in source for investigation but is not called. Right A confirmation
and the real-mouse fallback remain. The new native first-person visibility hook
and direct cleanup calls are removed; plasma-glow suppression is deferred.
Holster reach, hover haptics, empty-glove geometry and R1.2 inventory behavior
remain. A local Windows vectored exception observer writes VR/StylishCrash.log
with fault/module addresses and registers, always returning CONTINUE_SEARCH.
No errors are swallowed. First-chance exceptions may still be handled natively.
Windows compilation, exception logging and headset boot remain unverified here.

### Visual holsters requested 12 October — pending implementation

- Weapon 1: actual weapon mesh at the dominant-side hip.
- Weapon 2: shoulder draw zone, with a small separate indicator at the hip;
  no shoulder weapon mesh required.
- Frag and plasma: visible grenade meshes at the two chest homes.
- Magazine supply: visible pouch/holder at the opposite hip.
- Mirror the layout for left-handed preference. Visual anchors must match grab
  anchors. Hide a drawn item's duplicate and show slot occupancy accurately.

The current build still renders only the weapon-home test rings. Chest grenade
meshes, the magazine pouch, holstered gun mesh and separate shoulder-at-hip
indicator have not been implemented. They follow the control validation pass.

## Required behavior

1. Hold grip to hold an item. Releasing over a compatible empty holster stores it;
   releasing elsewhere drops a visible world object with physics. Never silently
   swap the old weapon out to make room for a pickup.
2. Right-handed layout: weapon 1 on right hip; weapon 2 on right shoulder;
   magazine/shell/rocket supply on left hip; plasma grenades on left chest and
   frag grenades on right chest. These grenade sides are initial defaults.
   Any weapon type fits either weapon holster. Left-handed preference mirrors
   the entire layout, without restricting which hand can draw an item.
3. Frag grenade: hold grip, grip its pin with the free hand, pull away, then
   throw by releasing the holding grip. Plasma grenade has NO pin: press RT
   when held in the right hand or LT when held in the left hand to arm, give
   haptic/audio confirmation, then swing and release grip. A trigger already
   held when drawing must not automatically arm a plasma grenade. An armed
   release at low velocity is still a physical drop, not a canceled throw.
4. One-handed recoil is substantially stronger. Only an actual second hand
   holding that same gun grants support; a hand holding another gun, a magazine,
   a grenade or the charging handle does not. Recoil belongs to each gun, not
   to the player's global handedness. Starting tuning target: supported recoil
   35% of unsupported recoil, adjustable per weapon.
5. Manual reload details are in the table below. Ammo changes must follow
   confirmed successful transfers. Native auto reload/chamber animation must
   not bypass the physical actions or charge the player twice.
6. Every weapon can be held, fired and reloaded in either hand. The free hand
   performs the manipulation. Magazine wells and charging handles remain on
   their model's physical geometry; do not mirror the weapon itself merely
   because it is held in the other hand.
7. VR Settings offers a right/left-handed layout preference. Drawing in the
   other hand must NOT change the saved preference or move the holsters.
8. A visible weapon pickup prompt is sufficient to grip-pickup with an empty
   hand. Two occupied/reserved weapon slots do not prevent temporarily holding
   another world weapon, but it cannot be stored until a slot is freed. Drawing
   a holstered weapon reserves its home slot; dropping that gun frees it.
   A pickup must never evict the other gun. Preserve object identity and ammo.
9. Keep native walk-over grenade replenishment.
10. Warthog: first-person driver position, physical wheel grip/turning with
    either/both hands, RT forward and LT reverse. Ghost: analogous physical
    handlebar steering and drive controls. Scorpion/Banshee: first-person
    vehicle views with their aiming reticles. Seat entry/exit must cleanly
    change input context; turret/passenger seats need separate treatment.
11. An empty hand reaching toward an interaction uses that hand's trigger.
    A held plasma grenade consumes that trigger for arming. A weapon's trigger
    fires that weapon. A reload part does not count as an empty hand.
12. Both holstered guns can be drawn and used simultaneously, with independent
    muzzle poses, triggers, ammunition, recoil, reload states and meshes.
13. A on the right controller jumps, regardless of handedness preference.
14. Right thumbstick turns; left thumbstick moves.
15. Proposed conflict resolution: left THUMBSTICK click toggles crouch.
    Left grip is already used to retain items and cannot also toggle crouch
    on every pickup. This is an explicitly stated implementation assumption.
16. Both grips must remain held for two-handed support. Letting go of the firing
    grip leaves the gun held by the support hand at its foregrip; it does not
    silently snap its pistol grip into that hand or permit firing by the barrel.
    Regrabbing the firing grip restores fire. Letting go of only the support
    grip returns to unsupported fire. The last hand releasing stores/drops it.
17. Flashlight: EITHER hand may touch EITHER temple with grip held. In the
    example, right grip retains the assault rifle and left grip at either side
    of the head toggles the light. A weapon already held does not block this.
    One toggle per head visit; move away to rearm. Require a deliberate short
    dwell if grip was already held before moving to the temple, so reaching
    behind the shoulder does not flicker the light.

Bonus: reduce assault-rifle spread and its reticle size independently. Reticle
scaling alone must never be presented as an accuracy improvement.

## Reload contract

The weapon hand's upper face button ejects/reloads: B on the right controller,
Y on the left. This is the proposed left-handed equivalent of B. A remains jump.
Capacity comes from the actual weapon, not an assumed constant for every map.

| Weapon | Reload | Chamber/action requirement |
|---|---|---|
| Magnum | Eject, grip spare magazine at supply hip, insert into well | Pull and release slide only if emptied before reload |
| Assault rifle | Eject, draw magazine, insert | Pull and release the physical left charging handle if emptied |
| Sniper rifle | Eject, draw magazine, insert | Pull and release the charging handle if emptied |
| Shotgun | Draw one shell, insert through top loading port; repeat | Full pump cycle after each shot and after an empty reload |
| Rocket launcher | Pull casing open, insert one rocket per missing round, close casing | No extra magazine/charging action; capacity two for requested behavior |
| Needler | Weapon-hand B/Y invokes native reload | Native behavior |
| Plasma rifle/pistol | No ammo reload | Keep native heat, charge and battery behavior |

Partial-magazine retention, exact attachment transforms and pump/handle stroke
lengths require visual tuning. Development model keeps remaining ejected ammo
as an explicit payload, never silently erases or creates it. Release of a held
reload item produces a dropped payload for the adapter, not an automatic return
to reserves.

## Architecture and priority

Keep physical hands separate from layout preference and active native weapon.
Resolve one grip action per hand. An existing attachment/reload manipulation
wins; a deliberate head touch consumes only that gesture; a visible world
pickup prompt wins over an overlapping holster; then choose an eligible nearby
holster. Geometric constraints use weapon-local points and headset-yaw/body
anchors, never full headset pitch/roll for hip holsters.

Use press/release edges for acquisition, continuous grip for retention, and
explicit trigger edges for grenade arming. On pause, tracking loss, loading,
death, recenter or vehicle entry, suspend input and reconcile engine objects.
Do not interpret a failed tracking sample as a deliberate throw/drop. A resumed
session must require fresh input; it must not fabricate a release or shot.

Model operations are proposals until the native action succeeds. Run them on
a copy; commit that copy only after the adapter confirms the native result.
Never free a holster, debit ammo, erase a payload or signal a successful pickup
because an unverified native call was merely attempted.

## Source audit / remaining engine integration

| Feature | Existing source seam | Remaining work |
|---|---|---|
| Pickup | `PlayerDatum::interactionObjectID`, `Controls::PickupWeapon` | Validate prompt category and native pickup result; suppress forced replacement |
| Holsters/drops | R1 `WeaponSession`, `RetailWeaponEngine`, physical input adapter | Native selection/drop integrated for exact retail campaign profile; headset-check world mesh, pose/velocity, collision, ammo and support-only hands |
| Two guns | Single `cachedViewModel`, global `bLeftHanded`, one fire path | Separate per-gun render/aim/reload state; verified second-weapon fire and inventory handling |
| Grenades | `H_ThrowGrenade`, unit `thrownGrenade` handle, controller velocity | Native spawn/hold/detach timing, type/count, visuals, fuse, collision and velocity units |
| Reload | `H_ReloadStart`, `H_ReloadEnd`, `WeaponDynamicObject::weaponData` | Suppress native auto transitions, verified ammo transfer, component rendering and animation |
| Recoil | `H_FireWeapon`, hand/muzzle transforms | Kick on confirmed discharge, independently per gun; physical support integration |
| Vehicles | `bInVehicle`, `UpdateCameraForVehicles` | Identify vehicle/seat, driver camera anchors, steering/throttle hooks and exit controls |
| AR accuracy | Weapon tag is mostly opaque in `Assets.h` | Verify actual trigger spread fields; local/runtime reversible tuning and reticle identification |

The DLL wrapper's source is available, but the full Halo engine implementation
is not in this repository. R1 adds verified retail selection/drop entry points
and guarded object-table reads; it does not provide arbitrary object creation,
third-weapon attachment or second-viewmodel rendering. Those remaining features
need additional verified engine interfaces and runtime observations.

## Validation gates

- Portable tests: hand ownership, simultaneous releases, reserved slots,
  temporary extra pickups, mirrored layout, flashlight conflicts, grenade
  arming/release, ammo conservation, empty versus tactical reloads, shotgun
  pumping, one/two-rocket reloads and rejected native transactions.
- Windows Release/x86 compilation against the real solution and dependencies.
- In headset: stock 1.4.0 launch first; log exact game version/edition; test
  each mechanic in both hands, then combinations and save/load/vehicle entry.
- Visual tests: meshes, pins, magazines, moving parts and dropped world items.
  Logic tests cannot certify any of these.
