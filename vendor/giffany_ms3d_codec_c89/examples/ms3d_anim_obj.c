/* Animated MS3D -> textured OBJ frame snapshot.
 * OBJ is for inspection; no rig in OBJ. Runtime stays direct C89 callbacks.
 */
#include "gif_ms3d_skin.h"
#include "gif_ms3d_uv.h"
#include "gif_ms3d_mtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FILE_CAP (16UL*1024UL*1024UL)
static unsigned char file_buf[FILE_CAP];
static ms3d_model model;
static ms3d_corner_normals normals;
static ms3d_skin_pose pose;
static ms3d_skin_vertices positions;
static ms3d_uv_bindings bindings;
static ms3d_mtl_bundle bundle;
static ms3d_fx deformed_normals[MS3D_MAX_TRIANGLES][3][3];
static int load(const char *p,size_t *length)
{
    FILE *f=fopen(p,"rb");size_t n=0U,got;int ch;
    if(!f)return 0;
    while(n<FILE_CAP){got=fread(file_buf+n,1U,FILE_CAP-n,f);n+=got;if(!got)break;}
    ch=fgetc(f);fclose(f);
    *length=n;return n>0U&&ch==EOF;
}
static void outfx(FILE *f,ms3d_fx n)
{
    char s[60];if(ms3d_fx_to_ascii(n,s,sizeof(s),5U)==MS3D_OK)fputs(s,f);else fputs("0",f);
}
static int emit_normal(void *user,ms3d_u16 i,ms3d_u8 corner,const ms3d_draw_corner *dc)
{
    unsigned k;(void)user;
    for(k=0U;k<3U;++k)deformed_normals[i][corner][k]=dc->normal[k];
    return MS3D_OK;
}
int main(int argc,char **argv)
{
    size_t size;int rc,mode=MS3D_NORMAL_SOURCE,i,prev_g=-2,prev_m=-2,g,mat;
    unsigned j,k;FILE *f;long milli;ms3d_fx time,u,v;char mtl[MS3D_MTL_PATH_CAP];
    const char *root=(const char *)0;const ms3d_triangle *tr;
    ms3d_shaded_sink sink;
    if(argc<4){fputs("usage: ms3d_anim_obj input.ms3d output.obj milliseconds [--source|--smooth|--smooth-weld|--flat] [--texture-root DIR]\n",stderr);return 2;}
    for(i=4;i<argc;++i) {
        if(strcmp(argv[i],"--source")==0)mode=MS3D_NORMAL_SOURCE;
        else if(strcmp(argv[i],"--smooth")==0)mode=MS3D_NORMAL_SMOOTH_GROUP;
        else if(strcmp(argv[i],"--smooth-weld")==0)mode=MS3D_NORMAL_SMOOTH_WELDED;
        else if(strcmp(argv[i],"--flat")==0)mode=MS3D_NORMAL_FLAT;
        else if(strcmp(argv[i],"--texture-root")==0 && i+1<argc)root=argv[++i];
        else{fputs("invalid option\n",stderr);return 2;}
    }
    milli=atol(argv[3]);
    if(milli<0L||milli>30000L){fputs("time must be 0..30000 ms\n",stderr);return 2;}
    time=ms3d_fx_div(ms3d_fx_from_int(milli),ms3d_fx_from_int(1000L));
    if(!load(argv[1],&size)){fputs("file read error\n",stderr);return 2;}
    rc=ms3d_parse(&model,file_buf,size);
    if(rc!=MS3D_OK){fprintf(stderr,"parse %d\n",rc);return 1;}
    if((rc=ms3d_build_corner_normals(&model,&normals,mode))!=MS3D_OK||
       (rc=ms3d_uv_build_bindings(&model,&bindings))!=MS3D_OK||
       (rc=ms3d_skin_prepare(&model,&pose))!=MS3D_OK||
       (rc=ms3d_skin_evaluate(&model,&pose,time))!=MS3D_OK||
       (rc=ms3d_skin_positions(&model,&pose,&positions))!=MS3D_OK){fprintf(stderr,"skinning %d\n",rc);return 1;}
    sink.user=0;sink.corner=emit_normal;
    if((rc=ms3d_emit_skinned_corners(&model,&pose,&positions,&normals,&sink))!=MS3D_OK){fprintf(stderr,"emit %d\n",rc);return 1;}
    ms3d_mtl_filename(argv[2],mtl,sizeof(mtl));
    if(!mtl[0])return 2;
    if(model.num_materials&&!ms3d_mtl_export(mtl,argv[1],root,&model,&bundle)){
        fputs("MTL failed\n",stderr);return 1;
    }
    f=fopen(argv[2],"wb");if(!f)return 2;
    fputs("# MilkShape skeletal pose with original material UVs\n",f);
    if(model.num_materials)fprintf(f,"mtllib %s\n",ms3d_mtl_basename(mtl));
    for(i=0;i<(int)model.num_vertices;++i){fputc('v',f);
        for(k=0U;k<3U;++k){fputc(' ',f);outfx(f,positions.xyz[i][k]);}
        fputc('\n',f);
    }
    for(i=0;i<(int)model.num_triangles;++i)for(j=0U;j<3U;++j){
        ms3d_uv_obj(&model.triangles[i],(ms3d_u8)j,&u,&v);
        fputs("vt ",f);outfx(f,u);fputc(' ',f);outfx(f,v);fputc('\n',f);
    }
    for(i=0;i<(int)model.num_triangles;++i)for(j=0U;j<3U;++j){
        fputs("vn",f);for(k=0U;k<3U;++k){fputc(' ',f);outfx(f,deformed_normals[i][j][k]);}
        fputc('\n',f);
    }
    for(i=0;i<(int)model.num_triangles;++i){
        tr=&model.triangles[i];g=bindings.group[i];mat=bindings.material[i];
        if(g!=prev_g){fputs("g ",f);
            if(g>=0)fprintf(f,"ms3d_group_%d",g);else fputs("unassigned",f);
            fputc('\n',f);prev_g=g;
        }
        if(mat!=prev_m){
            if(mat>=0)fprintf(f,"usemtl ms3d_mat_%d\n",mat);
            else fputs("usemtl ms3d_untextured\n",f);
            prev_m=mat;
        }
        if(tr->smoothing_group)fprintf(f,"s %u\n",(unsigned)tr->smoothing_group);
        else fputs("s off\n",f);
        fputc('f',f);
        for(j=0U;j<3U;++j) {
            unsigned long vi=(unsigned long)tr->vertex_indices[j]+1UL;
            unsigned long ci=(unsigned long)i*3UL+(unsigned long)j+1UL;
            fprintf(f," %lu/%lu/%lu",vi,ci,ci);
        }
        fputc('\n',f);
    }
    if(fclose(f)!=0)return 2;
    printf("PASS verts=%u tris=%u mats=%u tex_copied=%u missing=%u ms=%ld conflicts=%lu\n",
        (unsigned)model.num_vertices,(unsigned)model.num_triangles,
        (unsigned)model.num_materials,bundle.copied_diffuse,bundle.missing_diffuse,
        milli,bindings.conflict_count);
    return 0;
}
