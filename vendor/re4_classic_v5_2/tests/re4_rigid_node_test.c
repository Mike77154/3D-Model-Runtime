/* Synthetic zero-bone-table node, rigid pose delta and rejection checks. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do { if (!(x)) { printf("rigid node failure %d\n",__LINE__); return 1; } } while(0)
int main(void)
{
    re4_pmd_node node;re4_pmd_vertex_q16 v;
    re4_skel_matrix rm[1],am[1];re4_skel_pose rest,anim;
    re4_q16 p[3],n[3];re4_u32 i;
    memset(&node,0,sizeof(node));memset(&v,0,sizeof(v));
    memset(rm,0,sizeof(rm));memset(am,0,sizeof(am));
    for(i=0;i<3U;++i){rm[0].m[5U*i]=65536L;am[0].m[5U*i]=65536L;}
    rm[0].m[3]=65536L;am[0].m[3]=3L*65536L;
    node.node_bone_count=0U;node.skeleton_index=0;
    v.weights[0]=65536L;v.position[0]=5L*65536L;v.normal[1]=65536L;
    memset(&rest,0,sizeof(rest));memset(&anim,0,sizeof(anim));
    rest.world=rm;rest.bone_count=1;anim.world=am;anim.bone_count=1;
    CK(re4_skel_skin_rigid_node(&node,&v,&rest,&anim,p,n)==RE4_OK);
    CK(p[0]==7L*65536L && p[1]==0 && n[1]==65536L);
    v.weights[0]=32768L;
    CK(re4_skel_skin_rigid_node(&node,&v,&rest,&anim,p,n)==RE4_ERR_BAD_FORMAT);
    node.node_bone_count=2U;
    CK(re4_skel_skin_rigid_node(&node,&v,&rest,&anim,p,n)==RE4_ERR_UNSUPPORTED);
    puts("zero-bone-table rigid node PASS");return 0;
}
