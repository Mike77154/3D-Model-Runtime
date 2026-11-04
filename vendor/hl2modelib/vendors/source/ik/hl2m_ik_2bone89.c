/* SDK-derived architecture, independent fixed-point two-segment solver.
   Source SDK 2013 src/public/bone_setup.cpp Studio_SolveIK, Studio_AlignIKMatrix,
   CIKContext::SolveLock reference algorithm. Not copied from Valve source.
   NO float/double, malloc/free or 64-bit integer. Q16.16 pose; Q15 rotation.
   Restricted to legs of < 750 Source units, for safe 32-bit arithmetic. */
#include "hl2m_master89.h"
#include <string.h>
#include <limits.h>

typedef struct ik6_v_s { int x,y,z; } ik6_v;
typedef struct ik6_q_s { int x,y,z,w; } ik6_q;

static unsigned int ik6_sqrt(unsigned int n)
{
    unsigned int root=0u,bit=1u<<30;
    while(bit>n)bit>>=2;
    while(bit){
        if(n>=root+bit){n-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return root;
}
static int ik6_abs(int a){return a<0?-a:a;}
/* These scaled products remain below signed 32 bit for |a| <= 2^25, |b| <= 2^16.
   rot = Q15, position = Q16.16. */
static int ik6_mulq15(int a,int b){return (a/128)*(b/256);}
static int ik6_mul16(int a,int b){return (a/256)*(b/128);}
static ik6_v ik6_sub(ik6_v a,ik6_v b){ik6_v r;r.x=a.x-b.x;r.y=a.y-b.y;r.z=a.z-b.z;return r;}
static ik6_v ik6_from_hl(hl2m_vec3 a)
{ik6_v v;v.x=a.x;v.y=a.y;v.z=a.z;return v;}
static ik6_v ik6_vec(const hl2m_mat3x4 *m)
{ik6_v v;v.x=m->m[0][3];v.y=m->m[1][3];v.z=m->m[2][3];return v;}
static unsigned int ik6_length(ik6_v v)
{
    int x=v.x/4096,y=v.y/4096,z=v.z/4096;
    unsigned int sq;
    if(ik6_abs(x)>12000 || ik6_abs(y)>12000 || ik6_abs(z)>12000)return 0u;
    sq=(unsigned int)(x*x+y*y+z*z);
    return ik6_sqrt(sq); /* length in units of 1/16 Source unit */
}
static int ik6_unit(ik6_v v,ik6_v *out)
{
    unsigned int d=ik6_length(v);
    int x=v.x/4096,y=v.y/4096,z=v.z/4096;
    if(!d || d>12000u)return 0;
    out->x=(x*32768)/(int)d;
    out->y=(y*32768)/(int)d;
    out->z=(z*32768)/(int)d;
    if(out->x>32768)out->x=32768;
    if(out->x< -32768)out->x=-32768;
    if(out->y>32768)out->y=32768;
    if(out->y< -32768)out->y=-32768;
    if(out->z>32768)out->z=32768;
    if(out->z< -32768)out->z=-32768;
    return 1;
}
static int ik6_dot(ik6_v a,ik6_v b)
{return ik6_mulq15(a.x,b.x)+ik6_mulq15(a.y,b.y)+ik6_mulq15(a.z,b.z);}
static ik6_v ik6_cross(ik6_v a,ik6_v b)
{
    ik6_v v;
    v.x=ik6_mulq15(a.y,b.z)-ik6_mulq15(a.z,b.y);
    v.y=ik6_mulq15(a.z,b.x)-ik6_mulq15(a.x,b.z);
    v.z=ik6_mulq15(a.x,b.y)-ik6_mulq15(a.y,b.x);
    return v;
}
static int ik6_quat(ik6_v from,ik6_v to,ik6_q *q)
{
    ik6_v c=ik6_cross(from,to);
    unsigned int norm;
    int w=32768+ik6_dot(from,to),x=c.x/256,y=c.y/256,z=c.z/256,t=w/256;
    if(w<100 && ik6_abs(c.x)+ik6_abs(c.y)+ik6_abs(c.z)<100){
        ik6_v reference;
        reference.x=0;reference.y=0;reference.z=0;
        if(ik6_abs(from.z)<24000)reference.z=32768;
        else reference.y=32768;
        c=ik6_cross(from,reference);w=0;
        x=c.x/256;y=c.y/256;z=c.z/256;t=0;
    }
    norm=ik6_sqrt((unsigned int)(x*x+y*y+z*z+t*t));
    if(!norm)return 0;
    q->x=(c.x*128)/(int)norm;q->y=(c.y*128)/(int)norm;
    q->z=(c.z*128)/(int)norm;q->w=(w*128)/(int)norm;
    return 1;
}
/* Quaternion rotation of Q16.16 coordinate or basis column. */
static ik6_v ik6_rotate(ik6_q q,ik6_v v)
{
    ik6_v c,t,ct,r;ik6_v a;
    a.x=q.x;a.y=q.y;a.z=q.z;
    /* cross of Q15 with Q16.16 */
    c.x=ik6_mul16(a.y,v.z)-ik6_mul16(a.z,v.y);
    c.y=ik6_mul16(a.z,v.x)-ik6_mul16(a.x,v.z);
    c.z=ik6_mul16(a.x,v.y)-ik6_mul16(a.y,v.x);
    t.x=c.x*2;t.y=c.y*2;t.z=c.z*2;
    ct.x=ik6_mul16(a.y,t.z)-ik6_mul16(a.z,t.y);
    ct.y=ik6_mul16(a.z,t.x)-ik6_mul16(a.x,t.z);
    ct.z=ik6_mul16(a.x,t.y)-ik6_mul16(a.y,t.x);
    r.x=v.x+ik6_mul16(q.w,t.x)+ct.x;
    r.y=v.y+ik6_mul16(q.w,t.y)+ct.y;
    r.z=v.z+ik6_mul16(q.w,t.z)+ct.z;
    return r;
}
static int ik6_descendant(const hl2m_pose_matrices *p,int bone,int ancestor)
{
    int iter=0;
    while(bone>=0 && bone<p->bone_count && iter++<p->bone_count){
        if(bone==ancestor)return 1;
        bone=p->parent[bone];
    }
    return 0;
}
static int ik6_rotate_subtree(hl2m_pose_matrices *p,int joint,ik6_v old_v,ik6_v new_v)
{
    ik6_v a,b,origin,delta,rotated,col;ik6_q q;int i,j;
    if(!ik6_unit(old_v,&a)||!ik6_unit(new_v,&b)||!ik6_quat(a,b,&q))return 0;
    origin=ik6_vec(&p->world[joint]);
    /* original orientation and root unchanged, descendants rotate rigidly */
    for(i=0;i<p->bone_count;i++)if(ik6_descendant(p,i,joint)){
        delta=ik6_sub(ik6_vec(&p->world[i]),origin);
        rotated=ik6_rotate(q,delta);
        p->world[i].m[0][3]=origin.x+rotated.x;
        p->world[i].m[1][3]=origin.y+rotated.y;
        p->world[i].m[2][3]=origin.z+rotated.z;
        for(j=0;j<3;j++){
            col.x=p->world[i].m[0][j];col.y=p->world[i].m[1][j];col.z=p->world[i].m[2][j];
            rotated=ik6_rotate(q,col);
            p->world[i].m[0][j]=rotated.x;
            p->world[i].m[1][j]=rotated.y;
            p->world[i].m[2][j]=rotated.z;
        }
    }
    return 1;
}
static void ik6_matmul(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,hl2m_mat3x4 *out)
{
    hl2m_mat3x4 m;int r,c,k;
    memset(&m,0,sizeof(m));
    for(r=0;r<3;r++){
        for(c=0;c<3;c++)for(k=0;k<3;k++)
            m.m[r][c]+=ik6_mul16(a->m[r][k]/2,b->m[k][c]);
        m.m[r][3]=a->m[r][3];
        for(k=0;k<3;k++)m.m[r][3]+=ik6_mul16(a->m[r][k]/2,b->m[k][3]);
    }
    *out=m;
}
static int ik6_restore_tip(hl2m_pose_matrices *out,
    const hl2m_pose_matrices *base,int tip)
{
    int i,j,k,progress,pending=0,seen[HL2M_MAX_POSE_MATRICES];
    for(i=0;i<out->bone_count;i++)seen[i]=0;
    for(i=0;i<3;i++)for(j=0;j<3;j++)
        out->world[tip].m[i][j]=base->world[tip].m[i][j];
    seen[tip]=1;
    for(i=0;i<out->bone_count;i++)if(i!=tip && ik6_descendant(out,i,tip))pending++;
    for(k=0;k<out->bone_count && pending>0;k++){
        progress=0;
        for(i=0;i<out->bone_count;i++)if(!seen[i] && ik6_descendant(out,i,tip)){
            j=out->parent[i];
            if(j>=0 && seen[j]){
                ik6_matmul(&out->world[j],&out->local[i],&out->world[i]);
                seen[i]=1;pending--;progress++;
            }
        }
        if(!progress)break;
    }
    return pending==0;
}
static int ik6_local(hl2m_pose_matrices *p,int joint)
{
    hl2m_mat3x4 old,newm,*world=&p->world[joint];
    int parent=p->parent[joint],r,c,k;
    if(parent<0){p->local[joint]=*world;return 1;}
    if(parent>=p->bone_count)return 0;
    old=p->world[parent];memset(&newm,0,sizeof(newm));
    /* rigid inverse parent rotation (transpose) */
    for(r=0;r<3;r++){
        for(c=0;c<3;c++)for(k=0;k<3;k++)
            newm.m[r][c]+=ik6_mul16(old.m[k][r]/2,world->m[k][c]);
        for(k=0;k<3;k++)
            newm.m[r][3]+=ik6_mul16(old.m[k][r]/2,
                                    world->m[k][3]-old.m[k][3]);
    }
    p->local[joint]=newm;
    return 1;
}
int hl2m_ik_two_bone89(const hl2m_pose_matrices *pose,
    const hl2m_ik_chain89 *chain,hl2m_pose_matrices *out,
    hl2m_ik_chain_report89 *report)
{
    ik6_v hip,knee,foot,goal,dir,perp,knee_goal,new_upper,old_upper;
    ik6_v old_shin,new_shin;int a,b,c,proj,h,sq,d,upper,lower,
        reach,clamped=0,i;
    hl2m_ik_chain_report89 rep;
    if(!pose||!chain||!out)return HL2M_ERR_NULL;
    a=chain->hip;b=chain->knee;c=chain->foot;
    if(pose->bone_count<1 || pose->bone_count>HL2M_MAX_POSE_MATRICES ||
       a<0||b<0||c<0||a>=pose->bone_count||b>=pose->bone_count||c>=pose->bone_count||
       a==b||b==c||a==c||pose->parent[b]!=a||pose->parent[c]!=b||
       chain->weight<0||chain->weight>HL2M_FIXED_ONE)return HL2M_ERR_RANGE;
    hip=ik6_vec(&pose->world[a]);knee=ik6_vec(&pose->world[b]);foot=ik6_vec(&pose->world[c]);
    upper=(int)ik6_length(ik6_sub(knee,hip));
    lower=(int)ik6_length(ik6_sub(foot,knee));
    if(upper<1||lower<1||upper>11000||lower>11000)return HL2M_ERR_RANGE;
    goal.x=foot.x+ik6_mul16(chain->weight/2,chain->goal.x-foot.x);
    goal.y=foot.y+ik6_mul16(chain->weight/2,chain->goal.y-foot.y);
    goal.z=foot.z+ik6_mul16(chain->weight/2,chain->goal.z-foot.z);
    d=(int)ik6_length(ik6_sub(goal,hip));
    if(d<1)return HL2M_ERR_RANGE;
    if(!ik6_unit(ik6_sub(goal,hip),&dir))return HL2M_ERR_RANGE;
    reach=upper+lower-1;
    if(d>reach){d=reach;clamped=1;}
    if(d<ik6_abs(upper-lower)+1){d=ik6_abs(upper-lower)+1;clamped=1;}
    if(d<=0)return HL2M_ERR_RANGE;
    /* Q4 length law of cosines, controlled Source-sized bones. */
    proj=(upper*upper - lower*lower + d*d)/(2*d);
    if(proj>upper)proj=upper;
    if(proj< -upper)proj=-upper;
    sq=upper*upper-proj*proj;if(sq<0)sq=0;
    h=(int)ik6_sqrt((unsigned int)sq);
    old_upper=ik6_sub(knee,hip);
    i=((old_upper.x/4096)*dir.x/32768+
       (old_upper.y/4096)*dir.y/32768+
       (old_upper.z/4096)*dir.z/32768);
    perp.x=old_upper.x/4096-(i*dir.x/32768);
    perp.y=old_upper.y/4096-(i*dir.y/32768);
    perp.z=old_upper.z/4096-(i*dir.z/32768);
    if(ik6_abs(perp.x)+ik6_abs(perp.y)+ik6_abs(perp.z)<3){
        ik6_v axis;axis.x=0;axis.y=0;axis.z=0;
        if(ik6_abs(dir.z)<24000)axis.z=32768;
        else axis.x=32768;
        perp=ik6_cross(dir,axis);
        /* perpendicular q15, convert to Q16.16 before unit */
        perp.x*=2;perp.y*=2;perp.z*=2;
    }else{perp.x*=4096;perp.y*=4096;perp.z*=4096;}
    if(!ik6_unit(perp,&perp))return HL2M_ERR_RANGE;
    knee_goal.x=hip.x+(dir.x*proj+perp.x*h)/8;
    knee_goal.y=hip.y+(dir.y*proj+perp.y*h)/8;
    knee_goal.z=hip.z+(dir.z*proj+perp.z*h)/8;
    /* Improve precision by multiplying normalized vectors directly using Q4 */
    new_upper=ik6_sub(knee_goal,hip);

    memset(&rep,0,sizeof(rep));rep.upper_length=upper*4096;rep.lower_length=lower*4096;
    rep.error_before=(hl2m_fixed)(ik6_length(ik6_sub(foot,ik6_from_hl(chain->goal)))*4096u);
    rep.reach_clamped=clamped;
    if(rep.error_before<=4096 && !clamped){
        *out=*pose;rep.error_after=rep.error_before;
        if(report)*report=rep;
        return HL2M_OK;
    }
    if(chain->weight==0){*out=*pose;rep.error_after=rep.error_before;
        if(report)*report=rep;
        return HL2M_OK;
    }
    /* Work on copy for alias-safe mutations. */
    *out=*pose;
    if(!ik6_rotate_subtree(out,a,old_upper,new_upper))return HL2M_ERR_PARSE;
    old_shin=ik6_sub(ik6_vec(&out->world[c]),ik6_vec(&out->world[b]));

    /* target at attainable distance, along hip-goal ray */
    foot.x=hip.x+(dir.x*d)/8;
    foot.y=hip.y+(dir.y*d)/8;
    foot.z=hip.z+(dir.z*d)/8;
    new_shin=ik6_sub(foot,ik6_vec(&out->world[b]));
    if(!ik6_rotate_subtree(out,b,old_shin,new_shin))return HL2M_ERR_PARSE;
    if(!ik6_restore_tip(out,pose,c))return HL2M_ERR_PARSE;
    if(!ik6_local(out,a)||!ik6_local(out,b)||!ik6_local(out,c))return HL2M_ERR_PARSE;
    rep.applied=1;
    rep.error_after=(hl2m_fixed)(ik6_length(ik6_sub(ik6_vec(&out->world[c]),ik6_from_hl(chain->goal)))*4096u);
    if(report)*report=rep;
    return HL2M_OK;
}
