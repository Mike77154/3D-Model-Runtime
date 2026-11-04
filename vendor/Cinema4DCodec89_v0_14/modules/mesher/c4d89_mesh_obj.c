#include "c4d89.h"
#include <stdio.h>
#include <string.h>
typedef struct Writer {
    FILE *f;
    unsigned long voff,uoff;
    unsigned int mesh_index;
    const C4D89_PolyRegions *groups;
    char active_group[C4D89_MAX_NAME+32u];
} Writer;
static void writeq(FILE *f,long q)
{
    unsigned long a,ip,fp;
    if(q<0L) { fputc('-',f); a=(unsigned long)(-(q+1L))+1ul; }
    else a=(unsigned long)q;
    ip=a/65536ul;fp=((a%65536ul)*10000ul+32768ul)/65536ul;
    if(fp>=10000ul){ip++;fp=0ul;}
    fprintf(f,"%lu.%04lu",ip,fp);
}
static int onbegin(void *u,unsigned int n,const C4D89_MeshDesc *desc)
{Writer*w=(Writer*)u;(void)desc;w->mesh_index=n;w->active_group[0]='\0';return fprintf(w->f,"o C4Dmesh_%02u\n",n)>0;}
static int onvertex(void *u,unsigned long i,long x,long y,long z)
{Writer*w=(Writer*)u;(void)i;fputs("v ",w->f);writeq(w->f,x);fputc(' ',w->f);writeq(w->f,y);fputc(' ',w->f);writeq(w->f,z);fputc('\n',w->f);return !ferror(w->f);}
/* OBJ convention and our preview pipeline expect V with bottom-left origin.
 * Native recovered UVs behave as top-left oriented in the current evidence set,
 * so export V flipped to keep embedded textures aligned in standard OBJ tools. */
static int onuv(void *u,unsigned long i,long x,long y)
{Writer*w=(Writer*)u;long one=(long)65536L;long v=(one-y);(void)i;fputs("vt ",w->f);writeq(w->f,x);fputc(' ',w->f);writeq(w->f,v);fputc('\n',w->f);return !ferror(w->f);}
static int onface(void *u,unsigned long i,const C4D89_Face*face)
{
    Writer*w=(Writer*)u;unsigned long base=w->uoff+i*4ul+1ul;
    /* Groups are native polygon selections. A group is NOT a material slot. */
    if(w->groups) {
        unsigned int indices[2];
        unsigned int n=c4d89_find_face_regions(w->groups,w->mesh_index,i,indices,2u);
        char name[C4D89_MAX_NAME+32u];
        unsigned int j,k=0u;
        const char *value=(n==1u)?w->groups->ranges[indices[0]].selection:
                          (n>1u)?"overlapping_selections":"unassigned";
        k=(unsigned int)sprintf(name,"mesh%u_",w->mesh_index);
        for(j=0u;value[j]!=0 && k<sizeof(name)-1u;j++) {
            unsigned char c=(unsigned char)value[j];
            name[k++]=(char)((c>=65u && c<=90u)||(c>=97u && c<=122u)||
                             (c>=48u && c<=57u)||c==95u?c:95u);
        }
        name[k]='\0';
        if(strcmp(name,w->active_group)!=0){
            fprintf(w->f,"g %s\n",name);
            strcpy(w->active_group,name);
        }
    }
    fprintf(w->f,"f %lu/%lu %lu/%lu %lu/%lu",w->voff+face->a+1ul,base,
      w->voff+face->b+1ul,base+1ul,w->voff+face->c+1ul,base+2ul);
    if(face->corners==4u)fprintf(w->f," %lu/%lu",w->voff+face->d+1ul,base+3ul);
    fputc('\n',w->f);return !ferror(w->f);
}
/* End callback doesn't have desc; offsets derived from table in export function. */
static int onend(void*u,unsigned int n)
{(void)n;return !ferror(((Writer*)u)->f);}
static int export_obj_internal(const char *path,const char *dest,unsigned int grouped)
{
    static C4D89_MeshTable table;
    static C4D89_PolyRegions regions;
    Writer w; C4D89_MeshSink sink;
    unsigned int i;int rc;
    if(!path||!dest)return C4D89_ERR_CORRUPT;
    rc=c4d89_find_meshes(path,&table);if(rc)return rc;
    if(grouped){
        rc=c4d89_scan_poly_regions(path,&table,&regions);if(rc)return rc;
    }
    w.f=fopen(dest,"wb");if(!w.f)return C4D89_ERR_IO;
    w.voff=0ul;w.uoff=0ul;w.groups=grouped?&regions:(const C4D89_PolyRegions*)0;
    memset(&sink,0,sizeof(sink));
    sink.begin=onbegin;sink.vertex=onvertex;sink.uv=onuv;
    sink.face=onface;sink.end=onend;
    fprintf(w.f,"# Directly decoded from native XC4DC4D6; not an intermediary runtime format.\n");
    fprintf(w.f,"# Material selection, bones and hierarchy not yet decoded.\n");
    for(i=0u;i<table.count;i++) {
        rc=c4d89_stream_mesh(path,i,&table.meshes[i],&sink,&w);
        if(rc)break;
        w.voff+=table.meshes[i].vertex_count;
        w.uoff+=table.meshes[i].uv_count;
    }
    if(fclose(w.f)!=0 && rc==0)rc=C4D89_ERR_IO;
    return rc;
}

int c4d89_export_obj(const char *path,const char *dest)
{return export_obj_internal(path,dest,0u);}
int c4d89_export_obj_groups(const char *path,const char *dest)
{return export_obj_internal(path,dest,1u);}
