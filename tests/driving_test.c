#include "../src/driving.h"
#include "../src/upstream/vehicle_steering.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void check_original_steering(void)
{
    const float degreesToRadians = 0.017453292519943295f;
    float input = 0;
    /* At rest the original controller takes 0.3 s per unit of input. */
    shar_update_steering(1,0,0,0,35,0.01f,&input);
    assert(fabsf(input - 1.0f/30) < 0.00001f);
    for (int i=0; i<40; ++i) shar_update_steering(1,0,0,0,35,0.01f,&input);
    assert(input == 1); /* settles without overshoot */
    float angle = shar_update_steering(1,0,0,0,35,0,&input);
    assert(fabsf(angle - 45 * degreesToRadians) < 0.00001f);
    angle = shar_update_steering(1,50,0,0,35,0,&input);
    assert(fabsf(angle - 35 * degreesToRadians) < 0.00001f);
    /* Half top speed halves target input when steering drop is zero. */
    input = 0;
    for (int i=0; i<60; ++i) angle = shar_update_steering(1,110,0.5f,0,35,1.0f/60,&input);
    assert(fabsf(input - 0.5f) < 0.00001f);
    assert(fabsf(angle - 17.5f * degreesToRadians) < 0.00001f);
    /* Speed above the configured top speed must never invert steering. */
    for (int i=0; i<60; ++i) angle = shar_update_steering(1,300,1.4f,0,35,1.0f/60,&input);
    assert(input == 0 && angle == 0);
    input = 1;
    shar_update_steering(-1,0,0,0,35,0.03f,&input);
    assert(fabsf(input - 0.9f) < 0.00001f); /* reversal also respects slew limit */
    shar_update_steering(-1,0,0,0,35,0,&input);
    assert(fabsf(input - 0.9f) < 0.00001f);
}

int main(void)
{
    check_original_steering();
    Car car;
    car_reset(&car);
    for (int i=0; i<600; ++i) car_step(&car,(Controls){0,1,0,0},1.0f/60);
    assert(car.z <= 110 && car.x == 0); /* test-ground edge stops the car */
    assert(car.speed == 0);
    car_reset(&car);
    for (int i=0; i<60; ++i) car_step(&car,(Controls){0,1,0,0},1.0f/60);
    assert(car.speed > 10 && car.z > 0);
    for (int i=0; i<60; ++i) car_step(&car,(Controls){0,0,1,0},1.0f/60);
    assert(car.speed == 0); /* brakes must not accidentally engage reverse */
    car_reset(&car);
    for (int i=0; i<60; ++i) car_step(&car,(Controls){1,1,0,0},1.0f/60);
    assert(car.speed > 0 && car.heading < 0 && car.x < 0);
    car_reset(&car);
    for (int i=0; i<60; ++i) car_step(&car,(Controls){1,0,0,1},1.0f/60);
    assert(car.speed < 0 && car.heading > 0 && car.z < 0);
    car_reset(&car);
    car_step(&car,(Controls){1,1,0,0},0);
    assert(car.x == 0 && car.z == 0 && car.speed == 0);
    puts("Driving checks passed, including original steering: smoothing, speed reduction, low-speed turns, reversal, zero dt");
    return 0;
}
