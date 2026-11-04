/* Serious Engine GRO (ZIP method 0/8) extraction in strict ISO C89.
   Custom streaming raw DEFLATE decoder; independent/no malloc/no zlib.
   Limits: non-ZIP64, non-encrypted, single entry output at a time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;
#define HCAP 12000
#define OUTMAX 0x7fffffffU

typedef struct { U16 branch[2]; short val; } HNode;
static HNode hnode[HCAP];
static unsigned hcount;
static U8 history[32768];
static U32 crc_table[256];
typedef struct { FILE *f,*out;U32 remaining,expected,wrote,crc;U32 bits;unsigned used;unsigned ring;int error; } Decode;
static U32 rle32(FILE*f){U8 b[4];if(fread(b,1,4,f)!=4)return 0xffffffffU;return (U32)b[0]|((U32)b[1]<<8)|((U32)b[2]<<16)|((U32)b[3]<<24);}
static U16 rle16(FILE*f){U8 b[2];if(fread(b,1,2,f)!=2)return 0xffffU;return (U16)((U16)b[0]|((U16)b[1]<<8));}
static int seek(FILE*f,U32 n){if(n>OUTMAX)return 0;return fseek(f,(long)n,SEEK_SET)==0;}
static void crc_init(void){U32 c;unsigned i,j;for(i=0;i<256;i++){c=(U32)i;for(j=0;j<8;j++)c=(c>>1)^((c&1U)?0xedb88320U:0U);crc_table[i]=c;}}
static int bbit(Decode*d){unsigned char c;if(!d->used){if(!d->remaining||fread(&c,1,1,d->f)!=1){d->error=1;return 0;}d->remaining--;d->bits=(U32)c;d->used=8;}c=(U8)(d->bits&1U);d->bits>>=1;d->used--;return (int)c;}
static U32 bget(Decode*d,unsigned n){U32 v=0U;unsigned k;for(k=0;k<n&&!d->error;k++)v|=((U32)bbit(d)<<k);return v;}
static void balign(Decode*d){d->bits=0U;d->used=0U;}
static U16 newnode(void){U16 v;if(hcount>=HCAP)return 0U;v=(U16)hcount++;hnode[v].branch[0]=hnode[v].branch[1]=0U;hnode[v].val=-1;return v;}
static int hbuild(U16 *root,const U8 *lens,unsigned n){unsigned bl[16],next[16],i,j,code=0U;int left=1;U16 cur;memset(bl,0,sizeof(bl));memset(next,0,sizeof(next));for(i=0;i<n;i++){if(lens[i]>15U)return 0;if(lens[i])bl[lens[i]]++;}for(i=1;i<16;i++){left=(left<<1)-(int)bl[i];if(left<0)return 0;}
 for(i=1;i<16;i++){code=(code+bl[i-1])<<1;next[i]=code;}
 *root=newnode();if(!*root)return 0;
 for(i=0;i<n;i++)if(lens[i]){unsigned c=next[lens[i]]++;cur=*root;for(j=lens[i];j>0;j--){unsigned dir=(c>>(j-1U))&1U;U16 ch=hnode[cur].branch[dir];if(!ch){ch=newnode();if(!ch)return 0;hnode[cur].branch[dir]=ch;}cur=ch;}if(hnode[cur].val!=-1)return 0;hnode[cur].val=(short)i;}
 return 1;}
static int hdecode(Decode*d,U16 root){U16 cur=root;unsigned i;for(i=0;i<16&&!d->error;i++){int bit=bbit(d);if(d->error)return -1;cur=hnode[cur].branch[bit];if(!cur){d->error=1;return -1;}if(hnode[cur].val>=0)return (int)hnode[cur].val;}d->error=1;return -1;}
static void emit(Decode*d,U8 c){if(d->error)return;if(d->wrote>=d->expected){d->error=1;return;}history[d->ring]=c;d->ring=(d->ring+1U)&32767U;d->crc=(d->crc>>8)^crc_table[(d->crc^(U32)c)&255U];d->wrote++;if(fputc((int)c,d->out)==EOF)d->error=1;}
static int fixedh(U16*lit,U16*dist){U8 ll[288],dd[32];unsigned i;hcount=1U;for(i=0;i<288;i++)ll[i]=(U8)((i<144)?8:((i<256)?9:((i<280)?7:8)));for(i=0;i<32;i++)dd[i]=5;return hbuild(lit,ll,288)&&hbuild(dist,dd,32);}
static int dynamich(Decode*d,U16*lit,U16*dist){static const U8 perm[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};U8 clen[19],ll[288+32];U16 code;unsigned nl,nd,nc,i=0,j,run,v;int sym;nl=bget(d,5)+257U;nd=bget(d,5)+1U;nc=bget(d,4)+4U;if(nl>286||nd>32||nc>19){d->error=1;return 0;}memset(clen,0,sizeof(clen));memset(ll,0,sizeof(ll));for(j=0;j<nc;j++)clen[perm[j]]=(U8)bget(d,3);hcount=1U;if(!hbuild(&code,clen,19))return 0;
 while(i<nl+nd&&!d->error){sym=hdecode(d,code);if(sym<0)return 0;if(sym<16){ll[i++]=(U8)sym;continue;}if(sym==16){if(i==0){d->error=1;return 0;}v=ll[i-1];run=(unsigned)bget(d,2)+3U;}else if(sym==17){v=0;run=(unsigned)bget(d,3)+3U;}else if(sym==18){v=0;run=(unsigned)bget(d,7)+11U;}else{d->error=1;return 0;}if(run>nl+nd-i){d->error=1;return 0;}while(run--)ll[i++]=(U8)v;}
 hcount=1U;return hbuild(lit,ll,nl)&&hbuild(dist,ll+nl,nd);}
static int inflate(Decode*d){static const unsigned lb[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};static const U8 le[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};static const unsigned db[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};static const U8 de[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};U16 lit,dist;unsigned last=0U,typ,n,len,k,offs;int sym;
 d->ring=0;d->bits=0;d->used=0;d->crc=0xffffffffU;d->wrote=0;
 while(!last&&!d->error){last=bget(d,1);typ=bget(d,2);if(typ==0U){balign(d);len=bget(d,16);n=bget(d,16);if((len^(unsigned)n)!=65535U){d->error=1;break;}for(k=0;k<len&&!d->error;k++)emit(d,(U8)bget(d,8));continue;}if(typ==1U){if(!fixedh(&lit,&dist)){d->error=1;break;}}else if(typ==2U){if(!dynamich(d,&lit,&dist)){d->error=1;break;}}else{d->error=1;break;}
  for(;;){sym=hdecode(d,lit);if(sym<0){d->error=1;break;}if(sym<256){emit(d,(U8)sym);continue;}if(sym==256)break;if(sym>285){d->error=1;break;}sym-=257;len=lb[sym]+(unsigned)bget(d,le[sym]);sym=hdecode(d,dist);if(sym<0||sym>=30){d->error=1;break;}offs=db[sym]+(unsigned)bget(d,de[sym]);if(offs>32768U||offs>d->wrote){d->error=1;break;}for(k=0;k<len&&!d->error;k++)emit(d,history[(d->ring+32768U-offs)&32767U]);}
 }
 return !d->error && d->wrote==d->expected;
}
static int copyraw(FILE*in,FILE*out,U32 size,U32*crc){U8 buf[16384];U32 n,j,c=0xffffffffU;while(size){n=size>sizeof(buf)?(U32)sizeof(buf):size;if(fread(buf,1,(size_t)n,in)!=(size_t)n)return 0;if(fwrite(buf,1,(size_t)n,out)!=(size_t)n)return 0;for(j=0;j<n;j++)c=(c>>8)^crc_table[(c^(U32)buf[j])&255U];size-=n;}*crc=c^0xffffffffU;return 1;}
static int extract(FILE*f,U32 pos,U16 method,U16 flags,U32 compressed,U32 original,U32 expectedcrc,const char*outname){U32 at,data,crc=0;U16 nf,extra;FILE*out;Decode d;int ok;
 if(flags&1U||!(method==0U||method==8U)){fprintf(stderr,"gro: unsupported method or encrypted entry\n");return 0;}
 if(!seek(f,pos)||rle32(f)!=0x04034b50U)return 0;
 if(fseek(f,22L,SEEK_CUR))return 0;
 nf=rle16(f);extra=rle16(f);at=(U32)ftell(f);if(at>OUTMAX-(U32)nf-(U32)extra)return 0;data=at+(U32)nf+(U32)extra;if(!seek(f,data))return 0;
 out=fopen(outname,"wb");if(!out)return 0;
 if(method==0U){ok=(compressed==original)&&copyraw(f,out,compressed,&crc);}else{memset(&d,0,sizeof(d));d.f=f;d.out=out;d.remaining=compressed;d.expected=original;ok=inflate(&d);crc=d.crc^0xffffffffU;}
 if(fclose(out)!=0)ok=0;
 if(!ok||crc!=expectedcrc){remove(outname);fprintf(stderr,"gro: CRC/decompression failed\n");return 0;}printf("extracted %s (%u bytes)\n",outname,(unsigned)original);return 1;
}
static int gro(const char*archive,const char*target,const char*out){FILE*f;long total;U32 search,base,eocd=0,central,offset,size,crc,packed,unpacked,local;U16 count,i,method,flags,nf,extra,comment,disk;char name[2048];int done=0;f=fopen(archive,"rb");if(!f){fprintf(stderr,"gro: cannot open archive\n");return 1;}if(fseek(f,0L,SEEK_END)||((total=ftell(f))<0L)||total>2147483647L){fclose(f);return 1;}
 if(total<22L){fclose(f);return 1;}
 search=(U32)((total>65557L)?total-65557L:0L);
 for(base=(U32)total-22U;;base--){if(base<search)break;if(!seek(f,base))break;if(rle32(f)==0x06054b50U){eocd=base;break;}if(base==0U)break;}
 if(!eocd){fprintf(stderr,"gro: zip end-of-directory missing\n");fclose(f);return 1;}
 if(!seek(f,eocd+4U)){fclose(f);return 1;}disk=rle16(f);(void)rle16(f);count=rle16(f);(void)rle16(f);size=rle32(f);central=rle32(f);if(disk||central>OUTMAX||size>OUTMAX-central){fclose(f);return 1;}crc_init();if(!seek(f,central)){fclose(f);return 1;}
 printf("GRO ZIP: entries=%u\n",(unsigned)count);
 for(i=0;i<count;i++){
  if(rle32(f)!=0x02014b50U){fprintf(stderr,"gro: invalid central directory\n");break;}
  if(fseek(f,4L,SEEK_CUR))break;
  flags=rle16(f);method=rle16(f);if(fseek(f,4L,SEEK_CUR))break;crc=rle32(f);packed=rle32(f);unpacked=rle32(f);nf=rle16(f);extra=rle16(f);comment=rle16(f);if(fseek(f,8L,SEEK_CUR))break;local=rle32(f);
  if(nf==0||nf>=sizeof(name)){fprintf(stderr,"gro: entry filename too long\n");break;}if(fread(name,1,nf,f)!=(size_t)nf)break;name[nf]=0;if(fseek(f,(long)extra+(long)comment,SEEK_CUR))break;
  if(!target)printf("  %s (%u bytes; method=%u)\n",name,(unsigned)unpacked,(unsigned)method);
  else if(!strcmp(name,target)){offset=(U32)ftell(f);done=extract(f,local,method,flags,packed,unpacked,crc,out);(void)seek(f,offset);break;}
 }
 if(target&&!done)fprintf(stderr,"gro: target not found or extraction failed: %s\n",target);
 fclose(f);return target?!done:0;
}
int main(int argc,char**argv){if(argc==3&&!strcmp(argv[2],"--list"))return gro(argv[1],NULL,NULL);if(argc==5&&!strcmp(argv[2],"--extract"))return gro(argv[1],argv[3],argv[4]);fprintf(stderr,"usage: ss89gro archive.gro --list\n       ss89gro archive.gro --extract Models/path.mdl local.mdl\n");return 2;}
