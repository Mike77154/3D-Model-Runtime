/* RE4 PMD/FCV compatibility probe -- strictly C89, caller-owned buffers.
   Outputs one CSV record per invocation; no model assets are embedded.
   A successful FK evaluation is not proof of semantically matching IK/clips. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

#define PMD_CAP (16UL*1024UL*1024UL)
#define FCV_CAP (2UL*1024UL*1024UL)
#define ARENA_CAP (16UL*1024UL*1024UL)
#define BONES_CAP RE4_SKEL_MAX_BONES
static re4_u8 pmd_mem[PMD_CAP];
static re4_u8 fcv_mem[FCV_CAP];
static re4_u8 arena_mem[ARENA_CAP];
static re4_skel_matrix bone_matrices[BONES_CAP];

static int load_file(const char *path,re4_u8 *buf,re4_size limit,re4_span *out)
{
    FILE *f;
    long n;
    f=fopen(path,"rb");
    if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0 || (n=ftell(f))<0L ||
       (unsigned long)n>(unsigned long)limit || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buf,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);
    out->data=buf;out->size=(re4_size)n;
    return 1;
}
static void report(const char *state,const re4_pmd_model *m,
                   const re4_fcv *fcv,const re4_skel_pose *pose)
{
    printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",state,
      (unsigned long)(m?m->skeleton_count:0U),
      (unsigned long)(m?m->node_count:0U),
      (unsigned long)(m?m->material_count:0U),
      (unsigned long)(m?m->total_mesh_count:0U),
      (unsigned long)(m?m->total_vertex_count_hint:0U),
      (unsigned long)(m?m->total_index_count_hint:0U),
      (unsigned long)(fcv?fcv->max_time:0U),
      (unsigned long)(fcv?fcv->block_count:0U),
      (unsigned long)(pose?pose->applied_rotation_channels:0U),
      (unsigned long)(pose?pose->ignored_special_channels:0U));
}
int main(int argc,char **argv)
{
    re4_span bytes,fcv_bytes;
    re4_pmd_model model;
    re4_fcv fcv;
    re4_skel_pose pose;
    re4_arena arena;
    re4_err err;
    re4_u16 time;
    int has_fcv;
    if(argc!=2 && argc!=3){
        fputs("usage: re4_character_probe MODEL.pmd [ANIMATION.fcv]\n",stderr);
        return 2;
    }
    memset(&model,0,sizeof(model));memset(&fcv,0,sizeof(fcv));
    memset(&pose,0,sizeof(pose));
    if(!load_file(argv[1],pmd_mem,PMD_CAP,&bytes)){
        report("PMD_IO_OR_SIZE",0,0,0);return 3;
    }
    re4_arena_init(&arena,arena_mem,ARENA_CAP);
    err=re4_pmd_read_survey(bytes,&arena,&model);
    if(err!=RE4_OK){report(re4_err_name(err),0,0,0);return 4;}
    if(model.skeleton_count>BONES_CAP){report("BONE_LIMIT",&model,0,0);return 5;}
    pose.world=bone_matrices;
    fcv_bytes.data=0;fcv_bytes.size=0;
    has_fcv=argc==3 && strcmp(argv[2],"-")!=0;
    if(has_fcv){
        if(!load_file(argv[2],fcv_mem,FCV_CAP,&fcv_bytes)){
            report("FCV_IO_OR_SIZE",&model,0,0);return 6;
        }
        err=re4_fcv_read(fcv_bytes,RE4_ENDIAN_LITTLE,&arena,&fcv);
        if(err==RE4_OK)err=re4_fcv_validate(fcv_bytes,&fcv);
        if(err!=RE4_OK){report("FCV_PARSE_ERROR",&model,0,0);return 7;}
    }
    time=has_fcv?(re4_u16)(fcv.max_time/2U):0U;
    err=re4_skel_evaluate(&model,fcv_bytes,has_fcv?&fcv:0,time,&pose,BONES_CAP);
    if(err!=RE4_OK){report("FK_EVAL_ERROR",&model,has_fcv?&fcv:0,0);return 8;}
    report(has_fcv?(pose.applied_rotation_channels>0U?"FK_ROTATIONS":"FK_NO_MATCHING_ROTATIONS"):"REST_OK",
           &model,has_fcv?&fcv:0,&pose);
    return 0;
}
