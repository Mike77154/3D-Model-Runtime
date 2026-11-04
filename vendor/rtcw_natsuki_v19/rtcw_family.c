/* RTCW family bundler: inferred same-directory files and explicit material paths.
   Original CC0 implementation, ISO C89, fixed arrays, no heap / floats / stdin.
   Only for trusted destination parent and an initially nonexistent output folder. */
#include "rtcw_family.h"
#include "rtcw_pk3.h"
#include "rtcw_vfs.h"
#include "rtcw_codec.h"
#include "rtcw_texrender.h"
#include "rtcw_clipcfg.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir((p),0755)
#endif
#define F_MAX_SIBLING 384U
#define F_MAX_SHADER 512U
#define F_MAX_MATERIAL 1024U
#define F_MAX_TEXTURE 4096U
#define F_MAX_SKIN 192U
#define F_PATH 1024U
#define F_SCRIPT (1024UL*1024UL)
typedef struct f_req_s {char name[241];int seen,explicit_path;} f_req;
static char f_error[192]="OK";
static char f_root[F_PATH],f_dir[241],f_target[241];
static char f_siblings[F_MAX_SIBLING][241];
static char f_skins[F_MAX_SKIN][241];
static char f_cfgs[F_MAX_SKIN][241];
static unsigned int f_ncfg;
static char f_shaders[F_MAX_SHADER][241];
static unsigned int f_nsib,f_nskin,f_nshader,f_nmats,f_ntex;
static f_req f_materials[F_MAX_MATERIAL],f_textures[F_MAX_TEXTURE];
static char f_src[F_SCRIPT+1UL];
static FILE *f_manifest,*f_resolved;
static unsigned int f_shader_matches,f_textures_found,f_failed_models;

/* Source abstraction: legacy single PK3 or mounted VFS winner list. */
static int f_source_vfs=0;
static unsigned long f_vfs_cursor=0UL;
static const char *f_source_error(void){return f_source_vfs?rtcw_vfs_error():rtcw_pk3_error();}
static int f_source_open(const char *source){
    if(f_source_vfs){(void)source;f_vfs_cursor=0UL;return 1;}
    return rtcw_pk3_open(source);
}
static void f_source_close(void){if(!f_source_vfs)rtcw_pk3_close();}
static int f_source_next(rtcw_pk3_entry *e){
    if(f_source_vfs){
        rtcw_vfs_file found;
        if(f_vfs_cursor>=rtcw_vfs_count())return 0;
        if(!rtcw_vfs_at(f_vfs_cursor++,&found))return -1;
        *e=found.entry;return 1;
    }
    return rtcw_pk3_next(e);
}
static int f_source_extract(const rtcw_pk3_entry *e,const char *dst){
    if(f_source_vfs)return rtcw_vfs_extract(e->name,dst);
    return rtcw_pk3_extract(e,dst);
}

const char *rtcw_family_error(void){return f_error;}
static int failure(const char *s){strncpy(f_error,s,sizeof(f_error)-1U);f_error[sizeof(f_error)-1U]=0;return 0;}
static int folded(int c){return (c>='A'&&c<='Z')?c+32:c;}
static int same(const char *a,const char *b){
    while(*a&&*b){if(folded((unsigned char)*a)!=folded((unsigned char)*b))return 0;a++;b++;}
    return *a==0&&*b==0;
}
static int ends(const char *s,const char *suffix){
    size_t a=strlen(s),b=strlen(suffix);return a>=b&&same(s+a-b,suffix);
}
static int is_model(const char *p){return ends(p,".md3")||ends(p,".mdc")||ends(p,".mds");}
static int is_skin(const char *p){return ends(p,".skin");}
static int is_cfg(const char *p){return ends(p,".cfg");}
static int is_shader(const char *p){return ends(p,".shader");}
static int is_image(const char *p){return ends(p,".tga")||ends(p,".jpg")||ends(p,".jpeg")||ends(p,".pcx");}
static int relative_dir(const char *s,char *out){
    const char *p=strrchr(s,'/');size_t n=p?(size_t)(p-s)+1U:0U;
    if(n>240U)return 0;
    memcpy(out,s,n);out[n]=0;return 1;
}
static int under_dir(const char *p){
    size_t n=strlen(f_dir);
    if(n){
        size_t i;
        if(strlen(p)<n)return 0;
        for(i=0;i<n;i++)if(folded((unsigned char)p[i])!=folded((unsigned char)f_dir[i]))return 0;
        return 1;
    }
    return strchr(p,'/')==NULL;
}
static int make_path(const char *relative,char *out){
    size_t a=strlen(f_root),b=strlen(relative);
    if(!rtcw_pk3_safe_path(relative)||a+b+2U>=F_PATH)return 0;
    memcpy(out,f_root,a);
    if(a&&out[a-1]!='/'&&out[a-1]!='\\')out[a++]='/';
    memcpy(out+a,relative,b+1U);return 1;
}
static int make_parent(const char *target){
    char buf[F_PATH];size_t i,n=strlen(target);
    if(n>=F_PATH)return 0;
    memcpy(buf,target,n+1U);
    /* The newly created destination directory must not contain symlinks. */
    for(i=strlen(f_root)+1U;i<n;i++)if(buf[i]=='/'){
        buf[i]=0;
        if(MAKE_DIR(buf)!=0&&errno!=EEXIST)return 0;
        buf[i]='/';
    }
    return 1;
}
static int append(char arr[][241],unsigned int *count,unsigned int cap,const char *s){
    if(*count>=cap)return 0;
    strcpy(arr[(*count)++],s);return 1;
}
static int add_req(f_req *arr,unsigned int *count,unsigned int cap,const char *s,int explicit_path){
    unsigned int i;size_t n;
    if(!s||!s[0]||s[0]=='$'||s[0]=='*'||!rtcw_pk3_safe_path(s))return 1;
    n=strlen(s);if(n>240U)return 1;
    for(i=0;i<*count;i++)if(same(arr[i].name,s)){
        if(explicit_path)arr[i].explicit_path=1;
        return 1;
    }
    if(*count>=cap)return failure("material/texture request capacity exceeded");
    strcpy(arr[*count].name,s);arr[*count].seen=0;
    arr[*count].explicit_path=explicit_path;
    (*count)++;return 1;
}
static int texture_ref(const char *name,int explicit_path){
    char namebuf[241];size_t n;const char *exts[]={".tga",".jpg",".jpeg",".pcx"};
    unsigned int i;
    if(!name||!name[0]||name[0]=='*'||name[0]=='$')return 1;
    if(is_image(name))return add_req(f_textures,&f_ntex,F_MAX_TEXTURE,name,explicit_path);
    n=strlen(name);
    if(n==0U||n+6U>=sizeof(namebuf))return 1;
    for(i=0;i<4U;i++){
        strcpy(namebuf,name);strcat(namebuf,exts[i]);
        if(!add_req(f_textures,&f_ntex,F_MAX_TEXTURE,namebuf,0))return 0;
    }
    return 1;
}
static int material_ref(const char *name){
    if(!add_req(f_materials,&f_nmats,F_MAX_MATERIAL,name,1))return 0;
    return texture_ref(name,0);
}
static char *trim(char *s){
    char *p;
    while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')s++;
    p=s+strlen(s);
    while(p>s&&(p[-1]==' '||p[-1]=='\t'||p[-1]=='\r'||p[-1]=='\n'))*--p=0;
    return s;
}
static int skin_materials(const char *relative){
    char path[F_PATH],line[1024],*comma,*start,*end;FILE *in;int ch;
    if(!make_path(relative,path))return failure("skin path overflow");
    in=fopen(path,"rb");if(!in)return failure("cannot read extracted skin");
    while(fgets(line,sizeof(line),in)){
        size_t len=strlen(line);
        if(len && line[len-1]!='\n' && !feof(in)){
            while((ch=fgetc(in))!='\n'&&ch!=EOF){}
            fclose(in);return failure("skin line too long");
        }
        start=trim(line);
        if(!*start||*start=='#'||(start[0]=='/'&&start[1]=='/'))continue;
        comma=strchr(start,',');if(!comma)continue;
        *comma++=0;end=trim(comma);
        if(strlen(end)<=240U && !material_ref(end)){fclose(in);return 0;}
    }
    if(ferror(in)){fclose(in);return failure("skin read failed");}
    fclose(in);return 1;
}
/* Lexer provides newlines to delimit animMap image sequences. */
static int script_token(const char **pp,char *out,unsigned int cap){
    const char *p=*pp;unsigned int n=0;int quotes=0;
    for(;;){
        while(*p==' '||*p=='\t'||*p=='\r')p++;
        if(p[0]=='/'&&p[1]=='/'){
            p+=2;while(*p&&*p!='\n')p++;continue;
        }
        if(p[0]=='/'&&p[1]=='*'){
            p+=2;while(*p&&!(p[0]=='*'&&p[1]=='/'))p++;
            if(!*p)return -1;
            p+=2;continue;
        }
        break;
    }
    if(!*p){*pp=p;return 0;}
    if(*p=='\n'||*p=='{'||*p=='}'){
        out[0]=*p++;out[1]=0;*pp=p;return 1;
    }
    if(*p=='"'){p++;quotes=1;}
    while(*p){
        if(quotes){if(*p=='"'){p++;break;}}
        else if(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'||*p=='{'||*p=='}')break;
        if(n+1U>=cap)return -1;
        out[n++]=*p++;
    }
    if(!n)return -1;
    out[n]=0;*pp=p;return 1;
}
static int material_known(const char *p){
    unsigned int i;for(i=0;i<f_nmats;i++)if(same(f_materials[i].name,p))return 1;
    return 0;
}
static int map_candidate(const char *map,const char *mat,int first){
    if(!map[0]||map[0]=='$'||map[0]=='*')return 1;
    if(!rtcw_pk3_safe_path(map))return 1;
    if(!texture_ref(map,1))return 0;
    /* The standalone material resolver supports one diffuse map per shader. */
    if(first && f_resolved){
        if(fprintf(f_resolved,"%s { { map %s } }\n",mat,map)<0)
            return failure("cannot write generated shader");
    }
    return 1;
}
static int parse_shader(const char *name){
    char pth[F_PATH],tok[256],prev[256],header[256],w[256];
    FILE *f;size_t n;const char *p;int rc,depth=0,match=0,have_map=0,animations=0;
    if(!make_path(name,pth))return failure("shader path too long");
    f=fopen(pth,"rb");if(!f)return failure("cannot open extracted shader");
    n=fread(f_src,1,F_SCRIPT+1UL,f);
    if(ferror(f)){fclose(f);return failure("shader read error");}
    fclose(f);
    if(n>F_SCRIPT){fprintf(f_manifest,"skipped_large_shader\t%s\t>1MiB\n",name);return 1;}
    f_src[n]=0;p=f_src;prev[0]=0;header[0]=0;
    while((rc=script_token(&p,tok,sizeof(tok)))>0){
        if(!strcmp(tok,"{")){
            if(depth==0){
                strcpy(header,prev);match=material_known(header);have_map=0;
                if(match){
                    unsigned int mi;
                    f_shader_matches++;
                    for(mi=0;mi<f_nmats;mi++)if(same(f_materials[mi].name,header))f_materials[mi].seen=1;
                    fprintf(f_manifest,"shader_match\t%s\t%s\n",name,header);
                }
            }
            depth++;
        }else if(!strcmp(tok,"}")){
            if(depth>0)depth--;
            if(depth==0)match=0;
        }else if(!strcmp(tok,"\n")){
            /* Preserve the most recent non-newline word for shader headers. */
        }else if(match&&depth>=2&&
                (same(tok,"map")||same(tok,"clampmap")||same(tok,"animMap"))){
            animations=same(tok,"animMap");
            if(animations){
                rc=script_token(&p,w,sizeof(w));
                if(rc<=0||!strcmp(w,"\n")||!strcmp(w,"}"))continue;
            }
            for(;;){
                rc=script_token(&p,w,sizeof(w));
                if(rc<=0||!strcmp(w,"\n")||!strcmp(w,"{")||!strcmp(w,"}")){
                    /* A block delimiter after a map directive must not be lost. */
                    if(rc>0 && (*w=='{'||*w=='}'))p--;
                    break;
                }
                if(!map_candidate(w,header,!have_map))return 0;
                if(w[0]!='$'&&w[0]!='*')have_map=1;
                if(!animations)break;
            }
        }else if(depth==0)strcpy(prev,tok);
    }
    if(rc<0){fprintf(f_manifest,"skipped_shader_parse\t%s\toversized_token\n",name);}
    return 1;
}
static int family_index_mode(const char *archive,const char *report,int vfs){
    FILE *out;rtcw_pk3_entry e;int ret;unsigned long models=0;
    f_source_vfs=vfs;
    if(vfs && !rtcw_vfs_mounts(archive))return failure(rtcw_vfs_error());
    out=fopen(report,"wb");if(!out)return failure("cannot create index");
    if(!f_source_open(archive)){fclose(out);return failure(f_source_error());}
    fprintf(out,"type\tmodel\tfamily_directory\tbytes\n");
    while((ret=f_source_next(&e))>0){
        char d[241];
        if(!e.is_directory&&is_model(e.name)){
            if(!relative_dir(e.name,d)){ret=-1;break;}
            fprintf(out,"model\t%s\t%s\t%lu\n",e.name,d,e.uncompressed_size);
            models++;
        }
    }
    if(ret<0){f_source_close();fclose(out);return failure(f_source_error());}
    f_source_close();
    if(fclose(out)!=0)return failure("index write error");
    printf("OK index models=%lu report=%s\n",models,report);return 1;
}
int rtcw_family_index(const char *archive,const char *report){
    return family_index_mode(archive,report,0);
}
int rtcw_family_index_vfs(const char *list,const char *report){
    return family_index_mode(list,report,1);
}
static int extraction(const rtcw_pk3_entry *e,const char *why){
    char path[F_PATH];
    if(!make_path(e->name,path)||!make_parent(path))return failure("cannot prepare output path");
    if(!f_source_extract(e,path))return failure(f_source_error());
    if(f_source_vfs){
        rtcw_vfs_file where;
        if(!rtcw_vfs_lookup(e->name,&where))return failure(rtcw_vfs_error());
        if(fprintf(f_manifest,"source_archive\t%s\t%s\n",e->name,rtcw_vfs_mount_name(where.mount))<0)
            return failure("manifest archive write failed");
    }
    if(fprintf(f_manifest,"%s\t%s\t%s\n",why,e->name,"extracted")<0)
        return failure("manifest write failed");
    return 1;
}
static int directory_approved(const char *p){
    return under_dir(p)&&(is_model(p)||is_skin(p)||is_cfg(p));
}
static int collect_surfaces(const char *relative,int required){
    char path[F_PATH];unsigned int i;
    if(!make_path(relative,path))return failure("model path overflow");
    if(!rtcw_open(path)||!rtcw_decode(0UL)){
        if(required)return failure(rtcw_error());
        f_failed_models++;
        fprintf(f_manifest,"skipped_invalid_sibling_model\t%s\t%s\n",relative,rtcw_error());
        return 1;
    }
    for(i=0;i<rtcw_current.surface_count;i++){
        if(!material_ref(rtcw_current.surfaces[i].shader))return 0;
    }
    return 1;
}
static int selected_skin(char *out){
    unsigned int i;const char *p;
    if(!f_nskin)return 0;
    for(i=0;i<f_nskin;i++){
        p=strrchr(f_skins[i],'/');p=p?p+1:f_skins[i];
        if(same(p,"default.skin")){strcpy(out,f_skins[i]);return 1;}
    }
    if(f_nskin==1U){strcpy(out,f_skins[0]);return 1;}
    return 0; /* ambiguity: no arbitrary skin applied to preview */
}
static int family_bundle_mode(const char *archive,const char *model,const char *dest,int vfs){
    rtcw_pk3_entry e;int rc,got=0;unsigned int i,missing=0;char path[F_PATH],preview_model[F_PATH];
    char chosen_skin[241],outpath[F_PATH],root_rep[F_PATH],root_obj[F_PATH];
    char root_wire[F_PATH],root_tex[F_PATH],root_shader[F_PATH];
    FILE *probe;
    f_source_vfs=vfs;
    if(vfs && !rtcw_vfs_mounts(archive))return failure(rtcw_vfs_error());
    if(!model||!is_model(model)||!rtcw_pk3_safe_path(model))return failure("invalid model PK3 path");
    if(!dest||!dest[0]||strlen(dest)>650U)return failure("invalid output directory");
    if(strchr(dest,'\n')||strchr(dest,'\t'))return failure("invalid output directory characters");
    probe=fopen(dest,"rb");if(probe){fclose(probe);return failure("output folder already exists; use a NEW directory");}
    if(MAKE_DIR(dest)!=0)return failure("output folder already exists or cannot be created");
    strcpy(f_root,dest);strcpy(f_target,model);
    if(!relative_dir(model,f_dir))return failure("cannot derive family folder");
    f_nsib=f_nskin=f_ncfg=f_nshader=f_nmats=f_ntex=0;
    f_shader_matches=f_textures_found=f_failed_models=0;
    strcpy(outpath,f_root);strcat(outpath,"/manifest.tsv");
    f_manifest=fopen(outpath,"wb");if(!f_manifest)return failure("cannot create manifest");
    fprintf(f_manifest,"relationship\tpath\tdetail\n");
    if(!f_source_open(archive))return failure(f_source_error());
    while((rc=f_source_next(&e))>0){
        if(!e.is_directory&&same(e.name,model)){
            got++;
            if(strcmp(e.name,model)!=0)return failure("case-mismatched model path; use exact PK3 spelling");
        }
    }
    if(rc<0)return failure(f_source_error());
    f_source_close();
    if(got!=1)return failure(got?"ambiguous duplicate model":"model not found in archive");
    if(!f_source_open(archive))return failure(f_source_error());
    while((rc=f_source_next(&e))>0){
        if(e.is_directory)continue;
        if(directory_approved(e.name)){
            if(is_model(e.name)){
                if(!append(f_siblings,&f_nsib,F_MAX_SIBLING,e.name))return failure("too many family models");
            }else if(is_skin(e.name)){
                if(!append(f_skins,&f_nskin,F_MAX_SKIN,e.name))return failure("too many family skins");
            }else if(is_cfg(e.name)){
                if(!append(f_cfgs,&f_ncfg,F_MAX_SKIN,e.name))return failure("too many family CFGs");
            }
            if(!extraction(&e,same(e.name,model)?"selected_model":"same_directory_candidate"))return 0;
        }else if(is_shader(e.name)){
            if(!append(f_shaders,&f_nshader,F_MAX_SHADER,e.name))return failure("too many shader scripts");
            if(!extraction(&e,"shader_script_for_resolution"))return 0;
        }
    }
    if(rc<0)return failure(f_source_error());
    f_source_close();
    if(!make_path(model,preview_model))return failure("model path too long");
    for(i=0;i<f_nsib;i++)if(!collect_surfaces(f_siblings[i],same(f_siblings[i],model)))return 0;
    for(i=0;i<f_nskin;i++)if(!skin_materials(f_skins[i]))return 0;
    {
        FILE *cliptable;unsigned long rows=0UL;
        strcpy(outpath,f_root);strcat(outpath,"/clip_ranges.tsv");
        cliptable=fopen(outpath,"wb");
        if(!cliptable)return failure("cannot create clip range index");
        fprintf(cliptable,"source_cfg\tline\tfirst_frame\tframe_count\tloop_count\tfps\tconfidence\n");
        for(i=0;i<f_ncfg;i++){
            unsigned long before=rows;
            if(!make_path(f_cfgs[i],path)||!rtcw_clipcfg_append(path,f_cfgs[i],cliptable,&rows)){
                fclose(cliptable);return failure("CFG candidate range parse failed");
            }
            fprintf(f_manifest,"cfg_numeric_frame_rows\t%s\t%lu\n",f_cfgs[i],rows-before);
        }
        if(fclose(cliptable)!=0)return failure("clip range index write failed");
    }
    strcpy(root_shader,f_root);strcat(root_shader,"/resolved_firststage.shader");
    f_resolved=fopen(root_shader,"wb");if(!f_resolved)return failure("cannot write resolved shader");
    for(i=0;i<f_nshader;i++)if(!parse_shader(f_shaders[i]))return 0;
    if(fclose(f_resolved)!=0)return failure("cannot finalize resolved shader");
    f_resolved=NULL;
    if(!f_source_open(archive))return failure(f_source_error());
    while((rc=f_source_next(&e))>0){
        unsigned int j;
        if(e.is_directory||!is_image(e.name))continue;
        for(j=0;j<f_ntex;j++)if(same(f_textures[j].name,e.name)){
            if(f_textures[j].seen)return failure("duplicate texture entry name");
            if(!extraction(&e,"material_texture"))return 0;
            f_textures[j].seen=1;f_textures_found++;
            break;
        }
    }
    if(rc<0)return failure(f_source_error());
    f_source_close();
    for(i=0;i<f_ntex;i++)if(!f_textures[i].seen&&f_textures[i].explicit_path){
        fprintf(f_manifest,"missing_referenced_texture\t%s\tnot_in_this_pk3\n",f_textures[i].name);
        missing++;
    }
    for(i=0;i<f_nmats;i++){
        fprintf(f_manifest,"material_name\t%s\t%s\n",f_materials[i].name,
                f_materials[i].seen?"shader_definition_found":"no_matching_shader;_direct_texture_possible");
    }
    if(fclose(f_manifest)!=0)return failure("manifest write error");
    f_manifest=NULL;
    strcpy(root_obj,f_root);strcat(root_obj,"/selected.obj");
    strcpy(root_wire,f_root);strcat(root_wire,"/selected_wire.ppm");
    strcpy(root_rep,f_root);strcat(root_rep,"/selected_report.txt");
    strcpy(root_tex,f_root);strcat(root_tex,"/selected_texture.ppm");
    if(!rtcw_open(preview_model)||!rtcw_decode(0))return failure(rtcw_error());
    if(selected_skin(chosen_skin)){
        if(!make_path(chosen_skin,path))return failure("selected skin path invalid");
        if(!rtcw_apply_skin(path))return failure("selected skin could not be applied");
    }
    if(!rtcw_save_obj(root_obj)||!rtcw_save_ppm(root_wire)||!rtcw_report(root_rep))
        return failure("cannot create geometry preview or report");
    if(!rtcw_render_textured(root_tex,f_root,root_shader))
        return failure(rtcw_render_error());
    printf("OK bundle selected=%s sibling_models=%u skins=%u scripts=%u shader_matches=%u textures=%u missing_explicit=%u skipped_peers=%u\n",
           model,f_nsib,f_nskin,f_nshader,f_shader_matches,f_textures_found,missing,f_failed_models);
    return 1;
}

int rtcw_family_bundle(const char *archive,const char *model,const char *dest){
    return family_bundle_mode(archive,model,dest,0);
}
int rtcw_family_bundle_vfs(const char *list,const char *model,const char *dest){
    return family_bundle_mode(list,model,dest,1);
}
