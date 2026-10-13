#ifndef DRIVING_H
#define DRIVING_H

typedef struct { float x, z, heading, speed, steering_input; } Car;
typedef struct { float steering, throttle, brake, reverse; } Controls;
void car_reset(Car *car);
void car_step(Car *car, Controls controls, float dt);

#endif
