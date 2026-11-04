/* RE4 2007 FCV all-clips / all-original-keyframes exporter.
   No dynamic memory, no floating point in the codec or this CLI.
   The output is intentionally *metadata and pose matrices*, not copyrighted
   original game files. CSV paths are derived from NUMERIC indices only. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p,0775)
#endif
#define DAT_CAP (24UL*1024UL*1024UL)
#define PMD_CAP (16UL*1024UL*1024UL)
#define ARENA_CAP (16UL*1024UL*1024UL)
#define PATH_CAP 1024U
static re4_u8 dat_bytes[DAT_CAP],model_bytes[PMD_CAP],arena_data[ARENA_CAP];
static re4_u8 time_bits[8192];
static re4_skel_matrix pose_world[RE4_SKEL_MAX_BONES];
static re4_span model_span;
static re4_pmd_model model;
static int have_model=0;
static int readfile(const char *path,re4_u8 *buffer,re4_size cap,re4_span *out)
{
    FILE *f;long n;
    f=fopen(path,"rb");if(!f){perror(path);return 0;}
    if(fseek(f,0L,SEEK_END)!=0||(n=ftell(f))<0L||(re4_size)n>cap||fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buffer,1,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);out->data=buffer;out->size=(re4_size)n;return 1;
}
static FILE *numeric_file(const char *dir,const char *prefix,re4_u32 i)
{
    char path[PATH_CAP];
    if(strlen(dir)+strlen(prefix)+35U>=sizeof(path))return 0;
    sprintf(path,"%s/%s_%04lu.csv",dir,prefix,(unsigned long)i);
    return fopen(path,"wb");
}
static re4_u32 raw_scalar(re4_span value_stream,re4_size at,re4_u8 width,re4_endian en)
{
    if(width==4U && re4_span_ok(value_stream,at,4UL))return re4_read_u32_e(value_stream.data+at,en);
    if(width==2U && re4_span_ok(value_stream,at,2UL))return re4_read_u16_e(value_stream.data+at,en);
    if(width==1U && re4_span_ok(value_stream,at,1UL))return value_stream.data[at];
    return 0U;
}
static int dump_keys(FILE *out,re4_span src,const re4_fcv *clip)
{
    re4_u32 b,a,k;re4_u16 time;re4_q16 triple[3];re4_err er;re4_fcv_axis_view ax[3];
    fputs("channel,bone_id,bone_type,data_type,axis,index,frame,value_q16,tangent_in_q16,tangent_out_q16,value_bytes,tangent_in_bytes,tangent_out_bytes,value_raw_hex,tangent_in_raw_hex,tangent_out_raw_hex\n",out);
    for(b=0;b<clip->block_count;++b){
        er=re4_fcv_block_axis_views_e(src,&clip->blocks[b],clip->endian,&ax[0],&ax[1],&ax[2]);
        if(er!=RE4_OK)return 0;
        for(a=0;a<3;++a)for(k=0;k<ax[a].tag_count;++k){
            er=re4_fcv_axis_time_at_e(&ax[a],k,clip->endian,&time);if(er!=RE4_OK)return 0;
            er=re4_fcv_axis_key_q16(&ax[a],k,clip->endian,triple);if(er!=RE4_OK)return 0;
            {
                re4_size off=(re4_size)k*ax[a].record_size;
                re4_u32 rv=raw_scalar(ax[a].values,off,ax[a].value_size,clip->endian);
                re4_u32 ri=raw_scalar(ax[a].values,off+ax[a].value_size,ax[a].tangent_in_size,clip->endian);
                re4_u32 ro=raw_scalar(ax[a].values,off+ax[a].value_size+ax[a].tangent_in_size,ax[a].tangent_out_size,clip->endian);
                fprintf(out,"%lu,%u,%u,%u,%lu,%lu,%u,%ld,%ld,%ld,%u,%u,%u,%08lX,%08lX,%08lX\n",
                  (unsigned long)b,(unsigned)clip->blocks[b].bone_no,
                  (unsigned)clip->blocks[b].bone_type,(unsigned)clip->blocks[b].data_type,
                  (unsigned long)a,(unsigned long)k,(unsigned)time,
                  (long)triple[0],(long)triple[1],(long)triple[2],
                  (unsigned)ax[a].value_size,(unsigned)ax[a].tangent_in_size,(unsigned)ax[a].tangent_out_size,
                  (unsigned long)rv,(unsigned long)ri,(unsigned long)ro);
            }
        }
    }
    return !ferror(out);
}
static int dump_specials(FILE *out,re4_span data,const re4_fcv *clip)
{
    re4_u32 frame,b;re4_err er;re4_q16 v[3];
    fputs("frame,channel,bone_id,channel_kind,channel_encoding,x_q16,y_q16,z_q16,interpretation\n",out);
    for(frame=0;frame<=65535UL;++frame){
        if(!(time_bits[frame>>3]&(1U<<(frame&7U))))continue;
        for(b=0;b<clip->block_count;++b){
            if(clip->blocks[b].bone_type==2U)continue;
            er=re4_fcv_block_sample_hermite_q16(data,&clip->blocks[b],(re4_u16)frame,clip->endian,v);
            if(er!=RE4_OK)return 0;
            fprintf(out,"%lu,%lu,%u,%u,%u,%ld,%ld,%ld,unresolved_native_semantics\n",
                (unsigned long)frame,(unsigned long)b,(unsigned)clip->blocks[b].bone_no,
                (unsigned)clip->blocks[b].bone_type,(unsigned)clip->blocks[b].data_type,
                (long)v[0],(long)v[1],(long)v[2]);
        }
    }
    return !ferror(out);
}
static int skeleton_id(const re4_pmd_model *m,re4_u8 id)
{
    re4_u32 j;re4_strref name;
    for(j=0;j<m->skeleton_count && j<m->skeleton_name_count;++j){
        name=m->skeleton_names[j].name;
        if(name.len>=8UL && !memcmp(name.ptr,"born__",6UL) &&
           name.ptr[6]>='0' && name.ptr[6]<='9' &&
           name.ptr[7]>='0' && name.ptr[7]<='9' &&
           10*(name.ptr[6]-'0')+(name.ptr[7]-'0')==id)return (int)j;
    }
    return -1;
}
static int first_child(const re4_pmd_model *m,int parent)
{
    re4_u32 j;if(parent<0)return -1;
    for(j=0;j<m->skeleton_count;++j)if(m->skeletons[j].parent==parent)return (int)j;
    return -1;
}
static int dump_ik_roots(FILE *out,const re4_fcv *clip)
{
    re4_u32 i,j;int sk=-1,mid=-1,eff=-1,matched;
    fputs("channel,root_parts_id,kind,ik_bend_axis_code,toe_correction,four_joint,master_root_index,guessed_middle_index,guessed_effector_index,target_position_channel,verified_game_chain\n",out);
    for(i=0;i<clip->block_count;++i){
        const re4_fcv_block *b=&clip->blocks[i];
        if(!(b->bone_type&0x30U))continue;
        if(have_model){sk=skeleton_id(&model,b->bone_no);mid=first_child(&model,sk);eff=first_child(&model,mid);}
        matched=-1;
        for(j=0;j<clip->block_count;++j){
            if((clip->blocks[j].bone_type&4U)!=0U && eff>=0 &&
               skeleton_id(&model,clip->blocks[j].bone_no)==eff){matched=(int)j;break;}
        }
        fprintf(out,"%lu,%u,%u,%u,%u,%u,%d,%d,%d,%d,no\n",
            (unsigned long)i,(unsigned)b->bone_no,(unsigned)b->bone_type,
            (unsigned)(b->data_type&15U),(unsigned)((b->bone_type&0x20U)!=0U),
            (unsigned)((b->bone_type&0x80U)!=0U),sk,mid,eff,matched);
    }
    return !ferror(out);
}
static int dump_poses(FILE *out,re4_span data,const re4_fcv *clip)
{
    re4_u32 frame,b,j;re4_skel_pose pose;re4_err er;
    pose.world=pose_world;
    fputs("frame,bone_index,m00,m01,m02,tx,m10,m11,m12,ty,m20,m21,m22,tz,rot_channels,special_unresolved\n",out);
    for(frame=0;frame<=65535UL;++frame){
        if(!(time_bits[frame>>3]&(1U<<(frame&7U))))continue;
        er=re4_skel_evaluate(&model,data,clip,(re4_u16)frame,&pose,RE4_SKEL_MAX_BONES);
        if(er!=RE4_OK){fprintf(stderr,"pose unsupported frame=%lu error=%s\n",(unsigned long)frame,re4_err_name(er));return 0;}
        for(b=0;b<pose.bone_count;++b){
            fprintf(out,"%lu,%lu",(unsigned long)frame,(unsigned long)b);
            for(j=0;j<12;++j)fprintf(out,",%ld",(long)pose.world[b].m[j]);
            fprintf(out,",%lu,%lu\n",(unsigned long)pose.applied_rotation_channels,
              (unsigned long)pose.ignored_special_channels);
        }
    }
    return !ferror(out);
}
int main(int argc,char **argv)
{
    re4_span dat,one;re4_archive arc;re4_arena arena;re4_fcv clip;re4_err er;
    re4_u32 i,good=0,placeholder=0,bad=0,totalkeys=0,posefail=0;re4_size mark;
    FILE *cat,*out,*seqcat;char path[PATH_CAP];re4_u32 seqgood=0,seqbad=0;
    if(argc!=3 && argc!=4){fprintf(stderr,"usage: %s NESTED.DAT OUTPUT_DIR [MASTER.PMD]\n",argv[0]);return 2;}
    if(strlen(argv[2])+35U>=PATH_CAP){fputs("directory path too long\n",stderr);return 2;}
    if(!readfile(argv[1],dat_bytes,DAT_CAP,&dat)){fputs("DAT unreadable or over cap\n",stderr);return 2;}
    re4_arena_init(&arena,arena_data,ARENA_CAP);
    er=re4_dat_read_indexed(dat,RE4_ENDIAN_LITTLE,RE4_FALSE,&arena,&arc);
    if(er!=RE4_OK){fprintf(stderr,"DAT: %s\n",re4_err_name(er));return 2;}
    if(argc==4){
        if(!readfile(argv[3],model_bytes,PMD_CAP,&model_span)){fputs("PMD unreadable\n",stderr);return 2;}
        er=re4_pmd_read_survey(model_span,&arena,&model);
        if(er!=RE4_OK || model.skeleton_count>RE4_SKEL_MAX_BONES){fprintf(stderr,"PMD: %s\n",re4_err_name(er));return 2;}
        have_model=1;
    }
    MAKE_DIR(argv[2]);sprintf(path,"%s/clips.csv",argv[2]);cat=fopen(path,"wb");if(!cat){perror(path);return 2;}
    fputs("dat_index,byte_length,status,max_time,channels,original_keyframes,unique_times,rotation_tracks,special_tracks,unknown_types\n",cat);
    sprintf(path,"%s/sequences.csv",argv[2]);seqcat=fopen(path,"wb");if(!seqcat){fclose(cat);return 2;}
    fputs("dat_index,byte_length,status,key_count,seq_flags\n",seqcat);
    mark=arena.pos;
    for(i=0;i<arc.entry_count;++i){
        re4_u32 j,rot=0,special=0,unknown=0,unique=0,orig=0;int ok=1;
        re4_fcv_axis_view ax[3];
        if(arc.entries[i].name.len==4UL && memcmp(arc.entries[i].name.ptr,"SEQ",3)==0){
            re4_seq_timeline seq;re4_seq_key key;re4_u32 q;
            er=re4_archive_entry_span(dat,&arc.entries[i],&one);
            if(er==RE4_OK)er=re4_seq_timeline_read(one,RE4_ENDIAN_LITTLE,&seq);
            if(er!=RE4_OK){++seqbad;fprintf(seqcat,"%lu,0,%s,0,0\n",(unsigned long)i,re4_err_name(er));continue;}
            out=numeric_file(argv[2],"sequence",i);
            if(!out){fclose(seqcat);fclose(cat);return 3;}
            fputs("index,frame_q6,frame_integer,frame_fraction_64,event_code,auxiliary_code\n",out);
            for(q=0;q<seq.count;++q){
                er=re4_seq_timeline_key_at(&seq,q,&key);
                if(er!=RE4_OK)break;
                fprintf(out,"%lu,%u,%u,%u,%u,%u\n",(unsigned long)q,(unsigned)key.frame_q6,
                    (unsigned)(key.frame_q6>>6),(unsigned)(key.frame_q6&63U),(unsigned)key.se,(unsigned)key.flags);
            }
            if(er==RE4_OK && !ferror(out)){
                fprintf(seqcat,"%lu,%lu,ok,%u,%u\n",(unsigned long)i,(unsigned long)one.size,
                    (unsigned)seq.count,(unsigned)seq.flags);++seqgood;
            }else{++seqbad;fprintf(seqcat,"%lu,%lu,keys_error,0,0\n",(unsigned long)i,(unsigned long)one.size);}
            fclose(out);continue;
        }
        if(arc.entries[i].name.len!=4UL || memcmp(arc.entries[i].name.ptr,"FCV",3)!=0)continue;
        er=re4_archive_entry_span(dat,&arc.entries[i],&one);
        if(er!=RE4_OK || one.size<16UL || one.data[2]==0U){
            ++placeholder;fprintf(cat,"%lu,%lu,empty,0,0,0,0,0,0,0\n",(unsigned long)i,(unsigned long)one.size);continue;
        }
        arena.pos=mark;
        er=re4_fcv_read(one,RE4_ENDIAN_LITTLE,&arena,&clip);
        if(er==RE4_OK)er=re4_fcv_validate(one,&clip);
        if(er!=RE4_OK){
            ++bad;fprintf(cat,"%lu,%lu,%s,0,0,0,0,0,0,0\n",(unsigned long)i,(unsigned long)one.size,re4_err_name(er));continue;
        }
        for(j=0;j<clip.block_count;++j){
            if(clip.blocks[j].bone_type==2U)++rot;else ++special;
            if(clip.blocks[j].bone_type!=1U && clip.blocks[j].bone_type!=2U && clip.blocks[j].bone_type!=4U &&
               clip.blocks[j].bone_type!=8U && clip.blocks[j].bone_type!=16U && clip.blocks[j].bone_type!=32U &&
               clip.blocks[j].bone_type!=64U && clip.blocks[j].bone_type!=128U && clip.blocks[j].bone_type!=160U)++unknown;
            er=re4_fcv_block_axis_views_e(one,&clip.blocks[j],clip.endian,&ax[0],&ax[1],&ax[2]);
            if(er!=RE4_OK){ok=0;break;}
            orig+=(re4_u32)ax[0].tag_count+ax[1].tag_count+ax[2].tag_count;
        }
        if(!ok || re4_fcv_keyframe_union(one,&clip,time_bits,sizeof(time_bits),&unique)!=RE4_OK){
            ++bad;fprintf(cat,"%lu,%lu,keys_error,0,0,0,0,0,0,0\n",(unsigned long)i,(unsigned long)one.size);continue;
        }
        out=numeric_file(argv[2],"keys",i);
        if(!out||!dump_keys(out,one,&clip)){if(out)fclose(out);fclose(cat);fputs("keys write failed\n",stderr);return 3;}
        fclose(out);
        if(special){
            out=numeric_file(argv[2],"controls",i);
            if(!out||!dump_specials(out,one,&clip)){if(out)fclose(out);fclose(seqcat);fclose(cat);fputs("special channel write failed\n",stderr);return 3;}
            fclose(out);
        }
        {int pose_ok=1;
        if(have_model){
            out=numeric_file(argv[2],"ik_roots",i);
            if(!out||!dump_ik_roots(out,&clip)){if(out)fclose(out);fclose(cat);fclose(seqcat);return 3;}
            fclose(out);
            out=numeric_file(argv[2],"poses",i);
            if(!out){fclose(cat);fclose(seqcat);return 3;}
            if(!dump_poses(out,one,&clip))pose_ok=0;
            fclose(out);
            if(!pose_ok){
                sprintf(path,"%s/poses_%04lu.csv",argv[2],(unsigned long)i);
                remove(path);++posefail;
            }
        }
        fprintf(cat,"%lu,%lu,%s,%u,%u,%lu,%lu,%lu,%lu,%lu\n",
            (unsigned long)i,(unsigned long)one.size,pose_ok?"ok":"ok_pose_unavailable",(unsigned)clip.max_time,
            (unsigned)clip.block_count,(unsigned long)orig,(unsigned long)unique,
            (unsigned long)rot,(unsigned long)special,(unsigned long)unknown);
        ++good;totalkeys+=orig;
        }
    }
    fclose(cat);fclose(seqcat);
    printf("SEQ timelines: valid=%lu bad=%lu pose_unavailable=%lu\n",(unsigned long)seqgood,(unsigned long)seqbad,(unsigned long)posefail);
    printf("ALL FCV CATALOG: valid=%lu empty=%lu unsupported_or_bad=%lu keys=%lu pose_export=%s\n",
        (unsigned long)good,(unsigned long)placeholder,(unsigned long)bad,
        (unsigned long)totalkeys,have_model?"yes":"no");
    return (bad||seqbad)?1:0;
}
