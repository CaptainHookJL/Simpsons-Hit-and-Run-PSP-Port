#define _POSIX_C_SOURCE 200809L
#include "../src/wheels.h"
#include "../src/clip.h"
#include "../src/collision.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned char *read_file(const char *path,size_t *size)
{
    FILE *file=fopen(path,"rb");assert(file);
    assert(fseek(file,0,SEEK_END)==0);
    long length=ftell(file);assert(length>0);
    *size=(size_t)length;rewind(file);
    unsigned char *data=malloc(*size);assert(data);
    assert(fread(data,1,*size,file)==*size);assert(fclose(file)==0);
    return data;
}

static uint32_t word(const unsigned char *data)
{
    uint32_t value;memcpy(&value,data,4);return value;
}

static void geometry_checks(const WheelRig *rig,const TexturedScene *scene,const unsigned char *meta)
{
    unsigned char membership[310]={0};size_t offset=20;
    for (int part=1;part<5;++part) {
        uint32_t count=word(meta+offset+20);offset+=24;
        assert(count==36);
        for (uint32_t i=0;i<count;++i) {
            uint32_t id=word(meta+offset);offset+=4;
            assert(id<310 && membership[id]==0);membership[id]=(unsigned char)part;
        }
    }
    uint32_t triangles=0,batches=0;
    for (int id=0;id<5;++id) {
        const WheelPart *part=&rig->parts[id];
        assert(part->draw.count==(id?108u:498u));
        assert(part->draw.textures==scene->textures);
        assert(((uintptr_t)part->draw.vertices&15)==0);
        triangles+=part->draw.count/3;batches+=part->draw.batch_count;
        uint32_t cursor=0;
        for (uint32_t b=0;b<part->draw.batch_count;++b) {
            TextureBatch batch=part->draw.batches[b];assert(batch.first==cursor);
            for (uint32_t old_batch=0;old_batch<scene->batch_count;++old_batch) {
                TextureBatch source=scene->batches[old_batch];
                if (source.texture!=batch.texture) continue;
                for (uint32_t j=source.first;j<source.first+source.count;j+=3) {
                    if (membership[j/3]!=id) continue;
                    for (int v=0;v<3;++v) {
                        TexturedVertex a=part->draw.vertices[cursor++],original=scene->vertices[j+v];
                        assert(a.u==original.u && a.v==original.v && a.color==original.color);
                        assert(fabsf(a.x+part->x-original.x)<0.000001f);
                        assert(fabsf(a.y+part->y-original.y)<0.000001f);
                        assert(fabsf(a.z+part->z-original.z)<0.000001f);
                    }
                }
            }
            assert(cursor==batch.first+batch.count);
        }
        assert(cursor==part->draw.count);
    }
    assert(triangles==310 && batches==6);
}

static void animation_checks(const WheelRig *rig)
{
    const float pi=3.14159265f;
    Car steered={0};
    car_step(&steered,(Controls){.steering=1},1.0f/60);
    assert(steered.wheel_angle>0 && steered.wheel_angle<pi/4);
    for (int i=0;i<30;++i) car_step(&steered,(Controls){.steering=1},1.0f/60);
    assert(fabsf(steered.wheel_angle-pi/4)<0.00001f); /* radians from the actual controller */
    car_reset(&steered);assert(steered.wheel_angle==0);
    for (int heading=0;heading<4;++heading) {
        WheelAnimation animation={0};
        Car old={.heading=heading*pi/2},current=old;
        current.x+=sinf(old.heading)*0.2f;current.z+=cosf(old.heading)*0.2f;
        wheels_step(&animation,rig,&old,&current);
        for (int i=0;i<4;++i) assert(fabsf(animation.roll[i]-0.2f/rig->parts[i+1].radius)<0.00001f);
        wheels_step(&animation,rig,&current,&old);
        for (int i=0;i<4;++i) assert(fabsf(animation.roll[i])<0.00001f);
        current=old;current.wheel_angle=pi/4;current.speed=20;
        wheels_step(&animation,rig,&old,&current); /* steering at rest is not rolling */
        for (int i=0;i<4;++i) assert(fabsf(animation.roll[i])<0.00001f);
    }
    WheelAnimation animation={0};
    /* Circular left turn: wheels on negative X have the shorter path. */
    float angle=-0.04f,radius=5;
    Car old={.wheel_angle=0.4f},current=old;
    current.heading=angle;current.x=radius*(cosf(angle)-1);current.z=-radius*sinf(angle);
    wheels_step(&animation,rig,&old,&current);
    assert(animation.roll[0]>0 && animation.roll[2]>0);
    assert(animation.roll[0]<animation.roll[1] && animation.roll[2]<animation.roll[3]);
    /* Heading wrap must not produce a full backwards revolution. */
    wheels_reset(&animation);old=(Car){.heading=pi-0.01f};current=old;current.heading=-pi+0.01f;
    wheels_step(&animation,rig,&old,&current);
    for (int i=0;i<4;++i) assert(fabsf(animation.roll[i])<0.1f);
    for (int step=0;step<10000;++step) {
        old=current;current.z+=0.01f;
        wheels_step(&animation,rig,&old,&current);
        for (int i=0;i<4;++i) assert(isfinite(animation.roll[i]) && fabsf(animation.roll[i])<=pi+0.00001f);
    }
    wheels_reset(&animation);
    for (int i=0;i<4;++i) assert(animation.roll[i]==0);
}

static void collision_checks(const WheelRig *rig)
{
#define V(x,y,z) {0xffffffff,x,y,z}
    MeshVertex vertices[]={V(-20,0,-20),V(20,0,-20),V(20,0,20),
        V(-20,0,-20),V(20,0,20),V(-20,0,20),
        V(-10,0,0),V(10,0,0),V(10,10,0),V(-10,0,0),V(-10,10,0),V(10,10,0)};
#undef V
    StaticMesh mesh={vertices,12};CollisionWorld world={0};WheelAnimation animation={0};
    assert(collision_build(&world,&mesh)==0);
    Car car={.z=-4};unsigned int stopped=0;
    for (int step=0;step<600;++step) {
        Car previous=car;WheelAnimation before=animation;
        car_step(&car,(Controls){.throttle=1},1.0f/60);
        collision_move(&world,&car,&previous);wheels_step(&animation,rig,&previous,&car);
        float distance=car.z-previous.z;
        for (int i=0;i<4;++i) {
            float expected=remainderf(before.roll[i]+distance/rig->parts[i+1].radius,6.28318531f);
            assert(fabsf(animation.roll[i]-expected)<0.00001f);
        }
        if (distance==0) {++stopped;assert(memcmp(&before,&animation,sizeof(before))==0);}
    }
    assert(stopped>400 && car.speed==0 && car.z<-1.79f);
    collision_free(&world);
}

static void camera_checks(const WheelRig *rig)
{
    /* The PSP transform order is root yaw, pivot, front steering, axle roll. */
    for (int yaw=0;yaw<24;++yaw) {
        float h=yaw*0.261799388f,s=sinf(h),c=cosf(h);ClipCamera camera;
        assert(clip_camera(&camera,(ClipVector){-s*9,5,-c*9},(ClipVector){s*4,1,c*4},60,480.0f/272,0.5f,300)==0);
        for (int steering=-1;steering<=1;++steering) for (int roll=0;roll<12;++roll) {
            float r=roll*0.523598776f,rs=sinf(r),rc=cosf(r);
            for (int id=0;id<5;++id) {
                const WheelPart *part=&rig->parts[id];float a=part->steering?steering*0.785398163f:0;
                float ss=sinf(a),sc=cosf(a);
                for (uint32_t i=0;i<part->draw.count;++i) {
                    TexturedVertex v=part->draw.vertices[i];
                    float y=id?rc*v.y-rs*v.z:v.y,z=id?rs*v.y+rc*v.z:v.z;
                    float x=sc*v.x+ss*z;z=sc*z-ss*v.x;
                    x+=part->x;y+=part->y;z+=part->z;
                    ClipVector w={c*x+s*z,y,c*z-s*x};
                    for (int plane=0;plane<6;++plane) {
                        ClipPlane p=camera.planes[plane];
                        assert(p.normal.x*w.x+p.normal.y*w.y+p.normal.z*w.z+p.offset>0);
                    }
                }
            }
        }
    }
}

static void reject(const unsigned char *data,size_t size,WheelRig *rig,const StaticMesh *mesh,const TexturedScene *scene)
{
    char path[]="/tmp/shar-wheel-invalid-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    FILE *file=fdopen(fd,"wb");assert(file);
    assert(fwrite(data,1,size,file)==size);assert(fclose(file)==0);
    TexturedVertex *saved=rig->parts[0].draw.vertices;
    assert(wheels_load(rig,mesh,scene,path)!=0 && rig->parts[0].draw.vertices==saved);
    assert(unlink(path)==0);
}

static void invalid_checks(const unsigned char *data,size_t size,WheelRig *rig,const StaticMesh *mesh,const TexturedScene *scene)
{
    unsigned char *bad=malloc(size+1);assert(bad);
    for (size_t length=0;length<size;++length) reject(data,length,rig,mesh,scene);
    memcpy(bad,data,size);bad[size]=0;reject(bad,size+1,rig,mesh,scene);
    size_t offsets[]={0,4,8,12,16,36,40,44};
    for (size_t i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i) {
        memcpy(bad,data,size);uint32_t invalid=0xffffffffu;memcpy(bad+offsets[i],&invalid,4);
        reject(bad,size,rig,mesh,scene);
    }
    memcpy(bad,data,size);memcpy(bad+48,bad+44,4);reject(bad,size,rig,mesh,scene); /* duplicate triangle */
    memcpy(bad,data,size);memcpy(bad+188,bad+20,16);reject(bad,size,rig,mesh,scene); /* duplicate quadrant */
    TexturedScene invalid=*scene;invalid.count-=3;
    assert(wheels_load(rig,mesh,&invalid,"assets/compact.swh")!=0);
    free(bad);
}

int main(int argc,char *argv[])
{
    assert(argc==4);StaticMesh mesh={0};TexturedScene scene={0};WheelRig rig={0};size_t size;
    assert(mesh_load(&mesh,argv[1])==0);assert(texture_load(&scene,&mesh,argv[2])==0);
    unsigned char *data=read_file(argv[3],&size);assert(size==692);
    assert(wheels_load(&rig,&mesh,&scene,argv[3])==0);
    geometry_checks(&rig,&scene,data);animation_checks(&rig);collision_checks(&rig);camera_checks(&rig);
    invalid_checks(data,size,&rig,&mesh,&scene);
    assert(wheels_load(&rig,&mesh,&scene,argv[3])==0); /* replacement releases only owned buffers */
    wheels_free(&rig);wheels_free(&rig);free(data);
    assert(scene.textures[1].pixels!=NULL);texture_free(&scene);mesh_free(&mesh);
    puts("Wheel checks passed: original geometry/UVs preserved, four pivots, six batches; forward/reverse, turn arcs, collision stopping, reset, bounded roll, 864 camera poses and malformed metadata");
    return 0;
}
