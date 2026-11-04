/* Validate that wing IDs map to crow body only, not unrelated skeletons. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
static void name(re4_pmd_name *n,const char *s)
{n->name.ptr=s;n->name.len=strlen(s);}
int main(void)
{
 re4_pmd_model crow,wing,other;re4_pmd_name cn[4],wn[4],on[4];
 re4_pmd_skeleton cb[4],wb[4],ob[4];re4_character_links links;
 memset(&crow,0,sizeof(crow));memset(&wing,0,sizeof(wing));memset(&other,0,sizeof(other));
 memset(cn,0,sizeof(cn));memset(wn,0,sizeof(wn));memset(on,0,sizeof(on));
 memset(cb,0,sizeof(cb));memset(wb,0,sizeof(wb));memset(ob,0,sizeof(ob));
 crow.skeleton_count=4;crow.skeleton_name_count=4;crow.skeleton_names=cn;crow.skeletons=cb;
 wing.skeleton_count=4;wing.skeleton_name_count=4;wing.skeleton_names=wn;wing.skeletons=wb;
 other.skeleton_count=4;other.skeleton_name_count=4;other.skeleton_names=on;other.skeletons=ob;
 name(&cn[0],"Scene_Root");name(&cn[1],"em2300_crow__00");
 name(&cn[2],"em2300_crow__01");name(&cn[3],"em2300_crow__06");
 name(&wn[0],"Scene_Root");name(&wn[1],"em2301_openWing__00");
 name(&wn[2],"em2301_openWing__01");name(&wn[3],"em2301_openWing__06");
 name(&on[0],"Scene_Root");name(&on[1],"born__00");
 name(&on[2],"born__01");name(&on[3],"born__06");
 cb[0].parent=-1;cb[1].parent=0;cb[2].parent=1;cb[3].parent=2;
 wb[0].parent=-1;wb[1].parent=0;wb[2].parent=1;wb[3].parent=2;
 ob[0].parent=-1;ob[1].parent=0;ob[2].parent=1;ob[3].parent=2;
 if(re4_character_link(&crow,&wing,&links)!=RE4_OK)return 1;
 if(links.direct_links!=3||links.master_index[3]!=3)return 2;
 if(re4_character_link(&other,&wing,&links)!=RE4_OK)return 3;
 if(links.direct_links!=0)return 4;
 puts("crow wing links unique and isolated PASS");return 0;
}
