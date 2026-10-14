# Retail engine investigation — 12 October 2026

## Evidence and limits

The user's stock 1.4.0 launch log identifies `halor`, version
`01.00.10.0621`, successful OpenVR initialization and Chimera compatibility mode.
The user only opened the game; this is NOT an in-level gameplay baseline.
Their supplied `halo.exe` matches the SHA-256 in `retail-1.10-profile.json`.
The originals have not been modified and must never be bundled with our source.

Source baseline remains LivingFray/HaloCEVR tag 1.4.0, commit
`ada2d3ba2965bbb9c6d6d653456b0ba0a897cf63`. R1 now uses this investigation in
`RetailWeaponEngine`, guarded by fingerprints in `RetailProfile.h`. This remains
source-only work; the research files themselves do not establish runtime success.

Run `python tools/verify_retail_profile.py path/to/halo.exe` to reproduce the
exact on-disk check without executing Halo or installing Python dependencies.
This check deliberately refuses similar or modified builds. It is not a
runtime hook resolver: the existing mod hooks modify some function entries.

## Confirmed instruction paths

Addresses below are virtual addresses in this exact PE32 image at `0x400000`.
Store RVAs in code and verify the image before resolving them. Do not infer
other edition addresses by subtracting a constant.

| Function | VA | Observed calling convention | Important behavior |
|---|---|---|---|
| Ready desired weapon | `0x56D6E0` | cdecl(unit datum, force), no useful return | Reads desired slot at unit `+0x2F4`, puts away current weapon, activates desired or makes the unit unarmed for `-1` |
| Drop current weapon | `0x56DEC0` | cdecl(unit datum, force), success in AL | Calls native item drop, clears inventory entry, chooses next weapon |
| Get inventory weapon | `0x569970` | unit in EAX, signed slot in CX, result EAX | Returns full object datum from unit `+0x2F8 + slot*4` |
| Drop/detach item helper | `0x56ED00` | cdecl(unit datum, item datum) | Restores visibility, detaches, initializes physics/velocity, collision checks; does NOT itself remove a weapon inventory entry |
| Reload start | `0x4C35B0` | cdecl(weapon datum, short magazine, force) | Starts native reload state and animation |
| Reload end | `0x4C3900` | magazine index CX; weapon stack argument; caller cleanup | Transfers rounds, updates state, may start another reload iteration |
| Throw grenade | `0x56E440` | cdecl(unit datum, bool) | Uses grenade state `2` and attached projectile, detaches, applies velocity, clears grenade datum |
| Set world object position | `0x4F51C0` | position pointer EDI; object, forward and up pointers on stack; caller cleanup | Updates object transform and spatial data; null forward/up preserves orientation |
| Validate dropped object position | `0x4F7B70` | previous position ECX; object and ignored-object datum on stack; caller cleanup; AL result | Native collision validation; R1 restores the previous position if rejected |

For Ready, disassembly through the return at `0x56D986` shows both the armed
and unarmed branches. It detaches the old weapon, marks it hidden and records
its owning inventory unit through `0x4BCFC0`; it does not debit ammunition.
The new weapon is attached through `0x4F6180`, with readiness/animation updates.
Native callers at `0x468CDC` and `0x47932C` establish stack arguments and cleanup.

For Drop, disassembly through `0x56DFC3` shows refusal paths and success in AL.
The helper at `0x56ED00` handles native world physics, with a randomized impulse
and collision handling. The outer function clears the old slot and schedules
another native weapon. A physical inventory adapter must stop that automatic
selection when the remaining weapon is still holstered. Calling only the helper
would leave inconsistent inventory bookkeeping. Do not manufacture a replacement
object or copy ammo into a second weapon as a workaround.

The drop paths can delete objects under certain game/network/collision
conditions; a true return alone is NOT proof of a surviving world weapon.
Re-read the inventory and object table, including the object's generation, after
each operation. Never keep pre-call object pointers across mutation.

## Verified layout details

The routine paths use an object table pointer at `0x8603B0`, with element-array
pointer at table `+0x34`. Entries are 12 bytes; dynamic-object pointer is at
entry `+8`. The low 16 bits of a datum select an entry; the upper 16 bits are
its generation. New engine operations need generation/type/bounds checks.
The upstream `Helpers::GetDynamicObject` does not check generation and is not
adequate for physical inventory ownership transactions.

| Unit field | Offset | Size |
|---|---|---|
| Player datum | `0x218` | 4 |
| Grenade state | `0x28D` | 1 |
| Attached grenade datum | `0x294` | 4 |
| Seat index | `0x2F0` | 2 |
| Current weapon slot | `0x2F2` | 2, signed |
| Desired weapon slot | `0x2F4` | 2, signed |
| Weapon datums | `0x2F8` | 4 entries × 4 |
| Weapon ready timestamps | `0x308` | 4 entries × 4 |
| Equipment datum | `0x318` | 4 |
| Current/desired grenade type | `0x31C` / `0x31D` | 1 each |
| Grenade counts | `0x31E` | 2 entries × 1 |

The upstream `BaseDynamicObject::weapon` at `0x118` corresponds to the object's
first child, not the unit's inventory selection field. Existing rendering code
uses it for the single attached weapon. Inventory integration must explicitly
resolve current slot plus inventory array, especially with attached grenades,
stowed weapons or future dual wielding.

**Retail magazine stride is 12 bytes, not Custom Edition's 16 bytes.** The
supplied binary indexes magazine state at weapon `+0x2B0 + index*12`:

| Relative offset | uint16 field |
|---|---|
| `0` | Reload state |
| `2` | Remaining ticks |
| `4` | Total ticks |
| `6` | Reserve rounds |
| `8` | Loaded rounds |
| `10` | Not established here |

The retail weapon tag magazine-definition block pointer is at `+0x4F4`;
definitions have `0x70` stride, capacity at `+0xA`, and rounds-per-reload at
`+0x18`. Reload end has both ammo transfer and repeat-reload logic. Intercepting
only its animation would not implement manual reloading: both the automatic
transfer and any subsequent reload iteration must be accounted for.

For grenades, save the attached projectile datum BEFORE calling the original
throw function; it sets unit `+0x294` to `0xFFFFFFFF` and grenade state to `3`.
Do not read the projectile ID after the call. Velocity units and creation/fuse
timing still need tracing before physical grenade release can be integrated.

## Reference cross-check

OpenSauce (`https://github.com/HaloMods/OpenSauce`) helped identify candidate
routine names and ABIs in its Halo1 engine-function wrappers. All addresses and
retail strides above were checked against the supplied binary, rather than
copied from its Custom Edition address table. No OpenSauce engine-wrapper code
has been incorporated into this checkpoint. Its different magazine layout is
one reason to keep retail and Custom Edition profiles separate.

## Next engine integration gates

1. Use validated full object datums and post-call inventory snapshots. Decline
   unknown editions and incompatible in-memory patches, with one clear log line.
2. Separate holster home slots from native weapon-array indices and from the
   current firing hand. A drawn gun reserves its home; a dropped gun frees it.
3. Establish native prompt/pickup admission, including full inventory and a
   temporarily held third weapon. Do not use automatic replacement as grip pickup.
4. Distinguish failed/inactive OpenVR input from an intentional grip release.
   Suspend/rebaseline on pause, tracking loss, loading, death and vehicle entry.
5. Connect selection/drop only after those transaction boundaries are in place;
   verify world mesh, ammo, collision and checkpoint transitions in a level.
6. Keep dual viewmodels, manual reloads, grenade manipulation and vehicles marked
   unimplemented until their actual engine and rendering paths are connected.

No runtime behavior, Windows compilation or headset result is claimed here.

## R1 adapter choices

The runtime guard checks the PE identity and seven exact instruction-window
fingerprints, including the game-engine pointer assignment. The on-disk verifier
also checks all ten SHA-256 windows, the complete executable hash, and agreement
between the JSON profile and compiled fingerprint table. The three existing
reload/grenade hook prefixes are not runtime-fingerprinted because upstream
already hooks them; R1 does not call them directly.

The campaign check reads the dispatch pointer at `0x6F1D20`, selected from the
game-engine table at `0x688308`. A null entry plus a valid live local unparented
biped is required. Inventory fields are read from copied bounded structures,
with object generation, type, size, slot and duplicate checks. A campaign
inventory exceeding two weapons falls back to the original controls.

After successful native removal, the adapter asks the verified position routine
to place the same world object at the controller, within a three-metre sanity
bound. It validates collision before accepting the pose. Controller velocity
is clamped to eight metres/second, converted to world displacement per 30 Hz
simulation tick, and added to the player's native velocity. It does not create
substitute world objects or restore an item the native engine deleted.

Holster and hand ownership commit only after native result checks. Homes are
reconciled on unit/storage changes, backwards object age, or native inventory
changes. Native pickups currently reset homes with the selected gun first.
Stock item pickup and automatic reload remain active in R1. No claim is made
that the model's grenade, manual reload or dual-wield rules are integrated.


## R1.1 findings from the first playtest

The supplied R1 log confirmed the retail profile loaded, but contained a
physical-input-unavailable warning and no initialized/drawn/stowed/dropped
events. This supports a failed activation, not a successful native holster test.
The warning's timing alone does not prove which controller action stayed absent.
The video frame review showed multiplayer testing; the separate text report
also describes failure with the second campaign mission's starting rifle.

A confirmed input-adapter bug checked optional actions before campaign context
and returned "managed" on failure. That could suppress stock controls even in
multiplayer and before any holster session existed. R1.1 checks context first,
preserves stock input before activation, and logs the exact inactive reason.
Already-owned inventory still suspends on lost input instead of dropping.
Fixed-hand aliases use the original SwapWeaponHand/Fire actions when optional
physical actions are inactive. A valid released primary never falls through to
a held alias; changing source suspends every read in that input sample.

Retail's script readers at 0x47F390/0x47F3C0/0x47F3F0/0x47F420 read the first
word behind pointer 0x6B145C for up/down/left/right bits 0x80/0x100/0x200/0x400.
The combined reader at 0x47F450 requires mask 0x780. Native input-history writes
at 0x4727EF..0x472852 use directional input values, whereas HaloCEVR's camera
hook supplies HMD yaw/pitch separately. Updating the render/view direction alone
is therefore insufficient evidence that these script look tests were satisfied.

The bridge ORs only directions measured from valid HMD movement, with a fresh
baseline after invalid tracking, long sample gaps, vehicle use, player changes
or detected native resets. It removes virtual turn yaw before measuring motion.
It preserves unrelated flags and never overrides a script return or control lock.
Three-degree strokes reject alternating subdegree jitter; pose jumps rebaseline.
These script/read-write windows now have runtime fingerprints. R1.1 checks nine
runtime windows and twelve on-disk windows in total. Actual cryo progression
still needs a headset check; this is not claimed as an observed runtime fix.

The player table's +0x30 field was rechecked rather than changed based on a
reference header: retail allocation increments it at 0x4D04DF, free decrements
it at 0x4D0577, and reset clears it at 0x4D058E. It IS an allocation count in
this retail binary. The nonzero guard remains. Player datum salt must also be
nonzero, matching native datum validation at 0x4D06A5..0x4D06B5.

The expanded test suite covers production OpenVR routing, input ownership
fallback, hip-first stow, native history writes and the standalone head-motion
model. Engine, memory and VR doubles do not replace native/headset validation.


## R1.2 world pickups and persistent homes

R1.1's log and sampled campaign video show working grip draws/stows, followed
by repeated `initialized` events after drops/acquisition. `WeaponSession` treated
any change to the native four-entry inventory array as a new session and rebuilt
homes around the selected gun. R1.2 instead preserves every surviving object's
home/hand, forgets only removed IDs, seeds new scripted grants into free homes,
and restores the existing held selection before committing the reconciliation.
Checkpoint/unit/storage resets still start a new session. Releases on the same
frame as a scripted grant are processed, not swallowed by reconciliation.

The narrow world contact handler at `0x4788A0` is cdecl(player datum, item datum),
called at `0x4784B7` with item then player pushed. The unmodified routine checks
item parent `+0x11C`, recent owner `+0x200`, scavenges same-type ammo, handles
equipment and may automatically add a weapon via `0x56D400` at `0x478B5B`.
R1.2 detours ONLY this contact handler, not the generic inventory addition
function. Direct script grants are unaffected. Script-spawned floor objects
that rely on contact acquisition intentionally require grip now.

Only the active physical session's verified local campaign player and a new,
loose, pickable weapon are intercepted. Other players/modes, equipment,
same-tag ammo scavenging and native recent-owner cooldown retain the original
contact handler. The new handler offers Halo's actual interaction with
`0x478E00`: EAX player, EBX object, stack action 7 (pickup) or 6 (swap/full), then
seat -1; caller cleanup 8. The routine writes player +0x24 target, +0x28 category,
+0x2A seat after comparing interaction priority/distance. The wrapper preserves
EBX. This is needed because native swap admission at `0x56DAE0` rejects an
unarmed selection, even when the unit has two physically stowed guns.

`PickupTarget` validates the live player and native offered datum/category on
each sample. A fresh empty-hand grip requests native `0x56D400` with EAX weapon,
ECX unit, stack mode 1, caller cleanup 4. It checks weapon flags +0x10 bit 0x800,
parent, native usability and free slot. Mode 2 would drop other inventory and
is never used. The adapter limits acquisition to fewer than two owned weapons,
re-reads the inventory, preserves all prior IDs, then selects the acquired gun.
A failed selection after a committed add is reconciled and stowed next frame;
the code never fabricates an undo object or duplicates ammo.

`0x56DA00` uses ESI unit / EDI weapon and returns AL. This native usability check
is called before offering a pickup. Its body is fingerprinted, as are inventory
addition and interaction offering. The entire contact routine is validated once
before hooking; it is deliberately excluded from subsequent in-memory hashes
because MinHook changes its entry. Twelve unchanged windows are checked at
runtime, with a thirteenth checked before detouring; sixteen complete/on-disk
windows are verified by the offline tool. No guessed address scanning fallback.

The contact hook is enabled for acquisition only while a physical session is
active. Unsupported contexts and disabled physical settings retain native
pickup. New held grips after a tracking/input gap use the existing baseline
rules. Native weapon swap/interact controls are cleared when a loose weapon is
offered; unrelated interaction prompts keep their existing input.

The production pickup reader/handler/transaction code is exercised against
synthetic object/player memory, with native call doubles. Added cases cover
both fresh grip hands, walking/held grip without pickup, unchanged surviving
homes, empty/full inventory, partial add, scripted arrival during release,
wrong/stale offered IDs, same-tag ammo and equipment passthrough. The x86 calls,
MinHook trampoline and real game rendering still require the user's Windows
compile and headset validation; no runtime success is inferred from fixtures.
