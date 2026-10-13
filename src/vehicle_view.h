#ifndef PSP_VEHICLE_VIEW_H
#define PSP_VEHICLE_VIEW_H
#include "wheels.h"
#include "clip.h"
/* Reusable scratch for a parked car crossing the on-foot camera's planes.
   Textures are borrowed. Zero-initialize, and free before the source scene. */
typedef struct {TexturedScene draw;uint32_t capacity,batch_capacity;float radius;} VehicleView;
int vehicle_view_create(VehicleView *view,const TexturedScene *scene,const WheelRig *rig);
void vehicle_view_free(VehicleView *view);
/* -1 outside, 0 intersects planes, 1 wholly visible. */
int vehicle_view_visibility(const VehicleView *view,const Car *car,const ClipCamera *camera);
int vehicle_view_clip(VehicleView *view,const TexturedScene *scene,const WheelRig *rig,
                      const WheelAnimation *animation,const Car *car,const ClipCamera *camera);
#endif
