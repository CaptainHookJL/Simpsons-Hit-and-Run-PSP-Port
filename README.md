# Hit & Run PSP development

This is an early replacement-engine driving and on-foot prototype for PSP, not a playable
port of the original game. It includes a small original Springfield geometry
sample and the original CompactA car model; original middleware is not bundled.
It now includes the original game's speed-dependent, smoothed steering controller,
adapted to standalone C and linked into the PSP executable. See [source reuse
details](SOURCE_REUSE.md) for attribution, changes, and the boundaries of this reuse.

## Download for PSP

[Download the installation ZIP](https://github.com/CaptainHookJL/Simpsons-Hit-and-Run-PSP-Port/raw/refs/heads/main/downloads/SHAR-PSP-Prototype.zip).
Extract it and copy the `PSP` folder to your Memory Stick's root, then launch
SHAR PSP from Game > Memory Stick. Requires custom firmware supporting unsigned
homebrew. The user confirmed optimized build `716b316` running at locked 60 FPS
on a PSP-3000 with ARK 5. It clips textured world triangles at camera planes,
preserving UVs and colours, with cached bounds and direct draws for fully visible
meshes. See [hardware validation](docs/HARDWARE_VALIDATION.md) for the tested
build and the limits of that evidence. The imported area remains a bounded sample.
The user also confirmed CompactA vehicle build `e45c6e0` "works perfectly" on
the same PSP; the locked-60-FPS measurement above refers to the earlier baseline.
The current build adds walking/running and car entry/exit with an animated
placeholder character. Its PSP FPS still needs hardware measurement.

## Build

Install [PSPDEV](https://github.com/pspdev/pspdev), then:

```sh
export PSPDEV=/workspace/toolchains/pspdev
export PATH="$PSPDEV/bin:$PATH"
cd /workspace/Simpsons-Hit-and-Run-PSP-Port
make -j2
```

Create an installation ZIP with:

```sh
python3 tools/package_psp.py /tmp/SHAR-PSP-Prototype.zip
```

Extract it and copy its `PSP` folder to the root of your PSP Memory Stick.
Launch it through Game > Memory Stick. It requires custom firmware that supports
unsigned homebrew. Alternatively open `EBOOT.PBP` in PPSSPP.
The executable is built as a relocatable PSP PRX inside the PBP package.
The target uses an 8 MiB heap and does not request the PSP Slim expanded memory.

Controls:

| Mode | Controls |
| --- | --- |
| Driving | Stick/D-pad steers; Cross accelerates; Square brakes; Circle reverses. |
| On foot | Stick/D-pad moves relative to the camera; hold Cross to run; L/R turns the camera. |
| Both | Triangle enters/exits CompactA; Start resets to driving at spawn; Home quits. |

Stop the car before getting out. Exit tries both sides and nearby side positions,
checking reachable ground, capsule clearance and the path from the car. Entry
requires being within three metres with a clear path. The car stays parked while
on foot and blocks walking through it. Ground following, capsule/wall collision,
wall sliding and map-edge stopping apply to the player. Animation follows actual
travel and stops when blocked. The HUD shows `ON FOOT` and
`Character: animated placeholder`. See [character status](assets/CHARACTER.md).

The installation ZIP includes textured scenery near the Simpsons house,
converted from the supplied source checkout, plus the original CompactA parked-car
mesh and two textures. All four original tyres rotate with actual movement, and
the front pair turns with the smoothed steering angle. See
[vehicle provenance and conversion](assets/VEHICLE.md). The HUD shows `Vehicle: CompactA`
when it loads successfully; missing/invalid vehicle assets fall back to boxes.
`Wheel animation ON` confirms the wheel metadata loaded. Missing/invalid metadata
keeps the original rigid car visible. Start resets both the car and wheel rotation.
The screen shows texture/collision state and measured FPS. If scenery fails to load,
it falls back to the original test ground. When scenery and the collision grid
load, the screen shows `Collision ON | terrain following ON`. The car follows
nearby ground heights and stops on contact with steep scenery surfaces. It also
stops before leaving the imported ground; the older test-ground boundary remains
in the fallback mode. Missions, NPC pedestrians, sound, and original
suspension/force physics are absent. If textures fail to load, the original
untextured geometry remains visible. Recheck the same driving route after rendering
or asset changes to confirm road continuity and the locked-60-FPS baseline.
See [asset provenance and conversion instructions](assets/README.md).

Collision uses two spheres to approximate the car and infers ground
versus obstacles from the rendered triangles. It does not use the original
game's authored collision volumes. Narrow gaps, shrubs, and decorative geometry
can therefore behave differently from the original game. The car stays upright:
there is no suspension, pitch/roll, jumping, sliding response, or damage system.
The camera follows the active car or player and does not collide with scenery yet.
The parked car uses a conservative visibility bound while on foot. Fully visible
views use direct drawing; triangles crossing camera planes use software clipping
with UVs retained. Its 153 KiB vertex scratch is allocated once, sharing the
original textures. Native checks cover 144 on-foot camera views. This avoids
reintroducing near-plane geometry loss when walking close to the car.

The HUD reports `Software clipping ON` when the new rendering path is active.
Its buffers are allocated once at startup (about 3.1 MiB maximum vertex scratch
for this sample); frames reuse them. Geometry outside the view is rejected,
while triangles crossing view planes are split instead of being dropped.
Bounds are computed once at startup. In 60 test views, the optimized path checks
an average of 4,659 of 19,311 vertices and generates/copies 2,490 vertices; another
4,081 are drawn directly. Its output is compared vertex-by-vertex against the
full clipper to verify the optimization retains visible geometry and UVs.

## Checks

```sh
gcc -std=c99 -Wall -Wextra -Werror tests/driving_test.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-driving-test
/tmp/shar-driving-test
python3 -m unittest discover -s tests -p 'test_*.py'
gcc -std=c99 -Wall -Wextra -Werror tests/mesh_test.c src/mesh.c -lm -o /tmp/shar-mesh-test
/tmp/shar-mesh-test assets/springfield.shm
gcc -std=c99 -Wall -Wextra -Werror tests/collision_test.c src/collision.c src/mesh.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-collision-test
/tmp/shar-collision-test assets/springfield.shm
gcc -std=c99 -Wall -Wextra -Werror tests/texture_test.c src/texture.c src/mesh.c -lm -o /tmp/shar-texture-test
/tmp/shar-texture-test assets/springfield.shm assets/springfield.stx
gcc -std=c99 -Wall -Wextra -Werror tests/clip_test.c src/clip.c src/texture.c src/mesh.c -lm -o /tmp/shar-clip-test
/tmp/shar-clip-test assets/springfield.shm assets/springfield.stx
gcc -std=c99 -Wall -Wextra -Werror tests/vehicle_test.c src/clip.c src/texture.c src/mesh.c -lm -o /tmp/shar-vehicle-test
/tmp/shar-vehicle-test assets/compact.shm assets/compact.stx
gcc -std=c99 -Wall -Wextra -Werror tests/wheels_test.c src/wheels.c src/clip.c src/texture.c src/mesh.c src/collision.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-wheels-test
/tmp/shar-wheels-test assets/compact.shm assets/compact.stx assets/compact.swh
gcc -std=c99 -Wall -Wextra -Werror tests/player_test.c src/player.c src/collision.c src/mesh.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-player-test
/tmp/shar-player-test assets/springfield.shm
gcc -std=c99 -Wall -Wextra -Werror tests/vehicle_view_test.c src/vehicle_view.c src/wheels.c src/clip.c src/texture.c src/mesh.c src/player.c src/collision.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-vehicle-view-test
/tmp/shar-vehicle-view-test assets/compact.shm assets/compact.stx assets/compact.swh
python3 tools/audit_source.py /workspace/shar-source
```

The driving test covers acceleration, braking without unintended reverse,
forward and reverse steering, boundary stopping, and zero elapsed time. The original-source
audit deliberately returns failure when required middleware directories are absent.
Wheel checks preserve the source geometry/materials, verify forward/reverse and
turning arcs, stop rotation at collision, reject damaged metadata, and check 864
camera/steering/roll combinations. The split tyres add no triangles or textures;
their static vertex buffers are allocated once and reuse the existing textures.
Player checks cover walk/run speed, diagonal normalization, original input response,
camera controls, wall sliding, capsule waist/ceiling contact, slopes, ground edges,
parked-car collision, blocked/clear entry and exit, and 1,200 ticks of exploration
in the original Springfield sample. Player and parked-car clipping checks also
pass with AddressSanitizer and UndefinedBehaviorSanitizer.

## Source assessment and next milestones

Reference source: Svxy/The-Simpsons-Hit-and-Run at
`0cf35df0a921a803a69d0da4c2aca02aa2543f03`, retained in `/workspace/shar-source`.
The `game/code/main/platform.h` interface depends directly on RADCore drive
callbacks and Pure3D types; the PS2 entry point also calls PS2 graphics APIs.
Sharing a MIPS CPU family does not make the PS2 backend compatible with PSP.
The Windows project names 12 middleware source directories absent from the GitHub
archive. Its README points to a separate archive for additional build files.

Next milestones:

1. Download/inventory the supplied additional archive when its storage server is accessible.
2. Test driving/on-foot transitions and FPS on PSP. Obtain the missing original
   Homer character/animation files and add skinning/animation import support.
3. Add original vehicle hierarchies, improve collision/camera behavior, then
   implement streaming, audio, and mission execution in
   separately testable stages; reuse compatible original logic where practical.

The MEGA API lists one 3,399,653,232-byte archive named
`The Simpsons Hit & Run - PC.zip` under a compiled-PC folder. Archive contents
have not been inspected. Its storage URLs have used
`gfs214n104.userstorage.mega.co.nz` and, on a fresh request,
`gfs270n356.userstorage.mega.co.nz`. The fresh server returned HTTP 403; required
network entries are saved alongside `mega.nz` and `g.api.mega.co.nz`.
Do not assume the archive contains missing middleware until contents are verified.
