/* CC0-1.0. Strict ISO C89. Fixed-capacity geometry visibility layer.
   The original game's part IDs are not automatically inferred from models. */
#include "rtcw_visibility.h"
#include "rtcw_codec.h"
#include <string.h>
static unsigned char part_ids[RTCW_MAX_SURFACES];
static unsigned long hidden_tris,visible_tris;
static char reason[128]="OK";
static int bad(const char *s){strncpy(reason,s,sizeof(reason)-1U);reason[sizeof(reason)-1U]=0;return 0;}
const char *rtcw_visibility_error(void){return reason;}
void rtcw_visibility_reset(void){
    unsigned int i;
    for(i=0U;i<RTCW_MAX_SURFACES;i++)part_ids[i]=(unsigned char)RTCW_VIS_ALWAYS;
    hidden_tris=visible_tris=0UL;
    strcpy(reason,"OK");
}
int rtcw_visibility_bind(unsigned int first,unsigned int count,unsigned int part){
    unsigned int i;
    if(part>RTCW_VIS_ALWAYS||first>RTCW_MAX_SURFACES||count>RTCW_MAX_SURFACES-first)
        return bad("invalid visibility surface range or part ID");
    for(i=first;i<first+count;i++)part_ids[i]=(unsigned char)part;
    return 1;
}
int rtcw_visibility_apply(unsigned int script_mask){
    unsigned int s;unsigned long source,total=0UL,count,start,j;
    rtcw_surface *surf;
    if(script_mask>255U)return bad("script visibility mask exceeds eight bits");
    hidden_tris=0UL;visible_tris=0UL;
    for(s=0U;s<rtcw_current.surface_count;s++){
        surf=&rtcw_current.surfaces[s];source=surf->first_triangle;count=surf->triangles;
        if(source>rtcw_current.triangle_count||count>rtcw_current.triangle_count-source)
            return bad("invalid surface triangle range");
        start=total;
        if(part_ids[s]==RTCW_VIS_ALWAYS||(script_mask&(1U<<part_ids[s]))!=0U){
            /* Since start <= source for ordered contiguous surfaces, forward compaction is safe. */
            if(start>source)return bad("noncontiguous triangle ranges in assembled mesh");
            for(j=0UL;j<count;j++){
                rtcw_triangle tri=rtcw_current.tris[source+j];
                if(tri.surface!=s)return bad("triangle surface index mismatch");
                rtcw_current.tris[total+j]=tri;
            }
            total+=count;visible_tris+=count;
        }else{
            hidden_tris+=count;surf->triangles=0UL;
        }
        surf->first_triangle=start;
    }
    rtcw_current.triangle_count=total;
    return 1;
}
unsigned long rtcw_visibility_hidden_triangles(void){return hidden_tris;}
unsigned long rtcw_visibility_visible_triangles(void){return visible_tris;}
