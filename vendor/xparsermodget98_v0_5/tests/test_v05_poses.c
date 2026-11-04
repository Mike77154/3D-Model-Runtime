#include <stdio.h>
#include <string.h>
#include "xp.h"
#include "xp_anim_pose.h"

typedef struct pose_probe {
    unsigned long clips;
    unsigned long frames;
    unsigned long nodes;
    int walk_mid_ok;
    int matrix_hold_ok;
    int matrix_last_ok;
} pose_probe;

static void clip_begin(void* u,unsigned long ci,const xp_anim_clip_record* c,unsigned long nodes) {
    pose_probe* p=(pose_probe*)u;
    (void)ci; (void)nodes;
    p->clips++;
    if(strcmp(c->name,"Walk")==0) {
        if(c->frame_count!=3ul || c->first_tick!=0 || c->last_tick!=2 || c->source_key_count!=2ul) p->clips+=1000ul;
    }
    if(strcmp(c->name,"MatrixClip")==0) {
        if(c->frame_count!=3ul || c->first_tick!=10 || c->last_tick!=12 || c->source_key_count!=2ul) p->clips+=1000ul;
    }
}
static void frame_begin(void* u,unsigned long ci,unsigned long fi,int tick,unsigned long nodes) {
    pose_probe* p=(pose_probe*)u; (void)ci;(void)fi;(void)tick;(void)nodes; p->frames++;
}
static void pose_node(void* u,unsigned long ci,unsigned long fi,int tick,const xp_anim_node_record* n,const xp_anim_local_pose* pose) {
    pose_probe* p=(pose_probe*)u; (void)ci;(void)fi; p->nodes++;
    if(strcmp(n->name,"Bone")!=0) return;
    if(tick==1 && pose->local_matrix[12]==XP_FX_ONE && (pose->flags&XP_ANIM_POSE_INTERPOLATED)!=0u) p->walk_mid_ok=1;
    if(tick==11 && pose->local_matrix[12]==(xp_fx32)(5*XP_FX_ONE) && (pose->flags&XP_ANIM_POSE_MATRIX_HELD)!=0u) p->matrix_hold_ok=1;
    if(tick==12 && pose->local_matrix[12]==(xp_fx32)(7*XP_FX_ONE) && (pose->flags&XP_ANIM_POSE_MATRIX_HELD)==0u) p->matrix_last_ok=1;
}

static xp_result run_parse(const char* path,xp_anim_capture* cap) {
    FILE* f;
    xp_context ctx;
    xp_header hdr;
    xp_callbacks cb;
    xp_result r;
    f=fopen(path,"rb");
    if(!f) return XP_EIO;
    xp_anim_capture_callbacks(&cb);
    r=xp_open_file(&ctx,f,&hdr);
    if(r==XP_OK) r=xp_parse(&ctx,&cb,cap);
    xp_close(&ctx);
    fclose(f);
    return r;
}

int main(void) {
    const char* path="tests/fixtures/runtime_v05_multiclip_poses_text.x";
    xp_anim_capture count_cap,cap;
    xp_anim_requirements req;
    xp_anim_node_record nodes[2];
    xp_anim_clip_record clips[2];
    xp_anim_track_record tracks[2];
    xp_anim_key_record keys[4];
    xp_anim_local_pose scratch[2];
    xp_anim_pose_callbacks pcb;
    pose_probe probe;
    xp_result r;

    xp_anim_capture_init(&count_cap,0,0,0,0,0,0,0,0);
    r=run_parse(path,&count_cap);
    if(r!=XP_OK) return 10;
    xp_anim_capture_get_requirements(&count_cap,&req);
    if(req.nodes!=2ul || req.clips!=2ul || req.tracks!=2ul || req.keys!=4ul) return 11;
    if(xp_anim_capture_finalize(&count_cap)!=XP_ELIMIT) return 12;

    xp_anim_capture_init(&cap,nodes,2ul,clips,2ul,tracks,2ul,keys,4ul);
    r=run_parse(path,&cap);
    if(r!=XP_OK) return 20;
    r=xp_anim_capture_finalize(&cap);
    if(r!=XP_OK) return 21;
    if(tracks[0].target_frame_id!=1u || tracks[1].target_frame_id!=1u) return 22;
    if(xp_anim_clip_count(&cap)!=2ul || xp_anim_clip_at(&cap,1ul)!=&clips[1]) return 23;
    {
        int tick;
        r=xp_anim_sample_pose_frame(&cap,0ul,1ul,scratch,2ul,&tick);
        if(r!=XP_OK || tick!=1 || scratch[1].local_matrix[12]!=XP_FX_ONE) return 24;
        r=xp_anim_sample_pose_frame(&cap,1ul,1ul,scratch,2ul,&tick);
        if(r!=XP_OK || tick!=11 || scratch[1].local_matrix[12]!=(xp_fx32)(5*XP_FX_ONE)) return 25;
    }

    memset(&pcb,0,sizeof(pcb)); memset(&probe,0,sizeof(probe));
    pcb.on_clip_begin=clip_begin;
    pcb.on_pose_frame_begin=frame_begin;
    pcb.on_pose_node=pose_node;
    r=xp_anim_emit_all_poses(&cap,scratch,2ul,&pcb,&probe);
    if(r!=XP_OK) return 30;
    if(probe.clips!=2ul || probe.frames!=6ul || probe.nodes!=12ul) return 31;
    if(!probe.walk_mid_ok || !probe.matrix_hold_ok || !probe.matrix_last_ok) return 32;
    puts("v0.5 multi-clip dense pose recovery: OK");
    return 0;
}
