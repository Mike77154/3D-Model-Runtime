/* Read ONLY one specified 6-byte PARTS_SET instruction from an extracted RE3
 * SCD script. This is not an SCD interpreter, nor a scanner for raw RDT/EXE. */
#include "re3_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
int main(int argc,char **argv){
 unsigned long offset;char *end;FILE *fp;unsigned char bytes[6];re3c_parts_set_cmd cmd;
 int r;
 if(argc!=3){fprintf(stderr,"usage: re3scd_parts_probe file.SCD OFFSET (decimal or 0xhex)\n");return 2;}
 if(argv[2][0]=='-'){fprintf(stderr,"negative offset\n");return 2;}
 offset=strtoul(argv[2],&end,0);
 if(end==argv[2]||*end!='\0'||offset>(unsigned long)LONG_MAX){fprintf(stderr,"invalid offset\n");return 2;}
 fp=fopen(argv[1],"rb");if(!fp){fprintf(stderr,"cannot open file\n");return 1;}
 if(fseek(fp,(long)offset,SEEK_SET)!=0){fclose(fp);fprintf(stderr,"cannot seek offset\n");return 1;}
 if(fread(bytes,1u,6u,fp)!=6u){fclose(fp);fprintf(stderr,"truncated command\n");return 1;}
 fclose(fp);
 r=re3c_decode_parts_set(bytes,6u,&cmd);
 if(r!=RE3C_OK){fprintf(stderr,"not a valid PARTS_SET at requested offset: %s\n",re3c_result_string(r));return 1;}
 printf("PARTS_SET opcode=0x%02X offset=0x%lX align=0x%02X id=0x%02X type=0x%02X value=%d\n",
  (unsigned)cmd.opcode,offset,(unsigned)cmd.align,(unsigned)cmd.id,(unsigned)cmd.type,(int)cmd.value);
 puts("Only structure decoded; the effects of id/type/value require game verification.");
 return 0;
}
