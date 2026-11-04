/* Synthetic triangles deliberately stored in reverse visibility order.
   This fixture is CC0 and does not use original game assets. */
#include "rtcw_codec.h"
#include "rtcw_texrender.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void model(int reverse){
    unsigned int s,j;const char *label;long x,y,z,shift;
    memset(&rtcw_current,0,sizeof(rtcw_current));
    rtcw_current.vertex_count=6UL;rtcw_current.triangle_count=2UL;rtcw_current.surface_count=2U;
    for(s=0U;s<2U;s++){
        label=(s==0U)^(reverse!=0)?"near":"far";
        strcpy(rtcw_current.surfaces[s].shader,label);
        strcpy(rtcw_current.surfaces[s].name,label);
        rtcw_current.surfaces[s].first_triangle=(unsigned long)s;
        rtcw_current.surfaces[s].triangles=1UL;
        rtcw_current.tris[s].a=(unsigned long)s*3UL;
        rtcw_current.tris[s].b=(unsigned long)s*3UL+1UL;
        rtcw_current.tris[s].c=(unsigned long)s*3UL+2UL;
        rtcw_current.tris[s].surface=s;
        shift=strcmp(label,"near")==0?64L*12L:0L;
        for(j=0U;j<3U;j++){
            x=(j==1U)?4096L:0L;
            y=(j==2U)?4096L:0L;
            z=0L;
            rtcw_current.verts[s*3U+j].x=x+shift;
            rtcw_current.verts[s*3U+j].y=y+shift;
            rtcw_current.verts[s*3U+j].z=z+shift;
            rtcw_current.verts[s*3U+j].u=strcmp(label,"near")==0?1024L:0L;
            rtcw_current.verts[s*3U+j].v=0L;
        }
    }
}
int main(int argc,char **argv){
    int reverse;unsigned long ms;char *end;if(argc!=6)return 2;
    reverse=atoi(argv[4]);ms=strtoul(argv[5],&end,10);if(*end)return 2;model(reverse);
    if(!(argv[3][0]=='@' ? rtcw_render_textured_map_time(argv[2],argv[3]+1,ms) :
         rtcw_render_textured_time(argv[2],argv[1],strcmp(argv[3],"-")==0?NULL:argv[3],ms))){
        fprintf(stderr,"%s\n",rtcw_render_error());return 1;
    }
    printf("missing=%u partial=%u\n",rtcw_render_missing_textures(),rtcw_render_partial_shaders());
    return 0;
}
