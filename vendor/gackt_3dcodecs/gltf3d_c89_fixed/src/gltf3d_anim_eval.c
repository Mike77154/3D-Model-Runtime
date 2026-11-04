#include "gltf3d_anim_eval.h"

static int clamp_i(int v,int lo,int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }

static int find_keys(const gltf3d_asset *a,int input_accessor,gltf3d_fx t,unsigned long *k0,unsigned long *k1,gltf3d_fx *local_t)
{
    const gltf3d_accessor *ac;
    unsigned long i,n;
    gltf3d_fx t0,t1,den;
    int r;
    if(!a || input_accessor<0 || input_accessor>=a->accessor_count || !k0 || !k1 || !local_t) return GLTF3D_ERR_BAD_ARG;
    ac=&a->accessors[input_accessor];
    n=ac->count;
    if(n==0) return GLTF3D_ERR_RANGE;
    if(n==1){ *k0=0; *k1=0; *local_t=0; return GLTF3D_OK; }
    r=gltf3d_accessor_read_fixed(a,input_accessor,0,0,&t0); if(r<0) return r;
    if(t<=t0){ *k0=0; *k1=0; *local_t=0; return GLTF3D_OK; }
    for(i=0;i<n-1UL;i++){
        r=gltf3d_accessor_read_fixed(a,input_accessor,i,0,&t0); if(r<0) return r;
        r=gltf3d_accessor_read_fixed(a,input_accessor,i+1UL,0,&t1); if(r<0) return r;
        if(t<=t1){
            *k0=i; *k1=i+1UL;
            den=t1-t0;
            *local_t=den?gltf3d_fx_div(t-t0,den):0;
            return GLTF3D_OK;
        }
    }
    *k0=n-1UL; *k1=n-1UL; *local_t=0;
    return GLTF3D_OK;
}

static int read_out(const gltf3d_asset *a,int accessor,unsigned long element,int comp,gltf3d_fx *out)
{
    if(!a || !out) return GLTF3D_ERR_BAD_ARG;
    return gltf3d_accessor_read_fixed(a,accessor,element,comp,out);
}

static gltf3d_fx lerp_fx(gltf3d_fx a,gltf3d_fx b,gltf3d_fx t)
{
    return a+gltf3d_fx_mul(b-a,t);
}

static gltf3d_fx cubic_fx(gltf3d_fx p0,gltf3d_fx m0,gltf3d_fx p1,gltf3d_fx m1,gltf3d_fx t,gltf3d_fx dt)
{
    gltf3d_fx t2,t3,h00,h10,h01,h11;
    t2=gltf3d_fx_mul(t,t);
    t3=gltf3d_fx_mul(t2,t);
    h00=gltf3d_fx_mul(gltf3d_fx_from_int(2),t3)-gltf3d_fx_mul(gltf3d_fx_from_int(3),t2)+GLTF3D_FX_ONE;
    h10=t3-gltf3d_fx_mul(gltf3d_fx_from_int(2),t2)+t;
    h01=(-gltf3d_fx_mul(gltf3d_fx_from_int(2),t3))+gltf3d_fx_mul(gltf3d_fx_from_int(3),t2);
    h11=t3-t2;
    return gltf3d_fx_mul(h00,p0)+gltf3d_fx_mul(h10,gltf3d_fx_mul(m0,dt))+gltf3d_fx_mul(h01,p1)+gltf3d_fx_mul(h11,gltf3d_fx_mul(m1,dt));
}

int gltf3d_anim_eval_channel_fixed(const gltf3d_asset *a,
                                   int animation_index,
                                   int channel_local_index,
                                   gltf3d_fx time_seconds,
                                   gltf3d_fx *out_values,
                                   int out_value_capacity,
                                   int *target_node,
                                   int *target_path)
{
    const gltf3d_animation *an;
    const gltf3d_anim_channel *ch;
    const gltf3d_anim_sampler *sa;
    const gltf3d_accessor *out_ac;
    unsigned long k0,k1;
    gltf3d_fx lt,t0,t1,dt,v0,v1,m0,m1;
    int gi,gs,c,comps,r;
    if(!a || !out_values || animation_index<0 || animation_index>=a->animation_count) return GLTF3D_ERR_BAD_ARG;
    an=&a->animations[animation_index];
    if(channel_local_index<0 || channel_local_index>=an->channel_count) return GLTF3D_ERR_RANGE;
    gi=an->first_channel+channel_local_index;
    if(gi<0 || gi>=a->anim_channel_count) return GLTF3D_ERR_RANGE;
    ch=&a->anim_channels[gi];
    gs=an->first_sampler+ch->sampler;
    if(gs<0 || gs>=a->anim_sampler_count) return GLTF3D_ERR_RANGE;
    sa=&a->anim_samplers[gs];
    if(sa->input<0 || sa->input>=a->accessor_count || sa->output<0 || sa->output>=a->accessor_count) return GLTF3D_ERR_RANGE;
    out_ac=&a->accessors[sa->output];
    comps=out_ac->elem_count;
    if(comps>out_value_capacity) comps=out_value_capacity;
    if(comps<=0) return GLTF3D_ERR_RANGE;
    r=find_keys(a,sa->input,time_seconds,&k0,&k1,&lt); if(r<0) return r;
    if(target_node) *target_node=ch->target_node;
    if(target_path) *target_path=ch->target_path;
    if(sa->interpolation==GLTF3D_INTERP_STEP || k0==k1){
        unsigned long elem=k0;
        if(sa->interpolation==GLTF3D_INTERP_CUBICSPLINE) elem=k0*3UL+1UL;
        for(c=0;c<comps;c++){ r=read_out(a,sa->output,elem,c,&out_values[c]); if(r<0) return r; }
        return GLTF3D_OK;
    }
    if(sa->interpolation==GLTF3D_INTERP_CUBICSPLINE){
        r=gltf3d_accessor_read_fixed(a,sa->input,k0,0,&t0); if(r<0) return r;
        r=gltf3d_accessor_read_fixed(a,sa->input,k1,0,&t1); if(r<0) return r;
        dt=t1-t0;
        for(c=0;c<comps;c++){
            r=read_out(a,sa->output,k0*3UL+1UL,c,&v0); if(r<0) return r;
            r=read_out(a,sa->output,k0*3UL+2UL,c,&m0); if(r<0) return r;
            r=read_out(a,sa->output,k1*3UL+1UL,c,&v1); if(r<0) return r;
            r=read_out(a,sa->output,k1*3UL,c,&m1); if(r<0) return r;
            out_values[c]=cubic_fx(v0,m0,v1,m1,lt,dt);
        }
        return GLTF3D_OK;
    }
    for(c=0;c<comps;c++){
        r=read_out(a,sa->output,k0,c,&v0); if(r<0) return r;
        r=read_out(a,sa->output,k1,c,&v1); if(r<0) return r;
        out_values[c]=lerp_fx(v0,v1,lt);
    }
    (void)clamp_i;
    return GLTF3D_OK;
}

int gltf3d_anim_apply_channel_to_node(gltf3d_asset *a,
                                      int animation_index,
                                      int channel_local_index,
                                      gltf3d_fx time_seconds)
{
    gltf3d_fx vals[16];
    int node,path,r;
    r=gltf3d_anim_eval_channel_fixed(a,animation_index,channel_local_index,time_seconds,vals,16,&node,&path);
    if(r<0) return r;
    if(!a || node<0 || node>=a->node_count) return GLTF3D_ERR_RANGE;
    if(path==GLTF3D_TARGET_TRANSLATION){ a->nodes[node].translation[0]=vals[0]; a->nodes[node].translation[1]=vals[1]; a->nodes[node].translation[2]=vals[2]; a->nodes[node].has_matrix=0; return GLTF3D_OK; }
    if(path==GLTF3D_TARGET_ROTATION){ a->nodes[node].rotation[0]=vals[0]; a->nodes[node].rotation[1]=vals[1]; a->nodes[node].rotation[2]=vals[2]; a->nodes[node].rotation[3]=vals[3]; a->nodes[node].has_matrix=0; return GLTF3D_OK; }
    if(path==GLTF3D_TARGET_SCALE){ a->nodes[node].scale[0]=vals[0]; a->nodes[node].scale[1]=vals[1]; a->nodes[node].scale[2]=vals[2]; a->nodes[node].has_matrix=0; return GLTF3D_OK; }
    return GLTF3D_ERR_UNSUPPORTED;
}
