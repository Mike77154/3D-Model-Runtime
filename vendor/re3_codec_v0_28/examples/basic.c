#include "re3_codec.h"
#include <stdio.h>
int main(int argc,char **argv){re3c_model m;int r;if(argc!=2)return 2;re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"%s\n",re3c_result_string(r));return 1;}printf("parts=%u joints=%u\n",(unsigned)m.body.part_count,(unsigned)m.banks[RE3C_BANK_A].joint_count);re3c_model_close(&m);return 0;}
