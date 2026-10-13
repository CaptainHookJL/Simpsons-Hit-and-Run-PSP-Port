/*
 * Copyright (C) 2001 Radical Entertainment Ltd. All rights reserved.
 *
 * Adapted from Vehicle::SetWheelTurnAngle in:
 * Svxy/The-Simpsons-Hit-and-Run, commit
 * 0cf35df0a921a803a69d0da4c2aca02aa2543f03
 * game/code/worldsim/redbrick/vehicle.cpp
 *
 * PSP adaptation: retain the human gamepad steering path; pass original
 * member state/tuning as arguments; replace RADMath conversion/square with
 * scalar C arithmetic; return the wheel angle instead of updating sim wheels.
 * The steering-wheel accessory path, unused/commented experiments, asserts,
 * and middleware wheel writes are omitted. See SOURCE_REUSE.md.
 */
#include "vehicle_steering.h"

float shar_update_steering(float wheelTurnAngle, float speedKmh,
                          float percentOfTopSpeed, float highSpeedSteeringDrop,
                          float maxWheelTurnAngleDegrees, float dt,
                          float *wheelTurnAngleInputValue)
{
    float slope = highSpeedSteeringDrop - 1.0f;
    float yintercept = 1.0f;
    float value = slope * percentOfTopSpeed + yintercept;
    if (value < 0.0f) {
        value = 0.0f;
    }
    wheelTurnAngle *= value;

    const float thresholdLowSpeed = 90.0f;
    const float timeAtZero = 0.3f;
    float speedRatio = speedKmh / thresholdLowSpeed;
    float secondsToChangeOneUnitAtCurrentSpeed =
        (1.0f - speedRatio * speedRatio) * timeAtZero;
    if (secondsToChangeOneUnitAtCurrentSpeed < 0.15f) {
        secondsToChangeOneUnitAtCurrentSpeed = 0.15f;
    }
    if (secondsToChangeOneUnitAtCurrentSpeed > 0.0f) {
        float unitsPerSecond = 1.0f / secondsToChangeOneUnitAtCurrentSpeed;
        float unitsInThisFrame = unitsPerSecond * dt;
        if (wheelTurnAngle > *wheelTurnAngleInputValue) {
            if (wheelTurnAngle > (*wheelTurnAngleInputValue + unitsInThisFrame)) {
                *wheelTurnAngleInputValue += unitsInThisFrame;
            } else {
                *wheelTurnAngleInputValue = wheelTurnAngle;
            }
        } else if (wheelTurnAngle < *wheelTurnAngleInputValue) {
            if (wheelTurnAngle < (*wheelTurnAngleInputValue - unitsInThisFrame)) {
                *wheelTurnAngleInputValue -= unitsInThisFrame;
            } else {
                *wheelTurnAngleInputValue = wheelTurnAngle;
            }
        }
    }

    if (speedKmh < 50.0f) {
        maxWheelTurnAngleDegrees += 10.0f;
    }
    const float degreesToRadians = 0.017453292519943295f;
    return *wheelTurnAngleInputValue * maxWheelTurnAngleDegrees * degreesToRadians;
}
