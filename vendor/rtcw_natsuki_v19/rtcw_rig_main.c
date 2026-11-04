/* CC0-1.0: C89 offline route/player; scenario commands manually select branches.
   rtcw_preview run model cfg wolfanim.script scenario.txt new_dir
   rtcw_preview run-vfs mounts.txt virtual_model scenario.txt new_dir */
#include "rtcw_preview.h"
#include "rtcw_animscript.h"
#include "rtcw_clipbank.h"
#include "rtcw_codec.h"
#include "rtcw_vfs.h"
#include "rtcw_contact.h"
#include "rtcw_contact_color.h"
#include "rtcw_texrender.h"
#include "rtcw_vfs_material.h"
#include "rtcw_visibility.h"
#include "rtcw_texcache.h"
#include "rtcw_material.h"
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
#define RTCW_RIG_MAX 24U
typedef struct rig_piece_s {
    char tag[65],file[PATH_CAP],skin[PATH_CAP];
    unsigned int mode;
    unsigned long frame;
    unsigned int visibility_part; /* 0..7 = script bit, 8 = always */
} rig_piece;
static rig_piece rig[RTCW_RIG_MAX];
static unsigned int rig_count;
static char root_model[PATH_CAP],root_skin[PATH_CAP];
static char texture_root[PATH_CAP],shader_file[PATH_CAP];
static unsigned int root_visibility_part;
static int vfs_auto_materials=0;
static int number(const char *s,unsigned long *out);
static int fail(const char *msg);
static int load_rig(const char *filename){
    FILE *f;char buf[1700],*p,*tok,*tag,*file,*mode,*skin,*extra;
    unsigned long fixed;unsigned int count=0U;
    rig_count=0U;root_skin[0]=0;texture_root[0]=0;shader_file[0]=0;
    root_visibility_part=RTCW_VIS_ALWAYS;
    f=fopen(filename,"rb");if(!f)return fail("cannot open rig file");
    while(fgets(buf,sizeof(buf),f)){
        size_t len=strlen(buf);
        if(len&&buf[len-1U]!='\n'&&!feof(f)){fclose(f);return fail("rig line too long");}
        p=buf;while(*p==' '||*p=='\t')p++;
        if(*p=='#'||!*p||*p=='\r'||*p=='\n')continue;
        tok=strtok(p," \t\r\n");
        if(tok&&strcmp(tok,"ROOT_SKIN")==0){
            file=strtok(NULL," \t\r\n");extra=strtok(NULL," \t\r\n");
            if(!file||extra||strlen(file)>=PATH_CAP){fclose(f);return fail("bad ROOT_SKIN line");}
            strcpy(root_skin,file);if(strcmp(root_skin,"-")==0)root_skin[0]=0;
            continue;
        }
        if(tok&&(strcmp(tok,"TEXTURE_ROOT")==0||strcmp(tok,"SHADER_FILE")==0||strcmp(tok,"ROOT_PART")==0)){
            char *arg=strtok(NULL," \t\r\n");
            extra=strtok(NULL," \t\r\n");
            if(!arg||extra||strlen(arg)>=PATH_CAP){fclose(f);return fail("bad texture/shader/part directive");}
            if(strcmp(tok,"ROOT_PART")==0){
                if(!number(arg,&fixed)||fixed>7UL){fclose(f);return fail("ROOT_PART must be 0..7");}
                root_visibility_part=(unsigned int)fixed;
            }else if(strcmp(tok,"TEXTURE_ROOT")==0){
                strcpy(texture_root,arg);if(strcmp(texture_root,"-")==0)texture_root[0]=0;
            }else{
                strcpy(shader_file,arg);if(strcmp(shader_file,"-")==0)shader_file[0]=0;
            }
            continue;
        }
        if(!tok||strcmp(tok,"ATTACH")!=0||count>=RTCW_RIG_MAX){fclose(f);return fail("bad rig command or too many parts");}
        tag=strtok(NULL," \t\r\n");file=strtok(NULL," \t\r\n");
        mode=strtok(NULL," \t\r\n");skin=strtok(NULL," \t\r\n");extra=strtok(NULL," \t\r\n");
        if(!tag||!file||!mode||!skin||strlen(tag)>64U||
           strlen(file)>=PATH_CAP||strlen(skin)>=PATH_CAP){fclose(f);return fail("malformed ATTACH record");}
        strcpy(rig[count].tag,tag);strcpy(rig[count].file,file);strcpy(rig[count].skin,skin);
        rig[count].visibility_part=RTCW_VIS_ALWAYS;
        if(extra){
            if(strncmp(extra,"PART:",5)!=0||!number(extra+5,&fixed)||fixed>7UL||strtok(NULL," \t\r\n")){
                fclose(f);return fail("ATTACH accepts only optional PART:0..7");
            }
            rig[count].visibility_part=(unsigned int)fixed;
        }
        if(strcmp(mode,"LEGS")==0)rig[count].mode=1U;
        else if(strcmp(mode,"TORSO")==0)rig[count].mode=2U;
        else if(strncmp(mode,"FIXED:",6)==0&&number(mode+6,&fixed)&&fixed<4096UL){
            rig[count].mode=0U;rig[count].frame=fixed;
        }else{fclose(f);return fail("unknown rig mode: use FIXED:N, LEGS or TORSO");}
        count++;
    }
    if(ferror(f)){fclose(f);return fail("rig file read failed");}
    if(fclose(f)!=0)return fail("rig file close failed");
    rig_count=count;return 1;
}
static int assemble_pose(const rtcw_preview *p){
    unsigned int i;rtcw_preview_sample sample;
    if(!rtcw_open(root_model)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!rtcw_preview_render(p))return fail(rtcw_preview_error());
    if(root_skin[0]&&!rtcw_apply_skin(root_skin))return fail("parent skin could not be applied");
    if(!rtcw_attach_begin_decoded())return fail(rtcw_attach_error());
    rtcw_visibility_reset();
    if(!rtcw_visibility_bind(0U,rtcw_current.surface_count,root_visibility_part))
        return fail(rtcw_visibility_error());
    for(i=0U;i<rig_count;i++){
        unsigned int before=rtcw_current.surface_count;
        unsigned long a,b;unsigned int alpha;
        if(rig[i].mode==0U){a=b=rig[i].frame;alpha=0U;}
        else{
            const rtcw_preview_channel *ch=rig[i].mode==1U?&p->legs:&p->torso;
            if(!ch->enabled)return fail("child rig channel not active");
            if(!rtcw_preview_sample_channel(p,ch,&sample))return fail(rtcw_preview_error());
            a=sample.frame_a;b=sample.frame_b;alpha=sample.lerp_q12;
        }
        if(!rtcw_attach_add_blend(rig[i].tag,rig[i].file,a,b,alpha,
            strcmp(rig[i].skin,"-")==0?NULL:rig[i].skin))return fail(rtcw_attach_error());
        if(!rtcw_visibility_bind(before,rtcw_current.surface_count-before,rig[i].visibility_part))
            return fail(rtcw_visibility_error());
    }
    if(!rtcw_visibility_apply(p->visible_mask))return fail(rtcw_visibility_error());
    return 1;
}
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
    rtcw_preview_sample legs,torso;char obj[PATH_CAP],ppm[PATH_CAP],leaf[48];
    unsigned int missing=0U;char mapfile[PATH_CAP],provenance[PATH_CAP];
    int hl=p->legs.enabled!=0U,ht=p->torso.enabled!=0U;
    if(n>=RTCW_PREVIEW_MAX_SHOTS)return fail("snapshot limit exceeded");
    if(!assemble_pose(p))return 0;
    if(hl&&!rtcw_preview_sample_channel(p,&p->legs,&legs))return fail(rtcw_preview_error());
    if(ht&&!rtcw_preview_sample_channel(p,&p->torso,&torso))return fail(rtcw_preview_error());
    if(!hl)legs=torso;
    if(!ht)torso=legs;
    sprintf(leaf,"pose_%03u.obj",n);
    if(!path(out,leaf,obj)||!rtcw_save_obj(obj))return fail("cannot export OBJ snapshot");
    sprintf(leaf,"pose_%03u.ppm",n);
    if(!path(out,leaf,ppm)||!rtcw_save_ppm(ppm))return fail("cannot export PPM snapshot");
    if(texture_root[0]||vfs_auto_materials){
        sprintf(leaf,"pose_%03u_color.ppm",n);
        if(!path(out,leaf,ppm))return 0;
        if(vfs_auto_materials && !texture_root[0]){
            sprintf(leaf,"materials_%03u.tsv",n);
            if(!path(out,leaf,mapfile))return 0;
            sprintf(leaf,"material_sources_%03u.tsv",n);
            if(!path(out,leaf,provenance))return 0;
            if(!rtcw_vfs_material_stage_time(out,mapfile,provenance,shader_file[0]?shader_file:NULL,p->now_ms))
                return fail(rtcw_vfs_material_error());
            if(!rtcw_render_textured_map_time(ppm,mapfile,p->now_ms))return fail(rtcw_render_error());
        }else if(!rtcw_render_textured_time(ppm,texture_root,shader_file[0]?shader_file:NULL,p->now_ms))
            return fail(rtcw_render_error());
        missing=rtcw_render_missing_textures();
    }
    if(fprintf(timeline,"%u\t%lu\t%s\t%s\t%s\t%lu\t%lu\t%u\t%s\t%lu\t%lu\t%u\t%u\t%lu\tpose_%03u.obj\tpose_%03u.ppm\t%u\t%lu\t%lu\t%lu\t%u\n",
       n,p->now_ms,p->state,p->last_event,legs.name,legs.frame_a,legs.frame_b,legs.lerp_q12,
       torso.name,torso.frame_a,torso.frame_b,torso.lerp_q12,p->visible_mask,p->last_command_line,n,n,rig_count,rtcw_current.vertex_count,rtcw_current.triangle_count,rtcw_visibility_hidden_triangles(),missing)<0)
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
    if(fputs("shot\ttime_ms\tstate\tlast_event\tlegs_clip\tlegs_frame_a\tlegs_frame_b\tlegs_alpha_q12\ttorso_clip\ttorso_frame_a\ttorso_frame_b\ttorso_alpha_q12\tvisible_mask_metadata\tscript_line\tobj\tppm\tattached\tvertices\ttriangles\thidden_triangles\tmissing_textures\n",timeline)==EOF||
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
    if(!rtcw_contact_generate(out,shots)){fail(rtcw_contact_error());goto done;}
    if((texture_root[0]||vfs_auto_materials)&&!rtcw_contact_color_generate(out,shots)){
        fail(rtcw_contact_color_error());goto done;
    }
    {FILE *cachelog;
        if(!path(out,"texture_cache.tsv",pathbuf)){goto done;}
        cachelog=fopen(pathbuf,"wb");if(!cachelog){fail("cannot create cache report");goto done;}
        if(fprintf(cachelog,"texture_hits\ttexture_misses\ttexture_evictions\tvfs_extracted\tvfs_reused\n%lu\t%lu\t%lu\t%lu\t%lu\n",
            rtcw_texcache_hits(),rtcw_texcache_misses(),rtcw_texcache_evictions(),
            rtcw_vfs_material_extracted(),rtcw_vfs_material_reused())<0){
            fclose(cachelog);fail("cannot write cache report");goto done;
        }
        if(fclose(cachelog)!=0){fail("cache report close failed");goto done;}
    }
    result=1;
 done:
    rtcw_vfs_material_cache_clear();
    if(fclose(f)!=0)result=fail("scenario close failed");
    if(fclose(timeline)!=0)result=fail("timeline close failed");
    if(fclose(actions)!=0)result=fail("action report close failed");
    if(result)printf("OK snapshots=%u commands=%u time_ms=%lu\n",shots,rtcw_animscript_count(),state.now_ms);
    return result;
}
static int init(const char *model,const char *cfg,const char *script){
    if(strlen(model)>=sizeof(root_model))return fail("root model path too long");
    strcpy(root_model,model);
    if(!rtcw_open(model)||!rtcw_decode(0UL))return fail(rtcw_error());
    if(!rtcw_clipbank_read(cfg,rtcw_current.frames))return fail(rtcw_clipbank_error());
    if(!rtcw_animscript_read(script))return fail(rtcw_animscript_error());
    return 1;
}
static int local(const char *model,const char *cfg,const char *script,const char *plan,const char *rigfile,const char *output){
    rtcw_texcache_reset();rtcw_material_cache_clear();rtcw_vfs_material_cache_clear();
    vfs_auto_materials=0;
    if(!init(model,cfg,script)||!load_rig(rigfile))return 0;
    if(!new_dir(output))return 0;
    return scenario(plan,output);
}
/* Materialize explicit @virtual/path rig entries through the winning VFS mount.
   The local-mode rig still accepts ordinary paths; no implicit guessing. */
static int vfs_rig_assets(const char *output,const char *report){
    unsigned int i;FILE *f;char leaf[40],dst[PATH_CAP];rtcw_vfs_file entry;
    f=fopen(report,"ab");if(!f)return fail("cannot append rig provenance");
    for(i=0U;i<rig_count+1U;i++){
        char *model=(i==rig_count)?NULL:rig[i].file;
        char *skin=(i==rig_count)?root_skin:rig[i].skin;
        if(model&&model[0]=='@'){
            if(!rtcw_vfs_lookup(model+1,&entry)){fclose(f);return fail("rig model not found in multi-PK3 VFS");}
            sprintf(leaf,"rig_part_%02u.bin",i);
            if(!path(output,leaf,dst)||!rtcw_vfs_extract(entry.entry.name,dst)){
                fclose(f);return fail("rig model extraction failed");
            }
            if(fprintf(f,"rig_part_%u\t%s\t%s\texplicit_vfs\n",i,entry.entry.name,rtcw_vfs_mount_name(entry.mount))<0){
                fclose(f);return fail("rig provenance write failed");
            }
            strcpy(model,dst);
        }
        if(skin&&skin[0]=='@'){
            if(!rtcw_vfs_lookup(skin+1,&entry)){fclose(f);return fail("rig skin not found in multi-PK3 VFS");}
            sprintf(leaf,"rig_skin_%02u.skin",i);
            if(!path(output,leaf,dst)||!rtcw_vfs_extract(entry.entry.name,dst)){
                fclose(f);return fail("rig skin extraction failed");
            }
            if(fprintf(f,"rig_skin_%u\t%s\t%s\texplicit_vfs\n",i,entry.entry.name,rtcw_vfs_mount_name(entry.mount))<0){
                fclose(f);return fail("rig skin provenance write failed");
            }
            strcpy(skin,dst);
        }
    }
    if(fclose(f)!=0)return fail("rig provenance close failed");
    return 1;
}
static int vfs(const char *mounts,const char *model,const char *plan,const char *rigfile,const char *output){
    char cfg[260],script[260],modelpath[PATH_CAP],cfgpath[PATH_CAP],scriptpath[PATH_CAP],reportpath[PATH_CAP];
    const char *sep=strrchr(model,'/');size_t len=sep?(size_t)(sep-model)+1U:0U;
    rtcw_vfs_file m,c,s;FILE *f;
    rtcw_texcache_reset();rtcw_material_cache_clear();rtcw_vfs_material_cache_clear();
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
    if(!init(modelpath,cfgpath,scriptpath)||!load_rig(rigfile))return 0;
    if(!vfs_rig_assets(output,reportpath))return 0;
    vfs_auto_materials=1;
    if(shader_file[0]=='@'){
        rtcw_vfs_file shader_entry;char dst[PATH_CAP];
        if(!rtcw_vfs_lookup(shader_file+1,&shader_entry))return fail("virtual shader script missing");
        if(!path(output,"rig_material.shader",dst)||
           !rtcw_vfs_extract(shader_entry.entry.name,dst))return fail("shader extraction failed");
        strcpy(shader_file,dst);
    }
    return scenario(plan,output);
}
static void usage(void){fprintf(stderr,"RTCW v0.15 rig preview C89 (explicit scenario; no gameplay AI)\n rtcw_rig run model cfg script scenario rig NEW_DIR\n rtcw_rig run-vfs mounts virtual_model scenario rig NEW_DIR\n");}
int main(int argc,char **argv){int ok;
    if(argc==8&&strcmp(argv[1],"run")==0)ok=local(argv[2],argv[3],argv[4],argv[5],argv[6],argv[7]);
    else if(argc==7&&strcmp(argv[1],"run-vfs")==0)ok=vfs(argv[2],argv[3],argv[4],argv[5],argv[6]);
    else{usage();return 2;}
    if(!ok){fprintf(stderr,"RTCW preview: %s\n",error_text);return 1;}
    return 0;
}
