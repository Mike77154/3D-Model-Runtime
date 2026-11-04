/* RTCW .md3 / .mdc / .mds mesh loader. Copyright waived under CC0-1.0.
   Strict ANSI C89, no dynamic allocation, no float/double. Input little endian.
   F32 is decoded by integer IEEE754 bit manipulation into fixed point.
*/
#include "rtcw_codec.h"
#include "rtcw_mds_pose.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* No VLA, malloc, calloc, realloc, or free. */
static unsigned char raw[RTCW_MAX_FILE];
static unsigned long raw_size;
rtcw_model rtcw_current;
static char error_text[192] = "no file loaded";
static int invalid;
static char current_fmt[4];
static long bn_parent[RTCW_MAX_BONES], bn_distance[RTCW_MAX_BONES];
static long bn_mat[RTCW_MAX_BONES][3][3];
static long bn_origin[RTCW_MAX_BONES][3];
static void angles_matrix(unsigned short pitch,unsigned short yaw,unsigned short roll,long m[3][3]);
static rtcw_pose_input pose_in;
static rtcw_pose_output pose_out;
static void store_axis(long dst[3][3],long m[3][3]) {
    unsigned int i,j;
    for(i=0;i<3;i++)for(j=0;j<3;j++)dst[i][j]=m[j][i];
}
/* MDC angles encode a turn in 32700 signed steps (NOT 65536). */
static unsigned short mdc_turn(long val) {
    long turns=val*2L+(val*34L)/8175L;
    return (unsigned short)(turns & 65535L);
}

static void err(const char *s) { strncpy(error_text,s,sizeof(error_text)-1); error_text[sizeof(error_text)-1]=0; invalid=1; }
const char *rtcw_error(void) { return error_text; }
static int span(unsigned long off, unsigned long count, unsigned long item) {
    if(off>raw_size || item==0 || count>(raw_size-off)/item) { err("truncated file / offset outside bounds");return 0; }
    return 1;
}
static unsigned long u32(unsigned long off) {
    if(!span(off,1,4))return 0;
    return (unsigned long)raw[off] | ((unsigned long)raw[off+1]<<8) | ((unsigned long)raw[off+2]<<16) | ((unsigned long)raw[off+3]<<24);
}
static unsigned int u16(unsigned long off) {
    if(!span(off,1,2))return 0;
    return (unsigned int)raw[off] | ((unsigned int)raw[off+1]<<8);
}
static long s16(unsigned long off) {
    unsigned int x=u16(off);return x<32768U?(long)x:(long)x-65536L;
}
static long s32(unsigned long off) {
    unsigned long x=u32(off);return (x & 0x80000000UL)?-1L-(long)(~x & 0x7fffffffUL):(long)x;
}
static void name64(char *out, unsigned long off) {
    unsigned int i;
    if(!span(off,64,1)){out[0]=0;return;}
    for(i=0;i<64;i++){out[i]=(char)raw[off+i];if(out[i]==0)break;}
    out[64]=0;
}
/* Interpret binary32 without any native FP operation. q = fixed bits, range guarded. */
static long f32q(unsigned long off, int frac) {
    unsigned long bits, mant;
    int exp, shift;
    long n;
    bits=u32(off);exp=(int)((bits>>23)&255UL);
    if(exp==255){err("NaN/Infinity in model");return 0;}
    if(exp==0)return 0; /* subnormal below Q12 significance */
    mant=(bits&0x7fffffUL)|0x800000UL;
    shift=exp-127-23+frac;
    if(shift>=0) {
        if(shift>7 || mant>(0x7fffffffUL >> shift)){err("fixed coordinate overflow");return 0;}
        n=(long)(mant<<shift);
    } else {
        if(shift< -31)return 0;
        n=(long)(mant>>(-shift));
    }
    return (bits&0x80000000UL)?-n:n;
}
static int region(unsigned long base, unsigned long length,unsigned long rel,unsigned long n,unsigned long step) {
    if(rel>length || step==0 || n>(length-rel)/step) {err("invalid surface region");return 0;}
    return span(base+rel,n,step);
}
static int new_surface(unsigned long nverts,unsigned long ntris,unsigned int idx,
                       const char *nm,const char *shader,long lod) {
    rtcw_surface *s;
    if(idx>=RTCW_MAX_SURFACES || nverts==0 || nverts>RTCW_MAX_VERTICES-rtcw_current.vertex_count ||
       ntris>RTCW_MAX_TRIANGLES-rtcw_current.triangle_count){err("surface/geometry capacity exceeded");return 0;}
    s=&rtcw_current.surfaces[idx];
    strncpy(s->name,nm,64);s->name[64]=0;
    strncpy(s->shader,shader,64);s->shader[64]=0;
    s->first_vertex=rtcw_current.vertex_count;s->vertices=nverts;
    s->first_triangle=rtcw_current.triangle_count;s->triangles=ntris;
    s->min_lod=lod;
    rtcw_current.vertex_count+=nverts;rtcw_current.triangle_count+=ntris;
    return 1;
}
static int tris_at(unsigned long off,unsigned long nv,unsigned long nt,unsigned int idx) {
    unsigned long t,j,a,b,c;
    rtcw_surface *s=&rtcw_current.surfaces[idx];
    if(!span(off,nt,12))return 0;
    for(t=0;t<nt;t++) {
        j=off+t*12;a=u32(j);b=u32(j+4);c=u32(j+8);
        if(a>=nv||b>=nv||c>=nv){err("triangle index outside surface");return 0;}
        rtcw_current.tris[s->first_triangle+t].a=s->first_vertex+a;
        rtcw_current.tris[s->first_triangle+t].b=s->first_vertex+b;
        rtcw_current.tris[s->first_triangle+t].c=s->first_vertex+c;
        rtcw_current.tris[s->first_triangle+t].surface=idx;
    }
    return 1;
}
static int md3_decode(unsigned long frame) {
    unsigned long nf,ns,nt,ofs_s,end,ofs_fr,ofs_tags,here,off_e,sv,st,sh,tri,xyz,nv,ntt,nsh,snf,uv,p;
    unsigned int i;
    char nm[65],shader[65];
    rtcw_surface *s;
    if(raw_size<108 || u32(4)!=15UL){err("not MD3 v15");return 0;}
    nf=u32(76);nt=u32(80);ns=u32(84);ofs_fr=u32(92);ofs_tags=u32(96);ofs_s=u32(100);end=u32(104);
    if(nf<1||nf>1024||nt>16||ns>32||end>raw_size || frame>=nf){err("bad MD3 header counts");return 0;}
    if(!span(ofs_fr,nf,56)||!span(ofs_tags,nf*nt,112))return 0;
    name64(rtcw_current.name,8);rtcw_current.frames=nf;rtcw_current.frame=frame;
    rtcw_current.tag_count=nt;rtcw_current.surface_count=(unsigned int)ns;
    for(i=0;i<nt;i++) {
        name64(rtcw_current.tags[i].name,ofs_tags+frame*nt*112UL+i*112UL);
        rtcw_current.tags[i].bone_index=-1;
        { unsigned long tp=ofs_tags+frame*nt*112UL+i*112UL+64UL;
          unsigned int j,k;
          for(j=0;j<3;j++)rtcw_current.tags[i].origin[j]=f32q(tp+4UL*j,6);
          for(j=0;j<3;j++)for(k=0;k<3;k++)
              rtcw_current.tags[i].axis[j][k]=f32q(tp+12UL+4UL*(3UL*j+k),14); }
    }
    here=ofs_s;
    for(i=0;i<ns;i++) {
        if(!span(here,1,108))return 0;
        if(memcmp(raw+here,"IDP3",4)!=0){err("MD3 surface signature invalid");return 0;}
        name64(nm,here+4);
        snf=u32(here+72);nsh=u32(here+76);nv=u32(here+80);ntt=u32(here+84);
        tri=u32(here+88);sh=u32(here+92);uv=u32(here+96);xyz=u32(here+100);off_e=u32(here+104);
        if(snf!=nf||nv==0||nv>4096||ntt>8192||nsh>256||off_e<108||off_e>raw_size-here){err("bad MD3 surface header");return 0;}
        if(!region(here,off_e,tri,ntt,12)||!region(here,off_e,sh,nsh,68)||
           !region(here,off_e,uv,nv,8)||!region(here,off_e,xyz,nf*nv,8))return 0;
        shader[0]=0;if(nsh)name64(shader,here+sh);
        if(!new_surface(nv,ntt,i,nm,shader,0))return 0;
        s=&rtcw_current.surfaces[i];
        if(!tris_at(here+tri,nv,ntt,i))return 0;
        for(sv=0;sv<nv;sv++) {
            rtcw_vertex *v=&rtcw_current.verts[s->first_vertex+sv];
            p=here+xyz+(frame*nv+sv)*8;v->x=s16(p);v->y=s16(p+2);v->z=s16(p+4);
            st=here+uv+sv*8;v->u=f32q(st,12);v->v=f32q(st+4,12);
        }
        here+=off_e;
    }
    return !invalid;
}
/* An MDC compressed vertex encodes (byte - 127) * .05 world units.
   Q6 stores integer round((byte - 127) * 64/20) = round(delta*16/5). */
static long mdc_delta_q6(unsigned char b) {
    long d=(long)b-127L;
    if(d<0)return -(((-d)*16L+2L)/5L);
    return (d*16L+2L)/5L;
}
static int mdc_decode(unsigned long frame) {
    unsigned long nf,ns,nt,here,end,ofs_fr,ofs_tags,ofs_tagframes;
    unsigned long nverts,ntris,nsh,nb,nc,len,tri,sh,uv,base,comp,fb,fc,off_e;
    unsigned long t,st,at,p,b_idx,c_idx;
    unsigned int i,k;
    char nm[65],shader[65];
    rtcw_surface *s;
    if(raw_size<112||u32(4)!=2UL){err("not MDC v2");return 0;}
    nf=u32(76);nt=u32(80);ns=u32(84);ofs_fr=u32(92);ofs_tags=u32(96);ofs_tagframes=u32(100);here=u32(104);end=u32(108);
    if(nf<1||nf>1024||nt>16||ns>32||end>raw_size||frame>=nf){err("bad MDC header counts");return 0;}
    if(!span(ofs_fr,nf,56)||!span(ofs_tags,nt,64)||!span(ofs_tagframes,nf*nt,12))return 0;
    name64(rtcw_current.name,8);rtcw_current.frames=nf;rtcw_current.frame=frame;
    rtcw_current.tag_count=nt;rtcw_current.surface_count=(unsigned int)ns;
    for(i=0;i<nt;i++) {
        name64(rtcw_current.tags[i].name,ofs_tags+i*64UL);
        rtcw_current.tags[i].bone_index=-1;
        { unsigned long tp=ofs_tagframes+frame*nt*12UL+i*12UL;
          unsigned int j; long m[3][3];
          for(j=0;j<3;j++)rtcw_current.tags[i].origin[j]=s16(tp+2UL*j);
          angles_matrix(mdc_turn(s16(tp+6)),mdc_turn(s16(tp+8)),
                        mdc_turn(s16(tp+10)),m);
          store_axis(rtcw_current.tags[i].axis,m); }
    }
    for(i=0;i<ns;i++) {
        if(!span(here,1,124))return 0;
        if(u32(here)!=7UL){err("bad MDC surface signature");return 0;}
        name64(nm,here+4);
        nc=u32(here+72);nb=u32(here+76);nsh=u32(here+80);nverts=u32(here+84);ntris=u32(here+88);
        tri=u32(here+92);sh=u32(here+96);uv=u32(here+100);base=u32(here+104);
        comp=u32(here+108);fb=u32(here+112);fc=u32(here+116);off_e=u32(here+120);
        if(nverts==0||nverts>4096||ntris>8192||nsh>256||nb==0||nb>1024||nc>1024||
           off_e<124||off_e>raw_size-here){err("bad MDC surface header");return 0;}
        len=off_e;
        if(!region(here,len,tri,ntris,12)||!region(here,len,sh,nsh,68)||
           !region(here,len,uv,nverts,8)||!region(here,len,base,nb*nverts,8)||
           !region(here,len,comp,nc*nverts,4)||!region(here,len,fb,nf,2)||
           !region(here,len,fc,nf,2))return 0;
        b_idx=u16(here+fb+frame*2);c_idx=u16(here+fc+frame*2);
        if(b_idx>=nb||(c_idx!=65535U&&c_idx>=nc)){err("MDC frame->base/compressed index invalid");return 0;}
        shader[0]=0;if(nsh)name64(shader,here+sh);
        if(!new_surface(nverts,ntris,i,nm,shader,0))return 0;
        if(!tris_at(here+tri,nverts,ntris,i))return 0;
        s=&rtcw_current.surfaces[i];
        for(t=0;t<nverts;t++){
            rtcw_vertex *v=&rtcw_current.verts[s->first_vertex+t];
            at=here+base+(b_idx*nverts+t)*8;
            v->x=s16(at);v->y=s16(at+2);v->z=s16(at+4);
            if(c_idx!=65535U){p=here+comp+(c_idx*nverts+t)*4;
                v->x+=mdc_delta_q6(raw[p]);v->y+=mdc_delta_q6(raw[p+1]);v->z+=mdc_delta_q6(raw[p+2]);}
            /* Frame localOrigin is for bounds/culling, not vertex translation. */
            st=here+uv+t*8;v->u=f32q(st,12);v->v=f32q(st+4,12);
        }
        here+=off_e;
    }
    k=0; (void)k;
    return !invalid;
}
/* CORDIC Q14: angle is uint16 turn (65536 units = full circle). */
static void trig(unsigned short angle,long *co,long *si) {
    static const int atan_tab[15]={8192,4836,2555,1297,651,326,163,81,41,20,10,5,3,1,1};
    int z=(int)angle,fl=1,i;
    long x=9949L,y=0L,nx,ny,dx,dy;
    if(z>32767)z-=65536;
    if(z>16384){z-=32768;fl=-1;} else if(z< -16384){z+=32768;fl=-1;}
    for(i=0;i<15;i++) {
        dx=y/(1L<<i);dy=x/(1L<<i);
        if(z>=0){nx=x-dx;ny=y+dy;z-=atan_tab[i];}
        else {nx=x+dx;ny=y-dy;z+=atan_tab[i];}
        x=nx;y=ny;
    }
    *co=x*fl;*si=y*fl;
}
static long mulq14(long a,long b){return (a/16384L)*b+((a%16384L)*b)/16384L;}
static long mulq12(long a,long b){return (a/4096L)*b+((a%4096L)*b)/4096L;}
static void angles_matrix(unsigned short pitch,unsigned short yaw,unsigned short roll,long m[3][3]) {
    long cp,sp,cy,sy,cr,sr;
    trig(pitch,&cp,&sp);trig(yaw,&cy,&sy);trig(roll,&cr,&sr);
    m[0][0]=mulq14(cp,cy);m[1][0]=mulq14(cp,sy);m[2][0]=-sp;
    m[0][1]=mulq14(mulq14(sr,sp),cy)-mulq14(cr,sy);
    m[1][1]=mulq14(mulq14(sr,sp),sy)+mulq14(cr,cy);
    m[2][1]=mulq14(sr,cp);
    m[0][2]=mulq14(mulq14(cr,sp),cy)+mulq14(sr,sy);
    m[1][2]=mulq14(mulq14(cr,sp),sy)-mulq14(sr,cy);
    m[2][2]=mulq14(cr,cp);
}
static int mds_decode_pose(unsigned long frame,unsigned long frame_b,unsigned int t_q12,
                           unsigned long torso_frame,unsigned long torso_frame_b,
                           unsigned int torso_t_q12,unsigned short torso_yaw) {
    unsigned long nf,nb,ns,nt,ofs_fr,ofs_b,ofs_s,ofs_tags,end,fsz,fh,here;
    unsigned long nverts,ntris,at,offs_v,offs_t,offs_cm,offs_br,off_e,nbref,
                  nw,w,b,p,vi;
    unsigned int i,k;
    char nm[65],shader[65];
    long a0,a1,a2,weight,tmp[3],sx,sy,sz;
    rtcw_surface *s;
    if(raw_size<120||u32(4)!=4UL){err("not MDS v4");return 0;}
    nf=u32(80);nb=u32(84);ofs_fr=u32(88);ofs_b=u32(92);
    ns=u32(100);ofs_s=u32(104);nt=u32(108);ofs_tags=u32(112);end=u32(116);
    if(nf==0||nf>4096||nb==0||nb>128||ns>32||nt>128||end>raw_size||
       frame>=nf||frame_b>=nf||torso_frame>=nf||torso_frame_b>=nf||
       t_q12>4096U||torso_t_q12>4096U){err("bad MDS header counts/animation frames");return 0;}
    fsz=52UL+nb*12UL;
    if(!span(ofs_fr,nf,fsz)||!span(ofs_b,nb,80)||!span(ofs_tags,nt,72))return 0;
    name64(rtcw_current.name,8);rtcw_current.frames=nf;rtcw_current.frame=frame;
    rtcw_current.bone_count=nb;rtcw_current.tag_count=nt;rtcw_current.surface_count=(unsigned int)ns;
    /* A separate module constructs the interpolated skeleton BEFORE skinning. */
    memset(&pose_in,0,sizeof(pose_in));
    pose_in.raw=raw;pose_in.size=raw_size;pose_in.frames_offset=ofs_fr;
    pose_in.frame_stride=fsz;pose_in.frames=nf;pose_in.bones=nb;
    pose_in.legs_a=frame;pose_in.legs_b=frame_b;pose_in.legs_t=t_q12;
    pose_in.torso_a=torso_frame;pose_in.torso_b=torso_frame_b;
    pose_in.torso_t=torso_t_q12;pose_in.torso_yaw=torso_yaw;
    pose_in.torso_parent=s32(96);
    fh=ofs_fr+frame*fsz;
    for(k=0;k<3;k++)pose_in.root_a[k]=f32q(fh+40+k*4UL,6);
    fh=ofs_fr+frame_b*fsz;
    for(k=0;k<3;k++)pose_in.root_b[k]=f32q(fh+40+k*4UL,6);
    for(k=0;k<3;k++) {
        if(pose_in.root_a[k]< -1000000L || pose_in.root_a[k]>1000000L ||
           pose_in.root_b[k]< -1000000L || pose_in.root_b[k]>1000000L)
           {err("MDS root offset exceeds safe range");return 0;}
    }
    if(invalid)return 0;
    for(i=0;i<nb;i++) {
        at=ofs_b+i*80UL;
        bn_parent[i]=s32(at+64);bn_distance[i]=f32q(at+72,6);
        pose_in.torso_weight[i]=f32q(at+68,12);
        pose_in.parent[i]=bn_parent[i];pose_in.distance[i]=bn_distance[i];
        pose_in.flags[i]=u32(at+76);
        name64(rtcw_current.bones[i].name,at);
        rtcw_current.bones[i].parent=bn_parent[i];
        rtcw_current.bones[i].distance_q6=bn_distance[i];
        rtcw_current.bones[i].flags=pose_in.flags[i];
    }
    if(invalid||!rtcw_pose_solve(&pose_in,&pose_out)){
        err("invalid MDS skeleton/pose interpolation");return 0;
    }
    for(i=0;i<nb;i++){
        memcpy(bn_mat[i],pose_out.axis[i],sizeof(bn_mat[i]));
        memcpy(bn_origin[i],pose_out.origin[i],sizeof(bn_origin[i]));
    }
    for(i=0;i<nt;i++) {
        at=ofs_tags+i*72UL;name64(rtcw_current.tags[i].name,at);
        rtcw_current.tags[i].bone_index=s32(at+68);
        if(rtcw_current.tags[i].bone_index<0 || rtcw_current.tags[i].bone_index>=(long)nb){err("bad MDS tag bone index");return 0;}
        { unsigned int c;unsigned long bi=(unsigned long)rtcw_current.tags[i].bone_index;
          for(c=0;c<3;c++)rtcw_current.tags[i].origin[c]=bn_origin[bi][c];
          store_axis(rtcw_current.tags[i].axis,bn_mat[bi]); }
    }
    here=ofs_s;
    for(i=0;i<ns;i++) {
        if(!span(here,1,176))return 0;
        if(u32(here)!=8UL){err("bad MDS surface signature");return 0;}
        name64(nm,here+4);name64(shader,here+68);
        nverts=u32(here+144);offs_v=u32(here+148);ntris=u32(here+152);offs_t=u32(here+156);
        offs_cm=u32(here+160);nbref=u32(here+164);offs_br=u32(here+168);off_e=u32(here+172);
        if(nverts==0||nverts>6000||ntris>8192||nbref>nb||off_e<176||off_e>raw_size-here) {
            err("bad MDS surface header");return 0;
        }
        if(!region(here,off_e,offs_t,ntris,12)||!region(here,off_e,offs_cm,nverts,4)||
           !region(here,off_e,offs_br,nbref,4)||!region(here,off_e,offs_v,1,32))return 0;
        /* Validate bone ref table, although weights store global bone indexes. */
        for(b=0;b<nbref;b++)if(u32(here+offs_br+b*4)>=nb){err("bad MDS bone reference");return 0;}
        if(!new_surface(nverts,ntris,i,nm,shader,s32(here+136)))return 0;
        if(!tris_at(here+offs_t,nverts,ntris,i))return 0;
        s=&rtcw_current.surfaces[i];
        at=here+offs_v;
        for(vi=0;vi<nverts;vi++) {
            rtcw_vertex *v=&rtcw_current.verts[s->first_vertex+vi];
            if(at>here+off_e||here+off_e-at<32UL){err("truncated MDS vertex");return 0;}
            v->u=f32q(at+12,12);v->v=f32q(at+16,12);
            nw=u32(at+20);
            if(nw==0||nw>nb || nw>(here+off_e-at-32UL)/20UL){err("bad MDS vertex weight count");return 0;}
            sx=sy=sz=0;
            { long weight_sum=0;
            for(w=0;w<nw;w++) {
                p=at+32+w*20;
                b=u32(p);weight=f32q(p+4,12);
                if(b>=nb||weight<0||weight>4097){err("bad MDS weight");return 0;}
                weight_sum+=weight;
                if(weight_sum>4200L){err("MDS vertex weights exceed normalized sum");return 0;}
                a0=f32q(p+8,6);a1=f32q(p+12,6);a2=f32q(p+16,6);
                if(a0< -1000000L||a0>1000000L||a1< -1000000L||a1>1000000L||
                   a2< -1000000L||a2>1000000L){err("MDS offset exceeds safe range");return 0;}
                /* Q14 rotation*local offset; Q12 skin weight. */
                tmp[0]=mulq14(bn_mat[b][0][0],a0)+mulq14(bn_mat[b][0][1],a1)+mulq14(bn_mat[b][0][2],a2)+bn_origin[b][0];
                tmp[1]=mulq14(bn_mat[b][1][0],a0)+mulq14(bn_mat[b][1][1],a1)+mulq14(bn_mat[b][1][2],a2)+bn_origin[b][1];
                tmp[2]=mulq14(bn_mat[b][2][0],a0)+mulq14(bn_mat[b][2][1],a1)+mulq14(bn_mat[b][2][2],a2)+bn_origin[b][2];
                sx+=mulq12(tmp[0],weight);sy+=mulq12(tmp[1],weight);sz+=mulq12(tmp[2],weight);
            }
            if(weight_sum<2048L){err("MDS vertex weights below valid sum");return 0;}
            }
            if(sx< -10000000L||sx>10000000L||sy< -10000000L||sy>10000000L||
               sz< -10000000L||sz>10000000L){err("MDS skinned vertex exceeds safe range");return 0;}
            v->x=sx;v->y=sy;v->z=sz;
            at+=32+20*nw;
        }
        here+=off_e;
    }
    /* Full LOD. Torso/legs pose is solved before skinning. */
    return !invalid;
}
int rtcw_open(const char *filename) {
    FILE *f;
    long n;
    invalid=0;raw_size=0;
    f=fopen(filename,"rb");if(!f){err("cannot open file");return 0;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);err("seek failed");return 0;}
    n=ftell(f);
    if(n<0||(unsigned long)n>RTCW_MAX_FILE||n<4){fclose(f);err("invalid size or file >32 MiB");return 0;}
    if(fseek(f,0,SEEK_SET)!=0 || fread(raw,1,(size_t)n,f)!=(size_t)n){fclose(f);err("read failed");return 0;}
    fclose(f);raw_size=(unsigned long)n;
    if(memcmp(raw,"IDP3",4)==0)memcpy(current_fmt,"MD3",4);
    else if(memcmp(raw,"IDPC",4)==0)memcpy(current_fmt,"MDC",4);
    else if(memcmp(raw,"MDSW",4)==0)memcpy(current_fmt,"MDS",4);
    else {err("unsupported signature (MD3/MDC/MDS only)");return 0;}
    strcpy(error_text,"OK");return 1;
}
int rtcw_decode(unsigned long frame) {
    if(raw_size<4){err("no file loaded");return 0;}
    memset(&rtcw_current,0,sizeof(rtcw_current));invalid=0;
    strcpy(rtcw_current.format,current_fmt);
    if(strcmp(current_fmt,"MD3")==0)return md3_decode(frame);
    if(strcmp(current_fmt,"MDC")==0)return mdc_decode(frame);
    return mds_decode_pose(frame,frame,0U,frame,frame,0U,0U);
}
int rtcw_decode_mds_pose(unsigned long legs_a,unsigned long legs_b,unsigned int legs_t,
                         unsigned long torso_a,unsigned long torso_b,unsigned int torso_t,
                         unsigned short torso_yaw) {
    if(raw_size<4 || strcmp(current_fmt,"MDS")!=0){err("MDS skeletal pose requires MDS file");return 0;}
    memset(&rtcw_current,0,sizeof(rtcw_current));invalid=0;
    strcpy(rtcw_current.format,"MDS");
    return mds_decode_pose(legs_a,legs_b,legs_t,torso_a,torso_b,torso_t,torso_yaw);
}
static void qprint(FILE *f,long v,long scale) {
    unsigned long a;
    if(v<0){fputc('-',f);a=(unsigned long)(-(v+1L))+1UL;}
    else a=(unsigned long)v;
    fprintf(f,"%lu.%05lu",a/(unsigned long)scale,((a%(unsigned long)scale)*100000UL)/(unsigned long)scale);
}
int rtcw_save_obj(const char *filename) {
    FILE *f;
    unsigned long i;
    unsigned int s;
    rtcw_vertex *v;
    rtcw_triangle *t;
    f=fopen(filename,"wb");if(!f){err("cannot create OBJ");return 0;}
    fprintf(f,"# RTCW C89 codec: %s frame %lu of %lu, Q6 fixed coordinates\n",rtcw_current.format,rtcw_current.frame,rtcw_current.frames);
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];fputs("v ",f);qprint(f,v->x,64);fputc(' ',f);qprint(f,v->y,64);fputc(' ',f);qprint(f,v->z,64);fputc('\n',f);
    }
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];fputs("vt ",f);qprint(f,v->u,4096);fputc(' ',f);qprint(f,4096-v->v,4096);fputc('\n',f);
    }
    for(s=0;s<rtcw_current.surface_count;s++) {
        fprintf(f,"\ng %s\n# original shader: %s\n",rtcw_current.surfaces[s].name,rtcw_current.surfaces[s].shader);
        for(i=rtcw_current.surfaces[s].first_triangle;
            i<rtcw_current.surfaces[s].first_triangle+rtcw_current.surfaces[s].triangles;i++) {
            t=&rtcw_current.tris[i];fprintf(f,"f %lu/%lu %lu/%lu %lu/%lu\n",t->a+1,t->a+1,t->b+1,t->b+1,t->c+1,t->c+1);
        }
    }
    if(fclose(f)!=0){err("OBJ write failed");return 0;}return 1;
}
#define WW 800
#define HH 800
static unsigned char pixels[WW*HH*3];
static void plot(int x,int y,unsigned char r,unsigned char g,unsigned char b){
    unsigned long p;
    if(x<0||x>=WW||y<0||y>=HH)return;
    p=((unsigned long)y*WW+(unsigned long)x)*3UL;
    pixels[p]=r;pixels[p+1]=g;pixels[p+2]=b;
}
static long l_abs(long v){return v<0?-v:v;}
static void line(long xa,long ya,long xb,long yb,unsigned char r,unsigned char g,unsigned char b) {
    long dx,dy,sx,sy,e,e2;
    if(xa< -10000||xa>10000||ya< -10000||ya>10000||xb< -10000||xb>10000||yb< -10000||yb>10000)return;
    dx=l_abs(xb-xa);dy=l_abs(yb-ya);sx=xa<xb?1:-1;sy=ya<yb?1:-1;e=dx-dy;
    for(;;){plot((int)xa,(int)ya,r,g,b);if(xa==xb&&ya==yb)break;
        e2=e*2;if(e2> -dy){e-=dy;xa+=sx;}if(e2<dx){e+=dx;ya+=sy;}}
}
int rtcw_save_ppm(const char *filename) {
    FILE *f;
    unsigned long i,p;
    long minx=LONG_MAX,miny=LONG_MAX,maxx=LONG_MIN,maxy=LONG_MIN,x,y,w,h,den;
    rtcw_vertex *v;
    rtcw_triangle *t;
    unsigned char r,g,b;
    if(rtcw_current.vertex_count==0){err("empty geometry");return 0;}
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];x=v->x-v->y;y=(v->x+v->y)/2-v->z;
        v->sx=x;v->sy=y;
        if(x<minx)minx=x;
        if(x>maxx)maxx=x;
        if(y<miny)miny=y;
        if(y>maxy)maxy=y;
    }
    w=maxx-minx;h=maxy-miny;den=w>h?w:h;if(den<1)den=1;
    if(den>1000000L){err("wireframe extent exceeds safe limit");return 0;}
    for(i=0;i<rtcw_current.vertex_count;i++){
        v=&rtcw_current.verts[i];v->sx=50+(v->sx-minx)*700/den+(700-w*700/den)/2;
        v->sy=50+(v->sy-miny)*700/den+(700-h*700/den)/2;
    }
    for(p=0;p<WW*HH;p++){pixels[p*3]=12;pixels[p*3+1]=16;pixels[p*3+2]=28;}
    for(i=0;i<rtcw_current.triangle_count;i++) {
        t=&rtcw_current.tris[i];
        v=&rtcw_current.verts[t->a];r=(unsigned char)(80+((t->surface*71)%150));g=(unsigned char)(110+((t->surface*43)%120));b=210;
        line(v->sx,v->sy,rtcw_current.verts[t->b].sx,rtcw_current.verts[t->b].sy,r,g,b);
        line(rtcw_current.verts[t->b].sx,rtcw_current.verts[t->b].sy,rtcw_current.verts[t->c].sx,rtcw_current.verts[t->c].sy,r,g,b);
        line(rtcw_current.verts[t->c].sx,rtcw_current.verts[t->c].sy,v->sx,v->sy,r,g,b);
    }
    f=fopen(filename,"wb");if(!f){err("cannot create PPM");return 0;}
    fprintf(f,"P6\n%d %d\n255\n",WW,HH);
    if(fwrite(pixels,1,WW*HH*3,f)!=WW*HH*3){fclose(f);err("PPM write failed");return 0;}
    if(fclose(f)!=0){err("PPM close failed");return 0;}return 1;
}
int rtcw_report(const char *filename) {
    FILE *f;
    unsigned int i;
    f=fopen(filename,"wb");if(!f){err("cannot create report");return 0;}
    fprintf(f,"model=%s\nformat=%s\nframe=%lu\nframes=%lu\nbones=%lu\ntags=%lu\nsurfaces=%u\nvertices=%lu\ntriangles=%lu\n",
        rtcw_current.name,rtcw_current.format,rtcw_current.frame,rtcw_current.frames,
        rtcw_current.bone_count,rtcw_current.tag_count,rtcw_current.surface_count,
        rtcw_current.vertex_count,rtcw_current.triangle_count);
    for(i=0;i<rtcw_current.bone_count;i++)fprintf(f,"bone[%u]=%s | parent=%ld | distQ6=%ld | flags=%lu\n",
        i,rtcw_current.bones[i].name,rtcw_current.bones[i].parent,rtcw_current.bones[i].distance_q6,rtcw_current.bones[i].flags);
    for(i=0;i<rtcw_current.tag_count;i++)fprintf(f,"tag[%u]=%s | bone=%ld | originQ6=(%ld,%ld,%ld)\n",i,rtcw_current.tags[i].name,rtcw_current.tags[i].bone_index,rtcw_current.tags[i].origin[0],rtcw_current.tags[i].origin[1],rtcw_current.tags[i].origin[2]);
    for(i=0;i<rtcw_current.tag_count;i++)fprintf(f,
        "tag_axisQ14[%u]=(%ld,%ld,%ld);(%ld,%ld,%ld);(%ld,%ld,%ld)\n",i,
        rtcw_current.tags[i].axis[0][0],rtcw_current.tags[i].axis[0][1],rtcw_current.tags[i].axis[0][2],
        rtcw_current.tags[i].axis[1][0],rtcw_current.tags[i].axis[1][1],rtcw_current.tags[i].axis[1][2],
        rtcw_current.tags[i].axis[2][0],rtcw_current.tags[i].axis[2][1],rtcw_current.tags[i].axis[2][2]);
    for(i=0;i<rtcw_current.surface_count;i++)fprintf(f,"surface[%u]=%s | shader=%s | verts=%lu | tris=%lu | minLOD=%ld\n",
        i,rtcw_current.surfaces[i].name,rtcw_current.surfaces[i].shader,
        rtcw_current.surfaces[i].vertices,rtcw_current.surfaces[i].triangles,rtcw_current.surfaces[i].min_lod);
    if(fclose(f)!=0){err("report write failed");return 0;}return 1;
}
