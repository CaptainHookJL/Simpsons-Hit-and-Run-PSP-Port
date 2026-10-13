#include "mesh.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#define MAX_MESH_VERTICES 49152

int mesh_load(StaticMesh *mesh, const char *path)
{
    /* Format is little-endian, matching the PC converter and PSP MIPS CPU. */
    struct { char magic[4]; uint32_t version, count, reserved; } header;
    FILE *file = fopen(path,"rb");
    MeshVertex *vertices = NULL;
    if (!file) return -1;
    if (fread(&header,1,sizeof(header),file) != sizeof(header) ||
        memcmp(header.magic,"SHM1",4) || header.version != 1 || header.reserved ||
        !header.count || header.count > MAX_MESH_VERTICES || header.count % 3)
        goto invalid;
    size_t size = header.count * sizeof(MeshVertex);
    vertices = memalign(16,size);
    if (!vertices || fread(vertices,1,size,file) != size) goto invalid;
    if (fgetc(file) != EOF || ferror(file)) goto invalid;
    for (uint32_t i=0; i<header.count; ++i) {
        MeshVertex v = vertices[i];
        if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z) ||
            fabsf(v.x)>10000 || fabsf(v.y)>10000 || fabsf(v.z)>10000)
            goto invalid;
    }
    fclose(file);
    mesh_free(mesh);
    mesh->vertices = vertices;
    mesh->count = header.count;
    return 0;
invalid:
    free(vertices);
    fclose(file);
    return -1;
}

void mesh_free(StaticMesh *mesh)
{
    free(mesh->vertices);
    mesh->vertices = NULL;
    mesh->count = 0;
}
