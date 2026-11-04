#include "c4d89_img_internal.h"
/* Baseline sequential JPEG (SOF0), 8-bit grayscale or YCbCr, 1/2/4 chroma
   subsampling blocks. Fixed integer inverse DCT, no heap, no float. */
typedef struct JHuff {
    unsigned char counts[17];
    unsigned char vals[256];
    int min[17],max[17],ptr[17];
    unsigned int used;
} JHuff;
typedef struct JComp {
    unsigned int id,hs,vs,qt,dc,ac;
    int prev;
} JComp;
typedef struct JDec {
    const unsigned char *s;
    unsigned long n,pos,bitbuf;
    unsigned int remain,width,height,ncomp,maxh,maxv,restart;
    JComp comp[3];
    JHuff huff[2][4];
    unsigned char quant[4][64];
    unsigned int has_quant[4],has_huff[2][4];
    unsigned int error;
} JDec;
static JDec jd;
static unsigned char jpeg_planes[3][1024];
static const unsigned char zactual[64]={
 0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
 12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
 35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
 58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
static const signed char idctc[8][8]={
 {91,126,118,106,91,71,49,25},
 {91,106,49,-25,-91,-126,-118,-71},
 {91,71,-49,-126,-91,25,118,106},
 {91,25,-118,-71,91,106,-49,-126},
 {91,-25,-118,71,91,-106,-49,126},
 {91,-71,-49,126,-91,-25,118,-106},
 {91,-106,49,25,-91,126,-118,71},
 {91,-126,118,-106,91,-71,49,-25}
};
static unsigned int jread8(JDec *j)
{
    unsigned int b;
    if(j->pos>=j->n){j->error=1u;return 0u;}
    b=j->s[j->pos++];
    if(b==255u){
        if(j->pos>=j->n){j->error=1u;return 0u;}
        if(j->s[j->pos++]!=0u){j->error=1u;return 0u;}
    }
    return b;
}
static unsigned int jbits(JDec *j,unsigned int cnt)
{
    unsigned int v;
    if(cnt>16u){j->error=1u;return 0u;}
    while(j->remain<cnt && !j->error){
        j->bitbuf=(j->bitbuf<<8)|(unsigned long)jread8(j);
        j->remain+=8u;
    }
    if(j->error)return 0u;
    j->remain-=cnt;
    v=(unsigned int)((j->bitbuf>>j->remain)&((1ul<<cnt)-1ul));
    return v;
}
static int jdecode_huff(JDec *j,const JHuff *h)
{
    int code=0;unsigned int k;
    for(k=1u;k<=16u;k++){
        code=(code<<1)|(int)jbits(j,1u);
        if(j->error)return -1;
        if(h->max[k]>=0 && code<=h->max[k] && code>=h->min[k]){
            unsigned int idx=(unsigned int)(h->ptr[k]+code-h->min[k]);
            return idx<h->used?(int)h->vals[idx]:-1;
        }
    }
    j->error=1u;return -1;
}
static int jextend(unsigned int x,unsigned int bits)
{
    if(bits==0u)return 0;
    if(x<(1u<<(bits-1u)))return (int)x+1-(int)(1u<<bits);
    return (int)x;
}
static int jhuff_build(JHuff *h)
{
    unsigned int i,total=0u,code=0u;
    for(i=1u;i<=16u;i++){
        h->ptr[i]=(int)total;
        total+=(unsigned int)h->counts[i];
        if(total>256u)return 0;
        if(h->counts[i]){
            h->min[i]=(int)code;
            h->max[i]=(int)(code+h->counts[i]-1u);
        } else h->min[i]=h->max[i]=-1;
        code=(code+h->counts[i])<<1;
        if(code>131072u)return 0;
    }
    h->used=total;
    return 1;
}
static int jdct_block(JDec *j,unsigned int ci,unsigned int bx,unsigned int by)
{
    int coeff[64],tmp[64];
    unsigned int i,jj,k,run,cat,qt,z,hs,vs,y,x;
    JComp *c=&j->comp[ci];int symbol,value;
    JHuff *hd=&j->huff[0][c->dc];JHuff *ha=&j->huff[1][c->ac];
    memset(coeff,0,sizeof(coeff));
    symbol=jdecode_huff(j,hd);if(symbol<0||symbol>11)return 0;
    value=jextend(jbits(j,(unsigned int)symbol),(unsigned int)symbol);
    c->prev+=value;qt=c->qt;
    if(c->prev<(-32767)||c->prev>32767)return 0;
    coeff[0]=c->prev*(int)j->quant[qt][0];
    k=1u;
    while(k<64u && !j->error){
        symbol=jdecode_huff(j,ha);
        if(symbol<0)return 0;
        if(symbol==0)break;
        if(symbol==0xf0){k+=16u;continue;}
        run=(unsigned int)symbol>>4;cat=(unsigned int)symbol&15u;
        if(cat==0u||cat>10u)return 0;
        k+=run;if(k>=64u)return 0;
        z=zactual[k];
        value=jextend(jbits(j,cat),cat);
        coeff[z]=value*(int)j->quant[qt][z];k++;
    }
    if(j->error)return 0;
    /* Separable 2D IDCT, basis alpha*cos scaled by 128. */
    for(y=0u;y<8u;y++)for(x=0u;x<8u;x++){
        int sum=0;
        for(i=0u;i<8u;i++){
            int v=coeff[i*8u+x];
            if(v>32767||v<(-32767))return 0;
            sum+=v*(int)idctc[y][i];
        }
        tmp[y*8u+x]=sum/128;
    }
    hs=c->hs*8u;vs=c->vs*8u;
    for(y=0u;y<8u;y++)for(x=0u;x<8u;x++){
        int sum=0,pixel;
        for(jj=0u;jj<8u;jj++)sum+=tmp[y*8u+jj]*(int)idctc[x][jj];
        pixel=128+sum/512;
        if(by*8u+y<vs&&bx*8u+x<hs)
            jpeg_planes[ci][(by*8u+y)*hs+bx*8u+x]=(unsigned char)clamp8(pixel);
    }
    return 1;
}
static unsigned int jget_component(unsigned int ci,unsigned int px,unsigned int py)
{
    JComp *c=&jd.comp[ci];
    unsigned int cx=px*c->hs/jd.maxh;
    unsigned int cy=py*c->vs/jd.maxv;
    return jpeg_planes[ci][cy*(c->hs*8u)+cx];
}
static int jread_marker(JDec *j,unsigned int *marker)
{
    while(j->pos<j->n && j->s[j->pos]!=0xffu)j->pos++;
    if(j->pos>=j->n)return 0;
    while(j->pos<j->n && j->s[j->pos]==0xffu)j->pos++;
    if(j->pos>=j->n)return 0;
    *marker=j->s[j->pos++];return *marker!=0u;
}
static int jparse_header(JDec *j,unsigned long *scan_start)
{
    unsigned int marker,seg,kind,id,num,i,ct,k,offset,plen,rank,hs,vs;
    unsigned long end,p;
    if(j->n<4ul||j->s[0]!=255u||j->s[1]!=216u)return C4D89_IMG_INVALID;
    j->pos=2ul;
    while(jread_marker(j,&marker)){
        if(marker==0xd9u)return C4D89_IMG_CORRUPT;
        if(marker==0xdau){
            if(j->pos+2ul>j->n)return C4D89_IMG_TRUNCATED;
            seg=((unsigned int)j->s[j->pos]<<8)|j->s[j->pos+1ul];
            if(seg<8u||j->pos+(unsigned long)seg>j->n)return C4D89_IMG_TRUNCATED;
            p=j->pos+2ul;num=j->s[p++];
            if(num!=j->ncomp||num==0u||p+2ul*num+3ul>j->pos+(unsigned long)seg)return C4D89_IMG_UNSUPPORTED;
            for(i=0u;i<num;i++){
                id=j->s[p++];kind=j->s[p++];
                for(k=0u;k<j->ncomp;k++)if(j->comp[k].id==id)break;
                if(k==j->ncomp||((kind>>4)>3u)||(kind&15u)>3u)return C4D89_IMG_CORRUPT;
                j->comp[k].dc=kind>>4;j->comp[k].ac=kind&15u;
            }
            if(j->s[p++]!=0u||j->s[p++]!=63u||j->s[p]!=0u)return C4D89_IMG_UNSUPPORTED;
            j->pos+=(unsigned long)seg;*scan_start=j->pos;return C4D89_IMG_OK;
        }
        if(marker>=0xd0u && marker<=0xd7u)continue;
        if(marker==0x01u)continue;
        if(j->pos+2ul>j->n)return C4D89_IMG_TRUNCATED;
        seg=((unsigned int)j->s[j->pos]<<8)|j->s[j->pos+1ul];
        if(seg<2u||j->pos+(unsigned long)seg>j->n)return C4D89_IMG_TRUNCATED;
        p=j->pos+2ul;end=j->pos+(unsigned long)seg;
        if(marker==0xdbu){
            while(p<end){
                if(p+65ul>end)return C4D89_IMG_CORRUPT;
                kind=j->s[p++];if(kind>3u)return C4D89_IMG_UNSUPPORTED;
                for(k=0u;k<64u;k++)j->quant[kind][zactual[k]]=j->s[p++];
                j->has_quant[kind]=1u;
            }
        } else if(marker==0xc4u){
            while(p<end){
                unsigned int nvals=0u,cls;
                if(p+17ul>end)return C4D89_IMG_CORRUPT;
                kind=j->s[p++];cls=kind>>4;id=kind&15u;
                if(cls>1u||id>3u)return C4D89_IMG_UNSUPPORTED;
                for(k=1u;k<=16u;k++){j->huff[cls][id].counts[k]=j->s[p++];nvals+=j->huff[cls][id].counts[k];}
                if(nvals>256u||p+nvals>end)return C4D89_IMG_CORRUPT;
                memcpy(j->huff[cls][id].vals,j->s+p,nvals);p+=nvals;
                if(!jhuff_build(&j->huff[cls][id]))return C4D89_IMG_CORRUPT;
                j->has_huff[cls][id]=1u;
            }
        } else if(marker==0xc0u){
            if(p+6ul>end)return C4D89_IMG_CORRUPT;
            if(j->s[p++]!=8u)return C4D89_IMG_UNSUPPORTED;
            j->height=((unsigned int)j->s[p]<<8)|j->s[p+1ul];p+=2ul;
            j->width=((unsigned int)j->s[p]<<8)|j->s[p+1ul];p+=2ul;
            j->ncomp=j->s[p++];
            if((j->ncomp!=1u && j->ncomp!=3u)||p+3ul*j->ncomp>end)return C4D89_IMG_UNSUPPORTED;
            j->maxh=j->maxv=1u;
            for(i=0u;i<j->ncomp;i++){
                id=j->s[p++];kind=j->s[p++];rank=j->s[p++];
                hs=kind>>4;vs=kind&15u;
                if(hs<1u||hs>2u||vs<1u||vs>2u||rank>3u)return C4D89_IMG_UNSUPPORTED;
                j->comp[i].id=id;j->comp[i].hs=hs;j->comp[i].vs=vs;j->comp[i].qt=rank;
                if(hs>j->maxh)j->maxh=hs;
                if(vs>j->maxv)j->maxv=vs;
            }
        } else if(marker==0xddu){
            if(end-p!=2ul)return C4D89_IMG_CORRUPT;
            j->restart=((unsigned int)j->s[p]<<8)|j->s[p+1ul];
        } else if((marker>=0xc1u&&marker<=0xcfu && marker!=0xc4u)||marker==0xccu){
            return C4D89_IMG_UNSUPPORTED;
        }
        j->pos=end;
    }
    (void)offset;(void)plen;(void)ct;
    return C4D89_IMG_TRUNCATED;
}
int c4d89_img_jpeg(const unsigned char *src,unsigned long n,unsigned char *rgba,
                   unsigned long cap,C4D89_ImgInfo *info)
{
    unsigned int mw,mh,mx,my,c,bx,by,x,y,yy,xx,px,py,Y,Cb,Cr,r,g,b;
    unsigned long scan,mcu_index=0ul,p;
    int rc;unsigned int marker;
    memset(&jd,0,sizeof(jd));jd.s=src;jd.n=n;
    rc=jparse_header(&jd,&scan);if(rc)return rc;
    if(!size_ok(jd.width,jd.height,cap))return C4D89_IMG_LIMIT;
    for(c=0u;c<jd.ncomp;c++){
        if(jd.comp[c].hs*jd.comp[c].vs>4u||
           !jd.has_quant[jd.comp[c].qt] || !jd.has_huff[0][jd.comp[c].dc] || !jd.has_huff[1][jd.comp[c].ac])return C4D89_IMG_CORRUPT;
        if(jd.maxh%jd.comp[c].hs || jd.maxv%jd.comp[c].vs)return C4D89_IMG_UNSUPPORTED;
    }
    mw=jd.maxh*8u;mh=jd.maxv*8u;
    mx=(jd.width+mw-1u)/mw;my=(jd.height+mh-1u)/mh;
    jd.pos=scan;jd.bitbuf=0ul;jd.remain=0u;
    for(y=0u;y<my;y++)for(x=0u;x<mx;x++){
        if(jd.restart && mcu_index>0ul && mcu_index%(unsigned long)jd.restart==0ul){
            jd.remain=0u;jd.bitbuf=0ul;
            if(!jread_marker(&jd,&marker)||marker!=(0xd0u+(unsigned int)((mcu_index/(unsigned long)jd.restart-1ul)&7ul)))return C4D89_IMG_CORRUPT;
            for(c=0u;c<jd.ncomp;c++)jd.comp[c].prev=0;
        }
        for(c=0u;c<jd.ncomp;c++)for(by=0u;by<jd.comp[c].vs;by++)for(bx=0u;bx<jd.comp[c].hs;bx++){
            if(!jdct_block(&jd,c,bx,by))return C4D89_IMG_CORRUPT;
        }
        for(yy=0u;yy<mh;yy++)for(xx=0u;xx<mw;xx++){
            px=x*mw+xx;py=y*mh+yy;
            if(px>=jd.width||py>=jd.height)continue;
            Y=jget_component(0u,xx,yy);
            if(jd.ncomp==1u){r=g=b=Y;}
            else {
                int cb,cr;
                Cb=jget_component(1u,xx,yy);Cr=jget_component(2u,xx,yy);
                cb=(int)Cb-128;cr=(int)Cr-128;
                r=clamp8((int)Y+91881*cr/65536);
                g=clamp8((int)Y-(22554*cb+46802*cr)/65536);
                b=clamp8((int)Y+116130*cb/65536);
            }
            p=((unsigned long)py*(unsigned long)jd.width+px)*4ul;
            rgba[p]=(unsigned char)r;rgba[p+1ul]=(unsigned char)g;
            rgba[p+2ul]=(unsigned char)b;rgba[p+3ul]=255u;
        }
        mcu_index++;
    }
    info->width=jd.width;info->height=jd.height;info->format=C4D89_IMG_FORMAT_JPEG;
    info->source_channels=jd.ncomp;return C4D89_IMG_OK;
}
