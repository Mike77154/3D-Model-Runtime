/* RTCW animation catalog + bounded offline sequence previews.
   CC0, ISO C89, no heap or floats, no stdin/third-party dependencies. */
#include "rtcw_clipbank.h"
#include "rtcw_codec.h"
#include "rtcw_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#define DIR_CREATE(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define DIR_CREATE(p) mkdir(p,0755)
#endif
#define OUTMAX 768U
#define MAX_IMAGES 48UL
static char reason[200]="OK";
static int fail(const char *why){
    strncpy(reason,why,sizeof(reason)-1U);reason[sizeof(reason)-1U]=0;
    return 0;
}
static int number(const char *s,unsigned long *out){
    char *end;unsigned long v;
    if(!s||!*s||*s=='-')return 0;
    errno=0;v=strtoul(s,&end,10);
    if(errno||end==s||*end)return 0;
    *out=v;return 1;
}
static int file_path(const char *base,const char *leaf,char *out){
    size_t a=strlen(base),b=strlen(leaf);
    if(a+b+2U>OUTMAX)return fail("output path exceeds limit");
    strcpy(out,base);
    if(a&&out[a-1]!='/'&&out[a-1]!='\\')strcat(out,"/");
    strcat(out,leaf);return 1;
}
static int fresh_dir(const char *out){
    FILE *probe;
    if(!out||!*out||strlen(out)>570U||strchr(out,'\n')||strchr(out,'\t'))return fail("invalid output directory");
    probe=fopen(out,"rb");if(probe){fclose(probe);return fail("destination exists, use a new directory");}
    if(DIR_CREATE(out)!=0)return fail("cannot create new output directory (parent must exist)");
    return 1;
}
static int model_and_cfg(const char *model,const char *cfg){
    if(!rtcw_open(model)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!rtcw_clipbank_read(cfg,rtcw_current.frames))return fail(rtcw_clipbank_error());
    return 1;
}
static int catalog(const char *model,const char *cfg,const char *out){
    if(!model_and_cfg(model,cfg))return 0;
    if(!rtcw_clipbank_report(out))return fail(rtcw_clipbank_error());
    printf("OK format=%s model_frames=%lu clip_rows=%u\n",
           rtcw_current.format,rtcw_current.frames,rtcw_clipbank_count());
    return 1;
}
static int sequence(const char *model,const char *cfg,unsigned int id,const char *out,unsigned long stride){
    char path[OUTMAX],leaf[72];FILE *timeline;const rtcw_clip *clip;
    unsigned long count,offset,n=0UL,frame,elapsed;
    if(!model_and_cfg(model,cfg))return 0;
    clip=rtcw_clipbank_at(id);
    if(!clip)return fail("clip index out of range");
    if(!clip->valid)return fail("clip frame range exceeds model frames");
    count=clip->count;
    if(stride==0UL)stride=1UL+(count-1UL)/24UL;
    if(stride==0UL||1UL+(count-1UL)/stride>MAX_IMAGES)
        return fail("requested stride creates more than 48 previews");
    if(!fresh_dir(out))return 0;
    if(!file_path(out,"clips.tsv",path)||!rtcw_clipbank_report(path))return fail(rtcw_clipbank_error());
    if(!file_path(out,"timeline.tsv",path))return 0;
    timeline=fopen(path,"wb");if(!timeline)return fail("cannot create frame timeline");
    if(fputs("clip_id\tclip_name\tsource_line\tframe\tclip_offset\ttime_ms\tfps_milli\tobj\twire_ppm\n",timeline)==EOF){fclose(timeline);return fail("timeline write error");}
    offset=0UL;
    while(offset<count){
        if(clip->first>ULONG_MAX-offset){fclose(timeline);return fail("frame number overflow");}
        frame=clip->first+offset;
        if(!rtcw_decode(frame)){fclose(timeline);return fail(rtcw_error());}
        if(!rtcw_clip_elapsed_ms(offset,clip->fps_milli,&elapsed)){fclose(timeline);return fail("timestamp overflow");}
        sprintf(leaf,"pose_%03lu.obj",n);
        if(!file_path(out,leaf,path)||!rtcw_save_obj(path)){fclose(timeline);return fail("OBJ export failed");}
        sprintf(leaf,"pose_%03lu.ppm",n);
        if(!file_path(out,leaf,path)||!rtcw_save_ppm(path)){fclose(timeline);return fail("PPM export failed");}
        if(fprintf(timeline,"%u\t%s\t%lu\t%lu\t%lu\t%lu\t%lu\tpose_%03lu.obj\tpose_%03lu.ppm\n",
                   id,clip->name,clip->line,frame,offset,elapsed,clip->fps_milli,n,n)<0){fclose(timeline);return fail("timeline write error");}
        n++;
        if(stride>count-1UL-offset)break;
        offset+=stride;
    }
    if(fclose(timeline)!=0)return fail("timeline close failed");
    printf("OK clip=%s frames=%lu previews=%lu stride=%lu\n",clip->name,count,n,stride);
    return 1;
}
static int fold(int c){return c>='A'&&c<='Z'?c+32:c;}
static int path_ends(const char *name,const char *ext){
    size_t a=strlen(name),b=strlen(ext),i;
    if(a<b)return 0;
    for(i=0;i<b;i++)if(fold((unsigned char)name[a-b+i])!=fold((unsigned char)ext[i]))return 0;
    return 1;
}
/* Deliberate inference: only exact wolfanim.cfg or animation.cfg in MODEL directory. */
static int find_model_cfg(const char *model,char *cfg){
    const char *slash=strrchr(model,'/');size_t len=slash?(size_t)(slash-model)+1U:0U;
    const char *opts[]={"wolfanim.cfg","animation.cfg"};
    unsigned int i;rtcw_vfs_file found;
    if(!rtcw_vfs_lookup(model,&found))return fail("model not found in VFS");
    if(!path_ends(model,".mds")&&!path_ends(model,".mdc")&&!path_ends(model,".md3"))
        return fail("virtual model must have MD3, MDC, or MDS extension");
    if(len+strlen(opts[0])+1U>241U)return fail("virtual CFG path too long");
    for(i=0U;i<2U;i++){
        memcpy(cfg,model,len);strcpy(cfg+len,opts[i]);
        if(rtcw_vfs_lookup(cfg,&found))return 1;
    }
    return fail("no same-folder wolfanim.cfg or animation.cfg; use local catalog with explicit CFG");
}
static int vfs_job(const char *mounts,const char *model,int is_seq,unsigned int id,const char *out,unsigned long stride){
    char cfg[241],m_path[OUTMAX],c_path[OUTMAX],t_path[OUTMAX];
    rtcw_vfs_file m,c;FILE *manifest;int done=0;
    if(!rtcw_vfs_mounts(mounts))return fail(rtcw_vfs_error());
    if(!find_model_cfg(model,cfg))return 0;
    if(!rtcw_vfs_lookup(model,&m)||!rtcw_vfs_lookup(cfg,&c))return fail(rtcw_vfs_error());
    if(!fresh_dir(out))return 0;
    if(!file_path(out,"model.bin",m_path)||!file_path(out,"animation.cfg",c_path)||
       !file_path(out,"sources.tsv",t_path))return 0;
    if(!rtcw_vfs_extract(m.entry.name,m_path)||!rtcw_vfs_extract(c.entry.name,c_path))return fail(rtcw_vfs_error());
    manifest=fopen(t_path,"wb");if(!manifest)return fail("cannot create source provenance");
    fprintf(manifest,"role\tvirtual_path\twinning_archive\tassociation\n");
    fprintf(manifest,"model\t%s\t%s\texact_requested_path\n",m.entry.name,rtcw_vfs_mount_name(m.mount));
    fprintf(manifest,"cfg\t%s\t%s\tsame_folder_candidate_not_semantically_proven\n",c.entry.name,rtcw_vfs_mount_name(c.mount));
    if(fclose(manifest)!=0)return fail("source manifest close failed");
    if(!is_seq){
        if(!file_path(out,"clips.tsv",t_path))return 0;
        done=catalog(m_path,c_path,t_path);
    }else{
        char frame_dir[OUTMAX];
        if(!file_path(out,"sequence",frame_dir))return 0;
        done=sequence(m_path,c_path,id,frame_dir,stride);
    }
    return done;
}
static void usage(void){
    fprintf(stderr,
       "RTCW clips v0.9 (ISO C89):\n"
       " rtcw_clips catalog model.mds wolfanim.cfg clips.tsv\n"
       " rtcw_clips sequence model.mds wolfanim.cfg clip_id NEW_dir [stride=auto]\n"
       " rtcw_clips catalog-vfs mounts.txt models/players/name/body.mds NEW_dir\n"
       " rtcw_clips sequence-vfs mounts.txt models/players/name/body.mds clip_id NEW_dir [stride=auto]\n");
}
int main(int argc,char **argv){
    unsigned long id=0UL,stride=0UL;int ok=0;
    if(argc==5&&strcmp(argv[1],"catalog")==0)ok=catalog(argv[2],argv[3],argv[4]);
    else if((argc==6||argc==7)&&strcmp(argv[1],"sequence")==0){
        if(!number(argv[4],&id)||id>=RTCW_CLIPS_MAX||(argc==7&&!number(argv[6],&stride))){usage();return 2;}
        ok=sequence(argv[2],argv[3],(unsigned int)id,argv[5],stride);
    }else if(argc==5&&strcmp(argv[1],"catalog-vfs")==0){
        ok=vfs_job(argv[2],argv[3],0,0U,argv[4],0UL);
    }else if((argc==6||argc==7)&&strcmp(argv[1],"sequence-vfs")==0){
        if(!number(argv[4],&id)||id>=RTCW_CLIPS_MAX||(argc==7&&!number(argv[6],&stride))){usage();return 2;}
        ok=vfs_job(argv[2],argv[3],1,(unsigned int)id,argv[5],stride);
    }else {usage();return 2;}
    if(!ok){fprintf(stderr,"RTCW clips: %s\n",reason);return 1;}
    return 0;
}
