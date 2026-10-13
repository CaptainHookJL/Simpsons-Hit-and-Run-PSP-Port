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

PSP_MODULE_INFO("SHAR Driving Prototype", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(8192);

static volatile int running = 1;
static unsigned int __attribute__((aligned(16))) commands[65536];
static unsigned int draw_offset = 0;
static StaticMesh world_mesh = {0};
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

static void box(float x, float y, float z, float width, float height,
                float depth, float heading, unsigned int color)
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
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 position = {x,y,z};
    sceGumTranslate(&position);
    sceGumRotateY(heading);
    sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                   36, 0, vertices);
}

static void render(const Car *car)
{
    sceGuStart(GU_DIRECT, commands);
    sceGuClearColor(0xffdbb482);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(60, 480.0f / 272.0f, 0.5f, 300);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    ScePspFVector3 eye = {car->x - sinf(car->heading) * 9, 5,
                          car->z - cosf(car->heading) * 9};
    ScePspFVector3 target = {car->x + sinf(car->heading) * 4, 1,
                             car->z + cosf(car->heading) * 4};
    ScePspFVector3 up = {0,1,0};
    sceGumLookAt(&eye, &target, &up);
    if (world_mesh.vertices) {
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        /* Persistent aligned mesh memory is flushed once after loading. */
        sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                       world_mesh.count,0,world_mesh.vertices);
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
    box(car->x,0.65f,car->z,1.8f,0.8f,3.6f,car->heading,0xff40c8ff);
    box(car->x,1.3f,car->z,1.5f,0.65f,1.8f,car->heading,0xff654d32);
    sceGuFinish();
    sceGuSync(0,0);
    pspDebugScreenSetOffset(draw_offset);
    pspDebugScreenSetXY(0,0);
    pspDebugScreenPrintf("SHAR PSP | %3d km/h | %2u FPS              \n", (int)(fabsf(car->speed) * 3.6f),measured_fps);
    pspDebugScreenPrintf("X: gas  Square: brake  Circle: reverse      \n");
    pspDebugScreenPrintf("Stick/D-pad: steer  Start: reset  Home: exit \n");
    pspDebugScreenPrintf("%s\n",world_mesh.vertices ? "Springfield geometry sample (no textures)   " : "Scenery missing/invalid: test-ground fallback");
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
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    Car car;
    car_reset(&car);
    if (world_mesh.vertices) car.heading = 3.14159265f;
    unsigned int previous_buttons = 0;
    unsigned int previous_time = sceKernelGetSystemTimeLow();
    unsigned int fps_start = previous_time;
    unsigned int fps_frames = 0;
    float accumulator = 0;
    while (running) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad,1);
        if ((pad.Buttons & PSP_CTRL_START) && !(previous_buttons & PSP_CTRL_START)) {
            car_reset(&car);
            if (world_mesh.vertices) car.heading = 3.14159265f;
        }
        previous_buttons = pad.Buttons;
        float steering = (pad.Lx - 128) / 127.0f;
        if (fabsf(steering) < 0.15f) steering = 0;
        if (pad.Buttons & PSP_CTRL_LEFT) steering = -1;
        if (pad.Buttons & PSP_CTRL_RIGHT) steering = 1;
        Controls controls = {steering, !!(pad.Buttons & PSP_CTRL_CROSS),
            !!(pad.Buttons & PSP_CTRL_SQUARE), !!(pad.Buttons & PSP_CTRL_CIRCLE)};
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
        accumulator += elapsed > 0.1f ? 0.1f : elapsed;
        while (accumulator >= 1.0f / 60.0f) {
            car_step(&car,controls,1.0f / 60.0f);
            accumulator -= 1.0f / 60.0f;
        }
        render(&car);
    }
    sceGuTerm();
    mesh_free(&world_mesh);
    sceKernelExitGame();
    return 0;
}
