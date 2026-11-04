#include "re3_codec.h"
#include <stdio.h>
int main(void){
 const re3c_u8 sample[6]={0x2Au,0u,0x03u,0x04u,0xFEu,0xFFu};
 const re3c_u8 bad[6]={0x2Bu,0u,0x03u,0x04u,0xFEu,0xFFu};
 re3c_parts_set_cmd cmd;
 if(re3c_decode_parts_set(sample,6u,&cmd)!=RE3C_OK||cmd.id!=3u||cmd.type!=4u||cmd.value!=-2)return 1;
 if(re3c_decode_parts_set(bad,6u,&cmd)!=RE3C_ERR_BAD_FORMAT)return 1;
 if(re3c_decode_parts_set(sample,5u,&cmd)!=RE3C_ERR_RANGE)return 1;
 puts("PASS SCD PARTS_SET structural decode only");return 0;
}
