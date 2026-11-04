#include "ss89resources.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(int argc,char**argv){const char*out=NULL;unsigned lod=0U,frame=0U;int i;if(argc<2){fprintf(stderr,"usage: ss89resource file.[bm|skl|ba|ani|tex|smc] [--obj out.obj | --ppm out.ppm | --pam out.pam] [--lod N] [--frame N]\n");return 2;}for(i=2;i<argc;i++){if((!strcmp(argv[i],"--obj")||!strcmp(argv[i],"--ppm")||!strcmp(argv[i],"--pam"))&&i+1<argc)out=argv[++i];else if(!strcmp(argv[i],"--lod")&&i+1<argc)lod=(unsigned)strtoul(argv[++i],NULL,10);else if(!strcmp(argv[i],"--frame")&&i+1<argc)frame=(unsigned)strtoul(argv[++i],NULL,10);else{fprintf(stderr,"bad argument %s\n",argv[i]);return 2;}}return ss89_resource(argv[1],out,lod,frame);}
