#include "texture.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static uint32_t crc32(const unsigned char *data,size_t size)
{
    uint32_t crc=0xffffffffu;
    for (size_t i=0;i<size;++i) {
        crc^=data[i];
        for (int bit=0;bit<8;++bit) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return crc^0xffffffffu;
}

void texture_free(TexturedScene *scene)
{
    if (scene->textures) for (uint32_t i=0;i<scene->texture_count;++i) free(scene->textures[i].pixels);
    free(scene->vertices);free(scene->textures);free(scene->batches);
    memset(scene,0,sizeof(*scene));
}

int texture_load(TexturedScene *scene,const StaticMesh *mesh,const char *path)
{
    struct {char magic[4];uint32_t version,crc,count,textures,batches;} header;
    TexturedScene next={0};
    unsigned char *coverage=NULL;
    if (!mesh || !mesh->vertices || !mesh->count || mesh->count>49152 || mesh->count%3) return -1;
    FILE *file=fopen(path,"rb");
    if (!file) return -1;
    if (fread(&header,1,sizeof(header),file)!=sizeof(header) ||
        memcmp(header.magic,"STX1",4) || header.version!=1 || header.count!=mesh->count ||
        !header.textures || header.textures>128 || !header.batches || header.batches>1024 ||
        header.crc!=crc32((const unsigned char *)mesh->vertices,mesh->count*sizeof(MeshVertex)))
        goto invalid;
    next.count=header.count;next.texture_count=header.textures;next.batch_count=header.batches;
    next.vertices=memalign(16,next.count*sizeof(*next.vertices));
    next.textures=calloc(next.texture_count,sizeof(*next.textures));
    next.batches=malloc(next.batch_count*sizeof(*next.batches));
    coverage=calloc(next.count/3,1);
    if (!next.vertices || !next.textures || !next.batches || !coverage) goto invalid;
    for (uint32_t i=0;i<next.count;++i) {
        float uv[2];
        if (fread(uv,1,sizeof(uv),file)!=sizeof(uv) || !isfinite(uv[0])||!isfinite(uv[1]) ||
            fabsf(uv[0])>10000||fabsf(uv[1])>10000) goto invalid;
        MeshVertex v=mesh->vertices[i];
        next.vertices[i]=(TexturedVertex){uv[0],uv[1],v.color,v.x,v.y,v.z};
    }
    if (fread(next.batches,sizeof(*next.batches),next.batch_count,file)!=next.batch_count) goto invalid;
    for (uint32_t i=0;i<next.batch_count;++i) {
        TextureBatch b=next.batches[i];
        if (!b.count || b.first%3 || b.count%3 || b.first>=next.count ||
            b.count>next.count-b.first || b.texture>=next.texture_count) goto invalid;
        for (uint32_t j=b.first/3;j<(b.first+b.count)/3;++j) {
            if (coverage[j]) goto invalid;
            coverage[j]=1;
        }
    }
    for (uint32_t i=0;i<next.count/3;++i) if (!coverage[i]) goto invalid;
    for (uint32_t i=0;i<next.texture_count;++i) {
        uint32_t width,height,size;
        if (fread(&width,4,1,file)!=1 || fread(&height,4,1,file)!=1 || fread(&size,4,1,file)!=1 ||
            width<16 || width>128 || (width&(width-1)) || height<8 || height>128 || (height&(height-1)) ||
            size!=width*height*2 || next.pixel_bytes>2*1024*1024-size) goto invalid;
        SceneTexture *t=&next.textures[i];
        t->width=width;t->height=height;t->pixels=memalign(16,size);
        if (!t->pixels || fread(t->pixels,1,size,file)!=size) goto invalid;
        next.pixel_bytes+=size;
    }
    if (fgetc(file)!=EOF || ferror(file)) goto invalid;
    free(coverage);fclose(file);
    texture_free(scene);*scene=next;
    return 0;
invalid:
    free(coverage);fclose(file);texture_free(&next);
    return -1;
}
