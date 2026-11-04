/* CLI for skeletal torso/legs preview. Integer-only runtime. */
#include "rtcw_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static int num(const char *str,unsigned long *v) {
    char *end;errno=0;*v=strtoul(str,&end,10);
    return !errno && end!=str && *end==0 && str[0]!='-';
}
int main(int argc,char **argv) {
    unsigned long la,lb,lt,ta,tb,tt,yaw;
    if(argc!=12) {
        fprintf(stderr,"Usage: rtcw_mds_anim model.mds legs_a legs_b legs_t torso_a torso_b torso_t torso_yaw_q16 out.obj out.ppm out.txt\n");
        return 2;
    }
    if(!num(argv[2],&la)||!num(argv[3],&lb)||!num(argv[4],&lt)||
       !num(argv[5],&ta)||!num(argv[6],&tb)||!num(argv[7],&tt)||
       !num(argv[8],&yaw)||lt>4096UL||tt>4096UL||yaw>65535UL) {
        fprintf(stderr,"Invalid frames, fraction or yaw\n");return 2;
    }
    if(!rtcw_open(argv[1]) || !rtcw_decode_mds_pose(la,lb,(unsigned int)lt,
          ta,tb,(unsigned int)tt,(unsigned short)yaw)){
        fprintf(stderr,"MDS pose failed: %s\n",rtcw_error());return 1;
    }
    if(!rtcw_save_obj(argv[9]) || !rtcw_save_ppm(argv[10]) || !rtcw_report(argv[11])){
        fprintf(stderr,"Export failed: %s\n",rtcw_error());return 1;
    }
    printf("OK MDS bone-space pose legs=%lu/%lu torso=%lu/%lu\n",la,lb,ta,tb);
    return 0;
}
