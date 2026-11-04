/* Explicit shader stage audit CLI, CC0-1.0, ANSI C89. */
#include "rtcw_stagefx.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    static rtcw_fx fx;
    unsigned int i;unsigned long time_ms=0UL;char *end;int r;
    if(argc!=3&&argc!=4){
        fprintf(stderr,"usage: rtcw_fxprobe shaderfile material [time_ms]\n");return 2;
    }
    if(argc==4){time_ms=strtoul(argv[3],&end,10);if(!argv[3][0]||*end)return 2;}
    r=rtcw_fx_lookup(argv[1],argv[2],time_ms,&fx);
    if(r<0){fprintf(stderr,"stagefx: %s\n",rtcw_fx_error());return 1;}
    printf("found\tstages\tunhandled\n%d\t%u\t%u\n",r,fx.stages,fx.unhandled);
    printf("stage\tmap\tblend\talpha_func\tscale_s\tscale_t\tscroll_s\tscroll_t\tr\tg\tb\ta\tunhandled\n");
    for(i=0U;i<fx.stages;i++){
        const rtcw_fx_stage *s=&fx.stage[i];
        printf("%u\t%s\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%u\t%u\t%u\t%u\t%u\n",
            i,s->map,s->blend,s->alpha_func,s->scale_s,s->scale_t,
            s->scroll_s,s->scroll_t,s->rgb[0],s->rgb[1],s->rgb[2],s->alpha,s->unhandled);
    }
    return 0;
}
