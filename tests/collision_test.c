#include "../src/collision.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define V(x,y,z) {0xffaaaaaa,x,y,z}
static MeshVertex fixture[]={
    V(-20,0,-20),V(20,0,-20),V(20,0,20),
    V(-20,0,-20),V(20,0,20),V(-20,0,20),
    /* A wall with no thickness, intentionally mixed winding. */
    V(-10,0,0),V(10,0,0),V(10,10,0),
    V(-10,0,0),V(-10,10,0),V(10,10,0),
    /* A roof above the floor must not pull the car up. */
    V(-20,6,-20),V(20,6,-20),V(20,6,20),
    V(-20,6,-20),V(20,6,20),V(-20,6,20)
};

static void fixture_checks(void)
{
    CollisionWorld world={0};
    StaticMesh mesh={fixture,sizeof(fixture)/sizeof(fixture[0])};
    assert(collision_build(&world,&mesh)==0);
    float height=-100;
    assert(collision_ground(&world,0,-4,-1,0.5f,&height));
    assert(height==0);
    Car before={.z=-4,.speed=32},after=before;
    after.z=6;
    assert(collision_move(&world,&after,&before));
    assert(after.z<=-1.79f && after.z>=-2.01f && after.speed==0);
    before=(Car){.z=4,.speed=-10};after=before;after.z=-6;
    assert(collision_move(&world,&after,&before));
    assert(after.z>=1.79f && after.z<=2.01f && after.speed==0);
    /* Repeated acceleration cannot inch through the same wall. */
    before=(Car){.z=-4};
    for (int i=0;i<600;++i) {
        Car previous=before;
        car_step(&before,(Controls){0,1,0,0},1.0f/60);
        collision_move(&world,&before,&previous);
        assert(before.z<=-1.79f);
    }
    /* A path around the finite wall stays open. */
    before=(Car){.x=14,.z=-4,.speed=10};after=before;after.z=4;
    assert(!collision_move(&world,&after,&before));
    assert(after.z==4 && after.y==0);
    /* Rotation also sweeps the front/back hull, rather than clipping it. */
    before=(Car){.z=-1,.heading=1.57079633f};after=before;after.heading=0;
    assert(collision_move(&world,&after,&before));
    assert(after.heading>1.0f);
    /* Missing ground blocks travel off the imported section. */
    before=(Car){.x=19,.z=-10,.speed=10};after=before;after.x=23;
    assert(collision_move(&world,&after,&before));
    assert(after.x<=20.01f && after.speed==0);
    StaticMesh invalid={0};
    CollisionTriangle *saved=world.triangles;
    assert(collision_build(&world,&invalid)!=0 && world.triangles==saved);
    collision_free(&world);

    MeshVertex slope[]={V(-10,-1,-10),V(10,1,-10),V(10,1,10),
                        V(-10,-1,-10),V(10,1,10),V(-10,-1,10)};
    mesh=(StaticMesh){slope,6};
    assert(collision_build(&world,&mesh)==0);
    before=(Car){0};after=before;after.x=2;
    assert(!collision_move(&world,&after,&before));
    assert(fabsf(after.y-0.2f)<0.0001f);
    collision_free(&world);
}

int main(int argc,char *argv[])
{
    assert(argc==2);
    fixture_checks();
    StaticMesh mesh={0};CollisionWorld world={0};
    assert(mesh_load(&mesh,argv[1])==0);
    assert(collision_build(&world,&mesh)==0);
    Car car={.heading=3.14159265f};
    assert(collision_ground(&world,0,0,-1,0.5f,&car.y));
    Car previous=car;
    assert(!collision_move(&world,&car,&previous)); /* imported spawn is clear */
    int blocked=0;
    for (int i=0;i<600;++i) {
        previous=car;
        car_step(&car,(Controls){0,1,0,0},1.0f/60);
        blocked+=collision_move(&world,&car,&previous);
        assert(isfinite(car.x)&&isfinite(car.y)&&isfinite(car.z));
    }
    assert(blocked>0); /* original scenery must actually stop the vehicle */
    printf("Collision checks passed: sweep, reverse, rotation, roof separation, slope, boundary, real scenery (%u triangles, %u grid references)\n",world.triangle_count,world.reference_count);
    printf("Real-scene wall approach stopped at (%.2f, %.2f, %.2f)\n",car.x,car.y,car.z);
    collision_free(&world);mesh_free(&mesh);
    return 0;
}
