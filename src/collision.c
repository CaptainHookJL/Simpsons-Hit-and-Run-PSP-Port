#include "collision.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GRID_SIZE 32
#define MAX_REFERENCES 262144u
#define CAR_RADIUS 0.9f
#define HULL_OFFSET 0.9f
#define HULL_HEIGHT 0.9f

typedef struct {float x,y,z;} Vec3;
struct CollisionTriangle {
    Vec3 a,ab,ac;
    float dot_ab,dot_ac,dot_bc,inverse_xz,min_y,max_y;
    uint32_t stamp;
    int kind; /* 0: degenerate, 1: traversable slope, 2: obstacle */
};

static Vec3 add(Vec3 a,Vec3 b) {return (Vec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vec3 sub(Vec3 a,Vec3 b) {return (Vec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vec3 mul(Vec3 a,float s) {return (Vec3){a.x*s,a.y*s,a.z*s};}
static float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec3 point(MeshVertex v) {return (Vec3){v.x,v.y,v.z};}

static int cell(float value,float minimum,float size)
{
    int index=(int)floorf((value-minimum)/size);
    return index<0?0:(index>=GRID_SIZE?GRID_SIZE-1:index);
}

static void triangle_cells(const CollisionWorld *world,const CollisionTriangle *t,
                           int *x0,int *z0,int *x1,int *z1)
{
    Vec3 b=add(t->a,t->ab),c=add(t->a,t->ac);
    *x0=cell(fminf(t->a.x,fminf(b.x,c.x)),world->min_x,world->cell_size);
    *x1=cell(fmaxf(t->a.x,fmaxf(b.x,c.x)),world->min_x,world->cell_size);
    *z0=cell(fminf(t->a.z,fminf(b.z,c.z)),world->min_z,world->cell_size);
    *z1=cell(fmaxf(t->a.z,fmaxf(b.z,c.z)),world->min_z,world->cell_size);
}

int collision_build(CollisionWorld *world,const StaticMesh *mesh)
{
    if (!mesh || !mesh->vertices || !mesh->count || mesh->count%3 || mesh->count>49152)
        return -1;
    CollisionWorld next={0};
    next.triangle_count=mesh->count/3;
    next.triangles=calloc(next.triangle_count,sizeof(*next.triangles));
    if (!next.triangles) return -1;
    float max_x=mesh->vertices[0].x,max_z=mesh->vertices[0].z;
    next.min_x=max_x; next.min_z=max_z;
    for (uint32_t i=0;i<mesh->count;++i) {
        MeshVertex v=mesh->vertices[i];
        if (!isfinite(v.x)||!isfinite(v.y)||!isfinite(v.z) ||
            fabsf(v.x)>10000||fabsf(v.y)>10000||fabsf(v.z)>10000) goto failure;
        next.min_x=fminf(next.min_x,v.x); max_x=fmaxf(max_x,v.x);
        next.min_z=fminf(next.min_z,v.z); max_z=fmaxf(max_z,v.z);
    }
    next.cell_size=fmaxf(1.0f,fmaxf(max_x-next.min_x,max_z-next.min_z)/GRID_SIZE);
    for (uint32_t i=0;i<next.triangle_count;++i) {
        CollisionTriangle *t=&next.triangles[i];
        t->a=point(mesh->vertices[i*3]);
        Vec3 b=point(mesh->vertices[i*3+1]),c=point(mesh->vertices[i*3+2]);
        t->ab=sub(b,t->a); t->ac=sub(c,t->a);
        t->dot_ab=dot(t->ab,t->ab);t->dot_ac=dot(t->ac,t->ac);t->dot_bc=dot(t->ab,t->ac);
        Vec3 n={t->ab.y*t->ac.z-t->ab.z*t->ac.y,
                t->ab.z*t->ac.x-t->ab.x*t->ac.z,
                t->ab.x*t->ac.y-t->ab.y*t->ac.x};
        float norm=dot(n,n);
        if (norm<1e-10f) continue;
        t->kind=n.y*n.y>=0.4225f*norm?1:2; /* >=0.65 Y normal magnitude */
        float determinant=t->ab.x*t->ac.z-t->ab.z*t->ac.x;
        t->inverse_xz=t->kind==1?1.0f/determinant:0;
        t->min_y=fminf(t->a.y,fminf(b.y,c.y));
        t->max_y=fmaxf(t->a.y,fmaxf(b.y,c.y));
        int x0,z0,x1,z1;
        triangle_cells(&next,t,&x0,&z0,&x1,&z1);
        uint32_t references=(uint32_t)((x1-x0+1)*(z1-z0+1));
        if (next.reference_count>MAX_REFERENCES-references) goto failure;
        next.reference_count+=references;
        for (int z=z0;z<=z1;++z) for (int x=x0;x<=x1;++x)
            ++next.offsets[z*GRID_SIZE+x+1];
    }
    if (!next.reference_count) goto failure;
    for (int i=1;i<=GRID_SIZE*GRID_SIZE;++i) next.offsets[i]+=next.offsets[i-1];
    next.references=malloc(next.reference_count*sizeof(*next.references));
    if (!next.references) goto failure;
    uint32_t cursor[1024];
    memcpy(cursor,next.offsets,sizeof(cursor));
    for (uint32_t i=0;i<next.triangle_count;++i) {
        if (!next.triangles[i].kind) continue;
        int x0,z0,x1,z1;
        triangle_cells(&next,&next.triangles[i],&x0,&z0,&x1,&z1);
        for (int z=z0;z<=z1;++z) for (int x=x0;x<=x1;++x)
            next.references[cursor[z*GRID_SIZE+x]++]=i;
    }
    collision_free(world);
    *world=next;
    return 0;
failure:
    collision_free(&next);
    return -1;
}

void collision_free(CollisionWorld *world)
{
    free(world->triangles);free(world->references);
    memset(world,0,sizeof(*world));
}

int collision_ground(const CollisionWorld *world,float x,float z,
                     float minimum,float maximum,float *height)
{
    if (!world->triangles || !isfinite(x)||!isfinite(z)||minimum>maximum) return 0;
    int index=cell(z,world->min_z,world->cell_size)*GRID_SIZE+cell(x,world->min_x,world->cell_size);
    int found=0;
    float best=minimum;
    for (uint32_t i=world->offsets[index];i<world->offsets[index+1];++i) {
        const CollisionTriangle *t=&world->triangles[world->references[i]];
        if (t->kind!=1 || t->max_y<minimum || t->min_y>maximum) continue;
        float dx=x-t->a.x,dz=z-t->a.z;
        float u=(dx*t->ac.z-dz*t->ac.x)*t->inverse_xz;
        float v=(t->ab.x*dz-t->ab.z*dx)*t->inverse_xz;
        if (u<-0.0001f||v<-0.0001f||u+v>1.0001f) continue;
        float y=t->a.y+u*t->ab.y+v*t->ac.y;
        if (y>=minimum && y<=maximum && (!found||y>best)) {best=y;found=1;}
    }
    if (found) *height=best;
    return found;
}

/* Closest point handles face, edge, and vertex contacts, independent of winding. */
static Vec3 closest_point(const CollisionTriangle *t,Vec3 p)
{
    Vec3 ap=sub(p,t->a);
    float d1=dot(t->ab,ap),d2=dot(t->ac,ap);
    if (d1<=0 && d2<=0) return t->a;
    float d3=d1-t->dot_ab,d4=d2-t->dot_bc;
    if (d3>=0 && d4<=d3) return add(t->a,t->ab);
    float vc=d1*d4-d3*d2;
    if (vc<=0 && d1>=0 && d3<=0) return add(t->a,mul(t->ab,d1/(d1-d3)));
    float d5=d1-t->dot_bc,d6=d2-t->dot_ac;
    if (d6>=0 && d5<=d6) return add(t->a,t->ac);
    float vb=d5*d2-d1*d6;
    if (vb<=0 && d2>=0 && d6<=0) return add(t->a,mul(t->ac,d2/(d2-d6)));
    float va=d3*d6-d5*d4;
    if (va<=0 && d4-d3>=0 && d5-d6>=0)
        return add(add(t->a,t->ab),mul(sub(t->ac,t->ab),(d4-d3)/((d4-d3)+(d5-d6))));
    float inverse=1.0f/(va+vb+vc);
    return add(t->a,add(mul(t->ab,vb*inverse),mul(t->ac,vc*inverse)));
}

static int overlaps(CollisionWorld *world,const Car *car)
{
    const float extent=CAR_RADIUS+HULL_OFFSET;
    int x0=cell(car->x-extent,world->min_x,world->cell_size);
    int x1=cell(car->x+extent,world->min_x,world->cell_size);
    int z0=cell(car->z-extent,world->min_z,world->cell_size);
    int z1=cell(car->z+extent,world->min_z,world->cell_size);
    if (++world->stamp==0) {
        for (uint32_t i=0;i<world->triangle_count;++i) world->triangles[i].stamp=0;
        world->stamp=1;
    }
    float offset_x=sinf(car->heading)*HULL_OFFSET,offset_z=cosf(car->heading)*HULL_OFFSET;
    Vec3 centers[2]={{car->x+offset_x,car->y+HULL_HEIGHT,car->z+offset_z},
                    {car->x-offset_x,car->y+HULL_HEIGHT,car->z-offset_z}};
    for (int z=z0;z<=z1;++z) for (int x=x0;x<=x1;++x) {
        int index=z*GRID_SIZE+x;
        for (uint32_t i=world->offsets[index];i<world->offsets[index+1];++i) {
            CollisionTriangle *t=&world->triangles[world->references[i]];
            if (t->kind!=2 || t->stamp==world->stamp) continue;
            t->stamp=world->stamp;
            if (t->min_y>car->y+HULL_HEIGHT+CAR_RADIUS || t->max_y<car->y+HULL_HEIGHT-CAR_RADIUS) continue;
            for (int j=0;j<2;++j) {
                Vec3 delta=sub(centers[j],closest_point(t,centers[j]));
                if (dot(delta,delta)<CAR_RADIUS*CAR_RADIUS-0.00001f) return 1;
            }
        }
    }
    return 0;
}

int collision_move(CollisionWorld *world,Car *car,const Car *previous)
{
    if (!world->triangles) return 0;
    float dx=car->x-previous->x,dz=car->z-previous->z;
    float rotation=remainderf(car->heading-previous->heading,6.28318531f);
    /* Sweep translation and hull rotation in <=0.2 m increments. A car at the
       prototype's maximum speed moves 0.54 m per normal physics tick. */
    float distance=sqrtf(dx*dx+dz*dz)+fabsf(rotation)*HULL_OFFSET;
    int steps=(int)ceilf(distance/0.2f);
    if (steps<1) steps=1;
    if (steps>128) {car->x=previous->x;car->z=previous->z;car->y=previous->y;car->heading=previous->heading;car->speed=0;return 1;}
    Car last=*previous;
    for (int i=1;i<=steps;++i) {
        float fraction=(float)i/steps;
        Car candidate=*car;
        candidate.x=previous->x+dx*fraction;
        candidate.z=previous->z+dz*fraction;
        candidate.heading=previous->heading+rotation*fraction;
        if (!collision_ground(world,candidate.x,candidate.z,last.y-1.0f,last.y+0.35f,&candidate.y) ||
            overlaps(world,&candidate)) {
            car->x=last.x;car->z=last.z;car->y=last.y;car->heading=last.heading;car->speed=0;
            return 1;
        }
        last=candidate;
    }
    car->y=last.y;
    return 0;
}
