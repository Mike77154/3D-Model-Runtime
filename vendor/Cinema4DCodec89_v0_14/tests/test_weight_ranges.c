#include "c4d89.h"
#include <stdio.h>
#include <stdlib.h>
typedef struct PointCheck {
    unsigned long count;
    unsigned long previous;
    unsigned long max;
    int broken;
} PointCheck;
static int check_point(void *v,unsigned long id,unsigned long seq)
{
    PointCheck *s=(PointCheck*)v;
    if(seq!=s->count || (s->count && id<=s->previous)){
        s->broken=1;return 0;
    }
    s->previous=id;s->max=id;s->count++;
    return 1;
}
int main(int argc,char **argv)
{
    unsigned int i;C4D89_WeightInventory inv;int rc;
    unsigned long total=0ul;
    if(argc!=4){fprintf(stderr,"Usage: test_weight_ranges scene.c4d tags selectedpoints\n");return 2;}
    rc=c4d89_inventory_weight_tags(argv[1],&inv);
    if(rc!=C4D89_OK || inv.count!=(unsigned int)strtoul(argv[2],0,10)){
        fprintf(stderr,"FAILED tags %u status %d\n",inv.count,rc);return 1;
    }
    for(i=0u;i<inv.count;i++) {
        C4D89_WeightTagHint *w=&inv.tags[i];PointCheck pc;
        if(w->layout==1u){continue;}
        if(w->layout!=3u && w->layout!=4u){fprintf(stderr,"FAILED range layout\n");return 1;}
        pc.count=0ul;pc.previous=0ul;pc.max=0ul;pc.broken=0;
        rc=c4d89_stream_weight_point_ids(argv[1],w,check_point,&pc);
        if(rc!=C4D89_OK||pc.broken||pc.count!=w->selected_point_count||
           pc.max!=w->max_point_index){
            fprintf(stderr,"FAILED range stream\n");return 1;
        }
        total+=pc.count;
    }
    if(total!=strtoul(argv[3],0,10)){fprintf(stderr,"FAILED total %lu\n",total);return 1;}
    printf("PASS: %u Weight tags, %lu enumerated point IDs\n",inv.count,total);
    return 0;
}
