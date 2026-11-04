#include "re4c3d/re4_skeletal.h"
#include "re4c3d/re4_base.h"
#include <string.h>

#define ONE 65536L
#define PI 205887L
#define TWO_PI 411775L
#define HALF_PI 102944L
#define LIM 2147483647L

static re4_i32 satadd(re4_i32 a,re4_i32 b){if(b>0 && a>LIM-b)return LIM;if(b<0 && a<(-LIM-1L)-b)return -LIM-1L;return a+b;}
static re4_q16 mul(re4_q16 a,re4_q16 b)
{
    re4_u32 ua,ub,hi,lo,hj,lj,r,p;re4_bool n=((a<0)!=(b<0));
    ua=a<0?(re4_u32)(-(a+1))+1UL:(re4_u32)a;ub=b<0?(re4_u32)(-(b+1))+1UL:(re4_u32)b;
    hi=ua>>16;lo=ua&65535UL;hj=ub>>16;lj=ub&65535UL;
    if(hi && hj>32767UL/hi)return n?-LIM-1L:LIM;
    r=(hi*hj)<<16;
    p=hi*lj;if(p>LIM-(re4_i32)r)return n?-LIM-1L:LIM;r+=p;
    p=hj*lo;if(p>LIM-(re4_i32)r)return n?-LIM-1L:LIM;r+=p;
    p=(lo*lj)>>16;if(p>LIM-(re4_i32)r)return n?-LIM-1L:LIM;r+=p;
    return n?-(re4_i32)r:(re4_i32)r;
}
/* CORDIC Q16.16 angle input in radians; no libm, no float. */
static void sincos_fixed(re4_q16 a,re4_q16 *s,re4_q16 *c)
{
    static const re4_i32 atan_table[16]={51472,30386,16055,8150,4091,2047,1024,512,256,128,64,32,16,8,4,2};
    re4_i32 x=39797L,y=0,z=a,xn,yn;re4_u32 i;re4_i32 sign=1;
    if(a==0){*s=0;*c=ONE;return;}
    while(z>PI)z-=TWO_PI;
    while(z< -PI)z+=TWO_PI;
    if(z>HALF_PI){z-=PI;sign=-1;}else if(z< -HALF_PI){z+=PI;sign=-1;}
    for(i=0;i<16U;++i){
        if(z>=0){xn=x-(y>>i);yn=y+(x>>i);z-=atan_table[i];}
        else {xn=x+(y>>i);yn=y-(x>>i);z+=atan_table[i];}
        x=xn;y=yn;
    }
    *c=x*sign;*s=y*sign;
}
static void ident(re4_skel_matrix *m)
{
    re4_u32 i;for(i=0;i<12;++i)m->m[i]=0;
    m->m[0]=ONE;m->m[5]=ONE;m->m[10]=ONE;
}
static void rot_xyz(re4_q16 x,re4_q16 y,re4_q16 z,re4_skel_matrix *m)
{
    re4_q16 sx,cx,sy,cy,sz,cz;
    sincos_fixed(x,&sx,&cx);sincos_fixed(y,&sy,&cy);sincos_fixed(z,&sz,&cz);
    ident(m);
    /* R = Rz * Ry * Rx, local Euler radians. */
    m->m[0]=mul(cz,cy);
    m->m[1]=satadd(mul(mul(cz,sy),sx),-mul(sz,cx));
    m->m[2]=satadd(mul(mul(cz,sy),cx),mul(sz,sx));
    m->m[4]=mul(sz,cy);
    m->m[5]=satadd(mul(mul(sz,sy),sx),mul(cz,cx));
    m->m[6]=satadd(mul(mul(sz,sy),cx),-mul(cz,sx));
    m->m[8]=-sy;m->m[9]=mul(cy,sx);m->m[10]=mul(cy,cx);
}
/* Quaternion bind orientation from the PMD skeleton. Q16.16, xyzw.
   Compose bind orientation with the FCV Euler delta, retaining local
   translation, rather than rejecting non-identity bind quaternions. */
static void bind_quat_matrix(const re4_q16 *v,re4_skel_matrix *m)
{
    re4_q16 x=v[3],y=v[4],z=v[5],w=v[6];
    re4_q16 xx=mul(x,x),yy=mul(y,y),zz=mul(z,z);
    re4_q16 xy=mul(x,y),xz=mul(x,z),yz=mul(y,z);
    re4_q16 wx=mul(w,x),wy=mul(w,y),wz=mul(w,z);
    ident(m);
    m->m[0]=satadd(ONE,-2L*satadd(yy,zz));
    m->m[1]=2L*satadd(xy,-wz);
    m->m[2]=2L*satadd(xz,wy);
    m->m[4]=2L*satadd(xy,wz);
    m->m[5]=satadd(ONE,-2L*satadd(xx,zz));
    m->m[6]=2L*satadd(yz,-wx);
    m->m[8]=2L*satadd(xz,-wy);
    m->m[9]=2L*satadd(yz,wx);
    m->m[10]=satadd(ONE,-2L*satadd(xx,yy));
}
static void matmul(const re4_skel_matrix *a,const re4_skel_matrix *b,re4_skel_matrix *o)
{
    re4_u32 r,c,k;re4_skel_matrix t;
    for(r=0;r<3;++r)for(c=0;c<4;++c){re4_q16 v=0;
        for(k=0;k<3;++k)v=satadd(v,mul(a->m[r*4+k],b->m[k*4+c]));
        if(c==3)v=satadd(v,a->m[r*4+3]);
        t.m[r*4+c]=v;
    }
    *o=t;
}
/* Strict FCV -> PMD joint matching. Some small animals use Maya export names
   with a model-specific prefix rather than 'born__NN':
     em2300_crow__NN / em2900_bat__NN
   Matching a numeric suffix is NOT safe globally; only these verified stems
   and born__ are eligible. Ambiguous/duplicated IDs fail closed. C89. */
/* Some large enemy PMDs use 3-digit joint identifiers. FCV bone_no is
   8-bit; accept 2 or 3 decimal digits, reject values >255 and malformed
   suffixes.  Never truncate born__100 to born__10. */
static re4_bool fcv_joint_name(re4_strref n,re4_u8 id)
{
    const char *s=n.ptr;
    re4_size i,st;
    re4_u32 value=0U,digits=0U;
    if(!s)return RE4_FALSE;
    if(n.len>=6UL && memcmp(s,"born__",6)==0)st=6UL;
    /* Original armored Ganado rigs use born1__NN for the master,
       unlike the common born__NN convention.  Ambiguity is rejected
       by re4_skel_joint_for_fcv_id(), not resolved by preference. */
    else if(n.len>=7UL && (memcmp(s,"born1__",7)==0 ||
                             memcmp(s,"born2__",7)==0))st=7UL;
    else if(n.len>=13UL && memcmp(s,"em2300_crow__",13)==0)st=13UL;
    else if(n.len>=12UL && memcmp(s,"em2900_bat__",12)==0)st=12UL;
    else return RE4_FALSE;
    i=st;
    while(i<n.len && s[i]>='0' && s[i]<='9'){
        if(digits>=3U)return RE4_FALSE;
        value=value*10U+(re4_u32)(s[i]-'0');
        ++i;++digits;
    }
    if(digits<2U || value>255U)return RE4_FALSE;
    if(i<n.len && s[i]!='_')return RE4_FALSE;
    return value==(re4_u32)id;
}
re4_i32 re4_skel_joint_for_fcv_id(const re4_pmd_model *model,re4_u8 id)
{
    re4_u32 i;re4_i32 found=-1;
    if(!model || !model->skeleton_names)return -1;
    for(i=0;i<model->skeleton_name_count && i<model->skeleton_count;++i){
        if(fcv_joint_name(model->skeleton_names[i].name,id)){
            if(found>=0)return -1; /* ambiguous joint identities */
            found=(re4_i32)i;
        }
    }
    return found;
}
/* Sample the original rotation curves in local space. A missing channel is
   zero delta from the PMD bind pose, never an uninitialised matrix. */
static re4_err sample_local_angles(const re4_pmd_model *model, re4_span bytes,
    const re4_fcv *fcv, re4_u16 frame, re4_q16 angles[RE4_SKEL_MAX_BONES][3],
    re4_u32 *applied, re4_u32 *special)
{
    re4_u32 i,k;
    memset(angles,0,sizeof(re4_q16)*RE4_SKEL_MAX_BONES*3U);
    *applied=0;*special=0;
    if(!fcv)return RE4_OK;
    if(fcv->block_count && !fcv->blocks)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<fcv->block_count;++i){
        const re4_fcv_block *b=&fcv->blocks[i];
        re4_i32 sk=re4_skel_joint_for_fcv_id(model,b->bone_no);
        if(b->bone_type==2U && sk>=0){
            re4_fcv_axis_view v[3];
            re4_err e=re4_fcv_block_axis_views_e(bytes,b,fcv->endian,&v[0],&v[1],&v[2]);
            if(e!=RE4_OK)return e;
            for(k=0;k<3U;++k){
                e=re4_fcv_axis_sample_hermite_q16(&v[k],frame,fcv->endian,&angles[sk][k]);
                if(e!=RE4_OK)return e;
            }
            ++*applied;
        }else ++*special;
    }
    return RE4_OK;
}
static re4_err build_fk(const re4_pmd_model *model,
    re4_q16 angles[RE4_SKEL_MAX_BONES][3], re4_skel_pose *pose)
{
    re4_u32 i;
    for(i=0;i<model->skeleton_count;++i){
        re4_skel_matrix local,anim,bind;
        re4_i32 par=model->skeletons[i].parent;
        const re4_q16 *v=model->skeletons[i].data;
        if(par >= (re4_i32)i || par < -1)return RE4_ERR_BAD_FORMAT;
        rot_xyz(angles[i][0],angles[i][1],angles[i][2],&anim);
        bind_quat_matrix(v,&bind);
        matmul(&bind,&anim,&local);
        local.m[3]=v[7];local.m[7]=v[8];local.m[11]=v[9];
        if(par>=0)matmul(&pose->world[par],&local,&pose->world[i]);
        else pose->world[i]=local;
    }
    return RE4_OK;
}
/* Q16.16 interpolation over the shortest Euler angular arc. This preserves
   orthonormal rotations (unlike blending world matrices element by element).
   It is a diagnostic crossfade, not proof of Capcom's native blend policy. */
static re4_q16 blend_angle(re4_q16 a,re4_q16 b,re4_q16 w)
{
    re4_q16 delta;
    a%=TWO_PI;b%=TWO_PI;
    delta=b-a;
    if(delta>PI)delta-=TWO_PI;
    else if(delta< -PI)delta+=TWO_PI;
    return satadd(a,mul(delta,w));
}
re4_err re4_skel_evaluate_blend(const re4_pmd_model *model,
    re4_span src_a,const re4_fcv *clip_a,re4_u16 frame_a,
    re4_span src_b,const re4_fcv *clip_b,re4_u16 frame_b,
    re4_q16 weight_b,re4_skel_pose *pose,re4_u32 capacity)
{
    re4_q16 aa[RE4_SKEL_MAX_BONES][3],bb[RE4_SKEL_MAX_BONES][3];
    re4_u32 rot_a=0,rot_b=0,special_a=0,special_b=0,i,k;
    re4_err e;
    if(!model||!pose||!pose->world)return RE4_ERR_NULL;
    if(weight_b<0||weight_b>ONE)return RE4_ERR_BAD_FORMAT;
    if(model->skeleton_count>RE4_SKEL_MAX_BONES||capacity<model->skeleton_count)return RE4_ERR_NO_SPACE;
    if(model->skeleton_count&&!model->skeletons)return RE4_ERR_BAD_FORMAT;
    if(model->skeleton_name_count && !model->skeleton_names)return RE4_ERR_BAD_FORMAT;
    if(clip_a && !src_a.data)return RE4_ERR_NULL;
    if(clip_b && !src_b.data)return RE4_ERR_NULL;
    pose->bone_count=model->skeleton_count;
    pose->applied_rotation_channels=0;
    pose->ignored_special_channels=0;
    e=sample_local_angles(model,src_a,clip_a,frame_a,aa,&rot_a,&special_a);
    if(e!=RE4_OK)return e;
    if(weight_b){
        e=sample_local_angles(model,src_b,clip_b,frame_b,bb,&rot_b,&special_b);
        if(e!=RE4_OK)return e;
        for(i=0;i<model->skeleton_count;++i)
            for(k=0;k<3U;++k)aa[i][k]=blend_angle(aa[i][k],bb[i][k],weight_b);
    }
    pose->applied_rotation_channels=rot_a+rot_b;
    pose->ignored_special_channels=special_a+special_b;
    return build_fk(model,aa,pose);
}
re4_err re4_skel_evaluate(const re4_pmd_model *model,re4_span bytes,
    const re4_fcv *fcv,re4_u16 frame,re4_skel_pose *pose,re4_u32 cap)
{
    re4_span empty;
    empty.data=0;empty.size=0;
    return re4_skel_evaluate_blend(model,bytes,fcv,frame,
        empty,0,0,0,pose,cap);
}
static re4_err skin_matrix(re4_span bytes,const re4_pmd_node *node,re4_u32 slot,const re4_skel_pose *pose,re4_skel_matrix *out)
{
    re4_u32 k,r,c;re4_i32 bone;re4_size pos;re4_skel_matrix inv;
    if(slot>=node->node_bone_count)return RE4_ERR_BAD_FORMAT;
    pos=(re4_size)node->node_bone_offset+72UL*slot;
    if(!re4_span_ok(bytes,pos,72UL))return RE4_ERR_RANGE;
    bone=re4_read_i32_e(bytes.data+pos,RE4_ENDIAN_LITTLE);
    if(bone<0 || (re4_u32)bone>=pose->bone_count)return RE4_ERR_BAD_FORMAT;
    /* 4-byte global bone index, 4-byte auxiliary weight flag, 16 f32
       column-major inverse-bind entries. */
    for(r=0;r<3;++r)for(c=0;c<4;++c){
        k=(c*4U+r);inv.m[r*4U+c]=re4_f32bits_to_q16(re4_read_u32_le(bytes.data+pos+8UL+k*4UL));
    }
    matmul(&pose->world[bone],&inv,out);
    return RE4_OK;
}
static re4_err skslot(re4_q16 idx,re4_u32 *slot)
{
    if(idx<0)return RE4_ERR_BAD_FORMAT;
    /* PMD shader index is a FLOAT referring to a triplet, i.e. slot*3. */
    *slot=(re4_u32)((idx+98304L)/196608L);return RE4_OK;
}
re4_err re4_skel_skin_vertex(re4_span bytes,const re4_pmd_node *node,const re4_pmd_vertex_q16 *v,const re4_skel_pose *pose,re4_q16 p[3],re4_q16 n[3])
{
    re4_q16 w[2],ws=0,pp[3]={0,0,0},nn[3]={0,0,0};re4_u32 i,j,k;
    if(!node||!v||!pose||!pose->world||!p||!n)return RE4_ERR_NULL;
    w[0]=v->weights[0];w[1]=v->weights[1];
    if(w[0]<0||w[1]<0)return RE4_ERR_BAD_FORMAT;
    ws=satadd(w[0],w[1]);if(ws<32768L || ws>131072L)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<2;++i){
        re4_u32 slot;re4_skel_matrix mat;re4_q16 bp[3],bn[3];re4_err e;
        if(w[i]==0)continue;
        e=skslot(v->bone_indices[i],&slot);if(e!=RE4_OK)return e;
        e=skin_matrix(bytes,node,slot,pose,&mat);if(e!=RE4_OK)return e;
        for(j=0;j<3;++j){bp[j]=mat.m[4UL*j+3];bn[j]=0;
            for(k=0;k<3;++k){bp[j]=satadd(bp[j],mul(mat.m[4UL*j+k],v->position[k]));bn[j]=satadd(bn[j],mul(mat.m[4UL*j+k],v->normal[k]));}
            pp[j]=satadd(pp[j],mul(w[i],bp[j]));nn[j]=satadd(nn[j],mul(w[i],bn[j]));
        }
    }
    for(j=0;j<3;++j){p[j]=pp[j];n[j]=nn[j];}
    return RE4_OK;
}

/* Nodes without a node-bone table occur in original RE4 PC 2007 PMDs.
   Their vertices have one unit influence and two zero slot IDs. They are
   treated as rigid node transforms relative to the bind pose, NOT as a
   malformed skinned table. There is no change to the 2-weight LBS path. */
re4_err re4_skel_skin_rigid_node(const re4_pmd_node *node,
    const re4_pmd_vertex_q16 *v,const re4_skel_pose *rest,
    const re4_skel_pose *animated,re4_q16 p[3],re4_q16 n[3])
{
    re4_skel_matrix inv,delta;
    re4_u32 id,i,j;re4_q16 val;
    const re4_skel_matrix *base;
    if(!node||!v||!rest||!animated||!p||!n||!rest->world||!animated->world)
        return RE4_ERR_NULL;
    if(node->node_bone_count!=0U || node->skeleton_index<0)
        return RE4_ERR_UNSUPPORTED;
    if(v->weights[0]!=ONE || v->weights[1]!=0 ||
       v->bone_indices[0]!=0 || v->bone_indices[1]!=0)
        return RE4_ERR_BAD_FORMAT;
    id=(re4_u32)node->skeleton_index;
    if(id>=rest->bone_count||id>=animated->bone_count)return RE4_ERR_BAD_FORMAT;
    base=&rest->world[id];
    /* PMD rigid bind transforms must be orthonormal; this path uses
       transposition for the inverse as in the multipart retarget adapter. */
    for(i=0;i<3U;++i)for(j=0;j<3U;++j)
        inv.m[i*4U+j]=base->m[j*4U+i];
    for(i=0;i<3U;++i){
        val=0;
        for(j=0;j<3U;++j)val=satadd(val,mul(inv.m[i*4U+j],base->m[j*4U+3U]));
        inv.m[i*4U+3U]=val==(-LIM-1L)?LIM:-val;
    }
    matmul(&animated->world[id],&inv,&delta);
    for(i=0;i<3U;++i){
        p[i]=delta.m[4U*i+3U];n[i]=0;
        for(j=0;j<3U;++j){
            p[i]=satadd(p[i],mul(delta.m[4U*i+j],v->position[j]));
            n[i]=satadd(n[i],mul(delta.m[4U*i+j],v->normal[j]));
        }
    }
    return RE4_OK;
}

void re4_skel_clock_init(re4_skel_clock *c,re4_u16 end_frame)
{
    if(!c)return;
    c->position_q8=0;c->end_frame=end_frame;c->speed_q8=256;
    c->looping=RE4_TRUE;c->playing=RE4_TRUE;
}
void re4_skel_clock_seek(re4_skel_clock *c,re4_u16 frame)
{
    if(!c)return;
    if(frame>c->end_frame)frame=c->end_frame;
    c->position_q8=(re4_u32)frame*256UL;
}
void re4_skel_clock_step(re4_skel_clock *c,re4_u16 delta_q8)
{
    re4_u32 add,limit;
    if(!c||!c->playing)return;
    /* Bounded 32-bit multiplication: inputs are each 16-bit. */
    add=((re4_u32)delta_q8*(re4_u32)c->speed_q8)>>8;
    limit=((re4_u32)c->end_frame+1UL)*256UL;
    if(limit==0)return;
    if(c->looping)c->position_q8=(c->position_q8+add)%limit;
    else {
        re4_u32 maxpos=(re4_u32)c->end_frame*256UL;
        if(add>=maxpos || c->position_q8>=maxpos-add){c->position_q8=maxpos;c->playing=RE4_FALSE;}
        else c->position_q8+=add;
    }
}
re4_u16 re4_skel_clock_frame(const re4_skel_clock *c)
{
    re4_u32 frame;
    if(!c)return 0;
    frame=(c->position_q8+128UL)>>8;
    return (re4_u16)(frame>c->end_frame?c->end_frame:frame);
}

/* World-space two-joint CCD overlay. The game's native IK channel targets and
   graph semantics must be independently reconstructed. This solver is an
   opt-in geometric constraint useful for experimenting with those channels. */
static re4_bool ik_descendant(const re4_pmd_model *m,re4_u32 node,re4_u32 ancestor)
{
    re4_u32 steps=0;re4_i32 p=(re4_i32)node;
    while(p>=0 && steps<=m->skeleton_count){
        if((re4_u32)p==ancestor)return RE4_TRUE;
        if((re4_u32)p>=m->skeleton_count)return RE4_FALSE;
        p=m->skeletons[p].parent;++steps;
    }
    return RE4_FALSE;
}
static re4_q16 ik_abs(re4_q16 v)
{
    return v==(-2147483647L-1L)?2147483647L:(v<0?-v:v);
}
static re4_q16 ik_error(re4_q16 x,re4_q16 y,re4_q16 z,const re4_q16 target[3])
{
    return satadd(satadd(ik_abs(satadd(x,-target[0])),ik_abs(satadd(y,-target[1]))),
                  ik_abs(satadd(z,-target[2])));
}
static void ik_rotate_point(const re4_skel_matrix *r,const re4_q16 pivot[3],
                            const re4_q16 original[3],re4_q16 output[3])
{
    re4_u32 j,k;
    for(j=0;j<3;++j){
        re4_q16 v=pivot[j];
        for(k=0;k<3;++k)v=satadd(v,mul(r->m[4U*j+k],satadd(original[k],-pivot[k])));
        output[j]=v;
    }
}
static re4_err solve_ik(const re4_pmd_model *m,re4_skel_pose *pose,
        re4_u32 proximal,re4_u32 distal,re4_u32 tip,
        const re4_q16 target[3],re4_u32 iterations,re4_q16 step,re4_i32 plane_axis,
        const re4_q16 *lower,const re4_q16 *upper,re4_q16 *out_error)
{
    re4_u32 iter,k,axis,sgn,pivot_index,piv,j;
    re4_q16 before,best,current[3],origin[3],trial[3],best_step;
    re4_skel_matrix rot,best_rot,tmp;
    re4_q16 correction[2]={0,0};
    if((lower&&!upper)||(!lower&&upper))return RE4_ERR_NULL;
    if(lower && (lower[0]>0 || lower[1]>0 || upper[0]<0 || upper[1]<0 ||
       lower[0]>upper[0] || lower[1]>upper[1] ||
       lower[0]<-TWO_PI || lower[1]<-TWO_PI ||
       upper[0]>TWO_PI || upper[1]>TWO_PI))return RE4_ERR_BAD_FORMAT;
    if(!m||!pose||!pose->world||!target)return RE4_ERR_NULL;
    if(m->skeleton_count>RE4_SKEL_MAX_BONES||m->skeleton_count!=pose->bone_count ||
       proximal>=m->skeleton_count||distal>=m->skeleton_count||tip>=m->skeleton_count ||
       proximal==distal||distal==tip||!ik_descendant(m,distal,proximal)||
       !ik_descendant(m,tip,distal)||step<=0||step>ONE||iterations>256U)return RE4_ERR_BAD_FORMAT;
    for(iter=0;iter<iterations;++iter){
        re4_bool moved=RE4_FALSE;
        for(pivot_index=0;pivot_index<2U;++pivot_index){
            piv=(pivot_index==0U)?distal:proximal;
            for(k=0;k<3U;++k){origin[k]=pose->world[piv].m[k*4U+3U];current[k]=pose->world[tip].m[k*4U+3U];}
            before=ik_error(current[0],current[1],current[2],target);best=before;best_step=0;
            ident(&best_rot);
            for(axis=0;axis<3U;++axis)for(sgn=0;sgn<2U;++sgn){
                re4_q16 ang[3]={0,0,0};
                if(plane_axis>=0 && (re4_i32)axis!=plane_axis)continue;
                ang[axis]=sgn?step:-step;
                if(lower && (correction[pivot_index]+ang[axis]<lower[pivot_index] ||
                             correction[pivot_index]+ang[axis]>upper[pivot_index]))continue;
                rot_xyz(ang[0],ang[1],ang[2],&rot);
                ik_rotate_point(&rot,origin,current,trial);
                {re4_q16 candidate=ik_error(trial[0],trial[1],trial[2],target);
                 if(candidate<best){best=candidate;best_rot=rot;best_step=ang[axis];}}
            }
            if(best_step!=0){
                correction[pivot_index]+=best_step;
                moved=RE4_TRUE;
                for(j=0;j<m->skeleton_count;++j){
                    if(ik_descendant(m,j,piv)){
                        re4_q16 old[3],now[3];
                        for(k=0;k<3U;++k)old[k]=pose->world[j].m[k*4U+3U];
                        ik_rotate_point(&best_rot,origin,old,now);
                        matmul(&best_rot,&pose->world[j],&tmp);
                        for(k=0;k<3U;++k)tmp.m[k*4U+3U]=now[k];
                        pose->world[j]=tmp;
                    }
                }
            }
        }
        if(!moved){step/=2;if(step<16L)break;}
    }
    if(out_error)*out_error=ik_error(pose->world[tip].m[3],pose->world[tip].m[7],pose->world[tip].m[11],target);
    return RE4_OK;
}

re4_err re4_skel_ik_ccd_two_bone(const re4_pmd_model *m,re4_skel_pose *pose,
        re4_u32 a,re4_u32 b,re4_u32 tip,const re4_q16 target[3],
        re4_u32 iterations,re4_q16 step,re4_q16 *error)
{
    return solve_ik(m,pose,a,b,tip,target,iterations,step,-1,0,0,error);
}
re4_err re4_skel_ik_ccd_axis(const re4_pmd_model *m,re4_skel_pose *pose,
        re4_u32 a,re4_u32 b,re4_u32 tip,const re4_q16 target[3],
        re4_u32 iterations,re4_q16 step,re4_u8 axis,re4_q16 *error)
{
    if(axis>2U)return RE4_ERR_BAD_FORMAT;
    return solve_ik(m,pose,a,b,tip,target,iterations,step,(re4_i32)axis,0,0,error);
}

/* Optional bounded WORLD-AXIS CCD correction. These limits bound the
   ADDED correction on each pivot (not inferred native anatomical limits).
   Index 0=distal joint, index 1=proximal joint. */
re4_err re4_skel_ik_ccd_axis_limited(const re4_pmd_model *m,
    re4_skel_pose *pose,re4_u32 a,re4_u32 b,re4_u32 tip,
    const re4_q16 target[3],re4_u32 iterations,re4_q16 step,re4_u8 axis,
    const re4_q16 lower[2],const re4_q16 upper[2],re4_q16 *error)
{
    if(axis>2U)return RE4_ERR_BAD_FORMAT;
    if(!lower||!upper)return RE4_ERR_NULL;
    return solve_ik(m,pose,a,b,tip,target,iterations,step,(re4_i32)axis,
                    lower,upper,error);
}
