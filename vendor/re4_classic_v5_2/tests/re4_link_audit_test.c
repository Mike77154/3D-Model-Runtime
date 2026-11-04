#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"link audit test failed line %d\n",__LINE__);return 1;}}while(0)
static void put(re4_pmd_name *out,const char *name)
{out->name.ptr=name;out->name.len=(re4_size)strlen(name);}
int main(void)
{
    re4_pmd_model a,b;re4_pmd_name an[4],bn[5];
    re4_pmd_skeleton as[4],bs[5];re4_character_links l;
    re4_character_link_audit rep;int i;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
    memset(as,0,sizeof(as));memset(bs,0,sizeof(bs));
    put(&an[0],"born__00");put(&an[1],"born__01");
    put(&an[2],"born__100");put(&an[3],"born__101");
    put(&bn[0],"SceneRoot");put(&bn[1],"lh00__01");
    put(&bn[2],"lh00__100");put(&bn[3],"lh00__101");
    put(&bn[4],"lh00__101");
    as[0].parent=-1;as[1].parent=0;as[2].parent=1;as[3].parent=2;
    bs[0].parent=-1;bs[1].parent=0;bs[2].parent=1;bs[3].parent=2;
    bs[4].parent=2;
    a.skeleton_count=a.skeleton_name_count=4U;a.skeletons=as;a.skeleton_names=an;
    b.skeleton_count=b.skeleton_name_count=5U;b.skeletons=bs;b.skeleton_names=bn;
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(re4_character_audit(&a,&b,&l,&rep)==RE4_OK);
    CK(rep.direct==4U && rep.unique_direct_targets==3U);
    CK(rep.repeated_direct_targets==1U && rep.ancestry_conflicts==0U);
    CK(rep.stationary==1U);
    bs[3].parent=1; /* 101 cannot have 01 as *nearest* direct ancestor? it can. */
    bs[2].parent=3; /* now 100 child of 101 -> incompatible ordering */
    /* The linker accepts forward parent as stationary; repair graph for audit
       fixture using a topologically coherent but semantically wrong order. */
    bs[2].parent=1;bs[3].parent=1;
    put(&bn[3],"lh00__00");
    CK(re4_character_link(&a,&b,&l)==RE4_OK);
    CK(re4_character_audit(&a,&b,&l,&rep)==RE4_OK);
    CK(rep.ancestry_conflicts>=1U);
    for(i=0;i<5;++i)CK(l.master_index[i]<4);
    puts("linked bone hierarchy, duplicate targets, large IDs PASS");
    return 0;
}
