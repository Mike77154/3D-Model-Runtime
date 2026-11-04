#include "ss89resources.h"
#include <stdio.h>
int main(int argc,char **argv)
{
 SS89_Vertex v;
 SS89_Triangle t;
 SS89_i32 u,w;
 if(argc!=2)return 2;
 if(ss89_resource(argv[1],NULL,0U,0U)!=0)return 1;
 if(ss89_ska_vertex_count()!=3U)return 1;
 if(ss89_ska_triangle_count()!=1U)return 1;
 if(ss89_ska_uv_count()!=3U)return 1;
 if(ss89_ska_get_vertex(1U,&v)!=SS89_OK)return 1;
 if(v.x!=65536 || v.y!=0 || v.z!=0)return 1;
 if(ss89_ska_get_triangle(0U,&t)!=SS89_OK)return 1;
 if(t.v[0]!=0U||t.v[1]!=1U||t.v[2]!=2U)return 1;
 if(ss89_ska_get_uv(1U,&u,&w)!=SS89_OK)return 1;
 if(u!=65536||w!=0)return 1;
 if(ss89_ska_get_vertex(3U,&v)==SS89_OK)return 1;
 puts("Skeletal resource getter API PASSED");
 return 0;
}
