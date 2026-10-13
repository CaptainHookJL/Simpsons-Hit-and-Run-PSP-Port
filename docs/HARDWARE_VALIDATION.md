# PSP hardware validation

Performance baseline: **716b316**, reported by the user on 13 October 2026.

- Device: PSP-3000.
- Custom firmware: ARK 5 (as reported; exact firmware revision not supplied).
- Result: "back to locked 60fps" after installing the optimized clipping build.
- Validation method: user testing on the physical device; the cloud environment
  does not independently measure PSP frame time.

Prior hardware results:

| Build | User-reported result |
| --- | --- |
| e7f80f7 | Geometry loads correctly, 60 FPS. |
| 3d41e77 | Collision build runs perfectly, 60 FPS. |
| 4722096 | Textured build runs at 60 FPS, with visible geometry disappearing. |
| 116afb7 | Software clipping fixes disappearing roads, 30–60 FPS. |
| 716b316 | Optimized clipping restores locked 60 FPS. |
| e45c6e0 | Original CompactA vehicle build: "works perfectly"; no separate FPS figure supplied. |

The optimization passes native comparison against the full clipper across 60
camera views, with matching submitted geometry and UVs. Subsequent rendering or
asset changes should be checked on the same device and driving route for road
continuity, collision, and FPS. Other PSP models, firmware configurations, larger
maps, and other original vehicle assets have not been validated by these reports.

The current wheel-animation build is awaiting hardware testing. Cloud checks
pass for unchanged geometry/UVs, forward/reverse rotation, front steering inputs,
turning arcs, collision stopping, reset, damaged metadata and 864 chase-camera
orientations/steering/roll combinations. The PSP executable builds successfully.
On the PSP, confirm `Wheel animation ON`, front-tyre direction, rolling direction,
stopping at walls, Start reset, road continuity and FPS on the established route.
The prior locked-60-FPS report does not establish performance for this change.
