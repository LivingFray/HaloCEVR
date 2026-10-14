# R1.7: persistent selection and belt displays

## Evidence

The supplied R1.6.1 `inject(8).log` records 1,072 pose-boundary model resets:
729 for datum 3879469533, 338 for 3850764645 and 5 for 3955294807. Many occur
without intervening physical draw/stow events, and report equal old/new FP IDs.
The log alone cannot prove the complete cause, but shows the prior claim of
stable-frame reset avoidance did not hold in-game. Do not treat R1.6's visual
refresh as a confirmed repair.

The local retail executable (hash in retail-1.10-profile.json) supplies the
missing link. Player-control globals are pointed to by 0x6b145c. Player 0's
control begins at +0x10; its full unit datum is +0x10 and persistent weapon slot
is a signed short at +0x30 relative to the globals base.

- 0x470e80 binds a new control owner, initializes the slot, then copies the
  unit's desired selection into control+0x20 at 0x470f43–0x470f4d.
- 0x471ae0 computes the same local control address. At 0x471c29 it retains a
  valid control weapon slot. Only a missing/invalid slot falls back to the unit's
  desired slot (0x471c47–0x471c52).
- At 0x471ed3 that persistent slot goes into the generated player action.
- 0x563a69–0x563a73 writes unit+0x2f4 from a nonnegative unit-control selection.
- The engine's setter at 0x472100 resolves unit owner -> local player and writes
  globals+local*0x40+0x30 at 0x47214d. Its complete bytes through RET at 0x472152
  are now fingerprinted. We use checked stores to the same field, not a new hook.
- Unit+0x478's cached control copy is used for the multiplayer branch at
  0x563a13. It is not changed by this campaign-only repair.

R1.6.1 Select wrote unit+0x2f4 and called ready(0x56d6e0), but never wrote the
persistent player command. The next game tick could undo that selection and
physical-session reconciliation would select it again. That explains repeated
model refreshes and is consistent with dropping/re-picking clearing the problem.
Headset testing is still required to determine whether another renderer issue
also exists.

## Repair and scope

For a real draw/pickup, validate the existing executable fingerprints, owned
inventory, full control-owner datum and old command bounds; then set control
slot and unit desired slot before native ready. A failed transaction restores
both choices, with owner/pointer revalidation before restoring the command.
No grenade, zoom, facing, movement or action flags are rewritten. No inventory
objects are dropped/re-added as a visual workaround. Virtual stowing remains;
real draw transitions retain the guarded pose-boundary remap refresh.

The portable production-method fixture now includes native control ticks. With
the R1.6.1 Select method substituted, it fails at `out.current==id` on the first
tick after drawing the other valid slot. With R1.7 it holds each requested gun
for 120 simulated frames, across eight draws, with one refresh per draw and no
extra native ready calls. It also checks control-owner/slot/pointer rejection,
rollback and adjacent input-field preservation. These are engine doubles.

## Display changes

Shoulder indicator forward offset: 0.25 -> 0.21 metres from head yaw.
Shoulder scale multiplier: 0.72 -> 0.60 (17% reduction). Base hip scales remain
0.30 for long guns and 0.65 for pistol/plasma pistol/Needler. Rotate the shoulder
indicator in the display plane by 90 degrees, so its +X barrel axis runs across
the belt. Actual shoulder and hip grab anchors are unchanged.

Queue labels once in PreDrawFrame, after pose/recenter updates; draw them in
each eye using the existing font atlas. Existing Render state-block restoration
contains the fixed-function text pass. There are no new textures or runtime font
allocations. A dark framed EMPTY rectangle appears only for unreserved homes.
The saved handedness preference controls labels and hip mirroring; active firing
hand does not. Normal indicators and equipped grey silhouettes still use native
models from the loaded map. No native tag/material edits or game assets are shipped.

Portable checks cover queue lifecycle, empty/reserved state, labels, placement,
glyph geometry and required draw state. Actual x86 compilation, Halo timing,
Chimera interaction and headset readability have not been tested here.
