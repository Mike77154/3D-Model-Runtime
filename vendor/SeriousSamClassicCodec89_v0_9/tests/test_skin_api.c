#include "ss89skin.h"
#include <stdio.h>
int main(int argc,char**argv){SS89S_Info info;SS89S_Point p;SS89_Triangle t;
 if(argc!=4)return 2;
 if(!ss89s_load_mesh(argv[1],0U)||!ss89s_load_skeleton(argv[2],0U)||!ss89s_load_animation(argv[3])){fprintf(stderr,"LOAD: %s\n",ss89s_error());return 3;}
 ss89s_info(&info);
 if(info.vertices!=3U||info.triangles!=1U||info.bones!=2U||info.weights!=4U||info.clips!=1U)return 4;
 if(!ss89s_pose(0U,1U,0U,0)||!ss89s_get_vertex(1U,&p))return 5;
 if(p.x<65000||p.x>66000||p.y<65000||p.y>66000||p.z!=0)return 6;
 if(!ss89s_get_vertex(2U,&p)||p.x<65000||p.x>66000||p.y<65000||p.y>66000)return 7;
 if(!ss89s_get_triangle(0U,&t)||t.v[0]!=0U||t.v[1]!=1U||t.v[2]!=2U)return 8;
 if(ss89s_get_vertex(3U,&p))return 9;
 puts("C89 SKIN API: PASS root/child weighted blend, triangle and getters");return 0;
}
