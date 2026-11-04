/* Optional real-asset numerical validation against T-103 native animation.
   Run: ./test_t103_ik_real106 /path/to/tyrantphase1.mdl */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 buf[16*1024*1024];
static hl2m_mdl mdl;
static hl2m_pose_matrices pose,corrected;
static int dist_q4(const hl2m_pose_matrices *p,int a,int b)
{
    int x=(p->world[a].m[0][3]-p->world[b].m[0][3])/4096;
    int y=(p->world[a].m[1][3]-p->world[b].m[1][3])/4096;
    int z=(p->world[a].m[2][3]-p->world[b].m[2][3])/4096;
    int n=x*x+y*y+z*z,lo=0,hi=20000,mid;
    while(lo+1<hi){mid=(lo+hi)/2;if(mid*mid<=n)lo=mid;else hi=mid;}
    return lo;
}
static int idx(const char *name){int i;for(i=0;i<mdl.bone_count;i++)if(!strcmp(name,mdl.bones[i].name))return i;return -1;}
int main(int argc,char **argv)
{
    FILE *f;unsigned int size;int fixed=0,r,frame,walk=-1,hip,knee,foot;
    int successes=0,improvements=0,clamped=0,max_before=0,max_after=0,failed=0;
    int side,left[3],right[3],l1_before,l2_before,l1_after,l2_after;
    hl2m_ik_chain89 chain;hl2m_ik_chain_report89 report;
    if(argc<2){puts("provide real T-103 MDL corpus path");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    size=(unsigned int)fread(buf,1,sizeof(buf),f);fclose(f);
    r=hl2m_repair_missing_first_byte(buf,&size,sizeof(buf),HL2M_FORMAT_MDL,&fixed);
    if(r || hl2m_master_decode89(HL2M_FORMAT_MDL,buf,size,0,0,&mdl))return 3;
    left[0]=idx("L_momo");left[1]=idx("L_hiza");left[2]=idx("L_ashi");
    right[0]=idx("R_momo");right[1]=idx("R_hiza");right[2]=idx("R_ashi");
    if(left[0]<0 || right[0]<0){
        left[0]=idx("_22");left[1]=idx("_23");left[2]=idx("_24");
        right[0]=idx("_26");right[1]=idx("_27");right[2]=idx("_28");
    }
    if(left[0]<0 || left[1]<0 || left[2]<0 || right[0]<0 || right[1]<0 || right[2]<0){
        puts("skip model without recognized leg chains");return 0;
    }
    for(frame=0;frame<mdl.anim_count;frame++)if(!strcmp(mdl.anims[frame].name,"Walk_F"))walk=frame;
    if(walk<0)return 4;
    memset(&chain,0,sizeof(chain));chain.weight=HL2M_FIXED_ONE;
    for(frame=0;frame<mdl.anims[walk].numframes;frame++){
        r=hl2m_master_anim89(buf,size,&mdl,walk,frame,&pose,0);if(r){failed++;continue;}
        for(side=0;side<2;side++){
            hip=(side==0?left:right)[0];
            knee=(side==0?left:right)[1];
            foot=(side==0?left:right)[2];
            chain.hip=hip;chain.knee=knee;chain.foot=foot;
            chain.goal.x=pose.world[foot].m[0][3]+HL2M_FIXED_ONE;
            chain.goal.y=pose.world[foot].m[1][3];
            chain.goal.z=pose.world[foot].m[2][3]+HL2M_FIXED_ONE;
            l1_before=dist_q4(&pose,hip,knee);l2_before=dist_q4(&pose,knee,foot);
            r=hl2m_master_ik_two_bone89(&pose,&chain,&corrected,&report);
            if(r){failed++;continue;}
            successes++;
            if(report.error_after<report.error_before)improvements++;
            if(report.reach_clamped)clamped++;
            if(report.error_after>max_after)max_after=report.error_after;
            if(report.error_before>max_before)max_before=report.error_before;
            l1_after=dist_q4(&corrected,hip,knee);l2_after=dist_q4(&corrected,knee,foot);
            if(l1_before-l1_after>20 || l1_after-l1_before>20 ||
               l2_before-l2_after>20 || l2_after-l2_before>20){
                printf("FAIL distorted limb frame=%d side=%d lengths %d:%d %d:%d\n",
                    frame,side,l1_before,l1_after,l2_before,l2_after);return 9;
            }
        }
    }
    printf("T103 animation=%s bones=%d tested=%d better=%d clamped=%d failed=%d max_error_before_q16=%d max_error_after_q16=%d\n",
        mdl.anims[walk].name,mdl.bone_count,successes,improvements,clamped,failed,max_before,max_after);
    if(successes<16 || improvements<successes*3/4 || failed)return 8;
    puts("T103 real pose two-bone IK: PASS");return 0;
}
