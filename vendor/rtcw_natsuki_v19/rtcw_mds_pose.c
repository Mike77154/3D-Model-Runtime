/* MDS bone-space interpolation and split torso/legs evaluation. CC0-1.0.
   Independent source implementation guided by the public RTCW file layout.
   All calculations signed integer fixed-point; no heap, no float/double. */
#include "rtcw_mds_pose.h"
#include <string.h>

static unsigned int rd16(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
static long mul_fixed(long a,long b,long scale) {
    return (a/scale)*b + ((a%scale)*b)/scale;
}
static long lerp(long a,long b,unsigned int t) {
    /* Applies to Q6 root coordinates and other bounded values. */
    if(t==0U)return a;
    if(t==4096U)return b;
    return a+mul_fixed(b-a,(long)t,4096L);
}
/* Angles are modular 16-bit turns. Interpolate via shortest signed arc. */
static unsigned short lerp_turn(unsigned short a,unsigned short b,unsigned int t) {
    unsigned int d=((unsigned int)b-(unsigned int)a)&65535U;
    long delta=(d>32767U)?(long)d-65536L:(long)d;
    long out=(long)a+(delta*(long)t)/4096L;
    return (unsigned short)(((unsigned long)out)&65535UL);
}
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
static void angles_matrix(unsigned short pitch,unsigned short yaw,unsigned short roll,long m[3][3]) {
    long cp,sp,cy,sy,cr,sr;
    trig(pitch,&cp,&sp);trig(yaw,&cy,&sy);trig(roll,&cr,&sr);
    m[0][0]=mul_fixed(cp,cy,16384L);m[1][0]=mul_fixed(cp,sy,16384L);m[2][0]=-sp;
    m[0][1]=mul_fixed(mul_fixed(sr,sp,16384L),cy,16384L)-mul_fixed(cr,sy,16384L);
    m[1][1]=mul_fixed(mul_fixed(sr,sp,16384L),sy,16384L)+mul_fixed(cr,cy,16384L);
    m[2][1]=mul_fixed(sr,cp,16384L);
    m[0][2]=mul_fixed(mul_fixed(cr,sp,16384L),cy,16384L)+mul_fixed(sr,sy,16384L);
    m[1][2]=mul_fixed(mul_fixed(cr,sp,16384L),sy,16384L)-mul_fixed(sr,cy,16384L);
    m[2][2]=mul_fixed(cr,cp,16384L);
}
static unsigned long isqrt32(unsigned long n) {
    unsigned long res=0,bit=1UL<<30;
    while(bit>n)bit>>=2;
    while(bit) {
        if(n>=res+bit) {n-=res+bit;res=(res>>1)+bit;}
        else res>>=1;
        bit>>=2;
    }
    return res;
}
static void normalize_q14(long v[3]) {
    unsigned long n,mag;
    n=(unsigned long)(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    mag=isqrt32(n);
    if(!mag)return;
    v[0]=mul_fixed(v[0],16384L,(long)mag);
    v[1]=mul_fixed(v[1],16384L,(long)mag);
    v[2]=mul_fixed(v[2],16384L,(long)mag);
}
static unsigned short bone_angle(const rtcw_pose_input *in,unsigned long frame,unsigned long bone,
                                 unsigned int component) {
    unsigned long offset=in->frames_offset+frame*in->frame_stride+52UL+bone*12UL+(unsigned long)component*2UL;
    return (unsigned short)rd16(in->raw+offset);
}
static unsigned short blended_angle(const rtcw_pose_input *in,unsigned long bone,unsigned int component,int torso) {
    unsigned short a,b;
    unsigned long fa=torso?in->torso_a:in->legs_a;
    unsigned long fb=torso?in->torso_b:in->legs_b;
    unsigned int t=torso?in->torso_t:in->legs_t;
    a=bone_angle(in,fa,bone,component);
    b=bone_angle(in,fb,bone,component);
    return lerp_turn(a,b,t);
}
/* Direction vectors are interpolated, then normalized, as in MDS' frame
   interpolation. Interpolating pitch/yaw independently is not equivalent. */
static void offset_dir(const rtcw_pose_input *in,unsigned long frame,unsigned long bone,long dir[3]) {
    unsigned short pitch=bone_angle(in,frame,bone,4);
    unsigned short yaw=bone_angle(in,frame,bone,5);
    long cp,sp,cy,sy;
    trig(pitch,&cp,&sp);trig(yaw,&cy,&sy);
    dir[0]=mul_fixed(cp,cy,16384L);
    dir[1]=mul_fixed(cp,sy,16384L);
    dir[2]=-sp;
}
static void lerped_dir(const rtcw_pose_input *in,unsigned long bone,int torso,long dir[3]) {
    unsigned long a=torso?in->torso_a:in->legs_a;
    unsigned long b=torso?in->torso_b:in->legs_b;
    unsigned int t=torso?in->torso_t:in->legs_t;
    long v[3];unsigned int j;
    offset_dir(in,a,bone,dir);
    if(a==b||t==0)return;
    offset_dir(in,b,bone,v);
    for(j=0;j<3;j++)dir[j]=lerp(dir[j],v[j],t);
    normalize_q14(dir);
}
static int calc_bone(unsigned long idx,const rtcw_pose_input *in,rtcw_pose_output *out,
                     unsigned char state[RTCW_MAX_BONES],unsigned int depth) {
    long *translation,angles[3],ang_t[3],direction[3],dir_t[3];
    long w,dist,px,py,pz;
    long par;
    unsigned int j;
    if(depth>RTCW_MAX_BONES || state[idx]==1)return 0;
    if(state[idx]==2)return 1;
    state[idx]=1;
    par=in->parent[idx];
    if(par>=0 && !calc_bone((unsigned long)par,in,out,state,depth+1))return 0;
    w=in->torso_weight[idx];
    for(j=0;j<3;j++) {
        angles[j]=blended_angle(in,idx,j,0);
        ang_t[j]=blended_angle(in,idx,j,1);
    }
    for(j=0;j<3;j++)
        angles[j]=lerp_turn((unsigned short)angles[j],(unsigned short)ang_t[j],(unsigned int)w);
    angles_matrix((unsigned short)angles[0],(unsigned short)angles[1],(unsigned short)angles[2],out->axis[idx]);
    translation=out->origin[idx];
    if(par<0) {
        for(j=0;j<3;j++)translation[j]=lerp(in->root_a[j],in->root_b[j],in->legs_t);
    } else {
        /* MDS offset pitch/yaw are shorts [4],[5]. Apply normalised
           direction interpolation within legs and torso before mixing. */
        lerped_dir(in,idx,0,direction);
        if(w==4096L)lerped_dir(in,idx,1,direction);
        else if(w!=0) {
            lerped_dir(in,idx,1,dir_t);
            for(j=0;j<3;j++)direction[j]=lerp(direction[j],dir_t[j],(unsigned int)w);
            normalize_q14(direction);
        }
        dist=in->distance[idx];
        px=mul_fixed(dist,direction[0],16384L);
        py=mul_fixed(dist,direction[1],16384L);
        pz=mul_fixed(dist,direction[2],16384L);
        translation[0]=out->origin[par][0]+px;
        translation[1]=out->origin[par][1]+py;
        translation[2]=out->origin[par][2]+pz;
    }
    for(j=0;j<3;j++)if(translation[j]< -10000000L||translation[j]>10000000L)return 0;
    state[idx]=2;
    return 1;
}
int rtcw_pose_solve(const rtcw_pose_input *in,rtcw_pose_output *out) {
    unsigned char state[RTCW_MAX_BONES];
    unsigned long i;
    long c,s,dx,dy,wx,wy,pivot[3],weight;
    unsigned int j;
    if(!in || !out || !in->raw || in->bones==0 || in->bones>RTCW_MAX_BONES ||
       in->frames==0 || in->legs_t>4096U || in->torso_t>4096U ||
       in->legs_a>=in->frames || in->legs_b>=in->frames ||
       in->torso_a>=in->frames || in->torso_b>=in->frames ||
       in->torso_parent< -1 || in->torso_parent>=(long)in->bones ||
       in->frames_offset>in->size ||
       in->frame_stride>(in->size-in->frames_offset)/in->frames)return 0;
    for(i=0;i<in->bones;i++){
        if(in->parent[i]< -1 || in->parent[i]>=(long)in->bones ||
           in->torso_weight[i]<0 || in->torso_weight[i]>4096L ||
           in->distance[i]< -1000000L || in->distance[i]>1000000L)return 0;
    }
    memset(state,0,sizeof(state));
    for(i=0;i<in->bones;i++)if(!calc_bone(i,in,out,state,0))return 0;
    if(in->torso_yaw && in->torso_parent>=0){
        for(j=0;j<3;j++)pivot[j]=out->origin[in->torso_parent][j];
        for(i=0;i<in->bones;i++) {
            weight=in->torso_weight[i];
            if(weight==0)continue;
            trig(in->torso_yaw,&c,&s);
            c=16384L+mul_fixed(c-16384L,weight,4096L);
            s=mul_fixed(s,weight,4096L);
            dx=out->origin[i][0]-pivot[0];dy=out->origin[i][1]-pivot[1];
            wx=mul_fixed(c,dx,16384L)-mul_fixed(s,dy,16384L);
            wy=mul_fixed(s,dx,16384L)+mul_fixed(c,dy,16384L);
            out->origin[i][0]=pivot[0]+wx;
            out->origin[i][1]=pivot[1]+wy;
            if(out->origin[i][0]< -10000000L||out->origin[i][0]>10000000L ||
               out->origin[i][1]< -10000000L||out->origin[i][1]>10000000L)return 0;
            for(j=0;j<3;j++) {
                dx=out->axis[i][0][j];dy=out->axis[i][1][j];
                out->axis[i][0][j]=mul_fixed(c,dx,16384L)-mul_fixed(s,dy,16384L);
                out->axis[i][1][j]=mul_fixed(s,dx,16384L)+mul_fixed(c,dy,16384L);
            }
        }
    }
    return 1;
}
