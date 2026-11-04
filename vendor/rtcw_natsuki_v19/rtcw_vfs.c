/* Mount list VFS for RTCW PK3s. Independent source-only code, CC0-1.0.
   Strict ISO C89; fixed-capacity storage; no floats, heap or third-party libs.
   Last mount wins (as passed explicitly by caller). Does NOT claim to mimic
   automatic engine discovery / pure-server or loose-file precedence. */
#include "rtcw_vfs.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0755)
#endif
#define HASH_SLOTS 65536UL
#define REPORT_PATH 1024U
#define HISTORY_MAX 65536UL
typedef struct history_s {unsigned long slot;unsigned short oldpack,newpack;} history;
static rtcw_vfs_file files[RTCW_VFS_FILES];
static unsigned long buckets[HASH_SLOTS]; /* 1-based index, 0=empty */
static history history_log[HISTORY_MAX];
static unsigned long file_count=0,history_count=0;
static char mounts[RTCW_VFS_PACKS][RTCW_VFS_ARCHIVE_PATH+1U];
static unsigned int mount_count=0;
static char error_text[192]="not mounted";
static int error(const char *s){strncpy(error_text,s,sizeof(error_text)-1U);error_text[sizeof(error_text)-1U]=0;return 0;}
const char *rtcw_vfs_error(void){return error_text;}
void rtcw_vfs_clear(void){file_count=history_count=0;mount_count=0;memset(buckets,0,sizeof(buckets));strcpy(error_text,"not mounted");}
static unsigned char lowerascii(unsigned char c){return (c>='A'&&c<='Z')?(unsigned char)(c+('a'-'A')):c;}
static int equal_fold(const char *a,const char *b){
    while(*a&&*b){if(lowerascii((unsigned char)*a)!=lowerascii((unsigned char)*b))return 0;a++;b++;}
    return *a==0&&*b==0;
}
static unsigned long hashpath(const char *s){
    unsigned long h=2166136261UL;
    while(*s){h^=(unsigned long)lowerascii((unsigned char)*s++);h=(h*16777619UL)&0xffffffffUL;}
    return h&(HASH_SLOTS-1UL);
}
/* Returns bucket for matching or free path; supports all legal max paths. */
static unsigned long locate(const char *s){
    unsigned long pos=hashpath(s),tries=0;
    while(tries++<HASH_SLOTS){
        if(!buckets[pos]||equal_fold(files[buckets[pos]-1UL].entry.name,s))return pos;
        pos=(pos+1UL)&(HASH_SLOTS-1UL);
    }
    return HASH_SLOTS;
}
static int record_entry(const rtcw_pk3_entry *entry,unsigned int pack){
    unsigned long slot=locate(entry->name),idx;
    if(slot==HASH_SLOTS)return error("VFS hash table capacity exceeded");
    if(buckets[slot]){
        idx=buckets[slot]-1UL;
        if(files[idx].mount==pack)return error("ambiguous duplicate path within a single PK3");
        if(history_count>=HISTORY_MAX)return error("VFS collision history capacity exceeded");
        history_log[history_count].slot=idx;
        history_log[history_count].oldpack=(unsigned short)files[idx].mount;
        history_log[history_count].newpack=(unsigned short)pack;
        history_count++;
        files[idx].entry=*entry;
        files[idx].mount=pack;
        files[idx].overrides++;
        return 1;
    }
    if(file_count>=RTCW_VFS_FILES)return error("VFS distinct file capacity exceeded");
    idx=file_count++;
    buckets[slot]=idx+1UL;
    files[idx].entry=*entry;
    files[idx].mount=pack;
    files[idx].overrides=0U;
    return 1;
}
static char *trim(char *s){
    char *e;while(*s==' '||*s=='\t')s++;
    e=s+strlen(s);while(e>s&&(e[-1]==' '||e[-1]=='\t'||e[-1]=='\r'||e[-1]=='\n'))*--e=0;
    return s;
}
int rtcw_vfs_mounts(const char *manifest){
    FILE *input;char line[RTCW_VFS_ARCHIVE_PATH+8U],*path;
    int rc,ch;rtcw_pk3_entry entry;size_t len;
    rtcw_vfs_clear();
    input=fopen(manifest,"rb");if(!input)return error("cannot read mount list");
    while(fgets(line,sizeof(line),input)){
        len=strlen(line);
        if(len&&line[len-1]!='\n'&&!feof(input)){
            while((ch=fgetc(input))!=EOF&&ch!='\n'){}
            fclose(input);return error("overlong mount-list line");
        }
        path=trim(line);
        if(!*path||*path=='#'||(path[0]=='/'&&path[1]=='/'))continue;
        if(strlen(path)>RTCW_VFS_ARCHIVE_PATH){fclose(input);return error("mount path too long");}
        if(mount_count>=RTCW_VFS_PACKS){fclose(input);return error("too many mounted archives");}
        /* Mount order is exactly the line order. Never depend on directory enumeration. */
        strcpy(mounts[mount_count],path);
        if(!rtcw_pk3_open(path)){fclose(input);return error(rtcw_pk3_error());}
        while((rc=rtcw_pk3_next(&entry))>0){
            if(entry.is_directory)continue;
            if(!record_entry(&entry,mount_count)){rtcw_pk3_close();fclose(input);return 0;}
        }
        if(rc<0){rtcw_pk3_close();fclose(input);return error(rtcw_pk3_error());}
        rtcw_pk3_close();mount_count++;
    }
    if(ferror(input)){fclose(input);return error("cannot read entire mount list");}
    if(fclose(input)!=0)return error("mount-list close failed");
    if(!mount_count)return error("mount list has no PK3 paths");
    strcpy(error_text,"OK");return 1;
}
unsigned long rtcw_vfs_count(void){return file_count;}
unsigned int rtcw_vfs_mount_count(void){return mount_count;}
const char *rtcw_vfs_mount_name(unsigned int idx){return idx<mount_count?mounts[idx]:NULL;}
int rtcw_vfs_at(unsigned long idx,rtcw_vfs_file *out){
    if(idx>=file_count||!out)return 0;
    *out=files[idx];return 1;
}
int rtcw_vfs_lookup(const char *path,rtcw_vfs_file *out){
    unsigned long b;
    if(!path||!rtcw_pk3_safe_path(path))return error("invalid requested VFS path");
    b=locate(path);
    if(b==HASH_SLOTS||!buckets[b])return error("VFS resource not found");
    if(out)*out=files[buckets[b]-1UL];
    return 1;
}
int rtcw_vfs_extract(const char *path,const char *output){
    rtcw_vfs_file picked;
    if(!output||!*output)return error("invalid output path");
    if(!rtcw_vfs_lookup(path,&picked))return 0;
    if(!rtcw_pk3_open(mounts[picked.mount]))return error(rtcw_pk3_error());
    if(!rtcw_pk3_extract(&picked.entry,output)){
        error(rtcw_pk3_error());rtcw_pk3_close();return 0;
    }
    rtcw_pk3_close();return 1;
}
static int suffix(const char *s,const char *end){
    size_t a=strlen(s),b=strlen(end);return a>=b&&equal_fold(s+a-b,end);
}
static int accepted(const char *s,const char *selector){
    if(!strcmp(selector,"all"))return 1;
    if(!strcmp(selector,"models"))return suffix(s,".md3")||suffix(s,".mdc")||suffix(s,".mds")||suffix(s,".skin")||suffix(s,".cfg");
    if(!strcmp(selector,"textures"))return suffix(s,".tga")||suffix(s,".jpg")||suffix(s,".jpeg")||suffix(s,".pcx")||suffix(s,".shader");
    return 0;
}
static int mkdir_parents(const char *path,const char *root){
    char temp[REPORT_PATH];size_t i,n=strlen(path),from=strlen(root);
    if(n>=sizeof(temp))return 0;
    strcpy(temp,path);
    if(from<n && temp[from]!='/')return 0;
    for(i=from+1U;i<n;i++)if(temp[i]=='/'){
        temp[i]=0;
        if(MKDIR(temp)!=0&&errno!=EEXIST)return 0;
        temp[i]='/';
    }
    return 1;
}
int rtcw_vfs_unpack(const char *dir,const char *selector){
    unsigned long i,n=0;char dest[REPORT_PATH];FILE *probe;
    if(!dir||!*dir||strlen(dir)>650U)return error("invalid output directory");
    if(!selector||(strcmp(selector,"all")&&strcmp(selector,"models")&&strcmp(selector,"textures")))return error("invalid selector");
    if(strchr(dir,'\t')||strchr(dir,'\n'))return error("unsafe output directory");
    probe=fopen(dir,"rb");if(probe){fclose(probe);return error("destination exists (must be new)");}
    if(MKDIR(dir)!=0)return error("destination must be new and its parent must exist");
    for(i=0;i<file_count;i++){
        const char *p=files[i].entry.name;
        if(!accepted(p,selector))continue;
        if(strlen(dir)+strlen(p)+2U>=sizeof(dest))return error("extraction output path too long");
        {
            size_t a=strlen(dir),b=strlen(p);
            memcpy(dest,dir,a);dest[a]='/';
            memcpy(dest+a+1U,p,b+1U);
        }
        if(!mkdir_parents(dest,dir))return error("cannot create extraction path");
        if(!rtcw_vfs_extract(p,dest))return 0;
        n++;
    }
    printf("OK unpack winner_files=%lu selected=%lu output=%s\n",file_count,n,dir);
    return 1;
}
int rtcw_vfs_report(const char *path,int conflicts){
    unsigned long i;FILE *out;
    out=fopen(path,"wb");if(!out)return error("cannot create VFS report");
    if(conflicts){
        fprintf(out,"virtual_path\tprevious_archive\toverriding_archive\tactive_archive\n");
        for(i=0;i<history_count;i++){
            const history *h=&history_log[i];
            fprintf(out,"%s\t%s\t%s\t%s\n",files[h->slot].entry.name,
                    mounts[h->oldpack],mounts[h->newpack],mounts[files[h->slot].mount]);
        }
    }else{
        fprintf(out,"virtual_path\tarchive\tbytes\tcompression\toverrides\n");
        for(i=0;i<file_count;i++){
            const rtcw_vfs_file *f=&files[i];
            fprintf(out,"%s\t%s\t%lu\t%u\t%u\n",f->entry.name,mounts[f->mount],
                    f->entry.uncompressed_size,f->entry.method,f->overrides);
        }
    }
    if(fclose(out)!=0)return error("cannot close VFS report");
    return 1;
}
