/* v4.8: born1__ is a valid RE4 enemy MAIN rig (em3c armors).
   The mapper and FCV decoder must agree and reject duplicate bone IDs.
   Pure C89; synthetic fixtures, no copyright game data. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do { if(!(x)){fprintf(stderr,"born1 rig test failed line %d\n",__LINE__);return 1;} }while(0)
static void named(re4_pmd_name *n,const char *s){n->name.ptr=s;n->name.len=(re4_size)strlen(s);}
int main(void)
{
    re4_pmd_model m,p;re4_pmd_name mn[5],pn[4];
    re4_pmd_skeleton mb[5],pb[4];re4_character_links links;
    re4_character_link_audit q;
    memset(&m,0,sizeof(m));memset(&p,0,sizeof(p));
    memset(mb,0,sizeof(mb));memset(pb,0,sizeof(pb));
    named(&mn[0],"Scene_Root");named(&mn[1],"born1__00");
    named(&mn[2],"born1__01");named(&mn[3],"born1__10_S16");
    named(&mn[4],"em3c00_attachment");
    named(&pn[0],"Scene_Root");named(&pn[1],"born1__00");
    named(&pn[2],"born1__01");named(&pn[3],"born1__10_S16");
    mb[0].parent=-1;mb[1].parent=0;mb[2].parent=1;mb[3].parent=2;mb[4].parent=1;
    pb[0].parent=-1;pb[1].parent=0;pb[2].parent=1;pb[3].parent=2;
    m.skeletons=mb;m.skeleton_count=5;m.skeleton_names=mn;m.skeleton_name_count=5;
    p.skeletons=pb;p.skeleton_count=4;p.skeleton_names=pn;p.skeleton_name_count=4;
    CK(re4_skel_joint_for_fcv_id(&m,0)==1);
    CK(re4_skel_joint_for_fcv_id(&m,10)==3);
    CK(re4_character_link(&m,&p,&links)==RE4_OK);
    CK(links.direct_links==3U && links.stationary_links==1U);
    CK(links.master_index[1]==1 && links.master_index[2]==2 && links.master_index[3]==3);
    CK(re4_character_audit(&m,&p,&links,&q)==RE4_OK);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    /* Alternative master prefixes must never silently collide. */
    named(&mn[4],"born__10");
    CK(re4_skel_joint_for_fcv_id(&m,10)==-1);
    CK(re4_character_link(&m,&p,&links)==RE4_ERR_BAD_FORMAT);
    /* Reject malformed numeric suffix, not a prefix-only match. */
    named(&mn[4],"born__10X");
    CK(re4_skel_joint_for_fcv_id(&m,10)==3);
    CK(re4_character_link(&m,&p,&links)==RE4_OK);
    CK(links.master_index[3]==3);
    puts("born1 master rig / FCV / ambiguity guards PASS");
    return 0;
}
