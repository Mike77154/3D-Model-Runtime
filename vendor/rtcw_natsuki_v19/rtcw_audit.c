/* Self-contained QC layer for RTCW models: CC0-1.0, ISO C89. No heap, floating
   point or 64-bit types. This does NOT infer missing topology from open edges. */
#include "rtcw_audit.h"
#include "rtcw_codec.h"
#include "rtcw_clipbank.h"
#include "rtcw_vfs.h"
#include "rtcw_vfs_material.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#define AUD_PATH 768U
#define AUD_CAP 256U
static char aerr[192]="OK";
static char gdir[AUD_PATH];
static unsigned long num_errors,num_warnings,num_notes;
static FILE *findings;
static char mdl[AUD_PATH],model_label[AUD_PATH],cfgpath[AUD_PATH],skinpath[AUD_PATH];
static unsigned int full_vfs;

const char *rtcw_audit_error(void){return aerr;}
static int fail(const char *s){
    strncpy(aerr,s,sizeof(aerr)-1U);aerr[sizeof(aerr)-1U]=0;return 0;
}
static int is_dash(const char *s){return !s||s[0]==0||(s[0]=='-'&&s[1]==0);}
static int path(const char *name,char *buf){
    size_t n=strlen(gdir),m=strlen(name);
    if(n+m+2U>=AUD_PATH)return 0;
    strcpy(buf,gdir);if(n&&gdir[n-1U]!='/'&&gdir[n-1U]!='\\')strcat(buf,"/");
    strcat(buf,name);return 1;
}
static int make_dir(const char *p){
#ifdef _WIN32
    return _mkdir(p)==0;
#else
    return mkdir(p,0700)==0;
#endif
}
static int ends(const char *s,const char *ext){
    size_t n=strlen(s),e=strlen(ext),i;int a,b;
    if(n<e)return 0;
    for(i=0U;i<e;i++){
        a=(unsigned char)s[n-e+i];b=(unsigned char)ext[i];
        if(a>='A'&&a<='Z')a+=32;
        if(b>='A'&&b<='Z')b+=32;
        if(a!=b)return 0;
    }
    return 1;
}
static void finding(const char *severity,const char *code,const char *target,const char *detail){
    if(strcmp(severity,"ERROR")==0)num_errors++;
    else if(strcmp(severity,"WARN")==0)num_warnings++;
    else num_notes++;
    if(findings)fprintf(findings,"%s\t%s\t%s\t%s\n",severity,code,target,detail);
}
static unsigned long badbones(void){
    unsigned long bad=0UL;unsigned long i,k;long p;
    rtcw_model *m=&rtcw_current;
    for(i=0UL;i<m->bone_count;i++){
        p=m->bones[i].parent;
        if(p< -1L||p>=(long)m->bone_count){
            finding("ERROR","BONE_PARENT_OUT_OF_RANGE",m->bones[i].name,"Parent index invalid");
            bad++;continue;
        }
        p=(long)i;k=0UL;
        while(p>=0L&&k<=m->bone_count){
            if(p>=(long)m->bone_count)break;
            p=m->bones[p].parent;k++;
        }
        if(k>m->bone_count){
            finding("ERROR","BONE_CYCLE",m->bones[i].name,"Parent chain cycles");bad++;
        }
        if(m->bones[i].name[0]==0)
            finding("WARN","BONE_UNNAMED","bone","Empty bone name");
    }
    for(i=0UL;i<m->tag_count;i++){
        if(m->tags[i].bone_index>=0L&&m->tags[i].bone_index>=(long)m->bone_count){
            finding("ERROR","TAG_BONE_OUT_OF_RANGE",m->tags[i].name,"Tag references nonexistent bone");bad++;
        }
    }
    return bad;
}
/* Position comparisons are exact in Q6. Cross product uses reduced Q6 deltas
   bounded to +/-1024, ensuring signed 32-bit safe arithmetic. */
static int collapsed(const rtcw_vertex *a,const rtcw_vertex *b,const rtcw_vertex *c,int *unknown){
    long ab[3],ac[3],cr[3],v[3][3];unsigned int i;
    if((a->x==b->x&&a->y==b->y&&a->z==b->z)||
       (a->x==c->x&&a->y==c->y&&a->z==c->z)||
       (b->x==c->x&&b->y==c->y&&b->z==c->z))return 1;
    v[0][0]=a->x;v[0][1]=a->y;v[0][2]=a->z;
    v[1][0]=b->x;v[1][1]=b->y;v[1][2]=b->z;
    v[2][0]=c->x;v[2][1]=c->y;v[2][2]=c->z;
    for(i=0U;i<3U;i++){
        /* Bounded subtraction avoids overflow with signed 32-bit long. */
        if(v[0][i]<-1000000L||v[0][i]>1000000L||
           v[1][i]<-1000000L||v[1][i]>1000000L||
           v[2][i]<-1000000L||v[2][i]>1000000L){
            *unknown=1;return 0;
        }
        ab[i]=v[1][i]-v[0][i];
        ac[i]=v[2][i]-v[0][i];
        if(ab[i]< -1024L||ab[i]>1024L||ac[i]< -1024L||ac[i]>1024L){
            *unknown=1;return 0;
        }
    }
    cr[0]=ab[1]*ac[2]-ab[2]*ac[1];
    cr[1]=ab[2]*ac[0]-ab[0]*ac[2];
    cr[2]=ab[0]*ac[1]-ab[1]*ac[0];
    return cr[0]==0L&&cr[1]==0L&&cr[2]==0L;
}
static int geometry(FILE *fr,unsigned long frame){
    unsigned long t,deg=0UL,unknown=0UL,wrong=0UL;
    long lo[3],hi[3];unsigned long v;
    rtcw_model *m=&rtcw_current;
    for(v=0UL;v<m->vertex_count;v++){
        const rtcw_vertex *p=&m->verts[v];
        if(v==0UL){lo[0]=hi[0]=p->x;lo[1]=hi[1]=p->y;lo[2]=hi[2]=p->z;}
        else{
            if(p->x<lo[0])lo[0]=p->x;
            if(p->x>hi[0])hi[0]=p->x;
            if(p->y<lo[1])lo[1]=p->y;
            if(p->y>hi[1])hi[1]=p->y;
            if(p->z<lo[2])lo[2]=p->z;
            if(p->z>hi[2])hi[2]=p->z;
        }
    }
    if(m->vertex_count==0UL){for(v=0UL;v<3UL;v++)lo[v]=hi[v]=0L;}
    for(t=0UL;t<m->triangle_count;t++){
        rtcw_triangle *q=&m->tris[t];int uncertain=0;
        if(q->a>=m->vertex_count||q->b>=m->vertex_count||q->c>=m->vertex_count||q->surface>=m->surface_count){wrong++;continue;}
        if(q->a==q->b||q->a==q->c||q->b==q->c||
           collapsed(&m->verts[q->a],&m->verts[q->b],&m->verts[q->c],&uncertain))deg++;
        if(uncertain)unknown++;
    }
    if(fr && fprintf(fr,"%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
        frame,m->vertex_count,m->triangle_count,deg,wrong,unknown,
        lo[0],lo[1],lo[2],hi[0],hi[1],hi[2])<0)return 0;
    if(wrong)finding("ERROR","BAD_TRIANGLE_INDEX","mesh","Triangles reference out-of-range vertices/surfaces");
    if(deg)finding("WARN","DEGENERATE_TRIANGLE","mesh","Zero area / repeated-vertex triangles (inspect source geometry)");
    if(unknown)finding("NOTE","AREA_CHECK_SKIPPED","mesh","Some triangles are too large for bounded fixed-point area test");
    return 1;
}
static int surfaces(void){
    FILE *f,*uvf;char name[AUD_PATH];unsigned int i;
    rtcw_model *m=&rtcw_current;
    if(!path("surfaces.tsv",name))return fail("report path too long");
    f=fopen(name,"wb");if(!f)return fail("cannot write surfaces.tsv");
    if(!path("uvs.tsv",name)){fclose(f);return fail("path too long");}
    uvf=fopen(name,"wb");if(!uvf){fclose(f);return fail("cannot write uvs.tsv");}
    fputs("surface\tmin_u_q12\tmax_u_q12\tmin_v_q12\tmax_v_q12\toutside_unit_square\tstatus\n",uvf);
    fputs("surface\tname\tmaterial\tfirst_vertex\tvertices\tfirst_triangle\ttriangles\tmin_lod\n",f);
    for(i=0U;i<m->surface_count;i++){
        const rtcw_surface *s=&m->surfaces[i];
        fprintf(f,"%u\t%s\t%s\t%lu\t%lu\t%lu\t%lu\t%ld\n",i,s->name,s->shader,
           s->first_vertex,s->vertices,s->first_triangle,s->triangles,s->min_lod);
        if(s->shader[0]==0)finding("WARN","NO_MATERIAL_NAME",s->name,"Model surface has no shader name");
        if(s->vertices==0UL||s->triangles==0UL)
            finding("WARN","EMPTY_SURFACE",s->name,"Surface has zero vertices or triangles");
        if(s->first_vertex>m->vertex_count||s->vertices>m->vertex_count-s->first_vertex||
           s->first_triangle>m->triangle_count||s->triangles>m->triangle_count-s->first_triangle)
            finding("ERROR","SURFACE_RANGE",s->name,"Surface spans outside model geometry");
        else if(s->vertices>0UL){
            unsigned long v,outside=0UL;long min_u,max_u,min_v,max_v;
            const rtcw_vertex *first=&m->verts[s->first_vertex];
            min_u=max_u=first->u;min_v=max_v=first->v;
            for(v=s->first_vertex;v<s->first_vertex+s->vertices;v++){
                const rtcw_vertex *pt=&m->verts[v];
                if(pt->u<min_u)min_u=pt->u;
                if(pt->u>max_u)max_u=pt->u;
                if(pt->v<min_v)min_v=pt->v;
                if(pt->v>max_v)max_v=pt->v;
                if(pt->u<0L||pt->u>4096L||pt->v<0L||pt->v>4096L)outside++;
            }
            fprintf(uvf,"%u\t%ld\t%ld\t%ld\t%ld\t%lu\t%s\n",
                    i,min_u,max_u,min_v,max_v,outside,
                    min_u==max_u&&min_v==max_v?"constant":"varied");
            if(min_u==max_u&&min_v==max_v)
                finding("WARN","UV_CONSTANT",s->name,"All vertices share the same UV; may be intentional");
        }
    }
    if(fclose(uvf)!=0){fclose(f);return 0;}
    return fclose(f)==0;
}
static int clips(const char *cfg){
    char name[AUD_PATH];unsigned int i;unsigned long total=rtcw_current.frames;
    if(is_dash(cfg)){finding("NOTE","NO_ANIMATION_CFG","clips","No CFG supplied/found; clip names cannot be inferred");return 1;}
    if(!rtcw_clipbank_read(cfg,total)){
        finding("WARN","CFG_PARSE_FAILED","clips",rtcw_clipbank_error());return 1;
    }
    for(i=0U;i<rtcw_clipbank_count();i++){
        const rtcw_clip *c=rtcw_clipbank_at(i);
        if(!c->valid)finding("ERROR","CLIP_OUT_OF_RANGE",c->name,"Clip exceeds model frame count");
        if(c->fps_milli==0UL)finding("ERROR","CLIP_INVALID_FPS",c->name,"FPS is zero");
    }
    if(!path("clips.tsv",name))return fail("report path too long");
    return rtcw_clipbank_report(name);
}
static int provenance_findings(const char *f){
    char buf[4096],*cols[20],name[100];FILE *in;unsigned int n,i;unsigned long count=0UL;
    in=fopen(f,"rb");if(!in)return 0;
    if(!fgets(buf,sizeof(buf),in)){fclose(in);return 0;}
    while(fgets(buf,sizeof(buf),in)){
        char *p=buf; n=0U;
        while(n<20U){cols[n++]=p;p=strchr(p,'\t');if(!p)break;*p++=0;}
        if(n<6U)continue;
        for(i=0U;i<n;i++){
            size_t l=strlen(cols[i]);
            while(l&&(cols[i][l-1U]=='\r'||cols[i][l-1U]=='\n'))cols[i][--l]=0;
        }
        if(strcmp(cols[5],"found")!=0){
            strncpy(name,cols[1],sizeof(name)-1U);name[sizeof(name)-1U]=0;
            finding("WARN","MATERIAL_UNRESOLVED",name,"VFS shader/texture reference could not be resolved");
        }else count++;
        /* Extra stages: 3 columns (image, pk3, status). */
        for(i=9U;i<n;i+=3U)if(strcmp(cols[i],"missing")==0)
            finding("WARN","SHADER_STAGE_MISSING",cols[1],"Additional shader texture absent in mounted PK3");
    }
    fclose(in);
    if(count==0UL)finding("NOTE","NO_RESOLVED_TEXTURES","materials","No surface material resolved from PK3");
    return 1;
}
static int report(unsigned int cap,const char *material_pref){
    FILE *fr,*summary;char tmp[AUD_PATH],materialmap[AUD_PATH],prov[AUD_PATH];
    unsigned long k,step,frame,last=(unsigned long)(~0UL),scanned=0UL;int ok=1;
    unsigned long old_vertex_count,old_triangle_count;unsigned int old_surface_count;
    unsigned long origframes=rtcw_current.frames;
    if(!path("findings.tsv",tmp))return fail("report path too long");
    findings=fopen(tmp,"wb");if(!findings)return fail("cannot create findings.tsv");
    fputs("severity\tcode\tsubject\tdetail\n",findings);
    if(!path("frames.tsv",tmp))return fail("report path too long");
    fr=fopen(tmp,"wb");if(!fr)return fail("cannot create frames.tsv");
    fputs("frame\tvertices\ttriangles\tdegenerate\tbad_indices\tarea_skipped\tmin_x_q6\tmin_y_q6\tmin_z_q6\tmax_x_q6\tmax_y_q6\tmax_z_q6\n",fr);
    if(!rtcw_decode(0UL)){fclose(fr);return fail(rtcw_error());}
    if(!is_dash(skinpath)){
        if(!rtcw_apply_skin(skinpath))finding("WARN","SKIN_PARSE_FAILED","skin","Could not apply skin override");
    }
    if(!surfaces()){fclose(fr);return 0;}
    badbones();
    if(!clips(cfgpath)){fclose(fr);return fail("cannot write clips.tsv");}
    if(full_vfs){
        if(!path("materials.tsv",materialmap)||!path("material_sources.tsv",prov)){fclose(fr);return fail("output name too long");}
        if(rtcw_vfs_material_stage_time(gdir,materialmap,prov,material_pref,0UL))provenance_findings(prov);
        else finding("WARN","MATERIAL_STAGING_FAILED","materials",rtcw_vfs_material_error());
    }else finding("NOTE","MATERIALS_NOT_VALIDATED","materials","Local mode does not resolve shader stages; use VFS mode");
    old_vertex_count=rtcw_current.vertex_count;
    old_triangle_count=rtcw_current.triangle_count;old_surface_count=rtcw_current.surface_count;
    if(cap<1U)cap=1U;
    if(cap>AUD_CAP)cap=AUD_CAP;
    step=(origframes+(unsigned long)cap-1UL)/(unsigned long)cap;
    if(step<1UL)step=1UL;
    for(k=0UL;k<origframes;){
        frame=k;
        if(!rtcw_decode(frame)){finding("ERROR","FRAME_DECODE", "model",rtcw_error());ok=0;break;}
        if(!is_dash(skinpath))rtcw_apply_skin(skinpath);
        if(rtcw_current.vertex_count!=old_vertex_count||rtcw_current.triangle_count!=old_triangle_count||
           rtcw_current.surface_count!=old_surface_count){
            finding("ERROR","FRAME_TOPOLOGY_CHANGE","model","Frame changes vertex/triangle/surface counts");
        }
        if(!geometry(fr,frame)){ok=0;break;}
        scanned++;last=frame;
        if(origframes-k<=step)break;
        k+=step;
    }
    if(ok&&last!=origframes-1UL){
        if(rtcw_decode(origframes-1UL)&&geometry(fr,origframes-1UL)){scanned++;}
        else {finding("ERROR","LAST_FRAME_DECODE","model",rtcw_error());ok=0;}
    }
    if(fclose(fr)!=0)ok=0;
    if(!path("summary.tsv",tmp))return fail("output path too long");
    summary=fopen(tmp,"wb");if(!summary)return fail("cannot write summary.tsv");
    fprintf(summary,"key\tvalue\nmodel\t%s\nformat\t%s\nframes_total\t%lu\nframes_scanned\t%lu\nsurfaces\t%u\nvertices\t%lu\ntriangles\t%lu\nbones\t%lu\ntags\t%lu\nerrors\t%lu\nwarnings\t%lu\nnotes\t%lu\ncoverage\t%s\n",
       model_label,rtcw_current.format,origframes,scanned,old_surface_count,old_vertex_count,
       old_triangle_count,rtcw_current.bone_count,rtcw_current.tag_count,
       num_errors,num_warnings,num_notes,scanned==origframes?"full":"sampled");
    if(fclose(summary)!=0)ok=0;
    if(fclose(findings)!=0)ok=0;
    findings=NULL;
    if(!ok)return fail("audit incomplete: frame decode or output error");
    strcpy(aerr,"OK");return 1;
}
static int initialize(const char *dir){
    if(!dir||!dir[0]||strlen(dir)>AUD_PATH-40U)return fail("invalid output directory");
    strcpy(gdir,dir);
    if(!make_dir(gdir))return fail("output directory must be new and creatable");
    num_errors=num_warnings=num_notes=0UL;
    findings=NULL;
    cfgpath[0]=skinpath[0]=0;return 1;
}
int rtcw_audit_local(const char *model,const char *cfg,const char *skin,
                     const char *dir,unsigned int cap){
    full_vfs=0U;
    if(!model||strlen(model)>=AUD_PATH)return fail("invalid model filename");
    if(!initialize(dir))return 0;
    strcpy(mdl,model);strcpy(model_label,model);
    if(!is_dash(cfg)){
        if(strlen(cfg)>=AUD_PATH)return fail("CFG path too long");
        strcpy(cfgpath,cfg);
    }
    if(!is_dash(skin)){
        if(strlen(skin)>=AUD_PATH)return fail("skin path too long");
        strcpy(skinpath,skin);
    }
    if(!rtcw_open(mdl)||!rtcw_decode(0UL))return fail(rtcw_error());
    return report(cap,NULL);
}
static int virtual_sibling(const char *model,const char *leaf,char *out){
    const char *p=strrchr(model,'/');size_t len=p?(size_t)(p-model)+1U:0U;
    if(len+strlen(leaf)+1U>=AUD_PATH)return 0;
    memcpy(out,model,len);strcpy(out+len,leaf);return 1;
}
static int extract_sibling(const char *model,const char *leaf,const char *to){
    rtcw_vfs_file x;char virt[AUD_PATH];
    if(!virtual_sibling(model,leaf,virt))return -1;
    if(!rtcw_vfs_lookup(virt,&x))return 0;
    return rtcw_vfs_extract(virt,to)?1:-1;
}
int rtcw_audit_vfs(const char *mounts,const char *virt,const char *dir,unsigned int cap){
    rtcw_vfs_file x;char tmp[AUD_PATH],p[AUD_PATH],origin[AUD_PATH];FILE *f;int rc;
    full_vfs=1U;
    if(!virt||strlen(virt)>=AUD_PATH||(!ends(virt,".mds")&&!ends(virt,".mdc")&&!ends(virt,".md3")))
        return fail("virtual model must be .mds/.mdc/.md3");
    if(!initialize(dir))return 0;
    if(!rtcw_vfs_mounts(mounts))return fail(rtcw_vfs_error());
    if(!rtcw_vfs_lookup(virt,&x))return fail("model not found in mounted PK3");
    if(!path("_model.tmp",tmp))return fail("path too long");
    if(!rtcw_vfs_extract(virt,tmp))return fail(rtcw_vfs_error());
    strcpy(mdl,tmp);strcpy(model_label,virt);
    if(!path("sources.tsv",p))return fail("path too long");
    f=fopen(p,"wb");if(!f)return fail("cannot create source report");
    fprintf(f,"role\tvirtual_path\twinning_pk3\toverrides\nmodel\t%s\t%s\t%u\n",virt,rtcw_vfs_mount_name(x.mount),x.overrides);
    if(!path("_animation.tmp",cfgpath)){fclose(f);return fail("path too long");}
    rc=extract_sibling(virt,"wolfanim.cfg",cfgpath);
    if(rc==0)rc=extract_sibling(virt,"animation.cfg",cfgpath);
    if(rc<0){fclose(f);return fail("CFG extraction failed");}
    if(rc==0)cfgpath[0]=0;
    else{
        if(!virtual_sibling(virt,"wolfanim.cfg",origin)){fclose(f);return fail("path too long");}
        if(!rtcw_vfs_lookup(origin,&x))virtual_sibling(virt,"animation.cfg",origin);
        if(rtcw_vfs_lookup(origin,&x))fprintf(f,"cfg\t%s\t%s\t%u\n",origin,rtcw_vfs_mount_name(x.mount),x.overrides);
    }
    if(!path("_skin.tmp",skinpath)){fclose(f);return fail("path too long");}
    rc=extract_sibling(virt,"body.skin",skinpath);
    if(rc<0){fclose(f);return fail("skin extraction failed");}
    if(rc==0)skinpath[0]=0;
    else{
        virtual_sibling(virt,"body.skin",origin);
        if(rtcw_vfs_lookup(origin,&x))fprintf(f,"skin\t%s\t%s\t%u\n",origin,rtcw_vfs_mount_name(x.mount),x.overrides);
    }
    if(fclose(f)!=0)return fail("error closing sources report");
    if(!rtcw_open(mdl)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!report(cap,NULL))return 0;
    remove(mdl);
    if(cfgpath[0])remove(cfgpath);
    if(skinpath[0])remove(skinpath);
    return 1;
}

/* Bounded batch driver: one independent report directory per winning model.
   No source file names are used in host output paths (path traversal defense).
   A malformed model is recorded FAILED and does not abort unrelated models. */
int rtcw_audit_batch_vfs(const char *mounts,const char *root,
                         unsigned int max_models,unsigned int frame_cap){
    char dirname[AUD_PATH],catalog[AUD_PATH],virtual_name[256];
    unsigned long i,total;unsigned int done=0U,ok_count=0U,bad_count=0U;
    FILE *out;rtcw_vfs_file current;
    if(!root||strlen(root)>=AUD_PATH-40U||max_models<1U||max_models>256U)
        return fail("invalid batch output path or model cap");
    if(!make_dir(root))return fail("batch output directory must be new");
    if(!rtcw_vfs_mounts(mounts))return fail(rtcw_vfs_error());
    total=rtcw_vfs_count();
    if(strlen(root)+16U>=AUD_PATH)return fail("batch path too long");
    strcpy(catalog,root);strcat(catalog,"/catalog.tsv");
    out=fopen(catalog,"wb");if(!out)return fail("cannot write batch catalog");
    fputs("index\tmodel\tstatus\terrors\twarnings\tnotes\treport_dir\n",out);
    for(i=0UL;i<total&&done<max_models;i++){
        if(!rtcw_vfs_at(i,&current)){fclose(out);return fail("VFS iteration failed");}
        if(!ends(current.entry.name,".mds")&&!ends(current.entry.name,".mdc")&&
           !ends(current.entry.name,".md3"))continue;
        strncpy(virtual_name,current.entry.name,sizeof(virtual_name)-1U);
        virtual_name[sizeof(virtual_name)-1U]=0;
        if(strlen(current.entry.name)>=sizeof(virtual_name)){
            fprintf(out,"%u\t-\tSKIPPED_PATH\t0\t0\t0\t-\n",done);
            done++;bad_count++;continue;
        }
        sprintf(dirname,"%s/model_%03u",root,done);
        if(rtcw_audit_vfs(mounts,virtual_name,dirname,frame_cap)){
            fprintf(out,"%u\t%s\tAUDITED\t%lu\t%lu\t%lu\tmodel_%03u\n",
                    done,virtual_name,num_errors,num_warnings,num_notes,done);
            ok_count++;
        }else{
            fprintf(out,"%u\t%s\tFAILED\t0\t0\t0\tmodel_%03u\n",
                    done,virtual_name,done);
            bad_count++;
        }
        done++;
    }
    if(fclose(out)!=0)return fail("cannot close batch catalog");
    if(done==0U)return fail("no MD3/MDC/MDS files in mounted packages");
    if(ok_count==0U)return fail("all model audits failed");
    (void)bad_count;
    strcpy(aerr,"OK");return 1;
}
