#include "c4d89.h"
#include <stdio.h>
#include <string.h>
/* Interleaving 'vn' then 'f' is legal OBJ: normals precede the face using them. */
typedef struct SWriter {
    FILE *f;
    unsigned long voff,uoff,noff;
    unsigned int meshno;
    const C4D89_PolyRegions *regions;
    char group[C4D89_MAX_NAME+32u];
} SWriter;
static void writefrac(FILE*f,long val,long one)
{
    unsigned long a,ip,fp;
    if(val<0L){fputc('-',f);a=(unsigned long)(-(val+1L))+1ul;}
    else a=(unsigned long)val;
    ip=a/(unsigned long)one;
    fp=((a%(unsigned long)one)*10000ul+(unsigned long)one/2ul)/(unsigned long)one;
    if(fp==10000ul){ip++;fp=0ul;}
    fprintf(f,"%lu.%04lu",ip,fp);
}
static int onv(void*u,unsigned long i,long x,long y,long z)
{
    SWriter*w=(SWriter*)u;(void)i;
    fputs("v ",w->f);writefrac(w->f,x,65536L);fputc(' ',w->f);
    writefrac(w->f,y,65536L);fputc(' ',w->f);writefrac(w->f,z,65536L);
    fputc('\n',w->f);return !ferror(w->f);
}
static int onuv(void*u,unsigned long i,long x,long y)
{
    SWriter*w=(SWriter*)u;(void)i;
    fputs("vt ",w->f);writefrac(w->f,x,65536L);fputc(' ',w->f);
    writefrac(w->f,65536L-y,65536L);fputc('\n',w->f);
    return !ferror(w->f);
}
static int smoothface(void*u,unsigned long i,const C4D89_Face*f,short n[4][3])
{
    SWriter*w=(SWriter*)u;
    unsigned int j,ids[2],num;
    const char *group;
    char buf[C4D89_MAX_NAME+32u];unsigned int z=0u;
    unsigned int vs[4];
    num=c4d89_find_face_regions(w->regions,w->meshno,i,ids,2u);
    group=num==1u?w->regions->ranges[ids[0]].selection:
          (num>1u?"overlapping":"unassigned");
    z=(unsigned int)sprintf(buf,"mesh%u_",w->meshno);
    for(j=0u;group[j] && z<sizeof(buf)-1u;j++){
        unsigned char c=(unsigned char)group[j];
        buf[z++]=(char)(((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                       (c>='0'&&c<='9')||c=='_')?c:'_');
    }
    buf[z]=0;
    if(strcmp(buf,w->group)!=0){strcpy(w->group,buf);fprintf(w->f,"g %s\n",buf);}
    for(j=0u;j<f->corners;j++){
        fputs("vn ",w->f);writefrac(w->f,(long)n[j][0],1024L);
        fputc(' ',w->f);writefrac(w->f,(long)n[j][1],1024L);
        fputc(' ',w->f);writefrac(w->f,(long)n[j][2],1024L);
        fputc('\n',w->f);
    }
    vs[0]=f->a;vs[1]=f->b;vs[2]=f->c;vs[3]=f->d;
    fputc('f',w->f);
    for(j=0u;j<f->corners;j++)
        fprintf(w->f," %lu/%lu/%lu",w->voff+(unsigned long)vs[j]+1ul,
                w->uoff+i*4ul+(unsigned long)j+1ul,w->noff+(unsigned long)j+1ul);
    fputc('\n',w->f);w->noff+=(unsigned long)f->corners;
    return !ferror(w->f);
}
int c4d89_export_obj_smooth(const char *path,const char *dest,int cosine_q10)
{
    static C4D89_MeshTable tbl;
    static C4D89_PolyRegions regions;
    C4D89_MeshSink sink;SWriter w;
    unsigned int i;int rc;
    if(!path||!dest)return C4D89_ERR_CORRUPT;
    rc=c4d89_find_meshes(path,&tbl);if(rc)return rc;
    rc=c4d89_scan_poly_regions(path,&tbl,&regions);if(rc)return rc;
    w.f=fopen(dest,"wb");if(!w.f)return C4D89_ERR_IO;
    w.voff=w.uoff=w.noff=0ul;w.regions=&regions;
    fprintf(w.f,"# Native C4D6 mesh, UV and crease-aware fixed-point Phong-like normals.\n");
    fprintf(w.f,"# Selection groups are NOT proof of material binding.\n");
    memset(&sink,0,sizeof(sink));sink.vertex=onv;sink.uv=onuv;
    for(i=0u;i<tbl.count;i++){
        w.meshno=i;w.group[0]=0;
        fprintf(w.f,"o C4Dmesh_%02u\n",i);
        rc=c4d89_stream_mesh(path,i,&tbl.meshes[i],&sink,&w);
        if(rc)break;
        rc=c4d89_emit_smooth_faces(path,i,&tbl.meshes[i],&regions,
                                   cosine_q10,smoothface,&w);
        if(rc)break;
        w.voff+=tbl.meshes[i].vertex_count;
        w.uoff+=tbl.meshes[i].uv_count;
    }
    if(fclose(w.f)!=0 && !rc)rc=C4D89_ERR_IO;
    return rc;
}
