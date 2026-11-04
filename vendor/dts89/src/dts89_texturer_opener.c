#include "dts89_modules.h"
#include <string.h>
static dts89_u32 le32(const dts89_u8 *p) { return (dts89_u32)p[0]|((dts89_u32)p[1]<<8)|((dts89_u32)p[2]<<16)|((dts89_u32)p[3]<<24); }
static dts89_u32 be32(const dts89_u8 *p) { return ((dts89_u32)p[0]<<24)|((dts89_u32)p[1]<<16)|((dts89_u32)p[2]<<8)|(dts89_u32)p[3]; }
int dts89_texturer_opener_probe(const dts89_u8 *d,dts89_u32 n,dts89_mod_image *o)
{
 static const dts89_u8 png[8]={137,80,78,71,13,10,26,10};
 if(!d || !o) return DTS89_ERR_BAD_ARG;
 memset(o,0,sizeof(*o));
 if(n>=128u && memcmp(d,"DDS ",4)==0 && le32(d+4)==124u) { o->format=1; o->height=le32(d+12); o->width=le32(d+16); o->data_offset=128u; return DTS89_OK; }
 if(n>=54u && d[0]=='B' && d[1]=='M' && le32(d+14)>=40u) { o->format=2; o->width=le32(d+18); o->height=le32(d+22); o->data_offset=le32(d+10); return DTS89_OK; }
 if(n>=24u && memcmp(d,png,8)==0 && memcmp(d+12,"IHDR",4)==0) { o->format=3; o->width=be32(d+16); o->height=be32(d+20); o->data_offset=0u; return DTS89_OK; }
 return DTS89_ERR_RANGE;
}

/* Standalone image decoders for the two block encodings used by ZombiePack.
   Each 4x4 block is unpacked independently into caller-provided RGBA8. */
static dts89_u16 tex_le16(const dts89_u8 *p)
{ return (dts89_u16)((dts89_u16)p[0]|((dts89_u16)p[1]<<8)); }
static int tex_is_dds(const dts89_u8 *d,dts89_u32 n)
{ return n>=128u && d[0]=='D' && d[1]=='D' && d[2]=='S' && d[3]==' '; }
int dts89_texturer_opener_info(const dts89_u8 *d,dts89_u32 n,
 dts89_mod_texture_info *out)
{
 dts89_u32 w,h,bx,by,bits,offset,pitch,blocks;
 if(!d || !out)return DTS89_ERR_BAD_ARG;
 memset(out,0,sizeof(*out));
 if(tex_is_dds(d,n)){
  if(le32(d+4)!=124u || le32(d+76)!=32u)return DTS89_ERR_FORMAT;
  w=le32(d+16);h=le32(d+12);
  if(!w || !h || w>16384u || h>16384u)return DTS89_ERR_RANGE;
  if(!memcmp(d+84,"DXT1",4)){out->encoding=DTS89_TEX_BC1;bits=8u;}
  else if(!memcmp(d+84,"DXT5",4)){out->encoding=DTS89_TEX_BC3;bits=16u;}
  else return DTS89_ERR_UNSUPPORTED;
  bx=(w+3u)/4u;by=(h+3u)/4u;
  if(bx>0xFFFFFFFFu/by)return DTS89_ERR_RANGE;
  blocks=bx*by;
  if(blocks>0xFFFFFFFFu/bits)return DTS89_ERR_RANGE;
  out->level0_bytes=blocks*bits;
  if(out->level0_bytes>n-128u)return DTS89_ERR_TRUNCATED;
  out->level0_offset=128u;out->width=w;out->height=h;
  out->mip_count=le32(d+28);if(!out->mip_count)out->mip_count=1u;
  return DTS89_OK;
 }
 if(n>=54u && d[0]=='B' && d[1]=='M' && le32(d+14)>=40u){
  w=le32(d+18);h=le32(d+22);offset=le32(d+10);
  bits=tex_le16(d+28);
  if(!w || w>16384u || !h || h==0x80000000u)return DTS89_ERR_RANGE;
  if((bits!=24u && bits!=32u) || le32(d+30)!=0u)return DTS89_ERR_UNSUPPORTED;
  out->source_bottom_up=(h&0x80000000u)?0u:1u;
  if(h&0x80000000u)h=(~h)+1u;
  if(h>16384u)return DTS89_ERR_RANGE;
  if(w>(0xFFFFFFFFu-3u)/(bits/8u))return DTS89_ERR_RANGE;
  pitch=(w*(bits/8u)+3u)&~3u;
  if(pitch>0xFFFFFFFFu/h)return DTS89_ERR_RANGE;
  if(offset>n || pitch*h>n-offset)return DTS89_ERR_TRUNCATED;
  out->encoding=(bits==24u)?DTS89_TEX_BMP24:DTS89_TEX_BMP32;
  out->width=w;out->height=h;out->level0_offset=offset;
  out->level0_bytes=pitch*h;out->source_stride=pitch;
  out->mip_count=1u;return DTS89_OK;
 }
 return DTS89_ERR_UNSUPPORTED;
}
static void tex_rgb565(dts89_u16 v,dts89_u8 *out)
{
 dts89_u32 r=((dts89_u32)v>>11)&31u;
 dts89_u32 g=((dts89_u32)v>>5)&63u;
 dts89_u32 b=(dts89_u32)v&31u;
 out[0]=(dts89_u8)((r*255u+15u)/31u);
 out[1]=(dts89_u8)((g*255u+31u)/63u);
 out[2]=(dts89_u8)((b*255u+15u)/31u);
 out[3]=255u;
}
static void tex_bc_block(const dts89_u8 *block,dts89_u32 fmt,dts89_u8 *dst,
 dts89_u32 width,dts89_u32 height,dts89_u32 stride,
 dts89_u32 x0,dts89_u32 y0)
{
 dts89_u8 color[4][4],alpha[8];
 dts89_u32 i,x,y,bitcolor,a0,a1,lo,hi,sel,idx;
 const dts89_u8 *p;
 dts89_u16 c0,c1;
 p=block+(fmt==DTS89_TEX_BC3?8u:0u);
 c0=tex_le16(p);c1=tex_le16(p+2);
 tex_rgb565(c0,color[0]);tex_rgb565(c1,color[1]);
 if(c0>c1 || fmt==DTS89_TEX_BC3){
  for(i=0;i<3u;++i){
   color[2][i]=(dts89_u8)((2u*color[0][i]+color[1][i]+1u)/3u);
   color[3][i]=(dts89_u8)((color[0][i]+2u*color[1][i]+1u)/3u);
  }
  color[2][3]=color[3][3]=255u;
 }else{
  for(i=0;i<3u;++i){
   color[2][i]=(dts89_u8)((color[0][i]+color[1][i]+1u)/2u);
   color[3][i]=0u;
  }
  color[2][3]=255u;color[3][3]=0u;
 }
 bitcolor=le32(p+4);
 a0=0u;a1=0u;lo=0u;hi=0u;
 if(fmt==DTS89_TEX_BC3){
  a0=block[0];a1=block[1];alpha[0]=(dts89_u8)a0;alpha[1]=(dts89_u8)a1;
  if(a0>a1){
   for(i=2u;i<8u;++i)alpha[i]=(dts89_u8)(((8u-i)*a0+(i-1u)*a1+3u)/7u);
  }else{
   for(i=2u;i<6u;++i)alpha[i]=(dts89_u8)(((6u-i)*a0+(i-1u)*a1+2u)/5u);
   alpha[6]=0u;alpha[7]=255u;
  }
  lo=(dts89_u32)block[2]|((dts89_u32)block[3]<<8)|((dts89_u32)block[4]<<16);
  hi=(dts89_u32)block[5]|((dts89_u32)block[6]<<8)|((dts89_u32)block[7]<<16);
 }
 for(y=0u;y<4u;++y){
  if(y0+y>=height)break;
  for(x=0u;x<4u;++x){
   dts89_u8 *q;
   if(x0+x>=width)break;
   idx=y*4u+x;sel=(bitcolor>>(idx*2u))&3u;
   q=dst+(y0+y)*stride+(x0+x)*4u;
   for(i=0;i<4u;++i)q[i]=color[sel][i];
   if(fmt==DTS89_TEX_BC3){
    dts89_u32 ai=(idx<8u)?((lo>>(idx*3u))&7u):((hi>>((idx-8u)*3u))&7u);
    q[3]=alpha[ai];
   }
  }
 }
}
int dts89_texturer_opener_decode_rgba8(const dts89_u8 *data,dts89_u32 size,
 const dts89_mod_texture_info *info,dts89_u8 *rgba,dts89_u32 stride,
 dts89_u32 output_capacity)
{
 dts89_u32 w,h,x,y,bx,by,block_width,block_height,block_bytes;
 dts89_mod_texture_info verify;
 int rc;
 if(!data || !info || !rgba)return DTS89_ERR_BAD_ARG;
 rc=dts89_texturer_opener_info(data,size,&verify);if(rc)return rc;
 if(info->width!=verify.width || info->height!=verify.height ||
    info->encoding!=verify.encoding || info->level0_offset!=verify.level0_offset ||
    info->level0_bytes!=verify.level0_bytes)return DTS89_ERR_BAD_ARG;
 w=info->width;h=info->height;
 if(w>0xFFFFFFFFu/4u || stride<w*4u || h>0xFFFFFFFFu/stride ||
    stride*h>output_capacity)return DTS89_ERR_NO_SPACE;
 if(info->encoding==DTS89_TEX_BMP24 || info->encoding==DTS89_TEX_BMP32){
  for(y=0u;y<h;++y){
   dts89_u32 sy=info->source_bottom_up?h-1u-y:y;
   const dts89_u8 *p=data+info->level0_offset+sy*info->source_stride;
   dts89_u8 *q=rgba+y*stride;
   for(x=0u;x<w;++x){
    dts89_u32 sp=x*((info->encoding==DTS89_TEX_BMP24)?3u:4u);
    dts89_u32 dp=x*4u;
    q[dp]=p[sp+2u];q[dp+1u]=p[sp+1u];q[dp+2u]=p[sp];
    q[dp+3u]=(info->encoding==DTS89_TEX_BMP24)?255u:p[sp+3u];
   }
  }
  return DTS89_OK;
 }
 block_width=(w+3u)/4u;block_height=(h+3u)/4u;
 block_bytes=info->encoding==DTS89_TEX_BC1?8u:16u;
 for(by=0u;by<block_height;++by)for(bx=0u;bx<block_width;++bx){
  const dts89_u8 *p=data+info->level0_offset+(by*block_width+bx)*block_bytes;
  tex_bc_block(p,info->encoding,rgba,w,h,stride,bx*4u,by*4u);
 }
 return DTS89_OK;
}
