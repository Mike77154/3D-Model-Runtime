#include "re3_codec.h"
#include <stdio.h>
int main(int argc,char**argv){re3c_model m;re3c_u32 i,c[6]={0,0,0,0,0,0};int r;if(argc!=2)return 2;re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);if(r!=RE3C_OK)return 1;for(i=0;i<m.body.part_count;i++)if(m.body.parts[i].part_class<6u)c[m.body.parts[i].part_class]++;printf("unknown=%u dat=%u rigid=%u helper=%u extra=%u morph=%u\n",(unsigned)c[0],(unsigned)c[1],(unsigned)c[2],(unsigned)c[3],(unsigned)c[4],(unsigned)c[5]);re3c_model_close(&m);return 0;}
