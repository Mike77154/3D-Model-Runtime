#include "ss89.h"
#include <stdio.h>
static SS89_Model model;
int main(int argc,char **argv)
{
    int rc;
    if(argc!=3)return 2;
    rc=ss89_open(&model,argv[1]);
    if(rc!=SS89_OK||!model.frame_legacy_normals){fprintf(stderr,"AV16 not detected\n");return 1;}
    ss89_close(&model);
    rc=ss89_open(&model,argv[2]);
    if(rc!=SS89_OK||model.frame_legacy_normals){fprintf(stderr,"AV17 not detected\n");return 1;}
    ss89_close(&model);
    puts("PASS AV16 indexed legacy normal mode / AV17 heading-pitch mode");
    return 0;
}
