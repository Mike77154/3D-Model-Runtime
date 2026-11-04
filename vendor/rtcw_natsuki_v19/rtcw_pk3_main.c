/* List/catalog/unpack RTCW PK3. No filesystem access except explicit paths.
   Unpack only extracts relevant asset kinds. Portable C89 + mkdir OS headers. */
#include "rtcw_pk3.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir((p),0755)
#endif

static int fold(int c){return c>='A'&&c<='Z'?c+32:c;}
static int suffix(const char *s,const char *ext){
    size_t n=strlen(s),m=strlen(ext),i;
    if(n<m)return 0;
    for(i=0;i<m;i++)if(fold((unsigned char)s[n-m+i])!=fold((unsigned char)ext[i]))return 0;
    return 1;
}
static const char *kind(const char *p){
    if(suffix(p,".mds")||suffix(p,".mdc")||suffix(p,".md3"))return "model";
    if(suffix(p,".skin"))return "skin";
    if(suffix(p,".shader"))return "shader";
    if(suffix(p,".tga")||suffix(p,".jpg")||suffix(p,".jpeg")||suffix(p,".pcx"))return "texture";
    if(suffix(p,".cfg"))return "config";
    return "other";
}
static int selected(const char *name,const char *mode){
    const char *k=kind(name);
    if(strcmp(mode,"all")==0)return strcmp(k,"other")!=0;
    if(strcmp(mode,"models")==0)return strcmp(k,"model")==0||strcmp(k,"skin")==0||strcmp(k,"config")==0;
    if(strcmp(mode,"textures")==0)return strcmp(k,"texture")==0||strcmp(k,"shader")==0;
    return 0;
}
static int mkparent(const char *dst){
    char tmp[1024];size_t i,n=strlen(dst);
    if(n>=sizeof(tmp))return 0;
    memcpy(tmp,dst,n+1U);
    for(i=0;i<n;i++){
        if(tmp[i]=='/'||tmp[i]=='\\'){
            if(i==0 || (i==2U && tmp[1]==':'))continue;
            tmp[i]=0;
            if(MKDIR(tmp)!=0&&errno!=EEXIST)return 0;
            tmp[i]='/';
        }
    }
    return 1;
}
static void usage(void){
    fprintf(stderr,"Usage:\n"
        "  rtcw_pk3 list archive.pk3 report.tsv\n"
        "  rtcw_pk3 catalog archive.pk3 resources.tsv\n"
        "  rtcw_pk3 extract archive.pk3 internal/path.mds output.mds\n"
        "  rtcw_pk3 unpack archive.pk3 output_dir [all|models|textures]\n"
        "'all' extracts recognized model/material/config assets, not sounds/maps.\n");
}
int main(int argc,char **argv){
    rtcw_pk3_entry ent;FILE *out=NULL,*check=NULL;
    int rc,found=0,errors=0,extracted=0,skipped=0;
    char target[1024],folder[256],*slash;
    const char *cmd,*mode;
    unsigned long entries=0,models=0,skins=0,textures=0,shaders=0;
    if(argc<4){usage();return 2;}
    cmd=argv[1];
    if(strcmp(cmd,"list")&&strcmp(cmd,"catalog")&&strcmp(cmd,"extract")&&strcmp(cmd,"unpack")){usage();return 2;}
    if(!strcmp(cmd,"unpack") && argc>5){usage();return 2;}
    if(!strcmp(cmd,"extract")&&argc!=5){usage();return 2;}
    if((!strcmp(cmd,"catalog")||!strcmp(cmd,"list"))&&argc!=4){usage();return 2;}
    mode=argc==5?argv[4]:"all";
    if(!strcmp(cmd,"unpack")&&strcmp(mode,"all")&&strcmp(mode,"models")&&strcmp(mode,"textures")){
        usage();return 2;
    }
    if(!rtcw_pk3_open(argv[2])){fprintf(stderr,"PK3: %s\n",rtcw_pk3_error());return 1;}
    if(!strcmp(cmd,"list")||!strcmp(cmd,"catalog")){
        out=fopen(argv[3],"wb");
        if(!out){fprintf(stderr,"Cannot write report\n");rtcw_pk3_close();return 1;}
        if(!strcmp(cmd,"list"))fprintf(out,"type\tentry\tuncompressed\tcompressed\tmethod\tcrc32\n");
        else fprintf(out,"type\tgroup_directory\tentry\tuncompressed\tmethod\n");
    }
    while((rc=rtcw_pk3_next(&ent))>0){
        const char *k;
        entries++;
        k=kind(ent.name);
        if(!strcmp(k,"model"))models++;
        if(!strcmp(k,"skin"))skins++;
        if(!strcmp(k,"texture"))textures++;
        if(!strcmp(k,"shader"))shaders++;
        if(out){
            if(!strcmp(cmd,"list")){
                fprintf(out,"%s\t%s\t%lu\t%lu\t%u\t%08lX\n",k,ent.name,ent.uncompressed_size,ent.compressed_size,ent.method,ent.crc32);
            }else{
                size_t n;
                strcpy(folder,ent.name);
                slash=strrchr(folder,'/');if(slash)slash[1]=0;else folder[0]=0;
                n=strlen(folder);(void)n;
                fprintf(out,"%s\t%s\t%s\t%lu\t%u\n",k,folder,ent.name,ent.uncompressed_size,ent.method);
            }
        }else if(!strcmp(cmd,"extract")){
            if(strcmp(ent.name,argv[3])==0){
                found=1;
                if(ent.is_directory||!rtcw_pk3_extract(&ent,argv[4])){
                    fprintf(stderr,"Extract: %s\n",rtcw_pk3_error());errors++;}
                else extracted++;
                break;
            }
        }else if(!ent.is_directory&&selected(ent.name,mode)){
            size_t rootlen=strlen(argv[3]),namelen=strlen(ent.name);
            if(rootlen+namelen+2U>sizeof(target)){
                fprintf(stderr,"Too long output path %s\n",ent.name);errors++;break;
            }
            strcpy(target,argv[3]);
            if(rootlen&&target[rootlen-1U]!='/'&&target[rootlen-1U]!='\\')strcat(target,"/");
            strcat(target,ent.name);
            check=fopen(target,"rb");
            if(check){fclose(check);check=NULL;skipped++;continue;} /* no silent overwrites */
            if(!mkparent(target)||!rtcw_pk3_extract(&ent,target)){
                fprintf(stderr,"Unpack error %s: %s\n",ent.name,rtcw_pk3_error());errors++;
                break;
            }
            extracted++;
        }
    }
    if(rc<0){fprintf(stderr,"PK3 scan: %s\n",rtcw_pk3_error());errors++;}
    if(out && fclose(out)!=0){fprintf(stderr,"Report write error\n");errors++;}
    rtcw_pk3_close();
    if(!strcmp(cmd,"extract")&&!found){fprintf(stderr,"Entry not found: %s\n",argv[3]);errors++;}
    if(!errors){
        printf("OK %s entries=%lu models=%lu skins=%lu textures=%lu shaders=%lu extracted=%d skipped=%d\n",
            cmd,entries,models,skins,textures,shaders,extracted,skipped);
    }
    return errors?1:0;
}
