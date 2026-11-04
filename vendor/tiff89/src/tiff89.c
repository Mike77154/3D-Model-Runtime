#include "tiff89.h"
#include "zlib89.h"
#include <string.h>

typedef struct tt_s { unsigned short type; unsigned int count; unsigned int val; int ok; } tt;
typedef struct tif_s { const unsigned char *s; size_t n; int le; } tif;
typedef struct td_s {
    tt W,H,B,C,P,SO,SP,RPS,SBC,PR,PL,CM,TW,TH,TO,TBC,EX;
    size_t next_ifd;
} td;

static unsigned int u16(const tif *t, size_t p)
{
    if (p + 2u > t->n) return 0u;
    return t->le ? ((unsigned int)t->s[p] | ((unsigned int)t->s[p + 1u] << 8)) :
                   (((unsigned int)t->s[p] << 8) | (unsigned int)t->s[p + 1u]);
}
static unsigned int u32(const tif *t, size_t p)
{
    if (p + 4u > t->n) return 0u;
    if (t->le) return u16(t,p) | (u16(t,p + 2u) << 16);
    return (u16(t,p) << 16) | u16(t,p + 2u);
}
static unsigned int tsz(unsigned int type)
{
    return type == 1u || type == 2u ? 1u : (type == 3u ? 2u : (type == 4u ? 4u : 0u));
}
static unsigned int tv(const tif *t, const tt *a, unsigned int idx)
{
    unsigned int sz;
    size_t p;
    sz = tsz(a->type);
    if (!a->ok || idx >= a->count || sz == 0u) return 0u;
    p = (size_t)a->val + (size_t)idx * sz;
    if (p + sz > t->n) return 0u;
    if (a->type == 1u) return t->s[p];
    if (a->type == 3u) return u16(t,p);
    return u32(t,p);
}
static void settag(tt *a, const tif *t, size_t e, unsigned int type, unsigned int count)
{
    unsigned int sz;
    sz = tsz(type);
    a->type = (unsigned short)type; a->count = count; a->ok = 1;
    if (sz != 0u && (size_t)sz * count <= 4u) a->val = (unsigned int)(e + 8u);
    else a->val = u32(t,e + 8u);
}
static int parse_ifd(tif *t, size_t off, td *d)
{
    unsigned int cnt, i;
    memset(d, 0, sizeof(*d));
    if (off == 0u || off + 2u > t->n) return 0;
    cnt = u16(t,off);
    if (off + 2u + (size_t)cnt * 12u + 4u > t->n) return 0;
    for (i = 0u; i < cnt; ++i) {
        size_t e;
        unsigned int tag, type, count;
        tt *a;
        e = off + 2u + (size_t)i * 12u;
        tag = u16(t,e); type = u16(t,e + 2u); count = u32(t,e + 4u); a = 0;
        if (tag == 256u) a=&d->W; else if (tag == 257u) a=&d->H; else if (tag == 258u) a=&d->B;
        else if (tag == 259u) a=&d->C; else if (tag == 262u) a=&d->P; else if (tag == 273u) a=&d->SO;
        else if (tag == 277u) a=&d->SP; else if (tag == 278u) a=&d->RPS; else if (tag == 279u) a=&d->SBC;
        else if (tag == 284u) a=&d->PL; else if (tag == 317u) a=&d->PR; else if (tag == 320u) a=&d->CM;
        else if (tag == 322u) a=&d->TW; else if (tag == 323u) a=&d->TH; else if (tag == 324u) a=&d->TO;
        else if (tag == 325u) a=&d->TBC; else if (tag == 338u) a=&d->EX;
        if (a) settag(a,t,e,type,count);
    }
    d->next_ifd = u32(t, off + 2u + (size_t)cnt * 12u);
    return 1;
}
static int page_at(tif *t, unsigned int index, size_t *out_off, td *out_td)
{
    size_t off;
    unsigned int i;
    td d;
    off = u32(t,4u);
    for (i = 0u; i <= index; ++i) {
        if (off == 0u || !parse_ifd(t,off,&d)) return 0;
        if (i == index) { if (out_off) *out_off = off; if (out_td) *out_td = d; return 1; }
        if (d.next_ifd == off) return 0;
        off = d.next_ifd;
    }
    return 0;
}
static unsigned int page_count(tif *t)
{
    size_t off;
    unsigned int count;
    td d;
    off = u32(t,4u); count = 0u;
    while (off != 0u && count < 1024u) {
        if (!parse_ifd(t,off,&d)) break;
        ++count;
        if (d.next_ifd == off) break;
        off = d.next_ifd;
    }
    return count;
}
static int packbits(const unsigned char *s, size_t n, unsigned char *d, size_t cap, size_t *out)
{
    size_t p, o;
    p = o = 0u;
    while (p < n && o < cap) {
        unsigned int b;
        int k;
        b = s[p++]; k = b < 128u ? (int)b : (int)b - 256;
        if (k >= 0 && k <= 127) {
            unsigned int c;
            c = (unsigned int)k + 1u;
            if (p + c > n || o + c > cap) return 0;
            while (c--) d[o++] = s[p++];
        } else if (k >= -127 && k <= -1) {
            unsigned int c; unsigned char v;
            c = (unsigned int)(1 - k); if (p >= n || o + c > cap) return 0;
            v = s[p++]; while (c--) d[o++] = v;
        }
    }
    *out = o; return 1;
}
typedef struct lb_s { const unsigned char *s; size_t n,p; unsigned int buf,bits; } lb;
static unsigned int lbits(lb *b, unsigned int n)
{
    unsigned int v;
    while (b->bits < n) {
        if (b->p >= b->n) return 0xffffffffu;
        b->buf = (b->buf << 8) | b->s[b->p++]; b->bits += 8u;
    }
    v = (b->buf >> (b->bits - n)) & ((1u << n) - 1u); b->bits -= n;
    if (b->bits == 0u) b->buf = 0u; else b->buf &= (1u << b->bits) - 1u;
    return v;
}
static int lzw(const unsigned char *s,size_t n,unsigned char *d,size_t cap,size_t *out)
{
    unsigned short pre[4096]; unsigned char suf[4096],stack[4096]; lb b;
    unsigned int next,bitsn,old,first,o;
    next=258u; bitsn=9u; old=0xffffffffu; first=0u; o=0u;
    b.s=s;b.n=n;b.p=0u;b.buf=0u;b.bits=0u;
    for (;;) {
        unsigned int code,in,sp;
        code=lbits(&b,bitsn); sp=0u;
        if(code==0xffffffffu)break;
        if(code==256u){next=258u;bitsn=9u;old=0xffffffffu;continue;}
        if(code==257u)break;
        in=code;
        if(old==0xffffffffu){if(code>255u||o>=cap)return 0;d[o++]=(unsigned char)code;first=code;old=code;continue;}
        if(code>=next){if(code!=next||sp>=4095u)return 0;stack[sp++]=(unsigned char)first;code=old;}
        while(code>=258u){if(code>=next||sp>=4095u)return 0;stack[sp++]=suf[code];code=pre[code];}
        if(code>255u||sp>=4096u)return 0;
        first=code;stack[sp++]=(unsigned char)first;
        while(sp){if(o>=cap)return 0;d[o++]=stack[--sp];}
        if(next<4096u){pre[next]=(unsigned short)old;suf[next]=(unsigned char)first;next++;if(bitsn<12u&&next==((1u<<bitsn)-1u))bitsn++;}
        old=in;
    }
    *out=o; return 1;
}
static int unpack(unsigned int comp, const unsigned char *s, size_t n,
                  unsigned char *d, size_t want)
{
    size_t got, k;
    got = 0u;
    if (comp == 1u) {
        if (n < want) return 0;
        for (k = 0u; k < want; ++k) d[k] = s[k];
        return 1;
    }
    if (comp == 32773u) return packbits(s,n,d,want,&got) && got == want;
    if (comp == 5u) return lzw(s,n,d,want,&got) && got == want;
    if (comp == 8u || comp == 32946u) return zlib89_inflate_memory(s,n,d,want,&got) == 0 && got == want;
    return 0;
}
static void predictor(unsigned char *r, unsigned int rows, unsigned int row_samples,
                      unsigned int stride)
{
    unsigned int y;
    for (y = 0u; y < rows; ++y) {
        unsigned int k;
        unsigned char *row;
        row = r + (size_t)y * row_samples;
        for (k = stride; k < row_samples; ++k) row[k] = (unsigned char)(row[k] + row[k - stride]);
    }
}
static int put_pixel(unsigned char *d, unsigned int w, unsigned int x, unsigned int y,
                     unsigned int photo, unsigned int spp, const unsigned char *v,
                     const tif *t, const td *td)
{
    size_t q;
    unsigned char R,G,B,A;
    q=((size_t)y*w+x)*4u; R=G=B=0u; A=255u;
    if(photo==0u||photo==1u){unsigned char g;g=v[0];if(photo==0u)g=(unsigned char)(255u-g);R=G=B=g;if(spp>=2u)A=v[1];}
    else if(photo==2u){if(spp<3u)return 0;R=v[0];G=v[1];B=v[2];if(spp>=4u)A=v[3];}
    else if(photo==3u){unsigned int idx,colors;idx=v[0];colors=1u<<(td->B.ok?tv(t,&td->B,0):8u);if(!td->CM.ok||idx>=colors||td->CM.count<colors*3u)return 0;R=(unsigned char)(tv(t,&td->CM,idx)>>8);G=(unsigned char)(tv(t,&td->CM,colors+idx)>>8);B=(unsigned char)(tv(t,&td->CM,colors*2u+idx)>>8);}
    else return 0;
    d[q]=R;d[q+1u]=G;d[q+2u]=B;d[q+3u]=A;return 1;
}
static void put_plane(unsigned char *d,unsigned int w,unsigned int x,unsigned int y,
                      unsigned int photo,unsigned int plane,unsigned char v)
{
    size_t q;
    q=((size_t)y*w+x)*4u;
    if(photo==0u||photo==1u){if(plane==0u){if(photo==0u)v=(unsigned char)(255u-v);d[q]=d[q+1u]=d[q+2u]=v;}else if(plane==1u)d[q+3u]=v;}
    else if(photo==2u){if(plane<3u)d[q+plane]=v;else if(plane==3u)d[q+3u]=v;}
}

int tiff89_probe(const unsigned char *s,size_t n)
{
    if(s==0||n<8u)return 0;
    return (s[0]=='I'&&s[1]=='I'&&s[2]==42&&s[3]==0)||(s[0]=='M'&&s[1]=='M'&&s[2]==0&&s[3]==42);
}
static int setup(const unsigned char *s,size_t n,tif *t)
{
    if(!tiff89_probe(s,n))return 0;
    t->s=s;t->n=n;t->le=s[0]=='I';return u16(t,2u)==42u;
}
int tiff89_get_info(const unsigned char *s,size_t n,tiff89_info *o)
{
    tif t; td d; unsigned int pc,w,h,flags;
    if(o==0||!setup(s,n,&t))return -1;
    pc=page_count(&t);if(pc==0u||!page_at(&t,0u,0,&d))return -2;
    w=tv(&t,&d.W,0);h=tv(&t,&d.H,0);flags=0u;if(pc>1u)flags|=TIFF89_FLAG_MULTIPAGE;if(d.TO.ok&&d.TBC.ok)flags|=TIFF89_FLAG_TILED;if(d.PL.ok&&tv(&t,&d.PL,0)==2u)flags|=TIFF89_FLAG_PLANAR;
    o->width=w;o->height=h;o->page_count=pc;o->subresource_count=pc;o->flags=flags;return 0;
}
int tiff89_get_page_info(const unsigned char *s,size_t n,unsigned int page,tiff89_page_info *o)
{
    tif t;td d;unsigned int spp,pl;
    if(o==0||!setup(s,n,&t)||!page_at(&t,page,0,&d))return -1;
    spp=d.SP.ok?tv(&t,&d.SP,0):1u;pl=d.PL.ok?tv(&t,&d.PL,0):1u;
    o->page_index=page;o->width=tv(&t,&d.W,0);o->height=tv(&t,&d.H,0);o->samples_per_pixel=spp;o->compression=d.C.ok?tv(&t,&d.C,0):1u;o->photometric=d.P.ok?tv(&t,&d.P,0):1u;o->planar_configuration=pl;o->tiled=(d.TO.ok&&d.TBC.ok)?1u:0u;o->tile_width=d.TW.ok?tv(&t,&d.TW,0):0u;o->tile_height=d.TH.ok?tv(&t,&d.TH,0):0u;return 0;
}

int tiff89_decode_page_rgba(const unsigned char *s,size_t n,unsigned int page,
                            unsigned char *d,size_t cap,unsigned int *ow,unsigned int *oh,size_t *used)
{
    tif t;td a;unsigned int w,h,spp,comp,photo,pl,pred,i;size_t outneed,tempneed;unsigned char *tmp;
    if(d==0||ow==0||oh==0||used==0||!setup(s,n,&t)||!page_at(&t,page,0,&a))return -1;
    w=tv(&t,&a.W,0);h=tv(&t,&a.H,0);spp=a.SP.ok?tv(&t,&a.SP,0):1u;comp=a.C.ok?tv(&t,&a.C,0):1u;photo=a.P.ok?tv(&t,&a.P,0):1u;pl=a.PL.ok?tv(&t,&a.PL,0):1u;pred=a.PR.ok?tv(&t,&a.PR,0):1u;
    if(!w||!h||w>16384u||h>16384u||spp<1u||spp>4u||(pl!=1u&&pl!=2u))return -2;
    for(i=0u;i<spp;++i)if(a.B.ok&&tv(&t,&a.B,i)!=8u)return -3;
    if(!(comp==1u||comp==5u||comp==8u||comp==32946u||comp==32773u))return -4;
    if(pred!=1u&&pred!=2u)return -5;
    if(pl==2u&&photo==3u)return -6;
    outneed=(size_t)w*h*4u;if((size_t)w*h>cap/4u)return -7;
    tempneed=0u;
    if(a.TO.ok&&a.TBC.ok){unsigned int tw,th,ch;tw=tv(&t,&a.TW,0);th=tv(&t,&a.TH,0);if(!tw||!th)return -8;ch=pl==1u?spp:1u;tempneed=(size_t)tw*th*ch;}
    else {unsigned int rps,ch; rps=a.RPS.ok?tv(&t,&a.RPS,0):h;if(!rps)rps=h;ch=pl==1u?spp:1u;tempneed=(size_t)w*rps*ch;}
    if(tempneed>cap-outneed)return -7;
    tmp=d+cap-tempneed;memset(d,0,outneed);for(i=0u;i<(size_t)w*h;i++)d[i*4u+3u]=255u;
    if(a.TO.ok&&a.TBC.ok){unsigned int tw,th,tx,ty,tpi,planes,plane,ti;tw=tv(&t,&a.TW,0);th=tv(&t,&a.TH,0);tx=(w+tw-1u)/tw;ty=(h+th-1u)/th;tpi=tx*ty;planes=pl==1u?1u:spp;if(a.TO.count<tpi*planes||a.TBC.count<tpi*planes)return -8;
        for(plane=0u;plane<planes;++plane)for(ti=0u;ti<tpi;++ti){unsigned int off,bc,x0,y0,xx,yy,ch,row_samples;size_t want;off=tv(&t,&a.TO,plane*tpi+ti);bc=tv(&t,&a.TBC,plane*tpi+ti);ch=pl==1u?spp:1u;want=(size_t)tw*th*ch;if((size_t)off+bc>n||want>tempneed||!unpack(comp,s+off,bc,tmp,want))return -9;row_samples=tw*ch;if(pred==2u)predictor(tmp,th,row_samples,ch);x0=(ti%tx)*tw;y0=(ti/tx)*th;
            for(yy=0u;yy<th&&y0+yy<h;++yy)for(xx=0u;xx<tw&&x0+xx<w;++xx){if(pl==1u){const unsigned char*v=tmp+((size_t)yy*tw+xx)*spp;if(!put_pixel(d,w,x0+xx,y0+yy,photo,spp,v,&t,&a))return -10;}else put_plane(d,w,x0+xx,y0+yy,photo,plane,tmp[(size_t)yy*tw+xx]);}
        }}
    else {unsigned int rps,strips_per_plane,planes,plane,si;rps=a.RPS.ok?tv(&t,&a.RPS,0):h;if(!rps)rps=h;strips_per_plane=(h+rps-1u)/rps;planes=pl==1u?1u:spp;if(!a.SO.ok||!a.SBC.ok||a.SO.count<strips_per_plane*planes||a.SBC.count<strips_per_plane*planes)return -11;
        for(plane=0u;plane<planes;++plane)for(si=0u;si<strips_per_plane;++si){unsigned int off,bc,rows,ch,row_samples,y,x;size_t want;off=tv(&t,&a.SO,plane*strips_per_plane+si);bc=tv(&t,&a.SBC,plane*strips_per_plane+si);rows=(si+1u==strips_per_plane)?(h-si*rps):rps;ch=pl==1u?spp:1u;row_samples=w*ch;want=(size_t)rows*row_samples;if((size_t)off+bc>n||want>tempneed||!unpack(comp,s+off,bc,tmp,want))return -12;if(pred==2u)predictor(tmp,rows,row_samples,ch);
            for(y=0u;y<rows;++y)for(x=0u;x<w;++x){unsigned int gy=si*rps+y;if(pl==1u){const unsigned char*v=tmp+((size_t)y*w+x)*spp;if(!put_pixel(d,w,x,gy,photo,spp,v,&t,&a))return -13;}else put_plane(d,w,x,gy,photo,plane,tmp[(size_t)y*w+x]);}
        }}
    *ow=w;*oh=h;*used=outneed;return 0;
}
int tiff89_decode_rgba(const unsigned char *s,size_t n,unsigned char *d,size_t cap,unsigned int *w,unsigned int *h,size_t *used)
{
    return tiff89_decode_page_rgba(s,n,0u,d,cap,w,h,used);
}
