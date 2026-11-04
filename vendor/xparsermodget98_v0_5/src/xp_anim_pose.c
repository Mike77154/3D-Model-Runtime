/* xp_anim_pose.c - caller-owned .x animation clip/pose recovery
   C89, fixed-point only, no heap.
*/
#include <string.h>
#include <limits.h>
#include "xp_anim_pose.h"

#if ULONG_MAX == 0xffffffffUL
typedef unsigned long xp_ap_u32;
#else
typedef unsigned int xp_ap_u32;
#endif

static void ap_copyz(char* dst, unsigned int cap, const char* src) {
    unsigned int i;
    if (!dst || cap == 0u) return;
    if (!src) src = "";
    i = 0u;
    while (i + 1u < cap && src[i] != '\0') { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

static xp_ap_u32 ap_mag(xp_fx32 v) {
    if (v < 0) return (xp_ap_u32)(-(v + (xp_fx32)1)) + (xp_ap_u32)1;
    return (xp_ap_u32)v;
}

static xp_fx32 ap_neg_sat(xp_fx32 v) {
    if (v == XP_FX_MIN) return XP_FX_MAX;
    return (xp_fx32)(-v);
}

static xp_fx32 ap_add_sat(xp_fx32 a, xp_fx32 b) {
    if (b > 0 && a > (xp_fx32)(XP_FX_MAX - b)) return XP_FX_MAX;
    if (b < 0 && a < (xp_fx32)(XP_FX_MIN - b)) return XP_FX_MIN;
    return (xp_fx32)(a + b);
}

static xp_fx32 ap_sub_sat(xp_fx32 a, xp_fx32 b) {
    return ap_add_sat(a, ap_neg_sat(b));
}

static xp_fx32 ap_mul_sat(xp_fx32 a, xp_fx32 b) {
    xp_ap_u32 ua, ub, ahi, alo, bhi, blo;
    xp_ap_u32 hh, term, mag, limit;
    int neg;
    if (a == 0 || b == 0) return (xp_fx32)0;
    neg = ((a < 0) != (b < 0));
    ua = ap_mag(a); ub = ap_mag(b);
    ahi = ua >> 16; alo = ua & (xp_ap_u32)0xffffu;
    bhi = ub >> 16; blo = ub & (xp_ap_u32)0xffffu;
    limit = neg ? (xp_ap_u32)0x80000000UL : (xp_ap_u32)0x7fffffffUL;
    hh = ahi * bhi;
    if (hh > (limit >> 16)) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag = hh << 16;
    term = ahi * blo;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;
    term = alo * bhi;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;
    term = (alo * blo) >> 16;
    if (term > limit - mag) return neg ? XP_FX_MIN : XP_FX_MAX;
    mag += term;
    if (neg) {
        if (mag == (xp_ap_u32)0x80000000UL) return XP_FX_MIN;
        return (xp_fx32)(-(xp_fx32)mag);
    }
    return (xp_fx32)mag;
}

/* Positive Q16.16 division without 64-bit arithmetic. */
static xp_fx32 ap_div_pos(xp_fx32 a, xp_fx32 b) {
    xp_ap_u32 ua, ub, qi, rem, frac;
    int i;
    if (a <= 0 || b <= 0) return (xp_fx32)0;
    ua = (xp_ap_u32)a; ub = (xp_ap_u32)b;
    qi = ua / ub;
    if (qi > (xp_ap_u32)32767u) return XP_FX_MAX;
    rem = ua % ub;
    frac = 0u;
    for (i = 0; i < 16; ++i) {
        frac <<= 1;
        if (rem >= ub - rem) {
            rem = rem - (ub - rem);
            frac |= 1u;
        } else {
            rem += rem;
        }
    }
    return (xp_fx32)((qi << 16) | frac);
}

static xp_fx32 ap_div(xp_fx32 a, xp_fx32 b) {
    xp_fx32 q;
    int neg;
    xp_ap_u32 ua, ub;
    if (b == 0) return (a < 0) ? XP_FX_MIN : XP_FX_MAX;
    if (a == 0) return 0;
    neg = ((a < 0) != (b < 0));
    ua = ap_mag(a); ub = ap_mag(b);
    if (ua > (xp_ap_u32)XP_FX_MAX) ua = (xp_ap_u32)XP_FX_MAX;
    if (ub > (xp_ap_u32)XP_FX_MAX) ub = (xp_ap_u32)XP_FX_MAX;
    q = ap_div_pos((xp_fx32)ua, (xp_fx32)ub);
    return neg ? ap_neg_sat(q) : q;
}

static xp_fx32 ap_sqrt(xp_fx32 x) {
    xp_fx32 g, q, ng;
    int i;
    if (x <= 0) return 0;
    g = (x > XP_FX_ONE) ? x : XP_FX_ONE;
    for (i = 0; i < 18; ++i) {
        q = ap_div_pos(x, g);
        ng = ap_add_sat((xp_fx32)(g >> 1), (xp_fx32)(q >> 1));
        if ((g & 1) && (q & 1) && ng < XP_FX_MAX) ng++;
        if (ng == g || ng == 0) break;
        g = ng;
    }
    return g;
}

static xp_fx32 ap_ratio_q16(unsigned long num, unsigned long den) {
    unsigned long rem, frac;
    int i;
    if (den == 0ul || num == 0ul) return 0;
    if (num >= den) return XP_FX_ONE;
    rem = num;
    frac = 0ul;
    for (i = 0; i < 16; ++i) {
        frac <<= 1;
        if (rem >= den - rem) {
            rem = rem - (den - rem);
            frac |= 1ul;
        } else {
            rem += rem;
        }
    }
    return (xp_fx32)frac;
}

static xp_fx32 ap_lerp(xp_fx32 a, xp_fx32 b, xp_fx32 t) {
    return ap_add_sat(a, ap_mul_sat(ap_sub_sat(b, a), t));
}

static void ap_quat_normalize(xp_fx32 q[4]) {
    xp_fx32 n2, n;
    int i;
    n2 = 0;
    for (i = 0; i < 4; ++i) n2 = ap_add_sat(n2, ap_mul_sat(q[i], q[i]));
    n = ap_sqrt(n2);
    if (n <= 0) {
        q[0] = XP_FX_ONE; q[1] = 0; q[2] = 0; q[3] = 0;
        return;
    }
    for (i = 0; i < 4; ++i) q[i] = ap_div(q[i], n);
}

static void ap_quat_nlerp(const xp_fx32 a[4], const xp_fx32 b_in[4], xp_fx32 t, xp_fx32 out[4]) {
    xp_fx32 b[4], dot;
    int i;
    dot = 0;
    for (i = 0; i < 4; ++i) { b[i] = b_in[i]; dot = ap_add_sat(dot, ap_mul_sat(a[i], b[i])); }
    if (dot < 0) for (i = 0; i < 4; ++i) b[i] = ap_neg_sat(b[i]);
    for (i = 0; i < 4; ++i) out[i] = ap_lerp(a[i], b[i], t);
    ap_quat_normalize(out);
}

/* .x rotation keys are stored w,x,y,z. Produce a DirectX row-vector matrix. */
static void ap_quat_to_matrix(const xp_fx32 q_in[4], xp_fx32 out[16]) {
    xp_fx32 q[4];
    xp_fx32 w,x,y,z,xx,yy,zz,xy,xz,yz,xw,yw,zw,two;
    int i;
    for (i=0;i<4;++i) q[i]=q_in[i];
    ap_quat_normalize(q);
    w=q[0]; x=q[1]; y=q[2]; z=q[3];
    xx=ap_mul_sat(x,x); yy=ap_mul_sat(y,y); zz=ap_mul_sat(z,z);
    xy=ap_mul_sat(x,y); xz=ap_mul_sat(x,z); yz=ap_mul_sat(y,z);
    xw=ap_mul_sat(x,w); yw=ap_mul_sat(y,w); zw=ap_mul_sat(z,w);
    two=(xp_fx32)(2 * XP_FX_ONE);
    xp_mat4_identity(out);
    out[0]=ap_sub_sat(XP_FX_ONE,ap_mul_sat(two,ap_add_sat(yy,zz)));
    out[1]=ap_mul_sat(two,ap_add_sat(xy,zw));
    out[2]=ap_mul_sat(two,ap_sub_sat(xz,yw));
    out[4]=ap_mul_sat(two,ap_sub_sat(xy,zw));
    out[5]=ap_sub_sat(XP_FX_ONE,ap_mul_sat(two,ap_add_sat(xx,zz)));
    out[6]=ap_mul_sat(two,ap_add_sat(yz,xw));
    out[8]=ap_mul_sat(two,ap_add_sat(xz,yw));
    out[9]=ap_mul_sat(two,ap_sub_sat(yz,xw));
    out[10]=ap_sub_sat(XP_FX_ONE,ap_mul_sat(two,ap_add_sat(xx,yy)));
}

static xp_fx32 ap_row_scale(const xp_fx32 m[16], int row) {
    int o;
    xp_fx32 n2;
    o = row * 4;
    n2 = ap_add_sat(ap_mul_sat(m[o],m[o]), ap_mul_sat(m[o+1],m[o+1]));
    n2 = ap_add_sat(n2, ap_mul_sat(m[o+2],m[o+2]));
    return ap_sqrt(n2);
}

static void ap_bind_scales(const xp_fx32 m[16], xp_fx32 s[3]) {
    s[0]=ap_row_scale(m,0); s[1]=ap_row_scale(m,1); s[2]=ap_row_scale(m,2);
    if (s[0]<=0) s[0]=XP_FX_ONE;
    if (s[1]<=0) s[1]=XP_FX_ONE;
    if (s[2]<=0) s[2]=XP_FX_ONE;
}

static void ap_apply_scale_rows(xp_fx32 m[16], const xp_fx32 s[3]) {
    int r,c;
    for (r=0;r<3;++r) for (c=0;c<3;++c) m[r*4+c]=ap_mul_sat(m[r*4+c],s[r]);
}

static void ap_copy16(xp_fx32 d[16], const xp_fx32 s[16]) { int i; for(i=0;i<16;++i)d[i]=s[i]; }

static void ap_cap_node_begin(void* u, unsigned int id, unsigned int parent, const char* name, unsigned int depth) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    unsigned long i=c->required.nodes++;
    (void)depth;
    if (i < c->node_capacity && c->nodes) {
        xp_anim_node_record* n=&c->nodes[i];
        n->frame_id=id; n->parent_frame_id=parent; n->flags=0u;
        ap_copyz(n->name,XP_MAX_TOKEN_TEXT,name);
        xp_mat4_identity(n->bind_local);
    } else c->overflowed=1;
}
static void ap_cap_node_transform(void* u, unsigned int id, const xp_fx32 m[16]) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    unsigned long i;
    if (!c->nodes) return;
    for(i=0;i<c->required.nodes && i<c->node_capacity;++i) if(c->nodes[i].frame_id==id) {
        ap_copy16(c->nodes[i].bind_local,m); c->nodes[i].flags|=XP_ANIM_NODE_HAS_BIND_LOCAL; break;
    }
}
static void ap_cap_clip_begin(void* u,const char* name) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    unsigned long i=c->required.clips++;
    c->current_clip=i; c->in_clip=1;
    if(i<c->clip_capacity && c->clips) {
        xp_anim_clip_record* p=&c->clips[i];
        memset(p,0,sizeof(*p)); ap_copyz(p->name,XP_MAX_TOKEN_TEXT,name);
        p->first_track=c->required.tracks;
    } else c->overflowed=1;
}
static void ap_cap_clip_summary(void* u,const char* name,unsigned int tracks,int first,int last,unsigned int tps,int known) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    xp_anim_clip_record* p;
    unsigned long span;
    (void)name; (void)tracks;
    if(!c->in_clip || c->current_clip>=c->clip_capacity || !c->clips) return;
    p=&c->clips[c->current_clip]; p->first_tick=first; p->last_tick=last;
    p->ticks_per_second=tps; p->ticks_per_second_known=known;
    p->frame_count=0ul;
    if(p->source_key_count>0ul && last>=first && first>=0) {
        span=(unsigned long)(last-first);
        if(span<ULONG_MAX) p->frame_count=span+1ul;
    }
}
static void ap_cap_clip_end(void* u,const char* name) { xp_anim_capture* c=(xp_anim_capture*)u; (void)name; c->in_clip=0; }
static void ap_cap_track_begin(void* u,const char* name) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    unsigned long i=c->required.tracks++;
    c->current_track=i; c->in_track=1;
    if(i<c->track_capacity && c->tracks) {
        xp_anim_track_record* t=&c->tracks[i]; memset(t,0,sizeof(*t));
        ap_copyz(t->name,XP_MAX_TOKEN_TEXT,name); t->target_frame_id=XP_FRAME_ID_NONE;
        t->first_key=c->required.keys;
    } else c->overflowed=1;
    if(c->in_clip && c->current_clip<c->clip_capacity && c->clips) c->clips[c->current_clip].track_count++;
}
static void ap_cap_target(void* u,const char* name) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    if(c->in_track && c->current_track<c->track_capacity && c->tracks)
        ap_copyz(c->tracks[c->current_track].target_name,XP_MAX_TOKEN_TEXT,name);
}
static void ap_cap_key(void* u,int raw,xp_anim_key_semantic sem,unsigned int flags,int ki,int time,int nv,const xp_fx32* values) {
    xp_anim_capture* c=(xp_anim_capture*)u;
    unsigned long i=c->required.keys++;
    int j;
    (void)ki;
    if(c->in_clip && c->current_clip<c->clip_capacity && c->clips) c->clips[c->current_clip].source_key_count++;
    if(c->in_track && c->current_track<c->track_capacity && c->tracks) c->tracks[c->current_track].key_count++;
    if(i<c->key_capacity && c->keys) {
        xp_anim_key_record* k=&c->keys[i]; memset(k,0,sizeof(*k));
        k->time=time; k->raw_key_type=raw; k->semantic=sem; k->flags=flags;
        if(nv<0) nv=0;
        if(nv>(int)XP_MAX_KEY_VALUES) nv=(int)XP_MAX_KEY_VALUES;
        k->n_values=(unsigned int)nv;
        for(j=0;j<nv;++j) k->values[j]=values[j];
    } else c->overflowed=1;
}
static void ap_cap_track_end(void* u,const char* name) { xp_anim_capture* c=(xp_anim_capture*)u; (void)name; c->in_track=0; }

void xp_anim_capture_init(xp_anim_capture* c,
                          xp_anim_node_record* nodes,unsigned long nc,
                          xp_anim_clip_record* clips,unsigned long cc,
                          xp_anim_track_record* tracks,unsigned long tc,
                          xp_anim_key_record* keys,unsigned long kc) {
    if(!c) return;
    memset(c,0,sizeof(*c));
    c->nodes=nodes; c->node_capacity=nc; c->clips=clips; c->clip_capacity=cc;
    c->tracks=tracks; c->track_capacity=tc; c->keys=keys; c->key_capacity=kc;
}

void xp_anim_capture_callbacks(xp_callbacks* cb) {
    if(!cb) return;
    memset(cb,0,sizeof(*cb));
    cb->on_frame_node_begin=ap_cap_node_begin;
    cb->on_frame_node_transform=ap_cap_node_transform;
    cb->on_animation_set_begin=ap_cap_clip_begin;
    cb->on_animation_set_summary=ap_cap_clip_summary;
    cb->on_animation_set_end=ap_cap_clip_end;
    cb->on_animation_begin=ap_cap_track_begin;
    cb->on_animation_target=ap_cap_target;
    cb->on_animation_key_semantic=ap_cap_key;
    cb->on_animation_end=ap_cap_track_end;
}

void xp_anim_capture_get_requirements(const xp_anim_capture* c,xp_anim_requirements* r) {
    if(c && r) *r=c->required;
}

unsigned long xp_anim_clip_count(const xp_anim_capture* c) {
    if(!c) return 0ul;
    return c->required.clips;
}

const xp_anim_clip_record* xp_anim_clip_at(const xp_anim_capture* c,unsigned long i) {
    if(!c || !c->clips || i>=c->required.clips || i>=c->clip_capacity) return 0;
    return &c->clips[i];
}

xp_result xp_anim_capture_finalize(xp_anim_capture* c) {
    unsigned long ti,ni;
    if(!c) return XP_EFORMAT;
    if(c->overflowed || c->required.nodes>c->node_capacity || c->required.clips>c->clip_capacity ||
       c->required.tracks>c->track_capacity || c->required.keys>c->key_capacity ||
       (c->required.nodes && !c->nodes) || (c->required.clips && !c->clips) ||
       (c->required.tracks && !c->tracks) || (c->required.keys && !c->keys)) return XP_ELIMIT;
    for(ti=0;ti<c->required.tracks;++ti) {
        xp_anim_track_record* t=&c->tracks[ti];
        unsigned int found=XP_FRAME_ID_NONE; unsigned int matches=0u;
        for(ni=0;ni<c->required.nodes;++ni) if(strcmp(t->target_name,c->nodes[ni].name)==0) {
            found=c->nodes[ni].frame_id; matches++;
        }
        t->flags &= ~(XP_ANIM_TRACK_TARGET_UNRESOLVED|XP_ANIM_TRACK_TARGET_AMBIGUOUS);
        if(matches==0u) { t->target_frame_id=XP_FRAME_ID_NONE; t->flags|=XP_ANIM_TRACK_TARGET_UNRESOLVED; }
        else if(matches>1u) { t->target_frame_id=XP_FRAME_ID_NONE; t->flags|=XP_ANIM_TRACK_TARGET_AMBIGUOUS; }
        else t->target_frame_id=found;
    }
    return XP_OK;
}

static const xp_anim_key_record* ap_find_side(const xp_anim_capture* c,const xp_anim_track_record* t,
                                               xp_anim_key_semantic sem,int tick,int want_next) {
    unsigned long i,end; const xp_anim_key_record* best=0;
    end=t->first_key+t->key_count;
    for(i=t->first_key;i<end;++i) {
        const xp_anim_key_record* k=&c->keys[i];
        if(k->semantic!=sem) continue;
        if(want_next) {
            if(k->time>=tick && (!best || k->time<best->time)) best=k;
        } else {
            if(k->time<=tick && (!best || k->time>best->time)) best=k;
        }
    }
    return best;
}
static const xp_anim_key_record* ap_first_sem(const xp_anim_capture* c,const xp_anim_track_record* t,xp_anim_key_semantic sem) {
    unsigned long i,end; const xp_anim_key_record* best=0; end=t->first_key+t->key_count;
    for(i=t->first_key;i<end;++i) if(c->keys[i].semantic==sem && (!best||c->keys[i].time<best->time)) best=&c->keys[i];
    return best;
}
static const xp_anim_key_record* ap_last_sem(const xp_anim_capture* c,const xp_anim_track_record* t,xp_anim_key_semantic sem) {
    unsigned long i,end; const xp_anim_key_record* best=0; end=t->first_key+t->key_count;
    for(i=t->first_key;i<end;++i) if(c->keys[i].semantic==sem && (!best||c->keys[i].time>best->time)) best=&c->keys[i];
    return best;
}

static int ap_sample_vector(const xp_anim_capture* c,const xp_anim_track_record* t,xp_anim_key_semantic sem,
                            int tick,int n,xp_fx32* out,unsigned int* interp) {
    const xp_anim_key_record *a,*b,*f,*l; xp_fx32 q; int i;
    a=ap_find_side(c,t,sem,tick,0); b=ap_find_side(c,t,sem,tick,1);
    f=ap_first_sem(c,t,sem); l=ap_last_sem(c,t,sem);
    if(!f) return 0;
    if(!a) a=f;
    if(!b) b=l;
    if((int)a->n_values<n || (int)b->n_values<n) return 0;
    if(a==b || a->time==b->time || tick<=a->time) { for(i=0;i<n;++i)out[i]=a->values[i]; return 1; }
    if(tick>=b->time) { for(i=0;i<n;++i)out[i]=b->values[i]; return 1; }
    q=ap_ratio_q16((unsigned long)(tick-a->time),(unsigned long)(b->time-a->time));
    if(sem==XP_ANIM_SEM_ROTATION && n==4) ap_quat_nlerp(a->values,b->values,q,out);
    else for(i=0;i<n;++i) out[i]=ap_lerp(a->values[i],b->values[i],q);
    if(interp) *interp=1u;
    return 1;
}

static int ap_sample_matrix(const xp_anim_capture* c,const xp_anim_track_record* t,int tick,
                            xp_fx32 out[16],unsigned int* held) {
    const xp_anim_key_record *a,*f; int i;
    a=ap_find_side(c,t,XP_ANIM_SEM_MATRIX,tick,0); f=ap_first_sem(c,t,XP_ANIM_SEM_MATRIX);
    if(!f) return 0;
    if(!a) { a=f; if(held && tick!=a->time)*held=1u; }
    else if(held && tick!=a->time) *held=1u;
    if(a->n_values<16u) return 0;
    for(i=0;i<16;++i)out[i]=a->values[i];
    return 1;
}

static xp_anim_local_pose* ap_pose_for_frame(const xp_anim_capture* c,xp_anim_local_pose* poses,unsigned long n,unsigned int frame_id) {
    unsigned long i; (void)c; for(i=0;i<n;++i) if(poses[i].frame_id==frame_id) return &poses[i]; return 0;
}

static void ap_init_pose(const xp_anim_node_record* n,xp_anim_local_pose* p) {
    xp_fx32 s[3];
    memset(p,0,sizeof(*p)); p->frame_id=n->frame_id; p->parent_frame_id=n->parent_frame_id; p->flags=XP_ANIM_POSE_FROM_BIND;
    if(n->flags&XP_ANIM_NODE_HAS_BIND_LOCAL) ap_copy16(p->local_matrix,n->bind_local); else xp_mat4_identity(p->local_matrix);
    p->position[0]=p->local_matrix[12]; p->position[1]=p->local_matrix[13]; p->position[2]=p->local_matrix[14];
    p->rotation_wxyz[0]=XP_FX_ONE; p->rotation_wxyz[1]=p->rotation_wxyz[2]=p->rotation_wxyz[3]=0;
    ap_bind_scales(p->local_matrix,s); p->scale[0]=s[0];p->scale[1]=s[1];p->scale[2]=s[2];
}

static void ap_apply_track(const xp_anim_capture* c,const xp_anim_track_record* t,int tick,xp_anim_local_pose* p) {
    xp_fx32 m[16],pos[3],rot[4],sc[3],bind_sc[3]; unsigned int interp=0u,held=0u;
    int hp,hr,hs;
    if(ap_sample_matrix(c,t,tick,m,&held)) {
        ap_copy16(p->local_matrix,m); p->position[0]=m[12];p->position[1]=m[13];p->position[2]=m[14];
        p->flags=XP_ANIM_POSE_ANIMATED|XP_ANIM_POSE_MATRIX_KEY|(held?XP_ANIM_POSE_MATRIX_HELD:0u); return;
    }
    hp=ap_sample_vector(c,t,XP_ANIM_SEM_POSITION,tick,3,pos,&interp);
    hr=ap_sample_vector(c,t,XP_ANIM_SEM_ROTATION,tick,4,rot,&interp);
    hs=ap_sample_vector(c,t,XP_ANIM_SEM_SCALE,tick,3,sc,&interp);
    if(!hp&&!hr&&!hs) return;
    ap_bind_scales(p->local_matrix,bind_sc);
    if(!hp){pos[0]=p->local_matrix[12];pos[1]=p->local_matrix[13];pos[2]=p->local_matrix[14];}
    if(!hs){sc[0]=bind_sc[0];sc[1]=bind_sc[1];sc[2]=bind_sc[2];}
    if(hr) {
        ap_quat_to_matrix(rot,m); ap_apply_scale_rows(m,sc);
    } else {
        int r,col; ap_copy16(m,p->local_matrix);
        if(hs) {
            for(r=0;r<3;++r) {
                xp_fx32 bs=bind_sc[r]; if(bs<=0)bs=XP_FX_ONE;
                for(col=0;col<3;++col) m[r*4+col]=ap_mul_sat(ap_div(m[r*4+col],bs),sc[r]);
            }
        }
    }
    m[12]=pos[0];m[13]=pos[1];m[14]=pos[2];
    ap_copy16(p->local_matrix,m); p->position[0]=pos[0];p->position[1]=pos[1];p->position[2]=pos[2];
    p->scale[0]=sc[0];p->scale[1]=sc[1];p->scale[2]=sc[2];
    if(hr){p->rotation_wxyz[0]=rot[0];p->rotation_wxyz[1]=rot[1];p->rotation_wxyz[2]=rot[2];p->rotation_wxyz[3]=rot[3];p->has_rotation=1u;}
    p->has_position=hp?1u:0u; p->has_scale=hs?1u:0u;
    p->flags=XP_ANIM_POSE_ANIMATED|XP_ANIM_POSE_SRT_KEYS|(interp?XP_ANIM_POSE_INTERPOLATED:0u);
}

xp_result xp_anim_sample_pose_frame(const xp_anim_capture* c,unsigned long ci,unsigned long fi,
                                    xp_anim_local_pose* poses,unsigned long pc,int* out_tick) {
    const xp_anim_clip_record* clip;
    unsigned long ni,ti,tick_ul;
    int tick;
    if(!c || !poses) return XP_EFORMAT;
    if(c->overflowed || c->required.nodes>c->node_capacity || c->required.clips>c->clip_capacity ||
       c->required.tracks>c->track_capacity || c->required.keys>c->key_capacity) return XP_ELIMIT;
    if(ci>=c->required.clips || !c->clips) return XP_EFORMAT;
    if(pc<c->required.nodes) return XP_ELIMIT;
    clip=&c->clips[ci];
    if(fi>=clip->frame_count) return XP_EFORMAT;
    for(ti=clip->first_track;ti<clip->first_track+clip->track_count;++ti)
        if(c->tracks[ti].flags&(XP_ANIM_TRACK_TARGET_UNRESOLVED|XP_ANIM_TRACK_TARGET_AMBIGUOUS)) return XP_ENOTSUP;
    tick_ul=(unsigned long)clip->first_tick+fi;
    if(tick_ul>(unsigned long)INT_MAX) return XP_ELIMIT;
    tick=(int)tick_ul;
    for(ni=0;ni<c->required.nodes;++ni) ap_init_pose(&c->nodes[ni],&poses[ni]);
    for(ti=clip->first_track;ti<clip->first_track+clip->track_count;++ti) {
        xp_anim_local_pose* p;
        p=ap_pose_for_frame(c,poses,c->required.nodes,c->tracks[ti].target_frame_id);
        if(p) ap_apply_track(c,&c->tracks[ti],tick,p);
    }
    if(out_tick) *out_tick=tick;
    return XP_OK;
}

xp_result xp_anim_emit_all_poses(const xp_anim_capture* c,xp_anim_local_pose* poses,unsigned long pc,
                                 const xp_anim_pose_callbacks* cb,void* user) {
    unsigned long ci,fi,ni;
    xp_result rr;
    int tick;
    if(!c || !poses) return XP_EFORMAT;
    if(c->overflowed || c->required.nodes>c->node_capacity || c->required.clips>c->clip_capacity ||
       c->required.tracks>c->track_capacity || c->required.keys>c->key_capacity) return XP_ELIMIT;
    if(pc<c->required.nodes) return XP_ELIMIT;
    for(ci=0;ci<c->required.clips;++ci) {
        const xp_anim_clip_record* clip;
        clip=&c->clips[ci];
        if(cb&&cb->on_clip_begin)cb->on_clip_begin(user,ci,clip,c->required.nodes);
        for(fi=0;fi<clip->frame_count;++fi) {
            rr=xp_anim_sample_pose_frame(c,ci,fi,poses,pc,&tick);
            if(rr!=XP_OK) return rr;
            if(cb&&cb->on_pose_frame_begin)cb->on_pose_frame_begin(user,ci,fi,tick,c->required.nodes);
            for(ni=0;ni<c->required.nodes;++ni)
                if(cb&&cb->on_pose_node)cb->on_pose_node(user,ci,fi,tick,&c->nodes[ni],&poses[ni]);
            if(cb&&cb->on_pose_frame_end)cb->on_pose_frame_end(user,ci,fi,tick);
        }
        if(cb&&cb->on_clip_end)cb->on_clip_end(user,ci,clip);
    }
    return XP_OK;
}
