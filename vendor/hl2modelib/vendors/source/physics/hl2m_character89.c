/* Non-VPhysics, fixed 16.16 character grounding and contact capture.
   Source SDK 2013 CIKContext and game movement: independently maintain world
   root displacement and per-foot stationary contact target. Caller owns state. */
#include "hl2m_master89.h"
#include <limits.h>
static int ch6_abs(hl2m_fixed n){return n<0? -n:n;}
static int ch6_add(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *out)
{
    if(b>0 && a>INT_MAX-b)return HL2M_ERR_RANGE;
    if(b<0 && a<INT_MIN-b)return HL2M_ERR_RANGE;
    *out=a+b;return HL2M_OK;
}
int hl2m_physics_foot_lock89(hl2m_foot_lock89 *s,hl2m_vec3 foot,
    hl2m_fixed floor_z,hl2m_fixed stance_height,
    hl2m_fixed max_frame_motion,hl2m_vec3 *target,int *planted)
{
    int close,slow;
    if(!s||!target||!planted)return HL2M_ERR_NULL;
    if(stance_height<0 || max_frame_motion<0 ||
       stance_height>32*HL2M_FIXED_ONE || max_frame_motion>32*HL2M_FIXED_ONE)
        return HL2M_ERR_RANGE;
    if(foot.z>INT_MAX-stance_height || floor_z>INT_MAX-stance_height)return HL2M_ERR_RANGE;
    close=foot.z<=floor_z+stance_height && foot.z>=floor_z-stance_height;
    slow=(!s->initialized ||
        (ch6_abs((foot.x-s->last.x)/256)<=max_frame_motion/256 &&
         ch6_abs((foot.y-s->last.y)/256)<=max_frame_motion/256 &&
         ch6_abs((foot.z-s->last.z)/256)<=max_frame_motion/256));
    if(!close || !slow) s->planted=0;
    if(close && slow && !s->planted){
        s->anchor=foot;s->anchor.z=floor_z;s->planted=1;
    }
    s->last=foot;s->initialized=1;
    *target=s->planted?s->anchor:foot;*planted=s->planted;
    return HL2M_OK;
}
int hl2m_physics_character_move89(hl2m_physics_body89 *body,
    hl2m_vec3 motion,hl2m_fixed floor_z,hl2m_fixed max_step)
{
    hl2m_physics_body89 next;hl2m_fixed bottom,limit;
    if(!body)return HL2M_ERR_NULL;
    if(max_step<0 || max_step>128*HL2M_FIXED_ONE ||
       ch6_abs(motion.x)>256*HL2M_FIXED_ONE ||
       ch6_abs(motion.y)>256*HL2M_FIXED_ONE ||
       ch6_abs(motion.z)>256*HL2M_FIXED_ONE ||
       body->half_extent.z<0)return HL2M_ERR_RANGE;
    next=*body;
    if(ch6_add(body->center.x,motion.x,&next.center.x)||
       ch6_add(body->center.y,motion.y,&next.center.y)||
       ch6_add(body->center.z,motion.z,&next.center.z)||
       ch6_add(next.center.z,-next.half_extent.z,&bottom)||
       ch6_add(floor_z,next.half_extent.z,&limit))return HL2M_ERR_RANGE;
    if(bottom<floor_z){
        if(!body->grounded && floor_z-bottom>max_step && motion.z>0)
            return HL2M_ERR_RANGE;
        next.center.z=limit;next.grounded=1;
        if(next.velocity.z<0)next.velocity.z=0;
    }else if(bottom==floor_z)next.grounded=1;
    else next.grounded=0;
    *body=next;return HL2M_OK;
}
