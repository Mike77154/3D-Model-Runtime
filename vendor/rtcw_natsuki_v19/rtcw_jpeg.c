/* Original baseline JPEG/JFIF subset decoder, CC0-1.0, strict ISO C89.
   8-bit single-scan baseline SOF0, 1 or 3 components, h/v sampling 1..2,
   8-bit DQT, standard Huffman. No heap, float, long long or dependencies.
   Rejects progressive, arithmetic, CMYK, restart interval JPEGs, oversized files. */
#include "rtcw_texture.h"
#include <stdio.h>
#include <string.h>
#define JPEG_CAP (8UL*1024UL*1024UL)
static unsigned char jd[JPEG_CAP];
static unsigned long jsize,jpos;
static char jpeg_error[160];
static const unsigned char zig[64]={
0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
/* PRECALCULATED integer C(u)*cos((2*x+1)*u*pi/16)*256.
   This is static literal data: no floating-point in compiled C. */
static const short basis[8][8]={
  {181,181,181,181,181,181,181,181},
  {251,213,142,50,-50,-142,-213,-251},
  {237,98,-98,-237,-237,-98,98,237},
  {213,-50,-251,-142,142,251,50,-213},
  {181,-181,-181,181,181,-181,-181,181},
  {142,-251,50,213,-213,-50,251,-142},
  {98,-237,237,-98,-98,237,-237,98},
  {50,-142,213,-251,251,-213,142,-50}
};

typedef struct jhuff_s {unsigned char lens[16],symbols[256];unsigned int total;int valid;} jhuff;
typedef struct jcomponent_s {unsigned int id,h,v,q,dc,ac;long predict;} jcomponent;
static jhuff ht[2][4];
static unsigned int quant[4][64];
static int qvalid[4];
static jcomponent jc[3];
static unsigned int jwidth,jheight,jcomponents,jmaxh,jmaxv;
static unsigned char sample[3][32*32];
static unsigned long bitpos;static int bitremain;static unsigned int bitbyte;
const char *rtcw_jpeg_error(void){return jpeg_error;}
static int jerr(const char *msg){strncpy(jpeg_error,msg,159U);jpeg_error[159U]=0;rtcw_texture_current.width=0;rtcw_texture_current.height=0;return 0;}
static unsigned int be16(unsigned long p){return ((unsigned int)jd[p]<<8)|(unsigned int)jd[p+1U];}
static int segment(unsigned long at,unsigned long size){return at<=jsize && size<=jsize-at;}
static int readbits(unsigned int count,int *out){
    unsigned int n,value=0;
    if(count>16U)return 0;
    for(n=0;n<count;n++){
        if(bitremain==0){
            if(bitpos>=jsize)return 0;
            bitbyte=jd[bitpos++];
            if(bitbyte==255U){
                if(bitpos>=jsize||jd[bitpos++]!=0U)return 0;
            }
            bitremain=8;
        }
        bitremain--;value=(value<<1)|((bitbyte>>(unsigned int)bitremain)&1U);
    }
    *out=(int)value;return 1;
}
static int hdecode(const jhuff *h,int *symbol){
    unsigned int len,i=0,j;int code=0,b,first=0;
    if(!h->valid)return 0;
    for(len=1U;len<=16U;len++){
        if(!readbits(1U,&b))return 0;
        code=(code<<1)|b;
        for(j=0;j<h->lens[len-1U];j++){
            if(code==first+(int)j){
                if(i+j>=h->total)return 0;
                *symbol=(int)h->symbols[i+j];return 1;
            }
        }
        i+=h->lens[len-1U];first=(first+(int)h->lens[len-1U])<<1;
    }
    return 0;
}
static int category_value(unsigned int n,int *value){
    int v,threshold;
    if(n>11U||!readbits(n,&v))return 0;
    if(n==0U){*value=0;return 1;}
    threshold=1<<(n-1U);
    if(v<threshold)v-=((1<<(unsigned int)n)-1);
    *value=v;return 1;
}
static int idct_block(const long coeff[64],unsigned char *dest,unsigned int stride){
    long temp[8][8],sum,pixel;unsigned int x,y,u,v;
    for(v=0;v<8U;v++)for(x=0;x<8U;x++){
        sum=0;
        for(u=0;u<8U;u++)sum+=coeff[v*8U+u]*(long)basis[u][x];
        temp[v][x]=sum/256L;
    }
    for(y=0;y<8U;y++)for(x=0;x<8U;x++){
        sum=0;
        for(v=0;v<8U;v++)sum+=temp[v][x]*(long)basis[v][y];
        pixel=128L+(sum/256L)/4L;
        if(pixel<0L)pixel=0L;
        if(pixel>255L)pixel=255L;
        dest[y*stride+x]=(unsigned char)pixel;
    }
    return 1;
}
static int block(jcomponent *c,unsigned char *dest,unsigned int stride){
    long coeff[64];int sym,diff,value;unsigned int k,i,n;
    memset(coeff,0,sizeof(coeff));
    if(!hdecode(&ht[0][c->dc],&sym)||sym<0||sym>11||!category_value((unsigned int)sym,&diff))return 0;
    c->predict+=(long)diff;
    if(c->predict>8192L||c->predict< -8192L)return 0;
    coeff[0]=c->predict*(long)quant[c->q][0];
    k=1U;
    while(k<64U){
        if(!hdecode(&ht[1][c->ac],&sym))return 0;
        if(sym==0)break;
        if(sym==240){k+=16U;continue;}
        n=(unsigned int)(sym&15);
        if(n==0U||n>10U)return 0;
        k+=(unsigned int)(sym>>4);
        if(k>=64U||!category_value(n,&value))return 0;
        coeff[zig[k]]=(long)value*(long)quant[c->q][zig[k]];
        k++;
    }
    /* Prevent 32-bit overflows in both integer IDCT passes. */
    for(i=0;i<64U;i++)if(coeff[i]>32767L||coeff[i]< -32767L)return 0;
    return idct_block(coeff,dest,stride);
}
static int parse_dqt(unsigned long start,unsigned long end){
    unsigned long p=start;unsigned int i,q,n;
    while(p<end){
        q=(unsigned int)jd[p++];n=q>>4;q&=15U;
        if(q>=4U||n!=0U||p+64U>end)return 0;
        for(i=0;i<64U;i++){quant[q][zig[i]]=(unsigned int)jd[p++];if(!quant[q][zig[i]])return 0;}
        qvalid[q]=1;
    }
    return p==end;
}
static int parse_dht(unsigned long start,unsigned long end){
    unsigned long p=start;unsigned int t,k,i,n,sum;
    while(p<end){
        t=(unsigned int)jd[p++];k=t>>4;t&=15U;
        if(k>1U||t>=4U||p+16U>end)return 0;
        sum=0U;
        for(i=0;i<16U;i++){ht[k][t].lens[i]=jd[p++];sum+=ht[k][t].lens[i];}
        if(sum==0U||sum>256U||p+sum>end)return 0;
        for(n=0;n<sum;n++)ht[k][t].symbols[n]=jd[p++];
        ht[k][t].total=sum;ht[k][t].valid=1;
    }
    return p==end;
}
static int parse_sof(unsigned long start,unsigned long end){
    unsigned long p=start;unsigned int i,j,id,samp;
    if(end-start<6U||jd[p++]!=8U)return 0;
    jheight=be16(p);p+=2U;jwidth=be16(p);p+=2U;
    jcomponents=(unsigned int)jd[p++];
    if((jcomponents!=1U&&jcomponents!=3U)||p+3U*jcomponents!=end||
       !jwidth||!jheight||jwidth>RTCW_TEXTURE_MAX_DIM||jheight>RTCW_TEXTURE_MAX_DIM||
       (unsigned long)jwidth*(unsigned long)jheight>RTCW_TEXTURE_MAX_PIXELS)return 0;
    jmaxh=jmaxv=1U;
    for(i=0;i<jcomponents;i++){
        id=(unsigned int)jd[p++];samp=(unsigned int)jd[p++];
        jc[i].id=id;jc[i].h=samp>>4;jc[i].v=samp&15U;
        jc[i].q=(unsigned int)jd[p++];jc[i].predict=0L;
        if(jc[i].q>=4U||jc[i].h<1U||jc[i].h>2U||jc[i].v<1U||jc[i].v>2U)return 0;
        for(j=0;j<i;j++)if(jc[j].id==id)return 0;
        if(jmaxh<jc[i].h)jmaxh=jc[i].h;
        if(jmaxv<jc[i].v)jmaxv=jc[i].v;
    }
    return 1;
}
static unsigned char clamp(long x){return (unsigned char)(x<0L?0L:(x>255L?255L:x));}
static int scan(unsigned long start,unsigned long end){
    unsigned long pos,pix;unsigned int i,s,ci,which,dc,ac,mx,my,blocksx,blocksy,bx,by,x,y,ix,iy;
    unsigned int imw,imh,sx,sy;long yy,cb,cr,r,g,b;
    if(end-start<1U)return 0;
    s=(unsigned int)jd[start++];if(s!=jcomponents||start+2U*s+3U!=end)return 0;
    for(i=0;i<s;i++){
        ci=(unsigned int)jd[start++];which=(unsigned int)jd[start++];
        dc=which>>4;ac=which&15U;
        if(dc>=4U||ac>=4U||!ht[0][dc].valid||!ht[1][ac].valid)return 0;
        for(which=0;which<jcomponents;which++)if(jc[which].id==ci)break;
        if(which>=jcomponents)return 0;
        jc[which].dc=dc;jc[which].ac=ac;
    }
    if(jd[start++]!=0U||jd[start++]!=63U||jd[start]!=0U)return 0;
    for(i=0;i<jcomponents;i++)if(!qvalid[jc[i].q])return 0;
    bitpos=end;bitremain=0;bitbyte=0U;
    imw=jmaxh*8U;imh=jmaxv*8U;
    blocksx=(jwidth+imw-1U)/imw;
    blocksy=(jheight+imh-1U)/imh;
    for(my=0U;my<blocksy;my++)for(mx=0U;mx<blocksx;mx++){
        for(ci=0U;ci<jcomponents;ci++){
            memset(sample[ci],0,sizeof(sample[ci]));
            for(by=0U;by<jc[ci].v;by++)for(bx=0U;bx<jc[ci].h;bx++){
                unsigned char *dest=&sample[ci][by*8U*(jc[ci].h*8U)+bx*8U];
                if(!block(&jc[ci],dest,jc[ci].h*8U))return 0;
            }
        }
        for(y=0U;y<imh;y++)for(x=0U;x<imw;x++){
            ix=mx*imw+x;iy=my*imh+y;
            if(ix>=jwidth||iy>=jheight)continue;
            pos=(unsigned long)iy*(unsigned long)jwidth+(unsigned long)ix;pix=pos*4UL;
            sy=y*jc[0].v/jmaxv;sx=x*jc[0].h/jmaxh;
            yy=(long)sample[0][sy*jc[0].h*8U+sx];
            if(jcomponents==1U){r=g=b=yy;}
            else{
                sy=y*jc[1].v/jmaxv;sx=x*jc[1].h/jmaxh;
                cb=(long)sample[1][sy*jc[1].h*8U+sx]-128L;
                sy=y*jc[2].v/jmaxv;sx=x*jc[2].h/jmaxh;
                cr=(long)sample[2][sy*jc[2].h*8U+sx]-128L;
                r=yy+(1436L*cr)/1024L;
                g=yy-(352L*cb+731L*cr)/1024L;
                b=yy+(1815L*cb)/1024L;
            }
            rtcw_texture_current.pixel[pix]=clamp(r);
            rtcw_texture_current.pixel[pix+1U]=clamp(g);
            rtcw_texture_current.pixel[pix+2U]=clamp(b);
            rtcw_texture_current.pixel[pix+3U]=255U;
        }
    }
    /* The entropy stream must terminate in EOI, not just run out of bytes. */
    if(bitpos>=jsize||jd[bitpos++]!=255U)return 0;
    while(bitpos<jsize&&jd[bitpos]==255U)bitpos++;
    if(bitpos>=jsize||jd[bitpos]!=217U)return 0;
    rtcw_texture_current.width=jwidth;rtcw_texture_current.height=jheight;return 1;
}
int rtcw_texture_jpeg(const char *filename){
    FILE *f;unsigned long start,end;unsigned int marker,len;
    int seen_sof=0,seen_scan=0;
    rtcw_texture_current.width=0;rtcw_texture_current.height=0;
    if(!filename)return jerr("missing JPEG filename");
    f=fopen(filename,"rb");if(!f)return jerr("cannot open JPEG");
    jsize=(unsigned long)fread(jd,1,JPEG_CAP,f);
    if(ferror(f)||!feof(f)){fclose(f);return jerr("JPEG larger than fixed buffer");}
    if(fclose(f)!=0)return jerr("JPEG close failed");
    if(jsize<4U||jd[0]!=255U||jd[1]!=216U)return jerr("not JPEG SOI");
    memset(ht,0,sizeof(ht));memset(qvalid,0,sizeof(qvalid));
    memset(jc,0,sizeof(jc));jwidth=jheight=jcomponents=0U;
    jpos=2U;
    while(jpos<jsize){
        if(jd[jpos++]!=255U)return jerr("invalid JPEG marker boundary");
        while(jpos<jsize&&jd[jpos]==255U)jpos++;
        if(jpos>=jsize)return jerr("truncated JPEG marker");
        marker=(unsigned int)jd[jpos++];
        if(marker==217U)return seen_scan?1:jerr("JPEG missing scan");
        if(marker==216U||marker==1U|| (marker>=208U&&marker<=215U))return jerr("unexpected JPEG standalone marker");
        if(!segment(jpos,2U))return jerr("short JPEG segment");
        len=be16(jpos);if(len<2U||!segment(jpos,(unsigned long)len))return jerr("JPEG segment exceeds file");
        start=jpos+2U;end=jpos+(unsigned long)len;jpos=end;
        if(marker==219U){if(!parse_dqt(start,end))return jerr("invalid JPEG quantization table");}
        else if(marker==196U){if(!parse_dht(start,end))return jerr("invalid JPEG Huffman table");}
        else if(marker==192U){if(seen_sof||!parse_sof(start,end))return jerr("unsupported baseline SOF0");seen_sof=1;}
        else if(marker==218U){
            if(!seen_sof||!scan(start,end))return jerr("JPEG entropy scan invalid or unsupported");
            seen_scan=1;break;
        }else if(marker==221U){if(end-start!=2U||be16(start)!=0U)return jerr("JPEG restart intervals unsupported");}
        else if((marker>=192U&&marker<=207U)&&marker!=196U)return jerr("unsupported JPEG encoding (progressive/arithmetic/etc)");
    }
    if(!seen_scan)return jerr("JPEG has no decodable scan");
    return 1;
}
