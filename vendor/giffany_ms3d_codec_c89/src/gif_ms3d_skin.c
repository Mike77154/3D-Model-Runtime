#include "gif_ms3d_skin.h"
#include "gif_ms3d_quat.h"
#include <string.h>

#define MS3D_PI 205887L
#define MS3D_HALF_PI 102944L
#define MS3D_TWO_PI 411775L

/* 16-stage CORDIC: no math.h or floating-point dependency. */
static const long s_atan[16] = {
    51472L,30386L,16055L,8149L,4091L,2047L,1024L,512L,
    256L,128L,64L,32L,16L,8L,4L,2L
};
static void sincos_q16(ms3d_fx rad, ms3d_fx *s, ms3d_fx *c)
{
    long x, y, z, dx, dy, divisor;
    int i, invert;
    z = rad % MS3D_TWO_PI;
    if (z > MS3D_PI) z -= MS3D_TWO_PI;
    if (z < -MS3D_PI) z += MS3D_TWO_PI;
    invert = 0;
    if (z > MS3D_HALF_PI) { z -= MS3D_PI; invert = 1; }
    else if (z < -MS3D_HALF_PI) { z += MS3D_PI; invert = 1; }
    x = 39797L; y = 0L; divisor = 1L;
    for (i = 0; i < 16; ++i) {
        dx = x / divisor;
        dy = y / divisor;
        if (z >= 0L) {
            x -= dy; y += dx; z -= s_atan[i];
        } else {
            x += dy; y -= dx; z += s_atan[i];
        }
        divisor *= 2L;
    }
    *s = invert ? -y : y;
    *c = invert ? -x : x;
}

void ms3d_affine_identity(ms3d_affine *out)
{
    int i, j;
    for (i=0;i<3;++i) for(j=0;j<4;++j)
        out->m[i][j] = i==j ? MS3D_FX_ONE : 0L;
}
void ms3d_affine_mul(const ms3d_affine *a, const ms3d_affine *b, ms3d_affine *out)
{
    ms3d_affine tmp;
    int i, j, k;
    ms3d_fx v;
    for (i=0;i<3;++i) {
        for (j=0;j<4;++j) {
            v = j==3 ? a->m[i][3] : 0L;
            for (k=0;k<3;++k)
                v=ms3d_fx_add_sat(v,ms3d_fx_mul(a->m[i][k],b->m[k][j]));
            tmp.m[i][j]=v;
        }
    }
    *out=tmp;
}
void ms3d_affine_point(const ms3d_affine *m, const ms3d_fx p[3], ms3d_fx out[3])
{
    ms3d_fx a[3];
    int i,k;
    for(i=0;i<3;++i) {
        a[i]=m->m[i][3];
        for(k=0;k<3;++k) a[i]=ms3d_fx_add_sat(a[i],ms3d_fx_mul(m->m[i][k],p[k]));
    }
    memcpy(out,a,sizeof(a));
}
void ms3d_affine_direction(const ms3d_affine *m, const ms3d_fx p[3], ms3d_fx out[3])
{
    ms3d_fx a[3];
    int i,k;
    for(i=0;i<3;++i) {
        a[i]=0L;
        for(k=0;k<3;++k) a[i]=ms3d_fx_add_sat(a[i],ms3d_fx_mul(m->m[i][k],p[k]));
    }
    memcpy(out,a,sizeof(a));
}
void ms3d_affine_inverse_rigid(const ms3d_affine *m, ms3d_affine *out)
{
    ms3d_affine tmp;
    int i,j;
    ms3d_fx p[3],inv[3];
    for(i=0;i<3;++i) for(j=0;j<3;++j)tmp.m[i][j]=m->m[j][i];
    for(i=0;i<3;++i)p[i]=-m->m[i][3];
    ms3d_affine_direction(&tmp,p,inv);
    for(i=0;i<3;++i)tmp.m[i][3]=inv[i];
    *out=tmp;
}

/* MilkShape reference rotation convention Rz * Ry * Rx, radians. */
static void ms3d_rotation(ms3d_affine *out, const ms3d_fx angles[3], const ms3d_fx xyz[3])
{
    ms3d_fx sx,cx,sy,cy,sz,cz,sxsy,cxsy;
    ms3d_affine_identity(out);
    sincos_q16(angles[0],&sx,&cx);
    sincos_q16(angles[1],&sy,&cy);
    sincos_q16(angles[2],&sz,&cz);
    sxsy=ms3d_fx_mul(sx,sy); cxsy=ms3d_fx_mul(cx,sy);
    out->m[0][0]=ms3d_fx_mul(cy,cz);
    out->m[1][0]=ms3d_fx_mul(cy,sz);
    out->m[2][0]=-sy;
    out->m[0][1]=ms3d_fx_sub_sat(ms3d_fx_mul(sxsy,cz),ms3d_fx_mul(cx,sz));
    out->m[1][1]=ms3d_fx_add_sat(ms3d_fx_mul(sxsy,sz),ms3d_fx_mul(cx,cz));
    out->m[2][1]=ms3d_fx_mul(sx,cy);
    out->m[0][2]=ms3d_fx_add_sat(ms3d_fx_mul(cxsy,cz),ms3d_fx_mul(sx,sz));
    out->m[1][2]=ms3d_fx_sub_sat(ms3d_fx_mul(cxsy,sz),ms3d_fx_mul(sx,cz));
    out->m[2][2]=ms3d_fx_mul(cx,cy);
    out->m[0][3]=xyz[0];out->m[1][3]=xyz[1];out->m[2][3]=xyz[2];
}

/* Topological ordering lets child joints appear BEFORE their parents. */
int ms3d_skin_prepare(const ms3d_model *m, ms3d_skin_pose *p)
{
    unsigned char ready[MS3D_MAX_JOINTS];
    ms3d_affine local;
    unsigned i, pass, got;
    int parent;
    if (!m || !p) return MS3D_E_NULL;
    if (m->num_joints>MS3D_MAX_JOINTS || m->num_vertices>MS3D_MAX_VERTICES)
        return MS3D_E_BAD_COUNT;
    p->joint_count=m->num_joints;
    memset(ready,0,sizeof(ready));
    got=0U;
    for(pass=0;pass<m->num_joints;++pass) {
        unsigned found=0U;
        for(i=0;i<m->num_joints;++i) {
            if(ready[i])continue;
            parent=m->joints[i].parent_index;
            if(parent < -1 || parent >= (int)m->num_joints) return MS3D_E_BAD_PARENT;
            if(parent>=0 && !ready[parent])continue;
            ms3d_rotation(&local,m->joints[i].rotation,m->joints[i].position);
            if(parent>=0) ms3d_affine_mul(&p->bind_global[parent],&local,&p->bind_global[i]);
            else p->bind_global[i]=local;
            ms3d_affine_inverse_rigid(&p->bind_global[i],&p->bind_inverse[i]);
            p->joint_order[got++]=(ms3d_u16)i;
            ready[i]=1U;
            found=1U;
        }
        if(got==m->num_joints)break;
        if(!found)return MS3D_E_BAD_PARENT; /* cycle */
    }
    p->time=0L;
    for(i=0;i<m->num_joints;++i) {
        p->global[i]=p->bind_global[i];
        ms3d_affine_identity(&p->skin[i]);
    }
    return MS3D_OK;
}

static void ms3d_anim_sample(const ms3d_model *m,const ms3d_joint *j,ms3d_fx time,
                             ms3d_fx r[3], ms3d_fx t[3])
{
    unsigned k,c,base;
    ms3d_fx alpha,dt,diff;
    const ms3d_key_rot *ra,*rb;
    const ms3d_key_pos *ta,*tb;
    for(k=0;k<3;++k){r[k]=0L;t[k]=0L;}
    c=j->rot_key_count;base=j->rot_key_start;
    if(c) {
        if(time<=m->rot_keys[base].time)memcpy(r,m->rot_keys[base].rotation,sizeof(ms3d_fx)*3U);
        else if(time>=m->rot_keys[base+c-1U].time)memcpy(r,m->rot_keys[base+c-1U].rotation,sizeof(ms3d_fx)*3U);
        else for(k=0;k+1U<c;++k) {
            ra=&m->rot_keys[base+k];rb=&m->rot_keys[base+k+1U];
            if(time>=ra->time && time<=rb->time) {
                dt=ms3d_fx_sub_sat(rb->time,ra->time);
                alpha=dt>0L?ms3d_fx_div(ms3d_fx_sub_sat(time,ra->time),dt):0L;
                { unsigned a;for(a=0;a<3;++a){
                    diff=ms3d_fx_sub_sat(rb->rotation[a],ra->rotation[a]);
                    /* Shortest angular path over +/- pi. */
                    if(diff>MS3D_PI)diff-=MS3D_TWO_PI;
                    if(diff< -MS3D_PI)diff+=MS3D_TWO_PI;
                    r[a]=ms3d_fx_add_sat(ra->rotation[a],ms3d_fx_mul(diff,alpha));
                }}
                break;
            }
        }
    }
    c=j->pos_key_count;base=j->pos_key_start;
    if(c) {
        if(time<=m->pos_keys[base].time)memcpy(t,m->pos_keys[base].position,sizeof(ms3d_fx)*3U);
        else if(time>=m->pos_keys[base+c-1U].time)memcpy(t,m->pos_keys[base+c-1U].position,sizeof(ms3d_fx)*3U);
        else for(k=0;k+1U<c;++k){
            ta=&m->pos_keys[base+k];tb=&m->pos_keys[base+k+1U];
            if(time>=ta->time && time<=tb->time){
                dt=ms3d_fx_sub_sat(tb->time,ta->time);
                alpha=dt>0L?ms3d_fx_div(ms3d_fx_sub_sat(time,ta->time),dt):0L;
                {unsigned a;for(a=0;a<3;++a)t[a]=ms3d_fx_lerp(ta->position[a],tb->position[a],alpha);}
                break;
            }
        }
    }
}
/* Sample authored Euler keys as endpoint quaternions before interpolation.
 * Interpolating Euler components directly can bend across two-axis moves.
 * This is shortest-hemisphere NLERP, not exact SLERP. */
static void ms3d_anim_sample_quat(const ms3d_model *m, const ms3d_joint *j,
                                  ms3d_fx time, int rotation_mode, ms3d_quat *out)
{
    unsigned base,c,k;
    ms3d_fx dt,alpha;
    const ms3d_key_rot *a,*b;
    ms3d_quat qa,qb;
    ms3d_fx zero[3]={0L,0L,0L};
    base=j->rot_key_start;c=j->rot_key_count;
    if(c==0U){ms3d_quat_from_euler(zero,out);return;}
    if(time<=m->rot_keys[base].time){
        ms3d_quat_from_euler(m->rot_keys[base].rotation,out);return;
    }
    if(time>=m->rot_keys[base+c-1U].time){
        ms3d_quat_from_euler(m->rot_keys[base+c-1U].rotation,out);return;
    }
    for(k=0U;k+1U<c;++k){
        a=&m->rot_keys[base+k];b=&m->rot_keys[base+k+1U];
        if(time>=a->time&&time<=b->time){
            dt=ms3d_fx_sub_sat(b->time,a->time);
            alpha=dt>0L?ms3d_fx_div(ms3d_fx_sub_sat(time,a->time),dt):0L;
            ms3d_quat_from_euler(a->rotation,&qa);
            ms3d_quat_from_euler(b->rotation,&qb);
            if(rotation_mode==MS3D_ROTATION_QUAT_SLERP)
                ms3d_quat_slerp(&qa,&qb,alpha,out);
            else ms3d_quat_nlerp(&qa,&qb,alpha,out);
            return;
        }
    }
    ms3d_quat_from_euler(zero,out);
}
int ms3d_skin_evaluate_mode(const ms3d_model *m,ms3d_skin_pose *p,ms3d_fx time,int rotation_mode)
{
    ms3d_affine reference,anim,local;
    ms3d_fx r[3],t[3];
    ms3d_quat q;
    unsigned order,i;
    int parent;
    if(!m||!p)return MS3D_E_NULL;
    if(p->joint_count!=m->num_joints)return MS3D_E_BAD_COUNT;
    if(rotation_mode!=MS3D_ROTATION_EULER && rotation_mode!=MS3D_ROTATION_QUAT_NLERP &&
       rotation_mode!=MS3D_ROTATION_QUAT_SLERP)
        return MS3D_E_BAD_COUNT;
    for(order=0;order<p->joint_count;++order){
        i=p->joint_order[order];
        parent=m->joints[i].parent_index;
        ms3d_rotation(&reference,m->joints[i].rotation,m->joints[i].position);
        ms3d_anim_sample(m,&m->joints[i],time,r,t);
        if(rotation_mode!=MS3D_ROTATION_EULER){
            ms3d_anim_sample_quat(m,&m->joints[i],time,rotation_mode,&q);
            ms3d_quat_to_affine(&q,&anim);
            anim.m[0][3]=t[0];anim.m[1][3]=t[1];anim.m[2][3]=t[2];
        }else ms3d_rotation(&anim,r,t);
        ms3d_affine_mul(&reference,&anim,&local);
        if(parent>=0)ms3d_affine_mul(&p->global[parent],&local,&p->global[i]);
        else p->global[i]=local;
        ms3d_affine_mul(&p->global[i],&p->bind_inverse[i],&p->skin[i]);
    }
    p->time=time;
    return MS3D_OK;
}

int ms3d_skin_evaluate_blend(const ms3d_model *m,ms3d_skin_pose *p,
    ms3d_fx time_a,ms3d_fx time_b,ms3d_fx blend,int rotation_mode)
{
    ms3d_affine reference,anim,local;
    ms3d_fx ra[3],rb[3],ta[3],tb[3],t[3];
    ms3d_quat qa,qb,q;
    unsigned order,i,k;
    int parent;
    if(!m||!p)return MS3D_E_NULL;
    if(p->joint_count!=m->num_joints)return MS3D_E_BAD_COUNT;
    if(rotation_mode!=MS3D_ROTATION_EULER &&
       rotation_mode!=MS3D_ROTATION_QUAT_NLERP &&
       rotation_mode!=MS3D_ROTATION_QUAT_SLERP)return MS3D_E_BAD_COUNT;
    if(blend<=0L)return ms3d_skin_evaluate_mode(m,p,time_a,rotation_mode);
    if(blend>=MS3D_FX_ONE)return ms3d_skin_evaluate_mode(m,p,time_b,rotation_mode);
    for(order=0U;order<p->joint_count;++order){
        i=p->joint_order[order];
        parent=m->joints[i].parent_index;
        ms3d_rotation(&reference,m->joints[i].rotation,m->joints[i].position);
        ms3d_anim_sample(m,&m->joints[i],time_a,ra,ta);
        ms3d_anim_sample(m,&m->joints[i],time_b,rb,tb);
        for(k=0U;k<3U;++k)t[k]=ms3d_fx_lerp(ta[k],tb[k],blend);
        if(rotation_mode==MS3D_ROTATION_EULER){
            ms3d_quat_from_euler(ra,&qa);
            ms3d_quat_from_euler(rb,&qb);
        } else {
            ms3d_anim_sample_quat(m,&m->joints[i],time_a,rotation_mode,&qa);
            ms3d_anim_sample_quat(m,&m->joints[i],time_b,rotation_mode,&qb);
        }
        if(rotation_mode==MS3D_ROTATION_QUAT_SLERP)
            ms3d_quat_slerp(&qa,&qb,blend,&q);
        else ms3d_quat_nlerp(&qa,&qb,blend,&q);
        ms3d_quat_to_affine(&q,&anim);
        for(k=0U;k<3U;++k)anim.m[k][3]=t[k];
        ms3d_affine_mul(&reference,&anim,&local);
        if(parent>=0)ms3d_affine_mul(&p->global[parent],&local,&p->global[i]);
        else p->global[i]=local;
        ms3d_affine_mul(&p->global[i],&p->bind_inverse[i],&p->skin[i]);
    }
    p->time=time_a;
    return MS3D_OK;
}

int ms3d_skin_evaluate(const ms3d_model *m,ms3d_skin_pose *p,ms3d_fx time)
{
    return ms3d_skin_evaluate_mode(m,p,time,MS3D_ROTATION_EULER);
}

static void weighted_transform(const ms3d_model *m,const ms3d_skin_pose *p,
                               ms3d_u16 idx,const ms3d_fx vec[3],
                               ms3d_fx out[3],int is_normal)
{
    ms3d_influence inf[4];
    ms3d_u8 count;
    unsigned k, axis;
    ms3d_fx tmp[3],w,total;
    ms3d_fx accu[3]={0L,0L,0L};
    ms3d_fx used=0L;
    if(ms3d_vertex_get_influences(m,idx,inf,&count)!=MS3D_OK){
        memcpy(out,vec,3U*sizeof(ms3d_fx));return;
    }
    for(k=0;k<count;++k){
        if(inf[k].bone_id<0 || inf[k].bone_id>=(int)p->joint_count || inf[k].weight<=0L)continue;
        if(is_normal)ms3d_affine_direction(&p->skin[inf[k].bone_id],vec,tmp);
        else ms3d_affine_point(&p->skin[inf[k].bone_id],vec,tmp);
        w=inf[k].weight;used=ms3d_fx_add_sat(used,w);
        for(axis=0;axis<3;++axis)
            accu[axis]=ms3d_fx_add_sat(accu[axis],ms3d_fx_mul(tmp[axis],w));
    }
    /* Invalid/unused bone slots must not move vertices toward origin. */
    if(used==0L)memcpy(out,vec,3U*sizeof(ms3d_fx));
    else {
        total=used;
        for(axis=0;axis<3;++axis)out[axis]=ms3d_fx_div(accu[axis],total);
    }
}
int ms3d_skin_positions(const ms3d_model *m,const ms3d_skin_pose *p,ms3d_skin_vertices *out)
{
    unsigned i;
    if(!m||!p||!out)return MS3D_E_NULL;
    if(p->joint_count!=m->num_joints || m->num_vertices>MS3D_MAX_VERTICES)return MS3D_E_BAD_COUNT;
    for(i=0;i<m->num_vertices;++i)
        weighted_transform(m,p,(ms3d_u16)i,m->vertices[i].position,out->xyz[i],0);
    return MS3D_OK;
}

/* Normalize with fixed-point sqrt approximation; no 64-bit integers.
 * 3D normal values are -1..1; 3 squares after /8 fit 32-bit.
 */
static unsigned long isqrt32(unsigned long v)
{
    unsigned long bit=1UL<<30,root=0UL;
    while(bit>v)bit>>=2;
    while(bit){if(v>=root+bit){v-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    return root;
}
static void normalize(ms3d_fx v[3])
{
    long x[3],maxmag=0L,mag;
    unsigned long ss,r;
    int i;
    for(i=0;i<3;++i) {x[i]=v[i];if(x[i]>maxmag)maxmag=x[i];if(-x[i]>maxmag)maxmag=-x[i];}
    if(maxmag==0L)return;
    /* Scale max abs component close to 8192 for sqrt precision. */
    for(i=0;i<3;++i) x[i]=ms3d_fx_div(v[i],maxmag)*8192L/65536L;
    ss=(unsigned long)(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);r=isqrt32(ss);
    if(!r)return;
    mag=(long)r;
    for(i=0;i<3;++i)v[i]=ms3d_fx_div(x[i],mag);
}
int ms3d_skin_normal(const ms3d_model *m,const ms3d_skin_pose *p,ms3d_u16 v,
                     const ms3d_fx normal[3],ms3d_fx out[3])
{
    if(!m||!p||!normal||!out)return MS3D_E_NULL;
    if(v>=m->num_vertices)return MS3D_E_BAD_INDEX;
    weighted_transform(m,p,v,normal,out,1);
    normalize(out);
    return MS3D_OK;
}
int ms3d_emit_skinned_corners(const ms3d_model *m,const ms3d_skin_pose *p,
                              const ms3d_skin_vertices *verts,const ms3d_corner_normals *normals,
                              const ms3d_shaded_sink *sink)
{
    unsigned i,j,k,vi;
    ms3d_draw_corner dc;
    const ms3d_triangle *tri;
    int rc;
    if(!m||!p||!verts||!normals||!sink||!sink->corner)return MS3D_E_NULL;
    for(i=0;i<m->num_triangles;++i){
        tri=&m->triangles[i];
        for(j=0;j<3;++j){
            vi=tri->vertex_indices[j];
            if(vi>=m->num_vertices)return MS3D_E_BAD_INDEX;
            for(k=0;k<3;++k)dc.position[k]=verts->xyz[vi][k];
            rc=ms3d_skin_normal(m,p,(ms3d_u16)vi,normals->xyz[i][j],dc.normal);
            if(rc!=MS3D_OK)return rc;
            dc.uv[0]=tri->s[j];dc.uv[1]=tri->t[j];
            dc.source_vertex=(ms3d_u16)vi;
            dc.smoothing_group=tri->smoothing_group;
            dc.group_index=tri->group_index;
            dc.material_index=tri->group_index<m->num_groups ? m->groups[tri->group_index].material_index : -1;
            rc=sink->corner(sink->user,(ms3d_u16)i,(ms3d_u8)j,&dc);
            if(rc!=MS3D_OK)return rc;
        }
    }
    return MS3D_OK;
}
