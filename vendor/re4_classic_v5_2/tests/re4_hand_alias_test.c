/* Test observed Ada hand skeleton names and fail-closed unknown bone tags. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"hand alias FAIL %d\n",__LINE__);return 1;}}while(0)
static void put(re4_pmd_name *out,const char *s)
{out->name.ptr=s;out->name.len=(re4_size)strlen(s);}
int main(void)
{
    re4_pmd_model a,b;re4_pmd_name an[4],bn[5];
    re4_pmd_skeleton as[4],bs[5];re4_character_links l;
    re4_character_link_audit q;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
    memset(as,0,sizeof(as));memset(bs,0,sizeof(bs));
    put(&an[0],"born__00");put(&an[1],"born__09");
    put(&an[2],"born__15");put(&an[3],"born__16");
    put(&bn[0],"Scene_Root");put(&bn[1],"LHAND__15_S09");
    put(&bn[2],"LHAND__16_S10");put(&bn[3],"RHAND__09_S15");
    put(&bn[4],"RHAND__XX_INVALID");
    as[0].parent=-1;as[1].parent=0;as[2].parent=0;as[3].parent=2;
    bs[0].parent=-1;bs[1].parent=0;bs[2].parent=1;
    bs[3].parent=0;bs[4].parent=3;
    a.skeleton_count=a.skeleton_name_count=4U;a.skeletons=as;a.skeleton_names=an;
    b.skeleton_count=b.skeleton_name_count=5U;b.skeletons=bs;b.skeleton_names=bn;
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(l.master_index[1]==2 && l.master_index[2]==3 && l.master_index[3]==1);
    CK(l.link_kind[4]==2U && l.master_index[4]==1);
    CK(l.direct_links==3U && l.inherited_links==1U);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.direct==3 && q.repeated_direct_targets==0 && q.ancestry_conflicts==0);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    put(&bn[1],"pl0dlh00__15");put(&bn[2],"pl0dlh00__16");
    put(&bn[3],"pl0drh00__09");
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(l.direct_links==3U && l.master_index[1]==2 && l.master_index[2]==3 && l.master_index[3]==1);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.ancestry_conflicts==0 && q.repeated_direct_targets==0);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    put(&bn[1],"em39lh00__15");put(&bn[2],"em39lh00__16");
    put(&bn[3],"em39rh00__09");
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(l.direct_links==3U && l.master_index[1]==2 && l.master_index[2]==3 && l.master_index[3]==1);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.ancestry_conflicts==0 && q.repeated_direct_targets==0);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    put(&bn[1],"pl04lh04__15");put(&bn[2],"pl04lh04__16");
    put(&bn[3],"pl04rh01__09");
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(l.direct_links==3U && l.master_index[1]==2 && l.master_index[2]==3 && l.master_index[3]==1);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.ancestry_conflicts==0 && q.repeated_direct_targets==0);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    /* pl04 hands contain an ordinary born__ rig PLUS a separate matching
       named helper joint; this exact double attachment is legitimate. */
    put(&bn[4],"born__15");bs[4].parent=0;
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.recognized_hand_alias_pairs==1U && q.repeated_direct_targets==0U);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)==RE4_OK);
    put(&bn[4],"pl04lh04__15");
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(re4_character_audit(&a,&b,&l,&q)==RE4_OK);
    CK(q.recognized_hand_alias_pairs==0U && q.repeated_direct_targets==1U);
    CK(re4_character_check_policy(&q,RE4_PART_POLICY_STRICT)!=RE4_OK);
    puts("Ada/Wesker/Krauser/Luis aliases and safe duplicate policy PASS");return 0;
}
