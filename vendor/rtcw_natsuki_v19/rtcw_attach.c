/* Concatenate two decoded model meshes by a RTCW tag, with Q6/Q14
   coordinate transforms, keeping all data within static buffers.
   File loading remains the responsibility of rtcw_codec.c.
*/
#include "rtcw_codec.h"
#include <string.h>
#include <stdio.h>

static rtcw_model scene_parent;
static char scene_err[192];
static unsigned int attachment_count;
static int scene_ready;
const char *rtcw_attach_error(void) {return scene_err;}
static int fail(const char *msg){
    strncpy(scene_err,msg,sizeof(scene_err)-1);
    scene_err[sizeof(scene_err)-1]=0;
    return 0;
}
static int too_far(long v) {return v< -10000000L || v>10000000L;}
static int safe_axis(const rtcw_tag *tag) {
    unsigned int i,j;
    for(i=0;i<3;i++)for(j=0;j<3;j++)
        if(tag->axis[i][j]< -32768L||tag->axis[i][j]>32768L)return 0;
    return 1;
}
static long q14mul(long a,long b) {
    return (a/16384L)*b+((a%16384L)*b)/16384L;
}
/* axis[local_basis][world_component] */
static int apply(const rtcw_tag *tag, const long in[3],long out[3]) {
    unsigned int j,k;
    if(!safe_axis(tag))return 0;
    for(j=0;j<3;j++){
        out[j]=tag->origin[j];
        for(k=0;k<3;k++){
            if(too_far(in[k])||too_far(out[j]))return 0;
            out[j]+=q14mul(in[k],tag->axis[k][j]);
        }
        if(too_far(out[j]))return 0;
    }
    return 1;
}
static void prefixed(char out[65],const char *prefix,const char *name) {
    size_t n;
    strncpy(out,prefix,64);out[64]=0;
    n=strlen(out);
    if(n<64){strncat(out,name,64-n);out[64]=0;}
}
int rtcw_attach_begin(const char *parent_file, unsigned long parent_frame,
                      const char *parent_skin) {
    scene_ready=0;attachment_count=0;
    if(!parent_file)return fail("missing parent file");
    if(!rtcw_open(parent_file)||!rtcw_decode(parent_frame))return fail(rtcw_error());
    if(parent_skin&&!rtcw_apply_skin(parent_skin))return fail("parent skin failed");
    scene_ready=1;return 1;
}
/* Begin from an already decoded/interpolated parent pose.
   This is used by offline animation previews: no source file reload. */
int rtcw_attach_begin_decoded(void) {
    scene_ready=0;attachment_count=0;
    if(!rtcw_current.vertex_count||!rtcw_current.surface_count)
        return fail("decoded parent is empty");
    scene_ready=1;
    return 1;
}
int rtcw_attach_add_blend(const char *tag_name,const char *child_file,
                    unsigned long child_frame,unsigned long child_next,
                    unsigned int blend_q12,const char *child_skin) {

    rtcw_tag anchor;
    unsigned int k,j,base_surface,found=0;
    unsigned long i,base_v,base_t;
    unsigned int child_surfaces;
    unsigned long child_vertices,child_triangles,child_tags;
    char tmp[65],prefix[24];
    if(!scene_ready||!child_file||!tag_name)return fail("scene not started or missing child/tag");
    if(attachment_count>=30U)return fail("maximum attachment count reached");
    if(attachment_count==0U)strcpy(prefix,"child_");
    else sprintf(prefix,"child%u_",attachment_count+1U);
    for(i=0;i<rtcw_current.tag_count;i++){
        if(strcmp(rtcw_current.tags[i].name,tag_name)==0){
            anchor=rtcw_current.tags[i];found=1;break;
        }
    }
    if(!found)return fail("tag not found in parent model");
    for(i=0;i<3;i++)if(too_far(anchor.origin[i]))return fail("parent tag outside spatial safety range");
    if(!safe_axis(&anchor))return fail("invalid parent tag orientation");
    memcpy(&scene_parent,&rtcw_current,sizeof(rtcw_model));
    if(!rtcw_open(child_file)||!rtcw_decode_blend(child_frame,child_next,blend_q12))return fail(rtcw_error());
    if(child_skin && !rtcw_apply_skin(child_skin))return fail("child skin failed");
    child_surfaces=rtcw_current.surface_count;
    child_vertices=rtcw_current.vertex_count;
    child_triangles=rtcw_current.triangle_count;
    child_tags=rtcw_current.tag_count;
    base_surface=scene_parent.surface_count;
    base_v=scene_parent.vertex_count;
    base_t=scene_parent.triangle_count;
    if(child_surfaces>RTCW_MAX_SURFACES-base_surface ||
       child_vertices>RTCW_MAX_VERTICES-base_v ||
       child_triangles>RTCW_MAX_TRIANGLES-base_t ||
       child_tags>RTCW_MAX_BONES-scene_parent.tag_count)
        return fail("combined model exceeds fixed scene capacities");
    for(i=0;i<child_vertices;i++){
        rtcw_vertex v=rtcw_current.verts[i];
        long p[3],q[3];
        p[0]=v.x;p[1]=v.y;p[2]=v.z;
        if(!apply(&anchor,p,q))return fail("attachment geometry coordinate exceeds limit");
        v.x=q[0];v.y=q[1];v.z=q[2];
        scene_parent.verts[base_v+i]=v;
    }
    for(i=0;i<child_triangles;i++){
        rtcw_triangle tri=rtcw_current.tris[i];
        tri.a+=base_v;tri.b+=base_v;tri.c+=base_v;
        tri.surface+=(unsigned int)base_surface;
        scene_parent.tris[base_t+i]=tri;
    }
    for(j=0;j<child_surfaces;j++){
        rtcw_surface surface=rtcw_current.surfaces[j];
        prefixed(tmp,prefix,surface.name);
        strcpy(surface.name,tmp);
        surface.first_vertex+=base_v;
        surface.first_triangle+=base_t;
        scene_parent.surfaces[base_surface+j]=surface;
    }
    for(i=0;i<child_tags;i++){
        rtcw_tag tag=rtcw_current.tags[i];
        long pos[3];long out[3][3];
        if(!safe_axis(&tag))return fail("invalid child tag orientation");
        if(!apply(&anchor,tag.origin,pos))return fail("child tag position outside range");
        for(j=0;j<3;j++){
            for(k=0;k<3;k++){
                unsigned int a;
                long sum=0;
                for(a=0;a<3;a++)sum+=q14mul(tag.axis[j][a],anchor.axis[a][k]);
                out[j][k]=sum;
            }
        }
        for(j=0;j<3;j++){
            tag.origin[j]=pos[j];
            for(k=0;k<3;k++)tag.axis[j][k]=out[j][k];
        }
        tag.bone_index=-1; /* child skeleton is not copied into parent skeleton */
        prefixed(tmp,prefix,tag.name);strcpy(tag.name,tmp);
        scene_parent.tags[scene_parent.tag_count+i]=tag;
    }
    scene_parent.vertex_count+=child_vertices;
    scene_parent.triangle_count+=child_triangles;
    scene_parent.surface_count+=(unsigned int)child_surfaces;
    scene_parent.tag_count+=child_tags;
    memcpy(&rtcw_current,&scene_parent,sizeof(rtcw_model));
    attachment_count++;
    return 1;
}
int rtcw_attach_add(const char *tag_name,const char *child_file,
                    unsigned long child_frame,const char *child_skin) {
    return rtcw_attach_add_blend(tag_name,child_file,child_frame,child_frame,0U,child_skin);
}
int rtcw_attach_pair(const char *parent_file,unsigned long parent_frame,
                     const char *parent_skin,const char *tag_name,
                     const char *child_file,unsigned long child_frame,
                     const char *child_skin) {
    if(!rtcw_attach_begin(parent_file,parent_frame,parent_skin))return 0;
    return rtcw_attach_add(tag_name,child_file,child_frame,child_skin);
}
