#ifndef PSP_SCENE_CLIP_H
#define PSP_SCENE_CLIP_H
#include "texture.h"

typedef struct {float x,y,z;} ClipVector;
typedef struct {ClipVector normal;float offset;} ClipPlane;
typedef struct {
    ClipVector eye,right,up,forward;
    ClipPlane planes[6];
    float near_distance,far_distance,tan_x,tan_y;
} ClipCamera;
typedef struct {
    TexturedVertex *vertices;
    TextureBatch *batches;
    unsigned char *codes;
    uint32_t capacity,source_count,count,batch_count;
} ClippedScene;

int clip_camera(ClipCamera *camera,ClipVector eye,ClipVector target,
                float vertical_fov,float aspect,float near_distance,float far_distance);
/* A clipped triangle can form a nine-vertex polygon: at most 21 fan vertices. */
int clip_triangle(const ClipCamera *camera,const TexturedVertex input[3],TexturedVertex output[21]);
/* Zero-initialize output first. Allocate once; reuse without allocations each frame. */
int clip_create(ClippedScene *output,const TexturedScene *scene);
int clip_scene(ClippedScene *output,const TexturedScene *scene,const ClipCamera *camera);
void clip_free(ClippedScene *output);

#endif
