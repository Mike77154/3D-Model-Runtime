#include "c4d89.h"
#include <string.h>
#include <limits.h>
/* Strict C89, fixed point, no dynamic allocation. Large scratch tables are
   static so Win32 stacks remain small. Invocation is non-reentrant. */
#define C4D89_MAX_CORNERS (C4D89_MAX_MESH_FACES*4ul)
static long xyz[C4D89_MAX_MESH_VERTICES][3];
static C4D89_Face poly[C4D89_MAX_MESH_FACES];
static short fnormal[C4D89_MAX_MESH_FACES][3];
static unsigned long head[C4D89_MAX_MESH_VERTICES];
static unsigned long nextcorner[C4D89_MAX_CORNERS];
static unsigned int face_region[C4D89_MAX_MESH_FACES];
static unsigned long npoints,nfaces;

static unsigned long umax3(unsigned long a,unsigned long b,unsigned long c)
{ unsigned long v=a>b?a:b;return v>c?v:c; }
static unsigned long uabs(long n)
{ return n<0L?(unsigned long)(-(n+1L))+1ul:(unsigned long)n; }
static unsigned long isqrt32(unsigned long n)
{
    unsigned long op=n, res=0ul, bit=1ul<<30;
    while(bit>op)bit>>=2;
    while(bit){
        if(op>=res+bit){op-=res+bit;res=(res>>1)+bit;}
        else res>>=1;
        bit>>=2;
    }
    return res;
}
static void normal3(long vx,long vy,long vz,short out[3])
{
    unsigned long max,mag;
    long x,y,z;
    max=umax3(uabs(vx),uabs(vy),uabs(vz));
    if(max==0ul){out[0]=0;out[1]=0;out[2]=1024;return;}
    /* Cap before multiplying: sum of <=16384 face vectors can exceed
       signed 32-bit if multiplied by 256 without this downscale. */
    while(max>4096ul){vx/=2L;vy/=2L;vz/=2L;max=umax3(uabs(vx),uabs(vy),uabs(vz));}
    if(max==0ul)max=1ul;
    x=(vx*256L)/(long)max;y=(vy*256L)/(long)max;
    z=(vz*256L)/(long)max;
    mag=isqrt32((unsigned long)(x*x+y*y+z*z));
    if(mag==0ul)mag=1ul;
    out[0]=(short)((x*1024L)/(long)mag);
    out[1]=(short)((y*1024L)/(long)mag);
    out[2]=(short)((z*1024L)/(long)mag);
}
static void make_normal(const C4D89_Face *f,short out[3])
{
    unsigned int ia=f->a,ib=f->b,ic=f->c,k;
    long a[3],b[3],m,e[3];
    /* /1024 avoids 32-bit overflow on subtraction of Q16.16 coordinates. */
    for(k=0u;k<3u;k++){
        a[k]=xyz[ib][k]/1024L-xyz[ia][k]/1024L;
        b[k]=xyz[ic][k]/1024L-xyz[ia][k]/1024L;
    }
    m=(long)umax3(uabs(a[0]),uabs(a[1]),uabs(a[2]));
    if(m<1L)m=1L;
    for(k=0u;k<3u;k++)a[k]=(a[k]*128L)/m;
    m=(long)umax3(uabs(b[0]),uabs(b[1]),uabs(b[2]));
    if(m<1L)m=1L;
    for(k=0u;k<3u;k++)b[k]=(b[k]*128L)/m;
    e[0]=a[1]*b[2]-a[2]*b[1];
    e[1]=a[2]*b[0]-a[0]*b[2];
    e[2]=a[0]*b[1]-a[1]*b[0];
    normal3(e[0],e[1],e[2],out);
}
static int cbvertex(void*u,unsigned long i,long x,long y,long z)
{ (void)u;if(i>=C4D89_MAX_MESH_VERTICES)return 0;xyz[i][0]=x;xyz[i][1]=y;xyz[i][2]=z;npoints=i+1ul;return 1; }
static int cbface(void*u,unsigned long i,const C4D89_Face *f)
{ (void)u;if(i>=C4D89_MAX_MESH_FACES)return 0;poly[i]=*f;nfaces=i+1ul;return 1; }
static int same_group(unsigned long a,unsigned long b)
{
    /* A different group preserves hard edge at material/selections boundary. */
    return face_region[a]==face_region[b];
}
int c4d89_emit_smooth_faces(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_PolyRegions *regions,
    int cosine_q10,C4D89_SmoothFaceFn fn,void *user)
{
    C4D89_MeshSink sink;
    unsigned long i,link,fc;
    unsigned int j,v,ids[2],ct;
    int rc;
    if(!path||!mesh||!fn||cosine_q10<0||cosine_q10>1024)return C4D89_ERR_CORRUPT;
    if(mesh->vertex_count>C4D89_MAX_MESH_VERTICES||
       mesh->face_count>C4D89_MAX_MESH_FACES)return C4D89_ERR_LIMIT;
    npoints=nfaces=0ul;memset(&sink,0,sizeof(sink));
    sink.vertex=cbvertex;sink.face=cbface;
    rc=c4d89_stream_mesh(path,mesh_number,mesh,&sink,(void*)0);
    if(rc)return rc;
    for(i=0ul;i<npoints;i++)head[i]=C4D89_MAX_CORNERS;
    for(i=0ul;i<nfaces;i++){
        unsigned int vs[4];
        make_normal(&poly[i],fnormal[i]);
        ct=0u;
        if(regions)ct=c4d89_find_face_regions(regions,mesh_number,i,ids,2u);
        face_region[i]=(ct==1u)?ids[0]+1u:(ct==0u?0u:65535u);
        vs[0]=poly[i].a;vs[1]=poly[i].b;vs[2]=poly[i].c;vs[3]=poly[i].d;
        for(j=0u;j<poly[i].corners;j++){
            link=i*4ul+(unsigned long)j;
            v=vs[j];nextcorner[link]=head[v];head[v]=link;
        }
    }
    for(i=0ul;i<nfaces;i++){
        short normals[4][3];
        unsigned int vs[4];
        vs[0]=poly[i].a;vs[1]=poly[i].b;vs[2]=poly[i].c;vs[3]=poly[i].d;
        for(j=0u;j<poly[i].corners;j++){
            long sx=0,sy=0,sz=0,dot;
            unsigned int count=0u;
            v=vs[j];
            for(link=head[v];link!=C4D89_MAX_CORNERS;link=nextcorner[link]){
                fc=link/4ul;
                if(!same_group(i,fc))continue;
                dot=(long)fnormal[i][0]*(long)fnormal[fc][0]+
                    (long)fnormal[i][1]*(long)fnormal[fc][1]+
                    (long)fnormal[i][2]*(long)fnormal[fc][2];
                if(dot < (long)cosine_q10*1024L)continue;
                sx+=fnormal[fc][0];sy+=fnormal[fc][1];sz+=fnormal[fc][2];
                count++;
                if(count>=16384u)break;
            }
            if(!count){normals[j][0]=fnormal[i][0];normals[j][1]=fnormal[i][1];normals[j][2]=fnormal[i][2];}
            else normal3(sx,sy,sz,normals[j]);
        }
        if(poly[i].corners==3u){
            normals[3][0]=normals[2][0];normals[3][1]=normals[2][1];normals[3][2]=normals[2][2];
        }
        if(!fn(user,i,&poly[i],normals))return C4D89_ERR_SINK;
    }
    return C4D89_OK;
}
