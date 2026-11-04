/* MilkShape 3D v1.4 -> textured OBJ/MTL, bind pose.
 * C89, static buffers, fixed point, no heap. Face corner UV are preserved.
 */
#include "gif_ms3d_uv.h"
#include "gif_ms3d_normals.h"
#include "gif_ms3d_mtl.h"
#include <stdio.h>
#include <string.h>

#define FILE_MAX (16UL * 1024UL * 1024UL)
static unsigned char g_file[FILE_MAX];
static ms3d_model g_model;
static ms3d_corner_normals g_normals;
static ms3d_uv_bindings g_bindings;
static ms3d_mtl_bundle g_bundle;

static int load(const char *name,size_t *length)
{
    FILE *f;size_t total,got;int ch;
    f=fopen(name,"rb");if(!f)return 0;
    total=0U;
    while(total<sizeof(g_file)) {
        got=fread(g_file+total,1U,sizeof(g_file)-total,f);
        total+=got;if(got==0U)break;
    }
    ch=fgetc(f);fclose(f);
    if(ch!=EOF||total==0U)return 0;
    *length=total;return 1;
}
static void fx(FILE *f,ms3d_fx n)
{
    char str[48];
    if(ms3d_fx_to_ascii(n,str,sizeof(str),5U)==MS3D_OK)fputs(str,f);
    else fputs("0",f);
}
static void group_name(FILE *f,const char *str)
{
    unsigned char ch;
    if(!str||!*str){fputs("unnamed",f);return;}
    while(*str){ch=(unsigned char)*str;
        if(ch<=32U||ch=='#'||ch=='/'||ch=='\\')fputc('_',f);
        else fputc(ch,f);
        ++str;
    }
}
static int write_obj(const char *path,const char *mtl_path,const ms3d_model *m,int mode)
{
    FILE *f;unsigned i,j,k;int prev_group=-2,prev_mat=-2,g,mat;
    const ms3d_triangle *tri;
    ms3d_fx u,v;
    f=fopen(path,"wb");if(!f)return 0;
    fputs("# MilkShape v1.4 bind pose textured export\n",f);
    if(m->num_materials)fprintf(f,"mtllib %s\n",ms3d_mtl_basename(mtl_path));
    for(i=0U;i<m->num_vertices;++i) {
        fputc('v',f);
        for(j=0U;j<3U;++j){fputc(' ',f);fx(f,m->vertices[i].position[j]);}
        fputc('\n',f);
    }
    for(i=0U;i<m->num_triangles;++i){tri=&m->triangles[i];
        for(j=0U;j<3U;++j){ms3d_uv_obj(tri,(ms3d_u8)j,&u,&v);
            fputs("vt ",f);fx(f,u);fputc(' ',f);fx(f,v);fputc('\n',f);
        }
    }
    for(i=0U;i<m->num_triangles;++i)
        for(j=0U;j<3U;++j){fputs("vn",f);
            for(k=0U;k<3U;++k){fputc(' ',f);fx(f,g_normals.xyz[i][j][k]);}
            fputc('\n',f);
        }
    for(i=0U;i<m->num_triangles;++i) {
        tri=&m->triangles[i];g=g_bindings.group[i];mat=g_bindings.material[i];
        if(g!=prev_group) {
            fputs("g ",f);
            if(g>=0)group_name(f,m->groups[g].name);
            else fputs("unassigned",f);
            fputc('\n',f);prev_group=g;
        }
        if(mat!=prev_mat) {
            if(mat>=0)fprintf(f,"usemtl ms3d_mat_%d\n",mat);
            else fputs("usemtl ms3d_untextured\n",f);
            prev_mat=mat;
        }
        if(mode==MS3D_NORMAL_SMOOTH_ALL)fputs("s 1\n",f);
        else if(mode==MS3D_NORMAL_FLAT||tri->smoothing_group==0U)fputs("s off\n",f);
        else fprintf(f,"s %u\n",(unsigned)tri->smoothing_group);
        fputc('f',f);
        for(j=0U;j<3U;++j){unsigned long vi=(unsigned long)tri->vertex_indices[j]+1UL;
            unsigned long ci=(unsigned long)i*3UL+(unsigned long)j+1UL;
            fprintf(f," %lu/%lu/%lu",vi,ci,ci);
        }
        fputc('\n',f);
    }
    return fclose(f)==0;
}
int main(int argc,char **argv)
{
    size_t size;int rc,i,mode=MS3D_NORMAL_SMOOTH_GROUP;
    char mtl[MS3D_MTL_PATH_CAP];const char *root=(const char *)0;
    if(argc<3){fputs("usage: ms3d_obj_export input.ms3d output.obj [--smooth|--smooth-weld|--smooth-all|--source|--flat] [--texture-root DIR]\n",stderr);return 2;}
    for(i=3;i<argc;++i){
        if(strcmp(argv[i],"--smooth")==0)mode=MS3D_NORMAL_SMOOTH_GROUP;
        else if(strcmp(argv[i],"--smooth-weld")==0)mode=MS3D_NORMAL_SMOOTH_WELDED;
        else if(strcmp(argv[i],"--smooth-all")==0)mode=MS3D_NORMAL_SMOOTH_ALL;
        else if(strcmp(argv[i],"--source")==0)mode=MS3D_NORMAL_SOURCE;
        else if(strcmp(argv[i],"--flat")==0)mode=MS3D_NORMAL_FLAT;
        else if(strcmp(argv[i],"--texture-root")==0 && i+1<argc)root=argv[++i];
        else {fprintf(stderr,"invalid option: %s\n",argv[i]);return 2;}
    }
    if(!load(argv[1],&size)){fputs("input error/size cap\n",stderr);return 2;}
    rc=ms3d_parse(&g_model,g_file,size);
    if(rc!=MS3D_OK){fprintf(stderr,"parse: %s\n",ms3d_error_string(rc));return 1;}
    rc=ms3d_build_corner_normals(&g_model,&g_normals,mode);
    if(rc!=MS3D_OK){fputs("normals failed\n",stderr);return 1;}
    rc=ms3d_uv_build_bindings(&g_model,&g_bindings);
    if(rc!=MS3D_OK){fputs("bindings failed\n",stderr);return 1;}
    ms3d_mtl_filename(argv[2],mtl,sizeof(mtl));
    if(!mtl[0]){fputs("MTL path too long\n",stderr);return 2;}
    if(g_model.num_materials&&!ms3d_mtl_export(mtl,argv[1],root,&g_model,&g_bundle)){
        fputs("MTL export failed\n",stderr);return 1;
    }
    if(!write_obj(argv[2],mtl,&g_model,mode)){fputs("OBJ export failed\n",stderr);return 1;}
    printf("PASS verts=%u tris=%u mats=%u textures=%u missing=%u bindings=%lu conflicts=%lu\n",
        (unsigned)g_model.num_vertices,(unsigned)g_model.num_triangles,
        (unsigned)g_model.num_materials,g_bundle.copied_diffuse,g_bundle.missing_diffuse,
        g_bindings.assigned_count,g_bindings.conflict_count);
    return 0;
}
