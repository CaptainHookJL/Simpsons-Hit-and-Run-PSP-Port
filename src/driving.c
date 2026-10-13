#include "driving.h"
#include <math.h>

static float clamp(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

void car_reset(Car *car)
{
    *car = (Car){0, 0, 0, 0};
}

void car_step(Car *car, Controls controls, float dt)
{
    /* Bound time after a pause; the caller normally uses a fixed 60 Hz step. */
    dt = clamp(dt, 0, 0.05f);
    float throttle = clamp(controls.throttle, 0, 1);
    float reverse = clamp(controls.reverse, 0, 1);
    float brake = clamp(controls.brake, 0, 1);
    float steering = clamp(controls.steering, -1, 1);
    car->speed += (throttle * 12.0f - reverse * 8.0f) * dt;
    float deceleration = (1.2f + fabsf(car->speed) * 0.025f + brake * 24.0f) * dt;
    if (fabsf(car->speed) <= deceleration) car->speed = 0;
    else car->speed -= copysignf(deceleration, car->speed);
    car->speed = clamp(car->speed, -10, 32);
    float turn = steering * car->speed / (4.0f + fabsf(car->speed) * 0.35f);
    /* Looking along +Z with Y up, camera-right is -X. */
    car->heading -= turn * dt;
    car->x += sinf(car->heading) * car->speed * dt;
    car->z += cosf(car->heading) * car->speed * dt;
    /* Placeholder test-ground boundary, rather than streamed world geometry. */
    if (fabsf(car->x) > 110 || fabsf(car->z) > 110) {
        car->x = clamp(car->x, -110, 110);
        car->z = clamp(car->z, -110, 110);
        car->speed = 0;
    }
}
