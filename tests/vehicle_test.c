#include "../src/clip.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
    assert(argc==3);
    StaticMesh mesh={0};TexturedScene scene={0};
    assert(mesh_load(&mesh,argv[1])==0);
    assert(texture_load(&scene,&mesh,argv[2])==0);
    assert(mesh.count==930 && scene.texture_count==3 && scene.batch_count==2);
    float low_x=100,high_x=-100,low_y=100,high_y=-100,low_z=100,high_z=-100;
    for (uint32_t i=0;i<mesh.count;++i) {
        MeshVertex v=mesh.vertices[i];
        low_x=fminf(low_x,v.x);high_x=fmaxf(high_x,v.x);
        low_y=fminf(low_y,v.y);high_y=fmaxf(high_y,v.y);
        low_z=fminf(low_z,v.z);high_z=fmaxf(high_z,v.z);
    }
    assert(fabsf(low_y)<0.0001f && high_y>1 && high_y<2);
    assert(high_x-low_x<=1.8001f && fabsf(high_z-low_z-3.6f)<0.0001f);
    assert(fabsf(high_x+low_x)<0.0001f && fabsf(high_z+low_z)<0.0001f);
    /* The chase camera keeps the whole local car inside its view at every yaw,
       so it can use the direct GPU path without rebuilding world clip bounds. */
    for (int turn=0;turn<24;++turn) {
        float heading=turn*0.261799388f,s=sinf(heading),c=cosf(heading);
        ClipCamera camera;
        assert(clip_camera(&camera,(ClipVector){-s*9,5,-c*9},(ClipVector){s*4,1,c*4},60,480.0f/272,0.5f,300)==0);
        for (uint32_t i=0;i<mesh.count;++i) {
            MeshVertex local=mesh.vertices[i];
            ClipVector world={c*local.x+s*local.z,local.y,c*local.z-s*local.x};
            for (int plane=0;plane<6;++plane) {
                ClipPlane p=camera.planes[plane];
                assert(p.normal.x*world.x+p.normal.y*world.y+p.normal.z*world.z+p.offset>0);
            }
        }
    }
    printf("Original CompactA checks passed: 310 triangles, two textures, grounded %.2f x %.2f x %.2f m body; 24 chase-camera orientations\n",high_x-low_x,high_y-low_y,high_z-low_z);
    texture_free(&scene);mesh_free(&mesh);
    return 0;
}
