/* Direct C89 regression for CORDIC SLERP, pose transitions, UV and materials. */
#include "gif_ms3d_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FILE_CAP (16UL*1024UL*1024UL)
static unsigned char bytes[FILE_CAP];
static ms3d_model model;
static ms3d_skin_pose pose;
static ms3d_skin_vertices verts;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
static ms3d_render_queue render_queue;
static unsigned long corners,faces,materials;
static int seen_good=1;
static int frame(void *unused,unsigned long ms,ms3d_fx t,const ms3d_model *m)
{(void)unused;(void)ms;(void)t;(void)m;corners=0UL;faces=0UL;materials=0UL;return MS3D_OK;}
static int material(void *unused,ms3d_u16 index,const ms3d_material *mat)
{(void)unused;if(index>=model.num_materials||mat!=&model.materials[index])seen_good=0;++materials;return MS3D_OK;}
static int triangle(void *unused,ms3d_u16 index,int group,int mat)
{(void)unused;if(index>=model.num_triangles || group!=bindings.group[index] || mat!=bindings.material[index])seen_good=0;++faces;return MS3D_OK;}
static int corner(void *unused,ms3d_u16 tri,ms3d_u8 c,const ms3d_draw_corner *v)
{
    (void)unused;
    if(tri>=model.num_triangles||c>2U)return MS3D_E_BAD_INDEX;
    if(v->uv[0]!=model.triangles[tri].s[c] || v->uv[1]!=model.triangles[tri].t[c])seen_good=0;
    if(v->group_index!=(ms3d_u8)bindings.group[tri]&&bindings.group[tri]>=0)seen_good=0;
    if(v->material_index!=bindings.material[tri])seen_good=0;
    ++corners;return MS3D_OK;
}
int main(int argc,char **argv)
{
    FILE *f;size_t sz;int rc,extra;unsigned i;
    ms3d_player_sink sink;
    ms3d_fx front[3]={0L,0L,MS3D_FX_ONE};
    if(argc!=2){fprintf(stderr,"usage: %s FILE.ms3d\n",argv[0]);return 2;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    sz=fread(bytes,1U,sizeof(bytes),f);extra=fgetc(f);fclose(f);
    if(sz==0U||extra!=EOF)return 3;
    rc=ms3d_parse(&model,bytes,sz);if(rc!=MS3D_OK){fprintf(stderr,"parse=%d\n",rc);return 4;}
    rc=ms3d_player_prepare(&player,&model,&pose,&verts,&normals,&bindings,MS3D_NORMAL_SOURCE);
    if(rc!=MS3D_OK){fprintf(stderr,"prepare=%d\n",rc);return 5;}
    rc=ms3d_player_rotation_mode(&player,MS3D_ROTATION_QUAT_SLERP);
    if(rc!=MS3D_OK)return 6;
    memset(&sink,0,sizeof(sink));sink.begin_frame=frame;sink.material=material;
    sink.begin_triangle=triangle;sink.corner=corner;
    for(i=0U;i<3U;++i){
        if(player.duration_ms!=0UL && ms3d_player_seek(&player, (player.duration_ms*i)/4UL)!=MS3D_OK)return 7;
        if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 8;
        if(!seen_good||faces!=model.num_triangles||corners!=(unsigned long)model.num_triangles*3UL||materials!=model.num_materials)return 9;
        if(ms3d_render_build_queue(&model,&bindings,&verts,front,&render_queue)!=MS3D_OK)return 10;
        if(render_queue.opaque_count+render_queue.cutout_count+render_queue.blend_count!=model.num_triangles)return 11;
    }
    if(player.duration_ms>0UL){
        if(ms3d_player_blend(&player,player.duration_ms,32768L)!=MS3D_OK)return 12;
        if(ms3d_player_emit(&player,&sink)!=MS3D_OK)return 13;
        if(!seen_good||faces!=model.num_triangles||corners!=(unsigned long)model.num_triangles*3UL)return 14;
    }
    printf("OK tris=%u joints=%u materials=%u frames=3 blend=%d\n",
         (unsigned)model.num_triangles,(unsigned)model.num_joints,
         (unsigned)model.num_materials,player.duration_ms>0UL);
    return 0;
}
