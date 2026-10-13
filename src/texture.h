#ifndef PSP_SCENE_TEXTURE_H
#define PSP_SCENE_TEXTURE_H
#include "mesh.h"

typedef struct {float u,v;uint32_t color;float x,y,z;} TexturedVertex;
typedef struct {uint32_t width,height;uint16_t *pixels;} SceneTexture;
typedef struct {uint32_t first,count,texture;} TextureBatch;
typedef struct {
    TexturedVertex *vertices;
    SceneTexture *textures;
    TextureBatch *batches;
    uint32_t count,texture_count,batch_count,pixel_bytes;
} TexturedScene;
/* Zero-initialize before use. Failed loads preserve any previously loaded scene. */
int texture_load(TexturedScene *scene,const StaticMesh *mesh,const char *path);
void texture_free(TexturedScene *scene);

#endif
