/* DirectX .x legacy reader, subset of 0302/0303 txt/bin. Strict ISO C89.
 * No dynamic allocation, floating point types, scanf or dependency.
 * XYZ, normals, UV, polygons, MeshMaterialList, Material, TextureFilename,
 * skeleton SkinWeights (metadata), Frame and AnimationKey (metadata).
 * Frame transformations and skinning are NOT applied to OBJ positions.
 * Binary token IDs follow Direct3D X File Token Records documentation.
 */
#include "x_vendor.h"
#include "../vendor_common/fixed_matrix.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#define X_MAX_VERTICES 100000UL
#define X_MAX_FACES 100000UL
#define X_MAX_CORNERS 24UL
#define X_MAX_DEPTH 32
#define X_MAX_KEYS 100000UL
#define X_MAX_TOKENS 10000000UL
#define X_TEXT 0
#define X_BIN 1
#define T_END 0
#define T_ID 1
#define T_NUM 2
#define T_STR 3
#define T_SYM 4
#define T_BAD 5
#define X_MIN(a,b) ((a)<(b)?(a):(b))
typedef struct XTok {int kind, sym;char text[192];long fixed;unsigned long unsigned_val;int is_integer;} XTok;
typedef struct XFace {unsigned char n;unsigned long indices[X_MAX_CORNERS],nindices[X_MAX_CORNERS];unsigned short material;} XFace;
typedef struct XFrame {long world[16];unsigned long mesh_begin;} XFrame;
typedef struct XReader {
 FILE *f,*obj,*meta; X_Stats *s;long size;unsigned long tokens;
 int binary, width, bad, depth, in_animation, bake, frame_depth;
 XFrame frames[X_MAX_DEPTH+1]; unsigned long list_left;int list_kind;
 unsigned long voff,toff,noff,mesh_id; char frame[96];
 XTok look;int has_look;
} XReader;
/* Static bounded arena; intentionally non-reentrant; ~20 MB on Win32. */
static XFace faces[X_MAX_FACES];
static unsigned long norm_face_count;
static unsigned long face_count,vert_count,norm_count,tex_count,mat_count;
static long fix_from_ieee32(unsigned long b){
 unsigned long exp=(b>>23)&255UL,frac=b&0x7fffffUL,m;int sh;long val;
 if(exp==0UL)return 0L;
 if(exp==255UL)return 0L;
 m=(1UL<<23)|frac;sh=(int)exp-134;
 if(sh>=0){if(sh>7 || m>(2147483647UL>>sh))return (b&0x80000000UL)?-2147483647L:2147483647L;val=(long)(m<<sh);}
 else {if(sh< -31)return 0L;val=(long)(m>>(-sh));}
 return (b&0x80000000UL)?-val:val;
}
/* Decompose 64-bit IEEE as two 32-bit words. No C99 long long needed. */
static long fix_from_ieee64(unsigned long lo,unsigned long hi){
 unsigned long exp=(hi>>20)&2047UL,sign=hi&0x80000000UL;
 unsigned long mh=(hi&0xfffffUL)|0x100000UL,res=0UL;
 int sh, k;
 if(exp==0UL)return 0L;
 if(exp==2047UL)return 0L;
 sh=(int)exp-1059; /* mantissa 52 bits, times 2^16 */
 if(sh>=0)return sign?-2147483647L:2147483647L;
 k=-sh;
 if(k>=54)return 0L;
 if(k>=32){res=mh>>(k-32);}
 else if(k>0){
  if(mh>(2147483647UL>>(32-k)))return sign?-2147483647L:2147483647L;
  res=(mh<<(32-k)) | (lo>>k);
 } else return sign?-2147483647L:2147483647L;
 if(res>2147483647UL)return sign?-2147483647L:2147483647L;
 return sign?-(long)res:(long)res;
}
static void print_fixed(FILE *out,long value){
 unsigned long mag,whole,frac;
 if(!out)return;
 mag=value<0?(unsigned long)(-value):(unsigned long)value;
 whole=mag/65536UL;frac=((mag%65536UL)*10000UL+32768UL)/65536UL;
 if(frac>=10000UL){whole++;frac=0UL;}
 fprintf(out,"%s%lu.%04lu",value<0?"-":"",whole,frac);
}
static unsigned long rd32(FILE *f,int *ok){unsigned char b[4];
 if(fread(b,1,4,f)!=4){*ok=0;return 0;}
 return (unsigned long)b[0]|((unsigned long)b[1]<<8)|((unsigned long)b[2]<<16)|((unsigned long)b[3]<<24);
}
static unsigned rd16(FILE *f,int *ok){unsigned char b[2];
 if(fread(b,1,2,f)!=2){*ok=0;return 0;}
 return (unsigned)b[0]|((unsigned)b[1]<<8);
}
static int safe_read(XReader *r,unsigned long n){
 long at=ftell(r->f);
 return at>=0 && n<=(unsigned long)(r->size-at);
}
static long dec_fixed(const char *s){
 unsigned long whole=0UL,frac=0UL,pow=1UL;int neg=0,expo=0,esign=1,fr=0,any=0;
 if(*s=='-'||*s=='+'){neg=(*s=='-');s++;}
 while(*s && *s!='e' && *s!='E'){
  if(*s=='.'){fr=1;s++;continue;}
  if(*s<'0'||*s>'9')break;
  any=1;
  if(!fr){if(whole<100000UL)whole=whole*10UL+(unsigned long)(*s-'0');}
  else if(pow<1000000UL){frac=frac*10UL+(unsigned long)(*s-'0');pow*=10UL;}
  s++;
 }
 if(!any)return 0;
 if(*s=='e'||*s=='E'){
  s++;if(*s=='-'||*s=='+'){esign=(*s=='-')?-1:1;s++;}
  while(*s>='0'&&*s<='9'){if(expo<30)expo=expo*10+(*s-'0');s++;}
 }
 if(whole>=32768UL)return neg?-2147483647L:2147483647L;
 {
  unsigned long m=whole*65536UL,rem=frac,fraction=0UL;
  int i,adjust=expo*esign;
  /* Exact 16-bit fraction with only 32-bit unsigned arithmetic. */
  for(i=0;i<16;i++){
   rem*=2UL;fraction*=2UL;
   if(rem>=pow){rem-=pow;fraction++;}
  }
  if(rem*2UL>=pow && fraction<65535UL)fraction++;
  m+=fraction;
  if(adjust>15)return neg?-2147483647L:2147483647L;
  if(adjust< -15)return 0L;
  if(adjust>0)for(i=0;i<adjust;i++){if(m>214748364UL)return neg?-2147483647L:2147483647L;m*=10UL;}
  if(adjust<0)for(i=0;i< -adjust;i++)m/=10UL;
  if(m>2147483647UL)m=2147483647UL;
  return neg?-(long)m:(long)m;
 }
}
static XTok tx(int kind){XTok t;memset(&t,0,sizeof(t));t.kind=kind;return t;}
static XTok get_text(XReader *r){
 XTok t;int c,d,quote;unsigned long n=0;
 for(;;){
  c=fgetc(r->f);
  if(c==EOF)return tx(T_END);
  if(isspace((unsigned char)c))continue;
  if(c=='#'){while((c=fgetc(r->f))!=EOF && c!='\n'){}continue;}
  if(c=='/'){
   d=fgetc(r->f);if(d=='/') {while((c=fgetc(r->f))!=EOF&&c!='\n'){}continue;}
   if(d=='*'){
    int prev=0;while((c=fgetc(r->f))!=EOF){if(prev=='*'&&c=='/')break;prev=c;}
    if(c==EOF)return tx(T_BAD);
    continue;
   }
   if(d!=EOF)ungetc(d,r->f);
  }
  break;
 }
 if(c=='{'||c=='}'||c==';'||c==','||c=='<'||c=='>'||c=='['||c==']'){
  t=tx(T_SYM);t.sym=c;return t;
 }
 if(c=='"'){
  t=tx(T_STR);quote=0;
  while((c=fgetc(r->f))!=EOF){
   if(c=='"')break;
   if(c=='\\'){
    d=fgetc(r->f);if(d==EOF)return tx(T_BAD);
    if(d=='"'||d=='\\')c=d;else{if(n<sizeof(t.text)-1)t.text[n++]='\\';c=d;}
   }
   if(n>=sizeof(t.text)-1)return tx(T_BAD);
   t.text[n++]=(char)c;quote++;
  }
  if(c==EOF)return tx(T_BAD);
  (void)quote;return t;
 }
 t=tx(T_ID);
 for(;;){
  if(n>=sizeof(t.text)-1)return tx(T_BAD);
  t.text[n++]=(char)c;
  c=fgetc(r->f);
  if(c==EOF || isspace((unsigned char)c) || c=='{'||c=='}'||c==','||c==';'||c=='<'||c=='>'||c=='['||c==']')break;
 }
 if(c!=EOF && !isspace((unsigned char)c))ungetc(c,r->f);
 if((t.text[0]>='0'&&t.text[0]<='9')||t.text[0]=='-'||t.text[0]=='+'||t.text[0]=='.'){
  char *e;unsigned long u;
  t.kind=T_NUM;t.fixed=dec_fixed(t.text);
  u=strtoul(t.text,&e,10);
  t.is_integer=(*e=='\0' && t.text[0]!='-');t.unsigned_val=u;
 }
 return t;
}
static XTok get_bin(XReader *r){
 XTok t;int ok=1;unsigned tok;unsigned long n,i,a,b;
 if(r->list_left){
  t=tx(T_NUM);t.is_integer=(r->list_kind==6);
  if(!safe_read(r,(unsigned long)(r->list_kind==6?4:(r->width==64?8:4))))return tx(T_BAD);
  a=rd32(r->f,&ok);if(!ok)return tx(T_BAD);
  if(r->list_kind==6){t.unsigned_val=a;t.fixed=(a<=32767UL)?(long)(a*65536UL):2147483647L;}
  else if(r->width==64){b=rd32(r->f,&ok);if(!ok)return tx(T_BAD);t.fixed=fix_from_ieee64(a,b);}
  else t.fixed=fix_from_ieee32(a);
  r->list_left--;return t;
 }
 if(!safe_read(r,2UL))return tx(T_END);
 tok=rd16(r->f,&ok);if(!ok)return tx(T_BAD);
 if(tok==1U||tok==2U){
  n=rd32(r->f,&ok);if(!ok||n>65536UL||!safe_read(r,n))return tx(T_BAD);
  t=tx(tok==1U?T_ID:T_STR);
  for(i=0;i<n;i++){
   int c=fgetc(r->f);if(c==EOF)return tx(T_BAD);
   if(i<sizeof(t.text)-1)t.text[i]=(char)c;
  }
  if(n>=sizeof(t.text))return tx(T_BAD);
  /* Microsoft defines a DWORD delimiter after TOKEN_STRING. Some writers
     emit a WORD delimiter; accept either without swallowing the next token. */
  if(tok==2U && safe_read(r,2UL)){
   long mark=ftell(r->f);int delimiter=rd16(r->f,&ok);
   if(ok && (delimiter==19||delimiter==20)){
    if(safe_read(r,2UL)){
     int c1=fgetc(r->f),c2=fgetc(r->f);
     if(c1!=0 || c2!=0)fseek(r->f,-2L,SEEK_CUR);
    }
   }else fseek(r->f,mark,SEEK_SET);
  }
  return t;
 }
 if(tok==3U){
  a=rd32(r->f,&ok);if(!ok)return tx(T_BAD);
  t=tx(T_NUM);t.is_integer=1;t.unsigned_val=a;
  t.fixed=a<=32767UL?(long)(a*65536UL):2147483647L;return t;
 }
 if(tok==5U){if(!safe_read(r,16UL)||fseek(r->f,16L,SEEK_CUR))return tx(T_BAD);return tx(T_SYM);}
 if(tok==6U||tok==7U){
  n=rd32(r->f,&ok);if(!ok||n>10000000UL)return tx(T_BAD);
  if(n>(unsigned long)(r->size-ftell(r->f))/(unsigned long)(tok==6U?4:(r->width==64?8:4)))return tx(T_BAD);
  r->list_left=n;r->list_kind=(int)tok;
  if(n==0UL){t=tx(T_SYM);t.sym=';';return t;}
  return get_bin(r);
 }
 if(tok==10U||tok==11U||tok==12U||tok==13U||tok==14U||tok==15U||tok==16U||tok==17U||tok==19U||tok==20U){
  t=tx(T_SYM);t.sym=tok==10U?'{':tok==11U?'}':tok==12U?'(':tok==13U?')':tok==14U?'[':tok==15U?']':tok==16U?'<':tok==17U?'>':tok==19U?',':';';return t;
 }
 if(tok==31U){t=tx(T_ID);strcpy(t.text,"template");return t;}
 if(tok>=40U && tok<=52U){
  static const char *words[]={"WORD","DWORD","FLOAT","DOUBLE","CHAR","UCHAR","SWORD","SDWORD","VOID","LPSTR","UNICODE","CSTRING","array"};
  t=tx(T_ID);strcpy(t.text,words[tok-40U]);return t;
 }
 return tx(T_BAD);
}
static XTok next(XReader *r){XTok t;
 if(r->has_look){r->has_look=0;return r->look;}
 if(r->bad)return tx(T_BAD);
 if(++r->tokens>X_MAX_TOKENS){r->bad=1;return tx(T_BAD);}
 t=r->binary?get_bin(r):get_text(r);
 if(t.kind==T_BAD)r->bad=1;
 return t;
}
static void push(XReader *r,XTok t){r->look=t;r->has_look=1;}
static XTok value(XReader *r){XTok t;do{t=next(r);}while(t.kind==T_SYM && (t.sym==';'||t.sym==','));return t;}
static int unum(XReader *r,unsigned long *n){XTok t=value(r);
 if(t.kind!=T_NUM || !t.is_integer)return 0;
 *n=t.unsigned_val;return 1;
}
static int fixed(XReader *r,long *n){XTok t=value(r);if(t.kind!=T_NUM)return 0;*n=t.fixed;return 1;}
static int sym(XReader *r,int c){XTok t=value(r);return t.kind==T_SYM && t.sym==c;}
static int start_obj(XReader *r,char *name,size_t cap){XTok t=value(r);
 if(t.kind==T_ID || t.kind==T_STR){
  strncpy(name,t.text,cap-1);name[cap-1]=0;
  t=value(r);
 }else name[0]=0;
 return t.kind==T_SYM && t.sym=='{';
}
static int skip_block(XReader *r){XTok t;int depth=1;
 while(depth>0){t=next(r);
  if(t.kind==T_BAD||t.kind==T_END)return 0;
  if(t.kind==T_SYM && t.sym=='{')depth++;
  if(t.kind==T_SYM && t.sym=='}')depth--;
  if(depth>128)return 0;
 }
 return 1;
}
static int skip_object(XReader *r){char name[192];return start_obj(r,name,sizeof(name)) && skip_block(r);}
static void safe_text(FILE *f,const char *s){
 while(*s){unsigned char c=(unsigned char)*s++;fputc((c>=32U&&c<127U&&c!='\t'&&c!='\r')?c:'_',f);}
}
static void out_vec(FILE *f,const char *label,const long *v,int n){int i;
 if(!f)return;
 fputs(label,f);
 for(i=0;i<n;i++){fputc(' ',f);print_fixed(f,v[i]);}
 fputc('\n',f);
}
static int color(XReader *r,int components,long *p){int k;for(k=0;k<components;k++)if(!fixed(r,&p[k]))return 0;return 1;}
static int texture(XReader *r){char name[192];XTok t;
 if(!start_obj(r,name,sizeof(name)))return 0;
 t=value(r);if(t.kind!=T_STR && t.kind!=T_ID)return 0;
 r->s->textures++;
 if(r->meta){fprintf(r->meta,"TEXTURE\t%lu\t",r->mesh_id);safe_text(r->meta,t.text);fputc('\n',r->meta);}
 return sym(r,'}');
}
static int material(XReader *r,unsigned long id){
 char name[192];long data[11];XTok t;int i;
 if(!start_obj(r,name,sizeof(name)) || !color(r,4,data) || !fixed(r,&data[4]) || !color(r,3,data+5) || !color(r,3,data+8))return 0;
 r->s->materials++;
 if(r->meta){fprintf(r->meta,"MATERIAL\t%lu\t%lu\t",r->mesh_id,id);safe_text(r->meta,name);
  for(i=0;i<11;i++){fputc('\t',r->meta);print_fixed(r->meta,data[i]);}fputc('\n',r->meta);
 }
 for(;;){t=value(r);
  if(t.kind==T_SYM&&t.sym=='}')return 1;
  if(t.kind!=T_ID)return 0;
  if(!strcmp(t.text,"TextureFilename")||!strcmp(t.text,"TextureFileName")){if(!texture(r))return 0;}
  else if(!skip_object(r))return 0;
 }
}
static int meshmaterial(XReader *r){
 char name[192];unsigned long mats,nface,i,idx,material_id=0UL;XTok t;
 if(!start_obj(r,name,sizeof(name))||!unum(r,&mats)||!unum(r,&nface))return 0;
 if(mats>65535UL || nface!=face_count)return 0;
 mat_count=mats;
 for(i=0;i<nface;i++){
  if(!unum(r,&idx)||idx>=mats)return 0;
  faces[i].material=(unsigned short)idx;
 }
 for(;;){t=value(r);
  if(t.kind==T_SYM && t.sym=='}')return 1;
  if(t.kind==T_ID && !strcmp(t.text,"Material")){if(!material(r,material_id++))return 0;}
  else if(t.kind==T_SYM&&t.sym=='{'){
   /* referenced named material: only material ID retained */
   if(!skip_block(r))return 0;
  }else if(t.kind==T_ID){if(!skip_object(r))return 0;}
  else return 0;
 }
}
static int meshuv(XReader *r){char name[192];unsigned long n,i;long v[2];
 if(!start_obj(r,name,sizeof(name)) || !unum(r,&n) || n!=vert_count)return 0;
 for(i=0;i<n;i++){
  if(!color(r,2,v))return 0;
  out_vec(r->obj,"vt",v,2);
 }
 tex_count=n;r->s->texcoords+=n;
 return sym(r,'}');
}
static int meshnormals(XReader *r){char name[192];unsigned long n,nfaces,i,j,c,idx;long v[3];
 if(!start_obj(r,name,sizeof(name)) || !unum(r,&n)||n>X_MAX_VERTICES)return 0;
 norm_count=n;
 for(i=0;i<n;i++){
  if(!color(r,3,v))return 0;
  if(!r->bake)out_vec(r->obj,"vn",v,3);
 }
 if(!unum(r,&nfaces) || nfaces!=face_count)return 0;
 for(i=0;i<nfaces;i++){
  if(!unum(r,&c)||c!=faces[i].n)return 0;
  for(j=0;j<c;j++){
   if(!unum(r,&idx)||idx>=n)return 0;
   faces[i].nindices[j]=idx;
  }
 }
 norm_face_count=nfaces;r->s->normals+=n;
 return sym(r,'}');
}
static int skinweights(XReader *r){
 char name[192];XTok t;unsigned long n,i,idx;long w,m[16];int j;
 if(!start_obj(r,name,sizeof(name)))return 0;
 t=value(r);if(t.kind!=T_STR&&t.kind!=T_ID)return 0;
 strncpy(name,t.text,sizeof(name)-1);name[sizeof(name)-1]=0;
 if(!unum(r,&n)||n>vert_count)return 0;
 r->s->skin_clusters++;
 if(r->meta){fprintf(r->meta,"SKIN_CLUSTER\t%lu\t",r->mesh_id);safe_text(r->meta,name);fprintf(r->meta,"\t%lu\n",n);}
 /* Source format stores index array then weight array. Preserve index list in
    bounded static scratch; this is a fixed sized arena, not a heap. */
 {
  static unsigned long indices[X_MAX_VERTICES];
  for(i=0;i<n;i++)if(!unum(r,&idx)||idx>=vert_count)return 0;else indices[i]=idx;
  for(i=0;i<n;i++){
   if(!fixed(r,&w))return 0;
   if(r->meta){fprintf(r->meta,"SKIN\t%lu\t",r->mesh_id);safe_text(r->meta,name);
    fprintf(r->meta,"\t%lu\t",indices[i]);print_fixed(r->meta,w);fputc('\n',r->meta);
   }
  }
 }
 if(!color(r,16,m))return 0;
 if(r->meta){fprintf(r->meta,"SKIN_OFFSET\t%lu\t",r->mesh_id);safe_text(r->meta,name);
  for(j=0;j<16;j++){fputc('\t',r->meta);print_fixed(r->meta,m[j]);}fputc('\n',r->meta);
 }
 return sym(r,'}');
}
static int x_mesh(XReader *r){
 char name[192];XTok t;unsigned long n,i,j,c,idx,vbase,uvbase,nbase;
 long v[3];
 if(!start_obj(r,name,sizeof(name)))return 0;
 if(!unum(r,&n)||n>X_MAX_VERTICES)return 0;
 vert_count=n;vbase=r->voff;uvbase=r->toff;nbase=r->noff;
 face_count=norm_face_count=tex_count=norm_count=mat_count=0UL;
 r->mesh_id++;r->s->meshes++;
 if(r->obj){fprintf(r->obj,"o ");if(name[0])safe_text(r->obj,name);else fprintf(r->obj,"mesh_%lu",r->mesh_id);fputc('\n',r->obj);}
 if(r->meta){fprintf(r->meta,"MESH\t%lu\t",r->mesh_id);safe_text(r->meta,name);fprintf(r->meta,"\t%lu\n",n);}
 for(i=0;i<n;i++){
  if(!color(r,3,v))return 0;
  if(r->bake){
   long out[3];fx_transform_point(r->frames[r->frame_depth].world,v,out);
   out_vec(r->obj,"v",out,3);r->s->baked_vertices++;
  }else out_vec(r->obj,"v",v,3);
 }
 if(!unum(r,&face_count)||face_count>X_MAX_FACES)return 0;
 for(i=0;i<face_count;i++){
  if(!unum(r,&c)||c<3UL||c>X_MAX_CORNERS)return 0;
  faces[i].n=(unsigned char)c;faces[i].material=0U;
  for(j=0;j<c;j++){if(!unum(r,&idx)||idx>=n)return 0;faces[i].indices[j]=idx;}
 }
 for(;;){
  t=value(r);
  if(t.kind==T_SYM&&t.sym=='}')break;
  if(t.kind!=T_ID)return 0;
  if(!strcmp(t.text,"MeshNormals")){if(!meshnormals(r))return 0;}
  else if(!strcmp(t.text,"MeshTextureCoords")){if(!meshuv(r))return 0;}
  else if(!strcmp(t.text,"MeshMaterialList")){if(!meshmaterial(r))return 0;}
  else if(!strcmp(t.text,"SkinWeights")){if(!skinweights(r))return 0;}
  else {r->s->unsupported_objects++;if(!skip_object(r))return 0;}
 }
 /* Validate all indices before faces are generated, even without OBJ. */
 for(i=0;i<face_count;i++){
  if(mat_count && faces[i].material>=mat_count)return 0;
  for(j=0;j<faces[i].n;j++){
   if(faces[i].indices[j]>=vert_count || (norm_face_count && faces[i].nindices[j]>=norm_count))return 0;
  }
 }
 if(r->obj){
  unsigned long matprev=~0UL;
  for(i=0;i<face_count;i++){
   if(mat_count && faces[i].material!=matprev){fprintf(r->obj,"g mesh_%lu_material_%u\n",r->mesh_id,(unsigned)faces[i].material);matprev=faces[i].material;}
   for(j=1;j+1UL<(unsigned long)faces[i].n;j++){
    unsigned long k,corner;
    fprintf(r->obj,"f");
    for(k=0;k<3UL;k++){
     unsigned long nm,id;
     corner=k==0UL?0UL:k==1UL?j:j+1UL;
     id=faces[i].indices[corner]+vbase+1UL;
     fprintf(r->obj," %lu",id);
     if(tex_count || (norm_face_count && !r->bake)){
      fputc('/',r->obj);if(tex_count)fprintf(r->obj,"%lu",faces[i].indices[corner]+uvbase+1UL);
      if(norm_face_count && !r->bake){nm=faces[i].nindices[corner]+nbase+1UL;fprintf(r->obj,"/%lu",nm);}
     }
    }
    fputc('\n',r->obj);
   }
  }
 }
 r->voff+=vert_count;r->toff+=tex_count;r->noff+=norm_count;
 r->s->vertices+=vert_count;r->s->faces+=face_count;
 for(i=0;i<face_count;i++)r->s->triangles+=(unsigned long)faces[i].n-2UL;
 return 1;
}
static int frame_matrix(XReader *r){char name[192];long m[16];int i;
 if(!start_obj(r,name,sizeof(name))||!color(r,16,m)||!sym(r,'}'))return 0;
 if(r->bake){
  /* Streaming bake cannot correct prior meshes if a matrix appears late. */
  if(r->frames[r->frame_depth].mesh_begin!=r->s->meshes || !fx_affine(m))return 0;
  if(r->frame_depth>0){
   fx_matrix_mul(m,r->frames[r->frame_depth-1].world,
                 r->frames[r->frame_depth].world);
  }else{
   int k;for(k=0;k<16;k++)r->frames[0].world[k]=m[k];
  }
 }
 r->s->transforms++;
 if(r->meta){fprintf(r->meta,"FRAME_MATRIX\t");safe_text(r->meta,r->frame);
  for(i=0;i<16;i++){fputc('\t',r->meta);print_fixed(r->meta,m[i]);}fputc('\n',r->meta);
 }
 return 1;
}
static int animationkey(XReader *r){char name[192];unsigned long type,count,i,j,time,n;long val;
 if(!start_obj(r,name,sizeof(name))||!unum(r,&type)||!unum(r,&count)||count>X_MAX_KEYS)return 0;
 r->s->animation_keys++;
 for(i=0;i<count;i++){
  if(!unum(r,&time)||!unum(r,&n)||n>16UL)return 0;
  if(r->meta)fprintf(r->meta,"ANIM_KEY\t%lu\t%lu\t%lu\t%lu",type,i,time,n);
  for(j=0;j<n;j++){
   if(!fixed(r,&val))return 0;
   if(r->meta){fputc('\t',r->meta);print_fixed(r->meta,val);}
  }
  if(r->meta)fputc('\n',r->meta);
  r->s->keyframes++;
 }
 return sym(r,'}');
}
static int scan(XReader *r,int depth);
static int nested(XReader *r,int depth){XTok t;
 if(depth>X_MAX_DEPTH)return 0;
 for(;;){t=value(r);
  if(t.kind==T_SYM&&t.sym=='}')return 1;
  if(t.kind==T_SYM && t.sym=='{'){
   if(r->in_animation){
    XTok ref=value(r),end;
    if(ref.kind==T_ID){
     end=value(r);
     if(end.kind==T_SYM && end.sym=='}'){
      if(r->meta){fputs("ANIM_TARGET\t",r->meta);safe_text(r->meta,ref.text);fputc('\n',r->meta);}
      continue;
     }
     push(r,end);
    }else push(r,ref);
   }
   if(!skip_block(r))return 0;
   continue;
  }
  if(t.kind!=T_ID)return 0;
  push(r,t);if(!scan(r,depth+1))return 0;
 }
}
static int scan(XReader *r,int depth){XTok t;char name[192],old[96];
 if(depth>X_MAX_DEPTH)return 0;
 t=value(r);
 if(t.kind==T_END)return 1;
 if(t.kind==T_SYM && t.sym=='{')return skip_block(r);
 if(t.kind!=T_ID)return 0;
 if(!strcmp(t.text,"Mesh"))return x_mesh(r);
 if(!strcmp(t.text,"Frame")){
  strncpy(old,r->frame,sizeof(old));old[sizeof(old)-1]=0;
  if(!start_obj(r,name,sizeof(name)))return 0;
  if(r->frame_depth>=X_MAX_DEPTH)return 0;
  r->frame_depth++;
  {int k;for(k=0;k<16;k++)r->frames[r->frame_depth].world[k]=
                                 r->frames[r->frame_depth-1].world[k];}
  r->frames[r->frame_depth].mesh_begin=r->s->meshes;
  strncpy(r->frame,name,sizeof(r->frame)-1);r->frame[sizeof(r->frame)-1]=0;
  r->s->frames++;
  if(r->meta){fprintf(r->meta,"FRAME\t%lu\t",r->s->frames);safe_text(r->meta,name);fprintf(r->meta,"\t");safe_text(r->meta,old);fputc('\n',r->meta);}
  if(!nested(r,depth+1))return 0;
  strcpy(r->frame,old);r->frame_depth--;return 1;
 }
 if(!strcmp(t.text,"FrameTransformMatrix"))return frame_matrix(r);
 if(!strcmp(t.text,"AnimationSet")){
  if(!start_obj(r,name,sizeof(name)))return 0;
  r->s->animation_sets++;
  if(r->meta){fprintf(r->meta,"ANIM_SET\t");safe_text(r->meta,name);fputc('\n',r->meta);}
  return nested(r,depth+1);
 }
 if(!strcmp(t.text,"Animation")){
  int was_animation=r->in_animation;
  if(!start_obj(r,name,sizeof(name)))return 0;
  if(r->meta){fprintf(r->meta,"ANIMATION\t");safe_text(r->meta,name);fputc('\n',r->meta);}
  r->in_animation=1;
  if(!nested(r,depth+1))return 0;
  r->in_animation=was_animation;
  return 1;
 }
 if(!strcmp(t.text,"AnimationKey"))return animationkey(r);
 if(!strcmp(t.text,"AnimTicksPerSecond")){
  unsigned long ticks;if(!start_obj(r,name,sizeof(name))||!unum(r,&ticks)||!sym(r,'}'))return 0;
  if(r->meta)fprintf(r->meta,"ANIM_TICKS\t%lu\n",ticks);
  return 1;
 }
 if(!strcmp(t.text,"Material"))return material(r,0UL);
 if(!strcmp(t.text,"template")){r->s->unsupported_objects++;return skip_object(r);}
 r->s->unsupported_objects++;
 return skip_object(r);
}
int x_decode_ex(FILE *src,FILE *obj,FILE *metadata,X_Stats *stats,int verbose,int bake){
 XReader r;unsigned char hdr[16];XTok t;int ok=1;
 if(!src||!stats)return 0;
 memset(&r,0,sizeof(r));memset(stats,0,sizeof(*stats));
 r.f=src;r.obj=obj;r.meta=metadata;r.s=stats;r.bake=bake;
 fx_identity(r.frames[0].world);r.frames[0].mesh_begin=0UL;
 if(fseek(src,0L,SEEK_END))return 0;
 r.size=ftell(src);
 if(r.size<16L || fseek(src,0L,SEEK_SET)||fread(hdr,1,16,src)!=16)return 0;
 if(memcmp(hdr,"xof ",4)!=0 || (memcmp(hdr+4,"0303",4)!=0 && memcmp(hdr+4,"0302",4)!=0))return 0;
 if(memcmp(hdr+12,"0032",4)==0)r.width=32;
 else if(memcmp(hdr+12,"0064",4)==0)r.width=64;
 else return 0;
 stats->float_width=r.width;
 if(memcmp(hdr+8,"txt ",4)==0)r.binary=X_TEXT;
 else if(memcmp(hdr+8,"bin ",4)==0)r.binary=X_BIN;
 else if(memcmp(hdr+8,"tzip",4)==0 || memcmp(hdr+8,"bzip",4)==0){
  stats->compressed=1;
  if(verbose)fprintf(stderr,"DirectX .x compressed MSZIP not supported; refusing rather than misreading.\n");
  return 0;
 } else return 0;
 stats->binary=r.binary;
 if(obj)fprintf(obj,"# DirectX X uncompressed %s; %s\n",r.binary?"binary":"text",
   bake?"frame matrices baked into positions; normals omitted; skinning NOT applied":"no baked transforms or skinning");
 if(metadata)fprintf(metadata,"# DirectX X metadata. Values reconstructed as signed 16.16 fixed; transforms and animation NOT evaluated.\n");
 for(;;){
  t=value(&r);
  if(t.kind==T_END)break;
  if(t.kind==T_BAD){ok=0;break;}
  push(&r,t);
  if(!scan(&r,0)){ok=0;break;}
 }
 if(r.bad)ok=0;
 stats->error_offset=(unsigned long)(ftell(src)<0L?0L:ftell(src));
 if(verbose)printf("DX_X %s format=%s width=%d meshes=%lu vertices=%lu faces=%lu triangles=%lu normals=%lu UV=%lu frames=%lu transforms=%lu materials=%lu textures=%lu skin=%lu animationSets=%lu keys=%lu keyframes=%lu unsupported=%lu at=%lu\n",
  ok?"OK":"INVALID",r.binary?"bin":"txt",r.width,stats->meshes,stats->vertices,stats->faces,stats->triangles,stats->normals,stats->texcoords,stats->frames,stats->transforms,stats->materials,stats->textures,stats->skin_clusters,stats->animation_sets,stats->animation_keys,stats->keyframes,stats->unsupported_objects,stats->error_offset);
 return ok && stats->meshes>0UL;
}
int x_decode(FILE *src,FILE *obj,FILE *metadata,X_Stats *stats,int verbose){
 return x_decode_ex(src,obj,metadata,stats,verbose,0);
}
