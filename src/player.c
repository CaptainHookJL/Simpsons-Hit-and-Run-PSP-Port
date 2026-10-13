/* Player-input response adapted from Character::UpdateDesiredDirAndSpeed and
   Character::GetInputScale in Svxy/The-Simpsons-Hit-and-Run, commit
   0cf35df0a921a803a69d0da4c2aca02aa2543f03 (game/code/worldsim/character).
   Movement, capsule collision, transitions and camera are replacement code.
   No original puppet/character physics or animation middleware is linked. */
#include "player.h"
#include <math.h>

static float clamp(float x,float low,float high) {return x<low?low:(x>high?high:x);}

static int overlaps_car(const Player *player,float x,float y,float z)
{
    if (y>=player->car.y+1.65f || y+PLAYER_HEIGHT<=player->car.y) return 0;
    float dx=x-player->car.x,dz=z-player->car.z;
    float c=cosf(player->car.heading),s=sinf(player->car.heading);
    float local_x=c*dx-s*dz,local_z=s*dx+c*dz;
    float ex=local_x-clamp(local_x,-0.9f,0.9f),ez=local_z-clamp(local_z,-1.8f,1.8f);
    return ex*ex+ez*ez<PLAYER_RADIUS*PLAYER_RADIUS;
}

static int stand(Player *player,CollisionWorld *world,float x,float z,float reference,float *y)
{
    return collision_actor_fit(world,x,z,reference,PLAYER_RADIUS,PLAYER_HEIGHT,y) && !overlaps_car(player,x,*y,z);
}

static int clear_path(CollisionWorld *world,float x0,float z0,float x1,float z1,float reference)
{
    int steps=(int)ceilf(hypotf(x1-x0,z1-z0)/0.1f);if (steps<1) steps=1;
    for (int i=0;i<=steps;++i) {
        float fraction=(float)i/steps;
        if (!collision_actor_fit(world,x0+(x1-x0)*fraction,z0+(z1-z0)*fraction,
                reference,PLAYER_RADIUS,PLAYER_HEIGHT,&reference)) return 0;
    }
    return 1;
}

void player_reset(Player *player,CollisionWorld *world,int original_scenery)
{
    *player=(Player){.driving=1};
    if (original_scenery) player->car.heading=3.14159265f;
    collision_ground(world,0,0,-1,0.5f,&player->car.y);
    player->walker.camera_heading=player->car.heading;
}

int player_near_car(const Player *player)
{
    float dx=player->walker.x-player->car.x,dz=player->walker.z-player->car.z;
    return dx*dx+dz*dz<=3.0f*3.0f && fabsf(player->walker.y-player->car.y)<0.75f;
}

PlayerInteraction player_interact(Player *player,CollisionWorld *world)
{
    if (!player->driving) {
        if (!player_near_car(player)) return PLAYER_TOO_FAR;
        if (!clear_path(world,player->walker.x,player->walker.z,player->car.x,player->car.z,player->walker.y))
            return PLAYER_ENTRY_BLOCKED;
        player->car.speed=0;player->driving=1;player->walker.speed=0;
        return PLAYER_ENTERED;
    }
    if (fabsf(player->car.speed)>0.5f) return PLAYER_TOO_FAST;
    /* Try both sides, then forward/rear side positions; never teleport through
       a wall or place the actor inside the car or off loaded ground. */
    float c=cosf(player->car.heading),s=sinf(player->car.heading);
    const float longitudinal[3]={0,-1.2f,1.2f};
    for (int along=0;along<3;++along) for (int side=-1;side<=1;side+=2) {
        float local_x=side*1.65f,local_z=longitudinal[along];
        float x=player->car.x+c*local_x+s*local_z,z=player->car.z+c*local_z-s*local_x,y;
        if (stand(player,world,x,z,player->car.y,&y) &&
            clear_path(world,player->car.x,player->car.z,x,z,player->car.y)) {
            player->walker=(Walker){.x=x,.y=y,.z=z,.heading=player->car.heading,.camera_heading=player->car.heading};
            player->car.speed=0;player->driving=0;
            return PLAYER_EXITED;
        }
    }
    return PLAYER_EXIT_BLOCKED;
}

void player_step(Player *player,CollisionWorld *world,PlayerControls input,float dt)
{
    if (!isfinite(dt)||dt<=0) return;
    dt=fminf(dt,0.05f);
    if (player->driving) {
        Car previous=player->car;
        car_step(&player->car,input.car,dt);
        collision_move(world,&player->car,&previous);
        return;
    }
    Walker *actor=&player->walker;
    actor->camera_heading=remainderf(actor->camera_heading+clamp(input.camera_turn,-1,1)*1.8f*dt,6.28318531f);
    float horizontal=clamp(input.horizontal,-1,1),vertical=clamp(input.vertical,-1,1);
    float magnitude=hypotf(horizontal,vertical);
    if (magnitude<0.0001f) {actor->speed=0;return;}
    float c=cosf(actor->camera_heading),s=sinf(actor->camera_heading);
    float direction_x=(-c*horizontal+s*vertical)/magnitude,direction_z=(s*horizontal+c*vertical)/magnitude;
    /* Character::UpdateDesiredDirAndSpeed squares player input magnitude,
       divides by GetInputScale() (0.69), and caps the desired speed at one.
       The 2.5/5.0 m/s walk/run limits here are prototype tuning. */
    float response=fminf(magnitude*magnitude/0.69f,1);
    float speed=(input.run?5.0f:2.5f)*response;
    float turn=remainderf(atan2f(direction_x,direction_z)-actor->heading,6.28318531f);
    actor->heading=remainderf(actor->heading+clamp(turn,-10*dt,10*dt),6.28318531f);
    float dx=direction_x*speed*dt,dz=direction_z*speed*dt;
    int steps=(int)ceilf(hypotf(dx,dz)/0.1f);if (steps<1) steps=1;
    dx/=steps;dz/=steps;
    float old_x=actor->x,old_z=actor->z;
    for (int i=0;i<steps;++i) {
        float y;
        if (stand(player,world,actor->x+dx,actor->z+dz,actor->y,&y)) {
            actor->x+=dx;actor->z+=dz;actor->y=y;
        } else if (fabsf(dx)>0.000001f && stand(player,world,actor->x+dx,actor->z,actor->y,&y)) {
            actor->x+=dx;actor->y=y;
        } else if (fabsf(dz)>0.000001f && stand(player,world,actor->x,actor->z+dz,actor->y,&y)) {
            actor->z+=dz;actor->y=y;
        }
    }
    float distance=hypotf(actor->x-old_x,actor->z-old_z);
    actor->speed=distance/dt;
    actor->cycle=remainderf(actor->cycle+distance*4.0f,6.28318531f);
}

void player_camera(const Player *player,float eye[3],float target[3])
{
    float x=player->driving?player->car.x:player->walker.x;
    float y=player->driving?player->car.y:player->walker.y;
    float z=player->driving?player->car.z:player->walker.z;
    float heading=player->driving?player->car.heading:player->walker.camera_heading;
    float back=player->driving?9:6,height=player->driving?5:3.5f,forward=player->driving?4:1;
    eye[0]=x-sinf(heading)*back;eye[1]=y+height;eye[2]=z-cosf(heading)*back;
    target[0]=x+sinf(heading)*forward;target[1]=y+1;target[2]=z+cosf(heading)*forward;
}
