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
No textures, skinning, instancing, collision, or animated geometry are converted.
Source vertex colours are retained; texture-only surfaces can appear plain.
Alpha is forced opaque in this initial importer.

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
