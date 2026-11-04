/* Batch-friendly read-only FCV / PMD compatibility inspector.
   Caller-allocated storage, strict C89. Scans endpoint poses to distinguish
   a decodable FCV from one usable with the selected skeleton. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define PMD_CAP (16UL*1024UL*1024UL)
#define FCV_CAP (2UL*1024UL*1024UL)
#define ARENA_CAP (16UL*1024UL*1024UL)
static re4_u8 pmd_buf[PMD_CAP];
static re4_u8 fcv_buf[FCV_CAP];
static re4_u8 arena_buf[ARENA_CAP];
static re4_skel_matrix mat[RE4_SKEL_MAX_BONES];
static int read_file(const char *path,re4_u8 *buf,re4_size cap,re4_span *s)
{
    FILE *f; long n;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0 || (n=ftell(f))<0 ||
       (unsigned long)n>(unsigned long)cap || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buf,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);s->data=buf;s->size=(re4_size)n;return 1;
}
static int report(const char *status, re4_u32 bones, re4_u32 rotations,
                  re4_u32 linked, re4_u32 distinct, re4_u32 special,
                  re4_u32 matched_names, re4_u32 max_time,
                  re4_u32 frames_ok, re4_u32 ignored)
{
    printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",status,
       (unsigned long)bones,(unsigned long)rotations,(unsigned long)linked,
       (unsigned long)distinct,(unsigned long)special,(unsigned long)matched_names,
       (unsigned long)max_time,(unsigned long)frames_ok,(unsigned long)ignored);
    return strcmp(status,"FK_OK")==0 || strcmp(status,"FCV_NO_ROTATIONS")==0 ||
           strcmp(status,"FCV_UNMATCHED_BONES")==0 ? 0:1;
}
int main(int argc,char **argv)
{
    re4_arena arena;
    re4_span pmd_bytes,fcv_bytes;
    re4_pmd_model model;
    re4_fcv clip;
    re4_clip_coverage coverage;
    re4_skel_pose pose;
    re4_err e;
    re4_u32 frame_ok=0U,ignored=0U,i;
    re4_u16 times[3];
    if(argc!=3){fputs("usage: re4_clip_coverage MODEL.pmd CLIP.fcv\n",stderr);return 2;}
    memset(&model,0,sizeof(model));memset(&clip,0,sizeof(clip));
    if(!read_file(argv[1],pmd_buf,PMD_CAP,&pmd_bytes))
        return report("PMD_IO",0,0,0,0,0,0,0,0,0);
    re4_arena_init(&arena,arena_buf,ARENA_CAP);
    e=re4_pmd_read_survey(pmd_bytes,&arena,&model);
    if(e!=RE4_OK || model.skeleton_count>RE4_SKEL_MAX_BONES)
        return report("PMD_PARSE",0,0,0,0,0,0,0,0,0);
    if(!read_file(argv[2],fcv_buf,FCV_CAP,&fcv_bytes))
        return report("FCV_IO",model.skeleton_count,0,0,0,0,0,0,0,0);
    e=re4_fcv_read(fcv_bytes,RE4_ENDIAN_LITTLE,&arena,&clip);
    if(e==RE4_OK)e=re4_fcv_validate(fcv_bytes,&clip);
    if(e!=RE4_OK)return report("FCV_PARSE",model.skeleton_count,0,0,0,0,0,0,0,0);
    e=re4_character_clip_coverage(&model,&clip,&coverage);
    if(e!=RE4_OK)return report("COVERAGE_ERROR",model.skeleton_count,0,0,0,0,0,0,0,0);
    if(!coverage.rotation_channels || !coverage.linked_rotation_channels){
        const char *msg=coverage.rotation_channels?"FCV_UNMATCHED_BONES":"FCV_NO_ROTATIONS";
        return report(msg,coverage.skeleton_bones,coverage.rotation_channels,
            coverage.linked_rotation_channels,coverage.unique_linked_bones,
            coverage.special_channels,coverage.name_mapped_bones,clip.max_time,0,0);
    }
    times[0]=0;times[1]=(re4_u16)(clip.max_time/2U);times[2]=clip.max_time;
    memset(&pose,0,sizeof(pose));pose.world=mat;
    for(i=0;i<3U;++i){
        e=re4_skel_evaluate(&model,fcv_bytes,&clip,times[i],&pose,RE4_SKEL_MAX_BONES);
        if(e!=RE4_OK)break;
        ++frame_ok;
        if(i==0)ignored=pose.ignored_special_channels;
    }
    return report(frame_ok==3U?"FK_OK":"FK_EVAL_ERROR",
        coverage.skeleton_bones,coverage.rotation_channels,coverage.linked_rotation_channels,
        coverage.unique_linked_bones,coverage.special_channels,
        coverage.name_mapped_bones,clip.max_time,frame_ok,ignored);
}
