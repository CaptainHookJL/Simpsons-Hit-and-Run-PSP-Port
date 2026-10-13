#include "../src/clip.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void inside(const ClipCamera *camera,const TexturedVertex *vertices,uint32_t count)
{
    assert(count%3==0);
    for (uint32_t i=0;i<count;++i) {
        assert(isfinite(vertices[i].u)&&isfinite(vertices[i].v));
        for (int j=0;j<6;++j) {
            ClipPlane p=camera->planes[j];
            float d=p.normal.x*vertices[i].x+p.normal.y*vertices[i].y+p.normal.z*vertices[i].z+p.offset;
            assert(d>=-0.001f);
        }
    }
}

static void synthetic_checks(void)
{
    ClipCamera camera;
    assert(clip_camera(&camera,(ClipVector){0},(ClipVector){0,0,1},90,1,0.5f,100)==0);
    TexturedVertex input[3]={{0,0,0xff000000,-0.2f,-0.2f,0.1f},
                             {1,0,0xffffffff,0.2f,-0.2f,2},
                             {0,1,0xffffffff,0,0.2f,2}};
    TexturedVertex output[21];
    int count=clip_triangle(&camera,input,output);
    assert(count==6); /* one point behind near plane produces two triangles */
    inside(&camera,output,(uint32_t)count);
    int intersections=0;
    for (int i=0;i<count;++i) if (fabsf(output[i].z-camera.near_distance)<0.0001f) {
        ++intersections;
        float expected=(camera.near_distance-0.1f)/1.9f;
        assert(fabsf(output[i].u+output[i].v-expected)<0.0001f);
        unsigned int channel=output[i].color&255;
        assert(fabsf(channel-expected*255)<1);
    }
    assert(intersections>=2);
    for (int i=0;i<3;++i) input[i].z=0.1f;
    assert(clip_triangle(&camera,input,output)==0);
    for (int i=0;i<3;++i) input[i].z=2;
    count=clip_triangle(&camera,input,output);
    assert(count==3 && memcmp(input,output,sizeof(input))==0);
    input[0].x=-100;input[1].x=100;
    count=clip_triangle(&camera,input,output);
    assert(count>=3 && count<=21);inside(&camera,output,(uint32_t)count);
    assert(clip_camera(&camera,(ClipVector){0},(ClipVector){0},60,1,0.5f,100)!=0);
}

/* Compare every submitted vertex with the full per-triangle clipper, so bounds
   shortcuts cannot silently reintroduce road holes or drop visible geometry. */
static void compare_reference(const ClipCamera *camera,const TexturedScene *scene,const ClippedScene *output)
{
    uint32_t batch=0,offset=0;
    for (uint32_t b=0;b<scene->batch_count;++b) {
        TextureBatch source=scene->batches[b];
        for (uint32_t i=source.first;i<source.first+source.count;i+=3) {
            TexturedVertex expected[21];
            int count=clip_triangle(camera,scene->vertices+i,expected);
            assert(count>=0);
            for (int j=0;j<count;++j) {
                assert(batch<output->batch_count);
                TextureBatch draw=output->batches[batch];
                const TexturedVertex *vertices=output->original[batch]?scene->vertices:output->vertices;
                TexturedVertex actual=vertices[draw.first+offset];
                assert(draw.texture==source.texture);
                assert(fabsf(actual.x-expected[j].x)<0.002f && fabsf(actual.y-expected[j].y)<0.002f && fabsf(actual.z-expected[j].z)<0.002f);
                assert(fabsf(actual.u-expected[j].u)<0.002f && fabsf(actual.v-expected[j].v)<0.002f);
                for (int shift=0;shift<32;shift+=8)
                    assert(abs((int)((actual.color>>shift)&255)-(int)((expected[j].color>>shift)&255))<=1);
                if (++offset==draw.count) {++batch;offset=0;}
            }
        }
    }
    assert(batch==output->batch_count && offset==0);
}

int main(int argc,char *argv[])
{
    assert(argc==3);
    synthetic_checks();
    StaticMesh mesh={0};TexturedScene scene={0};ClippedScene output={0};
    assert(mesh_load(&mesh,argv[1])==0);
    assert(texture_load(&scene,&mesh,argv[2])==0);
    assert(clip_create(&output,&scene)==0);
    uint32_t peak=0;
    uint32_t tested=0,copied=0,direct=0;
    for (int position=0;position<5;++position) for (int angle=0;angle<12;++angle) {
        float heading=angle*0.523598776f,x=(position-2)*10.0f,z=(position-2)*7.0f;
        ClipVector eye={x-sinf(heading)*9,5,z-cosf(heading)*9};
        ClipVector target={x+sinf(heading)*4,1,z+cosf(heading)*4};
        ClipCamera camera;
        assert(clip_camera(&camera,eye,target,60,480.0f/272,0.5f,300)==0);
        assert(clip_scene(&output,&scene,&camera)==0);
        assert(output.count<=output.capacity && output.batch_count<=scene.batch_count);
        inside(&camera,output.vertices,output.count);
        uint32_t covered=0,direct_covered=0;
        for (uint32_t i=0;i<output.batch_count;++i) {
            TextureBatch b=output.batches[i];
            assert(b.count%3==0 && b.texture<scene.texture_count);
            if (output.original[i]) {
                assert(b.first+b.count<=scene.count);
                inside(&camera,scene.vertices+b.first,b.count);
                direct_covered+=b.count;
            } else {
                assert(b.first==covered);
                covered+=b.count;
            }
        }
        assert(covered==output.count && direct_covered==output.direct_count);
        compare_reference(&camera,&scene,&output);
        tested+=output.tested_count;copied+=output.count;direct+=output.direct_count;
        if (output.count>peak) peak=output.count;
    }
    printf("Software clipping checks passed: near-plane road regression, UV/colour interpolation, screen edges, rejection, 60 original-scene camera views (%u peak output vertices)\n",peak);
    printf("Optimized path matches full clipper: average %u/%u vertices tested, %u copied, %u drawn directly per view\n",tested/60,scene.count,copied/60,direct/60);
    clip_free(&output);texture_free(&scene);mesh_free(&mesh);
    return 0;
}
