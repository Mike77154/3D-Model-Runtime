#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"character test line %d\n",__LINE__);return 1;}}while(0)
static void bone(re4_pmd_skeleton *b,int p,int tx)
{int i;memset(b,0,sizeof(*b));b->parent=p;for(i=0;i<3;++i)b->data[i]=65536;b->data[6]=65536;b->data[7]=tx;}
static void matrix(re4_skel_matrix *m,int tx)
{int i;memset(m,0,sizeof(*m));for(i=0;i<3;++i)m->m[i*5]=65536;m->m[3]=tx;}
int main(void)
{
    re4_pmd_model master,part;re4_pmd_name mn[3],pn[4];re4_pmd_skeleton ms[3],ps[4];
    re4_skel_matrix mr[3],ma[3],pr[4],pa[4];re4_skel_pose mrest,manim,prest,pout;
    re4_character_links links;re4_err e;int i;
    memset(&master,0,sizeof(master));memset(&part,0,sizeof(part));
    for(i=0;i<3;++i)bone(&ms[i],i-1,0);
    for(i=0;i<4;++i)bone(&ps[i],i-1,0);
    mn[0].name.ptr="Scene_Root";mn[0].name.len=10;
    mn[1].name.ptr="born__15_S09";mn[1].name.len=12;
    mn[2].name.ptr="born__16_S10";mn[2].name.len=12;
    pn[0].name.ptr="Scene_Root";pn[0].name.len=10;
    pn[1].name.ptr="lh00__15";pn[1].name.len=8;
    pn[2].name.ptr="lh00__16";pn[2].name.len=8;
    pn[3].name.ptr="mesh_unmapped";pn[3].name.len=13;
    master.skeleton_name_count=master.skeleton_count=3;master.skeleton_names=mn;master.skeletons=ms;
    part.skeleton_name_count=part.skeleton_count=4;part.skeleton_names=pn;part.skeletons=ps;
    e=re4_character_link(&master,&part,&links);CK(e==RE4_OK);
    CK(links.direct_links==2);CK(links.master_index[1]==1);CK(links.master_index[2]==2);
    CK(links.master_index[3]==2);CK(links.stationary_links==1);
    matrix(&mr[0],0);matrix(&mr[1],65536);matrix(&mr[2],131072);
    matrix(&ma[0],0);matrix(&ma[1],131072);matrix(&ma[2],262144);
    matrix(&pr[0],0);matrix(&pr[1],32768);matrix(&pr[2],98304);matrix(&pr[3],131072);
    mrest.world=mr;mrest.bone_count=3;manim.world=ma;manim.bone_count=3;
    prest.world=pr;prest.bone_count=4;pout.world=pa;
    e=re4_character_retarget(&master,&mrest,&manim,&part,&prest,&links,&pout,4);CK(e==RE4_OK);
    CK(pa[0].m[3]==0);CK(pa[1].m[3]==98304);CK(pa[2].m[3]==229376);CK(pa[3].m[3]==262144);
    CK(re4_character_retarget(&master,&mrest,&manim,&part,&prest,&links,&pout,2)==RE4_ERR_NO_SPACE);
    puts("character retarget identity/name/inherit/space tests OK");return 0;
}
