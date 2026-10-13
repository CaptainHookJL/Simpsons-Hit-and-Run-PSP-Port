# Hit & Run PSP development

This is an early replacement-engine driving prototype for PSP, not a playable
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
The new CompactA vehicle build still needs a hardware appearance and FPS check.

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

Controls: analog stick or D-pad steers, Cross accelerates, Square brakes,
Circle reverses, Start resets, and Home exits on PSP.

The installation ZIP includes textured scenery near the Simpsons house,
converted from the supplied source checkout, plus the original CompactA parked-car
mesh and two textures. Its rigid wheels are not animated yet. See
[vehicle provenance and conversion](assets/VEHICLE.md). The HUD shows `Vehicle: CompactA`
when it loads successfully; missing/invalid vehicle assets fall back to boxes.
The screen shows texture/collision state and measured FPS. If scenery fails to load,
it falls back to the original test ground. When scenery and the collision grid
load, the screen shows `Collision ON | terrain following ON`. The car follows
nearby ground heights and stops on contact with steep scenery surfaces. It also
stops before leaving the imported ground; the older test-ground boundary remains
in the fallback mode. Missions, pedestrian gameplay, sound, and original
suspension/force physics are absent. If textures fail to load, the original
untextured geometry remains visible. Recheck the same driving route after rendering
or asset changes to confirm road continuity and the locked-60-FPS baseline.
See [asset provenance and conversion instructions](assets/README.md).

Collision uses two spheres to approximate the car and infers ground
versus obstacles from the rendered triangles. It does not use the original
game's authored collision volumes. Narrow gaps, shrubs, and decorative geometry
can therefore behave differently from the original game. The car stays upright:
there is no suspension, pitch/roll, jumping, sliding response, or damage system.
The chase camera does not collide with scenery yet.

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
python3 tools/audit_source.py /workspace/shar-source
```

The driving test covers acceleration, braking without unintended reverse,
forward and reverse steering, boundary stopping, and zero elapsed time. The original-source
audit deliberately returns failure when required middleware directories are absent.

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
2. Add vehicle hierarchy and wheel animation support, and import a complete
   drivable vehicle asset when available.
3. Refine collision, then add on-foot movement, streaming,
   audio, and mission execution in
   separately testable stages; reuse compatible original logic where practical.

The MEGA API lists one 3,399,653,232-byte archive named
`The Simpsons Hit & Run - PC.zip` under a compiled-PC folder. Archive contents
have not been inspected. Its storage URLs have used
`gfs214n104.userstorage.mega.co.nz` and, on a fresh request,
`gfs270n356.userstorage.mega.co.nz`. The fresh server returned HTTP 403; required
network entries are saved alongside `mega.nz` and `g.api.mega.co.nz`.
Do not assume the archive contains missing middleware until contents are verified.
