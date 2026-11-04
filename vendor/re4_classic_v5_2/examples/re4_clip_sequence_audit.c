/* Exhaustive FCV FK scan for a single PMD + FCV. RE4 PC 2007 LE only.
   This tool intentionally does not certify IK, root motion, accessory meshes,
   texture correctness, or native-game timing. C89; fixed storage; no heap. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PMD_CAP (16UL*1024UL*1024UL)
#define FCV_CAP (2UL*1024UL*1024UL)
#define ARENA_CAP (16UL*1024UL*1024UL)
static re4_u8 pmd_bytes[PMD_CAP],fcv_bytes[FCV_CAP],arena_buf[ARENA_CAP];
static re4_skel_matrix matrices[RE4_SKEL_MAX_BONES], previous[RE4_SKEL_MAX_BONES];
static int load_file(const char *path,re4_u8 *buf,re4_size cap,re4_span *out)
{
    FILE *f;long n;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0||(n=ftell(f))<0||(unsigned long)n>(unsigned long)cap||
       fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buf,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);out->data=buf;out->size=(re4_size)n;return 1;
}
static re4_u32 abs_whole_delta(re4_i32 a,re4_i32 b)
{
    re4_i32 x=a/65536L,y=b/65536L;
    return x>=y?(re4_u32)(x-y):(re4_u32)(y-x);
}
static int emit(const char *status,re4_u32 frames,re4_u32 expected,
                re4_u32 changed,re4_u32 step,re4_u32 saturated,
                re4_u32 ignored,re4_u32 fail_frame)
{
    printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",status,(unsigned long)frames,
        (unsigned long)expected,(unsigned long)changed,(unsigned long)step,
        (unsigned long)saturated,(unsigned long)ignored,(unsigned long)fail_frame);
    return strcmp(status,"FK_ALL_FRAMES_OK")==0?0:1;
}
int main(int argc,char **argv)
{
    re4_span pmd_span,fcv_span;re4_arena arena;re4_pmd_model model;re4_fcv clip;
    re4_skel_pose pose;re4_clip_coverage coverage;re4_err e;re4_u32 cap=2048U,frame,expected,bone,elem;
    re4_u32 changed=0U,maxstep=0U,sat=0U,ignored=0U,ran=0U;
    if(argc!=3 && argc!=4){fputs("usage: re4_clip_sequence_audit MODEL.pmd CLIP.fcv [frame_limit]\n",stderr);return 2;}
    if(argc==4){char *end=0;unsigned long v=strtoul(argv[3],&end,10);
       if(!argv[3][0]||!end||*end||v<1UL||v>16384UL){fputs("invalid frame_limit\n",stderr);return 2;}
       cap=(re4_u32)v;
    }
    if(!load_file(argv[1],pmd_bytes,PMD_CAP,&pmd_span)||
       !load_file(argv[2],fcv_bytes,FCV_CAP,&fcv_span))
       return emit("IO_ERROR",0,0,0,0,0,0,0);
    re4_arena_init(&arena,arena_buf,ARENA_CAP);
    memset(&model,0,sizeof(model));memset(&clip,0,sizeof(clip));
    e=re4_pmd_read_survey(pmd_span,&arena,&model);
    if(e!=RE4_OK || model.skeleton_count>RE4_SKEL_MAX_BONES)
        return emit("PMD_INVALID",0,0,0,0,0,0,0);
    e=re4_fcv_read(fcv_span,RE4_ENDIAN_LITTLE,&arena,&clip);
    if(e==RE4_OK)e=re4_fcv_validate(fcv_span,&clip);
    if(e!=RE4_OK)return emit("FCV_INVALID",0,0,0,0,0,0,0);
    e=re4_character_clip_coverage(&model,&clip,&coverage);
    if(e!=RE4_OK)return emit("COVERAGE_ERROR",0,0,0,0,0,0,0);
    if(!coverage.rotation_channels)return emit("NO_ROTATION_CHANNELS",0,0,0,0,0,0,0);
    if(!coverage.linked_rotation_channels)return emit("UNMATCHED_ROTATION_CHANNELS",0,0,0,0,0,0,0);
    expected=(re4_u32)clip.max_time+1U;
    if(expected>cap)return emit("FRAME_LIMIT_NOT_SCANNED",0,expected,0,0,0,0,0);
    memset(&pose,0,sizeof(pose));pose.world=matrices;
    for(frame=0U;frame<expected;++frame){
        re4_u32 any_change=0U;
        e=re4_skel_evaluate(&model,fcv_span,&clip,(re4_u16)frame,&pose,RE4_SKEL_MAX_BONES);
        if(e!=RE4_OK)return emit("FK_EVALUATION_ERROR",ran,expected,changed,maxstep,sat,ignored,frame);
        if(frame==0U)ignored=pose.ignored_special_channels;
        ++ran;
        for(bone=0U;bone<model.skeleton_count;++bone){
            re4_u32 k;
            if(frame>0U){
                re4_u32 difference=0U;
                for(k=0U;k<12U;++k){
                    re4_u32 d=abs_whole_delta(matrices[bone].m[k],previous[bone].m[k]);
                    if(d>maxstep)maxstep=d;
                    if(matrices[bone].m[k]!=previous[bone].m[k])difference=1U;
                }
                if(difference)any_change=1U;
            }
            for(elem=0U;elem<12U;++elem){
                re4_i32 v=matrices[bone].m[elem];
                if(v>=RE4_I32_MAX-255 || v<=RE4_I32_MIN+255)++sat;
            }
            previous[bone]=matrices[bone];
        }
        if(any_change)++changed;
    }
    return emit(sat?"FK_SATURATION_REVIEW":"FK_ALL_FRAMES_OK",ran,expected,changed,maxstep,sat,ignored,0);
}
