/* Validate bird/bat FCV joint aliases and deny accidental prefix/ID matches. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
static void setname(re4_pmd_name *n,const char *s)
{
 n->name.ptr=s;n->name.len=strlen(s);
}
int main(void)
{
 re4_pmd_model m;re4_pmd_name n[6];
 memset(&m,0,sizeof(m));memset(n,0,sizeof(n));m.skeleton_name_count=6;m.skeleton_count=6;m.skeleton_names=n;
 setname(&n[0],"Scene_Root");setname(&n[1],"em2300_crow__00");
 setname(&n[2],"em2300_crow__02");setname(&n[3],"em2900_bat__01_S03");
 setname(&n[4],"born__10_S02");setname(&n[5],"born__100");
 if(re4_skel_joint_for_fcv_id(&m,0)!=1 ||
    re4_skel_joint_for_fcv_id(&m,2)!=2 ||
    re4_skel_joint_for_fcv_id(&m,1)!=3 ||
    re4_skel_joint_for_fcv_id(&m,10)!=4 ||
    re4_skel_joint_for_fcv_id(&m,3)!=-1)return 1;
 /* Two equal IDs must never silently pick the first one. */
 setname(&n[5],"em2300_crow__02");
 if(re4_skel_joint_for_fcv_id(&m,2)!=-1)return 2;
 setname(&n[5],"enemy__02");
 if(re4_skel_joint_for_fcv_id(&m,2)!=2)return 3;
 puts("bird/bat joint alias and fail-closed ambiguity PASS");return 0;
}
