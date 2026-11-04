#include "c4d89.h"
#include <stdio.h>
#include <string.h>
/* All test fixtures are constructed from exact integer fixed point values;
   no Cinema 4D native skeleton fidelity is implied by these synthetic tests. */
static void identity(long m[12])
{
    unsigned int i;for(i=0u;i<12u;i++)m[i]=0L;
    m[3]=m[7]=m[11]=C4D89_QONE;
}
static int fails=0;
static void check(int flag,const char *desc)
{
    if(!flag){fprintf(stderr,"FAIL: %s\n",desc);fails++;}
}
int main(void)
{
    long mats[24],xyz[3]={C4D89_QONE,2L*C4D89_QONE,3L*C4D89_QONE};
    long out[3],q;C4D89_SkinVertex w;int rc;
    memset(&w,0,sizeof(w));
    identity(mats);identity(mats+12);
    check(c4d89_fixed_mul(C4D89_QONE/2L,C4D89_QONE/2L,&q)==C4D89_OK&&q==16384L,"0.5*0.5");
    check(c4d89_fixed_mul(-C4D89_QONE,C4D89_QONE/2L,&q)==C4D89_OK&&q==-32768L,"negative * weight");
    check(c4d89_fixed_mul(2147483647L,2147483647L,&q)==C4D89_ERR_LIMIT,"overflow rejected");
    w.count=1u;w.influences[0].joint=0u;w.influences[0].weight_q16=C4D89_QONE;
    rc=c4d89_skin_point(xyz,&w,mats,2u,out);
    check(rc==C4D89_OK&&out[0]==xyz[0]&&out[1]==xyz[1]&&out[2]==xyz[2],"identity no deformation");
    mats[12]=2L*C4D89_QONE; /* joint1 translates +2 on X */
    w.count=2u;w.influences[0].weight_q16=C4D89_QONE/2L;
    w.influences[1].joint=1u;w.influences[1].weight_q16=C4D89_QONE/2L;
    rc=c4d89_skin_point(xyz,&w,mats,2u,out);
    check(rc==C4D89_OK&&out[0]==2L*C4D89_QONE&&out[1]==2L*C4D89_QONE&&out[2]==3L*C4D89_QONE,"blended 2-joint translation");
    w.influences[1].weight_q16=C4D89_QONE/4L;
    check(c4d89_skin_point(xyz,&w,mats,2u,out)==C4D89_ERR_CORRUPT,"reject unnormalized weights");
    w.influences[1].weight_q16=C4D89_QONE/2L;
    w.influences[1].joint=2u;
    check(c4d89_skin_point(xyz,&w,mats,2u,out)==C4D89_ERR_CORRUPT,"reject invalid bone index");
    w.influences[1].joint=1u;
    identity(mats+12);
    mats[12+3]=0L;mats[12+4]=C4D89_QONE; /* +90 deg around Z */
    mats[12+6]=-C4D89_QONE;mats[12+7]=0L;
    w.count=1u;w.influences[0].joint=1u;w.influences[0].weight_q16=C4D89_QONE;
    rc=c4d89_skin_point(xyz,&w,mats,2u,out);
    check(rc==C4D89_OK&&out[0]==-2L*C4D89_QONE&&out[1]==C4D89_QONE&&out[2]==3L*C4D89_QONE,"90 degree Z rotation");
    if(fails){fprintf(stderr,"Skin tests failed: %d\n",fails);return 1;}
    puts("All 7 strict-C89 skin/fixed-point tests passed");return 0;
}
