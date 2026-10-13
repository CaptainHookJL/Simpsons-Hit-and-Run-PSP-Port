# Hit & Run PSP development

This is an early replacement-engine driving prototype for PSP, not a playable
port of the original game. It includes a small original Springfield geometry
sample; original middleware is not bundled.
It now includes the original game's speed-dependent, smoothed steering controller,
adapted to standalone C and linked into the PSP executable. See [source reuse
details](SOURCE_REUSE.md) for attribution, changes, and the boundaries of this reuse.

## Download for PSP

[Download the installation ZIP](https://github.com/CaptainHookJL/Simpsons-Hit-and-Run-PSP-Port/raw/refs/heads/main/downloads/SHAR-PSP-Prototype.zip).
Extract it and copy the `PSP` folder to your Memory Stick's root, then launch
SHAR PSP from Game > Memory Stick. Requires custom firmware supporting unsigned
homebrew. This is a driving prototype; physical PSP execution is not yet verified.

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

The installation ZIP includes untextured scenery near the Simpsons house,
converted from the supplied source checkout. The car is still placeholder geometry.
The screen shows scene load status and measured FPS. If scenery fails to load,
it falls back to the original test ground. The car stays on a flat plane and can
pass through scenery; terrain-following and collision are not implemented.
There is a boundary at 110 metres along either ground axis, but the scenery sample
is smaller. Missions, pedestrian gameplay, sound, and original suspension/force
physics are absent. Graphics and controls still require PPSSPP/hardware validation.
See [asset provenance and conversion instructions](assets/README.md).

## Checks

```sh
gcc -std=c99 -Wall -Wextra -Werror tests/driving_test.c src/driving.c src/upstream/vehicle_steering.c -lm -o /tmp/shar-driving-test
/tmp/shar-driving-test
python3 -m unittest discover -s tests -p 'test_*.py'
gcc -std=c99 -Wall -Wextra -Werror tests/mesh_test.c src/mesh.c -lm -o /tmp/shar-mesh-test
/tmp/shar-mesh-test assets/springfield.shm
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

1. Validate this EBOOT on PPSSPP and PSP, including steering direction and exits.
2. Download/inventory the supplied additional archive when its storage server is accessible.
3. Extend the static Pure3D importer with textures and vehicle hierarchy support,
   then replace the placeholder car with one original vehicle.
4. Add terrain-following and scenery collision, then on-foot movement, streaming,
   audio, and mission execution in
   separately testable stages; reuse compatible original logic where practical.

The MEGA API lists one 3,399,653,232-byte archive named
`The Simpsons Hit & Run - PC.zip` under a compiled-PC folder. Archive contents
have not been inspected: its current download server,
`gfs214n104.userstorage.mega.co.nz`, returns HTTP 403. Its required network entry
was saved in environment settings alongside `mega.nz` and `g.api.mega.co.nz`.
Do not assume the archive contains missing middleware until contents are verified.
