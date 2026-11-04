/* Decode DDS normal payload and reconstruct XYZ channels for visualization. */
#include "dts89_modules.h"
#include "dts89_shaderer.h"
#include <stdio.h>
#define N_IN (32u*1024u*1024u)
#define N_OUT (64u*1024u*1024u)
static dts89_u8 in[N_IN],out[N_OUT];
static dts89_u8 fx_to_byte(dts89_fx16_16 v)
{
 dts89_s32 x;
 if(v>65536)v=65536;
 if(v<(-65536))v=-65536;
 x=((v+65536)*255+65536)/131072;
 return (dts89_u8)(x<0?0:(x>255?255:x));
}
int main(int argc,char **argv)
{
 FILE *f;long len;int rc;
 dts89_mod_texture_info info;dts89_u32 count,mode,i;
 if(argc!=3){fputs("usage: dts89_normal_decode input.dds output.rgba\n",stderr);return 2;}
 f=fopen(argv[1],"rb");if(!f)return 2;
 if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
 len=ftell(f);if(len<0 || (unsigned long)len>N_IN){fclose(f);return 2;}
 rewind(f);if(fread(in,1,(size_t)len,f)!=(size_t)len){fclose(f);return 2;}
 fclose(f);
 rc=dts89_texturer_opener_info(in,(dts89_u32)len,&info);if(rc)return 3;
 if(info.width>0xFFFFFFFFu/4u || info.height>0xFFFFFFFFu/(info.width*4u))return 3;
 count=info.width*info.height*4u;if(count>N_OUT)return 3;
 rc=dts89_texturer_opener_decode_rgba8(in,(dts89_u32)len,&info,out,info.width*4u,count);
 if(rc)return 3;
 rc=dts89_shaderer_detect_normal_packing(out,info.width,info.height,info.width*4u,count,&mode);
 if(rc)return 3;
 for(i=0;i<count;i+=4u){
  dts89_shaderer_rgba8 p;dts89_fx_vec3 vec;
  p.r=out[i];p.g=out[i+1u];p.b=out[i+2u];p.a=out[i+3u];
  rc=dts89_shaderer_unpack_normal(p,mode,&vec);if(rc)return 3;
  out[i]=fx_to_byte(vec.x);out[i+1u]=fx_to_byte(vec.y);
  out[i+2u]=fx_to_byte(vec.z);out[i+3u]=255u;
 }
 f=fopen(argv[2],"wb");if(!f)return 2;
 if(fwrite(out,1,count,f)!=count){fclose(f);return 2;}
 fclose(f);
 printf("normal %lu x %lu packing=%lu\n",(unsigned long)info.width,
 (unsigned long)info.height,(unsigned long)mode);
 return 0;
}
