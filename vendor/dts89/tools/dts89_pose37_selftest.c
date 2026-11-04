#include "dts89_pose37.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"pose37 selftest failed at %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
 dts89_fx_mat34 m,n,r,global[2],pal[1];
 dts89_fx_vec3 p,out,vert[2];dts89_fx16_16 weights[2];
 dts89_u32 points[6],inv[16],w[1];
 dts89_s32 vi[1],bi[1],ni[1];
 dts89_mesh_view mesh;dts89_shape sh;
 unsigned i;
 dts89_pose37_identity(&m);dts89_pose37_identity(&n);
 n.a[3]=2*DTS89_FX_ONE;
 n.a[11]=3*DTS89_FX_ONE;
 dts89_pose37_mul(&m,&n,&r);
 CHECK(r.a[3]==2*DTS89_FX_ONE && r.a[11]==3*DTS89_FX_ONE);
 p.x=DTS89_FX_ONE;p.y=0;p.z=0;
 dts89_pose37_point(&r,&p,&out);
 CHECK(out.x==3*DTS89_FX_ONE && out.z==3*DTS89_FX_ONE);
 memset(&mesh,0,sizeof(mesh));memset(&sh,0,sizeof(sh));
 for(i=0;i<16u;++i)inv[i]=DTS89_F32_BITS_ZERO;
 inv[0]=inv[5]=inv[10]=inv[15]=DTS89_F32_BITS_ONE;
 points[0]=DTS89_F32_BITS_ONE;points[1]=points[2]=0;
 points[3]=points[0];points[4]=points[5]=0;
 w[0]=DTS89_F32_BITS_ONE;
 vi[0]=0;bi[0]=0;ni[0]=0;
 mesh.mesh_type=DTS89_MESH_SKIN;mesh.num_verts=2;
 mesh.skin_initial_vert_count=2;mesh.skin_initial_verts_bits=points;
 mesh.skin_initial_transform_count=1;mesh.skin_initial_transforms_bits=inv;
 mesh.skin_node_index_count=1;mesh.skin_node_index=ni;
 mesh.skin_vertex_index_count=1;mesh.skin_vertex_index=vi;
 mesh.skin_bone_index=bi;mesh.skin_weight_bits=w;
 sh.num_nodes=2;global[0]=r;global[1]=m;
 CHECK(dts89_mesher37_palette(&sh,&mesh,global,2u,pal,1u)==DTS89_OK);
 CHECK(dts89_mesher37_skin(&mesh,pal,1u,vert,weights,2u)==DTS89_OK);
 CHECK(vert[0].x==3*DTS89_FX_ONE && vert[0].z==3*DTS89_FX_ONE);
 CHECK(vert[1].x==DTS89_FX_ONE && vert[1].z==0);
 CHECK(weights[0]==DTS89_FX_ONE && weights[1]==0);
 /* reject bone indices outside the supplied palette */
 bi[0]=2;
 CHECK(dts89_mesher37_skin(&mesh,pal,1u,vert,weights,2u)==DTS89_ERR_RANGE);
 puts("pose37 selftest ok");
 return 0;
}
