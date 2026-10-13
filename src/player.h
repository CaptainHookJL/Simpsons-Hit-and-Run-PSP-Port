#ifndef PSP_PLAYER_H
#define PSP_PLAYER_H
#include "collision.h"

#define PLAYER_RADIUS 0.35f
#define PLAYER_HEIGHT 1.8f
typedef struct {float x,y,z,heading,camera_heading,speed,cycle;} Walker;
typedef struct {Car car;Walker walker;int driving;} Player;
typedef struct {Controls car;float horizontal,vertical,camera_turn;int run;} PlayerControls;
typedef enum {PLAYER_ENTERED,PLAYER_EXITED,PLAYER_TOO_FAST,PLAYER_EXIT_BLOCKED,PLAYER_TOO_FAR,PLAYER_ENTRY_BLOCKED} PlayerInteraction;

void player_reset(Player *player,CollisionWorld *world,int original_scenery);
void player_step(Player *player,CollisionWorld *world,PlayerControls input,float dt);
PlayerInteraction player_interact(Player *player,CollisionWorld *world);
int player_near_car(const Player *player);
/* Camera pose for both render and software clipping. */
void player_camera(const Player *player,float eye[3],float target[3]);
#endif
