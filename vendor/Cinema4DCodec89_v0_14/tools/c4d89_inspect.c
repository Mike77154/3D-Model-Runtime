#include "c4d89.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static C4D89_Report report;
static C4D89_TextureCatalog textures;
static C4D89_BitmapCatalog bitmaps;
static C4D89_RigEvidence rig;
static C4D89_AnimIndex animations;
static C4D89_JointTable joint_table;
static C4D89_WeightInventory weight_inventory;
typedef struct CSVOut {FILE *f;int bad;} CSVOut;
static int print_key(void *v,unsigned long i,const C4D89_Key *k)
{
    CSVOut *c=(CSVOut*)v;
    if(fprintf(c->f,"%lu,%ld,%ld,%ld,%ld,%lu\n",i,
       k->time_q16,k->value_q16,k->time_numerator_q16,
       k->time_denominator_q16,k->file_offset)<0){c->bad=1;return 0;}
    return 1;
}
static int output_frame_track(void *user,unsigned int index,
                              const C4D89_AnimTrack *track,long value)
{
    FILE *out=(FILE *)user;
    return fprintf(out,"%u,%s,%s,%ld\n",index,track->channel,
                   track->owner_hint,value)>0;
}
int main(int argc,char **argv)
{
    int rc;unsigned int i;
    if(argc<3){
        fputs("c4d89_inspect textures|rig|anim <native.c4d>\n"
            "c4d89_inspect animcsv <native.c4d> <track_index> <file.csv>\n"
            "c4d89_inspect nodes|trackowners|weighttags <native.c4d>\n"
            "c4d89_inspect bitmaps <native.c4d>\n"
            "c4d89_inspect frame <native.c4d> <time_q16> [shortarc=0|1]\n"
            "c4d89_inspect sample <native.c4d> <track_index> <time_q16>\n"
            "c4d89_inspect samplecsv|sampleanglecsv <native.c4d> <track> <fps> <frames> <out.csv>\n"
            "c4d89_inspect sample|sampleangle <native.c4d> <track> <time_q16>\n",stderr);return 2;
    }
    if(strcmp(argv[1],"frame")==0){
        char *end;long t;unsigned long opt=0ul;FILE *f;
        C4D89_FrameStats stats;
        if(argc<4 || argc>5)return 2;
        t=strtol(argv[3],&end,10);
        if(*end || t<0L)return 2;
        if(argc==5){opt=strtoul(argv[4],&end,10);if(*end||opt>1ul)return 2;}
        rc=c4d89_scan_animation(argv[2],&animations);if(rc)return 1;
        f=fopen(argv[2],"rb");if(!f)return 1;
        puts("track,channel,owner_candidate,value_q16");
        rc=c4d89_sample_animation_frame_file(f,&animations,t,
                (unsigned int)opt,output_frame_track,stdout,&stats);
        fclose(f);
        fprintf(stderr,"frame: %u sampled, %u unsupported, status %d\n",
                stats.sampled,stats.unsupported,rc);
        return rc?1:0;
    }
    if(strcmp(argv[1],"bitmaps")==0){
        rc=c4d89_scan_material_bitmaps(argv[2],&bitmaps);if(rc)return 1;
        rc=c4d89_scan(argv[2],&report);if(rc)return 1;
        rc=c4d89_attach_material_jpegs(&report,&bitmaps);if(rc)return 1;
        printf("Native material bitmap paths: %u; omitted: %u\n",bitmaps.count,bitmaps.omitted);
        for(i=0u;i<bitmaps.count;i++){
            const C4D89_BitmapPath *m=&bitmaps.paths[i];
            printf("[%u] %s | material_class=%lu filename=%lu param=%u JPEGs=%u preferred=%u\n",i,
                m->name,m->material_offset,m->filename_offset,m->parameter_id,
                m->jpeg_count,m->best_jpeg_index);
        }
        puts("These are typed material shader filenames; NOT proven TextureTag links.");
        return 0;
    }
    if(strcmp(argv[1],"samplecsv")==0||strcmp(argv[1],"sampleanglecsv")==0){
        char *end;unsigned long target,fps,frames,j;FILE *out,*source;long q,value;
        if(argc<7)return 2;
        target=strtoul(argv[3],&end,10);if(*end)return 2;
        fps=strtoul(argv[4],&end,10);if(*end||fps<1ul||fps>1000ul)return 2;
        frames=strtoul(argv[5],&end,10);if(*end||frames<1ul||frames>5000ul)return 2;
        rc=c4d89_scan_animation(argv[2],&animations);if(rc)return 1;
        if(target>=(unsigned long)animations.count)return 2;
        source=fopen(argv[2],"rb");if(!source)return 1;
        out=fopen(argv[6],"wb");if(!out){fclose(source);return 1;}
        fputs("frame,time_q16,value_q16\n",out);
        rc=C4D89_OK;
        for(j=0ul;j<frames;j++){
            unsigned long whole=j/fps,rem=j%fps;
            if(whole>32767ul){rc=C4D89_ERR_LIMIT;break;}
            q=(long)(whole*65536ul+rem*65536ul/fps);
            if(strcmp(argv[1],"sampleanglecsv")==0)
                rc=c4d89_sample_track_angle_file(source,&animations.tracks[(unsigned int)target],q,&value);
            else rc=c4d89_sample_track_linear_file(source,&animations.tracks[(unsigned int)target],q,&value);
            if(rc || fprintf(out,"%lu,%ld,%ld\n",j,q,value)<0){if(!rc)rc=C4D89_ERR_IO;break;}
        }
        if(fclose(out)!=0)rc=C4D89_ERR_IO;
        fclose(source);
        return rc?1:0;
    }
    if(strcmp(argv[1],"sample")==0||strcmp(argv[1],"sampleangle")==0){
        char *end;unsigned long target;long time,value;
        if(argc<5)return 2;
        target=strtoul(argv[3],&end,10);if(*end)return 2;
        time=strtol(argv[4],&end,10);if(*end||time<0L||time>2147483647L)return 2;
        rc=c4d89_scan_animation(argv[2],&animations);if(rc)return 1;
        if(target>=(unsigned long)animations.count)return 2;
        if(strcmp(argv[1],"sampleangle")==0)
            rc=c4d89_sample_track_angle(argv[2],&animations.tracks[(unsigned int)target],time,&value);
        else rc=c4d89_sample_track_linear(argv[2],&animations.tracks[(unsigned int)target],time,&value);
        if(rc){fprintf(stderr,"Sample error %d (possibly irregular stride)\n",rc);return 1;}
        printf("channel=%s owner_candidate=%s time_q16=%ld value_q16=%ld\n",
            animations.tracks[(unsigned int)target].channel,
            animations.tracks[(unsigned int)target].owner_hint,time,value);
        return 0;
    }
    if(strcmp(argv[1],"textures")==0){
        rc=c4d89_scan(argv[2],&report);if(rc)return 1;
        rc=c4d89_texture_hints(&report,&textures);if(rc)return 1;
        printf("Texture references: %u; embedded JPEG: %u; unassigned: %u\n",
            textures.count,report.image_count,textures.unassigned_jpegs);
        for(i=0u;i<textures.count;i++){
            C4D89_TextureHint *t=&textures.textures[i];
            printf("[%u] %s | JPEG candidates %u",i,t->path,t->jpeg_count);
            if(t->best_jpeg_index<C4D89_MAX_IMAGES)
                printf(" | preferred image %u (%ux%u)",t->best_jpeg_index,t->best_width,t->best_height);
            puts("");
        }
        return 0;
    }
    if(strcmp(argv[1],"rig")==0) {
        rc=c4d89_scan(argv[2],&report);if(rc)return 1;
        rc=c4d89_rig_evidence(&report,&rig);if(rc)return 1;
        printf("Joint type: %u; Skin: %u; Weight: %u\n",
           rig.joint_type_strings,rig.skin_type_strings,rig.weight_type_strings);
        printf("Joint-region name candidates: %u (parent links and weights not decoded)\n",rig.candidate_joint_count);
        for(i=0u;i<rig.candidate_joint_count;i++)
            printf("  %8lu  %s\n",rig.joint_hints[i].name_offset,rig.joint_hints[i].name);
        return 0;
    }
    if(strcmp(argv[1],"nodes")==0) {
        rc=c4d89_scan_joint_records(argv[2],&joint_table);if(rc)return 1;
        printf("Joint records: %u; observed matrices: %u; omitted: %u\n",
            joint_table.count,joint_table.with_matrix,joint_table.omitted);
        for(i=0u;i<joint_table.count;i++) {
            const C4D89_JointRecord *n=&joint_table.joints[i];
            printf("[%u] %s | name=%lu joint=%lu matrix=%lu %s\n",
                i,n->name,n->name_offset,n->joint_offset,n->matrix_offset,
                n->has_matrix?"candidate local matrix":"matrix unknown");
        }
        return 0;
    }
    if(strcmp(argv[1],"weighttags")==0) {
        rc=c4d89_inventory_weight_tags(argv[2],&weight_inventory);
        if(rc)return 1;
        printf("Native Weight class records: %u; omitted: %u\n",
               weight_inventory.count,weight_inventory.omitted);
        for(i=0u;i<weight_inventory.count;i++) {
            const C4D89_WeightTagHint *w=&weight_inventory.tags[i];
            printf("[%u] class=%lu alias=%lu layout=%u groups=%lu data=%lu bytes=%lu selected_points=%lu max_index=%lu bitmap=%lu bitmap_bytes=%lu\n",
               i,w->class_offset,w->alias_offset,w->layout,
               w->opaque_record_count,w->blob_offset,w->blob_bytes,
               w->selected_point_count,w->max_point_index,w->bitmap_offset,w->bitmap_bytes);
        }
        puts("Layout 4 = range+bitmap cross-validated point IDs; NOT joint weight values.");
        return 0;
    }
    if(strcmp(argv[1],"trackowners")==0) {
        rc=c4d89_scan_animation(argv[2],&animations);if(rc)return 1;
        for(i=0u;i<animations.count;i++) {
            const C4D89_AnimTrack *t=&animations.tracks[i];
            printf("[%u] %s -> %s | %lu keys | name=%lu joint=%lu | %s\n",
                i,t->channel,t->owner_confidence?t->owner_hint:"<unknown>",
                t->key_count,t->owner_name_offset,t->owner_joint_offset,
                t->owner_confidence?"name-adjacent (not verified graph link)":"unresolved");
        }
        return 0;
    }
    if(strcmp(argv[1],"anim")==0||strcmp(argv[1],"animcsv")==0) {
        rc=c4d89_scan_animation(argv[2],&animations);if(rc)return 1;
        if(strcmp(argv[1],"animcsv")==0){
            unsigned long target;char *end;FILE *out;CSVOut c;
            if(argc<5)return 2;
            target=strtoul(argv[3],&end,10);
            if(*end||target>=(unsigned long)animations.count)return 2;
            out=fopen(argv[4],"wb");if(!out)return 1;
            c.f=out;c.bad=0;
            fputs("index,time_q16,value_q16,time_numerator_q16,time_denominator_q16,source_offset\n",out);
            rc=c4d89_stream_track(argv[2],&animations.tracks[(unsigned int)target],print_key,&c);
            if(fclose(out)!=0 || c.bad || rc)return 1;
            return 0;
        }
        printf("Native scalar tracks: %u; decoded keys: %lu; omitted tracks: %u\n",
          animations.count,animations.decoded_keys,animations.omitted_tracks);
        for(i=0u;i<animations.count;i++){
            const C4D89_AnimTrack *t=&animations.tracks[i];
            if(t->key_count)printf("[%u] %-16s %6lu keys | time Q16 %ld .. %ld | stride %u\n",
              i,t->channel,t->key_count,t->start_time_q16,t->end_time_q16,t->key_stride);
        }
        return 0;
    }
    return 2;
}
