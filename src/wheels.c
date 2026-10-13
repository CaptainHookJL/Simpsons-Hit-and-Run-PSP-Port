/*
 * Rolling relation adapted from Wheel::CalculateRotAngle in
 * game/code/worldsim/redbrick/wheel.cpp, Svxy/The-Simpsons-Hit-and-Run
 * commit 0cf35df0a921a803a69d0da4c2aca02aa2543f03.
 * Original: Copyright (c) 2002 Radical Entertainment, Inc. All rights reserved.
 * This standalone implementation uses post-collision pivot displacement.
 * See SOURCE_REUSE.md for the differences from the original simulation.
 */
#include "wheels.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static uint32_t mesh_crc(const StaticMesh *mesh)
{
    const unsigned char *data=(const unsigned char *)mesh->vertices;
    uint32_t crc=0xffffffffu;
    for (size_t i=0;i<mesh->count*sizeof(MeshVertex);++i) {
        crc^=data[i];
        for (int bit=0;bit<8;++bit) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return crc^0xffffffffu;
}

void wheels_free(WheelRig *rig)
{
    for (int i=0;i<5;++i) {
        free(rig->parts[i].draw.vertices);
        free(rig->parts[i].draw.batches);
    }
    memset(rig,0,sizeof(*rig));
}

int wheels_load(WheelRig *rig,const StaticMesh *mesh,const TexturedScene *scene,const char *path)
{
    struct {char magic[4];uint32_t version,crc,vertices,wheels;} header;
    WheelRig next={0};
    unsigned char *part_ids=NULL;
    uint32_t *texture_ids=NULL;
    FILE *file=NULL;
    if (!mesh||!mesh->vertices||!mesh->count||mesh->count>6144||mesh->count%3||
        !scene||!scene->vertices||scene->count!=mesh->count||!scene->textures||
        !scene->texture_count||scene->texture_count>128||!scene->batches||!scene->batch_count||scene->batch_count>1024)
        return -1;
    file=fopen(path,"rb");
    if (!file) return -1;
    if (fread(&header,1,sizeof(header),file)!=sizeof(header)||memcmp(header.magic,"SWH1",4)||
        header.version!=1||header.wheels!=4||header.vertices!=mesh->count||header.crc!=mesh_crc(mesh)) goto invalid;
    uint32_t triangles=mesh->count/3;
    part_ids=calloc(triangles,1);
    texture_ids=malloc(triangles*sizeof(*texture_ids));
    if (!part_ids||!texture_ids) goto invalid;
    for (uint32_t i=0;i<triangles;++i) texture_ids[i]=0xffffffffu;
    for (uint32_t i=0;i<scene->batch_count;++i) {
        TextureBatch b=scene->batches[i];
        if (!b.count||b.first>=scene->count||b.count>scene->count-b.first||b.first%3||b.count%3||b.texture>=scene->texture_count) goto invalid;
        for (uint32_t j=b.first/3;j<(b.first+b.count)/3;++j) {
            if (texture_ids[j]!=0xffffffffu) goto invalid;
            texture_ids[j]=b.texture;
        }
    }
    for (uint32_t i=0;i<triangles;++i) if (texture_ids[i]==0xffffffffu) goto invalid;
    unsigned int quadrants=0;
    for (int wheel=1;wheel<=4;++wheel) {
        struct {float x,y,z,radius;uint32_t steering,triangles;} meta;
        if (fread(&meta,1,sizeof(meta),file)!=sizeof(meta)||
            !isfinite(meta.x)||!isfinite(meta.y)||!isfinite(meta.z)||!isfinite(meta.radius)||
            fabsf(meta.x)<0.5f||fabsf(meta.x)>2||fabsf(meta.z)<0.5f||fabsf(meta.z)>2||
            meta.y<0||meta.y>2||meta.radius<0.05f||meta.radius>1||meta.steering>1||
            meta.steering!=(meta.z>0)||meta.triangles<8||meta.triangles>256||meta.triangles>triangles)
            goto invalid;
        unsigned int quadrant=(meta.x>0?1:0)+(meta.steering?2:0);
        if (quadrants&(1u<<quadrant)) goto invalid;
        quadrants|=1u<<quadrant;
        WheelPart *part=&next.parts[wheel];
        part->x=meta.x;part->y=meta.y;part->z=meta.z;part->radius=meta.radius;part->steering=meta.steering;
        for (uint32_t i=0;i<meta.triangles;++i) {
            uint32_t id;
            if (fread(&id,4,1,file)!=1||id>=triangles||part_ids[id]) goto invalid;
            for (uint32_t j=id*3;j<id*3+3;++j) {
                MeshVertex v=mesh->vertices[j];
                if (fabsf(v.x-meta.x)>0.4f||hypotf(v.y-meta.y,v.z-meta.z)>meta.radius+0.03f) goto invalid;
            }
            part_ids[id]=(unsigned char)wheel;
        }
    }
    if (quadrants!=15||fgetc(file)!=EOF||ferror(file)) goto invalid;
    for (int part_id=0;part_id<5;++part_id) {
        WheelPart *part=&next.parts[part_id];
        uint32_t count=0;
        for (uint32_t i=0;i<triangles;++i) if (part_ids[i]==part_id) count+=3;
        if (!count) goto invalid;
        part->draw.count=count;
        part->draw.texture_count=scene->texture_count;
        part->draw.textures=scene->textures; /* borrowed; never freed by wheels_free */
        part->draw.vertices=memalign(16,count*sizeof(TexturedVertex));
        part->draw.batches=malloc(scene->texture_count*sizeof(TextureBatch));
        if (!part->draw.vertices||!part->draw.batches) goto invalid;
        uint32_t cursor=0;
        for (uint32_t texture=0;texture<scene->texture_count;++texture) {
            uint32_t first=cursor;
            for (uint32_t i=0;i<triangles;++i) if (part_ids[i]==part_id && texture_ids[i]==texture) {
                for (uint32_t j=i*3;j<i*3+3;++j) {
                    TexturedVertex v=scene->vertices[j];
                    v.x-=part->x;v.y-=part->y;v.z-=part->z;
                    part->draw.vertices[cursor++]=v;
                }
            }
            if (cursor>first) part->draw.batches[part->draw.batch_count++]=(TextureBatch){first,cursor-first,texture};
        }
        if (cursor!=count) goto invalid;
    }
    free(part_ids);free(texture_ids);fclose(file);
    wheels_free(rig);*rig=next;
    return 0;
invalid:
    free(part_ids);free(texture_ids);fclose(file);wheels_free(&next);
    return -1;
}

void wheels_reset(WheelAnimation *animation)
{
    memset(animation,0,sizeof(*animation));
}

void wheels_step(WheelAnimation *animation,const WheelRig *rig,const Car *previous,const Car *current)
{
    /* The original Wheel::CalculateRotAngle projects suspension-point motion
       along the wheel facing and divides travel by radius. Here we use actual
       post-collision pivot displacement, rather than unavailable sim velocities.
       Burnout, suspension and tyre-slip branches are not implemented. */
    float old_s=sinf(previous->heading),old_c=cosf(previous->heading);
    float new_s=sinf(current->heading),new_c=cosf(current->heading);
    float turn=remainderf(current->heading-previous->heading,6.28318531f);
    float facing=previous->heading+turn*0.5f;
    float steer=(previous->wheel_angle+current->wheel_angle)*-0.5f;
    float direction_s[2]={sinf(facing),sinf(facing+steer)};
    float direction_c[2]={cosf(facing),cosf(facing+steer)};
    for (int i=0;i<4;++i) {
        const WheelPart *part=&rig->parts[i+1];
        if (part->radius<=0) continue;
        float dx=current->x-previous->x+(new_c-old_c)*part->x+(new_s-old_s)*part->z;
        float dz=current->z-previous->z+(new_c-old_c)*part->z-(new_s-old_s)*part->x;
        float distance=dx*direction_s[part->steering]+dz*direction_c[part->steering];
        animation->roll[i]=remainderf(animation->roll[i]+distance/part->radius,6.28318531f);
    }
}
