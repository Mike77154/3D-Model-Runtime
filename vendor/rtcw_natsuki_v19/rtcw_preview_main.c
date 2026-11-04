/* CC0-1.0: C89 offline route/player; scenario commands manually select branches.
   rtcw_preview run model cfg wolfanim.script scenario.txt new_dir
   rtcw_preview run-vfs mounts.txt virtual_model scenario.txt new_dir */
#include "rtcw_preview.h"
#include "rtcw_animscript.h"
#include "rtcw_clipbank.h"
#include "rtcw_codec.h"
#include "rtcw_vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir(p,0755)
#endif
#define PATH_CAP 768U
static char error_text[200]="OK";
static int fail(const char *msg){strncpy(error_text,msg,sizeof(error_text)-1U);error_text[sizeof(error_text)-1U]=0;return 0;}
static int path(const char *dir,const char *leaf,char *out){
    size_t a=strlen(dir),b=strlen(leaf);
    if(a==0U||a+b+2U>=PATH_CAP)return fail("output path too long");
    strcpy(out,dir);if(out[a-1U]!='/'&&out[a-1U]!='\\')strcat(out,"/");strcat(out,leaf);return 1;
}
static int new_dir(const char *out){FILE *f;
    if(!out||!*out||strlen(out)>580U||strchr(out,'\t')||strchr(out,'\n'))return fail("invalid output directory");
    f=fopen(out,"rb");if(f){fclose(f);return fail("output already exists");}
    if(MAKE_DIR(out)!=0)return fail("cannot create output directory; parent must exist");
    return 1;
}
static int number(const char *s,unsigned long *out){char *end;unsigned long v;
    if(!s||!*s||*s=='-')return 0;
    errno=0;v=strtoul(s,&end,10);
    if(errno||end==s||*end)return 0;
    *out=v;return 1;
}
static char *trim(char *s){char *end;
    while(*s==' '||*s=='\t')s++;
    end=s+strlen(s);
    while(end>s&&(end[-1]==' '||end[-1]=='\t'||end[-1]=='\r'||end[-1]=='\n'))*--end=0;
    return s;
}
static int check_extra(void){return strtok(NULL," \t\r\n")==NULL;}
static int audit(FILE *f,const rtcw_preview *p,unsigned long line,const char *action){
    if(fprintf(f,"%lu\t%lu\t%s\t%s\t%s\t%s\t%u\t%lu\n",p->now_ms,line,action,
       p->state,p->last_event,p->last_sound,p->visible_mask,p->last_command_line)<0)
       return fail("action report write failed");
    return 1;
}
static int shot(const char *out,rtcw_preview *p,unsigned int n,FILE *timeline){
    rtcw_preview_sample legs,torso;char obj[PATH_CAP],ppm[PATH_CAP],leaf[32];
    int hl=p->legs.enabled!=0U,ht=p->torso.enabled!=0U;
    if(n>=RTCW_PREVIEW_MAX_SHOTS)return fail("snapshot limit exceeded");
    if(!rtcw_preview_render(p))return fail(rtcw_preview_error());
    if(hl&&!rtcw_preview_sample_channel(p,&p->legs,&legs))return fail(rtcw_preview_error());
    if(ht&&!rtcw_preview_sample_channel(p,&p->torso,&torso))return fail(rtcw_preview_error());
    if(!hl)legs=torso;
    if(!ht)torso=legs;
    sprintf(leaf,"pose_%03u.obj",n);
    if(!path(out,leaf,obj)||!rtcw_save_obj(obj))return fail("cannot export OBJ snapshot");
    sprintf(leaf,"pose_%03u.ppm",n);
    if(!path(out,leaf,ppm)||!rtcw_save_ppm(ppm))return fail("cannot export PPM snapshot");
    if(fprintf(timeline,"%u\t%lu\t%s\t%s\t%s\t%lu\t%lu\t%u\t%s\t%lu\t%lu\t%u\t%u\t%lu\tpose_%03u.obj\tpose_%03u.ppm\n",
       n,p->now_ms,p->state,p->last_event,legs.name,legs.frame_a,legs.frame_b,legs.lerp_q12,
       torso.name,torso.frame_a,torso.frame_b,torso.lerp_q12,p->visible_mask,p->last_command_line,n,n)<0)
       return fail("timeline row write failed");
    return 1;
}
static int scenario(const char *filename,const char *out){
    FILE *f,*timeline,*actions;char buf[512],report[PATH_CAP],auditpath[PATH_CAP],pathbuf[PATH_CAP];
    rtcw_preview state;unsigned long line=0UL,v;unsigned int shots=0U;char *cmd,*p1,*p2,*rest;int result=0;
    if(!path(out,"timeline.tsv",report)||!path(out,"actions.tsv",auditpath)||!path(out,"graph.tsv",pathbuf))return 0;
    if(!rtcw_animscript_report(pathbuf))return fail(rtcw_animscript_error());
    f=fopen(filename,"rb");if(!f)return fail("cannot open scenario script");
    timeline=fopen(report,"wb");if(!timeline){fclose(f);return fail("cannot open timeline output");}
    actions=fopen(auditpath,"wb");if(!actions){fclose(f);fclose(timeline);return fail("cannot open action report");}
    if(fputs("shot\ttime_ms\tstate\tlast_event\tlegs_clip\tlegs_frame_a\tlegs_frame_b\tlegs_alpha_q12\ttorso_clip\ttorso_frame_a\ttorso_frame_b\ttorso_alpha_q12\tvisible_mask_metadata\tscript_line\tobj\tppm\n",timeline)==EOF||
       fputs("time_ms\tscenario_line\taction\tstate\tlast_event\tlast_sound_metadata\tvisible_mask_metadata\tscript_line\n",actions)==EOF){fail("cannot write report headers");goto done;}
    rtcw_preview_reset(&state);
    while(fgets(buf,sizeof(buf),f)){
        size_t len=strlen(buf);int okay=0;line++;
        if(len>0U&&buf[len-1U]!='\n'&&!feof(f)){fail("scenario line exceeds 511 bytes");goto done;}
        cmd=trim(buf);if(!*cmd||*cmd=='#'||(cmd[0]=='/'&&cmd[1]=='/'))continue;
        cmd=strtok(cmd," \t\r\n");if(!cmd)continue;
        if(strcmp(cmd,"STATE")==0){
            p1=strtok(NULL," \t\r\n");
            if(p1&&check_extra())okay=rtcw_preview_state(&state,p1);
        }else if(strcmp(cmd,"PLAY")==0){
            p1=strtok(NULL," \t\r\n");p2=strtok(NULL," \t\r\n");
            rest=strtok(NULL,"\r\n");
            if(p1&&p2&&rest)okay=rtcw_preview_route(&state,p1,p2,trim(rest));
        }else if(strcmp(cmd,"EVENT")==0){
            p1=strtok(NULL," \t\r\n");rest=strtok(NULL,"\r\n");
            if(p1&&rest)okay=rtcw_preview_event(&state,p1,trim(rest));
        }else if(strcmp(cmd,"CHANGE")==0){
            p1=strtok(NULL," \t\r\n");rest=strtok(NULL,"\r\n");
            if(p1&&rest)okay=rtcw_preview_change(&state,p1,trim(rest));
        }else if(strcmp(cmd,"APPLY")==0){
            p1=strtok(NULL," \t\r\n");
            if(p1&&number(p1,&v)&&v<RTCW_SCRIPT_MAX_COMMANDS&&check_extra())
                okay=rtcw_preview_apply_id(&state,(unsigned int)v);
        }else if(strcmp(cmd,"ADVANCE")==0){
            p1=strtok(NULL," \t\r\n");
            if(p1&&number(p1,&v)&&check_extra())okay=rtcw_preview_advance(&state,v);
            if(okay){okay=shot(out,&state,shots,timeline);if(okay)shots++;}
        }else if(strcmp(cmd,"SHOT")==0){
            if(check_extra()){okay=shot(out,&state,shots,timeline);if(okay)shots++;}
        }
        if(!okay){char errorline[200];
            const char *why=rtcw_preview_error();
            if(!why||strcmp(why,"OK")==0)why="unsupported or malformed scenario command";
            sprintf(errorline,"scenario line %lu: %.140s",line,why);fail(errorline);goto done;
        }
        if(!audit(actions,&state,line,cmd))goto done;
    }
    if(ferror(f)){fail("scenario input read failed");goto done;}
    if(shots==0U){fail("no SHOT or ADVANCE produced output");goto done;}
    result=1;
 done:
    if(fclose(f)!=0)result=fail("scenario close failed");
    if(fclose(timeline)!=0)result=fail("timeline close failed");
    if(fclose(actions)!=0)result=fail("action report close failed");
    if(result)printf("OK snapshots=%u commands=%u time_ms=%lu\n",shots,rtcw_animscript_count(),state.now_ms);
    return result;
}
static int init(const char *model,const char *cfg,const char *script){
    if(!rtcw_open(model)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!rtcw_clipbank_read(cfg,rtcw_current.frames))return fail(rtcw_clipbank_error());
    if(!rtcw_animscript_read(script))return fail(rtcw_animscript_error());
    return 1;
}
static int local(const char *model,const char *cfg,const char *script,const char *plan,const char *output){
    if(!init(model,cfg,script))return 0;
    if(!new_dir(output))return 0;
    return scenario(plan,output);
}
static int vfs(const char *mounts,const char *model,const char *plan,const char *output){
    char cfg[260],script[260],modelpath[PATH_CAP],cfgpath[PATH_CAP],scriptpath[PATH_CAP],reportpath[PATH_CAP];
    const char *sep=strrchr(model,'/');size_t len=sep?(size_t)(sep-model)+1U:0U;
    rtcw_vfs_file m,c,s;FILE *f;
    if(len+strlen("wolfanim.script")+1U>sizeof(script))return fail("virtual path too long");
    if(!rtcw_vfs_mounts(mounts))return fail(rtcw_vfs_error());
    memcpy(script,model,len);strcpy(script+len,"wolfanim.script");
    memcpy(cfg,model,len);strcpy(cfg+len,"wolfanim.cfg");
    if(!rtcw_vfs_lookup(model,&m)||!rtcw_vfs_lookup(script,&s))return fail("model or wolfanim.script missing in VFS");
    if(!rtcw_vfs_lookup(cfg,&c)){
        memcpy(cfg,model,len);strcpy(cfg+len,"animation.cfg");
        if(!rtcw_vfs_lookup(cfg,&c))return fail("wolfanim.cfg and animation.cfg missing in VFS");
    }
    if(!new_dir(output))return 0;
    if(!path(output,"model.bin",modelpath)||!path(output,"wolfanim.cfg",cfgpath)||
       !path(output,"wolfanim.script",scriptpath)||!path(output,"sources.tsv",reportpath))return 0;
    if(!rtcw_vfs_extract(m.entry.name,modelpath)||!rtcw_vfs_extract(c.entry.name,cfgpath)||
       !rtcw_vfs_extract(s.entry.name,scriptpath))return fail(rtcw_vfs_error());
    f=fopen(reportpath,"wb");if(!f)return fail("cannot write source provenance");
    if(fprintf(f,"role\tvirtual_path\twinning_archive\tassociation\nmodel\t%s\t%s\texplicit\ncfg\t%s\t%s\tsame_directory_candidate\nscript\t%s\t%s\tsame_directory_candidate\n",
       m.entry.name,rtcw_vfs_mount_name(m.mount),c.entry.name,rtcw_vfs_mount_name(c.mount),
       s.entry.name,rtcw_vfs_mount_name(s.mount))<0){fclose(f);return fail("source report write failed");}
    if(fclose(f)!=0)return fail("source report close failed");
    if(!init(modelpath,cfgpath,scriptpath))return 0;
    return scenario(plan,output);
}
static void usage(void){fprintf(stderr,"RTCW v0.11 preview C89 (explicit scenario; no gameplay AI)\n rtcw_preview run model cfg script plan NEW_DIR\n rtcw_preview run-vfs mounts virtual_model plan NEW_DIR\n");}
int main(int argc,char **argv){int ok;
    if(argc==7&&strcmp(argv[1],"run")==0)ok=local(argv[2],argv[3],argv[4],argv[5],argv[6]);
    else if(argc==6&&strcmp(argv[1],"run-vfs")==0)ok=vfs(argv[2],argv[3],argv[4],argv[5]);
    else{usage();return 2;}
    if(!ok){fprintf(stderr,"RTCW preview: %s\n",error_text);return 1;}
    return 0;
}
