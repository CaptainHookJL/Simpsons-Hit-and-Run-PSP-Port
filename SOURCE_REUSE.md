# Original source integrated into the PSP build

Upstream: [Svxy/The-Simpsons-Hit-and-Run](https://github.com/Svxy/The-Simpsons-Hit-and-Run),
commit `0cf35df0a921a803a69d0da4c2aca02aa2543f03`.
Original copyright notices are preserved in the adapted implementation.

`src/upstream/vehicle_steering.c` adapts the human gamepad path of
[`Vehicle::SetWheelTurnAngle`](https://github.com/Svxy/The-Simpsons-Hit-and-Run/blob/0cf35df0a921a803a69d0da4c2aca02aa2543f03/game/code/worldsim/redbrick/vehicle.cpp#L1658).
It retains these active parts of the original implementation:

- Linear high-speed sensitivity reduction and the nonnegative multiplier clamp.
- The 90 km/h steering-smoothing threshold, 0.3-second rate at rest, parabolic
  speed dependence, and 0.15-second minimum.
- Both branches that approach the target input without overshoot.
- An extra 10 degrees of steering angle below 50 km/h.

Defaults of 220 km/h top speed, 35 degrees maximum wheel angle, and zero
high-speed steering drop come from
[`vehicleinit.cpp`](https://github.com/Svxy/The-Simpsons-Hit-and-Run/blob/0cf35df0a921a803a69d0da4c2aca02aa2543f03/game/code/worldsim/redbrick/vehicleinit.cpp#L440).
Original per-vehicle tuning scripts are not loaded yet.

Adaptation replaces member accesses with scalar parameters and a persistent
input value, RADMath square/radian helpers with equivalent C arithmetic, and
simulation wheel writes with a returned wheel angle. The prototype only drives
a user vehicle with gamepad controls, so steering-wheel accessory and other
vehicle-type branches are omitted, along with inactive experiments and assertions.

The PSP executable actually calls this code each simulation step. The resulting
wheel angle drives a new bicycle model, not the original suspension/force solver.
Acceleration, braking, and rendering remain prototype code. World geometry now includes a converted original
scenery sample documented in `assets/README.md`.
The visual player vehicle uses the original CompactA parked-car mesh and textures
documented in `assets/VEHICLE.md`. Its imported appearance does not add original
wheel animation, suspension, or vehicle damage behavior.
This is reuse of the original steering controller, not a claim that original
vehicle physics or complete gameplay has been ported.

Native tests cover the original rate limit, angle change at 50 km/h, half-top-speed
attenuation, target settling, prevention of negative sensitivity, and direction
reversal. The controller is also compiled and linked into the PSP PRX.
