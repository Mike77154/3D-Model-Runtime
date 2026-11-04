#include "gltf3d.h"
#include "gltf3d_util.h"

static int in_range_i(int v,int n){ return v>=0 && v<n; }
static int is_index_component(int ct){ return ct==GLTF3D_COMPONENT_UBYTE || ct==GLTF3D_COMPONENT_USHORT || ct==GLTF3D_COMPONENT_UINT; }

static int validate_accessor_shape(const gltf3d_asset *a,int ai)
{
    const gltf3d_accessor *ac;
    int cs;
    unsigned long off,len,stride;
    if(!a || ai<0 || ai>=a->accessor_count) return GLTF3D_ERR_BAD_ARG;
    ac=&a->accessors[ai];
    cs=gltf3d_component_size(ac->component_type);
    if(cs==0) return GLTF3D_ERR_UNSUPPORTED;
    if(ac->count==0) return GLTF3D_ERR_RANGE;
    if(ac->elem_count<1 || ac->elem_count>16) return GLTF3D_ERR_RANGE;
    if(ac->buffer_view>=0){
        int r=gltf3d_accessor_byte_range(a,ai,&off,&len,&stride);
        (void)off; (void)len;
        if(r<0) return r;
        if(stride && stride<gltf3d_accessor_elem_size(ac)) return GLTF3D_ERR_RANGE;
    }
    if(ac->sparse_count>0){
        if(ac->sparse_count>ac->count) return GLTF3D_ERR_RANGE;
        if(!in_range_i(ac->sparse_indices_buffer_view,a->buffer_view_count)) return GLTF3D_ERR_RANGE;
        if(!in_range_i(ac->sparse_values_buffer_view,a->buffer_view_count)) return GLTF3D_ERR_RANGE;
        if(!(ac->sparse_indices_component_type==GLTF3D_COMPONENT_UBYTE || ac->sparse_indices_component_type==GLTF3D_COMPONENT_USHORT || ac->sparse_indices_component_type==GLTF3D_COMPONENT_UINT)) return GLTF3D_ERR_UNSUPPORTED;
    }
    return GLTF3D_OK;
}

static int validate_primitive_semantics(const gltf3d_asset *a,int pi)
{
    const gltf3d_primitive *p;
    int ai,k,t;
    if(!a || pi<0 || pi>=a->primitive_count) return GLTF3D_ERR_BAD_ARG;
    p=&a->primitives[pi];
    if(!in_range_i(p->mesh,a->mesh_count)) return GLTF3D_ERR_RANGE;
    if(p->indices>=0){
        if(!in_range_i(p->indices,a->accessor_count)) return GLTF3D_ERR_RANGE;
        if(!is_index_component(a->accessors[p->indices].component_type)) return GLTF3D_ERR_UNSUPPORTED;
        if(a->accessors[p->indices].elem_count!=1) return GLTF3D_ERR_RANGE;
    }
    if(p->material>=0 && !in_range_i(p->material,a->material_count)) return GLTF3D_ERR_RANGE;
    for(k=0;k<GLTF3D_ATTR_COUNT;k++){
        ai=p->attributes[k];
        if(ai>=0 && !in_range_i(ai,a->accessor_count)) return GLTF3D_ERR_RANGE;
    }
    ai=p->attributes[GLTF3D_ATTR_POSITION];
    if(ai>=0 && a->accessors[ai].elem_count!=3) return GLTF3D_ERR_RANGE;
    ai=p->attributes[GLTF3D_ATTR_NORMAL];
    if(ai>=0 && a->accessors[ai].elem_count!=3) return GLTF3D_ERR_RANGE;
    ai=p->attributes[GLTF3D_ATTR_TANGENT];
    if(ai>=0 && a->accessors[ai].elem_count!=4) return GLTF3D_ERR_RANGE;
    ai=p->attributes[GLTF3D_ATTR_JOINTS_0];
    if(ai>=0 && a->accessors[ai].elem_count!=4) return GLTF3D_ERR_RANGE;
    ai=p->attributes[GLTF3D_ATTR_WEIGHTS_0];
    if(ai>=0 && a->accessors[ai].elem_count!=4) return GLTF3D_ERR_RANGE;
    if(p->has_draco){ if(!in_range_i(p->draco_buffer_view,a->buffer_view_count)) return GLTF3D_ERR_RANGE; for(k=0;k<GLTF3D_ATTR_COUNT;k++){ if(p->draco_attributes[k]<-1) return GLTF3D_ERR_RANGE; } }
    for(t=0;t<p->morph_target_count;t++){
        for(k=0;k<3;k++){
            ai=p->morph_targets[t][k];
            if(ai>=0 && !in_range_i(ai,a->accessor_count)) return GLTF3D_ERR_RANGE;
        }
    }
    return GLTF3D_OK;
}

int gltf3d_validate_basic(const gltf3d_asset *a)
{
    int i,k,r;
    if(!a) return GLTF3D_ERR_BAD_ARG;
    for(i=0;i<a->buffer_view_count;i++){
        if(!in_range_i(a->buffer_views[i].buffer,a->buffer_count)) return GLTF3D_ERR_RANGE;
        if(a->buffer_views[i].byte_length==0) return GLTF3D_ERR_RANGE;
        if(a->buffers[a->buffer_views[i].buffer].data && a->buffer_views[i].byte_offset+a->buffer_views[i].byte_length>a->buffers[a->buffer_views[i].buffer].data_length) return GLTF3D_ERR_RANGE;
        if(a->buffer_views[i].has_meshopt){
            if(!in_range_i(a->buffer_views[i].meshopt_buffer,a->buffer_count)) return GLTF3D_ERR_RANGE;
            if(a->buffer_views[i].meshopt_byte_length==0 || a->buffer_views[i].meshopt_count==0) return GLTF3D_ERR_RANGE;
            if(a->buffer_views[i].meshopt_mode<1 || a->buffer_views[i].meshopt_mode>4) return GLTF3D_ERR_UNSUPPORTED;
            if(a->buffer_views[i].meshopt_filter<0 || a->buffer_views[i].meshopt_filter>3) return GLTF3D_ERR_UNSUPPORTED;
            if(a->buffers[a->buffer_views[i].meshopt_buffer].data && a->buffer_views[i].meshopt_byte_offset+a->buffer_views[i].meshopt_byte_length>a->buffers[a->buffer_views[i].meshopt_buffer].data_length) return GLTF3D_ERR_RANGE;
        }
    }
    for(i=0;i<a->accessor_count;i++){ r=validate_accessor_shape(a,i); if(r<0) return r; }
    for(i=0;i<a->texture_count;i++){
        if(a->textures[i].sampler>=0 && !in_range_i(a->textures[i].sampler,a->sampler_count)) return GLTF3D_ERR_RANGE;
        if(a->textures[i].source>=0 && !in_range_i(a->textures[i].source,a->image_count)) return GLTF3D_ERR_RANGE;
        if(a->textures[i].basisu_source>=0 && !in_range_i(a->textures[i].basisu_source,a->image_count)) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->image_count;i++){
        if(a->images[i].buffer_view>=0 && !in_range_i(a->images[i].buffer_view,a->buffer_view_count)) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->material_count;i++){
        if(a->materials[i].base_color_texture>=0 && !in_range_i(a->materials[i].base_color_texture,a->texture_count)) return GLTF3D_ERR_RANGE;
        if(a->materials[i].metallic_roughness_texture>=0 && !in_range_i(a->materials[i].metallic_roughness_texture,a->texture_count)) return GLTF3D_ERR_RANGE;
        if(a->materials[i].normal_texture>=0 && !in_range_i(a->materials[i].normal_texture,a->texture_count)) return GLTF3D_ERR_RANGE;
        if(a->materials[i].occlusion_texture>=0 && !in_range_i(a->materials[i].occlusion_texture,a->texture_count)) return GLTF3D_ERR_RANGE;
        if(a->materials[i].emissive_texture>=0 && !in_range_i(a->materials[i].emissive_texture,a->texture_count)) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->primitive_count;i++){ r=validate_primitive_semantics(a,i); if(r<0) return r; }
    for(i=0;i<a->mesh_count;i++){
        if(a->meshes[i].first_primitive<0 || a->meshes[i].first_primitive+a->meshes[i].primitive_count>a->primitive_count) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->node_count;i++){
        if(a->nodes[i].mesh>=0 && !in_range_i(a->nodes[i].mesh,a->mesh_count)) return GLTF3D_ERR_RANGE;
        if(a->nodes[i].skin>=0 && !in_range_i(a->nodes[i].skin,a->skin_count)) return GLTF3D_ERR_RANGE;
        if(a->nodes[i].camera>=0 && !in_range_i(a->nodes[i].camera,a->camera_count)) return GLTF3D_ERR_RANGE;
        if(a->nodes[i].light>=0 && !in_range_i(a->nodes[i].light,a->light_count)) return GLTF3D_ERR_RANGE;
        for(k=0;k<a->nodes[i].child_count;k++) if(!in_range_i(a->nodes[i].children[k],a->node_count) || a->nodes[i].children[k]==i) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->scene_count;i++){
        for(k=0;k<a->scenes[i].node_count;k++) if(!in_range_i(a->scenes[i].nodes[k],a->node_count)) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->skin_count;i++){
        if(a->skins[i].inverse_bind_matrices>=0 && !in_range_i(a->skins[i].inverse_bind_matrices,a->accessor_count)) return GLTF3D_ERR_RANGE;
        if(a->skins[i].skeleton>=0 && !in_range_i(a->skins[i].skeleton,a->node_count)) return GLTF3D_ERR_RANGE;
        for(k=0;k<a->skins[i].joint_count;k++) if(!in_range_i(a->skins[i].joints[k],a->node_count)) return GLTF3D_ERR_RANGE;
    }
    for(i=0;i<a->animation_count;i++){
        if(a->animations[i].first_sampler<0 || a->animations[i].first_sampler+a->animations[i].sampler_count>a->anim_sampler_count) return GLTF3D_ERR_RANGE;
        if(a->animations[i].first_channel<0 || a->animations[i].first_channel+a->animations[i].channel_count>a->anim_channel_count) return GLTF3D_ERR_RANGE;
    }
    return GLTF3D_OK;
}
