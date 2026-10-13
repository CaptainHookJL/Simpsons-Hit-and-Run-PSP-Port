#ifndef SHAR_VEHICLE_STEERING_H
#define SHAR_VEHICLE_STEERING_H

/* Original vehicleinit.cpp defaults, before per-vehicle tuning scripts. */
#define SHAR_DEFAULT_TOP_SPEED_KMH 220.0f
#define SHAR_DEFAULT_MAX_WHEEL_ANGLE_DEG 35.0f
#define SHAR_DEFAULT_HIGH_SPEED_STEERING_DROP 0.0f

float shar_update_steering(float wheelTurnAngle, float speedKmh,
                          float percentOfTopSpeed, float highSpeedSteeringDrop,
                          float maxWheelTurnAngleDegrees, float dt,
                          float *wheelTurnAngleInputValue);

#endif
