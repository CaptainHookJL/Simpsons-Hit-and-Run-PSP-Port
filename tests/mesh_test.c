#define _POSIX_C_SOURCE 200809L
#include "../src/mesh.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc,char *argv[])
{
    assert(argc==2);
    StaticMesh mesh = {0};
    assert(mesh_load(&mesh,argv[1])==0);
    assert(mesh.count>0 && mesh.count%3==0);
    assert((uintptr_t)mesh.vertices%16==0);
    uint32_t loaded_count=mesh.count;
    MeshVertex *loaded_vertices=mesh.vertices;
    char path[]="/tmp/shar-mesh-test-XXXXXX";
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    struct {char magic[4];uint32_t version,count,reserved;} h={{'S','H','M','1'},1,3,0};
    FILE *f=fopen(path,"wb");
    assert(f);
    fwrite(&h,1,sizeof(h),f); /* missing vertex payload */
    fclose(f);
    assert(mesh_load(&mesh,path)!=0);
    assert(mesh.vertices==loaded_vertices && mesh.count==loaded_count);
    h.count=0xFFFFFFFFu; /* reject allocation before reading vertices */
    f=fopen(path,"wb"); assert(f); fwrite(&h,1,sizeof(h),f); fclose(f);
    assert(mesh_load(&mesh,path)!=0);
    h.count=3;
    MeshVertex vertices[3]={{0,NAN,0,0},{0,0,0,0},{0,1,0,0}};
    f=fopen(path,"wb"); assert(f);
    fwrite(&h,1,sizeof(h),f); fwrite(vertices,1,sizeof(vertices),f); fclose(f);
    assert(mesh_load(&mesh,path)!=0);
    vertices[0].x=0;
    f=fopen(path,"wb"); assert(f);
    fwrite(&h,1,sizeof(h),f); fwrite(vertices,1,sizeof(vertices),f); fputc('X',f); fclose(f);
    assert(mesh_load(&mesh,path)!=0); /* appended bytes are not silently accepted */
    unlink(path);
    mesh_free(&mesh);
    assert(mesh.vertices==NULL && mesh.count==0);
    puts("Mesh checks passed: real scenery, alignment, truncation, budget, nonfinite data, trailing bytes");
    return 0;
}
