# R1.9: manual reload integration

Based on the user-confirmed R1.8 commit
`73e134338c0aae0479ddc514af98e13c6318087f`. Physical selection, pickup,
holster allocation, native model refresh and recoil are not rewritten.

## Retail evidence and mutation boundary

The supplied Halo retail 1.10 EXE has SHA256
`b84131f2ee8cbf8d0dbda71d1d920059318bc55b36760dce32735026ced099a8`.
Reload-start is `0x4c35b0`, cdecl `(weapon datum, short magazine, bool force)`.
Reload-end is `0x4c3900`, with magazine in CX and weapon at entry `[ESP+4]`;
the caller cleans the weapon argument. Existing upstream hooks are reused.

Disassembly at `0x4c3923..0x4c39cc` independently establishes the retail layout:

- Object magazines start at `+0x2b0`, stride **12** (not CE's 16).
- Signed 16-bit reserve is magazine `+6`; loaded is `+8`.
- Tag magazine block is `+0x4f0/+0x4f4`, stride `0x70`, capacity `+0x0a`.
- Reload-end transfers rounds and can restart reload for another shell.
- Magazine state: 0 idle, 1 reload in progress, 2 reload ended, 3 chambering.

The added interior fingerprint is outside the already-detoured hook entry.
The runtime reader requires the verified profile, live campaign biped, local
ownership, full salted weapon/tag IDs, readable object size/type, supported
stock tag path, valid magazine block and bounded ammo counts. A mismatch keeps
native reload behavior. Needler, plasma and unknown/custom paths are excluded.

Commit re-reads current ammo and ownership, requires state 0 or 2, checks total
round conservation, and writes only four adjacent bytes (reserve and loaded).
Post-verification requires the same owner/tag/counts. Rollback is attempted only
for the same still-owned allocation with the just-written values still intact.
It never changes selection, calls ready/drop/pickup, or edits shared tag data.

Start/end guards block native reload only for recognized owned campaign guns
while the feature is active. An already-running reload/chamber is allowed to
finish. The end-hook prefix preserves flags and all eight general registers,
reads the weapon at saved-stack `ESP+40` (4 + PUSHFD 4 + PUSHAD 32), and restores
the original stack on both branches. The stock continuation is unchanged.
Linux cannot execute this MSVC/x86 native hook; it still needs a Windows test.

## Interaction states

State is keyed by full weapon datum/tag and resets on player allocation change,
checkpoint age rollback, feature deactivation or unsupported context. A menu or
tracking gap cancels a transient tool but preserves the owned gun's manual state.
Grip edges remain governed by the accepted input adapter; recovery invents no
release/press. Supply previews are single offhand tools, consumed only after a
confirmed insertion; release away from a port cannot destroy reserve rounds.

Detachable magazines preserve remaining rounds in reserve on ejection. Only an
empty-before-reload gun requires the action. Shotguns require a pump after a
confirmed shot and after loading from empty. A held support grip becomes a pump
interaction when required and returns to support after a completed stroke.
The ratio setting adds 1–4 rounds, clamped to remaining capacity/reserve, with
the same deduction from reserve. No input repeat can insert the same tool twice.

Rocket state tracks cover and spent casings independently from native live ammo.
Each discarded spent casing frees a space; fresh rockets require grip release
at the open port. Replacement can be interleaved one at a time or all spent
casings can be removed first. The cover blocks firing while open. Partial loads
are allowed. Native saves contain ammo, not the transient cover/tool state.

Manual offhand gestures consume that frame's grip before the weapon session
considers a support/holster grab. They do not move a stored weapon into a hand.
The supply anchor mirrors the accepted weapon hip anchor using saved handedness,
not the current firing hand. Head-flashlight consumption takes priority.

## Rendering and validation limits

The supply marker is a cyan arc and existing-font label queued once per frame.
Model nodes come from the current weapon's first-person model and map to the
animation skeleton by name. Only descendants of the current gun bone qualify.
Bind transforms provide native magazine/action locations. Detaching a magazine
moves its descendants, not the gun/wrist/other weapon. Native shotgun pump and
rocket cover movement are driven by the manual state where recognized. Loaded
pistol/rifle firing actions keep their normal animation. If a part is missing,
simple original meshes and per-weapon controller-space ports allow interaction.
Native firing effects/sounds are not retimed. Bone naming/alignment and physical
feel cannot be validated without the user's loaded maps and headset.

Portable fixtures compile the complete production reload controller, its real
state/model/matrix code and the production native ammo reader/commit methods.
Coverage includes either hand/layout, tactical/empty reloads, failed writes,
stale transactions, reserve conservation, dropped/cancelled tools, ratios,
repeated supported pumping, one/two-rocket replacement, native/disabled contexts,
gesture discrimination, skeletal index mapping and unchanged gun/wrist bones.
Existing input, inventory, holster, HUD, recoil, model-cache and OpenVR fixtures
remain passing. The exact verifier covers 27 blocks in the supplied EXE.

This is source verification with doubles, not an MSVC build or Halo/VR run.
