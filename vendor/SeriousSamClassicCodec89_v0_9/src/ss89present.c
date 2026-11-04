/* Independent CPU preview backend for SS89 scene OBJ + FX89 + PPM/PAM.
   The goal is deterministic material-state execution, not full SE1 rendering parity.
   C89 strict, integer-only; no malloc, no 64-bit, no GPU libraries. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ss89fxpasses.h"
#include "ss89env.h"
#include "ss89settings.h"
#include "ss89resources.h"
#define W 520
#define H 640
#define MAXV 6000
#define MAXUV 7000
#define MAXF 12000
#define PARTS 32
#define SURFACES 128
#define MAXT 512
#define TXPIX (MAXT*MAXT)
typedef struct {int x,y,z,sx,sy,sz;} Vtx;
typedef struct {int u,v;} Uv;
typedef struct {int x,y,z;} Normal;
typedef struct {unsigned v[3],uv[3],normal[3];unsigned char part,surf;} Face;
typedef struct {int visible,twosided,depth_write,alpha_test,blend,shading,diffuse,reflection,specular,bump,detail,translucency;unsigned tint,spec_tint,refl_tint;} Mat;
typedef struct {int w,h,channels; unsigned char data[TXPIX*4];} Tex;
static Vtx vs[MAXV];static Uv uvs[MAXUV];static Face faces[MAXF];static Normal norms[MAXV];static unsigned nnorm;
static Mat mats[PARTS][SURFACES];static Tex textures[4];
static Tex auxtex;
static Tex specmap,reflectionmap;
static int spec_loaded,reflection_loaded,bilinear_filter=0,legacy_lighting=0,materialize=255,alpha_cutoff=128;
static int viewcos=-8192,viewsin=5736;
static int use_env_sphere=1;
static unsigned sorted_faces[MAXF];
static int aux_loaded,fx_bump,fx_detail,force_bump,force_detail;
static int haze_amount,fog_amount,shadow_amount,decal_on,decal_face=-1;
static int minz,maxz,model_min_y,foot_x,foot_y,foot_w;
static unsigned char pixels[W*H*3];static int depthbuf[W*H];
static unsigned nvs,nuv,nf;
static int parse_decimal(const char *s)
{
 int neg=0,whole=0,frac=0,scale=1000,i=0;
 if(*s=='-'){neg=1;s++;}else if(*s=='+')s++;
 while(isdigit((unsigned char)*s)&&i++<6){whole=whole*10+*s-'0';s++;}
 if(*s=='.'){s++;while(isdigit((unsigned char)*s)&&scale){frac+=(*s-'0')*scale;scale/=10;s++;}}
 return neg?-(whole*10000+frac):(whole*10000+frac);
}
static int read_obj(const char *path)
{
 char line[4096];FILE *fp=fopen(path,"rb");int a,b,c,d,e,f,g,h,nn,part,surf,full;
 if(!fp)return 0;
 part=surf=0;nvs=nuv=nf=nnorm=0;
 while(fgets(line,sizeof(line),fp)){
   if(line[0]=='v'&&line[1]==' '){
     char sx[60],sy[60],sz[60];if(nvs>=MAXV)goto fail;
     if(sscanf(line+2,"%59s %59s %59s",sx,sy,sz)!=3)goto fail;
     vs[nvs].x=parse_decimal(sx);vs[nvs].y=parse_decimal(sy);vs[nvs].z=parse_decimal(sz);nvs++;
   }else if(!strncmp(line,"vn ",3)){
     char sx[60],sy[60],sz[60];if(nnorm>=MAXV)goto fail;
     if(sscanf(line+3,"%59s %59s %59s",sx,sy,sz)!=3)goto fail;
     norms[nnorm].x=parse_decimal(sx);norms[nnorm].y=parse_decimal(sy);norms[nnorm].z=parse_decimal(sz);nnorm++;
   }else if(!strncmp(line,"vt ",3)){
     char su[60],sv[60];if(nuv>=MAXUV)goto fail;
     if(sscanf(line+3,"%59s %59s",su,sv)!=2)goto fail;
     uvs[nuv].u=parse_decimal(su);uvs[nuv].v=parse_decimal(sv);nuv++;
   }else if(!strncmp(line,"usemtl ",7)){
     if(sscanf(line+7,"SS_Part_%d_Surface_%d",&part,&surf)!=2)goto fail;
     if(part<0||part>=PARTS||surf<0||surf>=SURFACES)goto fail;
   }else if(line[0]=='f'&&line[1]==' '){
     Face *t;
     if(nf>=MAXF)goto fail;
     full=sscanf(line+2,"%d/%d/%d %d/%d/%d %d/%d/%d",&a,&b,&g,&c,&d,&h,&e,&f,&nn);
     if(full!=9){
      if(sscanf(line+2,"%d/%d %d/%d %d/%d",&a,&b,&c,&d,&e,&f)!=6)goto fail;
     } else if(g<=0||h<=0||nn<=0||(unsigned)g>nnorm||(unsigned)h>nnorm||(unsigned)nn>nnorm)goto fail;
     if(a<=0||c<=0||e<=0||b<=0||d<=0||f<=0)goto fail;
     if((unsigned)a>nvs||(unsigned)c>nvs||(unsigned)e>nvs||(unsigned)b>nuv||(unsigned)d>nuv||(unsigned)f>nuv)goto fail;
     t=&faces[nf++];t->v[0]=(unsigned)(a-1);t->v[1]=(unsigned)(c-1);t->v[2]=(unsigned)(e-1);
     t->uv[0]=(unsigned)(b-1);t->uv[1]=(unsigned)(d-1);t->uv[2]=(unsigned)(f-1);
     t->normal[0]=full==9?(unsigned)(g-1):MAXV;
     t->normal[1]=full==9?(unsigned)(h-1):MAXV;
     t->normal[2]=full==9?(unsigned)(nn-1):MAXV;
     t->part=(unsigned char)part;t->surf=(unsigned char)surf;
   }
 }
 fclose(fp);return nf>0;
 fail:fclose(fp);return 0;
}
static int read_fx(const char *path)
{
 char line[1024],blend[64];FILE *f=fopen(path,"rb");int p,s,sh,a,c,dep,diff,refl,spec,bump,detail,fog,haze,hzextra;Mat *m;
 if(!f)return 0;
 memset(mats,0,sizeof(mats));
 while(fgets(line,sizeof(line),f)){
   if(strncmp(line,"material=",9))continue;
   { const char *vstr=strstr(line," visible=");
     if(!vstr || sscanf(line,"material=%d:%d",&p,&s)!=2 ||
       sscanf(vstr," visible=%d two_sided=%d shading=%d blend=%63s alpha_test=%d depth_write=%d diffuse=%d reflection=%d specular=%d bump=%d detail=%d fog=%d haze=%d",&a,&c,&sh,blend,&dep,&diff,&refl,&spec,&bump,&detail,&fog,&haze,&hzextra)!=13){
       fprintf(stderr,"bad fx material %s\n",line);fclose(f);return 0;
     }
   }
   if(p<0||p>=PARTS||s<0||s>=SURFACES){fprintf(stderr,"bad mat index %s\n",line);fclose(f);return 0;}
   m=&mats[p][s];m->visible=a;m->twosided=c;m->shading=sh;m->alpha_test=dep;m->depth_write=diff;
   m->diffuse=refl;m->reflection=spec;m->specular=bump;m->bump=detail;m->detail=fog;
   m->tint=0xffffffffU; m->spec_tint=0xffffffffU;m->refl_tint=0xffffffffU;
   { char *q=strstr(line,"color_diffuse="); if(q){unsigned color;if(sscanf(q,"color_diffuse=%x",&color)==1)m->tint=color;} }
   { char *q=strstr(line,"color_specular=");if(q){unsigned color;if(sscanf(q,"color_specular=%x",&color)==1)m->spec_tint=color;} }
   { char *q=strstr(line,"color_reflection=");if(q){unsigned color;if(sscanf(q,"color_reflection=%x",&color)==1)m->refl_tint=color;} }
   m->blend=(!strcmp(blend,"off"))?0:(!strcmp(blend,"src_alpha,inv_src_alpha"))?1:(!strcmp(blend,"src_alpha,one"))?2:(!strcmp(blend,"zero,inv_src_color"))?3:-1;
   if(m->blend<0){fprintf(stderr,"unknown blend %s\n",line);fclose(f);return 0;}
 }
 fclose(f);return 1;
}
static int read_aux_texture(const char *path)
{
 FILE *fp;int w,h,maxv;Tex *tx=&auxtex;
 fp=fopen(path,"rb");if(!fp)return 0;
 if(fscanf(fp,"P6\n%d %d\n%d\n",&w,&h,&maxv)!=3 || w<1 || h<1 || w>MAXT || h>MAXT || maxv!=255){fclose(fp);return 0;}
 tx->w=w;tx->h=h;tx->channels=3;
 if(fread(tx->data,3U,(size_t)(w*h),fp)!=(size_t)(w*h)){fclose(fp);return 0;}
 fclose(fp);return 1;
}
static int aux_luma(int u,int v)
{
 int x,y,i,ix,iy;
 if(!aux_loaded)return 128;
 x=u%10000;y=v%10000;if(x<0)x+=10000;if(y<0)y+=10000;
 /* OBJ VT is bottom-origin, PPM is top-origin. */
 y=(10000-y)%10000;
 ix=x*auxtex.w/10000;iy=y*auxtex.h/10000;
 i=(iy*auxtex.w+ix)*3;
 return (auxtex.data[i]*77+auxtex.data[i+1]*150+auxtex.data[i+2]*29)>>8;
}
static int read_texture(int id,const char *folder,const char *prefix)
{
 char path[1024],line[128];FILE *fp;Tex *tx=&textures[id];int w=0,h=0,dep=0,maxv=0,channels=3;
 if(strlen(folder)>800U)return 0;
 sprintf(path,"%s/%s_part_%02d.ppm",folder,prefix,id);
 fp=fopen(path,"rb");
 if(!fp){sprintf(path,"%s/%s_part_%02d.pam",folder,prefix,id);fp=fopen(path,"rb");}
 if(!fp){sprintf(path,"%s/ss_part_%02d.ppm",folder,id);fp=fopen(path,"rb");}
 if(!fp){sprintf(path,"%s/ss_part_%02d.pam",folder,id);fp=fopen(path,"rb");}
 if(!fp)return 0;
 {int magic=fgetc(fp);int version=fgetc(fp);rewind(fp);
 if(magic!='P' || (version!='6'&&version!='7')){fclose(fp);return 0;}
 if(version=='7'){
   if(!fgets(line,sizeof(line),fp)||strncmp(line,"P7",2)){fclose(fp);return 0;}
   while(fgets(line,sizeof(line),fp)){
     if(!strncmp(line,"ENDHDR",6))break;
     if(sscanf(line,"WIDTH %d",&w)==1)continue;
     if(sscanf(line,"HEIGHT %d",&h)==1)continue;
     if(sscanf(line,"DEPTH %d",&dep)==1)continue;
     if(sscanf(line,"MAXVAL %d",&maxv)==1)continue;
   }
   channels=dep;
 }else{
   if(fscanf(fp,"P6\n%d %d\n%d\n",&w,&h,&maxv)!=3){fclose(fp);return 0;}
 }
 if(w<1||w>MAXT||h<1||h>MAXT||maxv!=255||(channels!=3&&channels!=4)){fclose(fp);return 0;}
 tx->w=w;tx->h=h;tx->channels=channels;
 if(fread(tx->data,(size_t)channels,(size_t)(w*h),fp)!=(size_t)(w*h)){fclose(fp);return 0;}
 fclose(fp);return 1;}
}
/* Separate optional reflection/specular maps. Never synthesize a map that
   does not exist in an actual model resource. */
static int read_optional_map(Tex *out,const char *path)
{
 FILE *fp;char line[128];int w=0,h=0,dep=0,maxv=0,version,channels=3;
 if(!path)return 0;
 fp=fopen(path,"rb");if(!fp)return 0;
 if(fgetc(fp)!='P'){fclose(fp);return 0;}
 version=fgetc(fp);rewind(fp);
 if(version=='7'){
  if(!fgets(line,sizeof(line),fp)||strncmp(line,"P7",2)){fclose(fp);return 0;}
  while(fgets(line,sizeof(line),fp)){
   if(!strncmp(line,"ENDHDR",6))break;
   if(sscanf(line,"WIDTH %d",&w)==1)continue;
   if(sscanf(line,"HEIGHT %d",&h)==1)continue;
   if(sscanf(line,"DEPTH %d",&dep)==1)continue;
   if(sscanf(line,"MAXVAL %d",&maxv)==1)continue;
  }
  channels=dep;
 }else if(version=='6'){
  if(fscanf(fp,"P6\n%d %d\n%d\n",&w,&h,&maxv)!=3){fclose(fp);return 0;}
 }else {fclose(fp);return 0;}
 if(w<1||h<1||w>MAXT||h>MAXT||maxv!=255||(channels!=3&&channels!=4)){fclose(fp);return 0;}
 out->w=w;out->h=h;out->channels=channels;
 if(fread(out->data,(size_t)channels,(size_t)(w*h),fp)!=(size_t)(w*h)){fclose(fp);return 0;}
 fclose(fp);return 1;
}
/* Separated source colors and textures, filtered in the same texel space.
   Coordinates are [0,10000) normalized and V is already in top-origin space. */
static void sample_tex(const Tex *tx,int u,int v,int rgba[4])
{
 int x,y,k,rx,ry,x1,y1,idx;
 if(tx->w<1||tx->h<1){rgba[0]=rgba[1]=rgba[2]=255;rgba[3]=255;return;}
 u%=10000;v%=10000;if(u<0)u+=10000;if(v<0)v+=10000;
 x=u*tx->w/10000;y=v*tx->h/10000;
 if(!bilinear_filter){
  idx=(y*tx->w+x)*tx->channels;
  for(k=0;k<3;k++)rgba[k]=tx->data[idx+k];
  rgba[3]=tx->channels==4?tx->data[idx+3]:255;
  return;
 }
 rx=(u*tx->w)%10000;ry=(v*tx->h)%10000;
 x1=(x+1)%tx->w;y1=(y+1)%tx->h;
 for(k=0;k<4;k++){
  int a0,a1,b0,b1,top,bot;
  a0=tx->data[(y*tx->w+x)*tx->channels+(k==3&&tx->channels==3?0:k)];
  a1=tx->data[(y*tx->w+x1)*tx->channels+(k==3&&tx->channels==3?0:k)];
  b0=tx->data[(y1*tx->w+x)*tx->channels+(k==3&&tx->channels==3?0:k)];
  b1=tx->data[(y1*tx->w+x1)*tx->channels+(k==3&&tx->channels==3?0:k)];
  top=(a0*(10000-rx)+a1*rx)/10000;
  bot=(b0*(10000-rx)+b1*rx)/10000;
  rgba[k]=(top*(10000-ry)+bot*ry)/10000;
 }
 if(tx->channels==3)rgba[3]=255;

}
static int face_depth_compare(const void *aa,const void *bb)
{
 unsigned a=*(const unsigned*)aa,b=*(const unsigned*)bb;
 int za=vs[faces[a].v[0]].sz/3+vs[faces[a].v[1]].sz/3+vs[faces[a].v[2]].sz/3;
 int zb=vs[faces[b].v[0]].sz/3+vs[faces[b].v[1]].sz/3+vs[faces[b].v[2]].sz/3;
 if(za<zb)return -1;
 if(za>zb)return 1;
 return a<b?-1:a>b?1:0;
}
static void camera_setup(int yaw)
{
 int i,c,s,minx=2147483647,maxx=-2147483647,miny=2147483647,maxy=-2147483647,cx,cy,h,wide,scale;
 if(yaw==0){c=10000;s=0;}else if(yaw==90){c=0;s=10000;}else if(yaw==180){c=-10000;s=0;}else if(yaw==45){c=7071;s=7071;}else{c=-8192;s=5736;}
 viewcos=c;viewsin=s;
 minz=2147483647;maxz=-2147483647;model_min_y=2147483647;
 for(i=0;i<(int)nvs;i++){
   vs[i].sx=(vs[i].x*c-vs[i].z*s)/10000;
   vs[i].sz=(vs[i].x*s+vs[i].z*c)/10000;
   vs[i].sy=vs[i].y;
   if(vs[i].sz<minz)minz=vs[i].sz;
   if(vs[i].sz>maxz)maxz=vs[i].sz;
   if(vs[i].y<model_min_y)model_min_y=vs[i].y;
   if(vs[i].sx<minx)minx=vs[i].sx;
   if(vs[i].sx>maxx)maxx=vs[i].sx;
   if(vs[i].sy<miny)miny=vs[i].sy;
   if(vs[i].sy>maxy)maxy=vs[i].sy;
 }
 cx=(minx+maxx)/2;cy=(miny+maxy)/2;h=maxy-miny;wide=maxx-minx;
 if(h<1000)h=1000;
 if(wide<1000)wide=1000;
 scale=(H-64)*10000/h;
 if((W-64)*10000/wide < scale)scale=(W-64)*10000/wide;
 if(scale>150000)scale=150000;
 foot_x=W/2;foot_y=H/2+(cy-miny)*scale/10000;foot_w=(maxx-minx)*scale/20000;
 for(i=0;i<(int)nvs;i++){
   vs[i].sx=W/2+(vs[i].sx-cx)*scale/10000;
   vs[i].sy=H/2-(vs[i].sy-cy)*scale/10000;
 }
}
static int absi(int a){return a<0?-a:a;}
static int mini(int a,int b){return a<b?a:b;}
static int maxi(int a,int b){return a>b?a:b;}
static void drawface(const Face *f,int pass)
{
 const Vtx *a=&vs[f->v[0]],*b=&vs[f->v[1]],*c=&vs[f->v[2]];
 const Uv *u=&uvs[f->uv[0]],*v=&uvs[f->uv[1]],*w=&uvs[f->uv[2]];
 const Mat *m=&mats[f->part][f->surf];const Tex *tx=&textures[f->part];
 int den,minx,maxx,miny,maxy,x,y,mode,brightness;
 int nx,ny,nz,sum,lightdot,nlen,lightx=72,lighty=170,lightz=110,tdot, vshade[3],has_vn=0;
 int envru[3],envrv[3],envsu[3],envsv[3],iv;
 if(!m->visible || (pass==0 && m->blend!=0) || (pass==1 && m->blend==0))return;
 den=(b->sy-c->sy)*(a->sx-c->sx)+(c->sx-b->sx)*(a->sy-c->sy);
 if(!den)return;
 /* Raw winding depends on model handedness. The Sam preview is intentionally
    two-sided until a per-model front-face convention is calibrated. */
 minx=maxi(0,mini(a->sx,mini(b->sx,c->sx)));maxx=mini(W-1,maxi(a->sx,maxi(b->sx,c->sx)));
 miny=maxi(0,mini(a->sy,mini(b->sy,c->sy)));maxy=mini(H-1,maxi(a->sy,maxi(b->sy,c->sy)));
 if(minx>maxx||miny>maxy)return;
 nx=((b->y-a->y)/100)*((c->z-a->z)/100)-((b->z-a->z)/100)*((c->y-a->y)/100);
 ny=((b->z-a->z)/100)*((c->x-a->x)/100)-((b->x-a->x)/100)*((c->z-a->z)/100);
 nz=((b->x-a->x)/100)*((c->y-a->y)/100)-((b->y-a->y)/100)*((c->x-a->x)/100);
 sum=absi(nx)+absi(ny)+absi(nz);if(sum<=0)sum=1;
/* Light is a controllable diagnostic light, not level-sampled SE lighting.
    Original shading: fullbright=0, matte=1, flat=2. */
 lightdot=(nx*lightx/100+ny*lighty/100+nz*lightz/100);
 nlen=sum/100;if(nlen<1)nlen=1;
 tdot=lightdot/nlen;
 if(tdot>180)tdot=180;
 if(tdot<0)tdot=0;
 brightness=m->shading==0?255:(legacy_lighting?(160+(95*absi(ny))/sum):75+tdot);
 if(brightness>255)brightness=255;
 if(brightness<0)brightness=0;
 has_vn=(f->normal[0]<nnorm&&f->normal[1]<nnorm&&f->normal[2]<nnorm);
 if(has_vn){
   for(iv=0;iv<3;iv++){
     const Normal *vn=&norms[f->normal[iv]];
     int dp=vn->x*lightx/100+vn->y*lighty/100+vn->z*lightz/100;
     dp=dp/100;
     if(dp<0)dp=0;
     if(dp>180)dp=180;
     vshade[iv]=75+dp;
   }
 }
 /* The source computes sphere coordinates at mip vertices, then interpolates
    over surface triangles. Use per-vertex normals, not face-normal ENV lookup.
    Reflection is viewer-reflected X/Z with -Y pole; specular is reflected
    light X/Y with -Z pole, matching the non-ASM Serious Engine path. */
 if(use_env_sphere && ((reflection_loaded&&m->reflection)||(spec_loaded&&m->specular))){
   for(iv=0;iv<3;iv++){
     int an_x,an_y,an_z,ax,az,am;
     if(has_vn){
       const Normal *vn=&norms[f->normal[iv]];
       an_x=vn->x;an_y=vn->y;an_z=vn->z;
     }else{
       an_x=nx;an_y=ny;an_z=nz;
       am=absi(nx)+absi(ny)+absi(nz);
       if(am<1)am=1;
       an_x=nx*10000/am;an_y=ny*10000/am;an_z=nz*10000/am;
     }
     ax=(an_x*viewcos/10000)-(an_z*viewsin/10000);
     az=(an_x*viewsin/10000)+(an_z*viewcos/10000);
     /* Normals are already approximately unit-length Q10000. */
     ss89_env_sphere_reflection(ax,an_y,az,0,0,-10000,&envru[iv],&envrv[iv]);
     ss89_env_sphere_specular(ax,an_y,az,3350,7890,5100,&envsu[iv],&envsv[iv]);
   }
 }
 mode=m->blend;
 for(y=miny;y<=maxy;y++)for(x=minx;x<=maxx;x++){
   unsigned char *dst;int *zbuf,ww0,ww1,ww2,k,ui,vi,zz,alpha,rgb[3],src,cur,kk;
   int texel[4];
   int edge0=(b->sy-c->sy)*(x-c->sx)+(c->sx-b->sx)*(y-c->sy);
   int edge1=(c->sy-a->sy)*(x-c->sx)+(a->sx-c->sx)*(y-c->sy);
   if(den>0){if(edge0<0||edge1<0||edge0+edge1>den)continue;}
   else {if(edge0>0||edge1>0||edge0+edge1<den)continue;}
   ww0=edge0*4096/den;ww1=edge1*4096/den;ww2=4096-ww0-ww1;
   zz=(ww0*a->sz+ww1*b->sz+ww2*c->sz)/4096;
   k=y*W+x;zbuf=&depthbuf[k];if(zz<*zbuf)continue;
   ui=(ww0*u->u+ww1*v->u+ww2*w->u)/4096;
   vi=(ww0*u->v+ww1*v->v+ww2*w->v)/4096;
   ui%=10000;vi%=10000;if(ui<0)ui+=10000;if(vi<0)vi+=10000;
   /* OBJ V is bottom-origin; decoded PAM/PPM rows are top-origin. */
   vi=(10000-vi)%10000;
   sample_tex(tx,ui,vi,texel);
   alpha=texel[3];
   if(m->alpha_test && alpha<alpha_cutoff)continue;
   alpha=alpha*materialize/255;
   if(m->shading==1 && has_vn && !legacy_lighting){
     brightness=(ww0*vshade[0]+ww1*vshade[1]+ww2*vshade[2])/4096;
   }
   dst=&pixels[k*3];
   for(kk=0;kk<3;kk++){
     src=m->diffuse?texel[kk]:255;
     src=src*brightness/255;
     src=src*(int)((m->tint>>(24-8*kk))&255U)/255;
     if(aux_loaded && fx_bump && (m->bump || force_bump)){
       int up=aux_luma((ww0*u->u+ww1*v->u+ww2*w->u)/4096+70, (ww0*u->v+ww1*v->v+ww2*w->v)/4096+70);
       int dn=aux_luma((ww0*u->u+ww1*v->u+ww2*w->u)/4096-70, (ww0*u->v+ww1*v->v+ww2*w->v)/4096-70);
       src=ss89_fx_bump_channel(src,up,dn,110);
     }else if(aux_loaded && fx_detail && (m->detail || force_detail)){
       int det=aux_luma((ww0*u->u+ww1*v->u+ww2*w->u)/4096,(ww0*u->v+ww1*v->v+ww2*w->v)/4096);
       src=ss89_fx_detail_channel(src,det,125);
     }
     cur=dst[kk];
     if(mode==0){rgb[kk]=src;}
     else if(mode==1)rgb[kk]=(src*alpha+cur*(255-alpha))/255;
     else if(mode==2)rgb[kk]=mini(255,cur+src*alpha/255);
     else rgb[kk]=cur*(255-src)/255;
   }
   if(materialize<255 && mode==0){
     rgb[0]=(rgb[0]*materialize+dst[0]*(255-materialize))/255;
     rgb[1]=(rgb[1]*materialize+dst[1]*(255-materialize))/255;
     rgb[2]=(rgb[2]*materialize+dst[2]*(255-materialize))/255;
   }
   dst[0]=(unsigned char)rgb[0];dst[1]=(unsigned char)rgb[1];dst[2]=(unsigned char)rgb[2];
   if((spec_loaded&&m->specular)||(reflection_loaded&&m->reflection)){
    int envu,envv,layer[4],weight,kkk,cn;
    /* Legacy flag retained for before/after regression. */
    if(!use_env_sphere){
       envu=5000+nx*4000/sum;envv=5000-nz*4000/sum;
    }
    if(reflection_loaded&&m->reflection){
      if(use_env_sphere){
        envu=(ww0*envru[0]+ww1*envru[1]+ww2*envru[2])/4096;
        envv=(ww0*envrv[0]+ww1*envrv[1]+ww2*envrv[2])/4096;
      }
      sample_tex(&reflectionmap,envu,envv,layer);
      weight=layer[3]*(int)(m->refl_tint&255U)/255;
      if(m->shading!=0)weight=weight*brightness/255;
      for(kkk=0;kkk<3;kkk++){
        cn=(int)((m->refl_tint>>(24-8*kkk))&255U);
        src=layer[kkk]*cn/255;
        dst[kkk]=(unsigned char)((dst[kkk]*(255-weight)+src*weight)/255);
      }
    }
    if(spec_loaded&&m->specular){
      if(use_env_sphere){
        envu=(ww0*envsu[0]+ww1*envsu[1]+ww2*envsu[2])/4096;
        envv=(ww0*envsv[0]+ww1*envsv[1]+ww2*envsv[2])/4096;
      }
      sample_tex(&specmap,envu,envv,layer);
      if(use_env_sphere){
        /* Engine uses INV_SRC_ALPHA,ONE, with color attenuated by source
           light intensity. Opaque specular texels leave diffuse untouched. */
        weight=(specmap.channels==4)?(255-layer[3]):255;
        /* RGB-only external preview maps have no alpha mask. */
        if(m->shading!=0)weight=weight*brightness/255;
      }else{
        weight=tdot*tdot/180;
        if(weight>180)weight=180;
      }
      for(kkk=0;kkk<3;kkk++){
        cn=(int)((m->spec_tint>>(24-8*kkk))&255U);
        src=layer[kkk]*cn/255;
        dst[kkk]=(unsigned char)mini(255,(int)dst[kkk]+src*weight/255);
      }
    }
   }
   if(m->depth_write && materialize==255)*zbuf=zz;
 }
}
/* Preview shadow: flat ground plane under the model, not SE1 geometry shadowing.
   Ground and lights must be supplied by the eventual level/world presenter. */
static void paint_shadow(void)
{
 int x,y,x0=foot_x,y0=foot_y+2,rx=foot_w+12,ry=14;
 if(!shadow_amount)return;
 if(rx<18)rx=18;
 if(rx>140)rx=140;
 for(y=y0-ry;y<y0+ry;y++)for(x=x0-rx;x<x0+rx;x++){
  int dx,dy,d,k,a;
  if(y<0||y>=H||x<0||x>=W)continue;
  dx=(x-x0)*64/rx;dy=(y-y0)*64/ry;d=dx*dx+dy*dy;
  if(d>=4096)continue;
  a=shadow_amount*(4096-d)/4096;
  k=(y*W+x)*3;
  pixels[k]=ss89_fx_shadow_channel(pixels[k],a);
  pixels[k+1]=ss89_fx_shadow_channel(pixels[k+1],a);
  pixels[k+2]=ss89_fx_shadow_channel(pixels[k+2],a);
 }
}
static void paint_decal(void)
{
 unsigned i;int f=-1,mind=2147483647,cx=W/2,cy=H/2,xx,yy,r=15;
 if(!decal_on)return;
 if(decal_face>=0 && (unsigned)decal_face<nf)f=decal_face;
 else {
   /* Choose a triangle closest to the upper middle of the model's image. */
   for(i=0;i<nf;i++){
    const Face *t=&faces[i];int x=(vs[t->v[0]].sx+vs[t->v[1]].sx+vs[t->v[2]].sx)/3;
    int y=(vs[t->v[0]].sy+vs[t->v[1]].sy+vs[t->v[2]].sy)/3;
    int d=absi(x-W/2)+absi(y-(H*9/20));
    if(d<mind){mind=d;f=(int)i;}
   }
 }
 if(f<0)return;
 { const Face *t=&faces[f];cx=(vs[t->v[0]].sx+vs[t->v[1]].sx+vs[t->v[2]].sx)/3;
   cy=(vs[t->v[0]].sy+vs[t->v[1]].sy+vs[t->v[2]].sy)/3; }
 for(yy=cy-r;yy<=cy+r;yy++)for(xx=cx-r;xx<=cx+r;xx++){
  int d,k,a,dx=xx-cx,dy=yy-cy;
  if(xx<0||xx>=W||yy<0||yy>=H)continue;
  d=dx*dx+dy*dy;if(d>r*r)continue;
  k=yy*W+xx;
  /* The decal is clipped to already drawn model pixels. It tracks the
     selected animated triangle, not the screen origin. */
  if(depthbuf[k]==-2147483647)continue;
  a=(r*r-d)*185/(r*r);if(d<15)a=210;
  pixels[k*3]=ss89_fx_decal_channel(pixels[k*3],90,a);
  pixels[k*3+1]=ss89_fx_decal_channel(pixels[k*3+1],12,a);
  pixels[k*3+2]=ss89_fx_decal_channel(pixels[k*3+2],15,a);
 }
}
static void paint_atmosphere(void)
{
 int i,j,span=maxz-minz;
 if(!haze_amount&&!fog_amount)return;
 if(span<1)span=1;
 for(i=0;i<W*H;i++){
  int z=depthbuf[i],d=0,amt;
  if(z==-2147483647)continue;
  /* Orthographic viewport demo: explicit relative view-depth attenuation.
     Not the source engine's level-configured haze/fog lookup textures. */
  if(haze_amount){
    d=(maxz-z)/((span/255)+1);if(d>255)d=255;
    amt=d*haze_amount/255;
    for(j=0;j<3;j++)pixels[i*3+j]=ss89_fx_haze_channel(pixels[i*3+j],j==0?143:j==1?158:170,amt);
  }
  if(fog_amount){
    int row=i/W;d=(H-row)*255/H;amt=d*fog_amount/255;
    for(j=0;j<3;j++)pixels[i*3+j]=ss89_fx_haze_channel(pixels[i*3+j],j==0?107:j==1?139:163,amt);
  }
 }
}
int main(int argc,char **argv)
{
 int i,yaw=145,arg=3;char path[1024],folder[1024],stem[64],*p;FILE *out;
 const char *bump_path=NULL,*spec_path=NULL,*reflection_path=NULL;
 const char *settings_path=NULL,*asset_root=NULL;
 char autospec[1024],autorefl[1024],assetfile[1024];
 int remove_spec=0,remove_refl=0;
 SS89_Settings model_settings;
 if(argc<3){fprintf(stderr,"usage: ss89present scene.obj output.ppm [yaw=0|45|90|145|180]\n");return 2;}
 if(argc>3 && argv[3][0]!='-'){yaw=atoi(argv[3]);arg=4;}
 for(i=arg;i<argc;i++){
   if(!strcmp(argv[i],"--settings") && i+1<argc)settings_path=argv[++i];
   else if(!strcmp(argv[i],"--assets-root") && i+1<argc)asset_root=argv[++i];
   else if(!strcmp(argv[i],"--env-mode") && i+1<argc){const char *m=argv[++i];if(!strcmp(m,"sphere"))use_env_sphere=1;else if(!strcmp(m,"legacy"))use_env_sphere=0;else return 2;}
   else if(!strcmp(argv[i],"--aux-map") && i+1<argc)bump_path=argv[++i];
   else if(!strcmp(argv[i],"--spec-map")&&i+1<argc)spec_path=argv[++i];
   else if(!strcmp(argv[i],"--reflection-map")&&i+1<argc)reflection_path=argv[++i];
   else if(!strcmp(argv[i],"--filter")&&i+1<argc){const char *fm=argv[++i];if(!strcmp(fm,"bilinear"))bilinear_filter=1;else if(!strcmp(fm,"nearest"))bilinear_filter=0;else return 2;}
   else if(!strcmp(argv[i],"--lighting")&&i+1<argc){const char *lm=argv[++i];if(!strcmp(lm,"legacy"))legacy_lighting=1;else if(!strcmp(lm,"directional"))legacy_lighting=0;else return 2;}
   else if(!strcmp(argv[i],"--materialize")&&i+1<argc)materialize=atoi(argv[++i]);
   else if(!strcmp(argv[i],"--alpha-cutoff")&&i+1<argc)alpha_cutoff=atoi(argv[++i]);
   else if(!strcmp(argv[i],"--bump"))fx_bump=1;
   else if(!strcmp(argv[i],"--detail"))fx_detail=1;
   else if(!strcmp(argv[i],"--demo-bump")){fx_bump=1;force_bump=1;}
   else if(!strcmp(argv[i],"--demo-detail")){fx_detail=1;force_detail=1;}
   else if(!strcmp(argv[i],"--haze") && i+1<argc)haze_amount=atoi(argv[++i]);
   else if(!strcmp(argv[i],"--fog") && i+1<argc)fog_amount=atoi(argv[++i]);
   else if(!strcmp(argv[i],"--shadow") && i+1<argc)shadow_amount=atoi(argv[++i]);
   else if(!strcmp(argv[i],"--decal"))decal_on=1;
   else if(!strcmp(argv[i],"--decal-face") && i+1<argc){decal_on=1;decal_face=atoi(argv[++i]);}
   else {fprintf(stderr,"unknown present option: %s\n",argv[i]);return 2;}
 }
 if(materialize<0||materialize>255||alpha_cutoff<0||alpha_cutoff>255)return 2;
 if(haze_amount<0||haze_amount>255||fog_amount<0||fog_amount>255||shadow_amount<0||shadow_amount>255)return 2;
 if((fx_bump||fx_detail)&&!bump_path){fprintf(stderr,"bump/detail requires --aux-map FILE.ppm\n");return 2;}
 if(settings_path){
   if(!asset_root || !ss89_settings_read(settings_path,&model_settings)){
      fprintf(stderr,"settings: need valid WTEX .ini and --assets-root\n");return 2;
   }
   if(!spec_path && model_settings.specular[0]){
     if(strlen(asset_root)+strlen(model_settings.specular)+2U>=sizeof(assetfile)||strlen(argv[2])+32U>=sizeof(autospec))return 2;
     strcpy(assetfile,asset_root);strcat(assetfile,"/");strcat(assetfile,model_settings.specular);
     strcpy(autospec,argv[2]);strcat(autospec,".ss89_spec.pam");
     if(ss89_resource(assetfile,autospec,0U,0U)){
        fprintf(stderr,"missing or unsupported specular TEX: %s\n",assetfile);return 1;
     }
     spec_path=autospec;remove_spec=1;
   }
   if(!reflection_path && model_settings.reflection[0]){
     if(strlen(asset_root)+strlen(model_settings.reflection)+2U>=sizeof(assetfile)||strlen(argv[2])+32U>=sizeof(autorefl))return 2;
     strcpy(assetfile,asset_root);strcat(assetfile,"/");strcat(assetfile,model_settings.reflection);
     strcpy(autorefl,argv[2]);strcat(autorefl,".ss89_reflect.pam");
     if(ss89_resource(assetfile,autorefl,0U,0U)){
        fprintf(stderr,"missing or unsupported reflection TEX: %s\n",assetfile);return 1;
     }
     reflection_path=autorefl;remove_refl=1;
   }
 }
 if(spec_path){if(!read_optional_map(&specmap,spec_path)){fprintf(stderr,"invalid specular texture\n");return 1;}spec_loaded=1;}
 if(reflection_path){if(!read_optional_map(&reflectionmap,reflection_path)){fprintf(stderr,"invalid reflection texture\n");return 1;}reflection_loaded=1;}
 if(bump_path){if(!read_aux_texture(bump_path)){fprintf(stderr,"invalid auxiliary texture\n");return 1;}aux_loaded=1;}
 if(strlen(argv[1])>=sizeof(path)||strlen(argv[2])>=sizeof(path))return 2;
 strcpy(path,argv[1]);p=strrchr(path,'.');if(!p)return 2;strcpy(p,".fx89");
 if(!read_fx(path)){fprintf(stderr,"bad FX state\n");return 1;}
 if(!read_obj(argv[1])){fprintf(stderr,"bad OBJ state\n");return 1;}
 strcpy(folder,argv[1]);p=strrchr(folder,'/');if(!p)p=strrchr(folder,'\\');if(p)*p=0;else strcpy(folder,".");
 strncpy(stem,argv[1],sizeof(stem)-1U);stem[sizeof(stem)-1U]=0;
 p=strrchr(argv[1],'/');if(!p)p=strrchr(argv[1],'\\');if(p){strncpy(stem,p+1,sizeof(stem)-1U);stem[sizeof(stem)-1U]=0;}
 p=strrchr(stem,'.');if(p)*p=0;
 for(i=0;i<4;i++)if(!read_texture(i,folder,stem)){
   if(i==0){fprintf(stderr,"missing primary texture\n");return 1;}
   /* A single-model scene legitimately only provides part 00. */
   textures[i]=textures[0];
 }
 camera_setup(yaw);
 for(i=0;i<W*H;i++){
   pixels[i*3+0]=21;pixels[i*3+1]=27;pixels[i*3+2]=39;depthbuf[i]=-2147483647;
 }
 paint_shadow();
 for(i=0;i<(int)nf;i++)sorted_faces[i]=(unsigned)i;
 qsort(sorted_faces,nf,sizeof(sorted_faces[0]),face_depth_compare);
 for(i=0;i<(int)nf;i++)drawface(&faces[i],0);
 for(i=0;i<(int)nf;i++)drawface(&faces[sorted_faces[i]],1);
 paint_decal();paint_atmosphere();
 out=fopen(argv[2],"wb");if(!out)return 1;
 fprintf(out,"P6\n%d %d\n255\n",W,H);
 if(fwrite(pixels,3U,(size_t)(W*H),out)!=(size_t)(W*H)){fclose(out);return 1;}
 fclose(out);if(remove_spec)remove(autospec);if(remove_refl)remove(autorefl);printf("[present] %u vertices, %u triangles, %u UV -> %s (bump=%d detail=%d haze=%d fog=%d shadow=%d decal=%d)\n",nvs,nf,nuv,argv[2],fx_bump,fx_detail,haze_amount,fog_amount,shadow_amount,decal_on);return 0;
}
