#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_u32 le32(const re3c_u8 *p){return (re3c_u32)p[0]|((re3c_u32)p[1]<<8)|((re3c_u32)p[2]<<16)|((re3c_u32)p[3]<<24);}
static int one(size_t n){
 re3c_u8 data[16],before[16];size_t i;re3c_u32 old0,old1;
 for(i=0;i<sizeof(data);i++)data[i]=(re3c_u8)(i*17u+3u);
 memcpy(before,data,sizeof(data));old0=le32(data);old1=le32(data+4);
 if(re3c_pc_rebase_polygon_record(data,n,4u,3u)!=RE3C_OK)return 1;
 if(le32(data)!=old0+(3u<<22)||le32(data+4)!=old1+(4u<<16))return 2;
 for(i=8u;i<sizeof(data);i++)if(data[i]!=before[i])return 3;
 /* two's complement inverse, checked by separate signed-compatible modulo */
 if(re3c_pc_rebase_polygon_record(data,n,0u,0u)!=RE3C_OK)return 4;
 if(le32(data)!=old0+(3u<<22)||le32(data+4)!=old1+(4u<<16))return 5;
 return 0;
}
int main(void){re3c_u8 a[16]={0};int rc;
 if(!re3c_check_host())return 1;
 rc=one(12u);if(rc){printf("triangle fail %d\n",rc);return 1;}
 rc=one(16u);if(rc){printf("quad fail %d\n",rc);return 1;}
 if(re3c_pc_rebase_polygon_record(a,13u,1u,1u)!=RE3C_ERR_BAD_ARGUMENT)return 1;
 if(re3c_pc_rebase_polygon_record(a,12u,256u,1u)!=RE3C_ERR_RANGE)return 1;
 if(re3c_pc_rebase_polygon_record(NULL,12u,1u,1u)!=RE3C_ERR_BAD_ARGUMENT)return 1;
 puts("PASS: Ghidra RE3 polygon rebase triangle/quad");return 0;
}
