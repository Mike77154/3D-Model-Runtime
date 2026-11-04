/* Multipart RE4 PMD 2007 character OBJ exporter, fully C89.
   Usage: re4_character_pose MASTER.pmd CLIP.fcv FRAME OUTPUT.obj [PART.pmd ...]
   Reuses master animation for all parts using re4_character_retarget.
   Materials use names in the PMDs; put user-owned TGA beside exported OBJ. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <stdlib.h> /* CLI strtoul, no heap */
#include <string.h>
#define MAXPARTS 8
#define FILE_CAP (4UL*1024UL*1024UL)
#define ARENA_CAP (32UL*1024UL*1024UL)
static re4_u8 files[MAXPARTS][FILE_CAP];
static re4_span src[MAXPARTS];
static re4_pmd_model models[MAXPARTS];
static re4_skel_matrix bind_world[MAXPARTS][RE4_SKEL_MAX_BONES];
static re4_skel_matrix out_world[MAXPARTS][RE4_SKEL_MAX_BONES];
static re4_skel_pose bind_pose[MAXPARTS];
static re4_skel_pose animated_pose[MAXPARTS];
static re4_character_links links[MAXPARTS];
static re4_u8 arena_mem[ARENA_CAP];
static re4_u8 clipmem[2UL*1024UL*1024UL];
static re4_arena arena;
static re4_span clip;
static re4_fcv fcv;
static void printq(FILE *out,re4_q16 q)
{
    re4_u32 u,whole,frac;
    if(q<0){fputc('-',out);u=(re4_u32)(-(q+1))+1U;}else u=(re4_u32)q;
    whole=u/65536U;frac=((u%65536U)*3125U+1024U)/2048U;
    if(frac==100000U){++whole;frac=0;}
    fprintf(out,"%lu.%05lu",(unsigned long)whole,(unsigned long)frac);
}
static re4_err load_pmd(const char *path,int index)
{
    FILE *f;long n;
    f=fopen(path,"rb");if(!f){perror(path);return RE4_ERR_BAD_FORMAT;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return RE4_ERR_RANGE;}
    n=ftell(f);if(n<58L||(unsigned long)n>FILE_CAP){fclose(f);return RE4_ERR_RANGE;}
    rewind(f);
    if(fread(files[index],1,(size_t)n,f)!=(size_t)n){fclose(f);return RE4_ERR_RANGE;}
    fclose(f);src[index].data=files[index];src[index].size=(re4_size)n;
    return re4_pmd_read_survey(src[index],&arena,&models[index]);
}
static re4_err load_clip(const char *path)
{
    FILE *f;long n;re4_err er;
    clip.data=0;clip.size=0;if(strcmp(path,"-")==0)return RE4_OK;
    f=fopen(path,"rb");if(!f){perror(path);return RE4_ERR_BAD_FORMAT;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return RE4_ERR_RANGE;}
    n=ftell(f);if(n<16L||(unsigned long)n>sizeof(clipmem)){fclose(f);return RE4_ERR_RANGE;}
    rewind(f);if(fread(clipmem,1,(size_t)n,f)!=(size_t)n){fclose(f);return RE4_ERR_RANGE;}
    fclose(f);clip.data=clipmem;clip.size=(re4_size)n;
    er=re4_fcv_read(clip,RE4_ENDIAN_LITTLE,&arena,&fcv);
    if(er!=RE4_OK)return er;
    return re4_fcv_validate(clip,&fcv);
}
static int emit_part(FILE *obj,FILE *mtl,int idx,re4_u32 *vertex_base,re4_u32 *moved)
{
    re4_pmd_model *model=&models[idx];re4_span bytes=src[idx];re4_err er;
    re4_u32 i,j,k,t;re4_q16 p[3],n[3];re4_pmd_vertex_q16 v;re4_pmd_triangle tri;
    for(i=0;i<model->material_count;++i){
        re4_strref name=model->materials[i].texture_name;
        fprintf(mtl,"newmtl piece%u_mat%lu\nKd 1 1 1\n",idx,(unsigned long)i);
        if(name.len && name.len<256UL){fputs("map_Kd ",mtl);fwrite(name.ptr,1,(size_t)name.len,mtl);fputc('\n',mtl);}
        fputc('\n',mtl);
    }
    for(i=0;i<model->node_count;++i)for(j=0;j<model->nodes[i].mesh_count;++j){
        const re4_pmd_mesh *mesh=&model->nodes[i].meshes[j];
        re4_u32 mat=mesh->texture_index>=0?(re4_u32)mesh->texture_index:0U;
        if(mat>=model->material_count)mat=0;
        fprintf(obj,"o part_%u_node_%lu_mesh_%lu\nusemtl piece%u_mat%lu\n",idx,(unsigned long)i,(unsigned long)j,idx,(unsigned long)mat);
        for(k=0;k<mesh->vertex_count;++k){
            er=re4_pmd_mesh_vertex(bytes,mesh,k,&v);if(er!=RE4_OK)return 1;
            er=re4_skel_skin_vertex(bytes,&model->nodes[i],&v,&animated_pose[idx],p,n);
            if(er!=RE4_OK){fprintf(stderr,"SKIN FAIL part=%u node=%lu vert=%lu err=%s\n",idx,(unsigned long)i,(unsigned long)k,re4_err_name(er));return 1;}
            if(p[0]!=v.position[0]||p[1]!=v.position[1]||p[2]!=v.position[2])++*moved;
            fputs("v ",obj);printq(obj,p[0]);fputc(' ',obj);printq(obj,p[1]);fputc(' ',obj);printq(obj,p[2]);fputc('\n',obj);
        }
        for(k=0;k<mesh->vertex_count;++k){
            er=re4_pmd_mesh_vertex(bytes,mesh,k,&v);if(er!=RE4_OK)return 1;
            fputs("vt ",obj);printq(obj,v.uv[0]);fputc(' ',obj);printq(obj,(v.uv[1] > -2000000000 && v.uv[1] < 2000000000) ? 65536-v.uv[1] : 0);fputc('\n',obj);
        }
        for(k=0;k<mesh->vertex_count;++k){
            er=re4_pmd_mesh_vertex(bytes,mesh,k,&v);if(er!=RE4_OK)return 1;
            er=re4_skel_skin_vertex(bytes,&model->nodes[i],&v,&animated_pose[idx],p,n);
            if(er!=RE4_OK)return 1;
            fputs("vn ",obj);printq(obj,n[0]);fputc(' ',obj);printq(obj,n[1]);fputc(' ',obj);printq(obj,n[2]);fputc('\n',obj);
        }
        for(t=0;t<mesh->index_count/3U;++t){
            er=re4_pmd_mesh_triangle(bytes,mesh,t,&tri);if(er!=RE4_OK)return 1;
            fprintf(obj,"f ");
            for(k=0;k<3;++k){
                re4_u32 id=*vertex_base+(re4_u32)tri.vertex[k]+1U;
                fprintf(obj,"%lu/%lu/%lu%s",(unsigned long)id,(unsigned long)id,(unsigned long)id,k==2?"\n":" ");
            }
        }
        *vertex_base+=mesh->vertex_count;
    }
    return 0;
}
int main(int argc,char **argv)
{
    int i,parts;re4_err er;re4_u32 frame,base=0,moved=0;
    FILE *obj,*mtl;char mtlfile[1024];const char *basename;
    if(argc<6||argc>4+MAXPARTS){fprintf(stderr,"Usage: %s master.pmd clip.fcv frame output.obj part1.pmd [part2.pmd ...]\n",argv[0]);return 2;}
    parts=argc-4; /* master plus N accessory models */
    if(parts>MAXPARTS)return 2;
    frame=(re4_u32)strtoul(argv[3],0,10);if(frame>65535U)return 2;
    if(strlen(argv[4])+5>=sizeof(mtlfile))return 2;
    re4_arena_init(&arena,arena_mem,ARENA_CAP);
    for(i=0;i<parts;++i){
        er=load_pmd((i==0)?argv[1]:argv[i+4],i);
        if(er!=RE4_OK){fprintf(stderr,"Failed PMD %d: %s\n",i,re4_err_name(er));return 2;}
        memset(&bind_pose[i],0,sizeof(bind_pose[i]));bind_pose[i].world=bind_world[i];
        er=re4_skel_evaluate(&models[i],clip,0,0,&bind_pose[i],RE4_SKEL_MAX_BONES);
        if(er!=RE4_OK){fprintf(stderr,"Bind PMD %d: %s\n",i,re4_err_name(er));return 2;}
        animated_pose[i].world=out_world[i];
    }
    er=load_clip(argv[2]);if(er!=RE4_OK){fprintf(stderr,"Clip: %s\n",re4_err_name(er));return 2;}
    er=re4_skel_evaluate(&models[0],clip,clip.data?&fcv:0,(re4_u16)frame,&animated_pose[0],RE4_SKEL_MAX_BONES);
    if(er!=RE4_OK){fprintf(stderr,"Master: %s\n",re4_err_name(er));return 2;}
    for(i=1;i<parts;++i){
        er=re4_character_link(&models[0],&models[i],&links[i]);if(er!=RE4_OK)return 2;
        er=re4_character_retarget(&models[0],&bind_pose[0],&animated_pose[0],&models[i],&bind_pose[i],&links[i],&animated_pose[i],RE4_SKEL_MAX_BONES);
        if(er!=RE4_OK){fprintf(stderr,"Retarget part %d: %s\n",i,re4_err_name(er));return 2;}
        fprintf(stderr,"PART %d: direct=%lu inherited=%lu stationary=%lu\n",i,
                (unsigned long)links[i].direct_links,(unsigned long)links[i].inherited_links,(unsigned long)links[i].stationary_links);
    }
    obj=fopen(argv[4],"wb");if(!obj){perror(argv[4]);return 2;}
    strcpy(mtlfile,argv[4]);strcat(mtlfile,".mtl");mtl=fopen(mtlfile,"wb");if(!mtl){fclose(obj);return 2;}
    basename=strrchr(mtlfile,'/');if(!basename)basename=strrchr(mtlfile,'\\');basename=basename?basename+1:mtlfile;
    fprintf(obj,"# RE4 multipart PMD: shared master skeleton frame %lu\nmtllib %s\n",(unsigned long)frame,basename);
    for(i=0;i<parts;++i){if(emit_part(obj,mtl,i,&base,&moved)){fclose(obj);fclose(mtl);return 2;}}
    fclose(obj);fclose(mtl);
    printf("CHARACTER OK parts=%d vertices=%lu moved=%lu frame=%lu\n",parts,(unsigned long)base,(unsigned long)moved,(unsigned long)frame);
    return 0;
}
