/* Gun Survivor PC NAB native resource decoder v19 (C89, white-room).
 * Ghidra 12.0.4 crosscheck: FUN_00411630 reads initial 36-byte vertices,
 * 0x160 object descriptor, 0x18 draw-group descriptors, 0x3C secondary
 * vertices, 0x14 faces, 0x60 resource blocks and 0x104 coordinates + parent.
 * No malloc / calloc / realloc / free / float / double / stdin.
 * Usage: nab_native_c89_v16 ENEM22.NAB output_prefix
 * Group descriptor ref + 6th original DWORD is candidate local texture-plane ID.
 * Writes .parts.csv, .coords.csv, .tga, .mtl, .obj (world/bind assembled).
 * No animation claims. OBJ geometry in assembled bind pose (C89 fixed point).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_PARTS 2048UL
#define MAX_COORDS 2048UL
#define MAX_VERTS 2000000UL
#define MAX_BYTES 104857600UL
#define MAX_TEX 2097152UL

typedef struct DrawPart {
    unsigned long first, count, joint, texture_id, texture_descriptor, texture_plane;
} DrawPart;
typedef struct Coord {
    long r[16],t[16];
    unsigned long parent;
} Coord;
static Coord g_coords[MAX_COORDS+1UL];
static unsigned long g_ncoords=0UL;
static DrawPart g_parts[MAX_PARTS];
static unsigned long g_plane_base[8],g_plane_count[8];

static unsigned long read32(FILE *f,int *ok) {
    int a,b,c,d;
    unsigned long v;
    a=fgetc(f);b=fgetc(f);c=fgetc(f);d=fgetc(f);
    if(a==EOF||b==EOF||c==EOF||d==EOF){*ok=0;return 0UL;}
    v=(unsigned long)a | ((unsigned long)b<<8) | ((unsigned long)c<<16) | ((unsigned long)d<<24);
    return v;
}
static long decode_q16(unsigned long bits,int *ok) {
    unsigned long exp,mant,value,mag;
    int shift,neg;
    neg=((bits>>31)&1UL)!=0UL;
    mag=bits&0x7fffffffUL;
    exp=(mag>>23)&255UL;
    mant=mag&0x7fffffUL;
    if(exp==255UL){*ok=0;return 0L;}
    if(exp==0UL)return 0L;
    mant|=0x800000UL;
    shift=(int)exp-134;
    if(shift>=0) {
        if(shift>7||mant>((unsigned long)LONG_MAX>>shift)) {*ok=0;return 0L;}
        value=mant<<shift;
    } else {
        if(shift<=-32)return 0L;
        value=(mant+(1UL<<(-shift-1)))>>(-shift);
    }
    if(value>(unsigned long)LONG_MAX){*ok=0;return 0L;}
    return neg?-(long)value:(long)value;
}
static void print_q(FILE *f,long q) {
    unsigned long v,whole,dec;
    if(q<0){fputc('-',f);v=(unsigned long)(-(q+1L))+1UL;}
    else v=(unsigned long)q;
    whole=v>>16;
    dec=(((v&65535UL)*10000UL)+32768UL)>>16;
    if(dec==10000UL){whole++;dec=0;}
    fprintf(f,"%lu.%04lu",whole,dec);
}
static int skip(FILE *f,unsigned long bytes,unsigned long maxfile) {
    long cur=ftell(f);
    if(cur<0||bytes>MAX_BYTES||(unsigned long)cur>maxfile||bytes>maxfile-(unsigned long)cur)return 0;
    return fseek(f,(long)bytes,SEEK_CUR)==0;
}
static int write_texture(FILE *f,const char *name,unsigned long sz,unsigned long w,unsigned long h,int *ok){
    FILE *out;
    unsigned long i,pixels,v;
    unsigned char header[18];
    int a,b;
    if(w==0UL||h==0UL||w>2048UL||h>2048UL||w>MAX_TEX/h)return 0;
    pixels=w*h;
    if(sz!=pixels*2UL)return 0;
    out=fopen(name,"wb");if(!out)return 0;
    memset(header,0,sizeof(header));
    header[2]=2; /* uncompressed truecolor TGA */
    header[12]=(unsigned char)(w&255UL);header[13]=(unsigned char)(w>>8);
    header[14]=(unsigned char)(h&255UL);header[15]=(unsigned char)(h>>8);
    header[16]=32;header[17]=0x28; /* top-left origin and 8-bit alpha */
    if(fwrite(header,1UL,18UL,out)!=18UL){fclose(out);return 0;}
    for(i=0;i<pixels;i++){
        unsigned char rgba[4];
        a=fgetc(f);b=fgetc(f);
        if(a==EOF||b==EOF){*ok=0;break;}
        v=(unsigned long)a|((unsigned long)b<<8);
        rgba[0]=(unsigned char)(((v&31UL)*255UL+15UL)/31UL);
        rgba[1]=(unsigned char)((((v>>5)&31UL)*255UL+15UL)/31UL);
        rgba[2]=(unsigned char)((((v>>10)&31UL)*255UL+15UL)/31UL);
        rgba[3]=(v&32768UL)?255U:0U;
        if(fwrite(rgba,1UL,4UL,out)!=4UL){*ok=0;break;}
    }
    fclose(out);
    return *ok;
}

/* Fixed-point (no heap, no floating point). Positions Q8, matrices Q12.
   Convert float32 bit patterns using decode_q16; avoid 64-bit integers. */
static int add_safe(long a,long b,long *out){
    if((b>0L&&a>LONG_MAX-b)||(b<0L&&a<LONG_MIN-b))return 0;
    *out=a+b;return 1;
}
static int mulq12(long a,long b,long *out){
    long whole,rem,part,tmp;
    /* All native matrix coefficients and positions have bounded magnitudes. */
    if(b<-16384L||b>16384L)return 0;
    whole=a/4096L;rem=a%4096L;
    if(whole>LONG_MAX/16384L||whole<LONG_MIN/16384L)return 0;
    part=whole*b;
    /* |rem*b| <= 4095*16384 = 67092480, safe in C89 long */
    tmp=(rem*b)/4096L;
    return add_safe(part,tmp,out);
}
static int apply_mat(long p[3],const long mat[16],int translate){
    long out[3],v,tmp,sum;
    unsigned long r,c;
    for(c=0UL;c<3UL;c++){
        sum=0L;
        for(r=0UL;r<3UL;r++){
            if(!mulq12(p[r],mat[r*4UL+c],&v)||!add_safe(sum,v,&tmp))return 0;
            sum=tmp;
        }
        if(translate){
            if(!add_safe(sum,mat[12UL+c]/16L,&tmp))return 0;
            sum=tmp;
        }
        out[c]=sum;
    }
    for(c=0UL;c<3UL;c++)p[c]=out[c];
    return 1;
}
static int walk_to_world(long p[3],unsigned long node,int is_normal){
    unsigned long depth=0UL,i;
    if(node>g_ncoords)return 0;
    for(;;){
        Coord *c=&g_coords[node];
        if(!apply_mat(p,c->r,0)||!apply_mat(p,c->t,!is_normal))return 0;
        if(node==0UL||c->parent==node)break;
        node=c->parent;
        if(++depth>g_ncoords+1UL)return 0;
    }
    /* Source (x,y,z) -> OBJ y-up right-handed (z,-y,-x). */
    i=0UL; (void)i;
    {long x=p[0],y=p[1],z=p[2];
     if(x==LONG_MIN||y==LONG_MIN)return 0;
     p[0]=z;p[1]=-y;p[2]=-x;}
    return 1;
}
/* Print original model unit scaled to meters: input is q8, denom=256000.
   Decimal digits via repeated *10, no 64-bit arithmetic. */
static void print_meter(FILE *out,long q){
    unsigned long u,whole,rem,i;
    if(q<0L){fputc('-',out);u=(unsigned long)(-(q+1L))+1UL;}
    else u=(unsigned long)q;
    whole=u/256000UL;rem=u%256000UL;
    fprintf(out,"%lu.",whole);
    for(i=0UL;i<6UL;i++){rem*=10UL;fputc((int)('0'+(rem/256000UL)),out);rem%=256000UL;}
}
/* Print normal normalized by the original 4096-unit magnitude.
   input q8 normal after transforms: denom 4096*256 = 1048576. */
static void print_normal(FILE *out,long q){
    unsigned long u,whole,rem,i;
    if(q<0L){fputc('-',out);u=(unsigned long)(-(q+1L))+1UL;}
    else u=(unsigned long)q;
    whole=u/1048576UL;rem=u%1048576UL;
    fprintf(out,"%lu.",whole);
    for(i=0UL;i<6UL;i++){rem*=10UL;fputc((int)('0'+(rem/1048576UL)),out);rem%=1048576UL;}
}

static int texture_header_at(FILE *f,long at,unsigned long maxfile) {
    int good=1,valid=0;
    unsigned long sz,fmt,w,h;
    long save=ftell(f);
    if(at<0||(unsigned long)at>maxfile||maxfile-(unsigned long)at<16UL)return 0;
    if(fseek(f,at,SEEK_SET)!=0)return 0;
    sz=read32(f,&good);fmt=read32(f,&good);w=read32(f,&good);h=read32(f,&good);
    if(good&&fmt==25UL&&w>0UL&&h>0UL&&w<=2048UL&&h<=2048UL&&w<=MAX_TEX/h&&sz==w*h*2UL&&sz<=maxfile-(unsigned long)at-16UL)valid=1;
    (void)fseek(f,save,SEEK_SET);
    return valid;
}

static unsigned long group_for_vertex(unsigned long v,unsigned long np){
    unsigned long i,t=v/3UL;
    for(i=0UL;i<np;i++)if(t>=g_parts[i].first&&t<g_parts[i].first+g_parts[i].count)return i;
    return np;
}
static void write_obj(FILE *f,FILE *out,unsigned long nv,unsigned long np,int *ok,const char *mtl_basename){
    unsigned long i,j,k,idx,fbits[9],part;
    long p[3];
    if(fseek(f,8L,SEEK_SET)!=0){*ok=0;return;}
    fprintf(out,"# Gun Survivor NAB; pose de repos assembled via 0x108 native matrices, C89 fixedpoint\nmtllib %s.mtl\n",mtl_basename);
    for(i=0UL;i<nv && *ok;i++) {
        for(j=0UL;j<9UL;j++)fbits[j]=read32(f,ok);
        part=group_for_vertex(i,np);
        if(!*ok||part==np){*ok=0;break;}
        for(j=0UL;j<3UL;j++)p[j]=decode_q16(fbits[j],ok)/256L;
        if(!*ok||!walk_to_world(p,g_parts[part].joint,0)){*ok=0;break;}
        fputs("v ",out);
        for(j=0UL;j<3UL;j++){print_meter(out,p[j]);fputc(j==2UL?'\n':' ',out);}
    }
    if(fseek(f,8L,SEEK_SET)!=0){*ok=0;return;}
    for(i=0UL;i<nv && *ok;i++) {
        for(j=0UL;j<9UL;j++)fbits[j]=read32(f,ok);
        if(!*ok)break;
        fputs("vt ",out);
        print_q(out,decode_q16(fbits[7],ok));fputc(' ',out);
        /* OBJ texture V axis points upwards; source data is top-left. */
        print_q(out,65536L-decode_q16(fbits[8],ok));fputc('\n',out);
    }
    if(fseek(f,8L,SEEK_SET)!=0){*ok=0;return;}
    for(i=0UL;i<nv && *ok;i++) {
        for(j=0UL;j<9UL;j++)fbits[j]=read32(f,ok);
        part=group_for_vertex(i,np);
        if(!*ok||part==np){*ok=0;break;}
        for(j=0UL;j<3UL;j++)p[j]=decode_q16(fbits[j+3UL],ok)/256L;
        if(!*ok||!walk_to_world(p,g_parts[part].joint,1)){*ok=0;break;}
        fputs("vn ",out);
        for(j=0UL;j<3UL;j++){print_normal(out,p[j]);fputc(j==2UL?'\n':' ',out);}
    }
    for(i=0UL;i<np && *ok;i++){
        fprintf(out,"o part_%02lu_joint_%02lu\n",i,g_parts[i].joint);
        if(g_parts[i].texture_id==0xffffffffUL)fputs("usemtl no_texture\n",out);
        else fprintf(out,"usemtl tex_%02lu\n",g_parts[i].texture_id);
        for(j=0UL;j<g_parts[i].count;j++){
            idx=(g_parts[i].first+j)*3UL+1UL;
            fputs("f ",out);
            /* Source triangle indices follow original order. Pose transform uses axis reflection,
               therefore winding reversed to preserve front faces for OBJ right-handed coordinates. */
            for(k=0UL;k<3UL;k++){
                unsigned long v=idx+(k==1UL?2UL:(k==2UL?1UL:0UL));
                fprintf(out,"%lu/%lu/%lu%c",v,v,v,k==2UL?'\n':' ');
            }
        }
    }
}
static void write_mtl(FILE *f,const char *prefixbase,unsigned long ntex){
    unsigned long i;
    fputs("# Gun Survivor D3D A1R5G5B5 textures to TGA; alpha bit retained\n",f);
    for(i=0UL;i<ntex;i++){
        fprintf(f,"newmtl tex_%02lu\nKa 1 1 1\nKd 1 1 1\nd 1\nmap_Kd %s_tex%02lu.tga\n\n",i,prefixbase,i);
    }
    fputs("newmtl no_texture\nKd 0.4 0.5 0.6\n",f);
}

int main(int argc,char **argv){
    FILE *in,*parts,*coords,*obj,*mtl;
    char name[1024]; const char *base;
    unsigned long nv,nf,objs,i,j,sz=0UL,fmt=0UL,w=0UL,h=0UL,np,total,nblocks,ncoords,ref,assoc;
    unsigned long nd=0UL,nt=0UL;
    unsigned long tval[65],pval,maxfile,material_id;
    int ok=1;
    if(argc!=3){fputs("Usage: nab_native_c89 input.NAB output_prefix\n",stderr);return 2;}
    if(strlen(argv[2])>900){fputs("output prefix too long\n",stderr);return 2;}
    in=fopen(argv[1],"rb");if(!in){perror("input");return 2;}
    if(fseek(in,0,SEEK_END)!=0){fclose(in);return 2;}
    maxfile=(unsigned long)ftell(in);rewind(in);
    nv=read32(in,&ok);nf=read32(in,&ok);
    if(!ok||nf==0UL||nf>MAX_VERTS/3UL||nv!=nf*3UL||nv>MAX_VERTS||maxfile>MAX_BYTES||!skip(in,nv*36UL,maxfile))goto fail;
    objs=read32(in,&ok);
    if(!ok||objs<1UL||objs>8UL)goto fail;
    /* Descriptor count and image-plane count are not necessarily equal.
       Example: ENEM33 has 2 descriptors and 17 texture planes. */
    for(;;) {
        long at=ftell(in);
        if(at<0)goto fail;
        if(texture_header_at(in,at,maxfile)) {
            sz=read32(in,&ok);fmt=read32(in,&ok);w=read32(in,&ok);h=read32(in,&ok);
            if(!ok||nt>=128UL||fmt!=25UL||w==0UL||h==0UL||w>MAX_TEX/h||w*h>MAX_TEX||sz!=w*h*2UL)goto fail;
            sprintf(name,"%s_tex%02lu.tga",argv[2],nt);
            if(!write_texture(in,name,sz,w,h,&ok))goto fail;
            if(nd==0UL||nd>8UL)goto fail;
            g_plane_count[nd-1UL]++;
            ++nt;
        }
        else if(nd<objs && at<=LONG_MAX-0x160L && texture_header_at(in,at+0x160L,maxfile)) {
            if(!skip(in,0x160UL,maxfile))goto fail;
            g_plane_base[nd]=nt;
            ++nd;
        }
        else break;
    }
    if(nd!=objs||nt==0UL)goto fail;
    np=read32(in,&ok);
    if(!ok||np==0UL||np>MAX_PARTS)goto fail;
    sprintf(name,"%s.parts.csv",argv[2]);parts=fopen(name,"wb");if(!parts)goto fail;
    fputs("part,first_triangle,triangle_count,joint,resource_ref,primitive_ptr,primitive_count,texture_descriptor,texture_plane,texture_global_id\n",parts);
    total=0UL;
    for(i=0;i<np && ok;i++){
        unsigned long fields[6];
        for(j=0;j<6;j++)fields[j]=read32(in,&ok);
        ref=fields[0]!=0UL?read32(in,&ok):0UL;
        assoc=read32(in,&ok);
        g_parts[i].first=fields[2];g_parts[i].count=fields[3];g_parts[i].joint=assoc;
        g_parts[i].texture_descriptor=ref;
        g_parts[i].texture_plane=fields[5];
        material_id=0xffffffffUL;
        if(fields[0]!=0UL) {
            if(ref>=nd||fields[5]>=g_plane_count[ref])ok=0;
            else material_id=g_plane_base[ref]+fields[5];
        }
        g_parts[i].texture_id=material_id;
        if(fields[2]!=total||fields[3]>nf-total)ok=0;
        if(!ok)break;
        total+=fields[3];
        fprintf(parts,"%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",i,fields[2],fields[3],assoc,ref,fields[0],fields[1],ref,fields[5],material_id);
    }
    fclose(parts);
    if(!ok||total!=nf)goto fail;
    if(!skip(in,nv*60UL+nf*20UL,maxfile))goto fail;
    nblocks=read32(in,&ok);
    if(!ok||nblocks>MAX_COORDS)goto fail;
    for(i=0;i<nblocks;i++){
        unsigned long nref=0UL;
        for(j=0;j<24;j++){unsigned long word=read32(in,&ok);if(j==3)nref=word;}
        if(!ok||nref>5000UL||!skip(in,nref*8UL,maxfile))goto fail;
    }
    for(i=0;i<6;i++){ (void)read32(in,&ok); }
    ncoords=read32(in,&ok);
    if(!ok||ncoords!=nblocks||ncoords>MAX_COORDS)goto fail;
    sprintf(name,"%s.coords.csv",argv[2]);coords=fopen(name,"wb");if(!coords)goto fail;
    fputs("joint,parent,tx,ty,tz,r00,r01,r02,r10,r11,r12,r20,r21,r22\n",coords);
    for(i=0;i<=ncoords && ok;i++){
        long q[12];
        for(j=0;j<65;j++)tval[j]=read32(in,&ok);
        pval=read32(in,&ok);
        if(!ok||pval>ncoords){ok=0;break;}
        q[0]=decode_q16(tval[44],&ok);q[1]=decode_q16(tval[45],&ok);q[2]=decode_q16(tval[46],&ok);
        q[3]=decode_q16(tval[0],&ok);q[4]=decode_q16(tval[1],&ok);q[5]=decode_q16(tval[2],&ok);
        q[6]=decode_q16(tval[4],&ok);q[7]=decode_q16(tval[5],&ok);q[8]=decode_q16(tval[6],&ok);
        q[9]=decode_q16(tval[8],&ok);q[10]=decode_q16(tval[9],&ok);q[11]=decode_q16(tval[10],&ok);
        g_coords[i].parent=pval;
        for(j=0UL;j<16UL;j++){
            g_coords[i].r[j]=decode_q16(tval[j],&ok)/16L;
            g_coords[i].t[j]=decode_q16(tval[32UL+j],&ok)/16L;
        }
        fprintf(coords,"%lu,%lu",i,pval);
        for(j=0;j<12;j++){fputc(',',coords);print_q(coords,q[j]);}
        fputc('\n',coords);
    }
    fclose(coords);
    g_ncoords=ncoords;
    if(!ok||(unsigned long)ftell(in)!=maxfile)goto fail;
    for(i=0;i<np;i++)if(g_parts[i].joint>ncoords)goto fail;
    base=argv[2];
    {const char *cur=argv[2];while(*cur){if(*cur=='/'||*cur=='\\')base=cur+1;cur++;}}
    sprintf(name,"%s.mtl",argv[2]);mtl=fopen(name,"wb");if(!mtl)goto fail;
    write_mtl(mtl,base,nt);fclose(mtl);
    sprintf(name,"%s.obj",argv[2]);obj=fopen(name,"wb");if(!obj)goto fail;
    write_obj(in,obj,nv,np,&ok,base);
    fclose(obj);fclose(in);
    if(!ok)goto bad;
    printf("PASS vertices=%lu triangles=%lu groups=%lu coords=%lu tex_desc=%lu tex_planes=%lu final_tex=%lux%lu bytes=%lu\n",nv,nf,np,ncoords+1UL,nd,nt,w,h,maxfile);
    return 0;
fail:
    fclose(in);
bad:
    fputs("FAIL: invalid or unsupported NAB layout\n",stderr);
    return 1;
}
