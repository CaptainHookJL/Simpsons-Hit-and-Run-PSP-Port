#ifndef PSP_WORLD_COLLISION_H
#define PSP_WORLD_COLLISION_H
#include "driving.h"
#include "mesh.h"

typedef struct CollisionTriangle CollisionTriangle;
typedef struct {
    CollisionTriangle *triangles;
    uint32_t *references;
    uint32_t offsets[1025];
    uint32_t triangle_count, reference_count, stamp;
    float min_x, min_z, cell_size;
} CollisionWorld;

/* Zero-initialize before first build. Failed builds leave existing data intact. */
int collision_build(CollisionWorld *world,const StaticMesh *mesh);
void collision_free(CollisionWorld *world);
int collision_ground(const CollisionWorld *world,float x,float z,
                     float minimum,float maximum,float *height);
/* Resolve the proposed movement from previous to car, returning 1 when blocked.
   A two-sphere hull approximates the car; no suspension/damage is simulated. */
int collision_move(CollisionWorld *world,Car *car,const Car *previous);

#endif
