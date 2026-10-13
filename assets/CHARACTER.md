# On-foot character status

The current character is a new procedural placeholder drawn from six boxes.
It is not an imported Homer model. Arms and legs swing according to actual
ground travel, with a larger swing while running; idle/blocked movement stops
the swing. No original animation or character texture is included.

The on-foot controller uses a 0.35-metre-radius, 1.8-metre-high upright capsule
against the same scenery grid as the car, with ground-height following and
wall sliding. Capsule tests cover its entire vertical axis and triangle faces/
edges, including waist-height obstacles and low overhead surfaces. Ground
probes keep its footprint within the imported area. Walk/run limits are 2.5/5.0
m/s. The original character input-response curve is adapted as documented in
`SOURCE_REUSE.md`; original character physics and puppet code are not ported.

Triangle exits only below 0.5 m/s and checks both sides plus nearby side
positions. A candidate must have reachable ground, capsule clearance, no car
overlap and a clear path from the car. Entry requires a clear path and proximity
within three metres, with similar height. The car remains parked and solid while
on foot. Start returns to driving at the original spawn. There is no door/seat
animation, jumping, kicking, NPC traffic, missions or character audio yet.

## Original asset blocker

All 1,824 P3D files in the supplied checkout were inspected (1,784 game art
files plus 40 tool examples). Only two skin chunks were found, in unrelated
`biker_chick_temp.p3d` and `carney_A.p3d` examples. Homer text references in
scenery do not contain a Homer character skin or locomotion library.

The original `CharacterManager` loads model files from `art/chars/<name>_m.p3d`,
animation banks from `art/chars/<bank>_a.p3d`, choreography from
`art/chars/<bank>.cho`, and shared resources from `art/chars/global.p3d`.
The reward scripts explicitly reference `art/chars/homer_m.p3d`; that file
is missing from the GitHub checkout. To implement original Homer, obtain the
extracted `art/chars` files from the supplied PC archive, including Homer's
model, its locomotion bank/choreography and shared resources, then inspect their
skin weights, skeleton and animation channels before implementing the importer.

The linked MEGA archive has not been inspected: its storage servers previously
returned HTTP 403 in this environment. No missing character data was invented
or replaced with a claim of original animation support.
