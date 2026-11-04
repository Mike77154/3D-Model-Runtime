/* Offline PMD to OBJ static reference mesh exporter, C89, no heap/float.
   Writes MTL with original TGA names.  Copy matching TGA next to OBJ. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

#define PMD_BUFFER_SIZE (16UL*1024UL*1024UL)
#define PMD_ARENA_SIZE (8UL*1024UL*1024UL)
static re4_u8 file_data[PMD_BUFFER_SIZE];
static re4_u8 arena_data[PMD_ARENA_SIZE];

static void qnum(FILE *f,re4_q16 x)
{
    re4_u32 m;re4_u32 whole,frac;
    if(x<0){fputc('-',f);m=(re4_u32)(-(x+1))+1U;}else m=(re4_u32)x;
    whole=m/65536U;frac=((m%65536U)*3125U+1024U)/2048U;
    if(frac==100000U){++whole;frac=0U;}
    fprintf(f,"%lu.%05lu",(unsigned long)whole,(unsigned long)frac);
}

int main(int argc,char **argv)
{
    FILE *in,*obj,*mtl;char mtlpath[1024];const char *mtlbase;
    re4_span src;re4_arena arena;re4_pmd_model model;re4_err er;
    re4_u32 i,j,k,m,tri,vbase;re4_pmd_vertex_q16 vert;re4_pmd_triangle face;
    re4_size size;int invalid=0;
    if(argc!=3){fprintf(stderr,"Usage: re4_pmd_to_obj model.pmd output.obj\n");return 2;}
    if(strlen(argv[2])+5UL>=sizeof(mtlpath)){fprintf(stderr,"Output name too long\n");return 2;}
    in=fopen(argv[1],"rb");if(in==0){perror(argv[1]);return 2;}
    if(fseek(in,0L,SEEK_END)!=0){fclose(in);return 2;}
    size=(re4_size)ftell(in);rewind(in);
    if(size>PMD_BUFFER_SIZE || size<58UL){fclose(in);fprintf(stderr,"Invalid/oversized PMD\n");return 2;}
    if(fread(file_data,1,(size_t)size,in)!=(size_t)size){fclose(in);return 2;}fclose(in);
    src.data=file_data;src.size=size;
    re4_arena_init(&arena,arena_data,PMD_ARENA_SIZE);
    er=re4_pmd_read_survey(src,&arena,&model);
    if(er!=RE4_OK){fprintf(stderr,"PMD survey: %s\n",re4_err_name(er));return 2;}
    obj=fopen(argv[2],"wb");if(obj==0){perror(argv[2]);return 2;}
    strcpy(mtlpath,argv[2]);strcat(mtlpath,".mtl");mtl=fopen(mtlpath,"wb");if(mtl==0){fclose(obj);return 2;}
    mtlbase=strrchr(mtlpath,'/');if(mtlbase==0)mtlbase=strrchr(mtlpath,'\\');if(mtlbase==0)mtlbase=mtlpath;else ++mtlbase;
    fprintf(obj,"# RE4 PMD 2007 static OBJ export (unanimated)\nmtllib %s\n",mtlbase);
    for(i=0U;i<model.material_count;++i){
        re4_strref nm=model.materials[i].texture_name;
        fprintf(mtl,"newmtl material_%lu\nKd 1 1 1\n",(unsigned long)i);
        if(nm.len>0UL && nm.len<256UL){fputs("map_Kd ",mtl);fwrite(nm.ptr,1,(size_t)nm.len,mtl);fputc('\n',mtl);}fputc('\n',mtl);
    }
    vbase=0U;
    for(i=0U;i<model.node_count;++i){
        for(j=0U;j<model.nodes[i].mesh_count;++j){
            const re4_pmd_mesh *mesh=&model.nodes[i].meshes[j];
            fprintf(obj,"o node_%lu_mesh_%lu\n",(unsigned long)i,(unsigned long)j);
            m=(mesh->texture_index>=0)?(re4_u32)mesh->texture_index:0U;
            if(m>=model.material_count)m=0U;
            fprintf(obj,"usemtl material_%lu\n",(unsigned long)m);
            for(k=0U;k<mesh->vertex_count;++k){
                er=re4_pmd_mesh_vertex(src,mesh,k,&vert);
                if(er!=RE4_OK){invalid=1;break;}
                fputs("v ",obj);qnum(obj,vert.position[0]);fputc(' ',obj);qnum(obj,vert.position[1]);fputc(' ',obj);qnum(obj,vert.position[2]);fputc('\n',obj);
            }
            if(invalid)break;
            for(k=0U;k<mesh->vertex_count;++k){
                er=re4_pmd_mesh_vertex(src,mesh,k,&vert);if(er!=RE4_OK){invalid=1;break;}
                fputs("vt ",obj);qnum(obj,vert.uv[0]);fputc(' ',obj);qnum(obj,(vert.uv[1] > -2000000000 && vert.uv[1] < 2000000000) ? (65536 - vert.uv[1]) : 0);fputc('\n',obj);
            }
            if(invalid)break;
            for(k=0U;k<mesh->vertex_count;++k){
                er=re4_pmd_mesh_vertex(src,mesh,k,&vert);if(er!=RE4_OK){invalid=1;break;}
                fputs("vn ",obj);qnum(obj,vert.normal[0]);fputc(' ',obj);qnum(obj,vert.normal[1]);fputc(' ',obj);qnum(obj,vert.normal[2]);fputc('\n',obj);
            }
            if(invalid)break;
            for(tri=0U;tri<mesh->index_count/3U;++tri){
                er=re4_pmd_mesh_triangle(src,mesh,tri,&face);if(er!=RE4_OK){invalid=1;break;}
                fprintf(obj,"f ");
                for(k=0U;k<3U;++k){
                    re4_u32 id=vbase+1U+(re4_u32)face.vertex[k];
                    fprintf(obj,"%lu/%lu/%lu%s",(unsigned long)id,(unsigned long)id,(unsigned long)id,(k==2U)?"\n":" ");
                }
            }
            if(invalid)break;
            vbase+=mesh->vertex_count;
        }
        if(invalid)break;
    }
    fclose(obj);fclose(mtl);
    if(invalid){fprintf(stderr,"Malformed mesh in PMD\n");return 2;}
    printf("PMD OBJ OK nodes=%lu materials=%lu vertices=%lu\n",(unsigned long)model.node_count,(unsigned long)model.material_count,(unsigned long)vbase);
    return 0;
}
