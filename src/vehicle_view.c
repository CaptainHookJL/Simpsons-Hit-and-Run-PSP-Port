#include "vehicle_view.h"
#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

void vehicle_view_free(VehicleView *view)
{
    free(view->draw.vertices);free(view->draw.batches);memset(view,0,sizeof(*view));
}

int vehicle_view_create(VehicleView *view,const TexturedScene *scene,const WheelRig *rig)
{
    if (!scene->vertices||!scene->count||scene->count>6144||scene->count%3||!scene->batch_count) return -1;
    VehicleView next={0};
    next.capacity=scene->count*7;next.batch_capacity=scene->batch_count;
    if (rig->parts[0].draw.vertices) {
        next.batch_capacity=0;
        for (int part=0;part<5;++part) {
            const WheelPart *wheel=&rig->parts[part];
            next.batch_capacity+=wheel->draw.batch_count;
            float pivot=hypotf(hypotf(wheel->x,wheel->y-0.7f),wheel->z);
            for (uint32_t i=0;i<wheel->draw.count;++i) {
                TexturedVertex v=wheel->draw.vertices[i];
                float radius=part?pivot+hypotf(hypotf(v.x,v.y),v.z):hypotf(hypotf(v.x,v.y-0.7f),v.z);
                next.radius=fmaxf(next.radius,radius);
            }
        }
    } else for (uint32_t i=0;i<scene->count;++i) {
        TexturedVertex v=scene->vertices[i];next.radius=fmaxf(next.radius,hypotf(hypotf(v.x,v.y-0.7f),v.z));
    }
    next.radius+=0.01f;
    if (!next.batch_capacity||next.batch_capacity>1024) return -1;
    next.draw.vertices=memalign(16,next.capacity*sizeof(TexturedVertex));
    next.draw.batches=malloc(next.batch_capacity*sizeof(TextureBatch));
    if (!next.draw.vertices||!next.draw.batches) {vehicle_view_free(&next);return -1;}
    next.draw.textures=scene->textures;next.draw.texture_count=scene->texture_count;
    vehicle_view_free(view);*view=next;return 0;
}

int vehicle_view_visibility(const VehicleView *view,const Car *car,const ClipCamera *camera)
{
    int intersects=0;
    for (int i=0;i<6;++i) {
        ClipPlane p=camera->planes[i];
        float distance=p.normal.x*car->x+p.normal.y*(car->y+0.7f)+p.normal.z*car->z+p.offset;
        float extent=view->radius*hypotf(hypotf(p.normal.x,p.normal.y),p.normal.z);
        if (distance+extent<-0.002f) return -1;
        if (distance-extent<0.002f) intersects=1;
    }
    return intersects?0:1;
}

int vehicle_view_clip(VehicleView *view,const TexturedScene *scene,const WheelRig *rig,
                      const WheelAnimation *animation,const Car *car,const ClipCamera *camera)
{
    view->draw.count=0;view->draw.batch_count=0;
    if (!view->draw.vertices||view->capacity<scene->count*7) return -1;
    int parts=rig->parts[0].draw.vertices?5:1;
    float root_s=sinf(car->heading),root_c=cosf(car->heading);
    for (int part=0;part<parts;++part) {
        const WheelPart *wheel=parts==5?&rig->parts[part]:NULL;
        const TexturedScene *source=wheel?&wheel->draw:scene;
        float angle=wheel&&wheel->steering?-car->wheel_angle:0;
        float ss=sinf(angle),sc=cosf(angle),rs=part?sinf(animation->roll[part-1]):0,rc=part?cosf(animation->roll[part-1]):1;
        for (uint32_t b=0;b<source->batch_count;++b) {
            TextureBatch batch=source->batches[b];uint32_t first=view->draw.count;
            for (uint32_t i=batch.first;i<batch.first+batch.count;i+=3) {
                TexturedVertex input[3],output[21];
                for (int j=0;j<3;++j) {
                    TexturedVertex v=source->vertices[i+j];
                    float y=rc*v.y-rs*v.z,z=rs*v.y+rc*v.z,x=sc*v.x+ss*z;
                    z=sc*z-ss*v.x;
                    if (wheel) {x+=wheel->x;y+=wheel->y;z+=wheel->z;}
                    v.x=car->x+root_c*x+root_s*z;v.y=car->y+y;v.z=car->z+root_c*z-root_s*x;
                    input[j]=v;
                }
                int count=clip_triangle(camera,input,output);
                if (count<0||view->draw.count+(uint32_t)count>view->capacity) return -1;
                memcpy(view->draw.vertices+view->draw.count,output,count*sizeof(*output));
                view->draw.count+=(uint32_t)count;
            }
            if (view->draw.count>first) {
                if (view->draw.batch_count>=view->batch_capacity) return -1;
                view->draw.batches[view->draw.batch_count++]=(TextureBatch){first,view->draw.count-first,batch.texture};
            }
        }
    }
    return 0;
}
