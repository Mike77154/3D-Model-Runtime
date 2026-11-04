#include "jpg89.h"
#include <string.h>

#define J89_MAX_COMP 4
#define J89_MAX_BLOCKS 16

typedef signed short jcoef;
typedef struct jhuff_s {
    unsigned char count[16];
    unsigned char vals[256];
    unsigned short total;
    unsigned short first_code[16];
    unsigned short first_index[16];
} jhuff;
typedef struct jcomp_s {
    unsigned char id,h,v,qt;
    int pred;
    unsigned int bw,bh;
    size_t boff;
} jcomp;
typedef struct jbit_s {
    const unsigned char *s;
    size_t n,pos;
    unsigned int buf,bits;
    int marker;
} jbit;

static unsigned int r16be(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}
static int clampi(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static const unsigned char zig[64] = {
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
    12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};
static const short bas[8][8] = {
    {91,126,118,106,91,71,49,25},{91,106,49,-25,-91,-126,-118,-71},
    {91,71,-49,-126,-91,25,118,106},{91,25,-118,-71,91,106,-49,-126},
    {91,-25,-118,71,91,-106,-49,126},{91,-71,-49,126,-91,-25,118,-106},
    {91,-106,49,25,-91,126,-118,71},{91,-126,118,-106,91,-71,49,-25}
};

int jpg89_probe(const unsigned char *s,size_t n)
{
    return s != 0 && n >= 3u && s[0] == 0xffu && s[1] == 0xd8u && s[2] == 0xffu;
}
static void hb(jhuff *h,const unsigned char *c,const unsigned char *v,unsigned int total)
{
    unsigned int i,code,idx;
    code=idx=0u;
    for(i=0u;i<16u;++i){h->count[i]=c[i];h->first_code[i]=(unsigned short)code;h->first_index[i]=(unsigned short)idx;idx+=c[i];code=(code+c[i])<<1;}
    h->total=(unsigned short)total;for(i=0u;i<total;++i)h->vals[i]=v[i];
}
static int nextb(jbit *b,unsigned char *out)
{
    unsigned char x;
    if(b->pos>=b->n)return 0;
    x=b->s[b->pos++];
    if(x==0xffu){unsigned char y;do{if(b->pos>=b->n)return 0;y=b->s[b->pos++];}while(y==0xffu);if(y!=0u){b->marker=(int)y;return 0;}x=0xffu;}
    *out=x;return 1;
}
static unsigned int bits(jbit *b,unsigned int n)
{
    unsigned int v;unsigned char x;
    if(n==0u)return 0u;
    while(b->bits<n){if(!nextb(b,&x))return 0u;b->buf=(b->buf<<8)|(unsigned int)x;b->bits+=8u;}
    v=(b->buf>>(b->bits-n))&((1u<<n)-1u);b->bits-=n;
    if(b->bits==0u)b->buf=0u;else b->buf&=(1u<<b->bits)-1u;
    return v;
}
static int hdec(jbit *b,const jhuff *h)
{
    unsigned int code,len;code=0u;
    for(len=1u;len<=16u;++len){unsigned int first,cnt;code=(code<<1)|bits(b,1u);if(b->marker)return -1;first=h->first_code[len-1u];cnt=h->count[len-1u];if(cnt&&code>=first&&code<first+cnt)return h->vals[h->first_index[len-1u]+(code-first)];}
    return -1;
}
static int extendv(unsigned int v,unsigned int n)
{
    unsigned int vt;if(n==0u)return 0;vt=1u<<(n-1u);if(v<vt)return (int)v-(int)((1u<<n)-1u);return (int)v;
}
static void setc(jcoef *c,int v){if(v>32767)v=32767;if(v<-32767)v=-32767;*c=(jcoef)v;}
static void idct_quant(const jcoef *c,const unsigned short *q,unsigned char *out)
{
    int co[64],tmp[64];int y,x,u,v;
    for(y=0;y<64;++y){int z=(int)c[y]*(int)q[y];if(z>8388607)z=8388607;if(z<-8388607)z=-8388607;co[y]=z;}
    for(y=0;y<8;++y)for(x=0;x<8;++x){int sum=0;for(u=0;u<8;++u)sum+=co[y*8+u]*(int)bas[x][u];tmp[y*8+x]=sum/256;}
    for(y=0;y<8;++y)for(x=0;x<8;++x){int sum=0;for(v=0;v<8;++v)sum+=tmp[v*8+x]*(int)bas[y][v];out[y*8+x]=(unsigned char)clampi(128+sum/256);}
}
static int findc(jcomp *c,unsigned int nc,unsigned int id){unsigned int i;for(i=0u;i<nc;++i)if(c[i].id==id)return (int)i;return -1;}
static int seq_block(jbit *b,jcomp *c,const jhuff *hd,const jhuff *ha,jcoef *co)
{
    int ss,diff,k,rs,run,sz,val;unsigned int i;
    for(i=0u;i<64u;++i)co[i]=0;
    ss=hdec(b,hd);if(ss<0||ss>16)return 0;diff=extendv(bits(b,(unsigned int)ss),(unsigned int)ss);if(b->marker)return 0;c->pred+=diff;setc(&co[0],c->pred);
    k=1;while(k<64){rs=hdec(b,ha);if(rs<0)return 0;if(rs==0)break;run=(rs>>4)&15;sz=rs&15;if(sz==0){if(run!=15)return 0;k+=16;continue;}k+=run;if(k>=64)return 0;val=extendv(bits(b,(unsigned int)sz),(unsigned int)sz);if(b->marker)return 0;setc(&co[zig[k]],val);++k;}return 1;
}
static int prog_dc_first(jbit *b,jcomp *c,const jhuff *hd,jcoef *co,unsigned int al)
{
    int ss,diff,v;ss=hdec(b,hd);if(ss<0||ss>16)return 0;diff=extendv(bits(b,(unsigned int)ss),(unsigned int)ss);if(b->marker)return 0;c->pred+=diff;v=c->pred*(int)(1u<<al);setc(&co[0],v);return 1;
}
static int prog_dc_refine(jbit *b,jcoef *co,unsigned int al)
{
    int v;if(bits(b,1u)){v=(int)co[0];v|=(int)(1u<<al);setc(&co[0],v);}return b->marker?0:1;
}
static int prog_ac_first(jbit *b,const jhuff *ha,jcoef *co,unsigned int ss,unsigned int se,unsigned int al,unsigned int *eob)
{
    unsigned int k;
    if(*eob){--*eob;return 1;}
    k=ss;while(k<=se){int rs,r,sz,val;rs=hdec(b,ha);if(rs<0)return 0;r=(rs>>4)&15;sz=rs&15;if(sz==0){if(r==15){k+=16u;continue;}*eob=1u<<(unsigned int)r;if(r)*eob+=bits(b,(unsigned int)r);if(b->marker)return 0;--*eob;break;}k+=(unsigned int)r;if(k>se)return 0;val=extendv(bits(b,(unsigned int)sz),(unsigned int)sz)*(int)(1u<<al);if(b->marker)return 0;setc(&co[zig[k]],val);++k;}return 1;
}
static int refine_nonzero(jbit *b,jcoef *v,int p1)
{
    int x;if(!bits(b,1u))return b->marker?0:1;if(b->marker)return 0;x=(int)*v;if((x<0?-x:x)&p1)return 1;x+=x>=0?p1:-p1;setc(v,x);return 1;
}
static int prog_ac_refine(jbit *b,const jhuff *ha,jcoef *co,unsigned int ss,unsigned int se,unsigned int al,unsigned int *eob)
{
    unsigned int k;int p1;p1=(int)(1u<<al);k=ss;
    if(*eob){for(k=ss;k<=se;++k)if(co[zig[k]]!=0&&!refine_nonzero(b,&co[zig[k]],p1))return 0;--*eob;return 1;}
    while(k<=se){int rs,r,sz,newv;rs=hdec(b,ha);if(rs<0)return 0;r=(rs>>4)&15;sz=rs&15;newv=0;if(sz){if(sz!=1)return 0;newv=bits(b,1u)?p1:-p1;if(b->marker)return 0;}else if(r!=15){*eob=1u<<(unsigned int)r;if(r)*eob+=bits(b,(unsigned int)r);if(b->marker)return 0;break;}
        for(;;){jcoef *cv;if(k>se)break;cv=&co[zig[k]];if(*cv!=0){if(!refine_nonzero(b,cv,p1))return 0;}else{if(r==0)break;--r;}++k;}
        if(newv){if(k>se)return 0;setc(&co[zig[k]],newv);++k;}else if(sz==0&&r==15){if(k<=se)++k;}
    }
    if(*eob){for(;k<=se;++k)if(co[zig[k]]!=0&&!refine_nonzero(b,&co[zig[k]],p1))return 0;--*eob;}
    return 1;
}

static int take_restart(jbit *b)
{
    int m;
    b->bits=0u;b->buf=0u;m=b->marker;b->marker=0;
    if(m>=0xd0&&m<=0xd7)return 1;
    while(b->pos<b->n){unsigned char x,y;x=b->s[b->pos++];if(x!=0xffu)continue;do{if(b->pos>=b->n)return 0;y=b->s[b->pos++];}while(y==0xffu);if(y==0u)continue;if(y>=0xd0u&&y<=0xd7u)return 1;b->marker=(int)y;return 0;}
    return 0;
}
static jcoef *coef_block(jcoef *base,const jcomp *c,unsigned int bx,unsigned int by,jcoef *dummy)
{
    if(bx>=c->bw||by>=c->bh)return dummy;
    return base+(c->boff+(size_t)by*c->bw+bx)*64u;
}
static int decode_scan(const unsigned char *s,size_t n,size_t ep,jcomp *cp,unsigned int nc,
                       unsigned int w,unsigned int h,unsigned int maxh,unsigned int maxv,
                       int progressive,unsigned int restart,const jhuff *hd,const jhuff *ha,
                       const int *hdok,const int *haok,jcoef *coef,
                       const unsigned char *sh,unsigned int ns,unsigned int ss,unsigned int se,
                       unsigned int ah,unsigned int al,size_t *outpos,int *outmarker)
{
    int sci[J89_MAX_COMP];unsigned int sdc[J89_MAX_COMP],sac[J89_MAX_COMP];unsigned int i,mcols,mrows,mcu,mcus,eob; jbit br;jcoef dummy[64];
    for(i=0u;i<64u;++i)dummy[i]=0;
    for(i=0u;i<nc;++i)cp[i].pred=0;
    for(i=0u;i<ns;++i){int ci=findc(cp,nc,sh[1u+i*2u]);unsigned int ta=sh[2u+i*2u];if(ci<0)return 0;sci[i]=ci;sdc[i]=ta>>4;sac[i]=ta&15u;if(sdc[i]>=4u||sac[i]>=4u)return 0;if((ss==0u&&!hdok[sdc[i]])||(se>0u&&!haok[sac[i]]))return 0;}
    if(!progressive){if(ss!=0u||se!=63u||ah!=0u||al!=0u)return 0;}else{if(ss==0u&&se!=0u)return 0;if(ss>0u&&ns!=1u)return 0;if(ah>13u||al>13u||(ah!=0u&&ah!=al+1u))return 0;}
    br.s=s;br.n=n;br.pos=ep;br.buf=0u;br.bits=0u;br.marker=0;eob=0u;
    if(ns==1u)mcus=cp[sci[0]].bw*cp[sci[0]].bh;else{mcols=(w+maxh*8u-1u)/(maxh*8u);mrows=(h+maxv*8u-1u)/(maxv*8u);mcus=mcols*mrows;}
    mcols=(w+maxh*8u-1u)/(maxh*8u);mrows=(h+maxv*8u-1u)/(maxv*8u);
    for(mcu=0u;mcu<mcus;++mcu){
        if(restart&&mcu&&mcu%restart==0u){if(!take_restart(&br))return 0;for(i=0u;i<nc;++i)cp[i].pred=0;eob=0u;}
        if(ns==1u){unsigned int bx,by;int ci=sci[0];jcoef *co;bx=mcu%cp[ci].bw;by=mcu/cp[ci].bw;co=coef_block(coef,&cp[ci],bx,by,dummy);
            if(!progressive){if(!seq_block(&br,&cp[ci],&hd[sdc[0]],&ha[sac[0]],co))return 0;}
            else if(ss==0u){if(ah==0u){if(!prog_dc_first(&br,&cp[ci],&hd[sdc[0]],co,al))return 0;}else if(!prog_dc_refine(&br,co,al))return 0;}
            else if(ah==0u){if(!prog_ac_first(&br,&ha[sac[0]],co,ss,se,al,&eob))return 0;}
            else if(!prog_ac_refine(&br,&ha[sac[0]],co,ss,se,al,&eob))return 0;
        }else{
            unsigned int mx=mcu%mcols,my=mcu/mcols;
            for(i=0u;i<ns;++i){int ci=sci[i];unsigned int vy,vx;for(vy=0u;vy<cp[ci].v;++vy)for(vx=0u;vx<cp[ci].h;++vx){unsigned int bx=mx*cp[ci].h+vx,by=my*cp[ci].v+vy;jcoef *co=coef_block(coef,&cp[ci],bx,by,dummy);
                if(!progressive){if(!seq_block(&br,&cp[ci],&hd[sdc[i]],&ha[sac[i]],co))return 0;}
                else if(ss==0u){if(ah==0u){if(!prog_dc_first(&br,&cp[ci],&hd[sdc[i]],co,al))return 0;}else if(!prog_dc_refine(&br,co,al))return 0;}
                else return 0;
            }}
        }
    }
    *outpos=br.pos;*outmarker=br.marker;return 1;
}

static int render_image(unsigned char *d,size_t cap,unsigned int w,unsigned int h,
                        jcomp *cp,unsigned int nc,unsigned int maxh,unsigned int maxv,
                        unsigned short qt[4][64],const int qok[4],jcoef *coef)
{
    unsigned int mx,my,mcols,mrows,i,j;unsigned char samples[J89_MAX_COMP][J89_MAX_BLOCKS][64];
    if((size_t)w*h>cap/4u)return 0;
    for(i=0u;i<nc;++i)if(cp[i].qt>=4u||!qok[cp[i].qt])return 0;
    mcols=(w+maxh*8u-1u)/(maxh*8u);mrows=(h+maxv*8u-1u)/(maxv*8u);
    for(my=0u;my<mrows;++my)for(mx=0u;mx<mcols;++mx){
        for(i=0u;i<nc;++i){unsigned int nb=(unsigned int)cp[i].h*cp[i].v;for(j=0u;j<nb;++j){unsigned int bx=mx*cp[i].h+(j%cp[i].h),by=my*cp[i].v+(j/cp[i].h);if(bx<cp[i].bw&&by<cp[i].bh)idct_quant(coef+(cp[i].boff+(size_t)by*cp[i].bw+bx)*64u,qt[cp[i].qt],samples[i][j]);else memset(samples[i][j],128,64u);}}
        {unsigned int yy,xx,mw=maxh*8u,mh=maxv*8u;int rgb=(nc==3u&&cp[0].id=='R'&&cp[1].id=='G'&&cp[2].id=='B');for(yy=0u;yy<mh;++yy)for(xx=0u;xx<mw;++xx){unsigned int gx=mx*mw+xx,gy=my*mh+yy;if(gx<w&&gy<h){int sv[4]={0,128,128,255};size_t di=((size_t)gy*w+gx)*4u;for(i=0u;i<nc;++i){unsigned int sx=xx*cp[i].h/maxh,sy=yy*cp[i].v/maxv,bi=(sy/8u)*cp[i].h+(sx/8u);sv[i]=samples[i][bi][(sy&7u)*8u+(sx&7u)];}if(nc==1u){d[di]=d[di+1u]=d[di+2u]=(unsigned char)sv[0];d[di+3u]=255u;}else if(nc==3u){int R,G,B;if(rgb){R=sv[0];G=sv[1];B=sv[2];}else{int cb=sv[1]-128,cr=sv[2]-128;R=sv[0]+((359*cr)>>8);G=sv[0]-((88*cb+183*cr)>>8);B=sv[0]+((454*cb)>>8);}d[di]=(unsigned char)clampi(R);d[di+1u]=(unsigned char)clampi(G);d[di+2u]=(unsigned char)clampi(B);d[di+3u]=255u;}else return 0;}}}
    }
    return 1;
}

int jpg89_get_info(const unsigned char *s,size_t n,jpg89_info *o)
{
    size_t p;unsigned int w=0u,h=0u,nc=0u,scans=0u,prog=0u;
    if(o==0||!jpg89_probe(s,n))return -1;
    p=2u;
    while(p+1u<n){unsigned int m,len;while(p<n&&s[p]!=0xffu)++p;while(p<n&&s[p]==0xffu)++p;if(p>=n)break;m=s[p++];if(m==0u||(m>=0xd0u&&m<=0xd9u)||m==0x01u)continue;if(p+2u>n)break;len=r16be(s+p);if(len<2u||p+len>n)break;if((m==0xc0u||m==0xc2u)&&len>=8u){prog=m==0xc2u;h=r16be(s+p+3u);w=r16be(s+p+5u);nc=s[p+7u];}if(m==0xdau)++scans;p+=len;}
    if(!w||!h)return -2;
    o->width=w;
    o->height=h;
    o->progressive=prog;
    o->scan_count=scans;
    o->components=nc;
    return 0;
}

int jpg89_decode_rgba(const unsigned char *s,size_t n,unsigned char *d,size_t cap,unsigned int *ow,unsigned int *oh,size_t *used)
{
    unsigned short qt[4][64];int qok[4]={0,0,0,0};jhuff hd[4],ha[4];int hdok[4]={0,0,0,0},haok[4]={0,0,0,0};jcomp cp[J89_MAX_COMP];unsigned int nc=0u,w=0u,h=0u,maxh=1u,maxv=1u,restart=0u;size_t p=2u,total_blocks=0u,outneed=0u,coefbytes=0u;int gotsof=0,progressive=0,pending=0,got_scan=0;unsigned int i,scan_count=0u;jcoef *coef=0;
    memset(qt,0,sizeof(qt));memset(cp,0,sizeof(cp));
    if(!jpg89_probe(s,n)||d==0||ow==0||oh==0||used==0)return -1;
    while(p+1u<n||pending){unsigned int m,len;if(pending){m=(unsigned int)pending;pending=0;}else{while(p<n&&s[p]!=0xffu)++p;while(p<n&&s[p]==0xffu)++p;if(p>=n)break;m=s[p++];}
        if(m==0xd9u)break;
        if(m==0xd8u||m==0x01u||(m>=0xd0u&&m<=0xd7u))continue;
        if(p+2u>n)return -2;
        len=r16be(s+p);
        p+=2u;
        if(len<2u||p+(size_t)len-2u>n)return -2;
        len-=2u;
        if(m==0xdbu){size_t qpos=p,e=p+len;while(qpos<e){unsigned int pq=s[qpos++],id=pq&15u,prec=pq>>4;if(id>=4u||prec>1u)return -3;if(qpos+(prec?128u:64u)>e)return -3;for(i=0u;i<64u;++i){unsigned int v;if(prec){v=r16be(s+qpos);qpos+=2u;}else v=s[qpos++];qt[id][zig[i]]=(unsigned short)v;}qok[id]=1;}}
        else if(m==0xc0u||m==0xc2u){if(len<6u||s[p]!=8u)return -4;h=r16be(s+p+1u);w=r16be(s+p+3u);nc=s[p+5u];if(nc==0u||nc>J89_MAX_COMP||len<6u+3u*nc)return -4;maxh=maxv=1u;for(i=0u;i<nc;++i){unsigned int hv;cp[i].id=s[p+6u+i*3u];hv=s[p+7u+i*3u];cp[i].h=(unsigned char)(hv>>4);cp[i].v=(unsigned char)(hv&15u);cp[i].qt=s[p+8u+i*3u];cp[i].pred=0;if(cp[i].h==0u||cp[i].v==0u||cp[i].h*cp[i].v>J89_MAX_BLOCKS)return -4;if(cp[i].h>maxh)maxh=cp[i].h;if(cp[i].v>maxv)maxv=cp[i].v;}if(!w||!h)return -4;total_blocks=0u;for(i=0u;i<nc;++i){cp[i].bw=(w*cp[i].h+maxh*8u-1u)/(maxh*8u);cp[i].bh=(h*cp[i].v+maxv*8u-1u)/(maxv*8u);cp[i].boff=total_blocks;total_blocks+=(size_t)cp[i].bw*cp[i].bh;}outneed=(size_t)w*h*4u;coefbytes=total_blocks*64u*sizeof(jcoef);if((size_t)w*h>cap/4u||coefbytes>cap-outneed)return -11;coef=(jcoef *)(void *)(d+outneed);memset(coef,0,coefbytes);gotsof=1;progressive=m==0xc2u;}
        else if(m==0xc4u){size_t qpos=p,e=p+len;while(qpos<e){unsigned int tc=s[qpos++],cl=tc>>4,id=tc&15u,total=0u;unsigned char cnt[16];if(id>=4u||cl>1u||qpos+16u>e)return -7;for(i=0u;i<16u;++i){cnt[i]=s[qpos++];total+=cnt[i];}if(total>256u||qpos+total>e)return -7;if(cl==0u){hb(&hd[id],cnt,s+qpos,total);hdok[id]=1;}else{hb(&ha[id],cnt,s+qpos,total);haok[id]=1;}qpos+=total;}}
        else if(m==0xddu){if(len!=2u)return -8;restart=r16be(s+p);}
        else if(m==0xdau){unsigned int ns,ss,se,ahal,ah,al;size_t ep,newp;int mark;if(!gotsof||len<4u)return -9;ns=s[p];if(ns==0u||ns>nc||len<1u+2u*ns+3u)return -9;ss=s[p+1u+2u*ns];se=s[p+2u+2u*ns];ahal=s[p+3u+2u*ns];ah=ahal>>4;al=ahal&15u;ep=p+len;if(!decode_scan(s,n,ep,cp,nc,w,h,maxh,maxv,progressive,restart,hd,ha,hdok,haok,coef,s+p,ns,ss,se,ah,al,&newp,&mark))return -13;p=newp;if(mark)pending=mark;got_scan=1;++scan_count;continue;}
        p+=len;
    }
    if(!gotsof||!got_scan||coef==0)return -16;
    if(!render_image(d,cap,w,h,cp,nc,maxh,maxv,qt,qok,coef))return -14;
    *ow=w;
    *oh=h;
    *used=outneed;
    (void)scan_count;
    return 0;
}
