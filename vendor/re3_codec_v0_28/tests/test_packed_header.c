/* Synthetic PLD/PLW directory tests: no external game assets required. C89. */
#include "re3_packed.h"
#include <stdio.h>
#include <string.h>
static const char *test_path="re3packed_test_container.tmp";
static void put32(unsigned char *p,unsigned long n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);p[2]=(unsigned char)(n>>16);p[3]=(unsigned char)(n>>24);}
static int run_case(unsigned count,int bad){unsigned char d[80];unsigned i,total,idx;FILE*f;re3p_info info;int r;
 memset(d,0,sizeof(d));idx=40u;total=idx+count*4u;
 put32(d,idx);put32(d+4u,count);
 for(i=0;i<count;i++)put32(d+idx+i*4u,8u+i*3u);
 if(bad==1)put32(d+idx,12u); /* first segment invalid */
 if(bad==2)put32(d+idx+4u,8u); /* duplicate/nonmonotonic */
 if(bad==3)put32(d,idx+1u); /* invalid directory alignment/length */
 if(bad==4)total-=4u; /* truncated directory */
 f=fopen(test_path,"wb");if(!f)return 0;
 if(fwrite(d,1u,total,f)!=total){fclose(f);remove(test_path);return 0;}
 fclose(f);
 r=re3p_probe_file(test_path,&info);
 remove(test_path);
 if(bad==0)return r==RE3C_OK&&info.segment_count==count;
 return r==RE3C_ERR_BAD_FORMAT;
}
int main(void){
 if(!run_case(5u,0)||!run_case(9u,0)||!run_case(5u,1)||
    !run_case(5u,2)||!run_case(5u,3)||!run_case(9u,4)){
  fprintf(stderr,"FAIL packed header validation\n");return 1;
 }
 puts("PASS PLD/PLW section directory acceptance and rejection");return 0;
}
