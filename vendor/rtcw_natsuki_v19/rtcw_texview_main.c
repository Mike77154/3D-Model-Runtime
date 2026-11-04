/* Texture preview CLI for MD3/MDC/MDS (extracted assets). */
#include "rtcw_codec.h"
#include "rtcw_texrender.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int main(int argc,char **argv){
    char *end;unsigned long frame;const char *skin=NULL,*shader=NULL;
    if(argc<6||argc>8){
        fprintf(stderr,"Usage: rtcw_texview model.mds frame asset_root preview.ppm report.txt [skin|-] [shaders.shader|-]\n");return 2;
    }
    errno=0;frame=strtoul(argv[2],&end,10);
    if(errno||end==argv[2]||*end||argv[2][0]=='-'){fprintf(stderr,"invalid frame\n");return 2;}
    if(argc>=7&&argv[6][0]!='-')skin=argv[6];
    if(argc==8&&argv[7][0]!='-')shader=argv[7];
    if(!rtcw_open(argv[1])||!rtcw_decode(frame)){fprintf(stderr,"model: %s\n",rtcw_error());return 1;}
    if(skin&&!rtcw_apply_skin(skin)){fprintf(stderr,"skin parse failed\n");return 1;}
    if(!rtcw_render_textured(argv[4],argv[3],shader)){
        fprintf(stderr,"preview: %s\n",rtcw_render_error());return 1;
    }
    if(!rtcw_report(argv[5])){fprintf(stderr,"report: %s\n",rtcw_error());return 1;}
    printf("OK texture preview: %s; missing/unavailable textures on %u surfaces\n",
           argv[4],rtcw_render_missing_textures());
    return 0;
}
