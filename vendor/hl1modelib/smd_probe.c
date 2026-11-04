#include "hlfp.h"
#include <stdio.h>
#include <string.h>
#define MAX_BYTES (32U*1024U*1024U)
static char text[MAX_BYTES+1];
static hlfp_smd_model smd;
static hlfp_error err;
int main(int argc,char**argv){ FILE *f;long n;int rc;
 if(argc<2)return 1;
 f=fopen(argv[1],"rb");if(!f)return 2;
 fseek(f,0,SEEK_END);n=ftell(f);rewind(f);
 if(n<0 || n>MAX_BYTES){fclose(f);return 3;}
 if(fread(text,1,(size_t)n,f)!=(size_t)n){fclose(f);return 4;}fclose(f);
 text[n]='\0';
 hlfp_smd_init(&smd);rc=hlfp_smd_parse_buffer(text,(size_t)n,&smd,&err);
 printf("%d,%d,%d,%d,%s,%s\n",rc,smd.num_nodes,smd.num_frames,smd.num_triangles,argv[1],rc==0?"OK":err.message);
 return 0;
}
