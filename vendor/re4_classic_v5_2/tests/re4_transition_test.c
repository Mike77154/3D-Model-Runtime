#include "re4c3d/re4c3d.h"
#include <stdio.h>
#define CHECK(c) do { if(!(c)){fprintf(stderr,"transition test line %d\n",__LINE__);return 1;} }while(0)
static int expect(re4_transition *t,re4_u16 f,re4_q16 min,re4_q16 max)
{
    re4_q16 w=-1;
    if(re4_transition_weight(t,f,&w)!=RE4_OK)return 0;
    return w>=min&&w<=max;
}
int main(void)
{
    re4_transition t;re4_q16 result=0,old=0;re4_u32 i,curve;
    t.first=10;t.last=30;t.curve=RE4_TRANSITION_LINEAR;
    CHECK(expect(&t,0,0,0));CHECK(expect(&t,10,0,0));
    CHECK(expect(&t,20,32768,32768));CHECK(expect(&t,30,65536,65536));
    CHECK(expect(&t,40,65536,65536));
    t.curve=RE4_TRANSITION_SMOOTHSTEP;
    CHECK(expect(&t,15,10240,10240)); /* smoothstep(.25) */
    CHECK(expect(&t,20,32768,32768));
    CHECK(expect(&t,25,55296,55296));
    t.curve=RE4_TRANSITION_SMOOTHERSTEP;
    CHECK(expect(&t,20,32768,32768));
    CHECK(expect(&t,15,6750,6800));CHECK(expect(&t,25,58700,58800));
    t.first=0;t.last=65535;
    CHECK(expect(&t,65535,65536,65536));
    CHECK(expect(&t,10000,0,65536));
    /* Every supported curve must be monotonic, including full 16-bit time spans. */
    for(curve=0;curve<=2U;++curve){
        t.curve=(re4_u8)curve;t.first=0;t.last=65535;old=0;
        for(i=0;i<=65535UL;++i){
            CHECK(re4_transition_weight(&t,(re4_u16)i,&result)==RE4_OK);
            CHECK(result>=old && result<=65536);
            old=result;
        }
        CHECK(result==65536);
    }
    t.last=0;CHECK(re4_transition_weight(&t,0,&result)==RE4_ERR_BAD_FORMAT);
    t.last=30;t.curve=3;
    CHECK(re4_transition_weight(&t,10,&result)==RE4_ERR_BAD_FORMAT);
    CHECK(re4_transition_weight(0,10,&result)==RE4_ERR_NULL);
    CHECK(re4_transition_weight(&t,10,0)==RE4_ERR_NULL);
    puts("v3.0 time-varying crossfade envelopes OK");return 0;
}
