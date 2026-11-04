/* RE4 PC 2007 PMD/FCV multipart full-vertex sequence audit.
   C89; fixed-point Q16.16; no malloc or external model libraries.
   Diagnostics, not a certification of original IK or canon attachments.
   All output measurements use PMD world/model coordinates. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define AUD_PARTS 8U
#define PMD_BYTES (16UL*1024UL*1024UL)
#define FCV_BYTES (2UL*1024UL*1024UL)
#define ARENA_BYTES (64UL*1024UL*1024UL)
#define VERTEX_CAP 400000UL
static re4_u8 files[AUD_PARTS][PMD_BYTES],fcv_buf[FCV_BYTES],arena_buf[ARENA_BYTES];
static re4_span spans[AUD_PARTS],fcv_span;
static re4_pmd_model models[AUD_PARTS];
static re4_skel_matrix rest_m[AUD_PARTS][RE4_SKEL_MAX_BONES],anim_m[AUD_PARTS][RE4_SKEL_MAX_BONES];
static re4_skel_pose rest_p[AUD_PARTS],anim_p[AUD_PARTS];
static re4_character_links links[AUD_PARTS];
static re4_q16 prev_xyz[VERTEX_CAP][3];
static re4_arena arena;
static re4_fcv clip;
static int read_bytes(const char *name,re4_u8 *dst,unsigned long cap,re4_span *span)
{
    FILE *f;long len;
    f=fopen(name,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0||(len=ftell(f))<0 || (unsigned long)len>cap ||
       fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(dst,1U,(size_t)len,f)!=(size_t)len){fclose(f);return 0;}
    fclose(f);span->data=dst;span->size=(re4_size)len;return 1;
}
/* Convert signed fixed-point differences to unsigned without signed overflow. */
static re4_u32 delta_q16(re4_i32 a,re4_i32 b)
{
    return a>=b ? (re4_u32)a-(re4_u32)b : (re4_u32)b-(re4_u32)a;
}
static void report_failure(const char *label,unsigned int part,unsigned long node,
                           unsigned long vertex,re4_err err)
{
    fprintf(stderr,"AUDIT_FAIL,%s,part=%u,node=%lu,vertex=%lu,err=%s\n",
            label,part,node,vertex,re4_err_name(err));
}
int main(int argc,char **argv)
{
    re4_u32 first,last,step,frame,part_count,p,i,j,k,t,offset,total=0U;
    re4_u32 scanned=0U,alerts=0U,max_gap=0U,link_stationary=0U,changed_frames=0U,near_components=0U;
    re4_u32 threshold=250U;re4_err e;re4_clip_coverage cov;
    int reviewed=0,summary=0,arg_end=argc;char *tail;
    /* Options are parsed only from the trailing argument list. */
    while(arg_end>1){
        if(strcmp(argv[arg_end-1],"--review-ancestry")==0){reviewed=1;--arg_end;}
        else if(strcmp(argv[arg_end-1],"--summary")==0){summary=1;--arg_end;}
        else break;
    }
    if(arg_end<6 || arg_end>13){
        fputs("usage: re4_mesh_sequence_audit MASTER.pmd CLIP.fcv FIRST LAST STEP [PART.pmd ...] [--review-ancestry] [--summary]\n",stderr);
        return 2;
    }
    part_count=(re4_u32)(arg_end-5);
    if(part_count>AUD_PARTS)return 2;
    first=(re4_u32)strtoul(argv[3],&tail,10);if(!argv[3][0]||*tail)return 2;
    last=(re4_u32)strtoul(argv[4],&tail,10);if(!argv[4][0]||*tail)return 2;
    step=(re4_u32)strtoul(argv[5],&tail,10);if(!argv[5][0]||*tail)return 2;
    if(first>last || last>65535U || step==0U || (last-first)/step+1U>8192U)return 2;
    re4_arena_init(&arena,arena_buf,ARENA_BYTES);
    memset(&clip,0,sizeof(clip));
    for(p=0U;p<part_count;++p){
        const char *path=p==0U?argv[1]:argv[p+5U];
        if(!read_bytes(path,files[p],PMD_BYTES,&spans[p])){
            fprintf(stderr,"PMD_IO_ERROR,%s\n",path);return 3;
        }
        memset(&models[p],0,sizeof(models[p]));
        e=re4_pmd_read_survey(spans[p],&arena,&models[p]);
        if(e!=RE4_OK || models[p].skeleton_count>RE4_SKEL_MAX_BONES){
            report_failure("PMD_READ",(unsigned int)p,0U,0U,e);return 3;
        }
        rest_p[p].world=rest_m[p];anim_p[p].world=anim_m[p];
        e=re4_skel_evaluate(&models[p],spans[p],0,(re4_u16)0U,
                            &rest_p[p],RE4_SKEL_MAX_BONES);
        if(e!=RE4_OK){report_failure("REST",(unsigned int)p,0U,0U,e);return 3;}
        if(p>0U){
            re4_character_link_audit audit;
            e=re4_character_link(&models[0],&models[p],&links[p]);
            if(e==RE4_OK)e=re4_character_audit(&models[0],&models[p],&links[p],&audit);
            if(e==RE4_OK)e=re4_character_check_policy(&audit,reviewed?
                             RE4_PART_POLICY_REVIEW_ANCESTRY:RE4_PART_POLICY_STRICT);
            if(e!=RE4_OK){report_failure("PART_LINK",(unsigned int)p,0U,0U,e);return 4;}
            link_stationary+=links[p].stationary_links;
            if(!summary)fprintf(stderr,"PART_LINK,%u,direct=%lu,inherited=%lu,stationary=%lu,ancestry=%lu\n",
                    (unsigned int)p,(unsigned long)audit.direct,(unsigned long)audit.inherited,
                    (unsigned long)audit.stationary,(unsigned long)audit.ancestry_conflicts);
        }
        for(i=0U;i<models[p].node_count;++i){
            for(j=0U;j<models[p].nodes[i].mesh_count;++j){
                const re4_pmd_mesh *mesh=&models[p].nodes[i].meshes[j];
                if(mesh->vertex_count>VERTEX_CAP-total){
                    fputs("VERTEX_CAP_EXCEEDED\n",stderr);return 4;
                }
                total+=mesh->vertex_count;
                for(t=0U;t<mesh->index_count/3U;++t){
                    re4_pmd_triangle tri;
                    e=re4_pmd_mesh_triangle(spans[p],mesh,t,&tri);
                    if(e!=RE4_OK){report_failure("TRIANGLE",(unsigned int)p,i,t,e);return 4;}
                    if((re4_u32)tri.vertex[0]>=mesh->vertex_count ||
                       (re4_u32)tri.vertex[1]>=mesh->vertex_count ||
                       (re4_u32)tri.vertex[2]>=mesh->vertex_count){
                        report_failure("TRIANGLE_INDEX",(unsigned int)p,i,t,RE4_ERR_RANGE);return 4;
                    }
                }
            }
        }
    }
    if(!read_bytes(argv[2],fcv_buf,FCV_BYTES,&fcv_span)){
        fputs("FCV_IO_ERROR\n",stderr);return 3;
    }
    e=re4_fcv_read(fcv_span,RE4_ENDIAN_LITTLE,&arena,&clip);
    if(e==RE4_OK)e=re4_fcv_validate(fcv_span,&clip);
    if(e!=RE4_OK){report_failure("FCV",0U,0U,0U,e);return 3;}
    if(last>clip.max_time){fputs("FRAME_EXCEEDS_FCV\n",stderr);return 3;}
    e=re4_character_clip_coverage(&models[0],&clip,&cov);
    if(e!=RE4_OK || !cov.linked_rotation_channels){
        fputs("NO_LINKED_FCV_ROTATION\n",stderr);return 4;
    }
    if(!summary)printf("frame,vertices,changed_vertices,max_vertex_step_q16,max_vertex_step_units,"
           "x_min_q16,y_min_q16,z_min_q16,x_max_q16,y_max_q16,z_max_q16,"
           "near_i32_limits,ignored_special,stationary_part_joints\n");
    for(frame=first;frame<=last;){
        re4_u32 count=0U,changed=0U,near=0U,frame_max=0U;
        re4_i32 minxyz[3]={RE4_I32_MAX,RE4_I32_MAX,RE4_I32_MAX};
        re4_i32 maxxyz[3]={RE4_I32_MIN,RE4_I32_MIN,RE4_I32_MIN};
        e=re4_skel_evaluate(&models[0],fcv_span,&clip,(re4_u16)frame,
                            &anim_p[0],RE4_SKEL_MAX_BONES);
        if(e!=RE4_OK){report_failure("MASTER_FK",0U,0U,frame,e);return 5;}
        for(p=1U;p<part_count;++p){
            e=re4_character_retarget(&models[0],&rest_p[0],&anim_p[0],
                    &models[p],&rest_p[p],&links[p],&anim_p[p],RE4_SKEL_MAX_BONES);
            if(e!=RE4_OK){report_failure("RETARGET",(unsigned int)p,0U,frame,e);return 5;}
        }
        offset=0U;
        for(p=0U;p<part_count;++p){
            for(i=0U;i<models[p].node_count;++i){
                for(j=0U;j<models[p].nodes[i].mesh_count;++j){
                    const re4_pmd_mesh *mesh=&models[p].nodes[i].meshes[j];
                    for(k=0U;k<mesh->vertex_count;++k){
                        re4_pmd_vertex_q16 vertex;re4_q16 pos[3],norm[3];
                        re4_u32 coord;
                        e=re4_pmd_mesh_vertex(spans[p],mesh,k,&vertex);
                        if(e!=RE4_OK){report_failure("VERTEX",(unsigned int)p,i,k,e);return 6;}
                        if(models[p].nodes[i].node_bone_count==0U)
                            e=re4_skel_skin_rigid_node(&models[p].nodes[i],&vertex,
                                    &rest_p[p],&anim_p[p],pos,norm);
                        else e=re4_skel_skin_vertex(spans[p],&models[p].nodes[i],
                                    &vertex,&anim_p[p],pos,norm);
                        if(e!=RE4_OK){report_failure("SKIN",(unsigned int)p,i,k,e);return 6;}
                        if(scanned>0U){
                            int moved=0;
                            for(coord=0U;coord<3U;++coord){
                                re4_u32 d=delta_q16(pos[coord],prev_xyz[offset][coord]);
                                if(d>frame_max)frame_max=d;
                                if(d!=0U)moved=1;
                            }
                            if(moved)++changed;
                        }
                        for(coord=0U;coord<3U;++coord){
                            if(pos[coord]<minxyz[coord])minxyz[coord]=pos[coord];
                            if(pos[coord]>maxxyz[coord])maxxyz[coord]=pos[coord];
                            if(pos[coord]>RE4_I32_MAX-1024 || pos[coord]<RE4_I32_MIN+1024)++near;
                            prev_xyz[offset][coord]=pos[coord];
                        }
                        ++offset;++count;
                    }
                }
            }
        }
        if(frame_max>max_gap)max_gap=frame_max;
        /* max_gap is a raw displacement between *sampled* poses. If step>1,
           comparing that to a one-frame velocity threshold produces false
           alerts (e.g. Salazar em38 FCV42, sampled every 125 frames).
           This is only a coarse per-frame average, NOT proof that skipped
           intermediate frames have no spikes. Use STEP=1 for that. */
        if(near || (scanned>0U && frame_max/65536U/step>threshold))++alerts;
        near_components+=near;
        if(changed>0U)++changed_frames;
        if(!summary)printf("%lu,%lu,%lu,%lu,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%lu,%lu,%lu\n",
            (unsigned long)frame,(unsigned long)count,(unsigned long)changed,
            (unsigned long)frame_max,(unsigned long)(frame_max/65536U),
            (long)minxyz[0],(long)minxyz[1],(long)minxyz[2],
            (long)maxxyz[0],(long)maxxyz[1],(long)maxxyz[2],
            (unsigned long)near,(unsigned long)anim_p[0].ignored_special_channels,
            (unsigned long)link_stationary);
        ++scanned;
        if(last-frame<step)break;
        frame+=step;
    }
    if(summary)printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
        alerts?"MESH_AUDIT_ATTENTION":"MESH_AUDIT_OK",(unsigned long)scanned,(unsigned long)part_count,(unsigned long)total,
        (unsigned long)changed_frames,(unsigned long)max_gap,(unsigned long)alerts,
        (unsigned long)near_components,(unsigned long)link_stationary,
        (unsigned long)cov.linked_rotation_channels);
    else fprintf(stderr,"MESH_AUDIT_OK,frames=%lu,parts=%lu,vertices_per_frame=%lu,"
        "max_step_q16=%lu,attention_frames=%lu,stationary_part_joints=%lu,"
        "linked_rotations=%lu,IK_NATIVE=NOT_VALIDATED\n",
        (unsigned long)scanned,(unsigned long)part_count,(unsigned long)total,
        (unsigned long)max_gap,(unsigned long)alerts,(unsigned long)link_stationary,
        (unsigned long)cov.linked_rotation_channels);
    return alerts?7:0;
}
