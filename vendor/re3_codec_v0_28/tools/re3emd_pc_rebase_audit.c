/* Read-only audit of real EMD polygon records. C89, no heap. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model m;
static re3c_u16 u16(const re3c_u8*p){return (re3c_u16)((re3c_u16)p[0]|((re3c_u16)p[1]<<8));}
static re3c_u32 u32(const re3c_u8*p){return (re3c_u32)p[0]|((re3c_u32)p[1]<<8)|((re3c_u32)p[2]<<16)|((re3c_u32)p[3]<<24);}
int main(int argc,char **argv){re3c_u32 part,tri=0u,quad=0u;int rc; if(argc!=2){fprintf(stderr,"usage: %s EMXX.EMD\n",argv[0]);return 2;}
 re3c_model_init(&m);rc=re3c_open_file(&m,argv[1]);if(rc!=RE3C_OK){fprintf(stderr,"open %s\n",re3c_result_string(rc));return 1;}
 for(part=0;part<m.body.part_count;part++){
  re3c_u8 h[24],raw[16],changed[16];re3c_u32 j;re3c_u16 to,qo,tc,qc;
  rc=re3c_read_section(&m,RE3C_SEC_MD2_BODY,8u+part*24u,h,24u);if(rc!=RE3C_OK)return 1;
  to=u16(h+12);qo=u16(h+16);tc=u16(h+20);qc=u16(h+22);
  for(j=0;j<(re3c_u32)tc+(re3c_u32)qc;j++){
   int isq=j>=tc; size_t n=isq?16u:12u;
   re3c_u32 loc=8u+(isq?qo+(j-tc)*16u:to+j*12u),w0,w1;
   rc=re3c_read_section(&m,RE3C_SEC_MD2_BODY,loc,raw,n);if(rc!=RE3C_OK){fprintf(stderr,"read part=%u\n",(unsigned)part);return 1;}
   memcpy(changed,raw,n);w0=u32(raw);w1=u32(raw+4);
   rc=re3c_pc_rebase_polygon_record(changed,n,3u,2u);if(rc!=RE3C_OK)return 1;
   if(u32(changed)!=w0+(2u<<22)||u32(changed+4)!=w1+(3u<<16)||memcmp(raw+8,changed+8,n-8u)!=0){fprintf(stderr,"FAIL part=%u poly=%u\n",(unsigned)part,(unsigned)j);return 1;}
   if(isq)quad++;else tri++;
  }
 }
 printf("PASS file=%s triangles=%u quads=%u polygons=%u\n",argv[1],(unsigned)tri,(unsigned)quad,(unsigned)(tri+quad));
 re3c_model_close(&m);return 0;
}
