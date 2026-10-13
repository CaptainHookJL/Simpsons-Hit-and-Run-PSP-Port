# Original CompactA vehicle

Original asset data: The Simpsons: Hit & Run, Radical Entertainment.
Source: user-supplied Svxy/The-Simpsons-Hit-and-Run checkout at
`0cf35df0a921a803a69d0da4c2aca02aa2543f03`, file
`game/build/art/WORK/EL/PC/l01/l1_parkedA_Shape.p3d`.

The source names its materials `CompactAChassisNorm_m` and `WindsheildT_m`.
This is the original CompactA parked-car visual mesh, not Homer's family sedan.
The rigid mesh contains 310 triangles and two original textures, stored in
`compact.shm` and `compact.stx`. Texture pixel memory is 41,216 bytes including
the opaque fallback. See `compact.json` for source/generated SHA256 values.

The importer centers X/Z, moves the lowest vertex to Y=0, and uniformly scales
the model to fit the existing 1.8-by-3.6-metre collision footprint. Actual visual
dimensions are about 1.74 m wide, 1.42 m tall, and 3.6 m long. UVs are retained
and the texture sidecar CRC binds to the normalized mesh. Source +Z is used as
the vehicle's forward direction, consistent with the original vehicle code;
the user subsequently reported vehicle build `e45c6e0` "works perfectly" on the
PSP-3000 with ARK 5.

The player model renders with the car position and yaw through the same texture
draw path as scenery. It stays within the chase camera's view, so it does not
change cached static-world bounds. Native checks cover 24 camera/yaw orientations
and 864 combinations with steering and tyre rotation. Steering,
collision, and terrain-following use the existing prototype controller. Original
authored vehicle collision, suspension, occupants, and damage are not imported.

`compact.swh` identifies the four disconnected tyre components after welding
positions, leaving 166 body triangles and 36 triangles per tyre (310 total).
It stores triangle membership, geometry-derived axle pivots and approximately
0.3344-metre radii. Its CRC binds to `compact.shm`; source geometry, colours,
UVs and texture assignments are retained. These pivots are inferred from the
parked mesh, not read from an original drivable-car skeleton.

The front tyres turn with the original smoothed steering controller. Each tyre
rolls according to its actual pivot displacement after collision projected onto
its facing, divided by radius. Reverse rolls backwards, turning arcs differ
between inner/outer wheels, and a blocked car stops rolling. Start resets rotation.
The rendering order is body yaw, local pivot, front steering, then axle rotation.
Angles stay bounded and part buffers allocate only at startup (about 22 KiB),
sharing the original textures. The car uses six draw batches instead of two.
Missing/invalid wheel metadata retains the rigid-car fallback. The HUD shows
`Wheel animation ON` on success. This is visual animation without suspension,
burnout/tyre-slip simulation, damage or a full vehicle hierarchy. PSP performance
for this change has not yet been measured.

Regenerate from the source checkout with:

```sh
python3 tools/import_vehicle.py \
  /workspace/shar-source/game/build/art/WORK/EL/PC/l01/l1_parkedA_Shape.p3d \
  --output-dir assets --prefix compact --split-wheels
```

The mesh/texture files retain the hashes from the successful rigid-car build.
`tools/split_wheels.py` can also generate the wheel sidecar from `compact.shm`.
The CompactA-specific splitter requires one separate tyre at each wheel position.
Only rigid root/static meshes and embedded PNG textures are supported. Skinning
and vehicle-part hierarchies require further importer work. The separately
linked MEGA archive is not needed for this asset; a fresh archive download URL
still returned HTTP 403 from its storage server during this milestone.
