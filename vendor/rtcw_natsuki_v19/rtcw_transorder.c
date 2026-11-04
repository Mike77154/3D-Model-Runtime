/* Deterministic back-to-front heap sort. CC0-1.0 C89, no heap allocation. */
#include "rtcw_transorder.h"
#include "rtcw_codec.h"
static rtcw_transitem order[RTCW_MAX_TRIANGLES];
static unsigned long order_count;
static int cmp(const rtcw_transitem *a,const rtcw_transitem *b){
    if(a->depth<b->depth)return -1;
    if(a->depth>b->depth)return 1;
    return a->triangle<b->triangle?-1:(a->triangle>b->triangle?1:0);
}
static void swap(rtcw_transitem *a,rtcw_transitem *b){rtcw_transitem tmp=*a;*a=*b;*b=tmp;}
static void down(unsigned long start,unsigned long end){
    unsigned long root=start,child;
    while(end>0UL&&root<=(end-1UL)/2UL){
        child=root*2UL+1UL;
        if(child>end)break;
        if(child<end&&cmp(&order[child],&order[child+1UL])<0)child++;
        if(cmp(&order[root],&order[child])>=0)break;
        swap(&order[root],&order[child]);root=child;
    }
}
int rtcw_transorder_build(unsigned long count){
    unsigned long i,end;const rtcw_triangle *t;const rtcw_vertex *a,*b,*c;
    if(count>RTCW_MAX_TRIANGLES||count>rtcw_current.triangle_count)return 0;
    order_count=count;
    for(i=0;i<count;i++){
        t=&rtcw_current.tris[i];
        if(t->a>=rtcw_current.vertex_count||t->b>=rtcw_current.vertex_count||t->c>=rtcw_current.vertex_count)return 0;
        a=&rtcw_current.verts[t->a];b=&rtcw_current.verts[t->b];c=&rtcw_current.verts[t->c];
        /* Coordinates validated by the renderer: +/-1e6 Q6. */
        order[i].triangle=i;
        order[i].depth=(a->x/64L+a->y/64L+a->z/64L)+
                       (b->x/64L+b->y/64L+b->z/64L)+
                       (c->x/64L+c->y/64L+c->z/64L);
    }
    if(count<2UL)return 1;
    for(i=count/2UL;i>0UL;){i--;down(i,count-1UL);}
    for(end=count-1UL;end>0UL;end--){swap(&order[0],&order[end]);down(0,end-1UL);}
    return 1;
}
unsigned long rtcw_transorder_count(void){return order_count;}
const rtcw_transitem *rtcw_transorder_items(void){return order;}
