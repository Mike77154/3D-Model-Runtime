/* C89 command-line cross-reference of wolfanim.script with wolfanim.cfg and model.
   Usage: rtcw_script catalog model cfg script output.tsv
          rtcw_script catalog-vfs mounts.txt models/players/name/body.mds NEW_DIR */
#include "rtcw_animscript.h"
#include "rtcw_clipbank.h"
#include "rtcw_codec.h"
#include "rtcw_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p,0755)
#endif
#define MAX_PATH_LOCAL 720U
static char reason[180]="OK";
static int fail(const char *s){strncpy(reason,s,sizeof(reason)-1U);reason[sizeof(reason)-1U]=0;return 0;}
static int join(const char *folder,const char *leaf,char *path){
    size_t a=strlen(folder),b=strlen(leaf);
    if(!a||a+b+2U>MAX_PATH_LOCAL)return fail("output path too long");
    strcpy(path,folder);if(path[a-1U]!='/'&&path[a-1U]!='\\')strcat(path,"/");
    strcat(path,leaf);return 1;
}
static int newdir(const char *name){
    FILE *f;if(!name||!*name||strlen(name)>580U||strchr(name,'\t')||strchr(name,'\n'))return fail("invalid output folder");
    f=fopen(name,"rb");if(f){fclose(f);return fail("output directory already exists");}
    if(MAKE_DIR(name)!=0)return fail("unable to create new output directory");
    return 1;
}
static int report(const char *model,const char *cfg,const char *script,const char *output){
    if(!rtcw_open(model)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!rtcw_clipbank_read(cfg,rtcw_current.frames))return fail(rtcw_clipbank_error());
    if(!rtcw_animscript_read(script))return fail(rtcw_animscript_error());
    if(!rtcw_animscript_report(output))return fail(rtcw_animscript_error());
    printf("OK script_commands=%u unresolved_references=%u clip_bank=%u\n",
        rtcw_animscript_count(),rtcw_animscript_unresolved(),rtcw_clipbank_count());
    return 1;
}
static int suffix(const char *s,const char *end){size_t a=strlen(s),b=strlen(end),j;
    if(a<b)return 0;
    for(j=0U;j<b;j++){
        int x=(unsigned char)s[a-b+j],y=(unsigned char)end[j];
        if(x>='A'&&x<='Z')x+=32;
        if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }return 1;
}
static int vfs_job(const char *mounts,const char *model,const char *output){
    char cfg[244],script[244],mfile[MAX_PATH_LOCAL],cfile[MAX_PATH_LOCAL],sfile[MAX_PATH_LOCAL],table[MAX_PATH_LOCAL],src[MAX_PATH_LOCAL];
    const char *slash=strrchr(model,'/');size_t base=slash?(size_t)(slash-model)+1U:0U;
    rtcw_vfs_file m,c,s;FILE *f;
    if(!suffix(model,".mds")&&!suffix(model,".mdc")&&!suffix(model,".md3"))return fail("expected an MD3, MDC or MDS model path");
    if(!rtcw_vfs_mounts(mounts))return fail(rtcw_vfs_error());
    if(base+strlen("wolfanim.script")+1U>sizeof(script))return fail("virtual resource path too long");
    memcpy(script,model,base);strcpy(script+base,"wolfanim.script");
    memcpy(cfg,model,base);strcpy(cfg+base,"wolfanim.cfg");
    if(!rtcw_vfs_lookup(model,&m))return fail("model absent from mounted PK3");
    if(!rtcw_vfs_lookup(script,&s))return fail("no wolfanim.script in model directory; use explicit local catalog");
    if(!rtcw_vfs_lookup(cfg,&c)){
        memcpy(cfg,model,base);strcpy(cfg+base,"animation.cfg");
        if(!rtcw_vfs_lookup(cfg,&c))return fail("no wolfanim.cfg / animation.cfg in model directory");
    }
    if(!newdir(output))return 0;
    if(!join(output,"model.bin",mfile)||!join(output,"wolfanim.cfg",cfile)||
       !join(output,"wolfanim.script",sfile)||!join(output,"graph.tsv",table)||!join(output,"sources.tsv",src))return 0;
    if(!rtcw_vfs_extract(m.entry.name,mfile)||!rtcw_vfs_extract(c.entry.name,cfile)||!rtcw_vfs_extract(s.entry.name,sfile))return fail(rtcw_vfs_error());
    if(!report(mfile,cfile,sfile,table))return 0;
    f=fopen(src,"wb");if(!f)return fail("cannot write VFS provenance");
    if(fprintf(f,"role\tvirtual_path\twinning_archive\tassociation\nmodel\t%s\t%s\trequested\ncfg\t%s\t%s\tsame_directory_candidate\nscript\t%s\t%s\tsame_directory_candidate\n",
               m.entry.name,rtcw_vfs_mount_name(m.mount),c.entry.name,rtcw_vfs_mount_name(c.mount),
               s.entry.name,rtcw_vfs_mount_name(s.mount))<0){fclose(f);return fail("provenance write failed");}
    if(fclose(f)!=0)return fail("provenance close failed");
    return 1;
}
static void usage(void){fprintf(stderr,"RTCW script graph v0.10 / C89\n rtcw_script catalog model.mds wolfanim.cfg wolfanim.script graph.tsv\n rtcw_script catalog-vfs mounts.txt models/players/name/body.mds NEW_directory\n");}
int main(int argc,char **argv){int ok=0;
    if(argc==6&&strcmp(argv[1],"catalog")==0)ok=report(argv[2],argv[3],argv[4],argv[5]);
    else if(argc==5&&strcmp(argv[1],"catalog-vfs")==0)ok=vfs_job(argv[2],argv[3],argv[4]);
    else{usage();return 2;}
    if(!ok){fprintf(stderr,"RTCW script: %s\n",reason);return 1;}return 0;
}
