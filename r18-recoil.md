# R1.8: shot-driven physical recoil

R1.7 was confirmed working by the user before this change. No selection,
inventory transaction, pose-boundary reset or pickup hook is changed here.

## Retail firing observation

The supplied retail 1.10 executable has SHA256
`b84131f2ee8cbf8d0dbda71d1d920059318bc55b36760dce32735026ced099a8`.
The existing upstream FireWeapon hook is at VA `0x4c3f10` (cdecl weapon datum,
short trigger). A call alone is not evidence of a shot. Interior disassembly:

- `0x4c400d..0x4c40a5`: magazine index has stride **12**, base weapon+0x2b0,
  loaded rounds at magazine+8. The retail stride differs from CE's 16 bytes.
- `0x4c4328`: tests the local successful-fire flag and skips that branch when
  false. `0x4c436f..0x4c4372` copies the game tick (time globals+0xc) to
  weapon+0x2d0 on that branch.
- `0x4c4444..0x4c44e4`: firing adds heat at weapon+0x23c and, for applicable
  energy weapons, energy age at weapon+0x240. Infinite ammo/battery options may
  suppress consumption, but not the successful-fire tick.

Those three interior blocks have independent startup fingerprints. A mismatch
disables only new recoil observations; the accepted physical inventory remains.
The hook entry is already detoured and is not fingerprinted after installation.
ReadFireState uses bounded process reads, object type/size, full salted datum,
live local campaign biped, current selection and attached-parent validation.

Before/after firing observations must refer to the same unit and weapon. A changed
success tick, decreased loaded rounds, increased heat or increased energy age
adds one impulse for that hook event. Resource changes cover another event in
the same tick. No change means no new impulse. This observes native successful
firing, not projectile count; charged/alternate/native misfire behavior still
belongs to the game. No engine values are overwritten by recoil observation.

## Transform and timing

Recoil is added after firing so the first bullet follows the pre-shot aim.
The central dominant-hand transform adds local negative-Y pitch (+X muzzle
rises toward +Z) and negative-X translation. Weapon bones, reticle, scope and
relocated native bullet aim share it. No camera/HMD rotation is added.

The inverse kick is removed from gun/hand poses used for alignment caching.
That keeps temporary recoil out of cached fire offsets and two-handed alignment.
Grip/support capture uses the original, unrecoiled hand transform. A production
fixture checks cache correction with rotated hands and weapon-local offsets.

Recoil.Context keys state by complete unit and weapon handles. Context loss
resets it. It advances on a frame boundary and firing events using a steady clock
with exact exponential decay. Render/aim queries do not mutate it, so extra eye
or scope passes do not multiply kick or accelerate recovery. Impulses accumulate
at native fire cadence, with bounded sustained travel. Support comes from the
physical session's actual shared-gun ownership.

The supported fraction defaults to 0.35. Per-weapon pitch, backward travel and
recovery constants are in WeaponRecoilProfile. Global, supported fraction and
per-type scale settings use the existing config system. Changing support affects
future impulses; an earlier unsupported impulse settles normally.

## Verification

Portable tests compile production fire methods, transforms, cache correction,
input support query and retail reader against engine doubles. They cover every
profile, support ratio, dry firing, energy firing, actual next-shot aim, context
rejection, tuning, unchanged grip geometry, time-step invariance, bounded auto
buildup and no render-query advancement. Existing R1.7 inventory/selection/visual
tests pass. The verifier checks all 26 recorded retail blocks, including the
three added firing observations.

Linux fixtures are not an MSVC DLL build, native hook execution or headset test.
Those remain required to validate the feel and appearance of this source release.
