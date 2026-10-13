#ifndef PSP_VEHICLE_WHEELS_H
#define PSP_VEHICLE_WHEELS_H
#include "texture.h"
#include "driving.h"

typedef struct {
    TexturedScene draw; /* owns vertices/batches; borrows the parent's textures */
    float x,y,z,radius;
    uint32_t steering;
} WheelPart;
typedef struct {WheelPart parts[5];} WheelRig; /* body, then four wheels */
typedef struct {float roll[4];} WheelAnimation;

/* Zero-initialize before use. Failed loads preserve the existing rig.
   The source textures must outlive the rig; free the rig before its scene. */
int wheels_load(WheelRig *rig,const StaticMesh *mesh,const TexturedScene *scene,const char *path);
void wheels_free(WheelRig *rig);
void wheels_reset(WheelAnimation *animation);
void wheels_step(WheelAnimation *animation,const WheelRig *rig,const Car *previous,const Car *current);

#endif
