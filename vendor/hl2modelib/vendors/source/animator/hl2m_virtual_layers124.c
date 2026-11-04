/* Virtual Source model cross-group auto-layer evaluation (opt-in).
   Follows studio bone_setup.cpp AddSequenceLayers: iRelativeSeq,
   GetSharedPoseParameter and weight-mask accumulation. C89, fixed Q16.16.
   This is NOT Source complete IK/LOCAL/world blend or full CStudioHdr. */
#include "hl2m_master89.h"
#include <string.h>
#define VL124_DEPTH 8
#define VL124_WORLD 0x4000

static int vl124_visit(const hl2m_virtual_multi123 *c,
    int seq,hl2m_fixed cycle,hl2m_fixed inherited,
    const hl2m_fixed *global,unsigned int count,
    hl2m_pose_matrices *staging,hl2m_sequence_layers_report120 *report,
    int ancestors[VL124_DEPTH],int depth)
{
    const hl2m_virtual_group123 *owner,*child_group;
    const hl2m_mdl *childmdl;
    hl2m_virtual_ref122 here,childref;
    hl2m_seq_metadata89 meta,child_meta;
    hl2m_pose_matrices child_pose,composed;
    hl2m_sequence_layers_report120 child_report;
    hl2m_fixed local_weights[HL2M_MAX_BONES];
    hl2m_fixed childcycle,weight,physical;
    hl2m_poseparam121 merged;
    int i,j,r,id,anim,pg;
    if(depth>=VL124_DEPTH)return HL2M_ERR_LIMIT;
    for(j=0;j<depth;j++)if(ancestors[j]==seq){
        report->cycle_detected=1;report->failing_sequence=seq;
        return HL2M_ERR_UNSUPPORTED;
    }
    ancestors[depth]=seq;
    if(depth>report->max_depth)report->max_depth=depth;
    here=c->sequences[seq];
    if(here.group<0||here.group>=c->group_count)return HL2M_ERR_RANGE;
    owner=&c->groups[here.group];
    r=hl2m_sequence_metadata89(owner->source.mdl_bytes,
        owner->source.mdl_size,owner->source.mdl,here.local,&meta);
    if(r)return r;
    if(meta.flags&VL124_WORLD)return HL2M_ERR_UNSUPPORTED;
    report->declared+=meta.layer_count;
    for(i=0;i<meta.layer_count;i++){
        const hl2m_seq_layer89 *layer=&meta.layers[i];
        report->failing_sequence=seq;report->failing_layer=i;
        if(layer->flags&HL2M_SEQ_LAYER_LOCAL89)return HL2M_ERR_UNSUPPORTED;
        r=hl2m_virtual_layer_target124(c,seq,layer->sequence,&id);
        if(r)return r;
        childref=c->sequences[id];
        child_group=&c->groups[childref.group];
        childmdl=child_group->source.mdl;
        physical=0;
        if(layer->flags&HL2M_SEQ_LAYER_POSE89){
            /* In Source: GetSharedPoseParameter(iRelativeSeq(seq,layer.seq),
               layer.pose_index), so use TARGET group's parameter table. */
            if(layer->pose_index<0||layer->pose_index>=child_group->pose_count)
                return HL2M_ERR_RANGE;
            pg=child_group->pose_to_virtual[layer->pose_index];
            if(pg<0||pg>=c->pose_count||!global||(unsigned int)pg>=count)
                return HL2M_ERR_RANGE;
            memset(&merged,0,sizeof(merged));
            merged.start=c->pose_start[pg];merged.end=c->pose_end[pg];
            merged.loop=c->pose_loop[pg];
            r=hl2m_pose_physical_from_control121(&merged,global[pg],&physical);
            if(r)return r;
        }
        r=hl2m_sequence_layer_envelope_control120(layer,cycle,inherited,
            physical,&childcycle,&weight);
        if(r)return r;
        if(weight==0){report->skipped_zero_weight++;continue;}
        r=hl2m_sequence_metadata89(child_group->source.mdl_bytes,
            child_group->source.mdl_size,childmdl,childref.local,&child_meta);
        if(r)return r;
        if(child_meta.flags&VL124_WORLD)return HL2M_ERR_UNSUPPORTED;
        if(child_meta.flags&HL2M_SEQ_DELTA89){
            r=hl2m_native_sequence_animation89(child_group->source.mdl_bytes,
                child_group->source.mdl_size,childmdl,childref.local,0,0,&anim);
            if(r)return r;
            if(!(childmdl->anims[anim].flags&HL2M_SEQ_DELTA89))
                return HL2M_ERR_UNSUPPORTED;
        }
        r=hl2m_master_virtual_sequence_multi123(c,id,childcycle,global,count,
            0,&child_pose,&child_report);
        if(r)return r;
        /* Child bone-mask weights are MDL-local: project to the root
           skeleton and ZERO every unmapped bone, including for DELTA. */
        for(j=0;j<HL2M_MAX_BONES;j++)local_weights[j]=0;
        if(childref.group==0){
            for(j=0;j<child_meta.bone_count;j++)
                local_weights[j]=child_meta.bone_weights[j];
        }else{
            for(j=0;j<child_meta.bone_count;j++){
                int dst=child_group->bone_to_base[j];
                if(dst>=0&&dst<staging->bone_count)
                    local_weights[dst]=child_meta.bone_weights[j];
            }
        }
        r=hl2m_sequence_blend_mask89(staging,&child_pose,local_weights,weight,
            child_meta.flags&(HL2M_SEQ_DELTA89|HL2M_SEQ_POST89),&composed);
        if(r)return r;
        *staging=composed;
        report->applied++;
        if(depth>0)report->nested_applied++;
        if(layer->flags&HL2M_SEQ_LAYER_POSE89)report->pose_driven++;
        if(child_meta.flags&HL2M_SEQ_DELTA89)report->child_delta++;
        r=vl124_visit(c,id,childcycle,weight,global,count,staging,
            report,ancestors,depth+1);
        if(r)return r;
    }
    return HL2M_OK;
}
int hl2m_master_virtual_sequence_layers124(const hl2m_virtual_multi123 *c,
    int virtual_seq,hl2m_fixed cycle,const hl2m_fixed *global,
    unsigned int count,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report)
{
    int r,ancestors[VL124_DEPTH];
    hl2m_pose_matrices staging;
    hl2m_sequence_layers_report120 tmp,base_report;
    if(!c||!out||!report)return HL2M_ERR_NULL;
    if(virtual_seq<0||virtual_seq>=c->seq_count)return HL2M_ERR_RANGE;
    r=hl2m_master_virtual_sequence_multi123(c,virtual_seq,cycle,global,count,
        0,&staging,&base_report);
    if(r)return r;
    memset(&tmp,0,sizeof(tmp));tmp.failing_sequence=-1;tmp.failing_layer=-1;
    r=vl124_visit(c,virtual_seq,cycle,HL2M_FIXED_ONE,global,count,
        &staging,&tmp,ancestors,0);
    if(r)return r;
    tmp.failing_sequence=-1;tmp.failing_layer=-1;
    *out=staging;*report=tmp;return HL2M_OK;
}
