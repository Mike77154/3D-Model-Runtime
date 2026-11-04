/* Classify unusual PLD/PLW containers without accepting malformed geometry. */
#include "re3_packed.h"
#include <stdio.h>
#include <string.h>
static re3c_u16 getu16(const re3c_u8 *b){return (re3c_u16)((unsigned)b[0]|((unsigned)b[1]<<8));}
static re3c_u32 getu32(const re3c_u8 *b){return (re3c_u32)b[0]|((re3c_u32)b[1]<<8)|((re3c_u32)b[2]<<16)|((re3c_u32)b[3]<<24);}
int main(int argc,char**argv){FILE*f;re3p_info info;re3c_u8 buf[24],header[8];re3c_u32 i,n,external=0u,zero_normal=0u,bad_faces=0u;int r;
 if(argc!=2){fprintf(stderr,"usage: %s input.PLD|PLW\n",argv[0]);return 2;}
 f=fopen(argv[1],"rb");if(!f)return 1;
 if(fread(header,1u,8u,f)!=8u){fclose(f);return 1;}
 if(getu32(header)==0u&&getu32(header+4)==0u){
  if(fseek(f,0L,SEEK_END)==0&&ftell(f)==8L){
   printf("%s,EMPTY_MARKER,0,0,0,0\n",argv[1]);fclose(f);return 0;
  }
 }
 fclose(f);
 r=re3p_probe_file(argv[1],&info);
 if(r!=RE3C_OK){printf("%s,BAD_CONTAINER,0,0,0,0\n",argv[1]);return 0;}
 f=fopen(argv[1],"rb");if(!f)return 1;
 i=info.kind==RE3P_KIND_PLD?2u:7u;
 if(fseek(f,(long)info.segments[i].offset,SEEK_SET)!=0||fread(header,1u,8u,f)!=8u){fclose(f);return 1;}
 n=getu32(header+4);
 if(n>RE3C_MAX_PARTS||8u+n*24u>info.segments[i].size){
  printf("%s,BAD_MD2_HEADER,0,0,0,0\n",argv[1]);fclose(f);return 0;
 }
 for(i=0u;i<n;i++){
  re3c_u32 mo=info.kind==RE3P_KIND_PLD?2u:7u;
  re3c_u32 vo,vc,nc,tc,qc,to,qo,k,section_size=info.segments[mo].size;
  if(fseek(f,(long)(info.segments[mo].offset+8u+i*24u),SEEK_SET)!=0||fread(buf,1u,24u,f)!=24u){fclose(f);return 1;}
  vo=getu16(buf);vc=getu16(buf+8);nc=getu16(buf+10);
  to=getu16(buf+12);qo=getu16(buf+16);tc=getu16(buf+20);qc=getu16(buf+22);
  if(vc==0u||vc>256u||vo>section_size||8u+(re3c_u32)vo+(re3c_u32)vc*8u>section_size)external++;
  if(nc==0u)zero_normal++;
  if(8u+to+tc*12u>section_size||8u+qo+qc*16u>section_size){bad_faces+=tc+qc;continue;}
  /* Do not reinterpret unknown primitive encodings as valid classic tris. */
  for(k=0u;k<tc;k++){
   re3c_u8 t[12];re3c_u32 start=info.segments[mo].offset+8u+to+k*12u;
   if(fseek(f,(long)start,SEEK_SET)!=0||fread(t,1u,12u,f)!=12u){fclose(f);return 1;}
   if(t[7]>=vc||t[10]>=vc||t[11]>=vc)bad_faces++;
  }
 }
 fclose(f);
 printf("%s,%s,%u,%u,%u,%u\n",argv[1],external?"EXTERNAL_GEOMETRY_CANDIDATE":(bad_faces?"UNKNOWN_PRIMITIVE_ENCODING":"STRUCTURALLY_PLAUSIBLE"),(unsigned)n,(unsigned)external,(unsigned)zero_normal,(unsigned)bad_faces);
 return 0;
}
