/* CC0-1.0; strict C89, bounded PK3->shader->texture association.
   Tracks origin per surface. Never guesses attachments. Not a shader interpreter. */
#include "rtcw_vfs_material.h"
#include "rtcw_vfs.h"
#include "rtcw_codec.h"
#include "rtcw_material.h"
#include "rtcw_texcache.h"
#include "rtcw_stagefx.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define VM_PATH 768U
#define VM_CACHE_MATERIALS RTCW_MAX_SURFACES
static char vm_err[192]="OK";
static int fail(const char *s){strncpy(vm_err,s,191U);vm_err[191U]=0;return 0;}
const char *rtcw_vfs_material_error(void){return vm_err;}
static int lowerc(int c){return (c>='A'&&c<='Z')?c+32:c;}
static int suffix(const char *p,const char *ext){
    size_t i,n=strlen(p),e=strlen(ext);
    if(n<e)return 0;
    for(i=0;i<e;i++)if(lowerc((unsigned char)p[n-e+i])!=lowerc((unsigned char)ext[i]))return 0;
    return 1;
}
static int safe(const char *s){
    size_t i,n;if(!s)return 0;n=strlen(s);
    if(!n||n>240U||s[0]=='/'||s[0]=='\\'||strchr(s,':'))return 0;
    for(i=0;i<n;i++){
        if(s[i]=='\\'||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'||(unsigned char)s[i]<32U)return 0;
        if(s[i]=='.'&&i+1<n&&s[i+1]=='.')return 0;
    }
    return 1;
}
static int compose(const char *root,const char *leaf,char *out){
    size_t r=strlen(root),l=strlen(leaf);
    if(!r||r+l+2U>=VM_PATH)return 0;
    strcpy(out,root);if(out[r-1U]!='/'&&out[r-1U]!='\\')strcat(out,"/");strcat(out,leaf);return 1;
}
static int lookup_image(const char *name,rtcw_vfs_file *f,char *actual){
    static const char *exts[]={".tga",".jpg",".jpeg"};
    size_t n;unsigned int i;
    if(!safe(name))return 0;
    n=strlen(name);
    if(suffix(name,".tga")||suffix(name,".jpg")||suffix(name,".jpeg")){
        if(rtcw_vfs_lookup(name,f)){strcpy(actual,f->entry.name);return 1;}
        return 0;
    }
    for(i=0U;i<3U;i++){
        if(n+strlen(exts[i])+1U>=256U)return 0;
        strcpy(actual,name);strcat(actual,exts[i]);
        if(rtcw_vfs_lookup(actual,f)){strcpy(actual,f->entry.name);return 1;}
    }
    return 0;
}
typedef struct vm_shader_cache_s {
    char material[65],dir[VM_PATH],file[VM_PATH],origin[256];
} vm_shader_cache;
static vm_shader_cache scache[VM_CACHE_MATERIALS];
static unsigned int scount=0U;
static char prior_src[RTCW_MAX_SURFACES][RTCW_FX_MAX_STAGES][256];
static unsigned int prior_mount[RTCW_MAX_SURFACES][RTCW_FX_MAX_STAGES];
static char prior_dir[VM_PATH];
static unsigned long extracted,reused;
void rtcw_vfs_material_cache_clear(void){
    unsigned int i;
    for(i=0U;i<scount;i++){
        if(scache[i].file[0])remove(scache[i].file);
    }
    scount=0U;prior_dir[0]=0;
    for(i=0U;i<RTCW_MAX_SURFACES;i++){
        unsigned int j;
        for(j=0U;j<RTCW_FX_MAX_STAGES;j++)prior_src[i][j][0]=0;
    }
    extracted=0UL;reused=0UL;
}
unsigned long rtcw_vfs_material_extracted(void){return extracted;}
unsigned long rtcw_vfs_material_reused(void){return reused;}
static int shader_ref(const char *dir,const char *material,
                      const char *prefer,unsigned long time_ms,
                      char *ref,char *shader_origin){
    unsigned long count=rtcw_vfs_count(),i;
    unsigned int j;
    rtcw_vfs_file sf;char scratch[VM_PATH];int rc;
    ref[0]=0;shader_origin[0]=0;
    if(prefer && *prefer && prefer[0]!='@'){
        rc=rtcw_material_reference_time(prefer,material,time_ms,ref,256U);
        if(rc<0)return -1;
        if(rc==1){strcpy(shader_origin,"explicit_local_shader");return 1;}
    }
    for(j=0U;j<scount;j++){
        if(strcmp(dir,scache[j].dir)==0 && strcmp(material,scache[j].material)==0){
            rc=rtcw_material_reference_time(scache[j].file,material,time_ms,ref,256U);
            if(rc<=0)return -1;
            strcpy(shader_origin,scache[j].origin);return 1;
        }
    }
    if(prefer&&prefer[0]=='@'){
        if(!rtcw_vfs_lookup(prefer+1,&sf))return -1;
        if(!compose(dir,"vfs_shader_scratch.tmp",scratch))return -1;
        if(!rtcw_vfs_extract(sf.entry.name,scratch))return -1;
        rtcw_material_cache_clear();
        rc=rtcw_material_reference_time(scratch,material,time_ms,ref,256U);
        if(rc<0){remove(scratch);return -1;}
        if(rc==1){
            char leaf[48],keep[VM_PATH];
            if(scount>=VM_CACHE_MATERIALS){remove(scratch);return -1;}
            sprintf(leaf,"vfs_shader_cache_%02u.tmp",scount);
            if(!compose(dir,leaf,keep)){remove(scratch);return -1;}
            if(rename(scratch,keep)!=0){remove(scratch);return -1;}
            strcpy(scache[scount].dir,dir);
            strcpy(scache[scount].material,material);
            strcpy(scache[scount].file,keep);
            strcpy(scache[scount].origin,sf.entry.name);
            scount++;
            strcpy(shader_origin,sf.entry.name);return 1;
        }
        if(remove(scratch)!=0)return -1;
    }
    for(i=count;i>0UL;i--){
        char leaf[32],keep[VM_PATH];
        if(!rtcw_vfs_at(i-1UL,&sf))return -1;
        if(!suffix(sf.entry.name,".shader"))continue;
        if(!compose(dir,"vfs_shader_scratch.tmp",scratch))return -1;
        if(!rtcw_vfs_extract(sf.entry.name,scratch))return -1;
        rtcw_material_cache_clear();
        rc=rtcw_material_reference_time(scratch,material,time_ms,ref,256U);
        if(rc<0){remove(scratch);return -1;}
        if(rc==1){
            if(scount<VM_CACHE_MATERIALS){
                sprintf(leaf,"vfs_shader_cache_%02u.tmp",scount);
                if(!compose(dir,leaf,keep)){remove(scratch);return -1;}
                if(rename(scratch,keep)!=0)return -1;
                strcpy(scache[scount].dir,dir);
                strcpy(scache[scount].material,material);
                strcpy(scache[scount].file,keep);
                strcpy(scache[scount].origin,sf.entry.name);
                scount++;
            }else if(remove(scratch)!=0)return -1;
            strcpy(shader_origin,sf.entry.name);return 1;
        }
        if(remove(scratch)!=0)return -1;
    }
    return 0;
}
/* v0.18 staging: first four columns remain compatible with v0.16,
   followed by three bounded stage image paths ("-" means unresolved).
   All image paths are extracted winning VFS entries, never shader-provided
   local filesystem names. Each stage has a source and status in provenance. */
int rtcw_vfs_material_stage_time(const char *dir,const char *mapfile,const char *provenance,
                                 const char *preferred,unsigned long time_ms){
    FILE *map,*prov;unsigned int i,j;int result=1,rc,fxrc;
    char ref[256],src[256],leaf[72],dst[VM_PATH],origin[256],shader_local[VM_PATH];
    char stage_path[RTCW_FX_MAX_STAGES][VM_PATH];
    char stage_src[RTCW_FX_MAX_STAGES][256];
    const char *stage_pkg[RTCW_FX_MAX_STAGES],*stage_status[RTCW_FX_MAX_STAGES];
    rtcw_vfs_file image;
    rtcw_fx fx;
    map=fopen(mapfile,"wb");if(!map)return fail("cannot create material map");
    prov=fopen(provenance,"wb");if(!prov){fclose(map);return fail("cannot create provenance report");}
    if(fputs("surface\tmaterial\timage\tshader_definition\twinning_pk3\tstatus\ttime_ms",prov)==EOF)result=0;
    for(j=1U;j<RTCW_FX_MAX_STAGES&&result;j++){
        if(fprintf(prov,"\tstage%u_image\tstage%u_pk3\tstage%u_status",j,j,j)<0)result=0;
    }
    if(result&&fputc('\n',prov)==EOF)result=0;
    for(i=0U;result&&i<rtcw_current.surface_count;i++){
        const char *mat=rtcw_current.surfaces[i].shader;
        const char *status="missing",*winning="-";
        src[0]=0;ref[0]=0;
        for(j=0U;j<RTCW_FX_MAX_STAGES;j++){
            stage_path[j][0]=0;stage_src[j][0]=0;
            stage_pkg[j]="-";stage_status[j]="not_used";
        }
        if(!safe(mat)){result=0;break;}
        rc=shader_ref(dir,mat,preferred,time_ms,ref,origin);
        if(rc<0){result=0;break;}
        if(!rc){strcpy(ref,mat);strcpy(origin,"-");}
        strcpy(shader_local,"-");
        if(rc==1){
            unsigned int c;
            if(strcmp(origin,"explicit_local_shader")==0&&preferred&&
               strlen(preferred)<VM_PATH){strcpy(shader_local,preferred);}
            else for(c=0U;c<scount;c++){
                if(strcmp(scache[c].material,mat)==0&&strcmp(scache[c].dir,dir)==0){
                    strcpy(shader_local,scache[c].file);break;
                }
            }
        }
        memset(&fx,0,sizeof(fx));fxrc=0;
        if(strcmp(shader_local,"-")!=0){
            fxrc=rtcw_fx_lookup(shader_local,mat,time_ms,&fx);
            if(fxrc<0){result=0;break;}
        }
        if(lookup_image(ref,&image,src)){
            sprintf(leaf,"vfs_tex_%02u%s",i,suffix(src,".tga")?".tga": ".jpg");
            if(!compose(dir,leaf,dst)){result=0;break;}
            if(strcmp(prior_dir,dir)!=0 || strcmp(prior_src[i][0],src)!=0 ||
               prior_mount[i][0]!=image.mount){
                if(!rtcw_vfs_extract(src,dst)){result=0;break;}
                rtcw_texcache_invalidate(dst);
                strcpy(prior_dir,dir);strcpy(prior_src[i][0],src);
                prior_mount[i][0]=image.mount;extracted++;
            }else reused++;
            status="found";winning=rtcw_vfs_mount_name(image.mount);
        }else strcpy(dst,"-");
        for(j=1U;j<RTCW_FX_MAX_STAGES;j++){
            if(fxrc!=1||j>=fx.stages)continue;
            stage_status[j]="missing";
            if(!fx.stage[j].map[0]){stage_status[j]="unsupported";continue;}
            if(!safe(fx.stage[j].map)){result=0;break;}
            if(lookup_image(fx.stage[j].map,&image,stage_src[j])){
                sprintf(leaf,"vfs_fx_%02u_%u%s",i,j,
                        suffix(stage_src[j],".tga")?".tga":".jpg");
                if(!compose(dir,leaf,stage_path[j])){result=0;break;}
                if(strcmp(prior_dir,dir)!=0||strcmp(prior_src[i][j],stage_src[j])!=0||
                   prior_mount[i][j]!=image.mount){
                    if(!rtcw_vfs_extract(stage_src[j],stage_path[j])){result=0;break;}
                    rtcw_texcache_invalidate(stage_path[j]);
                    strcpy(prior_dir,dir);strcpy(prior_src[i][j],stage_src[j]);
                    prior_mount[i][j]=image.mount;extracted++;
                }else reused++;
                stage_pkg[j]=rtcw_vfs_mount_name(image.mount);
                stage_status[j]="found";
            }
        }
        if(!result)break;
        if(fprintf(map,"%u\t%s\t%s\t%s",i,mat,dst,shader_local)<0||
           fprintf(prov,"%u\t%s\t%s\t%s\t%s\t%s\t%lu",i,mat,
                   src[0]?src:"-",origin,winning,status,time_ms)<0){result=0;break;}
        for(j=1U;j<RTCW_FX_MAX_STAGES;j++){
            if(fprintf(map,"\t%s",stage_path[j][0]?stage_path[j]:"-")<0||
               fprintf(prov,"\t%s\t%s\t%s",stage_src[j][0]?stage_src[j]:"-",
                       stage_pkg[j],stage_status[j])<0){result=0;break;}
        }
        if(!result||fputc('\n',map)==EOF||fputc('\n',prov)==EOF){result=0;break;}
    }
    if(fclose(map)!=0)result=0;
    if(fclose(prov)!=0)result=0;
    if(!result)return fail("material staging failed (invalid shader/image, IO or capacity)");
    strcpy(vm_err,"OK");return 1;
}

int rtcw_vfs_material_stage(const char *dir,const char *mapfile,const char *provenance,
                            const char *preferred){
    return rtcw_vfs_material_stage_time(dir,mapfile,provenance,preferred,0UL);
}
