# R1.5 visual lifecycle investigation

The user supplied retail Halo PC 01.00.10.0621 executable remains the binary
reference. No executable, map, model, user log or video is distributed here.

R1.4 real-game evidence: 4,293 `[PhysicalVisuals] first-person refresh` lines;
2,514 AR-to-None, 1,332 pistol-to-None, 442 plasma-pistol-to-None, five None-to-None.
The prior test only checked an isolated refresh; it did not interleave Halo's
subsequent selection/update. R1.5 models physical stowing without repeatedly
changing native unit slots to -1. Native selection remains authoritative for
inventory mutations. Public physical reads mask the selected gun as None only
for the validated stowed unit, storage and non-rewound age.

Retail observations used by this change:

| Address | Verified behavior / ABI |
| --- | --- |
| 0x4923d0 | Tick update rebuilds first-person state if weapon datum is None. |
| 0x492430 | No-argument pose/visibility update entry. Calls visibility before posing. Existing vehicle patch changes +0x55; profile covers the preceding instructions. |
| 0x493e50 | DI=local player, stack +4=visibility bool, caller cleanup. Native teardown on visible-to-hidden transition. Entry itself is not detoured. |
| 0x4b1e20 | cdecl(player pointer), weapon HUD pass; skip while physically stowed. Unit HUD at 0x4b0320 remains untouched. |
| 0x4924b0 | Native first-person draw remaps and submits gun and hands separately. |
| 0x4d6fc0 | EAX=model tag, ECX=node matrices, eleven stack arguments, caller cleanup. Node count/data at model +0xb8/+0xbc; node stride 0x9c, inverse bind matrix +0x68. |
| 0x50ea00 | ESI=object datum; float distance stack +4; returns object lighting pointer. |

Native body models use the same eleven-argument submission as the hands call
at 0x4926d5. Materials come from the map, transforms from validated inverse-bind
matrices and current controller/body poses. Model submission never selects a
gun, modifies tags or creates a world object. Chief's hand tag is read from
loaded globals' first-person-interface reference; hip model from weapon +0x34.
Arm nodes collapse at their wrist while wrist descendants retain mesh shape.

Chimera's reference interpolation copies 128 quaternion nodes at FP +0x8c; it
does not own weapon IDs. Its model-detail fix patches 0x4d70da onward in this
executable, inside the native submission routine. The render entry fingerprint
stops before that LOD region. Do not infer that Chimera caused the stow loop.
Its source was inspected for compatibility, not copied into this project.

Tests exercise actual production read/select/synchronize/pose/HUD-guard and
body-model adapter methods with synthetic memory and native-call doubles. The
300-frame stow regression requires zero rebuilds and no weapon HUD or pose
reactivation. Actual generic HUD anchor routing verifies corner health graphics
remain allowed. Geometry tests reject missing wrists, cyclic/out-of-range node
parents, nonfinite or singular matrices; test independent hands and muzzle-down
placement. Native-call ABI and on-disk fingerprints were inspected, but neither
MSVC compilation nor live game rendering has been performed here.
