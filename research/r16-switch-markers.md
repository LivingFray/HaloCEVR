# R1.6 — selection identity, pose readiness and both home indicators

## Evidence and limits

User's R1.5 reproduction: keep one gun stowed, grip-pick a second, stow it,
draw the first. Only dropping and grip-picking the first clears the corruption.
The supplied `inject(7).log` has the expected inventory event sequence, native
body renderer profile=1, and no `selected model` refresh messages. It does not
contain animation-node dumps or establish which renderer caused corruption.

The previous early return in `SynchronizeViewModel` accepted an equal object ID
as sufficient readiness. R1.6 does not make that assumption across physical
selection. A successful draw marks a pending refresh, including drawing the
same native ID after virtual stow. Selection is finished before the refresh;
`OnPose` invokes the verified native 0x493c60 before the original pose routine.
This rebuilds hand/weapon node maps, animation selection and native blending
state. It never drops/re-adds objects or rewrites inventory to repair visuals.
Rendering stays gated if refresh fails. Stable equipped/stowed frames do not
repeat refresh. This is a targeted repair requiring a real-game retest.

## World model visibility

The retail 0x4d6fc0 model renderer compares the float detail parameter against
the model disappearance cutoff at +8 (0x4d7003–0x4d7014); it can return before
submission. Native first-person models use 0. Passing 0 to general world assets
is not equivalent. R1.6 passes FLT_MAX for indicators, retaining 0 for hands.
The lack of a shoulder display in R1.5 was a separate cause of missing indicators.
Both homes now use SlotOwner, not only stowed hip items. There is no type whitelist.

Long guns: 0.30 scale. Pistol/plasma pistol/Needler: 0.65. Shoulder indicator:
additional 0.72 multiplier. All use world models and bind-pose matrices; the held
weapon is unaffected. Shoulder center-front waist position is visual only.

## Grey silhouette scope

Retail model submission with flags=8 flushes via 0x52b530 before returning at
0x4d7288. A scoped marker flag on the existing D3D9 wrapper surrounds this call.
Individual draw calls bind a ps_2_0 constant-grey shader, temporarily disable
blend, alpha test and fog, and restore the real device's prior shader/state.
The game's state cache and loaded material assets are not rewritten. Failed
shader creation leaves normal marker materials and logs its HRESULT. Shader
resources are released on device reset/release. No extra compiler DLL is needed.

Shader token reference: Windows SDK d3d9types.h; Microsoft ps_2_0 registers:
https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-ps-2-0
The constant output uses no input semantics, textures or external shader constants.

## Verification boundaries

Portable production-method fixtures verify refresh sequencing, including equal
IDs with a simulated stale remap, both marker submissions, grip anchors and
native inventory preservation. Device doubles verify grey shader state is scoped,
also on failed state reads/bind and with a previously null pixel shader. All
stock weapon name categories and mirrored/center positions are covered.
The simulated native refresh does not prove Halo/Chimera interoperability.
Actual x86 inline assembly, GPU appearance and gameplay require the user's build.
