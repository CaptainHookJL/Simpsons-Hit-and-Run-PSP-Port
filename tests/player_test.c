#include "../src/player.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define V(x,y,z) {0xffffffff,x,y,z}
static MeshVertex floor_wall[]={V(-20,0,-20),V(20,0,-20),V(20,0,20),
    V(-20,0,-20),V(20,0,20),V(-20,0,20),
    V(-10,0,0),V(10,0,0),V(10,3,0),V(-10,0,0),V(-10,3,0),V(10,3,0)};

static Player walker(float x,float z)
{
    return (Player){.car={.x=15,.z=15},.walker={.x=x,.z=z}};
}

static void movement_checks(void)
{
    CollisionWorld empty={0};Player walk=walker(0,0),run=walk,diagonal=walk;
    for (int i=0;i<60;++i) {
        player_step(&walk,&empty,(PlayerControls){.vertical=1},1.0f/60);
        player_step(&run,&empty,(PlayerControls){.vertical=1,.run=1},1.0f/60);
        player_step(&diagonal,&empty,(PlayerControls){.horizontal=1,.vertical=1,.run=1},1.0f/60);
    }
    assert(fabsf(walk.walker.z-2.5f)<0.0001f && fabsf(run.walker.z-5)<0.0001f);
    assert(fabsf(hypotf(diagonal.walker.x,diagonal.walker.z)-5)<0.0001f);
    assert(diagonal.walker.x<0 && diagonal.walker.z>0); /* camera-right is -X */
    Player gentle=walker(0,0);
    player_step(&gentle,&empty,(PlayerControls){.vertical=0.5f},0.05f);
    assert(fabsf(gentle.walker.z-2.5f*0.25f/0.69f*0.05f)<0.00001f);
    Player before=run;player_step(&run,&empty,(PlayerControls){.run=1,.vertical=1},0);
    assert(memcmp(&before,&run,sizeof(run))==0);
    float phase=run.walker.cycle;
    player_step(&run,&empty,(PlayerControls){0},1.0f/60);
    assert(run.walker.speed==0 && run.walker.cycle==phase);
    Car parked=run.car;
    player_step(&run,&empty,(PlayerControls){.car={.throttle=1},.camera_turn=1},0.05f);
    assert(memcmp(&parked,&run.car,sizeof(parked))==0 && run.walker.camera_heading>0);
    run=walker(109.6f,0);
    for (int i=0;i<100;++i) player_step(&run,&empty,(PlayerControls){.horizontal=-1,.run=1},1.0f/60);
    assert(run.walker.x<=110-PLAYER_RADIUS && run.walker.speed==0);
    float eye[3],target[3];player_camera(&run,eye,target);
    assert(fabsf(eye[0]-run.walker.x)<0.0001f && fabsf(eye[2]-run.walker.z+6)<0.0001f);
    assert(fabsf(target[2]-run.walker.z-1)<0.0001f);
    player_reset(&run,&empty,0);assert(run.driving && run.car.x==0 && run.car.speed==0);
    player_camera(&run,eye,target);assert(eye[2]==-9 && eye[1]==5 && target[2]==4);
}

static void collision_checks(void)
{
    CollisionWorld world={0};StaticMesh mesh={floor_wall,12};float ground;
    assert(collision_build(&world,&mesh)==0);
    assert(collision_actor_fit(&world,0,-3,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground) && ground==0);
    assert(!collision_actor_fit(&world,0,-0.2f,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    assert(!collision_actor_fit(&world,19.9f,-3,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    assert(!collision_actor_fit(&world,0,-3,0,0,PLAYER_HEIGHT,&ground));
    Player p=walker(0,-4);
    for (int i=0;i<600;++i) player_step(&p,&world,(PlayerControls){.vertical=1,.run=1},1.0f/60);
    assert(p.walker.z<=-PLAYER_RADIUS+0.0001f && p.walker.speed==0);
    float phase=p.walker.cycle;
    for (int i=0;i<30;++i) player_step(&p,&world,(PlayerControls){.vertical=1,.run=1},1.0f/60);
    assert(p.walker.cycle==phase); /* pushing a wall does not animate walking */
    p=walker(0,-0.4f);
    for (int i=0;i<120;++i) player_step(&p,&world,(PlayerControls){.horizontal=-1,.vertical=1},1.0f/60);
    assert(p.walker.x>3 && p.walker.z<=-PLAYER_RADIUS+0.0001f); /* slide along wall */
    collision_free(&world);
    /* Thin obstacle at waist height: capsule endpoints alone would miss it. */
    MeshVertex waist[]={V(-20,0,-20),V(20,0,-20),V(20,0,20),V(-20,0,-20),V(20,0,20),V(-20,0,20),
        V(-2,0.85f,0),V(2,0.85f,0),V(2,1.05f,0),V(-2,0.85f,0),V(-2,1.05f,0),V(2,1.05f,0)};
    mesh=(StaticMesh){waist,12};assert(collision_build(&world,&mesh)==0);
    assert(!collision_actor_fit(&world,0,-0.3f,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    collision_free(&world);
    MeshVertex roof[]={V(-20,0,-20),V(20,0,-20),V(20,0,20),V(-20,0,-20),V(20,0,20),V(-20,0,20),
        V(-2,1.5f,-2),V(2,1.5f,-2),V(2,1.5f,2),V(-2,1.5f,-2),V(2,1.5f,2),V(-2,1.5f,2)};
    mesh=(StaticMesh){roof,12};assert(collision_build(&world,&mesh)==0);
    assert(!collision_actor_fit(&world,0,0,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    assert(collision_actor_fit(&world,5,0,0,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    collision_free(&world);
    MeshVertex slope[]={V(-20,-2,-20),V(20,2,-20),V(20,2,20),V(-20,-2,-20),V(20,2,20),V(-20,-2,20)};
    mesh=(StaticMesh){slope,6};assert(collision_build(&world,&mesh)==0);p=walker(0,0);
    for (int i=0;i<120;++i) player_step(&p,&world,(PlayerControls){.horizontal=-1},1.0f/60);
    assert(fabsf(p.walker.y-p.walker.x*0.1f)<0.0001f && p.walker.x>4.9f);
    collision_free(&world);
}

static void transition_checks(void)
{
    CollisionWorld world={0};StaticMesh mesh={floor_wall,12};
    assert(collision_build(&world,&mesh)==0);
    Player p={.driving=1,.car={.z=-4,.speed=4}},saved=p;
    assert(player_interact(&p,&world)==PLAYER_TOO_FAST && memcmp(&p,&saved,sizeof(p))==0);
    p.car.speed=0;assert(player_interact(&p,&world)==PLAYER_EXITED && !p.driving);
    assert(fabsf(p.walker.x+1.65f)<0.0001f && p.walker.z==-4 && player_near_car(&p));
    for (int i=0;i<200;++i) player_step(&p,&world,(PlayerControls){.horizontal=-1},1.0f/60);
    assert(p.walker.x<=-0.9f-PLAYER_RADIUS+0.0001f); /* parked car is solid */
    assert(player_interact(&p,&world)==PLAYER_ENTERED && p.driving && p.car.z==-4);
    assert(player_interact(&p,&world)==PLAYER_EXITED);
    p.walker.x=8;assert(player_interact(&p,&world)==PLAYER_TOO_FAR && !p.driving);
    collision_free(&world);
    MeshVertex alley[]={V(-20,0,-20),V(20,0,-20),V(20,0,20),V(-20,0,-20),V(20,0,20),V(-20,0,20),
        V(-1.2f,0,-8),V(-1.2f,3,-8),V(-1.2f,3,-1),V(-1.2f,0,-8),V(-1.2f,3,-1),V(-1.2f,0,-1),
        V(1.2f,0,-8),V(1.2f,3,-8),V(1.2f,3,-1),V(1.2f,0,-8),V(1.2f,3,-1),V(1.2f,0,-1)};
    mesh=(StaticMesh){alley,18};assert(collision_build(&world,&mesh)==0);
    p=(Player){.driving=1,.car={.z=-4}};saved=p;
    assert(player_interact(&p,&world)==PLAYER_EXIT_BLOCKED && memcmp(&p,&saved,sizeof(p))==0);
    p.driving=0;p.walker=(Walker){.x=1.8f,.z=-4};
    assert(player_interact(&p,&world)==PLAYER_ENTRY_BLOCKED && !p.driving); /* no entry through a wall */
    collision_free(&world);
    mesh.count=12;assert(collision_build(&world,&mesh)==0);p=(Player){.driving=1,.car={.z=-4}};
    assert(player_interact(&p,&world)==PLAYER_EXITED && p.walker.x>1.6f); /* choose clear side */
    collision_free(&world);
}

int main(int argc,char *argv[])
{
    assert(argc==2);movement_checks();collision_checks();transition_checks();
    StaticMesh mesh={0};CollisionWorld world={0};Player p;
    assert(mesh_load(&mesh,argv[1])==0 && collision_build(&world,&mesh)==0);
    player_reset(&p,&world,1);assert(p.driving && fabsf(p.car.heading-3.14159265f)<0.00001f);
    for (int i=0;i<20;++i) {
        assert(player_interact(&p,&world)==PLAYER_EXITED);
        assert(player_interact(&p,&world)==PLAYER_ENTERED);
    }
    assert(player_interact(&p,&world)==PLAYER_EXITED);
    for (int i=0;i<1200;++i) {
        Car parked=p.car;
        player_step(&p,&world,(PlayerControls){.vertical=1,.run=1,.camera_turn=i%300<150?0.2f:-0.2f},1.0f/60);
        assert(memcmp(&parked,&p.car,sizeof(parked))==0);
        assert(isfinite(p.walker.x)&&isfinite(p.walker.y)&&isfinite(p.walker.z));
        float ground;assert(collision_actor_fit(&world,p.walker.x,p.walker.z,p.walker.y,PLAYER_RADIUS,PLAYER_HEIGHT,&ground));
    }
    player_reset(&p,&world,1);assert(p.driving && p.car.x==0 && p.car.z==0 && p.walker.cycle==0);
    collision_free(&world);mesh_free(&mesh);
    puts("Player checks passed: walking/running, camera-relative input, input response, diagonal speed, wall sliding, capsule waist/ceiling contacts, slope/edge, parked-car collision, safe entry/exit and real Springfield exploration");
    return 0;
}
