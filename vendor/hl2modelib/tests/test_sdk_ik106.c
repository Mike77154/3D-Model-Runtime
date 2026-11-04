#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_pose_matrices base,out;
#define FP(x) ((x)*HL2M_FIXED_ONE)
static void ident(hl2m_mat3x4 *m){m->m[0][0]=FP(1);m->m[1][1]=FP(1);m->m[2][2]=FP(1);}
#define CHECK(a) do{if(!(a)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    hl2m_ik_chain89 c;hl2m_ik_chain_report89 r;
    hl2m_physics_body89 body;hl2m_foot_lock89 state;
    hl2m_vec3 t,v;int lock,res,i,j;
    memset(&base,0,sizeof(base));base.bone_count=4;
    base.parent[0]=-1;base.parent[1]=0;base.parent[2]=1;base.parent[3]=2;
    ident(&base.world[0]);ident(&base.world[1]);ident(&base.world[2]);ident(&base.world[3]);
    ident(&base.local[0]);ident(&base.local[1]);ident(&base.local[2]);ident(&base.local[3]);
    base.world[0].m[2][3]=FP(5);base.world[1].m[0][3]=FP(1);base.world[1].m[2][3]=FP(3);
    base.world[2].m[0][3]=FP(1);base.world[2].m[2][3]=FP(1);
    base.world[3].m[0][3]=FP(1);base.world[3].m[2][3]=FP(0);
    base.local[0].m[2][3]=FP(5);base.local[1].m[0][3]=FP(1);
    base.local[1].m[2][3]=-FP(2);base.local[2].m[2][3]=-FP(2);
    base.local[3].m[2][3]=-FP(1);
    memset(&c,0,sizeof(c));c.hip=0;c.knee=1;c.foot=2;c.weight=FP(1);
    c.goal.x=FP(1);c.goal.y=FP(1);c.goal.z=FP(2);
    res=hl2m_master_ik_two_bone89(&base,&c,&out,&r);
    printf("IK code=%d before=%d after=%d length=(%d,%d) knee=(%d,%d,%d) foot=(%d,%d,%d)\n",
        res,r.error_before,r.error_after,r.upper_length,r.lower_length,
        out.world[1].m[0][3],out.world[1].m[1][3],out.world[1].m[2][3],
        out.world[2].m[0][3],out.world[2].m[1][3],out.world[2].m[2][3]);
    CHECK(res==HL2M_OK);CHECK(r.error_before>FP(1));CHECK(r.error_after<FP(1)/4);
    CHECK(out.world[1].m[0][3]!=base.world[1].m[0][3] ||
          out.world[1].m[1][3]!=base.world[1].m[1][3]);
    CHECK(out.world[3].m[2][3]!=base.world[3].m[2][3]);
    for(i=0;i<3;i++)for(j=0;j<3;j++)
        CHECK(out.world[2].m[i][j]==base.world[2].m[i][j]); /* endpoint orientation */
    c.weight=0;CHECK(hl2m_master_ik_two_bone89(&base,&c,&out,&r)==HL2M_OK);
    CHECK(memcmp(&base,&out,sizeof(base))==0);
    memset(&state,0,sizeof(state));memset(&v,0,sizeof(v));
    CHECK(hl2m_master_foot_lock89(&state,v,0,FP(1)/2,FP(1),&t,&lock)==HL2M_OK && lock);
    v.x=FP(1)/8;
    CHECK(hl2m_master_foot_lock89(&state,v,0,FP(1)/2,FP(1),&t,&lock)==HL2M_OK && lock);
    CHECK(t.x==0);
    v.z=FP(3);
    CHECK(hl2m_master_foot_lock89(&state,v,0,FP(1)/2,FP(1),&t,&lock)==HL2M_OK && !lock);
    memset(&body,0,sizeof(body));body.center.z=FP(2);body.half_extent.z=FP(1);
    v.x=FP(3);v.y=0;v.z=-FP(2);
    CHECK(hl2m_master_character_move89(&body,v,0,FP(1))==HL2M_OK);
    CHECK(body.center.x==FP(3) && body.center.z==FP(1) && body.grounded);
    printf("C89 native 2-bone IK + persistent foot lock + kinematic collision: PASS\n");
    return 0;
}
