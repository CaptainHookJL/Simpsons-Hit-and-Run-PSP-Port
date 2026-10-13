#include "clip.h"
#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static ClipVector scale(ClipVector v,float s) {return (ClipVector){v.x*s,v.y*s,v.z*s};}
static ClipVector add(ClipVector a,ClipVector b) {return (ClipVector){a.x+b.x,a.y+b.y,a.z+b.z};}
static float dot(ClipVector a,ClipVector b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
struct ClipBounds {ClipVector center,extent;};

int clip_camera(ClipCamera *camera,ClipVector eye,ClipVector target,
                float vertical_fov,float aspect,float near_distance,float far_distance)
{
    if (!isfinite(vertical_fov)||!isfinite(aspect)||!isfinite(near_distance)||!isfinite(far_distance)||
        vertical_fov<=0||vertical_fov>=179||aspect<=0||near_distance<=0||far_distance<=near_distance+0.002f)
        return -1;
    ClipVector forward={target.x-eye.x,target.y-eye.y,target.z-eye.z};
    float length=sqrtf(dot(forward,forward));
    if (!isfinite(length)||length<0.00001f) return -1;
    forward=scale(forward,1.0f/length);
    ClipVector right={-forward.z,0,forward.x}; /* forward cross world-up */
    length=sqrtf(dot(right,right));
    if (length<0.00001f) return -1;
    right=scale(right,1.0f/length);
    ClipVector up={right.y*forward.z-right.z*forward.y,
                   right.z*forward.x-right.x*forward.z,
                   right.x*forward.y-right.y*forward.x};
    ClipCamera result={0};
    result.eye=eye;result.right=right;result.up=up;result.forward=forward;
    /* Tiny inset keeps reprojected intersections inside GE planes after float
       rounding; it is less than a tenth of a screen pixel at the side edges. */
    result.near_distance=near_distance+0.001f;
    result.far_distance=far_distance-0.001f;
    result.tan_y=tanf(vertical_fov*0.00872664626f)*0.9999f;
    result.tan_x=result.tan_y*aspect;
    result.planes[0]=(ClipPlane){forward,-dot(forward,eye)-result.near_distance};
    result.planes[1]=(ClipPlane){scale(forward,-1),dot(forward,eye)+result.far_distance};
    ClipVector normals[4]={add(scale(forward,result.tan_x),right),
                           add(scale(forward,result.tan_x),scale(right,-1)),
                           add(scale(forward,result.tan_y),up),
                           add(scale(forward,result.tan_y),scale(up,-1))};
    for (int i=0;i<4;++i) result.planes[i+2]=(ClipPlane){normals[i],-dot(normals[i],eye)};
    *camera=result;
    return 0;
}

static float distance(const ClipPlane *plane,TexturedVertex vertex)
{
    return plane->normal.x*vertex.x+plane->normal.y*vertex.y+plane->normal.z*vertex.z+plane->offset;
}

static unsigned char code(const ClipCamera *camera,TexturedVertex vertex,unsigned int mask)
{
    ClipVector v={vertex.x-camera->eye.x,vertex.y-camera->eye.y,vertex.z-camera->eye.z};
    float z=dot(v,camera->forward);
    unsigned char result=((mask&1)&&z<camera->near_distance?1:0)|
                         ((mask&2)&&z>camera->far_distance?2:0);
    if (mask&12) {
        float x=dot(v,camera->right),extent=z*camera->tan_x;
        result|=((mask&4)&&x < -extent?4:0)|((mask&8)&&x > extent?8:0);
    }
    if (mask&48) {
        float y=dot(v,camera->up),extent=z*camera->tan_y;
        result|=((mask&16)&&y < -extent?16:0)|((mask&32)&&y > extent?32:0);
    }
    return result;
}

/* Conservative bounds: skip only if the entire box is outside one plane.
   Otherwise return the subset of planes that might intersect its triangles. */
static int bounds_mask(const ClipCamera *camera,const ClipBounds *bounds)
{
    unsigned int mask=0;
    for (int i=0;i<6;++i) {
        ClipPlane plane=camera->planes[i];
        float center=dot(plane.normal,bounds->center)+plane.offset;
        float radius=fabsf(plane.normal.x)*bounds->extent.x+
                     fabsf(plane.normal.y)*bounds->extent.y+
                     fabsf(plane.normal.z)*bounds->extent.z;
        if (center+radius < -0.002f) return -1;
        if (center-radius < 0.002f) mask|=1u<<i;
    }
    return (int)mask;
}

static TexturedVertex interpolate(TexturedVertex a,TexturedVertex b,float t)
{
    TexturedVertex result={a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t,0,
                          a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
    for (int shift=0;shift<32;shift+=8) {
        float first=(float)((a.color>>shift)&255),last=(float)((b.color>>shift)&255);
        uint32_t value=(uint32_t)(first+(last-first)*t+0.5f);
        result.color|=value<<shift;
    }
    return result;
}

static int clip_triangle_mask(const ClipCamera *camera,const TexturedVertex input[3],
                              TexturedVertex output[21],unsigned int mask)
{
    TexturedVertex polygons[2][12];
    memcpy(polygons[0],input,3*sizeof(*input));
    int count=3,current=0;
    for (int plane=0;plane<6;++plane) {
        if (!(mask&(1u<<plane))) continue;
        TexturedVertex *source=polygons[current],*dest=polygons[1-current];
        int next_count=0;
        TexturedVertex previous=source[count-1];
        float previous_distance=distance(&camera->planes[plane],previous);
        for (int i=0;i<count;++i) {
            TexturedVertex vertex=source[i];
            float d=distance(&camera->planes[plane],vertex);
            if ((d>=0)!=(previous_distance>=0)) {
                float t=previous_distance/(previous_distance-d);
                if (t<0) t=0; else if (t>1) t=1;
                if (t>0 && t<1) {
                    if (next_count>=12) return -1;
                    dest[next_count++]=interpolate(previous,vertex,t);
                }
            }
            if (d>=0) {
                if (next_count>=12) return -1;
                dest[next_count++]=vertex;
            }
            previous=vertex;previous_distance=d;
        }
        count=next_count;current=1-current;
        if (count<3) return 0;
    }
    if (count>9) return -1;
    int n=0;
    for (int i=1;i<count-1;++i) {
        output[n++]=polygons[current][0];
        output[n++]=polygons[current][i];
        output[n++]=polygons[current][i+1];
    }
    return n;
}

int clip_triangle(const ClipCamera *camera,const TexturedVertex input[3],TexturedVertex output[21])
{
    return clip_triangle_mask(camera,input,output,63);
}

void clip_free(ClippedScene *output)
{
    free(output->vertices);free(output->batches);free(output->codes);
    free(output->original);free(output->bounds);
    memset(output,0,sizeof(*output));
}

int clip_create(ClippedScene *output,const TexturedScene *scene)
{
    if (!scene->vertices||!scene->count||scene->count>49152||scene->count%3||!scene->batch_count||scene->batch_count>1024)
        return -1;
    ClippedScene next={0};
    next.source_count=scene->count;
    next.source_batch_count=scene->batch_count;
    next.capacity=scene->count*7; /* at most 21 output vertices per three inputs */
    next.vertices=memalign(16,next.capacity*sizeof(*next.vertices));
    next.batches=malloc(scene->batch_count*sizeof(*next.batches));
    next.codes=malloc(scene->count);
    next.original=malloc(scene->batch_count);
    next.bounds=malloc(scene->batch_count*sizeof(*next.bounds));
    if (!next.vertices||!next.batches||!next.codes||!next.original||!next.bounds) {clip_free(&next);return -1;}
    for (uint32_t i=0;i<scene->batch_count;++i) {
        TextureBatch b=scene->batches[i];
        if (!b.count||b.first>=scene->count||b.count>scene->count-b.first||b.first%3||b.count%3)
            {clip_free(&next);return -1;}
        TexturedVertex first=scene->vertices[b.first];
        ClipVector low={first.x,first.y,first.z},high=low;
        for (uint32_t j=b.first+1;j<b.first+b.count;++j) {
            TexturedVertex v=scene->vertices[j];
            low.x=fminf(low.x,v.x);low.y=fminf(low.y,v.y);low.z=fminf(low.z,v.z);
            high.x=fmaxf(high.x,v.x);high.y=fmaxf(high.y,v.y);high.z=fmaxf(high.z,v.z);
        }
        next.bounds[i].center=scale(add(low,high),0.5f);
        next.bounds[i].extent=(ClipVector){(high.x-low.x)*0.5f,(high.y-low.y)*0.5f,(high.z-low.z)*0.5f};
    }
    clip_free(output);*output=next;
    return 0;
}

int clip_scene(ClippedScene *output,const TexturedScene *scene,const ClipCamera *camera)
{
    if (!output->vertices||output->source_count!=scene->count||output->source_batch_count!=scene->batch_count) return -1;
    output->count=0;output->batch_count=0;
    output->direct_count=0;output->tested_count=0;
    for (uint32_t batch=0;batch<scene->batch_count;++batch) {
        TextureBatch source=scene->batches[batch];
        if (source.first>=scene->count||source.count>scene->count-source.first||source.first%3||source.count%3) return -1;
        int mask=bounds_mask(camera,&output->bounds[batch]);
        if (mask<0) continue;
        if (!mask) {
            output->batches[output->batch_count]=source;
            output->original[output->batch_count++]=1;
            output->direct_count+=source.count;
            continue;
        }
        output->tested_count+=source.count;
        for (uint32_t i=source.first;i<source.first+source.count;++i)
            output->codes[i]=code(camera,scene->vertices[i],(unsigned int)mask);
        uint32_t first=output->count;
        for (uint32_t i=source.first;i<source.first+source.count;i+=3) {
            unsigned char a=output->codes[i],b=output->codes[i+1],c=output->codes[i+2];
            if (a&b&c) continue; /* all points outside the same half-space */
            int n;
            if (!(a|b|c)) {
                n=3;
                if (output->count+3>output->capacity) return -1;
                memcpy(output->vertices+output->count,scene->vertices+i,3*sizeof(TexturedVertex));
            } else {
                if (output->count+21>output->capacity) return -1;
                /* Only planes crossed by this triangle can change it. */
                n=clip_triangle_mask(camera,scene->vertices+i,output->vertices+output->count,a|b|c);
                if (n<0) return -1;
            }
            output->count+=(uint32_t)n;
        }
        if (output->count>first) {
            output->original[output->batch_count]=0;
            output->batches[output->batch_count++]=(TextureBatch){first,output->count-first,source.texture};
        }
    }
    return 0;
}
