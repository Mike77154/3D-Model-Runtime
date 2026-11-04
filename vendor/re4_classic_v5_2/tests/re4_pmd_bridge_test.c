#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do {if (!(condition)) {fprintf(stderr,"FAIL %d\n",__LINE__);return 1;}} while(0)
int main(void)
{
    re4_u8 data[96];re4_u8 unaligned_arena[128];re4_arena arena;void *ptr;re4_span span;re4_pmd_mesh mesh;re4_pmd_vertex_q16 v;re4_pmd_triangle t;
    memset(data,0,sizeof(data));memset(&mesh,0,sizeof(mesh));
    mesh.primitive_magic=64U;mesh.index_offset=2U;mesh.index_count=3U;mesh.vertex_offset=16U;mesh.vertex_count=1U;
    data[2]=0;data[4]=0;data[6]=0; /* triangle uses vertex 0 */
    data[16+0]=0;data[16+1]=0;data[16+2]=0x80;data[16+3]=0x3f; /* x 1.0f */
    data[16+40]=0;data[16+41]=0;data[16+42]=0x00;data[16+43]=0x3f; /* uv 0.5f */
    span.data=data;span.size=sizeof(data);
    CHECK(re4_pmd_mesh_vertex(span,&mesh,0,&v)==RE4_OK);
    CHECK(v.position[0]==65536);CHECK(v.uv[0]==32768);
    CHECK(re4_pmd_mesh_triangle(span,&mesh,0,&t)==RE4_OK);CHECK(t.vertex[0]==0 && t.vertex[1]==0);
    CHECK(re4_pmd_mesh_triangle(span,&mesh,1,&t)==RE4_ERR_RANGE);
    CHECK(re4_pmd_mesh_vertex(span,&mesh,1,&v)==RE4_ERR_RANGE);
    mesh.index_count=4;CHECK(re4_pmd_mesh_triangle(span,&mesh,0,&t)==RE4_ERR_BAD_FORMAT);
    mesh.index_count=3;span.size=19;CHECK(re4_pmd_mesh_vertex(span,&mesh,0,&v)==RE4_ERR_RANGE);
    re4_arena_init(&arena,unaligned_arena+1,127UL);
    ptr=re4_arena_push(&arena,sizeof(void *),sizeof(void *));
    CHECK(ptr!=0);CHECK(((size_t)ptr % sizeof(void *))==0U);
    puts("PMD mesh bridge OK (unaligned arena checked)");return 0;
}
