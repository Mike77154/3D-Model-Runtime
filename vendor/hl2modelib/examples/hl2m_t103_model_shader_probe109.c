/* Real T103 Source49 model VertexLitGeneric per-vertex shader test.
   Uses MDL/VVD/VTX mesher, VMT material params, VTF RGBA maps, VVD tangents.
   C89 pure integer; output PLY with vertex colors for external preview.
   Shaded vertex samples are NOT full GPU per-pixel shading. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define INPUT_MAX (8u*1024u*1024u)
#define RGBA_MAX (1024u*1024u*4u)
#define MAT_MAX 5
static hl2m_u8 mdlbytes[INPUT_MAX],vvdbytes[INPUT_MAX],vtxbytes[INPUT_MAX],source[INPUT_MAX];
static hl2m_u8 texbase[MAT_MAX][RGBA_MAX],texbump[MAT_MAX][RGBA_MAX];
static hl2m_mdl mdl;
static hl2m_vvd vvd;
static hl2m_vtx vtx;
static hl2m_engine_model engine;
static hl2m_vmt vmt;
static hl2m_material_fx89 fx[MAT_MAX];
static hl2m_texture_view89 base[MAT_MAX],bump[MAT_MAX];
static int matmap[MAT_MAX];
static hl2m_u8 colors[2][HL2M_MAX_ENGINE_VERTICES][3];
static int load(const char *path,hl2m_u8 *out,unsigned int cap,unsigned int *size)
{
    FILE *f=fopen(path,"rb");size_t n;
    if(!f)return 1;
    n=fread(out,1u,cap,f);if(!feof(f)){fclose(f);return 2;}fclose(f);
    *size=(unsigned int)n;return 0;
}
static int file_decode(const char *path,int format,hl2m_u8 *dst,unsigned int cap,unsigned int *out)
{
    int fixed=0;
    if(load(path,dst,cap,out))return 1;
    return hl2m_repair_missing_first_byte(dst,out,cap,format,&fixed);
}
static const char *basename89(const char *str)
{
    const char *p=str,*last=str;
    for(;*p;p++)if(*p=='/' || *p=='\\')last=p+1;
    return last;
}
static void normalize_name89(char *dst,const char *src)
{
    unsigned int i=0u;src=basename89(src);
    while(src[i] && i+1u<HL2M_PATH_MAX){
        char ch=src[i];if(ch>='A' && ch<='Z')ch=(char)(ch-'A'+'a');
        dst[i]=ch;i++;
    }
    dst[i]=0;
}
static void value(FILE *f,hl2m_fixed a)
{
    char str[40];hl2m_fixed_to_string(a,str,sizeof(str),5);fputs(str,f);
}
static int ply(const char *name,int mode)
{
    FILE *f=fopen(name,"wb");int i,k;
    if(!f)return 1;
    fprintf(f,"ply\nformat ascii 1.0\nelement vertex %d\n",engine.vertex_count);
    fputs("property float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\n",f);
    fprintf(f,"element face %d\nproperty list uchar int vertex_indices\nend_header\n",engine.index_count/3);
    for(i=0;i<engine.vertex_count;i++){
        hl2m_engine_vertex *v=&engine.vertices[i];
        value(f,v->position.x);fputc(' ',f);value(f,v->position.y);fputc(' ',f);value(f,v->position.z);
        fprintf(f," %u %u %u\n",(unsigned)colors[mode][i][0],(unsigned)colors[mode][i][1],(unsigned)colors[mode][i][2]);
    }
    for(k=0;k<engine.index_count;k+=3)
        fprintf(f,"3 %d %d %d\n",engine.indices[k],engine.indices[k+1],engine.indices[k+2]);
    fclose(f);return 0;
}
int main(int argc,char **argv)
{
    char path[1024],mapname[HL2M_PATH_MAX];unsigned int ms,vs,ts,size;
    hl2m_vtf_info info;hl2m_fragment89 frag;hl2m_u8 rgba[4];int d,i,m,r,mode,j;
    int bumps=0,changed=0;hl2m_u8 flat[4];
    if(argc!=7){fputs("usage: material_probe mdl vvd vtx texture_folder flat.ply bumped.ply\n",stderr);return 1;}
    if(strlen(argv[4])>300u)return 22;
    if(file_decode(argv[1],HL2M_FORMAT_MDL,mdlbytes,INPUT_MAX,&ms))return 2;
    if(file_decode(argv[2],HL2M_FORMAT_VVD,vvdbytes,INPUT_MAX,&vs))return 3;
    if(file_decode(argv[3],HL2M_FORMAT_VTX,vtxbytes,INPUT_MAX,&ts))return 4;
    if(hl2m_master_decode89(HL2M_FORMAT_MDL,mdlbytes,ms,0,0,&mdl))return 5;
    if(hl2m_master_decode89(HL2M_FORMAT_VVD,vvdbytes,vs,0,0,&vvd))return 6;
    if(hl2m_master_decode89(HL2M_FORMAT_VTX,vtxbytes,ts,0,0,&vtx))return 7;
    if(hl2m_check_mdl_vvd_vtx_sync(&mdl,&vvd,&vtx))return 8;
    if(hl2m_master_mesh89(&mdl,&vvd,&vtx,0,&engine))return 9;
    if(engine.draw_count>MAT_MAX)return 10;
    for(m=0;m<MAT_MAX;m++)matmap[m]=-1;
    for(d=0;d<engine.draw_count;d++){
        const char *mat=engine.draws[d].material_name;
        m=engine.draws[d].material;
        if(m<0 || m>=MAT_MAX)return 11;
        matmap[m]=m;
        sprintf(path,"%s/%s.vmt",argv[4],mat);
        if(file_decode(path,HL2M_FORMAT_VMT,source,INPUT_MAX,&size))return 12;
        if(hl2m_master_material89(source,size,&vmt,&fx[m]))return 13;
        normalize_name89(mapname,fx[m].material.base_texture);
        sprintf(path,"%s/%s.vtf",argv[4],mapname);
        if(file_decode(path,HL2M_FORMAT_VTF,source,INPUT_MAX,&size))return 14;
        if(hl2m_master_texture89(source,size,texbase[m],RGBA_MAX,&info))return 15;
        base[m].pixels=texbase[m];base[m].width=info.width;base[m].height=info.height;
        base[m].repeat_u=1;base[m].repeat_v=1;
        if(fx[m].normal_map){
            normalize_name89(mapname,fx[m].material.bumpmap);
            sprintf(path,"%s/%s.vtf",argv[4],mapname);
            if(file_decode(path,HL2M_FORMAT_VTF,source,INPUT_MAX,&size))return 16;
            if(hl2m_master_texture89(source,size,texbump[m],RGBA_MAX,&info))return 17;
            bump[m].pixels=texbump[m];bump[m].width=info.width;bump[m].height=info.height;
            bump[m].repeat_u=1;bump[m].repeat_v=1;
            bumps++;
        }
    }
    for(i=0;i<engine.vertex_count;i++){
        hl2m_engine_vertex *v=&engine.vertices[i];
        m=v->material;
        if(m<0 || m>=MAT_MAX || matmap[m]<0)return 18;
        memset(&frag,0,sizeof(frag));
        frag.light_dir.x=32768;frag.light_dir.y=21845;frag.light_dir.z=49152;
        frag.view_dir.z=65536;frag.half_dir.z=65536;
        frag.ambient=18022;frag.light_strength=49152;
        for(j=0;j<3;j++)frag.light_rgb[j]=255;
        r=hl2m_master_fragment89(&frag,&fx[m],v->normal,v->tangent,
            &base[m],fx[m].normal_map?&bump[m]:0,0,v->texcoord.x,v->texcoord.y);
        if(r)return 19;
        for(mode=0;mode<2;mode++){
            frag.have_normal_tex=mode?fx[m].normal_map:0;
            r=hl2m_master_shade89(&fx[m],&frag,rgba);if(r)return 20;
            for(j=0;j<3;j++)colors[mode][i][j]=rgba[j];
            if(!mode)memcpy(flat,rgba,4u);
            else if(rgba[0]!=flat[0] || rgba[1]!=flat[1] || rgba[2]!=flat[2])changed++;
        }
    }
    if(ply(argv[5],0) || ply(argv[6],1))return 21;
    printf("REAL_MODEL_SHADER109 PASS verts=%d triangles=%d materials=%d normal_maps=%d vertex_colors_changed=%d\n",
           engine.vertex_count,engine.index_count/3,engine.draw_count,bumps,changed);
    return 0;
}
