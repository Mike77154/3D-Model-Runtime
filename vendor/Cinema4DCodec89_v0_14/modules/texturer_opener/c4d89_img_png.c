#include "c4d89_img_internal.h"
/* Self contained PNG/zlib DEFLATE. C89, fixed buffers, no zlib dependency.
   8-bit PNG grayscale/RGB/indexed/gray-alpha/RGBA, non-interlaced.
   DEFLATE supports stored/fixed/dynamic Huffman blocks. */
#define PNG_CMAX 16777216ul
#define PNG_DMAX 33554432ul
static unsigned char png_zdata[PNG_CMAX];
static unsigned char png_raw[PNG_DMAX];
static unsigned char png_prev[16384];
static unsigned char png_cur[16384];
typedef struct PBit {
    const unsigned char *src;
    unsigned long len,pos;
    unsigned long bits;
    unsigned int have;
    int err;
} PBit;
typedef struct PHuff { unsigned int n[16];unsigned int symbols[320]; } PHuff;
static unsigned int pbits(PBit *s,unsigned int cnt)
{
    unsigned int v;
    if(cnt>16u){s->err=1;return 0u;}
    while(s->have<cnt){
        if(s->pos>=s->len){s->err=1;return 0u;}
        s->bits|=(unsigned long)s->src[s->pos++]<<s->have;
        s->have+=8u;
    }
    v=(unsigned int)(s->bits&((1ul<<cnt)-1ul));
    s->bits>>=cnt;s->have-=cnt;return v;
}
static int phuff_make(PHuff *h,const unsigned char *lens,unsigned int n)
{
    unsigned int i,off[16],total=0u,left=1u;
    memset(h,0,sizeof(*h));
    for(i=0u;i<n;i++){if(lens[i]>15u)return 0;h->n[lens[i]]++;}
    h->n[0]=0u;
    for(i=1u;i<=15u;i++){
        if(left>65535u/2u)return 0;
        left=left*2u;
        if(h->n[i]>left)return 0;
        left-=h->n[i];
    }
    for(i=1u;i<=15u;i++){off[i]=total;total+=h->n[i];}
    for(i=0u;i<n;i++)if(lens[i])h->symbols[off[lens[i]]++]=i;
    return 1;
}
static int phuff_read(PBit *s,const PHuff *h)
{
    unsigned int code=0u,first=0u,index=0u,len,ct;
    for(len=1u;len<=15u;len++){
        code|=pbits(s,1u);if(s->err)return -1;
        ct=h->n[len];
        if(code>=first && code-first<ct)return (int)h->symbols[index+code-first];
        index+=ct;first=(first+ct)<<1;code<<=1;
    }
    s->err=1;return -1;
}
static const unsigned int lensbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned int lensextra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned int distbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned int distextra[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned int corder[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
static int pinflate(const unsigned char *src,unsigned long srclen,
                    unsigned char *dst,unsigned long dstcap,unsigned long *outlen)
{
    PBit s;PHuff ll,dd,cc;
    unsigned char llens[288],dlens[32],clens[19],all[320];
    unsigned long n=0ul,adler1=1ul,adler2=0ul,expect;
    unsigned int last=0u,mode,nlit,ndist,nclen,i,code,repeat,val,extra,len,dist;
    if(srclen<6ul || (src[0]&15u)!=8u || ((unsigned int)src[0]*256u+src[1])%31u!=0u || (src[1]&32u))return C4D89_IMG_CORRUPT;
    expect=i32be(src+srclen-4ul);
    s.src=src+2;s.len=srclen-6ul;s.pos=0ul;s.bits=0ul;s.have=0u;s.err=0;
    while(!last && !s.err){
        last=pbits(&s,1u);mode=pbits(&s,2u);
        if(mode==0u){
            /* Stored block, byte aligned. */
            unsigned int sz,nz;
            s.bits=0ul;s.have=0u;
            if(s.pos+4ul>s.len)return C4D89_IMG_TRUNCATED;
            sz=(unsigned int)s.src[s.pos]|((unsigned int)s.src[s.pos+1ul]<<8);
            nz=(unsigned int)s.src[s.pos+2ul]|((unsigned int)s.src[s.pos+3ul]<<8);
            s.pos+=4ul;if((sz^nz)!=65535u)return C4D89_IMG_CORRUPT;
            if((unsigned long)sz>dstcap-n||s.pos+(unsigned long)sz>s.len)return C4D89_IMG_LIMIT;
            memcpy(dst+n,s.src+s.pos,sz);n+=(unsigned long)sz;s.pos+=(unsigned long)sz;
        } else if(mode==1u||mode==2u){
            if(mode==1u){
                for(i=0u;i<288u;i++)llens[i]=(unsigned char)(i<144u?8:(i<256u?9:(i<280u?7:8)));
                for(i=0u;i<32u;i++)dlens[i]=5u;
                if(!phuff_make(&ll,llens,288u)||!phuff_make(&dd,dlens,32u))return C4D89_IMG_CORRUPT;
            } else {
                nlit=pbits(&s,5u)+257u;ndist=pbits(&s,5u)+1u;nclen=pbits(&s,4u)+4u;
                if(nlit>286u||ndist>32u)return C4D89_IMG_CORRUPT;
                memset(clens,0,sizeof(clens));
                for(i=0u;i<nclen;i++)clens[corder[i]]=(unsigned char)pbits(&s,3u);
                if(!phuff_make(&cc,clens,19u))return C4D89_IMG_CORRUPT;
                i=0u;
                while(i<nlit+ndist && !s.err){
                    code=(unsigned int)phuff_read(&s,&cc);
                    if(code<=15u)all[i++]=(unsigned char)code;
                    else if(code==16u && i>0u){repeat=pbits(&s,2u)+3u;val=all[i-1u];
                        if(repeat>nlit+ndist-i)return C4D89_IMG_CORRUPT;
                        while(repeat--)all[i++]=(unsigned char)val;
                    } else if(code==17u||code==18u){repeat=code==17u?pbits(&s,3u)+3u:pbits(&s,7u)+11u;
                        if(repeat>nlit+ndist-i)return C4D89_IMG_CORRUPT;
                        while(repeat--)all[i++]=0u;
                    } else return C4D89_IMG_CORRUPT;
                }
                memset(llens,0,sizeof(llens));memset(dlens,0,sizeof(dlens));
                memcpy(llens,all,nlit);memcpy(dlens,all+nlit,ndist);
                if(!phuff_make(&ll,llens,288u)||!phuff_make(&dd,dlens,32u))return C4D89_IMG_CORRUPT;
            }
            while(!s.err){
                int symbol=phuff_read(&s,&ll);
                if(symbol<0)return C4D89_IMG_TRUNCATED;
                if(symbol<256){if(n>=dstcap)return C4D89_IMG_LIMIT;dst[n++]=(unsigned char)symbol;}
                else if(symbol==256)break;
                else {
                    if(symbol>285)return C4D89_IMG_CORRUPT;
                    code=(unsigned int)symbol-257u;
                    extra=pbits(&s,lensextra[code]);len=lensbase[code]+extra;
                    symbol=phuff_read(&s,&dd);
                    if(symbol<0||symbol>=30)return C4D89_IMG_CORRUPT;
                    code=(unsigned int)symbol;
                    extra=pbits(&s,distextra[code]);dist=distbase[code]+extra;
                    if(dist==0u||(unsigned long)dist>n||(unsigned long)len>dstcap-n)return C4D89_IMG_CORRUPT;
                    while(len--){dst[n]=dst[n-dist];n++;}
                }
            }
        } else return C4D89_IMG_CORRUPT;
    }
    if(s.err)return C4D89_IMG_TRUNCATED;
    {unsigned long q;for(q=0ul;q<n;q++){
        adler1+=dst[q];adler2+=adler1;
        if((q&4095ul)==4095ul){adler1%=65521ul;adler2%=65521ul;}
    }}
    adler1%=65521ul;adler2%=65521ul;
    if((adler2<<16|adler1)!=expect)return C4D89_IMG_CORRUPT;
    *outlen=n;return C4D89_IMG_OK;
}
static unsigned long pcrc32(const unsigned char *data,unsigned long n,unsigned long prior)
{
    unsigned long crc=prior;unsigned long i;unsigned int j;
    for(i=0ul;i<n;i++){
        crc^=data[i];for(j=0u;j<8u;j++)crc=crc&1ul?(crc>>1)^0xedb88320ul:crc>>1;
    }
    return crc;
}
static unsigned int ppaeth(unsigned int a,unsigned int b,unsigned int c)
{
    int p=(int)a+(int)b-(int)c,da=p-(int)a,db=p-(int)b,dc=p-(int)c;
    if(da<0)da=-da;
    if(db<0)db=-db;
    if(dc<0)dc=-dc;
    if(da<=db&&da<=dc)return a;
    if(db<=dc)return b;
    return c;
}
int c4d89_img_png(const unsigned char *src,unsigned long n,unsigned char *rgba,
                  unsigned long cap,C4D89_ImgInfo *info)
{
    unsigned long pos=8ul,w=0ul,h=0ul,zlen=0ul,rawlen=0ul,needed,rowbytes=0ul,y,x,p,q;
    unsigned int bitdepth=0u,color=0u,ch=0u,bpp=0u,filter,r,g,b,a,px,paln=0u,has_ihdr=0u,ended=0u;
    unsigned int trns=0u,tr=0u,tg=0u,tb=0u;
    unsigned char pal[256][4];int res;
    if(n<33ul||memcmp(src,"\211PNG\r\n\032\n",8u)!=0)return C4D89_IMG_INVALID;
    for(x=0ul;x<256ul;x++){pal[x][0]=pal[x][1]=pal[x][2]=0u;pal[x][3]=255u;}
    while(pos+12ul<=n){
        unsigned long sz=i32be(src+pos),crc,stored;const unsigned char *name,*body;
        if(sz>n-pos-12ul)return C4D89_IMG_TRUNCATED;
        name=src+pos+4ul;body=src+pos+8ul;
        crc=pcrc32(name,sz+4ul,0xfffffffful)^0xfffffffful;
        stored=i32be(body+sz);
        if(crc!=stored)return C4D89_IMG_CORRUPT;
        if(memcmp(name,"IHDR",4u)==0){
            if(has_ihdr||sz!=13ul)return C4D89_IMG_CORRUPT;
            w=i32be(body);h=i32be(body+4ul);bitdepth=body[8];color=body[9];
            if(body[10]!=0u||body[11]!=0u||body[12]!=0u)return C4D89_IMG_UNSUPPORTED;
            if(bitdepth!=8u)return C4D89_IMG_UNSUPPORTED;
            if(color==0u)ch=1u;else if(color==2u)ch=3u;
            else if(color==3u)ch=1u;else if(color==4u)ch=2u;
            else if(color==6u)ch=4u;else return C4D89_IMG_UNSUPPORTED;
            bpp=ch;has_ihdr=1u;
            if(!size_ok(w,h,cap)||w>16384ul/(unsigned long)ch)return C4D89_IMG_LIMIT;
            rowbytes=w*ch;
            if(h>(PNG_DMAX/(rowbytes+1ul)))return C4D89_IMG_LIMIT;
        } else if(memcmp(name,"PLTE",4u)==0){
            if(sz%3ul!=0ul||sz>768ul)return C4D89_IMG_CORRUPT;
            paln=(unsigned int)(sz/3ul);
            for(x=0ul;x<paln;x++){pal[x][0]=body[3ul*x];pal[x][1]=body[3ul*x+1ul];pal[x][2]=body[3ul*x+2ul];}
        } else if(memcmp(name,"tRNS",4u)==0){
            if(color==3u){if(sz>256ul)return C4D89_IMG_CORRUPT;
                for(x=0ul;x<sz;x++)pal[x][3]=body[x];
            } else if(color==0u && sz==2ul){
                trns=1u;tr=body[1];
            } else if(color==2u && sz==6ul){
                trns=1u;tr=body[1];tg=body[3];tb=body[5];
            } else return C4D89_IMG_UNSUPPORTED;
        } else if(memcmp(name,"IDAT",4u)==0){
            if(!has_ihdr||sz>PNG_CMAX-zlen)return C4D89_IMG_LIMIT;
            memcpy(png_zdata+zlen,body,(size_t)sz);zlen+=sz;
        } else if(memcmp(name,"IEND",4u)==0){ended=1u;break;}
        else if(!(name[0]&0x20u))return C4D89_IMG_UNSUPPORTED;
        pos+=12ul+sz;
    }
    if(!has_ihdr||!ended||!zlen||(color==3u&&!paln))return C4D89_IMG_CORRUPT;
    needed=h*(rowbytes+1ul);
    res=pinflate(png_zdata,zlen,png_raw,PNG_DMAX,&rawlen);
    if(res)return res;
    if(rawlen!=needed)return C4D89_IMG_CORRUPT;
    memset(png_prev,0,(size_t)rowbytes);
    pos=0ul;
    for(y=0ul;y<h;y++){
        filter=png_raw[pos++];if(filter>4u)return C4D89_IMG_CORRUPT;
        for(x=0ul;x<rowbytes;x++){
            unsigned int left=x>=bpp?png_cur[x-bpp]:0u;
            unsigned int up=png_prev[x];unsigned int ul=x>=bpp?png_prev[x-bpp]:0u;
            unsigned int predictor=filter==0u?0u:(filter==1u?left:(filter==2u?up:(filter==3u?(left+up)/2u:ppaeth(left,up,ul))));
            png_cur[x]=(unsigned char)((unsigned int)png_raw[pos++]+predictor);
        }
        for(x=0ul;x<w;x++){
            p=x*ch;q=(y*w+x)*4ul;
            r=g=b=0u;a=255u;
            if(color==0u){r=g=b=png_cur[p];}
            else if(color==2u){r=png_cur[p];g=png_cur[p+1ul];b=png_cur[p+2ul];}
            else if(color==3u){px=png_cur[p];if(px>=paln)return C4D89_IMG_CORRUPT;
                r=pal[px][0];g=pal[px][1];b=pal[px][2];a=pal[px][3];
            } else if(color==4u){r=g=b=png_cur[p];a=png_cur[p+1ul];}
            else {r=png_cur[p];g=png_cur[p+1ul];b=png_cur[p+2ul];a=png_cur[p+3ul];}
            if(trns && ((color==0u && r==tr) || (color==2u && r==tr && g==tg && b==tb)))a=0u;
            rgba[q]=(unsigned char)r;rgba[q+1ul]=(unsigned char)g;
            rgba[q+2ul]=(unsigned char)b;rgba[q+3ul]=(unsigned char)a;
        }
        memcpy(png_prev,png_cur,(size_t)rowbytes);
    }
    info->width=(unsigned int)w;info->height=(unsigned int)h;info->format=C4D89_IMG_FORMAT_PNG;
    info->source_channels=ch;return C4D89_IMG_OK;
}
