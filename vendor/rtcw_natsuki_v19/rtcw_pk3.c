/* RTCW PK3 (ordinary ZIP) independent reader + raw DEFLATE decoder.
   CC0-1.0, strict ISO C89; no malloc, float, zlib, or third-party code.
   32 MiB MAX per entry, non-ZIP64 single-volume archives, no encryption. */
#include "rtcw_pk3.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define INFL_MAX_BITS 15U
#define INFL_MAX_SYMBOLS 288U
#define INFL_MAX_DISTANCE 32768UL

static FILE *archive_file;
static unsigned long archive_size,central_start,central_end,central_pos,entry_count,entry_index;
static char pk3_error_text[160]="not open";
/* Static by design; expect approx. 64 MiB total BSS for input and output. */
static unsigned char comp_buf[RTCW_PK3_DATA_MAX];
static unsigned char raw_buf[RTCW_PK3_DATA_MAX];
static unsigned char tail[65557U];

static int failed(const char *s){
    strncpy(pk3_error_text,s,sizeof(pk3_error_text)-1U);
    pk3_error_text[sizeof(pk3_error_text)-1U]=0;return 0;
}
const char *rtcw_pk3_error(void){return pk3_error_text;}
static unsigned int rd16(const unsigned char *p){return (unsigned int)p[0]|((unsigned int)p[1]<<8);}
static unsigned long rd32(const unsigned char *p){
    return (unsigned long)p[0]|((unsigned long)p[1]<<8)|
           ((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
}
static int seek_abs(unsigned long pos){
    if(pos>archive_size||pos>(unsigned long)LONG_MAX)return 0;
    return fseek(archive_file,(long)pos,SEEK_SET)==0;
}
static int read_at(unsigned long pos,unsigned char *dst,unsigned long n){
    if(pos>archive_size||n>archive_size-pos||!seek_abs(pos))return 0;
    return fread(dst,1,(size_t)n,archive_file)==(size_t)n;
}
void rtcw_pk3_close(void){
    if(archive_file)fclose(archive_file);
    archive_file=NULL;entry_count=entry_index=central_start=central_end=central_pos=archive_size=0;
}
unsigned long rtcw_pk3_count(void){return entry_count;}
int rtcw_pk3_safe_path(const char *s){
    unsigned int n=0,start=0,i;char c;
    if(!s||!*s)return 0;
    for(i=0;(c=s[i])!=0;i++){
        if(++n>RTCW_PK3_PATH)return 0;
        if(c=='\\'||c==':'||(unsigned char)c<32U)return 0;
        if(c=='/'){
            if(i==start)return 0; /* excludes absolute and doubled slash */
            if(i-start==1U&&s[start]=='.')return 0;
            if(i-start==2U&&s[start]=='.'&&s[start+1]=='.')return 0;
            start=i+1U;
        }
    }
    if(i==start && i>0){ /* directory trailing slash allowed */
        if(i>=2U&&s[i-2U]=='/')return 0;
        return 1;
    }
    if(i-start==1U&&s[start]=='.')return 0;
    if(i-start==2U&&s[start]=='.'&&s[start+1U]=='.')return 0;
    return 1;
}
int rtcw_pk3_open(const char *filename){
    long full;unsigned long n,start,i,eocd=ULONG_MAX,cdsize,cdoff,num;
    unsigned char *p;
    rtcw_pk3_close();
    archive_file=fopen(filename,"rb");
    if(!archive_file)return failed("cannot open PK3");
    if(fseek(archive_file,0,SEEK_END)!=0||(full=ftell(archive_file))<22L){
        rtcw_pk3_close();return failed("PK3 too short or exceeds seek limit");
    }
    archive_size=(unsigned long)full;
    n=archive_size<sizeof(tail)?archive_size:(unsigned long)sizeof(tail);
    start=archive_size-n;
    if(!read_at(start,tail,n)){rtcw_pk3_close();return failed("cannot read PK3 EOCD");}
    /* EOCD comment must end at physical EOF; reject appended garbage. */
    for(i=n-22UL;;i--){
        if(rd32(tail+i)==0x06054b50UL&&
           i+22UL+(unsigned long)rd16(tail+i+20UL)==n){eocd=i;break;}
        if(i==0)break;
    }
    if(eocd==ULONG_MAX){rtcw_pk3_close();return failed("ZIP EOCD missing");}
    p=tail+eocd;
    if(rd16(p+4)!=0||rd16(p+6)!=0||rd16(p+8)!=rd16(p+10)){
        rtcw_pk3_close();return failed("multi-volume ZIP unsupported");
    }
    num=rd16(p+10);cdsize=rd32(p+12);cdoff=rd32(p+16);
    if(num==65535UL||cdsize==0xffffffffUL||cdoff==0xffffffffUL){
        rtcw_pk3_close();return failed("ZIP64 unsupported");
    }
    if(cdoff>archive_size||cdsize>archive_size-cdoff||
       cdoff+cdsize>start+eocd||num>25000UL){
        rtcw_pk3_close();return failed("invalid PK3 central directory bounds");
    }
    central_start=cdoff;central_end=cdoff+cdsize;central_pos=central_start;
    entry_index=0;entry_count=num;
    strcpy(pk3_error_text,"OK");return 1;
}
int rtcw_pk3_next(rtcw_pk3_entry *out){
    unsigned char h[46];unsigned long at,name_len,extra_len,comment_len,after;
    if(!archive_file||!out){failed("PK3 not open");return -1;}
    if(entry_index>=entry_count){
        if(central_pos!=central_end){failed("central directory trailing bytes");return -1;}
        return 0;
    }
    at=central_pos;
    if(at>central_end||central_end-at<46UL||!read_at(at,h,46UL)||rd32(h)!=0x02014b50UL){
        failed("malformed PK3 central entry");return -1;
    }
    name_len=rd16(h+28);extra_len=rd16(h+30);comment_len=rd16(h+32);
    if(name_len<1UL||name_len>RTCW_PK3_PATH||
       46UL+name_len+extra_len+comment_len>central_end-at){
        failed("invalid/overlong PK3 filename or entry");return -1;
    }
    after=at+46UL+name_len+extra_len+comment_len;
    memset(out,0,sizeof(*out));
    if(!read_at(at+46UL,(unsigned char*)out->name,name_len)){
        failed("cannot read PK3 name");return -1;
    }
    out->name[name_len]=0;
    if(strlen(out->name)!=name_len||!rtcw_pk3_safe_path(out->name)){
        failed("unsafe PK3 path");return -1;
    }
    out->is_directory=out->name[name_len-1UL]=='/';
    out->flags=rd16(h+8);out->method=rd16(h+10);
    out->crc32=rd32(h+16);
    out->compressed_size=rd32(h+20);
    out->uncompressed_size=rd32(h+24);
    out->local_offset=rd32(h+42);
    if(rd16(h+34)!=0 || (out->flags&1U)!=0 || (out->flags&0x40U)!=0){
        failed("encrypted/multi-disk ZIP entry unsupported");return -1;
    }
    if(out->compressed_size==0xffffffffUL||out->uncompressed_size==0xffffffffUL||
       out->local_offset==0xffffffffUL){
        failed("ZIP64 entry unsupported");return -1;
    }
    if(out->method!=0U&&out->method!=8U){failed("ZIP compression method unsupported");return -1;}
    central_pos=after;entry_index++;
    return 1;
}

typedef struct huff_s {
    unsigned short counts[INFL_MAX_BITS+1U];
    unsigned short first[INFL_MAX_BITS+1U];
    unsigned short start[INFL_MAX_BITS+1U];
    unsigned short sym[INFL_MAX_SYMBOLS];
} huff;
typedef struct bits_s {
    const unsigned char *in;unsigned long len,pos;
    unsigned long buf;unsigned int count;int bad;
} bits;
static int getbits(bits *b,unsigned int n){
    unsigned long mask,r;
    if(n>16U){b->bad=1;return 0;}
    while(b->count<n){
        if(b->pos>=b->len){b->bad=1;return 0;}
        b->buf|=(unsigned long)b->in[b->pos++]<<b->count;
        b->count+=8U;
    }
    mask=(1UL<<n)-1UL;
    r=b->buf&mask;b->buf>>=n;b->count-=n;
    return (int)r;
}
static int huff_init(huff *h,const unsigned char *lengths,unsigned int n){
    unsigned int i,l,sum=0,next[INFL_MAX_BITS+1U];
    unsigned int code=0;int left=1;
    if(n>INFL_MAX_SYMBOLS)return 0;
    memset(h,0,sizeof(*h));
    for(i=0;i<n;i++){
        if(lengths[i]>INFL_MAX_BITS)return 0;
        if(lengths[i])h->counts[lengths[i]]++;
    }
    for(l=1;l<=INFL_MAX_BITS;l++){
        left=left*2-(int)h->counts[l];
        if(left<0)return 0; /* oversubscribed Huffman tree */
        h->start[l]=(unsigned short)sum;sum+=h->counts[l];
        next[l]=h->start[l];
        code=(code+h->counts[l-1U])<<1;
        h->first[l]=(unsigned short)code;
    }
    if(sum==0)return 0;
    for(i=0;i<n;i++)if(lengths[i]){
        h->sym[next[lengths[i]]++]=(unsigned short)i;
    }
    return 1;
}
static int huff_read(bits *b,const huff *h){
    unsigned int l,code=0,base,count;
    for(l=1U;l<=INFL_MAX_BITS;l++){
        code=(code<<1U)|(unsigned int)getbits(b,1U);
        if(b->bad)return -1;
        base=h->first[l];count=h->counts[l];
        if(code>=base&&code-base<count)return h->sym[h->start[l]+code-base];
    }
    b->bad=1;return -1;
}
static int deflate(const unsigned char *data,unsigned long inlen,
                   unsigned char *out,unsigned long outlen){
    static const unsigned short length_base[29]={
        3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,
        31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const unsigned char length_extra[29]={
        0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,
        2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const unsigned short dist_base[30]={
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,
        193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const unsigned char dist_extra[30]={
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,
        6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const unsigned char clorder[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    bits b;huff lit,dist,cl;
    unsigned char ll[288],dd[32],cllens[19],alllens[320];
    unsigned int hlit,hdist,hclen,i,idx,rep,val,prev,blocktype,final=0;
    unsigned long written=0,sz,distance,j;
    int sym,dsym,lsym;
    memset(&b,0,sizeof(b));b.in=data;b.len=inlen;
    while(!final && !b.bad){
        final=(unsigned int)getbits(&b,1U);
        blocktype=(unsigned int)getbits(&b,2U);
        if(b.bad)return 0;
        if(blocktype==0U){
            /* Stored block: byte align, validate LEN/NLEN. */
            (void)getbits(&b,b.count%8U);
            i=(unsigned int)getbits(&b,16U);
            val=(unsigned int)getbits(&b,16U);
            if(b.bad||((i^val)&65535U)!=65535U||i>outlen-written)return 0;
            while(i--){out[written++]=(unsigned char)getbits(&b,8U);if(b.bad)return 0;}
            continue;
        }
        if(blocktype==3U)return 0;
        memset(ll,0,sizeof(ll));memset(dd,0,sizeof(dd));
        if(blocktype==1U){
            for(i=0;i<=143U;i++)ll[i]=8;
            for(i=144U;i<=255U;i++)ll[i]=9;
            for(i=256U;i<=279U;i++)ll[i]=7;
            for(i=280U;i<=287U;i++)ll[i]=8;
            for(i=0;i<32U;i++)dd[i]=5;
        }else{
            hlit=(unsigned int)getbits(&b,5U)+257U;
            hdist=(unsigned int)getbits(&b,5U)+1U;
            hclen=(unsigned int)getbits(&b,4U)+4U;
            if(b.bad||hlit>286U||hdist>32U)return 0;
            memset(cllens,0,sizeof(cllens));
            for(i=0;i<hclen;i++)cllens[clorder[i]]=(unsigned char)getbits(&b,3U);
            if(b.bad||!huff_init(&cl,cllens,19U))return 0;
            idx=0;prev=0;
            while(idx<hlit+hdist){
                sym=huff_read(&b,&cl);if(sym<0)return 0;
                if(sym<=15){alllens[idx++]=(unsigned char)sym;prev=(unsigned int)sym;}
                else{
                    if(sym==16){if(idx==0)return 0;rep=(unsigned int)getbits(&b,2U)+3U;val=prev;}
                    else if(sym==17){rep=(unsigned int)getbits(&b,3U)+3U;val=0;}
                    else if(sym==18){rep=(unsigned int)getbits(&b,7U)+11U;val=0;}
                    else return 0;
                    if(b.bad||rep>hlit+hdist-idx)return 0;
                    prev=val;
                    while(rep--)alllens[idx++]=(unsigned char)val;
                }
            }
            for(i=0;i<hlit;i++)ll[i]=alllens[i];
            for(i=0;i<hdist;i++)dd[i]=alllens[hlit+i];
            if(ll[256]==0)return 0; /* EOB required */
        }
        if(!huff_init(&lit,ll,288U))return 0;
        /* Some valid literal-only streams have no distance codes. */
        if(!huff_init(&dist,dd,32U))memset(&dist,0,sizeof(dist));
        for(;;){
            sym=huff_read(&b,&lit);
            if(sym<0)return 0;
            if(sym<256){if(written>=outlen)return 0;out[written++]=(unsigned char)sym;continue;}
            if(sym==256)break;
            lsym=sym-257;
            if(lsym<0||lsym>=29)return 0;
            sz=(unsigned long)length_base[lsym]+(unsigned long)getbits(&b,length_extra[lsym]);
            dsym=huff_read(&b,&dist);
            if(dsym<0||dsym>=30||b.bad)return 0;
            distance=(unsigned long)dist_base[dsym]+(unsigned long)getbits(&b,dist_extra[dsym]);
            if(b.bad||distance<1UL||distance>INFL_MAX_DISTANCE||distance>written||sz>outlen-written)return 0;
            for(j=0;j<sz;j++){
                out[written]=out[written-distance];written++;
            }
        }
    }
    return !b.bad&&final&&written==outlen;
}
static unsigned long crc32_ieee(const unsigned char *p,unsigned long n){
    unsigned long crc=0xffffffffUL,i;unsigned int j;
    for(i=0;i<n;i++){
        crc^=p[i];
        for(j=0;j<8U;j++)crc=(crc>>1)^((crc&1UL)?0xedb88320UL:0UL);
    }
    return (crc^0xffffffffUL)&0xffffffffUL;
}
int rtcw_pk3_extract(const rtcw_pk3_entry *e,const char *dst){
    unsigned char lh[30];unsigned long offset,csize,usize,loff;unsigned int local_flags;
    FILE *out;unsigned char namebuf[RTCW_PK3_PATH+1U];
    if(!archive_file||!e||!dst||!*dst||e->is_directory)return failed("invalid extraction request");
    csize=e->compressed_size;usize=e->uncompressed_size;loff=e->local_offset;
    if(csize>RTCW_PK3_DATA_MAX||usize>RTCW_PK3_DATA_MAX)return failed("entry exceeds 32 MiB buffer");
    if(loff>archive_size||archive_size-loff<30UL||!read_at(loff,lh,30UL)||
       rd32(lh)!=0x04034b50UL)return failed("invalid ZIP local header");
    local_flags=rd16(lh+6);
    if(local_flags!=e->flags || rd16(lh+8)!=e->method ||
       rd16(lh+26)!=(unsigned int)strlen(e->name))return failed("ZIP local header disagrees with central directory");
    /* Cross-check the exact local name, not merely its length. */
    if(!read_at(loff+30UL,namebuf,(unsigned long)rd16(lh+26)) ||
       memcmp(namebuf,e->name,strlen(e->name))!=0)
        return failed("ZIP local filename mismatch");
    if((e->flags&8U)==0U &&
       (rd32(lh+14)!=e->crc32 || rd32(lh+18)!=csize || rd32(lh+22)!=usize))
        return failed("ZIP local sizes/CRC mismatch");
    offset=loff+30UL+(unsigned long)rd16(lh+26)+(unsigned long)rd16(lh+28);
    if(offset>central_start||csize>central_start-offset)return failed("ZIP file data outside local region");
    if(csize&&!read_at(offset,comp_buf,csize))return failed("truncated ZIP file data");
    if(e->method==0U){
        if(csize!=usize)return failed("invalid stored ZIP entry length");
        if(usize)memcpy(raw_buf,comp_buf,(size_t)usize);
    }else if(!deflate(comp_buf,csize,raw_buf,usize))return failed("invalid DEFLATE stream");
    if(crc32_ieee(raw_buf,usize)!=e->crc32)return failed("CRC32 mismatch");
    out=fopen(dst,"wb");if(!out)return failed("cannot create extracted file");
    if(usize&&fwrite(raw_buf,1,(size_t)usize,out)!=(size_t)usize){
        fclose(out);remove(dst);return failed("cannot write extracted data");
    }
    if(fclose(out)!=0){remove(dst);return failed("cannot close extracted file");}
    strcpy(pk3_error_text,"OK");return 1;
}
