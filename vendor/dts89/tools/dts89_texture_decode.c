/* RGBA8 decoder CLI for corpus comparisons; all storage is static. */
#include "dts89_modules.h"
#include <stdio.h>
#define TX_IN_MAX (32u*1024u*1024u)
#define TX_OUT_MAX (64u*1024u*1024u)
static dts89_u8 input[TX_IN_MAX],output[TX_OUT_MAX];
int main(int argc,char **argv)
{
 FILE *f,*o;long n;dts89_mod_texture_info info;dts89_u32 count;int rc;
 if(argc!=3){fputs("usage: dts89_texture_decode input.dds out.rgba\n",stderr);return 2;}
 f=fopen(argv[1],"rb");if(!f)return 2;
 if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
 n=ftell(f);if(n<0 || (unsigned long)n>TX_IN_MAX){fclose(f);return 2;}
 rewind(f);if(fread(input,1,(size_t)n,f)!=(size_t)n){fclose(f);return 2;}
 fclose(f);rc=dts89_texturer_opener_info(input,(dts89_u32)n,&info);
 if(rc){fprintf(stderr,"unsupported image %d\n",rc);return 3;}
 if(info.width>0xFFFFFFFFu/4u)return 3;
 if(info.height>0xFFFFFFFFu/(info.width*4u))return 3;
 count=info.height*info.width*4u;if(count>TX_OUT_MAX)return 3;
 rc=dts89_texturer_opener_decode_rgba8(input,(dts89_u32)n,&info,output,info.width*4u,count);
 if(rc){fprintf(stderr,"decode failure %d\n",rc);return 3;}
 o=fopen(argv[2],"wb");if(!o)return 2;
 if(fwrite(output,1,count,o)!=count){fclose(o);return 2;}
 fclose(o);printf("decode ok %lu x %lu format %lu bytes %lu\n",
 (unsigned long)info.width,(unsigned long)info.height,
 (unsigned long)info.encoding,(unsigned long)count);
 return 0;
}
