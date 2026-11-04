#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"rigid anchor test line %d\n",__LINE__);return 1;}}while(0)
static void ident(re4_skel_matrix *m,re4_i32 tx)
{memset(m,0,sizeof(*m));m->m[0]=65536;m->m[5]=65536;m->m[10]=65536;m->m[3]=tx;}
int main(void)
{
 re4_pmd_model master,part;re4_character_links links;
 re4_skel_pose mr,ma,pr,pa;re4_skel_matrix a[2],b[2],c[2],d[2];
 memset(&master,0,sizeof(master));memset(&part,0,sizeof(part));
 master.skeleton_count=2U;part.skeleton_count=2U;
 CK(re4_character_rigid_anchor(&master,&part,1U,&links)==RE4_OK);
 CK(links.direct_links==0U && links.inherited_links==2U);
 CK(links.master_index[0]==1 && links.master_index[1]==1);
 CK(re4_character_rigid_anchor(&master,&part,2U,&links)==RE4_ERR_RANGE);
 ident(&a[0],0);ident(&a[1],65536);
 ident(&b[0],0);ident(&b[1],196608);
 ident(&c[0],32768);ident(&c[1],-32768);
 mr.world=a;mr.bone_count=2;ma.world=b;ma.bone_count=2;
 pr.world=c;pr.bone_count=2;pa.world=d;
 CK(re4_character_rigid_anchor(&master,&part,1U,&links)==RE4_OK);
 CK(re4_character_retarget(&master,&mr,&mr,&part,&pr,&links,&pa,2U)==RE4_OK);
 CK(d[0].m[3]==32768 && d[1].m[3]==-32768);
 CK(re4_character_retarget(&master,&mr,&ma,&part,&pr,&links,&pa,2U)==RE4_OK);
 CK(d[0].m[3]==163840 && d[1].m[3]==98304);
 puts("explicit rigid attachment retarget PASS");return 0;
}
