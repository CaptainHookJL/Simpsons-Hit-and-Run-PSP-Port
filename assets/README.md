# Springfield geometry sample

Original asset data: The Simpsons: Hit & Run, Radical Entertainment.
Converted from the user-supplied Svxy/The-Simpsons-Hit-and-Run checkout,
commit `0cf35df0a921a803a69d0da4c2aca02aa2543f03`:

- `game/build/art/WORK/STORE/PC/levels/L01B/z1/statics/z1_statics.p3d`
- `game/build/art/WORK/STORE/PC/levels/L01B/z1/track/z1_track.p3d`

`springfield.shm` contains 6,437 triangles in 142 static meshes selected within
45 metres along X and Z of source position `(225, 3.27, -157)`, near the Simpsons
house. Selection includes triangles intersecting that square, so triangles can
extend beyond it. Coordinates are translated to put that position at the origin.
The geometry file does not contain textures, skinning, instancing, authored collision volumes, or animated geometry.
The runtime now builds approximate ground/wall collision from these render triangles.
Source vertex colours are retained. Textures and their UVs are loaded separately
from `springfield.stx`; an absent or invalid sidecar leaves plain geometry visible.
Texture alpha uses one-bit cutouts; translucent glass is not faithfully reproduced.

Rebuild from the reference source with:

```sh
python3 tools/p3d_mesh.py \
  /workspace/shar-source/game/build/art/WORK/STORE/PC/levels/L01B/z1/statics/z1_statics.p3d \
  /workspace/shar-source/game/build/art/WORK/STORE/PC/levels/L01B/z1/track/z1_track.p3d \
  --center 225 3.27 -157 --radius 45 --output assets/springfield.shm
```

The 308,992-byte file starts with `SHM1`, uint32 version 1, uint32 vertex count,
and a reserved uint32 zero, followed by 16-byte vertices: uint32 PSP colour and
float32 X/Y/Z. All fields are little-endian. The runtime rejects malformed files
and caps vertex memory at 768 KiB.

## Original textures

`springfield.stx` contains UVs, 142 draw batches, and 47 original textures from
the same source files. Textures are limited to 128 pixels per dimension and
converted to PSP RGB5551 with one-bit alpha, in swizzled 16-byte-by-eight-row
blocks. Pixel storage totals 530,688 bytes, including an opaque white fallback.
UV values are kept as exported; hardware validation will confirm orientation.
Draw batches are sorted by texture to reduce state changes. The geometry and
collision file is unchanged from the hardware-tested collision build.

Regenerate textures using Pillow 12.3.0 (provided by the cloud runtime, or install
`requirements-tools.txt` into a virtual environment):

```sh
python3 tools/p3d_texture.py \
  /workspace/shar-source/game/build/art/WORK/STORE/PC/levels/L01B/z1/statics/z1_statics.p3d \
  /workspace/shar-source/game/build/art/WORK/STORE/PC/levels/L01B/z1/track/z1_track.p3d \
  --center 225 3.27 -157 --radius 45 \
  --mesh assets/springfield.shm --output assets/springfield.stx
```

The sidecar binds to the mesh with a CRC32 of the mesh vertex bytes. Header:
`STX1`, uint32 version, mesh CRC, vertex count, texture count, batch count.
Then come float32 UV pairs per vertex, uint32 first/count/texture per batch,
and uint32 width/height/byte count followed by RGB5551 pixels per texture.
All fields are little-endian. Loader limits are 128 textures, 1,024 batches,
and 2 MiB of pixels; invalid counts, coverage, UVs, dimensions, or CRC are rejected.
