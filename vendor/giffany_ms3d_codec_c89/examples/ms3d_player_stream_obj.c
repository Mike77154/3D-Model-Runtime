/* Textured frame sequence from the NEW v1.7 player callback API.
 * ANSI C89 / fixed point; OBJ only serves as portable frame verification.
 * Runtime consumers should subscribe directly to ms3d_player_emit().
 */
#include "gif_ms3d_player.h"
#include "gif_ms3d_mtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_FILE 16777216UL
static unsigned char input[MAX_FILE];
static ms3d_model model;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_player player;
static ms3d_mtl_bundle assets;
static int mat_for_tri[MS3D_MAX_TRIANGLES];
static int group_for_tri[MS3D_MAX_TRIANGLES];
static FILE *out;
static const char *mtl_base;
static void wfx(ms3d_fx v)
{
    char b[48];
    if(ms3d_fx_to_ascii(v,b,sizeof(b),5U)==MS3D_OK)fputs(b,out);
    else fputc('0',out);
}
static int begin(void *user,unsigned long ms,ms3d_fx t,const ms3d_model *m)
{
    (void)user;(void)t;(void)m;
    fprintf(out,"# MilkShape v1.7 skinned frame %lu ms, native UV inverted ONCE for OBJ\n",ms);
    fprintf(out,"mtllib %s\n",mtl_base);
    return MS3D_OK;
}
static int tri(void *user,ms3d_u16 ix,int g,int mat)
{
    (void)user;mat_for_tri[ix]=mat;group_for_tri[ix]=g;return MS3D_OK;
}
static int corner(void *user,ms3d_u16 ix,ms3d_u8 c,const ms3d_draw_corner *v)
{
    unsigned k;(void)user;(void)ix;(void)c;
    fputs("v",out);for(k=0U;k<3U;++k){fputc(' ',out);wfx(v->position[k]);}fputc('\n',out);
    fputs("vt ",out);wfx(v->uv[0]);fputc(' ',out);
    wfx(ms3d_fx_sub_sat(MS3D_FX_ONE,v->uv[1]));fputc('\n',out);
    fputs("vn",out);for(k=0U;k<3U;++k){fputc(' ',out);wfx(v->normal[k]);}fputc('\n',out);
    return ferror(out)?MS3D_E_SHORT:MS3D_OK;
}
static int end(void *user)
{
    unsigned i,j;int pm=-9999,pg=-9999,m,g;
    (void)user;
    for(i=0U;i<model.num_triangles;++i){
        m=mat_for_tri[i];g=group_for_tri[i];
        if(g!=pg){if(g>=0)fprintf(out,"g ms3d_group_%d\n",g);
            else fputs("g unassigned\n",out);
            pg=g;}
        if(m!=pm){if(m>=0)fprintf(out,"usemtl ms3d_mat_%d\n",m);
            else fputs("usemtl ms3d_untextured\n",out);
            pm=m;}
        if(model.triangles[i].smoothing_group)
            fprintf(out,"s %u\n",(unsigned)model.triangles[i].smoothing_group);
        else fputs("s off\n",out);
        fputc('f',out);
        for(j=0U;j<3U;++j){
            unsigned long idx=(unsigned long)i*3UL+(unsigned long)j+1UL;
            fprintf(out," %lu/%lu/%lu",idx,idx,idx);
        }
        fputc('\n',out);
    }
    return ferror(out)?MS3D_E_SHORT:MS3D_OK;
}
int main(int argc,char **argv)
{
    ms3d_player_sink sink;
    char mtl[MS3D_MTL_PATH_CAP],obj[MS3D_MTL_PATH_CAP];
    const char *texture_root=0;
    unsigned long steps,dt,k;int mode=MS3D_NORMAL_SOURCE,rc,i,extra;
    int rotation_mode=MS3D_ROTATION_QUAT_NLERP;
    FILE *f;size_t sz;
    if(argc<5){fprintf(stderr,"usage: ms3d_player_stream_obj model.ms3d outprefix frames step_ms [--texture-root DIR] [--smooth-weld] [--euler|--nlerp|--slerp]\n");return 2;}
    steps=(unsigned long)atol(argv[3]);dt=(unsigned long)atol(argv[4]);
    if(steps==0UL||steps>120UL||dt==0UL||dt>10000UL||strlen(argv[2])>900U)return 2;
    for(i=5;i<argc;++i){
        if(strcmp(argv[i],"--smooth-weld")==0)mode=MS3D_NORMAL_SMOOTH_WELDED;
        else if(strcmp(argv[i],"--euler")==0)rotation_mode=MS3D_ROTATION_EULER;
        else if(strcmp(argv[i],"--nlerp")==0)rotation_mode=MS3D_ROTATION_QUAT_NLERP;
        else if(strcmp(argv[i],"--slerp")==0)rotation_mode=MS3D_ROTATION_QUAT_SLERP;
        else if(strcmp(argv[i],"--texture-root")==0&&i+1<argc)texture_root=argv[++i];
        else return 2;
    }
    f=fopen(argv[1],"rb");if(!f)return 2;
    sz=fread(input,1U,sizeof(input),f);extra=fgetc(f);fclose(f);
    if(sz==0U||extra!=EOF)return 2;
    rc=ms3d_parse(&model,input,sz);if(rc!=MS3D_OK)return 1;
    rc=ms3d_player_prepare(&player,&model,&pose,&vertices,&normals,&bindings,mode);
    if(rc!=MS3D_OK){fprintf(stderr,"prepare %d\n",rc);return 1;}
    rc=ms3d_player_rotation_mode(&player,rotation_mode);
    if(rc!=MS3D_OK)return 1;
    sprintf(mtl,"%s.mtl",argv[2]);
    if(!ms3d_mtl_export(mtl,argv[1],texture_root,&model,&assets))return 1;
    mtl_base=ms3d_mtl_basename(mtl);
    memset(&sink,0,sizeof(sink));sink.begin_frame=begin;
    sink.begin_triangle=tri;sink.corner=corner;sink.end_frame=end;
    for(k=0UL;k<steps;++k){
        sprintf(obj,"%s_%03lu.obj",argv[2],k);
        out=fopen(obj,"wb");if(!out)return 2;
        rc=ms3d_player_emit(&player,&sink);
        if(fclose(out)!=0||rc!=MS3D_OK)return 1;
        printf("FRAME %lu time_ms=%lu corners=%lu materials=%u file=%s\n",k,
                player.current_ms,(unsigned long)model.num_triangles*3UL,
                (unsigned)model.num_materials,obj);
        if(ms3d_player_advance(&player,dt)!=MS3D_OK)return 1;
    }
    printf("ASSETS copied=%u missing=%u\n",assets.copied_diffuse,assets.missing_diffuse);
    return 0;
}
