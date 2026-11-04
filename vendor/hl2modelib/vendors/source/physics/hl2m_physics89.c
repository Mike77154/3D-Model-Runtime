/* Source PHY -> metadata + portable 16.16 AABB / ground-body runtime.
   NOT Valve VPhysics (no convex polygon extraction, rigid body contacts or joints).
   All state caller-owned, deterministic, no heap or float. */
#include "hl2m_master89.h"
#include <limits.h>
#include <string.h>
#define PH89_MAX (128 * HL2M_FIXED_ONE)
static int ph89_add(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *out)
{
    if(b>0 && a>INT_MAX-b)return HL2M_ERR_RANGE;
    if(b<0 && a<INT_MIN-b)return HL2M_ERR_RANGE;
    *out=a+b;return HL2M_OK;
}
/* Multiply nonnegative 16.16 a,b, each <= 128 and <= 1 respectively.
   Compute a* b / 65536 without 64-bit integers. */
static hl2m_fixed ph89_mul(hl2m_fixed a,hl2m_fixed b)
{
    int s=1,wh;unsigned int x,y,fr;
    if(a<0){s=-s;x=(unsigned int)(-(a+1))+1u;}else x=(unsigned int)a;
    if(b<0){s=-s;y=(unsigned int)(-(b+1))+1u;}else y=(unsigned int)b;
    wh=(int)(x>>16);
    fr=(x&65535u)*(y>>8);
    fr=(fr>>8)+(((x&65535u)*(y&255u))>>16);
    return (hl2m_fixed)(s*((hl2m_fixed)((unsigned int)wh*y+fr)));
}
int hl2m_physics_from_phy89(const hl2m_phy *phy,hl2m_phy_summary89 *out)
{
    int i;
    if(!phy || !out)return HL2M_ERR_NULL;
    if(phy->solid_count<0 || phy->solid_count>HL2M_MAX_PHY_SOLIDS)return HL2M_ERR_RANGE;
    memset(out,0,sizeof(*out));
    out->source_solid_count=phy->source_solid_count;
    out->parsed_solid_count=phy->solid_count;
    out->checksum=phy->checksum;
    for(i=0;i<phy->solid_count;i++){
        if(phy->solids[i].legacy)out->legacy_solids++;
        else out->compact_solids++;
    }
    return HL2M_OK;
}
int hl2m_physics_step89(hl2m_physics_body89 *body,hl2m_fixed dt,
    hl2m_fixed gravity_z,hl2m_fixed ground_z)
{
    hl2m_physics_body89 b;hl2m_fixed new_v,new_pos,bottom;
    int r;
    if(!body)return HL2M_ERR_NULL;
    if(dt<0 || dt>HL2M_FIXED_ONE || gravity_z>PH89_MAX || gravity_z<-PH89_MAX)return HL2M_ERR_RANGE;
    if(body->velocity.x>PH89_MAX || body->velocity.x<-PH89_MAX ||
       body->velocity.y>PH89_MAX || body->velocity.y<-PH89_MAX ||
       body->velocity.z>PH89_MAX || body->velocity.z<-PH89_MAX)return HL2M_ERR_RANGE;
    if(body->half_extent.x<0 || body->half_extent.y<0 || body->half_extent.z<0)return HL2M_ERR_RANGE;
    b=*body;
    r=ph89_add(b.velocity.z,ph89_mul(gravity_z,dt),&new_v);if(r)return r;
    if(new_v>PH89_MAX)new_v=PH89_MAX;
    if(new_v<-PH89_MAX)new_v=-PH89_MAX;
    b.velocity.z=new_v;
    r=ph89_add(b.center.x,ph89_mul(b.velocity.x,dt),&new_pos);if(r)return r;b.center.x=new_pos;
    r=ph89_add(b.center.y,ph89_mul(b.velocity.y,dt),&new_pos);if(r)return r;b.center.y=new_pos;
    r=ph89_add(b.center.z,ph89_mul(b.velocity.z,dt),&new_pos);if(r)return r;b.center.z=new_pos;
    r=ph89_add(b.center.z,-b.half_extent.z,&bottom);if(r)return r;
    b.grounded=0;
    if(bottom<=ground_z){
        r=ph89_add(ground_z,b.half_extent.z,&b.center.z);if(r)return r;
        if(b.velocity.z<0)b.velocity.z=0;
        b.grounded=1;
    }
    *body=b;return HL2M_OK;
}
int hl2m_physics_overlap89(const hl2m_physics_body89 *a,
    const hl2m_physics_body89 *b,int *overlap)
{
    int axis;hl2m_fixed a0[3],b0[3],ah[3],bh[3];
    if(!a || !b || !overlap)return HL2M_ERR_NULL;
    a0[0]=a->center.x;a0[1]=a->center.y;a0[2]=a->center.z;
    b0[0]=b->center.x;b0[1]=b->center.y;b0[2]=b->center.z;
    ah[0]=a->half_extent.x;ah[1]=a->half_extent.y;ah[2]=a->half_extent.z;
    bh[0]=b->half_extent.x;bh[1]=b->half_extent.y;bh[2]=b->half_extent.z;
    *overlap=1;
    for(axis=0;axis<3;axis++){
        hl2m_fixed alo,ahi,blo,bhi;
        if(ah[axis]<0 || bh[axis]<0)return HL2M_ERR_RANGE;
        if(ph89_add(a0[axis],-ah[axis],&alo) || ph89_add(a0[axis],ah[axis],&ahi) ||
           ph89_add(b0[axis],-bh[axis],&blo) || ph89_add(b0[axis],bh[axis],&bhi))return HL2M_ERR_RANGE;
        if(ahi<blo || bhi<alo){*overlap=0;return HL2M_OK;}
    }
    return HL2M_OK;
}
