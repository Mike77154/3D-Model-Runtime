/* CC0. Standalone ISO C89 explicit animation selection and deterministic sampling.
   No AI execution or interpretation of wolfanim.script boolean conditions. */
#include "rtcw_preview.h"
#include "rtcw_animscript.h"
#include "rtcw_clipbank.h"
#include "rtcw_codec.h"
#include <string.h>
#include <limits.h>
static char reason[180]="OK";
static int failed(const char *s){strncpy(reason,s,sizeof(reason)-1U);reason[sizeof(reason)-1U]=0;return 0;}
const char *rtcw_preview_error(void){return reason;}
static int fold(int c){return c>='A'&&c<='Z'?c+32:c;}
static int eq(const char *a,const char *b){
    if(!a||!b)return 0;
    while(*a&&*b){if(fold((unsigned char)*a)!=fold((unsigned char)*b))return 0;a++;b++;}
    return *a==0&&*b==0;
}
static int cp(char *dst,unsigned int max,const char *src){size_t n;
    if(!src)return 0;
    n=strlen(src);
    if(n>=max)return 0;
    memcpy(dst,src,n+1U);
    return 1;
}
void rtcw_preview_reset(rtcw_preview *p){if(p){memset(p,0,sizeof(*p));p->visible_mask=255U;}strcpy(reason,"OK");}
int rtcw_preview_state(rtcw_preview *p,const char *name){
    if(!p||!name||!name[0]||!cp(p->state,RTCW_PREVIEW_STATE,name))return failed("invalid state name");
    return 1;
}
static int find_clip(const char *name,unsigned int *index){
    unsigned int i,hits=0U;
    for(i=0;i<rtcw_clipbank_count();i++){
        const rtcw_clip *c=rtcw_clipbank_at(i);
        if(eq(name,c->name)){
            if(!c->valid)return failed("clip frame range exceeds model frames");
            *index=i;hits++;
        }
    }
    if(hits!=1U)return failed("clip absent or duplicated in CFG");
    return 1;
}
int rtcw_preview_apply_id(rtcw_preview *p,unsigned int id){
    const rtcw_script_command *cmd=rtcw_animscript_at(id);
    unsigned int i,idx[2]={0U,0U};rtcw_preview next;
    if(!p||!cmd)return failed("animation command index out of range");
    if(cmd->parts>2U)return failed("unexpected animation command parts");
    for(i=0;i<cmd->parts;i++){
        if(!find_clip(cmd->clip[i],&idx[i]))return 0;
    }
    next=*p;
    for(i=0;i<cmd->parts;i++){
        rtcw_preview_channel *dst;
        if(eq(cmd->body[i],"BOTH")){
            next.legs.enabled=next.torso.enabled=1U;
            next.legs.clip_id=next.torso.clip_id=idx[i];
            next.legs.start_ms=next.torso.start_ms=next.now_ms;
            next.legs.hold_ms=next.torso.hold_ms=cmd->duration[i];
            continue;
        }
        dst=eq(cmd->body[i],"LEGS")?&next.legs:
            eq(cmd->body[i],"TORSO")?&next.torso:NULL;
        if(!dst)return failed("unknown animation body channel");
        dst->enabled=1U;dst->clip_id=idx[i];dst->start_ms=next.now_ms;
        dst->hold_ms=cmd->duration[i];
    }
    next.visible_mask|=cmd->showparts;
    next.visible_mask&=~cmd->hideparts;
    if(!cp(next.last_sound,sizeof(next.last_sound),cmd->sound))return failed("invalid script sound name");
    next.last_command_line=cmd->line;
    if(eq(cmd->section,"events")){
        if(!cp(next.last_event,sizeof(next.last_event),cmd->event))return failed("invalid event name");
    }
    *p=next;return 1;
}
static int route(rtcw_preview *p,const char *section,const char *name,const char *condition,const char *old_state){
    unsigned int i,matches=0U,found=0U;
    if(!p||!section||!name||!condition)return failed("route requires section, name, exact condition");
    for(i=0;i<rtcw_animscript_count();i++){
        const rtcw_script_command *c=rtcw_animscript_at(i);
        if(!eq(c->section,section)||!eq(c->condition,condition))continue;
        if(eq(section,"events")){
            if(!eq(c->event,name))continue;
        }else if(eq(section,"statechanges")){
            if(!eq(c->old_state,old_state)||!eq(c->new_state,name))continue;
        }else{
            if(!eq(c->state,p->state)||!eq(c->movement,name))continue;
        }
        found=i;matches++;
    }
    if(matches==0U)return failed("no exact script route: conditions are NOT evaluated");
    if(matches>1U)return failed("ambiguous script route: use APPLY command_id");
    if(!rtcw_preview_apply_id(p,found))return 0;
    return 1;
}
int rtcw_preview_route(rtcw_preview *p,const char *section,const char *name,const char *condition){
    if(!eq(section,"animations")&&!eq(section,"canned_animations"))return failed("PLAY supports animations or canned_animations only");
    return route(p,section,name,condition,"");
}
int rtcw_preview_event(rtcw_preview *p,const char *event,const char *condition){
    return route(p,"events",event,condition,"");
}
int rtcw_preview_change(rtcw_preview *p,const char *new_state,const char *condition){
    char old[RTCW_PREVIEW_STATE];
    if(!p||!p->state[0]||!new_state)return failed("CHANGE requires existing state");
    strcpy(old,p->state);
    if(!route(p,"statechanges",new_state,condition,old))return 0;
    if(!rtcw_preview_state(p,new_state))return 0;
    return 1;
}
int rtcw_preview_advance(rtcw_preview *p,unsigned long delta){
    if(!p||delta>RTCW_PREVIEW_MAX_MS||p->now_ms>RTCW_PREVIEW_MAX_MS-delta)return failed("preview exceeds one hour limit");
    p->now_ms+=delta;return 1;
}
/* 32-bit-safe Q12 position: millis (<=3,600,000) * milli-FPS (<=1,000,000)
   is split into whole and fractional arithmetic, WITHOUT 64-bit integers. */
static int framepos(unsigned long millis,unsigned long fps,unsigned long *frames,unsigned int *fraction){
    unsigned long secs,ms,whole,remainder,rem_fraction;unsigned int i,f=0U;
    if(millis>RTCW_PREVIEW_MAX_MS||fps==0UL||fps>1000000UL)return failed("time or frame rate outside supported preview limits");
    secs=millis/1000UL;ms=millis%1000UL;
    /* secs <= 3600; secs*fps fits 32 bit unsigned long. */
    whole=secs*fps;
    remainder=(whole%1000UL)*1000UL+ms*fps;
    *frames=whole/1000UL+remainder/1000000UL;
    rem_fraction=remainder%1000000UL;
    for(i=0U;i<12U;i++){
        rem_fraction*=2UL;f=(unsigned int)(f*2U);
        if(rem_fraction>=1000000UL){f++;rem_fraction-=1000000UL;}
    }
    *fraction=f;return 1;
}
int rtcw_preview_sample_channel(const rtcw_preview *p,const rtcw_preview_channel *ch,rtcw_preview_sample *out){
    const rtcw_clip *clip;unsigned long elapsed,steps,slot,loop_first;
    unsigned int frac;
    if(!p||!ch||!out||!ch->enabled)return failed("uninitialized animation channel");
    clip=rtcw_clipbank_at(ch->clip_id);
    if(!clip||!clip->valid||clip->count==0UL)return failed("clip is not valid for model");
    if(p->now_ms<ch->start_ms)return failed("animation clock moved backwards");
    elapsed=p->now_ms-ch->start_ms;
    if(!framepos(elapsed,clip->fps_milli,&steps,&frac))return 0;
    if(steps>=clip->count){
        if(clip->loop==0UL){slot=clip->count-1UL;frac=0U;}
        else{
            loop_first=clip->count-clip->loop;
            slot=loop_first+(steps-loop_first)%clip->loop;
        }
    }else slot=steps;
    out->frame_a=clip->first+slot;
    if(steps>=clip->count&&clip->loop==0UL)out->frame_b=out->frame_a;
    else if(slot+1UL>=clip->count){
        out->frame_b=clip->loop>0UL?clip->first+(clip->count-clip->loop):out->frame_a;
    }else out->frame_b=out->frame_a+1UL;
    out->lerp_q12=frac;
    if(!cp(out->name,sizeof(out->name),clip->name))return failed("clip name over limit");
    return 1;
}
int rtcw_preview_render(const rtcw_preview *p){
    rtcw_preview_sample legs,torso;int has_legs,has_torso;
    if(!p)return failed("null animation state");
    has_legs=p->legs.enabled!=0U;has_torso=p->torso.enabled!=0U;
    if(!has_legs&&!has_torso)return failed("SHOT requires an active clip");
    if(has_legs&&!rtcw_preview_sample_channel(p,&p->legs,&legs))return 0;
    if(has_torso&&!rtcw_preview_sample_channel(p,&p->torso,&torso))return 0;
    if(!has_legs)legs=torso;
    if(!has_torso)torso=legs;
    if(strcmp(rtcw_current.format,"MDS")==0){
        if(!rtcw_decode_mds_pose(legs.frame_a,legs.frame_b,legs.lerp_q12,
                                 torso.frame_a,torso.frame_b,torso.lerp_q12,0U))return failed(rtcw_error());
    }else{
        if(legs.frame_a!=torso.frame_a||legs.frame_b!=torso.frame_b||legs.lerp_q12!=torso.lerp_q12)
            return failed("separate LEGS/TORSO preview requires MDS skeleton");
        if(!rtcw_decode_blend(legs.frame_a,legs.frame_b,legs.lerp_q12))return failed(rtcw_error());
    }
    return 1;
}
