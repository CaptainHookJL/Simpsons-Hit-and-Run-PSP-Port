#define _POSIX_C_SOURCE 200809L
#include "../src/texture.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void corrupt_check(TexturedScene *scene,const StaticMesh *mesh,const unsigned char *data,size_t size)
{
    char path[]="/tmp/shar-texture-test-XXXXXX";
    int fd=mkstemp(path);assert(fd>=0);
    FILE *f=fdopen(fd,"wb");assert(f);assert(fwrite(data,1,size,f)==size);fclose(f);
    TexturedVertex *saved=scene->vertices;
    assert(texture_load(scene,mesh,path)!=0 && scene->vertices==saved);
    unlink(path);
}

int main(int argc,char *argv[])
{
    assert(argc==3);
    StaticMesh mesh={0};TexturedScene scene={0};
    assert(mesh_load(&mesh,argv[1])==0);
    assert(texture_load(&scene,&mesh,argv[2])==0);
    assert(scene.count==mesh.count && scene.texture_count>1);
    assert((uintptr_t)scene.vertices%16==0);
    for (uint32_t i=0;i<scene.texture_count;++i) assert((uintptr_t)scene.textures[i].pixels%16==0);
    for (uint32_t i=0;i<scene.count;++i) {
        assert(scene.vertices[i].color==mesh.vertices[i].color);
        assert(scene.vertices[i].x==mesh.vertices[i].x && scene.vertices[i].y==mesh.vertices[i].y && scene.vertices[i].z==mesh.vertices[i].z);
    }
    FILE *f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);assert(size>0);rewind(f);
    unsigned char *data=malloc((size_t)size+1);assert(data);assert(fread(data,1,(size_t)size,f)==(size_t)size);fclose(f);
    corrupt_check(&scene,&mesh,data,24); /* truncated payload */
    data[8]^=1;corrupt_check(&scene,&mesh,data,(size_t)size);data[8]^=1; /* mismatched mesh CRC */
    uint32_t *first_batch=(uint32_t *)(data+24+mesh.count*8);
    uint32_t saved=first_batch[0];first_batch[0]=mesh.count;
    corrupt_check(&scene,&mesh,data,(size_t)size);first_batch[0]=saved; /* out-of-range draw */
    saved=first_batch[2];first_batch[2]=scene.texture_count;
    corrupt_check(&scene,&mesh,data,(size_t)size);first_batch[2]=saved; /* missing texture */
    uint32_t *dimensions=(uint32_t *)(data+24+mesh.count*8+scene.batch_count*12);
    saved=dimensions[0];dimensions[0]=0;
    corrupt_check(&scene,&mesh,data,(size_t)size);dimensions[0]=saved;
    data[size]='X';corrupt_check(&scene,&mesh,data,(size_t)size+1);
    printf("Texture checks passed: %u original textures, %u draw batches, %u pixel bytes; mesh binding, ranges, truncation, dimensions and trailing bytes validated\n",scene.texture_count-1,scene.batch_count,scene.pixel_bytes);
    free(data);texture_free(&scene);mesh_free(&mesh);
    return 0;
}
