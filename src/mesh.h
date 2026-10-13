#ifndef PSP_STATIC_MESH_H
#define PSP_STATIC_MESH_H
#include <stdint.h>

typedef struct { uint32_t color; float x,y,z; } MeshVertex;
typedef struct { MeshVertex *vertices; uint32_t count; } StaticMesh;
/* Zero-initialize mesh before first use. On success, caller owns the aligned
   vertices and must call mesh_free. A failed load preserves existing data. */
int mesh_load(StaticMesh *mesh, const char *path);
void mesh_free(StaticMesh *mesh);

#endif
