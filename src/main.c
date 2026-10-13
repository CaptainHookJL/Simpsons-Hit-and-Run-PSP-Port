#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspiofilemgr.h>
#include <math.h>
#include <string.h>
#include "driving.h"
#include "mesh.h"
#include "collision.h"
#include "texture.h"
#include "clip.h"
#include "wheels.h"
#include "player.h"
#include "vehicle_view.h"

PSP_MODULE_INFO("SHAR Driving Prototype", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8192);

static volatile int running = 1;
static unsigned int __attribute__((aligned(16))) commands[65536];
static unsigned int draw_offset = 0;
static StaticMesh world_mesh = {0};
static CollisionWorld collision_world = {0};
static TexturedScene textured_scene = {0};
static ClippedScene clipped_scene = {0};
static StaticMesh vehicle_mesh = {0};
static TexturedScene vehicle_textures = {0};
static WheelRig wheel_rig = {0};
static WheelAnimation wheel_animation = {0};
static VehicleView vehicle_view = {0};
static int software_clip_active = 0;
static unsigned int measured_fps = 0;
typedef struct { unsigned int color; float x, y, z; } Vertex;

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callback_thread(SceSize size, void *arg)
{
    (void)size; (void)arg;
    int callback = sceKernelCreateCallback("Exit", exit_callback, 0);
    sceKernelRegisterExitCallback(callback);
    sceKernelSleepThreadCB();
    return 0;
}

static void draw_box(float width,float height,float depth,unsigned int color)
{
    static const unsigned char indices[36] = {
        0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
        3,2,6, 3,6,7, 1,5,6, 1,6,2, 0,3,7, 0,7,4
    };
    const float corners[8][3] = {
        {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
        {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
    };
    Vertex *vertices = sceGuGetMemory(sizeof(Vertex) * 36);
    for (int i = 0; i < 36; ++i) {
        const float *corner = corners[indices[i]];
        vertices[i] = (Vertex){color, corner[0] * width * 0.5f,
            corner[1] * height * 0.5f, corner[2] * depth * 0.5f};
    }
    sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                   36, 0, vertices);
}

static void box(float x,float y,float z,float width,float height,float depth,float heading,unsigned int color)
{
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 position={x,y,z};sceGumTranslate(&position);sceGumRotateY(heading);
    draw_box(width,height,depth,color);
}

static void actor_part(const Walker *actor,float x,float y,float offset_y,
                       float width,float height,float depth,float roll,unsigned int color)
{
    sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
    ScePspFVector3 root={actor->x,actor->y,actor->z},pivot={x,y,0},offset={0,offset_y,0};
    sceGumTranslate(&root);sceGumRotateY(actor->heading);
    sceGumTranslate(&pivot);sceGumRotateX(roll);sceGumTranslate(&offset);
    draw_box(width,height,depth,color);
}

static void draw_actor(const Walker *actor)
{
    /* Procedural placeholder; no original Homer mesh/animations are available. */
    float swing=actor->speed>0.01f?sinf(actor->cycle)*(actor->speed>3?0.55f:0.4f):0;
    actor_part(actor,0,1.1f,0,0.48f,0.65f,0.36f,0,0xffeeeeee);
    actor_part(actor,0,1.6f,0,0.36f,0.36f,0.36f,0,0xff40ddff);
    actor_part(actor,-0.3f,1.35f,-0.26f,0.14f,0.52f,0.16f,-swing,0xff40ddff);
    actor_part(actor,0.3f,1.35f,-0.26f,0.14f,0.52f,0.16f,swing,0xff40ddff);
    actor_part(actor,-0.14f,0.8f,-0.38f,0.2f,0.76f,0.23f,swing,0xffc07040);
    actor_part(actor,0.14f,0.8f,-0.38f,0.2f,0.76f,0.23f,-swing,0xffc07040);
}

static void draw_textured(const TexturedScene *scene,const ClippedScene *clipped)
{
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_ALPHA_TEST);
    sceGuAlphaFunc(GU_GREATER,0,0xff);
    sceGuTexMode(GU_PSM_5551,0,0,1);
    sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    sceGuTexWrap(GU_REPEAT,GU_REPEAT);
    sceGuTexScale(1,1);
    sceGuTexOffset(0,0);
    uint32_t previous_texture=0xffffffffu;
    uint32_t batch_count=clipped?clipped->batch_count:scene->batch_count;
    const TextureBatch *batches=clipped?clipped->batches:scene->batches;
    for (uint32_t i=0;i<batch_count;++i) {
        TextureBatch b=batches[i];
        const TexturedVertex *vertices=clipped && !clipped->original[i]?clipped->vertices:scene->vertices;
        if (b.texture!=previous_texture) {
            SceneTexture t=scene->textures[b.texture];
            sceGuTexImage(0,t.width,t.height,t.width,t.pixels);
            sceGuTexFlush();
            previous_texture=b.texture;
        }
        for (uint32_t offset=0;offset<b.count;) {
            uint32_t count=b.count-offset;
            if (count>65535) count=65535;
            sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                           count,0,vertices+b.first+offset);
            offset+=count;
        }
    }
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_TEXTURE_2D);
}

static void flush_textures(const TexturedScene *scene)
{
    sceKernelDcacheWritebackRange(scene->vertices,scene->count*sizeof(TexturedVertex));
    for (uint32_t i=0;i<scene->texture_count;++i) {
        SceneTexture t=scene->textures[i];
        sceKernelDcacheWritebackRange(t.pixels,t.width*t.height*2);
    }
}

static void render(const Player *player,const char *message)
{
    const Car *car=&player->car;
    sceGuStart(GU_DIRECT, commands);
    sceGuClearColor(0xffdbb482);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(60, 480.0f / 272.0f, 0.5f, 300);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    float camera_eye[3],camera_target[3];player_camera(player,camera_eye,camera_target);
    ScePspFVector3 eye={camera_eye[0],camera_eye[1],camera_eye[2]};
    ScePspFVector3 target={camera_target[0],camera_target[1],camera_target[2]};
    ScePspFVector3 up = {0,1,0};
    sceGumLookAt(&eye, &target, &up);
    software_clip_active=0;
    ClipCamera camera;
    int camera_valid=clip_camera(&camera,(ClipVector){eye.x,eye.y,eye.z},
            (ClipVector){target.x,target.y,target.z},60,480.0f/272.0f,0.5f,300)==0;
    if (clipped_scene.vertices && camera_valid &&
            clip_scene(&clipped_scene,&textured_scene,&camera)==0) {
        software_clip_active=1;
        if (clipped_scene.count)
            sceKernelDcacheWritebackRange(clipped_scene.vertices,clipped_scene.count*sizeof(TexturedVertex));
    }
    if (world_mesh.vertices) {
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        /* Persistent aligned mesh memory is flushed once after loading. */
        if (textured_scene.vertices) {
            draw_textured(&textured_scene,software_clip_active?&clipped_scene:NULL);
        } else {
            sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                           world_mesh.count,0,world_mesh.vertices);
        }
    } else {
        box(0,-0.6f,0,240,1,240,0,0xff53864b);
        box(0,-0.04f,0,14,0.1f,230,0,0xff555555);
        box(0,-0.03f,0,230,0.1f,14,0,0xff555555);
        for (int i = -10; i <= 10; ++i) {
            box(0,0.04f,i * 10.0f,0.2f,0.04f,4,0,0xffdddddd);
            box(i * 10.0f,0.05f,0,4,0.04f,0.2f,0,0xffdddddd);
        }
        for (int i = -2; i <= 2; ++i) {
            for (int j = -2; j <= 2; ++j) {
                if (i == 0 || j == 0) continue;
                box(i * 35.0f,4,j * 35.0f,14,8,14,0,0xffa0a0b0);
            }
        }
    }
    int visibility=1;
    if (!player->driving && camera_valid && vehicle_view.draw.vertices)
        visibility=vehicle_view_visibility(&vehicle_view,car,&camera);
    if (vehicle_textures.vertices && visibility==0) {
        if (vehicle_view_clip(&vehicle_view,&vehicle_textures,&wheel_rig,&wheel_animation,car,&camera)==0) {
            sceGumMatrixMode(GU_MODEL);sceGumLoadIdentity();
            if (vehicle_view.draw.count) sceKernelDcacheWritebackRange(vehicle_view.draw.vertices,vehicle_view.draw.count*sizeof(TexturedVertex));
            draw_textured(&vehicle_view.draw,NULL);
        }
    } else if (vehicle_textures.vertices && visibility==1) {
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        ScePspFVector3 position={car->x,car->y,car->z};
        sceGumTranslate(&position);
        sceGumRotateY(car->heading);
        /* Direct draw while wholly visible. Intersecting on-foot views use
           the separate dynamic scratch above; static-world bounds stay fixed. */
        if (wheel_rig.parts[0].draw.vertices) {
            draw_textured(&wheel_rig.parts[0].draw,NULL);
            for (int i=0;i<4;++i) {
                const WheelPart *part=&wheel_rig.parts[i+1];
                sceGumPushMatrix();
                ScePspFVector3 pivot={part->x,part->y,part->z};
                sceGumTranslate(&pivot);
                if (part->steering) sceGumRotateY(-car->wheel_angle);
                sceGumRotateX(wheel_animation.roll[i]);
                draw_textured(&part->draw,NULL);
                sceGumPopMatrix();
            }
        } else {
            draw_textured(&vehicle_textures,NULL);
        }
    } else if (!vehicle_textures.vertices) {
        box(car->x,car->y+0.65f,car->z,1.8f,0.8f,3.6f,car->heading,0xff40c8ff);
        box(car->x,car->y+1.3f,car->z,1.5f,0.65f,1.8f,car->heading,0xff654d32);
    }
    if (!player->driving) draw_actor(&player->walker);
    sceGuFinish();
    sceGuSync(0,0);
    pspDebugScreenSetOffset(draw_offset);
    pspDebugScreenSetXY(0,0);
    float speed=player->driving?fabsf(car->speed):player->walker.speed;
    pspDebugScreenPrintf("SHAR PSP | %3d km/h | %2u FPS              \n", (int)(speed * 3.6f),measured_fps);
    pspDebugScreenPrintf("%s\n",player->driving?"DRIVING | X: gas  Square: brake  O: reverse  ":"ON FOOT | Stick/D-pad: move  Hold X: run     ");
    pspDebugScreenPrintf("%s\n",player->driving?"Triangle: exit car  Start: reset  Home: quit ":"Triangle: enter car  L/R: camera  Start: reset");
    pspDebugScreenPrintf("%s\n",textured_scene.vertices ? "Springfield textures ON                     " :
                        (world_mesh.vertices ? "Textures OFF | Springfield geometry only    " : "Scenery missing/invalid: test-ground fallback"));
    pspDebugScreenPrintf("%s\n",collision_world.triangles ? "Collision ON | terrain following ON         " : "Collision OFF | terrain following OFF       ");
    pspDebugScreenPrintf("%s\n",software_clip_active ? "Software clipping ON                        " : "Software clipping OFF                       ");
    pspDebugScreenPrintf("%s\n",vehicle_textures.vertices ? "Vehicle: CompactA                           " : "Vehicle: placeholder (asset load failed)     ");
    pspDebugScreenPrintf("%s\n",wheel_rig.parts[0].draw.vertices ? "Wheel animation ON                          " : "Wheel animation OFF                         ");
    pspDebugScreenPrintf("%-48s\n",player->driving?"": "Character: animated placeholder");
    pspDebugScreenPrintf("%-48s\n",message?message:(player->driving?"Stop, then press Triangle to get out":
        (player_near_car(player)?"Triangle: enter CompactA":"Walk back to CompactA to enter")));
    sceDisplayWaitVblankStart();
    draw_offset = (unsigned int)sceGuSwapBuffers();
}

int main(int argc, char *argv[])
{
    int thread = sceKernelCreateThread("Callbacks", callback_thread, 0x11, 0x1000, 0, 0);
    if (thread >= 0) sceKernelStartThread(thread,0,0);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    /* Homebrew launchers can differ in their initial working directory. */
    if (argc > 0 && argv[0] && strlen(argv[0]) < 256) {
        char directory[256];
        strcpy(directory,argv[0]);
        char *slash = strrchr(directory,'/');
        if (slash) {
            slash[1] = '\0';
            sceIoChdir(directory);
        }
    }
    if (mesh_load(&world_mesh,"assets/springfield.shm") == 0)
        sceKernelDcacheWritebackRange(world_mesh.vertices,world_mesh.count*sizeof(MeshVertex));
    if (world_mesh.vertices) collision_build(&collision_world,&world_mesh);
    if (texture_load(&textured_scene,&world_mesh,"assets/springfield.stx")==0) {
        clip_create(&clipped_scene,&textured_scene);
        flush_textures(&textured_scene);
    }
    if (mesh_load(&vehicle_mesh,"assets/compact.shm")==0 &&
            texture_load(&vehicle_textures,&vehicle_mesh,"assets/compact.stx")==0) {
        flush_textures(&vehicle_textures);
        if (wheels_load(&wheel_rig,&vehicle_mesh,&vehicle_textures,"assets/compact.swh")==0) {
            for (int i=0;i<5;++i) {
                const TexturedScene *part=&wheel_rig.parts[i].draw;
                sceKernelDcacheWritebackRange(part->vertices,part->count*sizeof(TexturedVertex));
            }
        }
        vehicle_view_create(&vehicle_view,&vehicle_textures,&wheel_rig);
    } else {
        texture_free(&vehicle_textures);
        mesh_free(&vehicle_mesh);
    }
    pspDebugScreenInit();
    sceGuInit();
    sceGuStart(GU_DIRECT,commands);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(480,272,(void *)0x88000,512);
    sceGuDepthBuffer((void *)0x110000,512);
    sceGuOffset(2048-240,2048-136);
    sceGuViewport(2048,2048,480,272);
    sceGuDepthRange(65535,0);
    sceGuScissor(0,0,480,272);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    /* Let the GE clip triangles crossing the view boundary, as in PSPSDK's
       GU samples. Leaving this off can make large scenery triangles pop. */
    sceGuEnable(GU_CLIP_PLANES);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    Player player;player_reset(&player,&collision_world,world_mesh.vertices!=NULL);
    const char *message=NULL;float message_time=0;
    unsigned int previous_buttons = 0;
    unsigned int previous_time = sceKernelGetSystemTimeLow();
    unsigned int fps_start = previous_time;
    unsigned int fps_frames = 0;
    float accumulator = 0;
    while (running) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad,1);
        if ((pad.Buttons & PSP_CTRL_START) && !(previous_buttons & PSP_CTRL_START)) {
            player_reset(&player,&collision_world,world_mesh.vertices!=NULL);
            wheels_reset(&wheel_animation);
            message=NULL;message_time=0;
        } else if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(previous_buttons & PSP_CTRL_TRIANGLE)) {
            PlayerInteraction result=player_interact(&player,&collision_world);
            const char *messages[]={"Entered CompactA","Exited CompactA","Stop the car before getting out",
                "No clear exit: move to an open space","Move closer to CompactA","Entry blocked: go around the obstacle"};
            message=messages[result];message_time=2;
        }
        previous_buttons = pad.Buttons;
        float steering = (pad.Lx - 128) / 127.0f;
        if (fabsf(steering) < 0.15f) steering = 0;
        if (pad.Buttons & PSP_CTRL_LEFT) steering = -1;
        if (pad.Buttons & PSP_CTRL_RIGHT) steering = 1;
        float forward=(128-pad.Ly)/127.0f;
        if (fabsf(forward)<0.15f) forward=0;
        if (pad.Buttons & PSP_CTRL_UP) forward=1;
        if (pad.Buttons & PSP_CTRL_DOWN) forward=-1;
        PlayerControls controls={.car={steering,!!(pad.Buttons & PSP_CTRL_CROSS),
            !!(pad.Buttons & PSP_CTRL_SQUARE),!!(pad.Buttons & PSP_CTRL_CIRCLE)},
            .horizontal=steering,.vertical=forward,.run=!!(pad.Buttons & PSP_CTRL_CROSS),
            .camera_turn=!!(pad.Buttons & PSP_CTRL_LTRIGGER)-!!(pad.Buttons & PSP_CTRL_RTRIGGER)};
        unsigned int now = sceKernelGetSystemTimeLow();
        ++fps_frames;
        unsigned int fps_elapsed = now - fps_start;
        if (fps_elapsed >= 1000000) {
            measured_fps = (unsigned int)(fps_frames * 1000000.0f / fps_elapsed);
            fps_frames = 0;
            fps_start = now;
        }
        float elapsed = (now - previous_time) / 1000000.0f;
        previous_time = now;
        if (message_time>0) {message_time-=elapsed;if (message_time<=0) message=NULL;}
        accumulator += elapsed > 0.1f ? 0.1f : elapsed;
        while (accumulator >= 1.0f / 60.0f) {
            Car previous=player.car;
            player_step(&player,&collision_world,controls,1.0f / 60.0f);
            if (player.driving && wheel_rig.parts[0].draw.vertices) wheels_step(&wheel_animation,&wheel_rig,&previous,&player.car);
            accumulator -= 1.0f / 60.0f;
        }
        render(&player,message);
    }
    sceGuTerm();
    vehicle_view_free(&vehicle_view);
    wheels_free(&wheel_rig);
    texture_free(&vehicle_textures);
    mesh_free(&vehicle_mesh);
    clip_free(&clipped_scene);
    texture_free(&textured_scene);
    collision_free(&collision_world);
    mesh_free(&world_mesh);
    sceKernelExitGame();
    return 0;
}
