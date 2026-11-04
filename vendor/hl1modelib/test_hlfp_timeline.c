#include "hlfp_timeline.h"
#include <stdio.h>
#include <string.h>

static hlfp_mdl mdl;
static hlfp_timeline_hit hits[64];
static hlfp_error err;
#define F(x) ((hlfp_fixed)((x)*HLFP_FIXED_ONE))
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,err.message);return 1;}}while(0)

int main(void) {
    int n,rc;
    hlfp_mdl_init(&mdl);
    mdl.num_seqdesc=2;
    mdl.seqdesc[0].numevents=3;
    mdl.seqdesc[0].numframes=5;
    mdl.seqdesc[0].fps=F(10);
    mdl.seqdesc[0].flags=HLFP_STUDIO_LOOPING;
    mdl.seqdesc[1].numevents=2;
    mdl.seqdesc[1].numframes=5;
    mdl.seqdesc[1].fps=F(10);
    mdl.num_events=5;
    mdl.events[0].frame=0;mdl.events[0].event=100;
    mdl.events[1].frame=2;mdl.events[1].event=102;
    mdl.events[2].frame=4;mdl.events[2].event=104;
    mdl.events[3].frame=1;mdl.events[3].event=201;
    mdl.events[4].frame=3;mdl.events[4].event=203;

    rc=hlfp_timeline_collect_frames(&mdl,0,F(-1),F(9),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==8);
    CHECK(hits[0].event_index==0 && hits[0].cycle==0);
    CHECK(hits[1].event_index==2 && hits[1].cycle==-1);
    CHECK(hits[2].event_index==1 && hits[2].cycle==0);
    CHECK(hits[3].event_index==0 && hits[3].cycle==1);
    CHECK(hits[4].event_index==2 && hits[4].cycle==0);
    CHECK(hits[5].event_index==1 && hits[5].cycle==1);
    CHECK(hits[6].event_index==0 && hits[6].cycle==2);
    CHECK(hits[7].event_index==2 && hits[7].cycle==1);

    rc=hlfp_timeline_collect_frames(&mdl,0,F(9),F(-1),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==8);
    CHECK(hits[0].event_index==0 && hits[0].cycle==2);
    CHECK(hits[1].event_index==2 && hits[1].cycle==1);
    CHECK(hits[2].event_index==1 && hits[2].cycle==1);

    rc=hlfp_timeline_collect_frames(&mdl,0,F(-1),F(9),hits,2,&n,&err);
    CHECK(rc==HLFP_ERR_CAPACITY && n==2);
    rc=hlfp_timeline_collect_frames(&mdl,1,F(0),F(4),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==2 && hits[0].event_index==3 && hits[1].event_index==4);
    rc=hlfp_timeline_collect_frames(&mdl,1,F(4),F(0),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==2 && hits[0].event_index==4 && hits[1].event_index==3);
    rc=hlfp_timeline_collect_seconds(&mdl,1,0,F(1),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==2);
    rc=hlfp_timeline_collect_frames(&mdl,0,F(4),F(4),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==0);
    rc=hlfp_timeline_collect_frames(&mdl,0,F(-9),F(-1),hits,64,&n,&err);
    CHECK(rc==HLFP_OK && n==6);
    puts("timeline_ok=1 loops=1 forward=1 reverse=1 duplicate_keys=1 capacity=1 seconds=1");
    return 0;
}
