/* SeriousSam Classic SKA pose decoder: fresh C89 implementation based on
 * externally documented Croteam Serious Engine 1 serialisation layouts.
 * No engine source code linked or copied. No heap, no floating operations,
 * no 64-bit integers. All geometry uses signed Q16.16 fixed point.
 * Original character resources needed to verify appearance/compatibility.
 */
#include "ss89skin.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#if UINT_MAX != 0xffffffffU || INT_MAX != 2147483647 || USHRT_MAX != 65535
#error SS89SKIN requires 32-bit unsigned/int and 16-bit short
#endif
#define ONE 65536
#define CAP(a,b) ((a) < (b) ? (a) : (b))
#define ABSENT 0xffffffffU

typedef struct Reader { FILE *f; unsigned bytes; int err; } Reader;
typedef struct Weight { unsigned vertex, map; SS89_i32 weight; } Weight;
typedef struct Bone { char name[64], parent_name[64]; int parent; SS89_i32 bind[12], world[12], skin[12]; SS89S_Point rest; SS89S_Quat q; SS89_i32 len; } Bone;
typedef struct PosKey { unsigned frame; SS89S_Point p; } PosKey;
typedef struct RotKey { unsigned frame; SS89S_Quat q; } RotKey;
typedef struct Channel { char name[64]; unsigned pos_start, pos_count, rot_start, rot_count; SS89_i32 offset; } Channel;
typedef struct Clip { char name[64]; unsigned frames, chan_start, chan_count; SS89_i32 spf; } Clip;
typedef struct Workspace {
 unsigned nv, nt, nwm, nw, nb, nc, nch, np, nr, selected_lod, mesh_lods, skeleton_lods;
 SS89S_Point base[SS89S_MAX_VERTICES];
 SS89S_Point final[SS89S_MAX_VERTICES];
 SS89_i32 uv[SS89S_MAX_VERTICES][2];
 unsigned has_uv;
 SS89_Triangle triangles[SS89S_MAX_TRIANGLES];
 char weight_map_names[SS89S_MAX_MAPS][64];
 Weight weights[SS89S_MAX_WEIGHTS];
 Bone bones[SS89S_MAX_BONES];
 unsigned order[SS89S_MAX_BONES];
 Channel channels[SS89S_MAX_CHANNELS];
 Clip clips[SS89S_MAX_CLIPS];
 PosKey pk[SS89S_MAX_KEYS];
 RotKey rk[SS89S_MAX_KEYS];
} Workspace;
static Workspace W;
static char issue[128]="OK";
static void fail(const char *s){strncpy(issue,s,sizeof(issue)-1U);issue[sizeof(issue)-1U]=0;}
const char *ss89s_error(void){return issue;}
static unsigned magnitude(SS89_i32 v){return v<0?(unsigned)(-(v+1))+1U:(unsigned)v;}
static SS89_i32 sat(int neg,unsigned u){if(neg){if(u>=2147483648U)return (-2147483647-1);return -(SS89_i32)u;}if(u>2147483647U)return 2147483647;return (SS89_i32)u;}
static SS89_i32 addq(SS89_i32 a,SS89_i32 b){if(b>0 && a>2147483647-b)return 2147483647;if(b<0 && a<(-2147483647-1)-b)return (-2147483647-1);return a+b;}
static SS89_i32 negq(SS89_i32 a){return a==(-2147483647-1)?2147483647:-a;}
static SS89_i32 subq(SS89_i32 a,SS89_i32 b){return addq(a,negq(b));}
/* 16-bit limb multiplication, uses no implicit 64-bit intermediate. */
static SS89_i32 mulq(SS89_i32 a,SS89_i32 b){
 unsigned x=magnitude(a),y=magnitude(b),h=(x>>16)*(y>>16),v=(x>>16)*(y&65535U),z=(x&65535U)*(y>>16),lo=(x&65535U)*(y&65535U);
 unsigned out;int negative=(a<0)!=(b<0);
 if(h>32767U)return negative?(-2147483647-1):2147483647;
 out=h<<16;
 if(v>2147483648U-out)return negative?(-2147483647-1):2147483647;
 out+=v;
 if(z>2147483648U-out)return negative?(-2147483647-1):2147483647;
 out+=z;
 if((lo>>16)>2147483648U-out)return negative?(-2147483647-1):2147483647;
 out+=(lo>>16);
 return sat(negative,out);
}
/* Binary long division (numerator << 16) / denominator. */
static SS89_i32 divq(SS89_i32 a,SS89_i32 b){unsigned n,d,whole,rem,frac=0U;int i,negative;
 if(!b)return a<0?(-2147483647-1):2147483647;
 negative=(a<0)!=(b<0);n=magnitude(a);d=magnitude(b);whole=n/d;rem=n%d;
 if(whole>=32768U)return negative?(-2147483647-1):2147483647;
 for(i=0;i<16;i++){frac<<=1;if(rem>=d-rem){rem=rem-(d-rem);frac|=1U;}else rem*=2U;}
 return sat(negative,(whole<<16)|frac);
}
static SS89_i32 sqrtq(SS89_i32 q){SS89_i32 x,next;int i;if(q<=0)return 0;x=(q>ONE)?q:ONE;for(i=0;i<18;i++){next=addq(x,divq(q,x))/2;if(next==x||next==x-1||next==x+1)break;x=next;}return next;}
static void normalize(SS89S_Quat *q){SS89_i32 s=0,mag; s=addq(s,mulq(q->w,q->w));s=addq(s,mulq(q->x,q->x));s=addq(s,mulq(q->y,q->y));s=addq(s,mulq(q->z,q->z)); if(s<16){q->w=ONE;q->x=q->y=q->z=0;return;} mag=sqrtq(s);if(mag==0)return;q->w=divq(q->w,mag);q->x=divq(q->x,mag);q->y=divq(q->y,mag);q->z=divq(q->z,mag);}
static SS89_i32 lerp(SS89_i32 a,SS89_i32 b,SS89_i32 t){return addq(a,mulq(subq(b,a),t));}
static SS89S_Quat nlerp(SS89S_Quat a,SS89S_Quat b,SS89_i32 t){SS89S_Quat q;SS89_i32 dot=0;dot=addq(dot,mulq(a.w,b.w));dot=addq(dot,mulq(a.x,b.x));dot=addq(dot,mulq(a.y,b.y));dot=addq(dot,mulq(a.z,b.z));if(dot<0){b.w=negq(b.w);b.x=negq(b.x);b.y=negq(b.y);b.z=negq(b.z);}q.w=lerp(a.w,b.w,t);q.x=lerp(a.x,b.x,t);q.y=lerp(a.y,b.y,t);q.z=lerp(a.z,b.z,t);normalize(&q);return q;}
static int rbytes(Reader *r,void *dst,unsigned n){long p=ftell(r->f);if(r->err||p<0||(unsigned long)p>r->bytes||n>r->bytes-(unsigned)p||fread(dst,1U,n,r->f)!=n){r->err=1;return 0;}return 1;}
static int rskip(Reader *r,unsigned n){long p=ftell(r->f);if(r->err||p<0||(unsigned long)p>r->bytes||n>r->bytes-(unsigned)p||fseek(r->f,(long)n,SEEK_CUR)!=0){r->err=1;return 0;}return 1;}
static unsigned r32(Reader *r){unsigned char b[4];if(!rbytes(r,b,4U))return 0;return (unsigned)b[0]|((unsigned)b[1]<<8)|((unsigned)b[2]<<16)|((unsigned)b[3]<<24);}
static unsigned r16(Reader *r){unsigned char b[2];if(!rbytes(r,b,2U))return 0;return (unsigned)b[0]|((unsigned)b[1]<<8);}
static SS89_i32 decode_float_bits(unsigned bits,Reader *r){unsigned e=(bits>>23)&255U,m=bits&0x7fffffU,mag;int sh,neg=(bits>>31)!=0U;
 if(e==255U){r->err=1;return 0;}if(e==0U)return 0;
 m|=0x800000U;sh=(int)e-134;if(sh< -23)return 0;
 if(sh<0)mag=m>>(unsigned)(-sh);
 else if(sh>7||m>(2147483648U>>(unsigned)sh))mag=2147483648U;
 else mag=m<<(unsigned)sh;
 return sat(neg,mag);
}
static SS89_i32 rq(Reader *r){return decode_float_bits(r32(r),r);}
static unsigned count(Reader *r,unsigned max){unsigned n=r32(r);if(n>max)r->err=1;return n;}
static int str(Reader *r,char *out,unsigned cap){unsigned n=count(r,1048576U),use=n;if(r->err)return 0;if(out&&cap){if(use>=cap)use=cap-1U;if(!rbytes(r,out,use))return 0;out[use]=0;return rskip(r,n-use);}return rskip(r,n);}
static int tag(Reader *r,const char *name){char b[4];if(!rbytes(r,b,4U)||memcmp(b,name,4U)){r->err=1;return 0;}return 1;}
static int skiparray(Reader *r,unsigned n,unsigned elem){if(elem&&n>2147483647U/elem){r->err=1;return 0;}return rskip(r,n*elem);}
static int start(Reader *r,const char*filename){long size;memset(r,0,sizeof(*r));r->f=fopen(filename,"rb");if(!r->f){fail("cannot open file");return 0;}if(fseek(r->f,0L,SEEK_END)||(size=ftell(r->f))<0L||size>2147483647L||fseek(r->f,0L,SEEK_SET)){fclose(r->f);r->f=NULL;fail("invalid file size");return 0;}r->bytes=(unsigned)size;return 1;}
static int finish(Reader*r){int ret=!r->err;if(r->f)fclose(r->f);if(!ret)fail("corrupted, truncated, excessive or unsupported binary resource");return ret;}
static void matrix_from_q(SS89_i32 m[12],SS89S_Point p,SS89S_Quat q){SS89_i32 xx,yy,zz,xy,xz,yz,wx,wy,wz;
 normalize(&q);xx=mulq(q.x,q.x);yy=mulq(q.y,q.y);zz=mulq(q.z,q.z);xy=mulq(q.x,q.y);xz=mulq(q.x,q.z);yz=mulq(q.y,q.z);wx=mulq(q.w,q.x);wy=mulq(q.w,q.y);wz=mulq(q.w,q.z);
 m[0]=subq(ONE,mulq(131072,addq(yy,zz)));m[1]=mulq(131072,subq(xy,wz));m[2]=mulq(131072,addq(xz,wy));m[3]=p.x;
 m[4]=mulq(131072,addq(xy,wz));m[5]=subq(ONE,mulq(131072,addq(xx,zz)));m[6]=mulq(131072,subq(yz,wx));m[7]=p.y;
 m[8]=mulq(131072,subq(xz,wy));m[9]=mulq(131072,addq(yz,wx));m[10]=subq(ONE,mulq(131072,addq(xx,yy)));m[11]=p.z;
}
static void mmul(SS89_i32 out[12],const SS89_i32 a[12],const SS89_i32 b[12]){SS89_i32 z[12];int row,col,k;for(row=0;row<3;row++)for(col=0;col<4;col++){SS89_i32 v=(col==3)?a[row*4+3]:0;for(k=0;k<3;k++)v=addq(v,mulq(a[row*4+k],b[k*4+col]));z[row*4+col]=v;}memcpy(out,z,sizeof(z));}
static void invrigid(SS89_i32 dst[12],const SS89_i32 src[12]){int row,col,k;SS89_i32 inv[12];for(row=0;row<3;row++){for(col=0;col<3;col++)inv[row*4+col]=src[col*4+row];inv[row*4+3]=0;for(k=0;k<3;k++)inv[row*4+3]=subq(inv[row*4+3],mulq(inv[row*4+k],src[k*4+3]));}memcpy(dst,inv,sizeof(inv));}
static SS89S_Point trans(const SS89_i32 m[12],SS89S_Point p){SS89S_Point out;out.x=addq(m[3],addq(mulq(m[0],p.x),addq(mulq(m[1],p.y),mulq(m[2],p.z))));out.y=addq(m[7],addq(mulq(m[4],p.x),addq(mulq(m[5],p.y),mulq(m[6],p.z))));out.z=addq(m[11],addq(mulq(m[8],p.x),addq(mulq(m[9],p.y),mulq(m[10],p.z))));return out;}

int ss89s_load_mesh(const char *path,unsigned lod){Reader r;unsigned ver,lods,l,n,i,j,k,nuv,nsurf,ntri,has,texn,uvn,coln,flt,mapn,morphn,ct,first,span,tri0;char s[96];
 fail("OK");W.nv=W.nt=W.nwm=W.nw=W.has_uv=0U;
 if(!start(&r,path))return 0;
 if(!tag(&r,"MESH")){finish(&r);return 0;}ver=r32(&r);if(ver!=11U&&ver!=12U)r.err=1;lods=count(&r,128U);if(lod>=lods)r.err=1;
 for(l=0U;l<lods&&!r.err;l++){
  str(&r,s,sizeof(s));(void)rq(&r);(void)r32(&r);
  n=count(&r,SS89S_MAX_VERTICES);
  for(i=0;i<n&&!r.err;i++){SS89_i32 x=rq(&r),y=rq(&r),z=rq(&r);(void)r32(&r);if(l==lod){W.base[i].x=x;W.base[i].y=y;W.base[i].z=z;W.final[i]=W.base[i];}}
  if(l==lod)W.nv=n;
  skiparray(&r,n,16U);nuv=count(&r,256U);
  for(i=0;i<nuv&&!r.err;i++){str(&r,s,sizeof(s));if(l==lod&&i==0U){W.has_uv=1U;for(j=0;j<n&&!r.err;j++){W.uv[j][0]=rq(&r);W.uv[j][1]=rq(&r);}}else skiparray(&r,n,8U);}
  nsurf=count(&r,4096U);for(i=0;i<nsurf&&!r.err;i++){
   str(&r,s,sizeof(s));first=r32(&r);span=r32(&r);ntri=count(&r,SS89S_MAX_TRIANGLES);
   if(first>n||span>n-first){r.err=1;break;}
   if(l==lod){tri0=W.nt;if(ntri>SS89S_MAX_TRIANGLES-W.nt){r.err=1;break;}W.nt+=ntri;
    for(j=0;j<ntri&&!r.err;j++){SS89_Triangle *t=&W.triangles[tri0+j];for(k=0;k<3U;k++){t->v[k]=r32(&r);t->uv[k]=t->v[k];if(t->v[k]>=n)r.err=1;}t->surface=i;t->color=0U;}}
   else skiparray(&r,ntri,12U);
   has=r32(&r);if(has>1U){r.err=1;break;}if(has){texn=count(&r,4096U);uvn=count(&r,4096U);coln=count(&r,4096U);flt=count(&r,4096U);str(&r,s,sizeof(s));for(j=0;j<texn&&!r.err;j++)str(&r,s,sizeof(s));skiparray(&r,uvn,4U);skiparray(&r,coln,4U);skiparray(&r,flt,4U);if(ver>=12U)(void)r32(&r);}
  }
  mapn=count(&r,SS89S_MAX_MAPS);
  for(i=0;i<mapn&&!r.err;i++){
   str(&r,s,sizeof(s));ct=count(&r,SS89S_MAX_WEIGHTS);
   if(l==lod){if(W.nwm>=SS89S_MAX_MAPS||ct>SS89S_MAX_WEIGHTS-W.nw){r.err=1;break;}{size_t length=strlen(s);if(length>63U)length=63U;memcpy(W.weight_map_names[W.nwm],s,length);W.weight_map_names[W.nwm][length]=0;}
    for(j=0;j<ct&&!r.err;j++){Weight*w=&W.weights[W.nw++];w->vertex=r32(&r);w->weight=rq(&r);w->map=W.nwm;if(w->vertex>=n||w->weight<0||w->weight>ONE)r.err=1;}W.nwm++;
   }else skiparray(&r,ct,8U);
  }
  morphn=count(&r,4096U);for(i=0;i<morphn&&!r.err;i++){str(&r,s,sizeof(s));(void)r32(&r);ct=count(&r,SS89S_MAX_WEIGHTS);skiparray(&r,ct,32U);}
 }
 if(!finish(&r)){W.nv=W.nt=W.nwm=W.nw=W.has_uv=0U;return 0;}
 W.mesh_lods=lods;W.selected_lod=lod;return 1;
}
int ss89s_load_skeleton(const char*path,unsigned lod){Reader r;unsigned l,n,i,j,k,lods,done=0U;int progress;char s[80];fail("OK");W.nb=0U;
 if(!start(&r,path))return 0;
 if(!tag(&r,"SKEL")){finish(&r);return 0;}
 if(r32(&r)!=6U)r.err=1;
 lods=count(&r,128U);if(lod>=lods)r.err=1;
 for(l=0U;l<lods&&!r.err;l++){
  str(&r,s,sizeof(s));(void)rq(&r);n=count(&r,SS89S_MAX_BONES);if(l==lod)W.nb=n;
  for(i=0;i<n&&!r.err;i++){
   Bone *b=&W.bones[i];if(l==lod){str(&r,b->name,sizeof(b->name));str(&r,b->parent_name,sizeof(b->parent_name));}else{str(&r,s,sizeof(s));str(&r,s,sizeof(s));}
   for(j=0;j<12U&&!r.err;j++){SS89_i32 v=rq(&r);if(l==lod)b->bind[j]=v;}
   for(j=0;j<3U&&!r.err;j++){SS89_i32 v=rq(&r);if(l==lod){if(j==0U)b->rest.x=v;else if(j==1U)b->rest.y=v;else b->rest.z=v;}}
   for(j=0;j<4U&&!r.err;j++){SS89_i32 v=rq(&r);if(l==lod){if(j==0)b->q.w=v;else if(j==1)b->q.x=v;else if(j==2)b->q.y=v;else b->q.z=v;}}
   {SS89_i32 off=rq(&r);(void)rq(&r);if(l==lod)b->len=off;}
  }
 }
 if(!finish(&r)){W.nb=0U;return 0;}
 W.skeleton_lods=lods;
 for(i=0;i<W.nb;i++){Bone*b=&W.bones[i];b->parent=-1;if(b->parent_name[0]){for(j=0;j<W.nb;j++)if(!strcmp(b->parent_name,W.bones[j].name)){b->parent=(int)j;break;}if(j==W.nb){fail("skeleton missing parent bone");W.nb=0U;return 0;}}if(b->parent==(int)i){fail("bone cannot parent itself");W.nb=0U;return 0;}}
 /* Topological sort, detects ancestor cycles without recursion. */
 {unsigned char visited[SS89S_MAX_BONES];memset(visited,0,sizeof(visited));while(done<W.nb){progress=0;for(k=0;k<W.nb;k++){int par=W.bones[k].parent;if(!visited[k]&&(par<0||visited[(unsigned)par])){W.order[done++]=k;visited[k]=1;progress=1;}}if(!progress){fail("skeleton cyclic hierarchy");W.nb=0U;return 0;}}}
 return 1;
}
int ss89s_load_animation(const char*path){Reader r;unsigned clips,c,channels,k,m,p,rots,morph,fc,i;char s[96];fail("OK");W.nc=W.nch=W.np=W.nr=0U;
 if(!start(&r,path))return 0;
 if(!tag(&r,"ANIM")){finish(&r);return 0;}
 if(r32(&r)!=14U)r.err=1;
 clips=count(&r,SS89S_MAX_CLIPS);
 for(c=0;c<clips&&!r.err;c++){
  Clip *cl=&W.clips[c];str(&r,s,sizeof(s));str(&r,cl->name,sizeof(cl->name));cl->spf=rq(&r);cl->frames=count(&r,32767U);(void)rq(&r);(void)r32(&r);(void)r32(&r);channels=count(&r,SS89S_MAX_CHANNELS);if(cl->frames==0U||channels>SS89S_MAX_CHANNELS-W.nch)r.err=1;
  cl->chan_start=W.nch;cl->chan_count=channels;
  for(i=0;i<channels&&!r.err;i++){
   Channel *ch=&W.channels[W.nch++];str(&r,ch->name,sizeof(ch->name));skiparray(&r,12U,4U);
   p=count(&r,SS89S_MAX_KEYS);if(p>SS89S_MAX_KEYS-W.np)r.err=1;ch->pos_start=W.np;ch->pos_count=p;
   for(k=0;k<p&&!r.err;k++){PosKey*pk=&W.pk[W.np++];pk->frame=r16(&r);rskip(&r,2U);pk->p.x=rq(&r);pk->p.y=rq(&r);pk->p.z=rq(&r);if(pk->frame>=cl->frames||(k>0&&pk->frame<W.pk[W.np-2U].frame))r.err=1;}
   rots=count(&r,SS89S_MAX_KEYS);if(rots>SS89S_MAX_KEYS-W.nr)r.err=1;ch->rot_start=W.nr;ch->rot_count=rots;
   for(k=0;k<rots&&!r.err;k++){RotKey*rk=&W.rk[W.nr++];rk->frame=r16(&r);rskip(&r,2U);rk->q.w=rq(&r);rk->q.x=rq(&r);rk->q.y=rq(&r);rk->q.z=rq(&r);if(rk->frame>=cl->frames||(k>0&&rk->frame<W.rk[W.nr-2U].frame))r.err=1;}
   ch->offset=rq(&r);
  }
  morph=count(&r,SS89S_MAX_MAPS);for(m=0U;m<morph&&!r.err;m++){str(&r,s,sizeof(s));fc=count(&r,SS89S_MAX_KEYS);skiparray(&r,fc,4U);}
  W.nc++;
 }
 if(!finish(&r)){W.nc=W.nch=W.np=W.nr=0U;return 0;}
 return 1;
}
/* Key interval at frame+fraction, with wrap only when requested. */
static void interval(unsigned frames,unsigned frame,unsigned alpha,int loop,unsigned first,unsigned last,int rot,unsigned *a,unsigned*b,SS89_i32 *frac){unsigned i,start=0U,next=0U,lo,hi,idx0,idx1;unsigned base,n;
 n=last-first;*a=*b=first;*frac=0;if(!n)return;
 if(loop&&frames)frame%=frames;
 if(!loop){unsigned f0=rot?W.rk[first].frame:W.pk[first].frame;unsigned fend=rot?W.rk[last-1U].frame:W.pk[last-1U].frame;if(frame<f0|| (frame==f0&&alpha==0U))return;if(frame>=fend){*a=*b=last-1U;return;}}
 for(i=0;i<n;i++){unsigned key=rot?W.rk[first+i].frame:W.pk[first+i].frame;if(key<=frame){start=i;next=i+1U;}}
 if(next>=n){if(!loop){*a=*b=last-1U;return;}next=0U;}
 if(loop&&frame<(rot?W.rk[first].frame:W.pk[first].frame)){start=n-1U;next=0U;}
 idx0=first+start;idx1=first+next;
 lo=rot?W.rk[idx0].frame:W.pk[idx0].frame;
 hi=rot?W.rk[idx1].frame:W.pk[idx1].frame;
 if(loop&& (next<=start||hi<=lo))hi+=frames;
 if(loop&&frame<lo)frame+=frames;
 *a=idx0;*b=idx1;
 if(hi<=lo){*frac=0;return;}
 base=(frame-lo)*65536U+alpha;*frac=divq((SS89_i32)base,(SS89_i32)((hi-lo)*65536U));if(*frac<0)*frac=0;if(*frac>ONE)*frac=ONE;
}
static int findbone(const char *name){unsigned j;for(j=0U;j<W.nb;j++)if(!strcmp(name,W.bones[j].name))return (int)j;return -1;}
static void evalbone(SS89S_Point *p,SS89S_Quat *q,const Channel *ch,const Clip *clip,unsigned frame,unsigned alpha,int loop){unsigned a,b;SS89_i32 t;
 if(ch->pos_count){interval(clip->frames,frame,alpha,loop,ch->pos_start,ch->pos_start+ch->pos_count,0,&a,&b,&t);p->x=lerp(W.pk[a].p.x,W.pk[b].p.x,t);p->y=lerp(W.pk[a].p.y,W.pk[b].p.y,t);p->z=lerp(W.pk[a].p.z,W.pk[b].p.z,t);}
 if(ch->rot_count){interval(clip->frames,frame,alpha,loop,ch->rot_start,ch->rot_start+ch->rot_count,1,&a,&b,&t);*q=nlerp(W.rk[a].q,W.rk[b].q,t);}
}
int ss89s_pose(unsigned clip,unsigned frame,unsigned alpha,int loop){unsigned i,j,n;Clip *cl;SS89_i32 local[12],inv[12];SS89_i32 sum[SS89S_MAX_VERTICES];int map_bone[SS89S_MAX_MAPS];
 if(!W.nv||!W.nb||clip>=W.nc||alpha>=65536U){fail("missing mesh/skeleton/clip or invalid pose parameters");return 0;}
 cl=&W.clips[clip];if(!loop&&frame>=cl->frames){frame=cl->frames-1U;alpha=0;}
 for(n=0;n<W.nb;n++){i=W.order[n];{Bone*bone=&W.bones[i];SS89S_Point pos=bone->rest;SS89S_Quat q=bone->q;for(j=0;j<cl->chan_count;j++){Channel*ch=&W.channels[cl->chan_start+j];if(!strcmp(ch->name,bone->name)){evalbone(&pos,&q,ch,cl,frame,alpha,loop);if(ch->offset>0&&bone->len>0){SS89_i32 ratio=divq(bone->len,ch->offset);pos.x=mulq(pos.x,ratio);pos.y=mulq(pos.y,ratio);pos.z=mulq(pos.z,ratio);}break;}}
  matrix_from_q(local,pos,q);if(bone->parent<0)memcpy(bone->world,local,sizeof(local));else mmul(bone->world,W.bones[(unsigned)bone->parent].world,local);
  invrigid(inv,bone->bind);mmul(bone->skin,bone->world,inv);
 }}
 for(i=0;i<W.nv;i++){W.final[i].x=0;W.final[i].y=0;W.final[i].z=0;sum[i]=0;}
 for(j=0;j<W.nwm;j++)map_bone[j]=findbone(W.weight_map_names[j]);
 for(j=0;j<W.nw;j++){Weight*w=&W.weights[j];int ib=map_bone[w->map];if(ib>=0){SS89S_Point p=trans(W.bones[(unsigned)ib].skin,W.base[w->vertex]);SS89S_Point*out=&W.final[w->vertex];out->x=addq(out->x,mulq(p.x,w->weight));out->y=addq(out->y,mulq(p.y,w->weight));out->z=addq(out->z,mulq(p.z,w->weight));sum[w->vertex]=addq(sum[w->vertex],w->weight);}}
 for(i=0;i<W.nv;i++){if(sum[i]==0)W.final[i]=W.base[i];else if(sum[i]!=ONE){SS89_i32 norm=divq(ONE,sum[i]);W.final[i].x=mulq(W.final[i].x,norm);W.final[i].y=mulq(W.final[i].y,norm);W.final[i].z=mulq(W.final[i].z,norm);}}
 return 1;
}
void ss89s_info(SS89S_Info *dst){if(!dst)return;dst->vertices=W.nv;dst->triangles=W.nt;dst->bones=W.nb;dst->weights=W.nw;dst->clips=W.nc;dst->channels=W.nch;dst->selected_lod=W.selected_lod;dst->mesh_lods=W.mesh_lods;dst->skeleton_lods=W.skeleton_lods;}
const char*ss89s_clip_name(unsigned i){return i<W.nc?W.clips[i].name:NULL;}
unsigned ss89s_clip_frames(unsigned i){return i<W.nc?W.clips[i].frames:0U;}
unsigned ss89s_vertex_count(void){return W.nv;}
unsigned ss89s_triangle_count(void){return W.nt;}
int ss89s_get_vertex(unsigned i,SS89S_Point *out){if(i>=W.nv||!out)return 0;*out=W.final[i];return 1;}
int ss89s_get_triangle(unsigned i,SS89_Triangle *out){if(i>=W.nt||!out)return 0;*out=W.triangles[i];return 1;}
static void pfix(FILE *f,SS89_i32 q){unsigned n,whole,frac;if(q<0){fputc('-',f);n=magnitude(q);}else n=(unsigned)q;whole=n>>16;frac=((n&65535U)*1000U+32768U)>>16;if(frac>=1000U){whole++;frac=0U;}fprintf(f,"%u.%03u",whole,frac);}
int ss89s_export_obj(const char *path){FILE*f;unsigned i;f=fopen(path,"wb");if(!f){fail("cannot write OBJ");return 0;}fputs("# SKA skinned pose, Q16.16\n",f);for(i=0;i<W.nv;i++){fputs("v ",f);pfix(f,W.final[i].x);fputc(' ',f);pfix(f,W.final[i].y);fputc(' ',f);pfix(f,W.final[i].z);fputc('\n',f);}if(W.has_uv)for(i=0;i<W.nv;i++){fputs("vt ",f);pfix(f,W.uv[i][0]);fputc(' ',f);pfix(f,W.uv[i][1]);fputc('\n',f);}
 for(i=0;i<W.nt;i++){SS89_Triangle*t=&W.triangles[i];if(W.has_uv)fprintf(f,"f %u/%u %u/%u %u/%u\n",t->v[0]+1U,t->v[0]+1U,t->v[1]+1U,t->v[1]+1U,t->v[2]+1U,t->v[2]+1U);else fprintf(f,"f %u %u %u\n",t->v[0]+1U,t->v[1]+1U,t->v[2]+1U);}if(fclose(f)!=0){fail("OBJ write failed");return 0;}return 1;}
