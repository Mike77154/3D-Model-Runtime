/* Multipart RE4 PMD 2007 character OBJ exporter, fully C89.
   Usage: re4_character_pose MASTER.pmd CLIP.fcv FRAME OUTPUT.obj [PART.pmd ...]
   Reuses master animation for all parts using re4_character_retarget.
   Materials use names in the PMDs; put user-owned TGA beside exported OBJ. */
#include "re4c3d/re4c3d.h"
#include "re4c3d/re4_required_parts.h"
#include <stdio.h>
#include <stdlib.h> /* CLI strtoul, no heap */
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#define MAKE_DIR(d) mkdir(d,0775)
#endif
#define MAXPARTS 8
#define MAX_IK 256
#define FILE_CAP (16UL*1024UL*1024UL) /* large bosses, e.g. em38 > 4 MiB */
#define ARENA_CAP (64UL*1024UL*1024UL)
static re4_u8 files[MAXPARTS][FILE_CAP];
static re4_span src[MAXPARTS];
static re4_pmd_model models[MAXPARTS];
static re4_skel_matrix bind_world[MAXPARTS][RE4_SKEL_MAX_BONES];
static re4_skel_matrix out_world[MAXPARTS][RE4_SKEL_MAX_BONES];
static re4_skel_pose bind_pose[MAXPARTS];
static re4_skel_pose animated_pose[MAXPARTS];
static re4_character_links links[MAXPARTS];
static re4_u8 rigid_anchor_set[MAXPARTS];
static re4_u32 rigid_anchor_joint[MAXPARTS];
static re4_u8 arena_mem[ARENA_CAP];
static re4_u8 clipmem[2UL*1024UL*1024UL];
static re4_u8 clipmem_b[2UL*1024UL*1024UL];
static re4_arena arena;
static re4_span clip;
static re4_fcv fcv;
static re4_fcv fcv_b;
static re4_span clip_b;
static re4_root_track root_track_b;
static re4_u32 blend_percent=0;
static re4_transition blend_transition;
static int has_blend_ramp=0;
static re4_skel_matrix reference_world[RE4_SKEL_MAX_BONES];
static re4_skel_pose reference_pose;
static re4_ik_chain ik_chains[MAX_IK];
static re4_root_track root_track;
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
static re4_err load_blend_clip(const char *path)
{
    FILE *f;long n;re4_err e;
    f=fopen(path,"rb");if(!f){perror(path);return RE4_ERR_BAD_FORMAT;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return RE4_ERR_RANGE;}
    n=ftell(f);
    if(n<16L||(unsigned long)n>sizeof(clipmem_b)){fclose(f);return RE4_ERR_RANGE;}
    rewind(f);
    if(fread(clipmem_b,1,(size_t)n,f)!=(size_t)n){fclose(f);return RE4_ERR_RANGE;}
    fclose(f);clip_b.data=clipmem_b;clip_b.size=(re4_size)n;
    e=re4_fcv_read(clip_b,RE4_ENDIAN_LITTLE,&arena,&fcv_b);
    if(e!=RE4_OK)return e;
    return re4_fcv_validate(clip_b,&fcv_b);
}
/* Align each FCV's native duration, not its absolute animation frames. */
static re4_u16 blend_frame(re4_u16 a)
{
    if(!fcv.max_time)return 0;
    return (re4_u16)(((re4_u32)a*(re4_u32)fcv_b.max_time)/
                      (re4_u32)fcv.max_time);
}
static re4_err weight_at(re4_u16 frame,re4_q16 *weight)
{
    if(!weight)return RE4_ERR_NULL;
    if(has_blend_ramp)return re4_transition_weight(&blend_transition,frame,weight);
    *weight=(re4_q16)(blend_percent*65536UL/100UL);return RE4_OK;
}
static re4_err evaluate_master(re4_u16 frame,re4_skel_pose *out)
{
    re4_q16 weight;re4_err er;
    if(!clip_b.data)return re4_skel_evaluate(&models[0],clip,
            clip.data?&fcv:0,frame,out,RE4_SKEL_MAX_BONES);
    er=weight_at(frame,&weight);if(er!=RE4_OK)return er;
    return re4_skel_evaluate_blend(&models[0],clip,&fcv,frame,
        clip_b,&fcv_b,blend_frame(frame),weight,out,RE4_SKEL_MAX_BONES);
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
            if(model->nodes[i].node_bone_count==0U)
                er=re4_skel_skin_rigid_node(&model->nodes[i],&v,
                    &bind_pose[idx],&animated_pose[idx],p,n);
            else er=re4_skel_skin_vertex(bytes,&model->nodes[i],&v,&animated_pose[idx],p,n);
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
            if(model->nodes[i].node_bone_count==0U)
                er=re4_skel_skin_rigid_node(&model->nodes[i],&v,
                    &bind_pose[idx],&animated_pose[idx],p,n);
            else er=re4_skel_skin_vertex(bytes,&model->nodes[i],&v,&animated_pose[idx],p,n);
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

/* Range exporter: read each PMD/FCV once, then evaluate all multipart poses.
   Usage: re4_character_clip master.pmd clip.fcv first last stride out_dir
                               [part1.pmd ...] [--ik-lock]
   0..2048 exported frames per invocation, to bound accidental disk usage.
   --ik-lock is an EXPLICIT experimental contact constraint, not native IK. */
int main(int argc,char **argv)
{
    int i,parts,use_ik=0,use_root=0,has_window=0,has_blend=0,has_blend_pct=0,has_blend_curve=0,strict_parts=0,review_ancestry=0;
    const re4_beast_profile *creature_profile=0;
    const char *blend_path=0;re4_err er;re4_u32 start,end,stride,frame,base,moved;
    re4_u32 window_start=0,window_end=0,contact_active=0;
    re4_u16 frame_b=0;
    re4_q16 root_raw[3],root_delta[3],weight=0;
    re4_u32 ccount=0,cvalid=0,applied=0,skipped=0,frames=0;
    re4_character_link_audit audit;
    const char *required_parts[MAXPARTS-1];
    const char *provided_parts[MAXPARTS-1];
    re4_u32 required_count=0U,missing_idx=0U;
    FILE *obj,*mtl,*summary;char objpath[1024],mtlpath[1032],summpath[1024];
    const char *basename;re4_skel_pose *master=&animated_pose[0];
    if(argc<7){
       fputs("Usage: re4_character_clip master.pmd clip.fcv first last stride output_dir [part.pmd ...] [--root-apply] [--ik-lock | --ik-window=FIRST:LAST] [--blend-file=other.fcv (--blend-percent=0..100 | --blend-ramp=FIRST:LAST [--blend-curve=linear|smooth|smoother])] [--strict-parts] [--review-ancestry] [--rigid-anchor=PART_INDEX:MASTER_BONE] [--creature-profile=SLUG] [--require-part=exact_basename.pmd ...]\n",stderr);
       return 2;
    }
    /* Parse opt-in flags at the end; keep all old positional arguments. */
    while(argc>7){
        const char *flag=argv[argc-1];
        if(strncmp(flag,"--rigid-anchor=",15U)==0){
            char *tail=0;unsigned long pi,bi;
            if(!flag[15])return 2;
            pi=strtoul(flag+15,&tail,10);
            if(tail==flag+15||*tail!=':'||!tail[1]||pi==0UL||pi>=MAXPARTS)return 2;
            bi=strtoul(tail+1,&tail,10);
            if(*tail||bi>=RE4_SKEL_MAX_BONES||rigid_anchor_set[pi])return 2;
            rigid_anchor_set[pi]=1U;rigid_anchor_joint[pi]=(re4_u32)bi;
            --argc;continue;
        }
        if(strncmp(flag,"--creature-profile=",19U)==0){
            if(creature_profile || !flag[19])return 2;
            creature_profile=re4_beast_find(flag+19);
            if(!creature_profile){fprintf(stderr,"UNKNOWN_CREATURE_PROFILE %s\n",flag+19);return 2;}
            --argc;continue;
        }
        if(strncmp(flag,"--blend-file=",13U)==0){
            if(has_blend||!flag[13])return 2;
            blend_path=flag+13;has_blend=1;--argc;continue;
        }
        if(strncmp(flag,"--blend-ramp=",13U)==0){
            char *stop=0;unsigned long a,b;
            if(has_blend_ramp||!flag[13])return 2;
            a=strtoul(flag+13,&stop,10);
            if(stop==flag+13||*stop!=':'||!stop[1]||a>65535UL)return 2;
            b=strtoul(stop+1,&stop,10);
            if(*stop||b>65535UL||a>=b)return 2;
            blend_transition.first=(re4_u16)a;
            blend_transition.last=(re4_u16)b;
            if(!has_blend_curve)blend_transition.curve=RE4_TRANSITION_SMOOTHSTEP;
            has_blend_ramp=1;--argc;continue;
        }
        if(strncmp(flag,"--blend-curve=",14U)==0){
            if(has_blend_curve)return 2;
            if(strcmp(flag+14,"linear")==0)
                blend_transition.curve=RE4_TRANSITION_LINEAR;
            else if(strcmp(flag+14,"smooth")==0)
                blend_transition.curve=RE4_TRANSITION_SMOOTHSTEP;
            else if(strcmp(flag+14,"smoother")==0)
                blend_transition.curve=RE4_TRANSITION_SMOOTHERSTEP;
            else return 2;
            has_blend_curve=1;--argc;continue;
        }
        if(strncmp(flag,"--blend-percent=",16U)==0){
            char *stop=0;unsigned long n;
            if(has_blend_pct||!flag[16])return 2;
            n=strtoul(flag+16,&stop,10);
            if(stop==flag+16||*stop||n>100UL)return 2;
            blend_percent=(re4_u32)n;has_blend_pct=1;--argc;continue;
        }
        if(strncmp(flag,"--require-part=",15U)==0){
            if(required_count>=MAXPARTS-1U || !flag[15])return 2;
            required_parts[required_count++]=flag+15;--argc;continue;
        }
        if(strcmp(flag,"--root-apply")==0){use_root=1;--argc;continue;}
        if(strcmp(flag,"--strict-parts")==0){strict_parts=1;--argc;continue;}
        if(strcmp(flag,"--review-ancestry")==0){review_ancestry=1;--argc;continue;}
        if(strcmp(flag,"--ik-lock")==0){
            if(has_window)return 2;
            use_ik=1;--argc;continue;
        }
        if(strncmp(flag,"--ik-window=",12U)==0){
            const char *p=flag+12;char *tail=0;
            if(use_ik||has_window||!*p)return 2;
            window_start=(re4_u32)strtoul(p,&tail,10);
            if(tail==p||*tail!=':'||!tail[1])return 2;
            p=tail+1;window_end=(re4_u32)strtoul(p,&tail,10);
            if(tail==p||*tail!='\0'||window_start>window_end||window_end>65535UL)return 2;
            has_window=1;use_ik=1;--argc;continue;
        }
        break;
    }
    if((has_blend && ((has_blend_pct+has_blend_ramp)!=1 || strcmp(argv[2],"-")==0)) ||
       (!has_blend && (has_blend_pct||has_blend_ramp||has_blend_curve)) ||
       (has_blend_pct && has_blend_ramp) || (has_blend_curve && !has_blend_ramp)){
        fputs("Use --blend-file and exactly one of --blend-percent or --blend-ramp (with primary FCV)\n",stderr);
        return 2;
    }
    if(review_ancestry && !strict_parts){
        fputs("--review-ancestry requires --strict-parts (explicit per-part audit)\n",stderr);
        return 2;
    }
    parts=argc-6; /* master plus zero or more components */
    if(parts<1||parts>MAXPARTS)return 2;
    for(i=1;i<MAXPARTS;++i)if(rigid_anchor_set[i] && i>=parts){
        fputs("Rigid anchor refers to nonexistent part index\n",stderr);return 2;
    }
    /* Fail closed BEFORE any output when a named variant is ambiguous or
       mismatched. A general profile does not replace --strict-parts. */
    if(creature_profile){
        for(i=0;i<parts;++i){
            const char *file=i==0?argv[1]:argv[i+6];
            int status=re4_beast_check_part(creature_profile,file);
            if(status!=1){
                fprintf(stderr,"CREATURE_PROFILE_%s %s part=%s (no pose output)\n",
                    status==0?"MISMATCH":"REVIEW_REQUIRED",creature_profile->slug,file);
                return 2;
            }
        }
    }
    for(i=1;i<parts;++i)provided_parts[i-1]=argv[i+6];
    if(!re4_required_parts_check(provided_parts,(re4_u32)(parts-1),
                                 required_parts,required_count,&missing_idx)){
        fprintf(stderr,"REQUIRED_PART_MISSING_OR_DUPLICATE %s (no incomplete render exported)\n",
                required_parts[missing_idx]);
        return 2;
    }
    start=(re4_u32)strtoul(argv[3],0,10);
    end=(re4_u32)strtoul(argv[4],0,10);
    stride=(re4_u32)strtoul(argv[5],0,10);
    if(start>end||end>65535UL||stride<1UL || (end-start)/stride+1UL>2048UL){
        fputs("Invalid range, step, or more than 2048 output frames\n",stderr);return 2;
    }
    if(has_blend_ramp &&
       (blend_transition.first<start || blend_transition.last>end)){
        fputs("Blend ramp must fit within the exported frame range\n",stderr);return 2;
    }
    if(has_window && (window_start<start || window_end>end)){
        fputs("IK contact window must fit within the requested frame range\n",stderr);return 2;
    }
    if(strlen(argv[6])+40U>=sizeof(objpath))return 2;
    re4_arena_init(&arena,arena_mem,ARENA_CAP);
    for(i=0;i<parts;++i){
       er=load_pmd(i==0?argv[1]:argv[i+6],i);
       if(er!=RE4_OK){fprintf(stderr,"PMD %d: %s\n",i,re4_err_name(er));return 2;}
       memset(&bind_pose[i],0,sizeof(bind_pose[i]));bind_pose[i].world=bind_world[i];
       er=re4_skel_evaluate(&models[i],clip,0,0,&bind_pose[i],RE4_SKEL_MAX_BONES);
       if(er!=RE4_OK){fprintf(stderr,"Bind %d: %s\n",i,re4_err_name(er));return 2;}
       animated_pose[i].world=out_world[i];
       if(i>0){
           if(rigid_anchor_set[i]){
               er=re4_character_rigid_anchor(&models[0],&models[i],rigid_anchor_joint[i],&links[i]);
               if(er!=RE4_OK){fprintf(stderr,"RIGID_ANCHOR_BAD_JOINT part=%d joint=%lu\n",i,(unsigned long)rigid_anchor_joint[i]);return 2;}
               fprintf(stderr,"EXPERIMENTAL_RIGID_ANCHOR part=%d master_bone=%lu: not canonical IK or weight remapping\n",
                   i,(unsigned long)rigid_anchor_joint[i]);
           }else{
               er=re4_character_link(&models[0],&models[i],&links[i]);
               if(er!=RE4_OK)return 2;
           }
           if(strict_parts && !rigid_anchor_set[i]){
               er=re4_character_audit(&models[0],&models[i],&links[i],&audit);
               if(er!=RE4_OK ||
                  re4_character_check_policy(&audit,review_ancestry?
                       RE4_PART_POLICY_REVIEW_ANCESTRY:RE4_PART_POLICY_STRICT)!=RE4_OK){
                   fprintf(stderr,"PART_REVIEW_REQUIRED part=%d err=%s direct=%lu duplicate=%lu ancestry=%lu invalid=%lu\n",
                           i,re4_err_name(er),(unsigned long)(er==RE4_OK?audit.direct:0U),
                           (unsigned long)(er==RE4_OK?audit.repeated_direct_targets:0U),
                           (unsigned long)(er==RE4_OK?audit.ancestry_conflicts:0U),
                           (unsigned long)(er==RE4_OK?audit.out_of_range_targets:0U));
                   return 2;
               }
               if(audit.ancestry_conflicts){
                   fprintf(stderr,"REVIEWED_ANCESTRY part=%d conflicts=%lu direct=%lu: "
                       "world-space retarget only; verify every motion visually\n",
                       i,(unsigned long)audit.ancestry_conflicts,
                       (unsigned long)audit.direct);
               }
           }
       }
    }
    er=load_clip(argv[2]);if(er!=RE4_OK){fprintf(stderr,"FCV: %s\n",re4_err_name(er));return 2;}
    if(has_blend){
        er=load_blend_clip(blend_path);
        if(er!=RE4_OK){fprintf(stderr,"Blend FCV: %s\n",re4_err_name(er));return 2;}
    }
    if(clip.data && end>fcv.max_time){
        fprintf(stderr,"Requested end %lu beyond FCV max_time %u\n",(unsigned long)end,fcv.max_time);
        return 2;
    }
    reference_pose.world=reference_world;
    er=evaluate_master((re4_u16)start,&reference_pose);
    if(er!=RE4_OK)return 2;
    memset(&root_track,0,sizeof(root_track));
    memset(&root_track_b,0,sizeof(root_track_b));
    if(clip.data){
        er=re4_root_find(&fcv,&root_track);
        if(er!=RE4_OK){fprintf(stderr,"Root track: %s\n",re4_err_name(er));return 2;}
        if(has_blend){
            er=re4_root_find(&fcv_b,&root_track_b);
            if(er!=RE4_OK)return 2;
        }
        er=re4_ik_discover(&models[0],&fcv,ik_chains,MAX_IK,&ccount,&cvalid);
        if(er!=RE4_OK)return 2;
    }
    /* Contact reference is captured at its activation frame, NOT the clip
       start (unless legacy --ik-lock is selected). No retroactive locking. */
    if(has_window){
        er=evaluate_master((re4_u16)window_start,&reference_pose);
        if(er!=RE4_OK)return 2;
        if(use_root && clip.data){
            er=weight_at((re4_u16)window_start,&weight);if(er!=RE4_OK)return 2;
            if(has_blend)
                er=re4_root_blend_relative(clip,&fcv,&root_track,(re4_u16)start,
                    (re4_u16)window_start,clip_b,&fcv_b,&root_track_b,
                    blend_frame((re4_u16)start),blend_frame((re4_u16)window_start),
                    weight,root_delta);
            else er=re4_root_relative(clip,&fcv,&root_track,(re4_u16)start,
                (re4_u16)window_start,root_delta);
            if(er!=RE4_OK)return 2;
            er=re4_root_translate(&reference_pose,root_delta);
            if(er!=RE4_OK)return 2;
        }
    } else if(use_root && clip.data){
        /* At start, relative motion is zero: reference remains unchanged. */
    }
    if(MAKE_DIR(argv[6])!=0){ /* an existing directory is permitted */
        /* fopen(summary) below is the definitive check. */
    }
    sprintf(summpath,"%s/poses_index.csv",argv[6]);
    summary=fopen(summpath,"wb");if(!summary){perror(summpath);return 3;}
    fputs("frame,parts,vertices,vertices_changed,ik_candidate_chains,ik_valid_topology,ik_applied,ik_skipped,rot_tracks,special_unresolved,root_present,root_raw_x_q16,root_raw_y_q16,root_raw_z_q16,root_delta_x_q16,root_delta_y_q16,root_delta_z_q16,root_applied,ik_contact_active,blend_percent,blend_frame_b,blend_weight_q16\n",summary);
    for(frame=start;frame<=end;){
        moved=0;base=0;applied=0;skipped=0;
        frame_b=has_blend?blend_frame((re4_u16)frame):0;
        er=weight_at((re4_u16)frame,&weight);if(er!=RE4_OK){fclose(summary);return 3;}
        er=evaluate_master((re4_u16)frame,master);
        if(er!=RE4_OK){fprintf(stderr,"FK frame %lu: %s\n",(unsigned long)frame,re4_err_name(er));fclose(summary);return 3;}
        root_raw[0]=root_raw[1]=root_raw[2]=0;
        root_delta[0]=root_delta[1]=root_delta[2]=0;
        if(clip.data){
            er=re4_root_sample(clip,&fcv,&root_track,(re4_u16)frame,root_raw);
            if(er!=RE4_OK){fclose(summary);return 3;}
            if(has_blend)
                er=re4_root_blend_relative(clip,&fcv,&root_track,(re4_u16)start,
                    (re4_u16)frame,clip_b,&fcv_b,&root_track_b,
                    blend_frame((re4_u16)start),frame_b,
                    weight,root_delta);
            else er=re4_root_relative(clip,&fcv,&root_track,(re4_u16)start,
                                      (re4_u16)frame,root_delta);
            if(er!=RE4_OK){fclose(summary);return 3;}
        }
        if(use_root){
            er=re4_root_translate(master,root_delta);
            if(er!=RE4_OK){fprintf(stderr,"Root overflow at frame %lu\n",(unsigned long)frame);fclose(summary);return 3;}
        }
        contact_active=(use_ik && (!has_window || (frame>=window_start && frame<=window_end)))?1UL:0UL;
        if(contact_active && ccount){
            er=re4_ik_lock_contacts(&models[0],ik_chains,ccount,&reference_pose,
                                  master,40U,13000L,&applied,&skipped);
            if(er!=RE4_OK){fprintf(stderr,"IK frame %lu: %s\n",(unsigned long)frame,re4_err_name(er));fclose(summary);return 3;}
        }
        for(i=1;i<parts;++i){
            er=re4_character_retarget(&models[0],&bind_pose[0],master,&models[i],
                                      &bind_pose[i],&links[i],&animated_pose[i],RE4_SKEL_MAX_BONES);
            if(er!=RE4_OK){fclose(summary);return 3;}
        }
        sprintf(objpath,"%s/frame_%05lu.obj",argv[6],(unsigned long)frame);
        if(strlen(objpath)+5U>=sizeof(mtlpath)){fclose(summary);return 3;}
        strcpy(mtlpath,objpath);strcat(mtlpath,".mtl");
        obj=fopen(objpath,"wb");if(!obj){perror(objpath);fclose(summary);return 3;}
        mtl=fopen(mtlpath,"wb");if(!mtl){perror(mtlpath);fclose(obj);fclose(summary);return 3;}
        basename=strrchr(mtlpath,'/');if(!basename)basename=strrchr(mtlpath,'\\');
        basename=basename?basename+1:mtlpath;
        fprintf(obj,"# Multi-part RE4 frame %lu IK=%d (experimental contact lock)\nmtllib %s\n",(unsigned long)frame,use_ik,basename);
        for(i=0;i<parts;++i){
            if(emit_part(obj,mtl,i,&base,&moved)){
                fclose(obj);fclose(mtl);fclose(summary);return 3;
            }
        }
        if(ferror(obj)||ferror(mtl)){fclose(obj);fclose(mtl);fclose(summary);return 3;}
        fclose(obj);fclose(mtl);
        fprintf(summary,"%lu,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d,%ld,%ld,%ld,%ld,%ld,%ld,%d,%lu,%lu,%lu,%ld\n",
            (unsigned long)frame,parts,(unsigned long)base,(unsigned long)moved,
            (unsigned long)ccount,(unsigned long)cvalid,(unsigned long)applied,
            (unsigned long)skipped,(unsigned long)master->applied_rotation_channels,
            (unsigned long)master->ignored_special_channels,
            (int)(root_track.present || root_track_b.present),(long)root_raw[0],(long)root_raw[1],
            (long)root_raw[2],(long)root_delta[0],(long)root_delta[1],
            (long)root_delta[2],use_root,(unsigned long)contact_active,
            (unsigned long)(has_blend?((re4_u32)weight*100UL/65536UL):0),
            (unsigned long)frame_b,(long)weight);
        ++frames;
        if(end-frame<stride)break;
        frame+=stride;
    }
    fclose(summary);
    printf("COMPLETE CLIP OK frames=%lu parts=%d IK_candidates=%lu topology_ok=%lu contact_window=%d root_track=%d root_applied=%d blend_ramp=%d blend_percent=%lu\n",
        (unsigned long)frames,parts,(unsigned long)ccount,(unsigned long)cvalid,
        has_window,(int)root_track.present,use_root,has_blend_ramp,
        (unsigned long)(has_blend?blend_percent:0));
    return 0;
}
