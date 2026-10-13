#include "../src/vehicle_view.h"
#include "../src/player.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
    assert(argc==4);StaticMesh mesh={0};TexturedScene scene={0};WheelRig rig={0},empty={0};VehicleView view={0};
    assert(mesh_load(&mesh,argv[1])==0 && texture_load(&scene,&mesh,argv[2])==0);
    assert(wheels_load(&rig,&mesh,&scene,argv[3])==0 && vehicle_view_create(&view,&scene,&rig)==0);
    assert(view.capacity==6510 && view.batch_capacity==6 && view.draw.textures==scene.textures);
    WheelAnimation animation={{0.6f,-0.7f,1.4f,-1.2f}};
    unsigned int partial=0,outside=0,inside=0,peak=0;
    for (int i=0;i<144;++i) {
        float angle=i*0.261799388f,distance=i<48?2:(i<96?7:18);
        Player player={.car={.heading=0.5f,.wheel_angle=0.4f},
            .walker={.x=sinf(angle)*distance,.z=cosf(angle)*distance,.camera_heading=angle+(i%3)*1.57079633f}};
        float eye[3],target[3];player_camera(&player,eye,target);ClipCamera camera;
        assert(clip_camera(&camera,(ClipVector){eye[0],eye[1],eye[2]},(ClipVector){target[0],target[1],target[2]},60,480.0f/272,0.5f,300)==0);
        int visible=vehicle_view_visibility(&view,&player.car,&camera);
        assert(vehicle_view_clip(&view,&scene,&rig,&animation,&player.car,&camera)==0);
        if (visible<0) {++outside;assert(view.draw.count==0);}
        else if (visible>0) {++inside;assert(view.draw.count==930);}
        else ++partial;
        if (view.draw.count>peak) peak=view.draw.count;
        assert(view.draw.count<=view.capacity && view.draw.batch_count<=view.batch_capacity && view.draw.count%3==0);
        uint32_t cursor=0;
        for (uint32_t j=0;j<view.draw.batch_count;++j) {
            TextureBatch b=view.draw.batches[j];assert(b.first==cursor && b.texture<scene.texture_count);cursor+=b.count;
        }
        assert(cursor==view.draw.count);
        for (uint32_t v=0;v<view.draw.count;++v) for (int plane=0;plane<6;++plane) {
            TexturedVertex vertex=view.draw.vertices[v];ClipPlane p=camera.planes[plane];
            assert(isfinite(vertex.u)&&isfinite(vertex.v));
            assert(p.normal.x*vertex.x+p.normal.y*vertex.y+p.normal.z*vertex.z+p.offset>-0.002f);
        }
    }
    assert(partial && outside && inside);
    /* Rigid fallback: transform geometry once and retain UVs/materials exactly. */
    assert(vehicle_view_create(&view,&scene,&empty)==0);
    Player player={.driving=1,.car={.x=10,.z=8,.heading=0.5f}};
    float eye[3],target[3];player_camera(&player,eye,target);ClipCamera camera;
    assert(clip_camera(&camera,(ClipVector){eye[0],eye[1],eye[2]},(ClipVector){target[0],target[1],target[2]},60,480.0f/272,0.5f,300)==0);
    assert(vehicle_view_visibility(&view,&player.car,&camera)==1);
    assert(vehicle_view_clip(&view,&scene,&empty,&animation,&player.car,&camera)==0 && view.draw.count==930);
    uint32_t cursor=0;
    for (uint32_t b=0;b<scene.batch_count;++b) {
        TextureBatch batch=scene.batches[b];assert(view.draw.batches[b].texture==batch.texture);
        for (uint32_t j=batch.first;j<batch.first+batch.count;++j) {
            TexturedVertex v=scene.vertices[j],out=view.draw.vertices[cursor++];
            assert(out.u==v.u && out.v==v.v && out.color==v.color);
            assert(fabsf(out.x-(10+cosf(0.5f)*v.x+sinf(0.5f)*v.z))<0.00001f);
            assert(fabsf(out.z-(8+cosf(0.5f)*v.z-sinf(0.5f)*v.x))<0.00001f);
        }
    }
    TexturedVertex *saved=view.draw.vertices;TexturedScene invalid={0};
    assert(vehicle_view_create(&view,&invalid,&empty)!=0 && view.draw.vertices==saved);
    vehicle_view_free(&view);wheels_free(&rig);texture_free(&scene);mesh_free(&mesh);
    printf("Parked-car clipping checks passed: 144 on-foot views (%u partial, %u outside, %u inside, %u peak vertices), animated wheels, rigid fallback and preserved UV/materials\n",partial,outside,inside,peak);
    return 0;
}
