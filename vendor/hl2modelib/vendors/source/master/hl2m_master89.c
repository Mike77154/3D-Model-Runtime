/* Source master codec / vendored frontend, C89, no ownership of model data. */
#include "hl2m_master89.h"
#include <string.h>
/* Keep mesher and textual formats as separately compiled vendor facades. */
int hl2m_vendor_mesh89(const hl2m_mdl *,const hl2m_vvd *,const hl2m_vtx *,int,hl2m_engine_model *);
int hl2m_vendor_smd89(const hl2m_u8 *,unsigned int,hl2m_smd *);
int hl2m_vendor_vta89(const hl2m_u8 *,unsigned int,hl2m_vta *);
int hl2m_vendor_qc89(const hl2m_u8 *,unsigned int,hl2m_qc *);
int hl2m_vendor_ani89(const hl2m_u8 *,unsigned int,const hl2m_mdl *,hl2m_ani *);
int hl2m_master_guess89(const char *path,const hl2m_u8 *data,unsigned int size)
{ return hl2m_guess_format(data,size,path); }
int hl2m_master_decode89(int format,const hl2m_u8 *data,unsigned int size,
                          const hl2m_mdl *related,int lod,void *out)
{
    if(!data || !out)return HL2M_ERR_NULL;
    switch(format){
    case HL2M_FORMAT_MDL:return hl2m_decode_mdl(data,size,(hl2m_mdl*)out);
    case HL2M_FORMAT_VVD:return hl2m_decode_vvd(data,size,lod,(hl2m_vvd*)out);
    case HL2M_FORMAT_VTX:return hl2m_decode_vtx(data,size,(hl2m_vtx*)out);
    case HL2M_FORMAT_VMT:return hl2m_decode_vmt(data,size,(hl2m_vmt*)out);
    case HL2M_FORMAT_PHY:return hl2m_decode_phy(data,size,(hl2m_phy*)out);
    case HL2M_FORMAT_ANI:
        if(!related)return HL2M_ERR_NULL;
        return hl2m_vendor_ani89(data,size,related,(hl2m_ani*)out);
    case HL2M_FORMAT_SMD:return hl2m_vendor_smd89(data,size,(hl2m_smd*)out);
    case HL2M_FORMAT_VTA:return hl2m_vendor_vta89(data,size,(hl2m_vta*)out);
    case HL2M_FORMAT_QC:return hl2m_vendor_qc89(data,size,(hl2m_qc*)out);
    case HL2M_FORMAT_VTF:return hl2m_vtf_read_info(data,size,(hl2m_vtf_info*)out);
    default:return HL2M_ERR_UNSUPPORTED;
    }
}
int hl2m_master_mesh89(const hl2m_mdl *mdl,const hl2m_vvd *vvd,
    const hl2m_vtx *vtx,int lod,hl2m_engine_model *out)
{ return hl2m_vendor_mesh89(mdl,vvd,vtx,lod,out); }
int hl2m_master_anim89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int anim,int frame,hl2m_pose_matrices *out,
    hl2m_native_anim_report89 *report)
{ return hl2m_native_sample_anim_matrices89(data,size,mdl,anim,frame,out,report); }
int hl2m_master_anim_external89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_u8 *ani_data,unsigned int ani_size,const hl2m_mdl *mdl,
    int anim,int frame,hl2m_pose_matrices *out,hl2m_native_anim_report89 *report)
{ return hl2m_native_sample_anim_matrices_external89(mdl_data,mdl_size,ani_data,ani_size,mdl,anim,frame,out,report); }
int hl2m_master_skin89(const hl2m_engine_model *mesh,
    const hl2m_pose_matrices *inverse_bind,
    const hl2m_pose_matrices *anim_pose,hl2m_engine_model *out,
    hl2m_skin_report *report)
{ return hl2m_engine_cpu_skin(mesh,inverse_bind,anim_pose,out,report); }
int hl2m_master_texture89(const hl2m_u8 *data,unsigned int size,
    hl2m_u8 *rgba,unsigned int cap,hl2m_vtf_info *info)
{
    int r;
    if(!data || !rgba || !info)return HL2M_ERR_NULL;
    r=hl2m_vtf_read_info(data,size,info);if(r!=HL2M_OK)return r;
    return hl2m_vtf_decode_rgba(data,size,info,rgba,cap);
}
int hl2m_master_material89(const hl2m_u8 *data,unsigned int size,
    hl2m_vmt *vmt,hl2m_material_fx89 *fx)
{
    int r;
    if(!data || !vmt || !fx)return HL2M_ERR_NULL;
    r=hl2m_decode_vmt(data,size,vmt);if(r!=HL2M_OK)return r;
    return hl2m_vmt_to_fx89(vmt,fx);
}

int hl2m_master_motion89(const hl2m_u8 *data,unsigned int size,
 const hl2m_mdl *mdl,int anim,int frame,hl2m_vec3 *pos,hl2m_fixed *yaw)
{ return hl2m_native_root_motion89(data,size,mdl,anim,frame,pos,yaw); }

/* Renderer/physics/IK-independent master adapters. */
int hl2m_physics_from_phy89(const hl2m_phy *,hl2m_phy_summary89 *);
int hl2m_physics_step89(hl2m_physics_body89 *,hl2m_fixed,hl2m_fixed,hl2m_fixed);
int hl2m_physics_overlap89(const hl2m_physics_body89 *,const hl2m_physics_body89 *,int *);
int hl2m_ik_root_plant89(const hl2m_pose_matrices *,int,hl2m_vec3,hl2m_fixed,hl2m_vec3 *);
int hl2m_ik_apply_root89(const hl2m_pose_matrices *,hl2m_vec3,hl2m_pose_matrices *);
int hl2m_ik_qc89(const hl2m_qc *,const hl2m_ik_pose *,int,hl2m_ik_pose *,hl2m_ik_report *);
int hl2m_master_phy_summary89(const hl2m_phy *phy,hl2m_phy_summary89 *out)
{ return hl2m_physics_from_phy89(phy,out); }
int hl2m_master_physics_step89(hl2m_physics_body89 *body,hl2m_fixed dt,
    hl2m_fixed gravity_z,hl2m_fixed ground_z)
{ return hl2m_physics_step89(body,dt,gravity_z,ground_z); }
int hl2m_master_physics_overlap89(const hl2m_physics_body89 *a,
    const hl2m_physics_body89 *b,int *overlap)
{ return hl2m_physics_overlap89(a,b,overlap); }
int hl2m_master_ik_root_plant89(const hl2m_pose_matrices *pose,int tip,
    hl2m_vec3 world,hl2m_fixed max_shift,hl2m_vec3 *out)
{ return hl2m_ik_root_plant89(pose,tip,world,max_shift,out); }
int hl2m_master_ik_apply_root89(const hl2m_pose_matrices *pose,
    hl2m_vec3 correction,hl2m_pose_matrices *out)
{ return hl2m_ik_apply_root89(pose,correction,out); }
int hl2m_master_ik_qc89(const hl2m_qc *qc,const hl2m_ik_pose *pose,
    int frame,hl2m_ik_pose *out,hl2m_ik_report *rep)
{ return hl2m_ik_qc89(qc,pose,frame,out,rep); }
int hl2m_master_shade89(const hl2m_material_fx89 *fx,
    const hl2m_fragment89 *frag,hl2m_u8 out[4])
{ return hl2m_shade_fragment89(fx,frag,out); }
/* SDK-inspired v1.0.6 additive APIs; callable from any presenter/engine. */
int hl2m_ik_two_bone89(const hl2m_pose_matrices *,const hl2m_ik_chain89 *,
    hl2m_pose_matrices *,hl2m_ik_chain_report89 *);
int hl2m_physics_foot_lock89(hl2m_foot_lock89 *,hl2m_vec3,hl2m_fixed,
    hl2m_fixed,hl2m_fixed,hl2m_vec3 *,int *);
int hl2m_physics_character_move89(hl2m_physics_body89 *,hl2m_vec3,
    hl2m_fixed,hl2m_fixed);
int hl2m_master_ik_two_bone89(const hl2m_pose_matrices *p,
    const hl2m_ik_chain89 *c,hl2m_pose_matrices *o,hl2m_ik_chain_report89 *r)
{return hl2m_ik_two_bone89(p,c,o,r);}
int hl2m_master_foot_lock89(hl2m_foot_lock89 *s,hl2m_vec3 foot,
    hl2m_fixed floor_z,hl2m_fixed height,hl2m_fixed speed,hl2m_vec3 *target,int *lock)
{return hl2m_physics_foot_lock89(s,foot,floor_z,height,speed,target,lock);}
int hl2m_master_character_move89(hl2m_physics_body89 *b,hl2m_vec3 delta,
    hl2m_fixed floor_z,hl2m_fixed step)
{return hl2m_physics_character_move89(b,delta,floor_z,step);}

int hl2m_master_fragment89(hl2m_fragment89 *frag,const hl2m_material_fx89 *fx,
    hl2m_vec3 normal,hl2m_vec4 tangent,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail,hl2m_fixed u,hl2m_fixed v)
{
    int r=hl2m_fragment_tangent_basis89(frag,normal,tangent);
    if(r!=HL2M_OK)return r;
    return hl2m_fragment_sample_maps89(frag,fx,base,bump,detail,u,v);
}

/* Dispatch separately vendored sampler without disrupting older APIs. */
int hl2m_master_uv_sample_linear89(const hl2m_texture_view89 *texture,
    hl2m_fixed u,hl2m_fixed v,hl2m_u8 rgba[4])
{ return hl2m_texture_sample_linear89(texture,u,v,rgba); }

/* Explicit model-authoring anomaly guard; no changes to ordinary decoding. */
int hl2m_master_anim_guard89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_u8 *ani_bytes,unsigned int ani_size,const hl2m_mdl *mdl,
    int animation,int frame,hl2m_fixed max_delta,hl2m_pose_matrices *out_pose,
    hl2m_native_anim_report89 *report,hl2m_anim_safety_report89 *safety)
{
    return hl2m_native_sample_anim_matrices_guard89(mdl_data,mdl_size,
        ani_bytes,ani_size,mdl,animation,frame,max_delta,out_pose,report,safety);
}

/* v1.0.16: composition is entirely in the vendored animator. */
int hl2m_master_sequence89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed param_x,hl2m_fixed param_y,
    hl2m_pose_matrices *out,int animations_used[4])
{ return hl2m_sequence_sample_blend89(data,size,ani,ani_size,mdl,sequence,
         cycle,param_x,param_y,out,animations_used); }
int hl2m_master_gesture89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *delta,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,hl2m_pose_matrices *out)
{ return hl2m_sequence_add_delta89(base,delta,bone_weights,strength,out); }

/* v1.0.19: expose separately vendored SDK layer metadata. */
int hl2m_master_sequence_metadata89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int sequence,hl2m_seq_metadata89 *meta)
{ return hl2m_sequence_metadata89(data,size,mdl,sequence,meta); }
int hl2m_master_layer_mask89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *overlay,const hl2m_fixed *weights,
    hl2m_fixed strength,int flags,hl2m_pose_matrices *out)
{ return hl2m_sequence_blend_mask89(base,overlay,weights,strength,flags,out); }
int hl2m_master_sequence_pose89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,hl2m_fixed physical_y,
    hl2m_pose_matrices *out,int used[4])
{
    hl2m_seq_metadata89 meta;
    hl2m_fixed x,y;int r;
    if(!mdl)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    r=hl2m_sequence_metadata89(data,size,mdl,sequence,&meta);if(r)return r;
    r=hl2m_sequence_pose_axis89(&mdl->sequences[sequence],&meta,0,physical_x,&x);if(r)return r;
    r=hl2m_sequence_pose_axis89(&mdl->sequences[sequence],&meta,1,physical_y,&y);if(r)return r;
    return hl2m_sequence_sample_blend89(data,size,ani,ani_size,mdl,sequence,cycle,x,y,out,used);
}

/* v1.0.19: a conservative, opt-in Source autolayer compositor. */
int hl2m_master_sequence_layered89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,hl2m_fixed physical_y,
    hl2m_pose_matrices *out,hl2m_sequence_layers_report89 *report)
{
    hl2m_seq_metadata89 meta,child_meta;
    hl2m_pose_matrices child,composed;
    int i,r,flags,anim_used[4];hl2m_fixed local_cycle,weight;
    if(!data || !mdl || !out || !report)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    memset(report,0,sizeof(*report));
    r=hl2m_master_sequence_metadata89(data,size,mdl,sequence,&meta);if(r)return r;
    report->declared=meta.layer_count;
    /* Source STUDIO_WORLD requires a different world-space blending branch. */
    if(meta.flags&0x4000)return HL2M_ERR_UNSUPPORTED;
    r=hl2m_master_sequence_pose89(data,size,ani,ani_size,mdl,sequence,cycle,
        physical_x,physical_y,out,NULL);if(r)return r;
    for(i=0;i<meta.layer_count;i++){
        const hl2m_seq_layer89 *layer=&meta.layers[i];
        int child_sequence=layer->sequence;
        if(child_sequence<0 || child_sequence>=mdl->sequence_count ||
           (layer->flags&(HL2M_SEQ_LAYER_LOCAL89|HL2M_SEQ_LAYER_POSE89))){
            report->unsupported_layer=i+1;return HL2M_ERR_UNSUPPORTED;
        }
        r=hl2m_sequence_layer_envelope89(layer,cycle,HL2M_FIXED_ONE,
            &local_cycle,&weight);if(r)return r;
        if(weight==0){report->skipped_zero_weight++;continue;}
        r=hl2m_master_sequence_metadata89(data,size,mdl,child_sequence,&child_meta);
        if(r)return r;
        flags=child_meta.flags;
        if(flags&0x4000){report->unsupported_layer=i+1;return HL2M_ERR_UNSUPPORTED;}
        /* Ensure no base-space pose masquerades as an additive delta. */
        if(flags&HL2M_SEQ_DELTA89){
            int a;
            if(child_meta.layer_count){report->unsupported_layer=i+1;return HL2M_ERR_UNSUPPORTED;}
            r=hl2m_native_sequence_animation89(data,size,mdl,child_sequence,0,0,&a);
            if(r)return r;
            if(!(mdl->anims[a].flags&HL2M_SEQ_DELTA89)){
                report->unsupported_layer=i+1;return HL2M_ERR_UNSUPPORTED;
            }
        }
        r=hl2m_master_sequence_pose89(data,size,ani,ani_size,mdl,child_sequence,
            local_cycle,physical_x,physical_y,&child,anim_used);if(r)return r;
        r=hl2m_master_layer_mask89(out,&child,child_meta.bone_weights,weight,
            flags&(HL2M_SEQ_DELTA89|HL2M_SEQ_POST89),&composed);
        if(r)return r;
        *out=composed;
        report->applied++;
        if(flags&HL2M_SEQ_DELTA89)report->child_delta++;
    }
    return HL2M_OK;
}

/* v1.0.20: Source AddSequenceLayers recurses through AccumulatePose.
   Accumulate child onto the final pose BEFORE visiting its descendants.
   Unlike baking a child+grandchild pose at full strength, this carries the
   inherited weight and the child's own cycle into every nested layer. */
#define HL2M_LAYERS120_MAX_DEPTH 8
static int hl2m120_visit(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed inherited_weight,
    hl2m_fixed physical_x,hl2m_fixed physical_y,
    const hl2m_fixed *pose_values,unsigned int pose_count,
    const hl2m_poseparam121 *descs,unsigned int desc_count,
    const hl2m_fixed *controls,unsigned int control_count,
    hl2m_pose_matrices *out,hl2m_sequence_layers_report120 *report,
    int ancestors[HL2M_LAYERS120_MAX_DEPTH],int depth)
{
    hl2m_seq_metadata89 meta,child_meta;
    hl2m_pose_matrices child,composed;
    int i,r,child_seq,a,used[4],j;
    hl2m_fixed child_cycle,weight,value,child_x,child_y;
    if(depth>=HL2M_LAYERS120_MAX_DEPTH)return HL2M_ERR_LIMIT;
    for(j=0;j<depth;j++)if(ancestors[j]==sequence){
        report->cycle_detected=1;report->failing_sequence=sequence;
        return HL2M_ERR_UNSUPPORTED;
    }
    ancestors[depth]=sequence;
    if(depth>report->max_depth)report->max_depth=depth;
    r=hl2m_sequence_metadata89(data,size,mdl,sequence,&meta);
    if(r)return r;
    if(meta.flags&0x4000)return HL2M_ERR_UNSUPPORTED; /* STUDIO_WORLD */
    report->declared+=meta.layer_count;
    for(i=0;i<meta.layer_count;i++){
        const hl2m_seq_layer89 *layer=&meta.layers[i];
        child_seq=layer->sequence;
        report->failing_sequence=sequence;
        report->failing_layer=i;
        if(child_seq<0 || child_seq>=mdl->sequence_count ||
           (layer->flags&HL2M_SEQ_LAYER_LOCAL89))return HL2M_ERR_UNSUPPORTED;
        value=0;
        if(layer->flags&HL2M_SEQ_LAYER_POSE89){
            if(descs){
                /* MDL-local pose index; virtual remapping not performed. */
                if(layer->pose_index<0 || !controls ||
                   (unsigned int)layer->pose_index>=desc_count ||
                   (unsigned int)layer->pose_index>=control_count)
                    return HL2M_ERR_UNSUPPORTED;
                r=hl2m_pose_physical_from_control121(&descs[layer->pose_index],
                    controls[layer->pose_index],&value);
                if(r)return r;
            }else{
                if(!pose_values || layer->pose_index<0 ||
                   (unsigned int)layer->pose_index>=pose_count)
                    return HL2M_ERR_UNSUPPORTED;
                value=pose_values[layer->pose_index];
            }
        }
        r=hl2m_sequence_layer_envelope_control120(layer,cycle,
            inherited_weight,value,&child_cycle,&weight);
        if(r)return r;
        if(weight==0){report->skipped_zero_weight++;continue;}
        r=hl2m_sequence_metadata89(data,size,mdl,child_seq,&child_meta);
        if(r)return r;
        if(child_meta.flags&0x4000)return HL2M_ERR_UNSUPPORTED;
        if(child_meta.flags&HL2M_SEQ_DELTA89){
            r=hl2m_native_sequence_animation89(data,size,mdl,child_seq,0,0,&a);
            if(r)return r;
            if(!(mdl->anims[a].flags&HL2M_SEQ_DELTA89))
                return HL2M_ERR_UNSUPPORTED;
        }
        child_x=physical_x;child_y=physical_y;
        if(descs){
            r=hl2m_pose_sequence_axes121(mdl,child_seq,descs,desc_count,
                controls,control_count,&child_x,&child_y);
            if(r)return r;
        }
        r=hl2m_master_sequence_pose89(data,size,ani,ani_size,mdl,
            child_seq,child_cycle,child_x,child_y,&child,used);
        if(r)return r;
        r=hl2m_sequence_blend_mask89(out,&child,child_meta.bone_weights,weight,
            child_meta.flags&(HL2M_SEQ_DELTA89|HL2M_SEQ_POST89),&composed);
        if(r)return r;
        *out=composed;
        report->applied++;
        if(depth>0)report->nested_applied++;
        if(layer->flags&HL2M_SEQ_LAYER_POSE89)report->pose_driven++;
        if(child_meta.flags&HL2M_SEQ_DELTA89)report->child_delta++;
        r=hl2m120_visit(data,size,ani,ani_size,mdl,child_seq,child_cycle,
            weight,physical_x,physical_y,pose_values,pose_count,
            descs,desc_count,controls,control_count,
            out,report,ancestors,depth+1);
        if(r)return r;
    }
    return HL2M_OK;
}
int hl2m_master_sequence_layers120(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,
    hl2m_fixed physical_y,const hl2m_fixed *pose_values,
    unsigned int pose_count,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report)
{
    int r,ancestors[HL2M_LAYERS120_MAX_DEPTH];
    hl2m_pose_matrices staging;
    if(!data || !mdl || !out || !report)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    memset(report,0,sizeof(*report));
    report->failing_sequence=-1;report->failing_layer=-1;
    /* Transactional output: an invalid layer must not replace a healthy
       pose with a half-composited skeleton. */
    r=hl2m_master_sequence_pose89(data,size,ani,ani_size,mdl,
        sequence,cycle,physical_x,physical_y,&staging,NULL);
    if(r)return r;
    r=hl2m120_visit(data,size,ani,ani_size,mdl,sequence,cycle,
        HL2M_FIXED_ONE,physical_x,physical_y,pose_values,pose_count,
        NULL,0u,NULL,0u,
        &staging,report,ancestors,0);
    if(r==HL2M_OK){
        report->failing_sequence=-1;report->failing_layer=-1;
        *out=staging;
    }
    return r;
}

/* v1.0.21: take actual SDK-style global normalized controller values.
   Each child layer selects its OWN paramindex and sequence blend grid. */
int hl2m_master_sequence_controls121(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,const hl2m_fixed *normalized,
    unsigned int normalized_count,int apply_layers,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report)
{
    hl2m_poseparam121 descs[HL2M_POSEPARAM_CAP121];
    unsigned int desc_count=0u;
    int r,ancestors[HL2M_LAYERS120_MAX_DEPTH];
    hl2m_fixed x,y;
    hl2m_pose_matrices staging;
    if(!data || !mdl || !out || !report)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count ||
       (apply_layers!=0 && apply_layers!=1))return HL2M_ERR_RANGE;
    r=hl2m_poseparams_read121(data,size,descs,HL2M_POSEPARAM_CAP121,&desc_count);
    if(r)return r;
    if(normalized_count<desc_count || (desc_count && !normalized))
        return HL2M_ERR_RANGE;
    r=hl2m_pose_sequence_axes121(mdl,sequence,descs,desc_count,
        normalized,normalized_count,&x,&y);
    if(r)return r;
    memset(report,0,sizeof(*report));
    report->failing_sequence=-1;report->failing_layer=-1;
    r=hl2m_master_sequence_pose89(data,size,ani,ani_size,mdl,
        sequence,cycle,x,y,&staging,NULL);
    if(r)return r;
    if(apply_layers){
        r=hl2m120_visit(data,size,ani,ani_size,mdl,sequence,cycle,
            HL2M_FIXED_ONE,x,y,NULL,0u,
            descs,desc_count,normalized,normalized_count,
            &staging,report,ancestors,0);
        if(r)return r;
    }
    report->failing_sequence=-1;report->failing_layer=-1;
    *out=staging;
    return HL2M_OK;
}

/* Resolve Source virtual sequence owner and map child local pose to the
   root skeleton, without changing the older master sequence functions. */
int hl2m_master_virtual_sequence122(const hl2m_u8 *base_bytes,
    unsigned int base_size,const hl2m_u8 *base_ani,unsigned int base_ani_size,
    const hl2m_mdl *base,const hl2m_u8 *child_bytes,
    unsigned int child_size,const hl2m_u8 *child_ani,
    unsigned int child_ani_size,const hl2m_mdl *child,
    const hl2m_virtual_link122 *link,int virtual_seq,hl2m_fixed cycle,
    const hl2m_fixed *global_controls,unsigned int controls_count,
    int apply_layers,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report)
{
    hl2m_pose_matrices local;
    hl2m_fixed child_controls[HL2M_POSEPARAM_CAP121];
    hl2m_fixed base_controls[HL2M_POSEPARAM_CAP121];
    hl2m_virtual_ref122 ref;int r;
    if(!base_bytes||!base||!child_bytes||!child||!link||!out||!report)
        return HL2M_ERR_NULL;
    if(virtual_seq<0 || virtual_seq>=link->seq_count)return HL2M_ERR_RANGE;
    if(controls_count<(unsigned int)link->pose_count ||
       (controls_count&&!global_controls))return HL2M_ERR_RANGE;
    ref=link->sequences[virtual_seq];
    if(ref.group==0){
        r=hl2m_virtual_base_controls122(link,base_bytes,base_size,
            global_controls,controls_count,base_controls,HL2M_POSEPARAM_CAP121);
        if(r)return r;
        return hl2m_master_sequence_controls121(base_bytes,base_size,
            base_ani,base_ani_size,base,ref.local,cycle,
            base_controls,(unsigned int)link->base_pose_count,
            apply_layers,out,report);
    }
    if(ref.group!=1)return HL2M_ERR_RANGE;
    r=hl2m_virtual_child_controls122(link,child_bytes,child_size,
        global_controls,controls_count,child_controls,
        HL2M_POSEPARAM_CAP121);
    if(r)return r;
    r=hl2m_master_sequence_controls121(child_bytes,child_size,
        child_ani,child_ani_size,child,ref.local,cycle,child_controls,
        (unsigned int)link->child_pose_count,apply_layers,&local,report);
    if(r)return r;
    return hl2m_virtual_remap_pose122(base,link,&local,out);
}


/* Multi-model local sequence evaluator. Locally declared autolayers only:
   Source can also reference other virtual groups from autolayers; those
   require global sequence remapping and are intentionally NOT inferred. */
int hl2m_master_virtual_sequence_multi123(const hl2m_virtual_multi123 *c,
    int virtual_seq,hl2m_fixed cycle,const hl2m_fixed *global,
    unsigned int count,int apply_local_layers,
    hl2m_pose_matrices *out,hl2m_sequence_layers_report120 *report)
{
    hl2m_fixed controls[HL2M_POSEPARAM_CAP121];
    hl2m_pose_matrices pose;hl2m_sequence_layers_report120 local_report;
    hl2m_virtual_ref122 ref;const hl2m_virtual_group123 *g;int r;
    if(!c||!out||!report)return HL2M_ERR_NULL;
    if(virtual_seq<0||virtual_seq>=c->seq_count)return HL2M_ERR_RANGE;
    ref=c->sequences[virtual_seq];
    if(ref.group<0||ref.group>=c->group_count)return HL2M_ERR_RANGE;
    g=&c->groups[ref.group];
    if(ref.local<0||ref.local>=g->source.mdl->sequence_count)
        return HL2M_ERR_RANGE;
    r=hl2m_virtual_controls_multi123(c,ref.group,global,count,
        controls,HL2M_POSEPARAM_CAP121);if(r)return r;
    r=hl2m_master_sequence_controls121(g->source.mdl_bytes,
        g->source.mdl_size,g->source.ani_bytes,g->source.ani_size,
        g->source.mdl,ref.local,cycle,controls,(unsigned int)g->pose_count,
        apply_local_layers,&pose,&local_report);
    if(r)return r;
    r=hl2m_virtual_remap_multi123(c,ref.group,&pose,out);
    if(r)return r;
    *report=local_report;
    return HL2M_OK;
}
