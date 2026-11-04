/* Frame-between-frame preview, strictly integral math. */
#include "rtcw_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static int number(const char *str,unsigned long *v) {
    char *end;errno=0;*v=strtoul(str,&end,10);
    return !errno && end!=str && *end==0 && str[0]!='-';
}
int main(int argc,char **argv){
    unsigned long a,b,t;
    if(argc!=8) {
        fprintf(stderr,"Usage: rtcw_anim model.mds frame_a frame_b t_q12[0..4096] out.obj out.ppm out.txt\n");
        return 2;
    }
    if(!number(argv[2],&a)||!number(argv[3],&b)||!number(argv[4],&t)||t>4096UL){
        fprintf(stderr,"bad frame/fraction\n");return 2;
    }
    if(!rtcw_open(argv[1]) || !rtcw_decode_blend(a,b,(unsigned int)t)){
        fprintf(stderr,"decode blend failed: %s\n",rtcw_error());return 1;
    }
    if(!rtcw_save_obj(argv[5])||!rtcw_save_ppm(argv[6])||!rtcw_report(argv[7])){
        fprintf(stderr,"export failed: %s\n",rtcw_error());return 1;
    }
    printf("OK blended frame %lu to %lu, fraction %lu/4096\n",a,b,t);
    return 0;
}
