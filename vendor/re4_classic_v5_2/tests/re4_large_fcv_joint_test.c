/* FCV 8-bit bone IDs with three-digit PMD identifiers; fail closed. C89. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
static void named(re4_pmd_name *n,const char *p){n->name.ptr=p;n->name.len=(re4_size)strlen(p);}
int main(void)
{
  re4_pmd_model m;re4_pmd_name names[8];
  memset(&m,0,sizeof(m));memset(names,0,sizeof(names));
  m.skeleton_name_count=8U;m.skeleton_count=8U;m.skeleton_names=names;
  named(&names[0],"born__10");named(&names[1],"born__100");
  named(&names[2],"born__255_S01");named(&names[3],"born__256");
  named(&names[4],"born__1000");named(&names[5],"unverified__101");
  named(&names[6],"em2300_crow__101");named(&names[7],"em2900_bat__102_S04");
  if(re4_skel_joint_for_fcv_id(&m,10)!=0 || re4_skel_joint_for_fcv_id(&m,100)!=1 ||
     re4_skel_joint_for_fcv_id(&m,255)!=2 || re4_skel_joint_for_fcv_id(&m,101)!=6 ||
     re4_skel_joint_for_fcv_id(&m,102)!=7)return 1;
  named(&names[4],"born__10_extra");
  if(re4_skel_joint_for_fcv_id(&m,10)!=-1)return 2;
  named(&names[4],"born__1000");
  named(&names[7],"born__100");
  if(re4_skel_joint_for_fcv_id(&m,100)!=-1)return 3;
  puts("3-digit FCV bone IDs and ambiguity guards PASS");return 0;
}
