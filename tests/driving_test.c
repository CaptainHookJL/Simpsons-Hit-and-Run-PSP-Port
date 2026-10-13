#include "../src/driving.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
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
    puts("Driving checks passed: acceleration, braking, forward/reverse steering, boundary, zero dt");
    return 0;
}
