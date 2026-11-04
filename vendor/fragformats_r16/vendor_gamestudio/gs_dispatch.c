#include "gs_dispatch.h"
#include "gs_registry.h"
#include <string.h>
int gs_dispatch(FILE *f,FILE *out,FILE *meta,GSStats *stats,unsigned long frame){
 unsigned char id[4];
 if(!f||!stats)return 0;
 memset(stats,0,sizeof(*stats));
 if(fread(id,1,4,f)!=4 || fseek(f,0L,SEEK_SET))return 0;
 if(memcmp(id,"MDL2",4)==0)return gs_a3_probe(f,out,meta,stats,frame);
 if(memcmp(id,"MDL3",4)==0)return gs_a4_mdl3(f,out,meta,stats,frame);
 if(memcmp(id,"MDL4",4)==0 || memcmp(id,"MDL5",4)==0)return gs_a5_mdl45(f,out,meta,stats,frame);
 if(memcmp(id,"MDL7",4)==0)return gs_a678_mdl7(f,out,meta,stats,frame);
 return 0;
}
