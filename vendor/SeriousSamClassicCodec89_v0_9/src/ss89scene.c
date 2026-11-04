/* Serious Engine 1 AMC scene assembler — independent ISO C89 implementation.
   Model and texture files are loaded from disk. No heap, FP, or 64-bit.
   Relative attachment Euler angles are intentionally rejected if nonzero.
   Renderer-side shader effects are not recreated by the OBJ material export. */
#include "ss89.h"
#include "ss89resources.h"
#include "ss89fx.h"
#include "ss89normals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define S89_NODES 32
#define S89_DEPTH 20
#define S89_PATH 512
#define S89_AMC_LINE 1024
#define S89_Q 32768
#define S89_SRF_DIFFUSE 32U
#define S89_SRF_INVISIBLE 8U

typedef struct { SS89_i32 m[3][3], p[3]; } S89_Pose;
typedef struct {
    char model[S89_PATH],texture[S89_PATH];
    int parent, parent_attach, animation;
    S89_Pose world;
    S89_Pose anchor[SS89_MAX_ATTACHMENTS];
    unsigned char anchor_valid[SS89_MAX_ATTACHMENTS];
    unsigned anchors;
} S89_Node;
static S89_Node nodes[S89_NODES];
static int node_count;
static SS89_Model model;
static SS89_Vertex points[SS89_MAX_VERTICES];
static SS89_Vertex nextpoints[SS89_MAX_VERTICES];
static SS89_Vertex worldpoints[SS89_MAX_VERTICES];
static SS89_i32 interpolated_normals[SS89_MAX_VERTICES][3];
static unsigned isqrt32(unsigned n);
static int scene_gouraud_ready=0;

static void set_identity(S89_Pose *p)
{
    int i,j; memset(p,0,sizeof(*p));
    for(i=0;i<3;i++)for(j=0;j<3;j++)p->m[i][j]=(i==j)?S89_Q:0;
}
static SS89_i32 add32(SS89_i32 a,SS89_i32 b)
{
    if(b>0 && a>2147483647-b)return 2147483647;
    if(b<0 && a<(-2147483647-1)-b)return -2147483647-1;
    return a+b;
}
static SS89_i32 diff32(SS89_i32 a,SS89_i32 b){return add32(a,-b);}
/* Animation timing is not taken from the AMC's preview choices when a
   named override is requested. Each part has its own animation clock.
   Phase synchronization is opt-in and only affects the selected body node. */
typedef struct { unsigned frame0,frame1,key; unsigned fraction; } S89_Sample;
static int name_equal(const char *a,const char *b)
{
    while(*a&&*b){
        if(toupper((unsigned char)*a)!=toupper((unsigned char)*b))return 0;
        a++;b++;
    }
    return *a==0&&*b==0;
}
static int find_clip(const SS89_Model *m,const char *name)
{
    unsigned i;
    if(!name || !*name)return -1;
    for(i=0U;i<m->anim_count;i++)if(name_equal(m->anims[i].name,name))return (int)i;
    return -1;
}
static int sample_keys(const SS89_Model *m,int ai,unsigned key,unsigned fraction,S89_Sample *out)
{
    const SS89_Animation *an;
    unsigned k1;
    if(ai<0||(unsigned)ai>=m->anim_count)return 0;
    an=&m->anims[ai];
    if(!an->keys || key>=an->keys || an->first_key+an->keys>m->anim_keys_count)return 0;
    k1=(key+1U)%an->keys;
    out->frame0=m->anim_keys[an->first_key+key];
    out->frame1=m->anim_keys[an->first_key+k1];
    out->key=key;out->fraction=fraction;
    return out->frame0<m->frames && out->frame1<m->frames;
}
static int sample_time(const SS89_Model *m,int ai,unsigned time_ms,int loop,S89_Sample *out)
{
    const SS89_Animation *an;
    unsigned spf,period,within,key,frac;
    SS89_i32 q;
    if(ai<0||(unsigned)ai>=m->anim_count)return 0;
    an=&m->anims[ai];q=an->seconds_per_frame_q16;
    if(q<=0||!an->keys)return 0;
    spf=(unsigned)(q/65536)*1000U+((unsigned)(q%65536)*1000U+32768U)/65536U;
    if(!spf || spf>60000U || an->keys>65535U/spf)return 0;
    period=an->keys*spf;
    if(!loop && time_ms>=period)return sample_keys(m,ai,an->keys-1U,0U,out);
    within=time_ms%period;
    key=within/spf;
    frac=(within%spf)*65536U/spf;
    if(frac>=65536U)frac=65535U;
    return sample_keys(m,ai,key,frac,out);
}
static int sample_phase(const SS89_Model *m,int ai,unsigned root_key,unsigned root_fraction,unsigned root_keys,S89_Sample *out)
{
    unsigned phase,child_ticks,k,frac,child_keys;
    if(ai<0||(unsigned)ai>=m->anim_count || !root_keys || root_keys>32768U || root_key>=root_keys)return 0;
    child_keys=m->anims[ai].keys;
    if(!child_keys || child_keys>32768U)return 0;
    phase=(root_key*65536U+root_fraction)/root_keys;
    child_ticks=phase*child_keys;
    k=child_ticks>>16;frac=child_ticks&65535U;
    if(k>=child_keys){k=child_keys-1U;frac=65535U;}
    return sample_keys(m,ai,k,frac,out);
}
static SS89_i32 lerp_q16(SS89_i32 a,SS89_i32 b,unsigned frac)
{
    SS89_i32 d,hi,lo,r;
    SS89_i32 fh,fl;
    if(frac==0U)return a;
    if(frac>=65536U)return b;
    /* divide into bytes, avoiding signed 64-bit multiplication and float */
    d=diff32(b,a);hi=d/256;lo=d%256;
    fh=(SS89_i32)(frac/256U);fl=(SS89_i32)(frac%256U);
    r=hi*fh+(hi*fl)/256+(lo*fh)/256+(lo*fl)/65536;
    return add32(a,r);
}
static int decode_interpolated(SS89_Model *m,const S89_Sample *sample)
{
    unsigned i;
    int interpolate=(sample->fraction>0U && sample->frame0!=sample->frame1);
    int rc=ss89_decode_frame(m,sample->frame0,points,SS89_MAX_VERTICES);
    if(rc!=SS89_OK)return 0;
    if(interpolate){
        rc=ss89_decode_frame(m,sample->frame1,nextpoints,SS89_MAX_VERTICES);
        if(rc!=SS89_OK)return 0;
    }
    for(i=0U;i<m->vertices;i++){
        {
            int a[3],b[3],k,di[3];unsigned sq=0U,len;
            int can_decode=((m->flags&SS89_MF_COMPRESSED_16BIT)!=0U && !m->frame_legacy_normals) || scene_gouraud_ready;
            if(can_decode){
                if((m->flags&SS89_MF_COMPRESSED_16BIT)!=0U && !m->frame_legacy_normals){
                    ss89_normal_hp(points[i].norm0,points[i].norm1,a);
                    if(interpolate)ss89_normal_hp(nextpoints[i].norm0,nextpoints[i].norm1,b);
                }else{
                    if(!ss89_normal_index(points[i].norm0,a))return 0;
                    if(interpolate && !ss89_normal_index(nextpoints[i].norm0,b))return 0;
                }
                /* Q14 intermediary keeps normalization inside signed 32 bits. */
                for(k=0;k<3;k++){
                    di[k]=(interpolate?lerp_q16(a[k],b[k],sample->fraction):a[k])/2;
                    sq+=(unsigned)(di[k]*di[k]);
                }
                len=isqrt32(sq);
                if(len<1U)len=1U;
                for(k=0;k<3;k++)interpolated_normals[i][k]=(di[k]*32767)/(SS89_i32)len;
            }else{
                interpolated_normals[i][0]=0;
                interpolated_normals[i][1]=0;
                interpolated_normals[i][2]=0;
            }
        }
        if(interpolate){
            points[i].x=lerp_q16(points[i].x,nextpoints[i].x,sample->fraction);
            points[i].y=lerp_q16(points[i].y,nextpoints[i].y,sample->fraction);
            points[i].z=lerp_q16(points[i].z,nextpoints[i].z,sample->fraction);
        }
    }
    return 1;
}
static unsigned isqrt32(unsigned n)
{
    unsigned r=0U,b=1U<<30;
    while(b>n)b>>=2;
    while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}
    return r;
}
static int vec_norm(const SS89_i32 v[3], SS89_i32 out[3])
{
    SS89_u32 m=0U, a[3], scale, len2=0U, len;
    SS89_i32 tmp[3];int i;
    for(i=0;i<3;i++){
        a[i]=v[i]<0?(SS89_u32)(-(v[i]+1))+1U:(SS89_u32)v[i];
        if(a[i]>m)m=a[i];
    }
    if(!m)return 0;
    scale=(m>>12)+1U;
    for(i=0;i<3;i++){
        tmp[i]=v[i]/(SS89_i32)scale;
        len2+=(SS89_u32)(tmp[i]*tmp[i]);
    }
    len=isqrt32(len2);if(!len)return 0;
    for(i=0;i<3;i++)out[i]=(tmp[i]*32767)/(SS89_i32)len;
    return 1;
}
static void vec_cross(const SS89_i32 a[3],const SS89_i32 b[3],SS89_i32 out[3])
{
    out[0]=(a[1]/8)*(b[2]/8)-(a[2]/8)*(b[1]/8);
    out[1]=(a[2]/8)*(b[0]/8)-(a[0]/8)*(b[2]/8);
    out[2]=(a[0]/8)*(b[1]/8)-(a[1]/8)*(b[0]/8);
}
static SS89_i32 mul_p(SS89_i32 a,SS89_i32 b)
{
    /* Q16 * Q15 -> Q16 using safe 32-bit partial products. */
    return (a/32768)*b + ((a%32768)*b)/32768;
}
static void pose_transform(const S89_Pose *p,const SS89_i32 in[3],SS89_i32 out[3])
{
    int i,j;SS89_i32 n;
    for(i=0;i<3;i++){
        n=p->p[i];
        for(j=0;j<3;j++)n=add32(n,mul_p(in[j],p->m[i][j]));
        out[i]=n;
    }
}
static void pose_compose(const S89_Pose *a,const S89_Pose *b,S89_Pose *out)
{
    int i,j,k;SS89_i32 v;
    for(i=0;i<3;i++)for(j=0;j<3;j++){
        v=0;
        for(k=0;k<3;k++)v=add32(v,(a->m[i][k]*b->m[k][j])/32768);
        out->m[i][j]=v;
    }
    pose_transform(a,b->p,out->p);
}
static int anchor_pose(const SS89_Attachment *att,const SS89_Vertex *pts,S89_Pose *out)
{
    SS89_i32 c[3],f[3],u[3],vx[3],vy[3],vz[3],d[3];int i;
    const SS89_Vertex *pc=&pts[att->center_vertex],*pf=&pts[att->front_vertex],*pu=&pts[att->up_vertex];
    c[0]=pc->x;c[1]=pc->y;c[2]=pc->z;
    f[0]=pf->x;f[1]=pf->y;f[2]=pf->z;
    u[0]=pu->x;u[1]=pu->y;u[2]=pu->z;
    for(i=0;i<3;i++){d[i]=diff32(c[i],f[i]);}
    if(!vec_norm(d,vz))return 0;
    for(i=0;i<3;i++)d[i]=diff32(u[i],c[i]);
    vec_cross(d,vz,vx);
    if(!vec_norm(vx,vx))return 0;
    vec_cross(vz,vx,vy);
    if(!vec_norm(vy,vy))return 0;
    set_identity(out);
    for(i=0;i<3;i++){
        out->m[i][0]=vx[i];out->m[i][1]=vy[i];out->m[i][2]=vz[i];
        out->p[i]=c[i];
    }
    /* Source reference pose offset is expressed in attachment local coordinates. */
    for(i=0;i<3;i++){
        out->p[i]=add32(out->p[i],mul_p(att->offset[0],out->m[i][0]));
        out->p[i]=add32(out->p[i],mul_p(att->offset[1],out->m[i][1]));
        out->p[i]=add32(out->p[i],mul_p(att->offset[2],out->m[i][2]));
    }
    return 1;
}
static char *trim(char *s)
{
    char *e;
    while(*s && isspace((unsigned char)*s))s++;
    e=s+strlen(s);
    while(e>s&&isspace((unsigned char)e[-1]))--e;
    *e=0;return s;
}
static int keyword(const char *s,const char *kw)
{
    size_t n=strlen(kw);return strncmp(s,kw,n)==0 && s[n]==':';
}
static void get_amc_value(const char *line,const char *key,char *dest,size_t size)
{
    const char *v=line+strlen(key)+1;
    size_t n;
    while(*v && isspace((unsigned char)*v))v++;
    if(!strncmp(v,"TFNM",4) && isspace((unsigned char)v[4])){
        v+=4;while(*v && isspace((unsigned char)*v))v++;
    }
    n=strlen(v);
    if(n>=2 && v[0]=='"' && v[n-1]=='"'){v++;n-=2;}
    if(n>=size)n=size-1;
    memcpy(dest,v,n);dest[n]=0;
    for(n=0;dest[n];n++)if(dest[n]=='\\')dest[n]='/';
}
static int read_amc(const char *path)
{
    FILE *f=fopen(path,"rb");char line[S89_AMC_LINE],*t;
    int stack[S89_DEPTH],depth=0,pending=-1,skip=0,pending_skip=0,i;
    if(!f)return 0;
    memset(nodes,0,sizeof(nodes));node_count=1;nodes[0].parent=-1;nodes[0].animation=0;
    for(i=0;i<S89_DEPTH;i++)stack[i]=0;
    while(fgets(line,sizeof(line),f)){
        t=trim(line);
        if(!*t||*t==';'||*t=='#'||!strncmp(t,"//",2))continue;
        if(*t=='{'){
            if(depth>=S89_DEPTH-1){fclose(f);return 0;}
            if(skip||pending_skip){skip++;pending_skip=0;stack[depth++]=-1;}
            else {int sel=pending>=0?pending:(depth?stack[depth-1]:0); stack[depth]=sel; depth++; pending=-1;}
            continue;
        }
        if(*t=='}'){
            if(depth==0){fclose(f);return 0;}
            if(stack[depth-1]==-1)skip--;
            depth--;continue;
        }
        if(skip)continue;
        if(!strncmp(t,"PreviewOnly",11)){pending_skip=1;continue;}
        if(keyword(t,"Attachment")){
            if(node_count>=S89_NODES || depth==0){fclose(f);return 0;}
            nodes[node_count].parent=stack[depth-1];
            nodes[node_count].parent_attach=atoi(t+11);
            nodes[node_count].animation=0;
            pending=node_count++;
            continue;
        }
        if(keyword(t,"Model"))get_amc_value(t,"Model",nodes[stack[depth?depth-1:0]].model,S89_PATH);
        else if(keyword(t,"Texture"))get_amc_value(t,"Texture",nodes[stack[depth?depth-1:0]].texture,S89_PATH);
        else if(keyword(t,"Animation"))nodes[stack[depth?depth-1:0]].animation=atoi(t+10);
    }
    fclose(f);
    if(depth || !nodes[0].model[0])return 0;
    for(i=0;i<node_count;i++)if(!nodes[i].model[0] || (i>0&&nodes[i].parent>=i))return 0;
    return 1;
}
static int diskpath(const char *root,const char *relative,char out[S89_PATH])
{
    size_t n=strlen(root),k;
    if(!relative[0]||strstr(relative,"..")||relative[0]=='/'||relative[0]=='\\'||strchr(relative,':'))return 0;
    if(n+strlen(relative)+2>=S89_PATH)return 0;
    strcpy(out,root);
    if(n>0&&out[n-1]!='/')strcat(out,"/");
    strcat(out,relative);
    for(k=0;out[k];k++)if(out[k]=='\\')out[k]='/';
    return 1;
}
static void qout(FILE *f,SS89_i32 v)
{
    SS89_u32 mag,integer,frac;
    if(v<0){fputc('-',f);mag=(SS89_u32)(-(v+1))+1U;}else mag=(SS89_u32)v;
    integer=mag>>16;frac=((mag&65535U)*10000U+32768U)>>16;
    if(frac>=10000U){integer++;frac=0U;}
    fprintf(f,"%u.%04u",(unsigned)integer,(unsigned)frac);
}
static void uvout(FILE *f,SS89_i32 val,SS89_i32 denom)
{
    SS89_u32 mag,d,w,frac;
    if(denom<=0){fputs("0.0000",f);return;}
    if(val<0){fputc('-',f);mag=(SS89_u32)(-(val+1))+1U;}else mag=(SS89_u32)val;
    d=(SS89_u32)denom;w=mag/d;frac=((mag%d)*10000U+d/2U)/d;
    if(frac>=10000U){w++;frac=0U;}
    fprintf(f,"%u.%04u",(unsigned)w,(unsigned)frac);
}
static int textureinfo(const char *path,SS89_i32 *w,SS89_i32 *h,int *alpha)
{
    unsigned char b[36];FILE *f=fopen(path,"rb");SS89_u32 ver,flags;
    if(!f)return 0;
    if(fread(b,1,36U,f)!=36U){fclose(f);return 0;}
    fclose(f);
    if(memcmp(b,"TVER",4)||memcmp(b+8,"TDAT",4))return 0;
    ver=(SS89_u32)b[4]|((SS89_u32)b[5]<<8);
    if(ver!=3U&&ver!=4U)return 0;
    flags=(SS89_u32)b[12];
    *w=(SS89_i32)((SS89_u32)b[16]|((SS89_u32)b[17]<<8)|((SS89_u32)b[18]<<16)|((SS89_u32)b[19]<<24));
    *h=(SS89_i32)((SS89_u32)b[20]|((SS89_u32)b[21]<<8)|((SS89_u32)b[22]<<16)|((SS89_u32)b[23]<<24));
    *alpha=(flags&1U)?1:0;
    return *w>0&&*h>0&&*w<=1048576&&*h<=1048576;
}
static char scene_texture_prefix[40]="ss";
static unsigned scene_mip=0U,scene_texture_frame=0U;
static int scene_auto_mip=0;
static SS89_i32 scene_mip_factor=0;
/* Avoid material cross-contamination when dozens of creatures are exported
   into the same directory. The v0.5 fixed 'ss_part_00' name was unsafe. */
static void select_texture_prefix(const char *objpath)
{
    const char *base=objpath,*p=objpath;
    size_t i=0;
    while(*p){if(*p=='/'||*p=='\\')base=p+1;p++;}
    while(base[i] && base[i]!='.' && i<sizeof(scene_texture_prefix)-1U){
        unsigned char c=(unsigned char)base[i];
        scene_texture_prefix[i]=(isalnum(c)||c=='_'||c=='-')?(char)c:'_';
        i++;
    }
    if(!i){strcpy(scene_texture_prefix,"ss");return;}
    scene_texture_prefix[i]=0;
}
static void texture_name(unsigned part,const char *ext,char *out)
{
    sprintf(out,"%s_part_%02u.%s",scene_texture_prefix,part,ext);
}
static void safe_color(FILE *f,SS89_u32 color)
{
    unsigned r=(unsigned)((color>>24)&255U),g=(unsigned)((color>>16)&255U),b=(unsigned)((color>>8)&255U);
    fprintf(f,"Kd %u.%03u %u.%03u %u.%03u\n",r/255U,((r%255U)*1000U)/255U,g/255U,((g%255U)*1000U)/255U,b/255U,((b%255U)*1000U)/255U);
}
static int export_amc(const char *root,const char *objpath,int rootframe,int key,int textures,
                      int has_clock,unsigned time_ms,const char *rootclip,const char *bodyclip,int sync_body,int loop)
{
    FILE *obj=NULL,*mtl=NULL,*fx=NULL;
    char path[S89_PATH],texpath[S89_PATH],mtlpath[S89_PATH],fxpath[S89_PATH],base[S89_PATH],texout[S89_PATH],texname[64];
    const char *slash;SS89_u32 vbase=0U,vtbase=0U;SS89_i32 texw,texh;
    int i,rc,alpha,anim;unsigned j,t,s,frame;S89_Pose local,wp;S89_Sample sample,root_sample;unsigned root_keys=0U;
    size_t length;
    select_texture_prefix(objpath);
    obj=fopen(objpath,"wb");if(!obj)return 0;
    length=strlen(objpath);
    if(length+8>=sizeof(mtlpath)){fclose(obj);return 0;}
    strcpy(mtlpath,objpath);if(length>4&&!strcmp(mtlpath+length-4,".obj"))strcpy(mtlpath+length-4,".mtl");else strcat(mtlpath,".mtl");
    slash=strrchr(mtlpath,'/');if(!slash)slash=strrchr(mtlpath,'\\');
    if(slash){size_t n=(size_t)(slash-mtlpath)+1U;if(n>=sizeof(base)){fclose(obj);return 0;}memcpy(base,mtlpath,n);base[n]=0;}else strcpy(base,"");
    if(strlen(mtlpath)+1U>=sizeof(fxpath)){fclose(obj);return 0;}
    strcpy(fxpath,mtlpath);
    length=strlen(fxpath);
    if(length>4U && !strcmp(fxpath+length-4U,".mtl"))strcpy(fxpath+length-4U,".fx89");
    mtl=fopen(mtlpath,"wb");if(!mtl){fclose(obj);return 0;}
    fx=fopen(fxpath,"wb");if(!fx){fclose(obj);fclose(mtl);return 0;}
    fprintf(fx,"# SS89 material state description v1; stages require a renderer\n");
    fprintf(fx,"# GFX enum mapping audited against Serious Engine 1.10\n");
    fprintf(obj,"# Serious Sam 1 AMC assembled with independent C89 codec\nmtllib %s\n",slash?slash+1:mtlpath);
    for(i=0;i<node_count;i++){
        S89_Node *node=&nodes[i];
        if(!diskpath(root,node->model,path) || (rc=ss89_open(&model,path))!=SS89_OK){fprintf(stderr,"[scene] cannot open %s\n",path);goto failed;}
        if(scene_auto_mip){
            unsigned selected=ss89_choose_mip(&model,scene_mip_factor);
            if(ss89_select_mip(&model,selected)!=SS89_OK)goto failed;
            fprintf(stderr,"[scene] %s: FMIP factor %d.%03d -> LOD %u\n",node->model,
                    (int)(scene_mip_factor/65536), (int)(((unsigned)(scene_mip_factor<0 && scene_mip_factor!=(-2147483647-1)?-scene_mip_factor:scene_mip_factor)%65536U)*1000U/65536U),selected);
        }else if(scene_mip){
            if(ss89_select_mip(&model,scene_mip)!=SS89_OK){fprintf(stderr,"[scene] invalid LOD %u\n",scene_mip);goto failed;}
        }
        if(((model.flags&SS89_MF_COMPRESSED_16BIT)==0U || model.frame_legacy_normals) && !scene_gouraud_ready)
            fprintf(stderr,"[scene] WARNING: 8-bit Gouraud normals unavailable for %s; use --gouraud-table FILE\n",node->model);
        anim=node->animation;
        if(i==0 && rootclip && *rootclip){anim=find_clip(&model,rootclip);}
        if(i==1 && bodyclip && *bodyclip){anim=find_clip(&model,bodyclip);}
        if(anim<0 || (unsigned)anim>=model.anim_count){fprintf(stderr,"[scene] clip not found for %s\n",node->model);goto failed;}
        if(has_clock){
            if(i==1 && sync_body && root_keys>0U){
                if(!sample_phase(&model,anim,root_sample.key,root_sample.fraction,root_keys,&sample))goto failed;
            } else {
                if(!sample_time(&model,anim,time_ms,loop,&sample))goto failed;
            }
        } else {
            if(!sample_keys(&model,anim,0U,0U,&sample))goto failed;
            if(i==0 && rootframe>=0){
                if((unsigned)rootframe>=model.frames)goto failed;
                sample.frame0=(unsigned)rootframe;sample.frame1=sample.frame0;
            }
            if(i==0 && key>=0){
                if((unsigned)key>=model.anims[anim].keys)goto failed;
                if(!sample_keys(&model,anim,(unsigned)key,0U,&sample))goto failed;
            }
        }
        if(i==0){root_sample=sample;root_keys=model.anims[anim].keys;}
        frame=sample.frame0;
        if(!decode_interpolated(&model,&sample)){fprintf(stderr,"[scene] frame decode error\n");goto failed;}
        printf("[clock] part=%d clip=%s key=%u next_frame=%u lerp=%u/65536 time_ms=%u\n",i,model.anims[anim].name,sample.key,sample.frame1,sample.fraction,time_ms);
        if(i==0)set_identity(&node->world);
        else {
            S89_Node *parent=&nodes[node->parent];
            if(node->parent_attach<0||(unsigned)node->parent_attach>=parent->anchors || !parent->anchor_valid[node->parent_attach]){fprintf(stderr,"[scene] attachment %d missing or unsupported\n",node->parent_attach);goto failed;}
            node->world=parent->anchor[node->parent_attach];
        }
        if(model.attachments>SS89_MAX_ATTACHMENTS)goto failed;
        node->anchors=model.attachments;
        for(j=0U;j<model.attachments;j++){
            SS89_Attachment *a=&model.attachment_data[j];
            if(a->center_vertex>=model.vertices || a->front_vertex>=model.vertices || a->up_vertex>=model.vertices){
                node->anchor_valid[j]=0;continue;
            }
            if(a->angles[0]||a->angles[1]||a->angles[2]){
                /* Keep unsupported attachments unbound unless a child references one. */
                node->anchor_valid[j]=0;continue;
            }
            if(!anchor_pose(a,points,&local)){
                fprintf(stderr,"[scene] degenerate attachment in %s\n",node->model);goto failed;
            }
            pose_compose(&node->world,&local,&wp);
            node->anchor[j]=wp;node->anchor_valid[j]=1;
        }
        for(j=0U;j<model.vertices;j++){
            SS89_i32 v[3],dest[3];
            v[0]=points[j].x;v[1]=points[j].y;v[2]=points[j].z;
            pose_transform(&node->world,v,dest);
            worldpoints[j].x=dest[0];worldpoints[j].y=dest[1];worldpoints[j].z=dest[2];
        }
        if(!diskpath(root,node->texture,texpath)||!textureinfo(texpath,&texw,&texh,&alpha)){
            fprintf(stderr,"[scene] texture header invalid: %s\n",node->texture);goto failed;
        }
        texture_name((unsigned)i,alpha?"pam":"ppm",texname);
        if(textures){
            if(strlen(base)+strlen(texname)+1U>=sizeof(texout))goto failed;
            strcpy(texout,base);strcat(texout,texname);
            if(ss89_resource(texpath,texout,scene_texture_frame,0U)!=0){fprintf(stderr,"[scene] texture decode failed\n");goto failed;}
        }
        fprintf(obj,"o Part_%02d\n",i);
        for(j=0U;j<model.vertices;j++){
            fputs("v ",obj);qout(obj,worldpoints[j].x);fputc(' ',obj);qout(obj,worldpoints[j].y);fputc(' ',obj);qout(obj,worldpoints[j].z);fputc('\n',obj);
        }
        /* Native MDL AV17 normals: heading/pitch decoded with integer LUTs.
           Rotated to parent attachment space and exported as OBJ normals. */
        if(((model.flags&SS89_MF_COMPRESSED_16BIT)!=0U && !model.frame_legacy_normals) || scene_gouraud_ready)for(j=0U;j<model.vertices;j++){
            int n[3],k;SS89_i32 wn[3],nv[3];
            for(k=0;k<3;k++)n[k]=(int)interpolated_normals[j][k];
            for(k=0;k<3;k++)nv[k]=n[k];
            for(k=0;k<3;k++)wn[k]=mul_p(nv[0],node->world.m[k][0])+mul_p(nv[1],node->world.m[k][1])+mul_p(nv[2],node->world.m[k][2]);
            fputs("vn ",obj);
            for(k=0;k<3;k++){
                if(k)fputc(' ',obj);
                qout(obj,wn[k]*2);
            }
            fputc('\n',obj);
        }
        for(j=0U;j<model.uv_count;j++){
            fputs("vt ",obj);uvout(obj,model.texvertices[j].u_mex,texw);
            fputc(' ',obj); /* Flip V for OBJ bottom-origin texture readers. */
            if(model.texvertices[j].v_mex>0){fputc('-',obj);uvout(obj,model.texvertices[j].v_mex,texh);}
            else if(model.texvertices[j].v_mex<0)uvout(obj,-model.texvertices[j].v_mex,texh);
            else fputs("0.0000",obj);
            fputc('\n',obj);
        }
        /* One independently named OBJ material per SE1 mapping surface. */
        for(s=0U;s<model.surface_count;s++){
            SS89_Surface *surf=&model.surfaces[s];SS89_u32 diff=surf->diffuse;SS89_FXState rs;
            if(!ss89_fx_classify(surf,&rs)){fprintf(stderr,"[scene] invalid render state\n");goto failed;}
            if(!diff)diff=0xffffffffU;
            fprintf(fx,"material=%d:%u name=%s visible=%d two_sided=%d shading=%d blend=%s alpha_test=%d depth_write=%d diffuse=%d reflection=%d specular=%d bump=%d detail=%d fog=%d haze=%d color_diffuse=%08x color_specular=%08x color_reflection=%08x color_bump=%08x\n",i,s,surf->name,rs.visible,rs.double_sided,surf->shading,ss89_fx_blend_name(rs.blend),rs.alpha_test,rs.depth_write,rs.diffuse,rs.reflection,rs.specular,rs.bump,rs.detail,rs.fog,rs.haze,rs.color_diffuse,rs.color_specular,rs.color_reflection,rs.color_bump);
            fprintf(mtl,"newmtl SS_Part_%02d_Surface_%03u\n",i,(unsigned)s);
            fprintf(mtl,"# %s flags=%08x shading=%d translucency=%d\n",surf->name,(unsigned)surf->flags,(int)surf->shading,(int)surf->translucency);
            fputs("Ka 0 0 0\n",mtl);safe_color(mtl,diff);
            fputs("Ks 0 0 0\nillum 1\n",mtl);
            if(surf->translucency==1 || surf->translucency==3)fputs("d 0.600\n",mtl);
            else fputs("d 1.000\n",mtl);
            if(rs.diffuse)fprintf(mtl,"map_Kd %s\n",texname);
            if(rs.alpha_test && alpha)fprintf(mtl,"map_d %s\n",texname);
            if(rs.blend==SS89_FX_BLEND_ADD || rs.blend==SS89_FX_BLEND_MULTIPLY)
                fputs("# OBJ/MTL cannot express source GFX blending; consult .fx89\n",mtl);
            fputc('\n',mtl);
        }
        for(t=0U;t<model.triangles_count;t++){
            SS89_Triangle *tri=&model.triangles[t];
            if(tri->surface>=model.surface_count)goto failed;
            if((model.surfaces[tri->surface].flags&S89_SRF_INVISIBLE)!=0U)continue;
            if(t==0U || model.triangles[t-1U].surface!=tri->surface)
                fprintf(obj,"usemtl SS_Part_%02d_Surface_%03u\n",i,(unsigned)tri->surface);
            if(((model.flags&SS89_MF_COMPRESSED_16BIT)!=0U && !model.frame_legacy_normals) || scene_gouraud_ready){
                fprintf(obj,"f %u/%u/%u %u/%u/%u %u/%u/%u\n",
                    (unsigned)(tri->v[0]+vbase+1U),(unsigned)(tri->uv[0]+vtbase+1U),(unsigned)(tri->v[0]+vbase+1U),
                    (unsigned)(tri->v[1]+vbase+1U),(unsigned)(tri->uv[1]+vtbase+1U),(unsigned)(tri->v[1]+vbase+1U),
                    (unsigned)(tri->v[2]+vbase+1U),(unsigned)(tri->uv[2]+vtbase+1U),(unsigned)(tri->v[2]+vbase+1U));
            }else{
                fprintf(obj,"f %u/%u %u/%u %u/%u\n",
                    (unsigned)(tri->v[0]+vbase+1U),(unsigned)(tri->uv[0]+vtbase+1U),
                    (unsigned)(tri->v[1]+vbase+1U),(unsigned)(tri->uv[1]+vtbase+1U),
                    (unsigned)(tri->v[2]+vbase+1U),(unsigned)(tri->uv[2]+vtbase+1U));
            }
        }
        printf("[scene] part=%d %s frame=%u vertices=%u tris=%u UV=%u material_surf=%u texture_MEX=%dx%d\n",i,node->model,(unsigned)frame,(unsigned)model.vertices,(unsigned)model.triangles_count,(unsigned)model.uv_count,(unsigned)model.surface_count,(int)texw,(int)texh);
        vbase+=model.vertices;vtbase+=model.uv_count;
        ss89_close(&model);
    }
    if(fclose(fx)||fclose(mtl)||fclose(obj))return 0;
    printf("[scene] exported %d model parts -> %s\n",node_count,objpath);
    return 1;
failed:
    ss89_close(&model);if(fx)fclose(fx);if(mtl)fclose(mtl);if(obj)fclose(obj);remove(objpath);remove(mtlpath);remove(fxpath);return 0;
}
int main(int argc,char **argv)
{
    const char *amc=NULL,*root=NULL,*obj=NULL,*rootclip=NULL,*bodyclip=NULL;
    const char *single_model=NULL,*single_texture=NULL;
    const char *sequence=NULL,*gouraud_table=NULL;
    int i,frame=-1,key=-1,textures=1,has_clock=0,sync_body=0,loop=1;
    unsigned time_ms=0U,count=0U,step_ms=50U,n;
    char name[S89_PATH];
    if(argc<2){fprintf(stderr,"usage: ss89scene file.amc --root assets --obj file.obj [--model relative.mdl --texture relative.tex] [--root-clip RUN --body-clip NORMALWALK --time-ms 150 --sync-body] [--sequence prefix --count 23 --step-ms 50]\n");return 2;}
    amc=argv[1];
    for(i=2;i<argc;i++){
        if(!strcmp(argv[i],"--model")&&i+1<argc)single_model=argv[++i];
        else if(!strcmp(argv[i],"--texture")&&i+1<argc)single_texture=argv[++i];
        else if(!strcmp(argv[i],"--root")&&i+1<argc)root=argv[++i];
        else if(!strcmp(argv[i],"--obj")&&i+1<argc)obj=argv[++i];
        else if(!strcmp(argv[i],"--frame")&&i+1<argc)frame=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--key")&&i+1<argc)key=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--time-ms")&&i+1<argc){time_ms=(unsigned)strtoul(argv[++i],NULL,10);has_clock=1;}
        else if(!strcmp(argv[i],"--root-clip")&&i+1<argc)rootclip=argv[++i];
        else if(!strcmp(argv[i],"--body-clip")&&i+1<argc)bodyclip=argv[++i];
        else if(!strcmp(argv[i],"--sync-body"))sync_body=1;
        else if(!strcmp(argv[i],"--once"))loop=0;
        else if(!strcmp(argv[i],"--sequence")&&i+1<argc)sequence=argv[++i];
        else if(!strcmp(argv[i],"--count")&&i+1<argc)count=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--step-ms")&&i+1<argc)step_ms=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--mip")&&i+1<argc)scene_mip=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--auto-mip-factor")&&i+1<argc){
            long raw=strtol(argv[++i],NULL,10);
            if(raw<(-2147483647L-1L) || raw>2147483647L)return 2;
            scene_auto_mip=1;scene_mip_factor=(SS89_i32)raw;
        }
        else if(!strcmp(argv[i],"--gouraud-table")&&i+1<argc)gouraud_table=argv[++i];
        else if(!strcmp(argv[i],"--texture-frame")&&i+1<argc)scene_texture_frame=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--no-textures"))textures=0;
        else {fprintf(stderr,"invalid option %s\n",argv[i]);return 2;}
    }
    if(!root){fprintf(stderr,"missing --root\n");return 2;}
    if(scene_auto_mip && scene_mip){fprintf(stderr,"choose --mip or --auto-mip-factor, not both\n");return 2;}
    if(gouraud_table){
        if(!ss89_gouraud_load(gouraud_table)){fprintf(stderr,"invalid Gouraud table (need 1536 bytes)\n");return 2;}
        scene_gouraud_ready=1;
    }
    if(single_model || single_texture){
        if(!single_model || !single_texture || strlen(single_model)>=S89_PATH || strlen(single_texture)>=S89_PATH){
            fprintf(stderr,"single-model mode needs --model and --texture\n");return 2;
        }
        memset(nodes,0,sizeof(nodes));node_count=1;
        strcpy(nodes[0].model,single_model);strcpy(nodes[0].texture,single_texture);
        nodes[0].parent=-1;nodes[0].animation=0;
    }else if(!read_amc(amc)){fprintf(stderr,"invalid AMC\n");return 2;}
    if(frame>=0 && key>=0){fprintf(stderr,"specify only --frame or --key\n");return 2;}
    if(has_clock && (frame>=0||key>=0)){fprintf(stderr,"clock cannot be combined with --frame or --key\n");return 2;}
    if(sync_body && (!rootclip||!bodyclip)){fprintf(stderr,"sync requires --root-clip and --body-clip\n");return 2;}
    if(sequence){
        if(count==0U||count>500U || step_ms>10000U || strlen(sequence)>S89_PATH-20U){fprintf(stderr,"invalid sequence count/path\n");return 2;}
        for(n=0U;n<count;n++){
            if(n && step_ms > (0xffffffffU-time_ms)/n)return 2;
            sprintf(name,"%s_%04u.obj",sequence,n);
            if(!export_amc(root,name,frame,key,textures,1,time_ms+n*step_ms,rootclip,bodyclip,sync_body,loop))return 1;
        }
        return 0;
    }
    if(!obj){fprintf(stderr,"missing --obj\n");return 2;}
    return export_amc(root,obj,frame,key,textures,has_clock,time_ms,rootclip,bodyclip,sync_body,loop)?0:1;
}
