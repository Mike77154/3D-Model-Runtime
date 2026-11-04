#include "model_runtime89.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"R22 header check line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char**argv){modelrt_frag_ugh_header h;
 if(argc!=4)return 2;
 CHECK(modelrt_frag_r22_ugh_header(argv[1],&h)==MODELRT_OK);
 CHECK(h.nested_header_offset==32u&&h.stream_version==0x10001u&&h.root_record_offset==48u&&h.object_count_hint==1u);
 CHECK(modelrt_frag_r22_ugh_header(argv[2],&h)!=MODELRT_OK);
 CHECK(modelrt_frag_r22_ugh_header(argv[3],&h)!=MODELRT_OK);
 puts("PASS UGH nested-stream header R22; no geometry claim");return 0;
}
