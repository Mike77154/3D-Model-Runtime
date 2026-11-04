/* Explicit, deterministic SEQ-to-visual-state bridge. No guessed game opcode
   meanings: the user must independently verify each binding. Strict C89. */
#include "re4c3d/re4_seq_state.h"
#include "re4c3d/re4_base.h"
#include <string.h>

typedef struct state_context_s {
    re4_seq_state work;
    re4_seq_state_stats counts;
    const re4_seq_state_rule *rules;
    re4_u32 rule_count;
    re4_bool strict;
} state_context;

void re4_seq_state_init(re4_seq_state *s,re4_u32 visible,re4_u8 pose)
{
    re4_u32 i;
    if(!s)return;
    memset(s,0,sizeof(*s));
    s->visible_mask=visible;s->pose_id=pose;s->initialized=1U;
    for(i=0U;i<RE4_SEQ_STATE_PARTS;++i)s->parent[i]=RE4_SEQ_PARENT_NONE;
}

re4_err re4_seq_state_rules_validate(const re4_seq_state_rule *r,re4_u32 n)
{
    re4_u32 i,j;
    if(n && !r)return RE4_ERR_NULL;
    if(n>256U)return RE4_ERR_RANGE;
    for(i=0U;i<n;++i){
        if(!r[i].event_code || r[i].match_auxiliary>1U)return RE4_ERR_BAD_FORMAT;
        if(r[i].action<RE4_SEQ_ACTION_SHOW ||
           r[i].action>RE4_SEQ_ACTION_CLEAR_PARENT)return RE4_ERR_BAD_FORMAT;
        if(r[i].action!=RE4_SEQ_ACTION_SET_POSE &&
           r[i].target>=RE4_SEQ_STATE_PARTS)return RE4_ERR_RANGE;
        if(r[i].action==RE4_SEQ_ACTION_SET_PARENT &&
           r[i].value>=RE4_SEQ_STATE_PARTS)return RE4_ERR_RANGE;
        /* Wildcards and exact matches of same event would be ambiguous. */
        for(j=0U;j<i;++j){
            if(r[i].event_code==r[j].event_code &&
               (!r[i].match_auxiliary || !r[j].match_auxiliary ||
                r[i].auxiliary==r[j].auxiliary))return RE4_ERR_BAD_FORMAT;
        }
    }
    return RE4_OK;
}

static re4_err state_fire(void *opaque,const re4_seq_fire *ev)
{
    state_context *ctx=(state_context *)opaque;
    const re4_seq_state_rule *rule=0;
    re4_u32 i,mask;
    if(!ev->key.se)return RE4_OK;
    ++ctx->counts.raw_nonzero;
    ctx->counts.last_absolute_q6=ev->absolute_q6;
    for(i=0U;i<ctx->rule_count;++i){
        const re4_seq_state_rule *candidate=&ctx->rules[i];
        if(candidate->event_code==ev->key.se &&
           (!candidate->match_auxiliary || candidate->auxiliary==ev->key.flags)){
            rule=candidate;break;
        }
    }
    if(!rule){
        ++ctx->counts.unmapped;
        return ctx->strict?RE4_ERR_UNSUPPORTED:RE4_OK;
    }
    mask=0U;
    if(rule->action!=RE4_SEQ_ACTION_SET_POSE)
        mask=(re4_u32)1U << rule->target;
    switch(rule->action){
    case RE4_SEQ_ACTION_SHOW:ctx->work.visible_mask|=mask;break;
    case RE4_SEQ_ACTION_HIDE:ctx->work.visible_mask&=~mask;break;
    case RE4_SEQ_ACTION_SET_POSE:ctx->work.pose_id=rule->value;break;
    case RE4_SEQ_ACTION_SET_PARENT:ctx->work.parent[rule->target]=rule->value;break;
    case RE4_SEQ_ACTION_CLEAR_PARENT:
        ctx->work.parent[rule->target]=RE4_SEQ_PARENT_NONE;break;
    default:return RE4_ERR_BAD_FORMAT;
    }
    ++ctx->counts.applied;
    return RE4_OK;
}

re4_err re4_seq_state_advance(const re4_seq_timeline *seq,
    re4_u32 period,re4_u32 from,re4_u32 to,re4_u32 max_loops,
    const re4_seq_state_rule *rules,re4_u32 n,re4_bool strict,
    re4_seq_state *state,re4_seq_state_stats *stats)
{
    re4_err err;re4_u32 fired;
    state_context ctx;
    if(!seq || !state || !stats)return RE4_ERR_NULL;
    memset(stats,0,sizeof(*stats));
    if(!state->initialized || state->cursor_q6!=from || to<from)
        return RE4_ERR_RANGE;
    err=re4_seq_state_rules_validate(rules,n);
    if(err!=RE4_OK)return err;
    memset(&ctx,0,sizeof(ctx));
    ctx.work=*state;ctx.rules=rules;ctx.rule_count=n;ctx.strict=strict;
    err=re4_seq_dispatch_range(seq,period,from,to,max_loops,RE4_FALSE,
                               state_fire,&ctx,&fired);
    if(err!=RE4_OK)return err; /* Transactional: original state untouched. */
    ctx.work.cursor_q6=to;
    *state=ctx.work;
    *stats=ctx.counts;
    return RE4_OK;
}

re4_err re4_seq_state_seek(const re4_seq_timeline *seq,
    re4_u32 period,re4_u32 to,re4_u32 max_loops,
    const re4_seq_state_rule *rules,re4_u32 n,re4_bool strict,
    re4_u32 initial_visible,re4_u8 initial_pose,
    re4_seq_state *out,re4_seq_state_stats *stats)
{
    re4_seq_state candidate;re4_seq_state_stats report;
    re4_err err;
    if(!seq || !out || !stats)return RE4_ERR_NULL;
    re4_seq_state_init(&candidate,initial_visible,initial_pose);
    err=re4_seq_state_advance(seq,period,0U,to,max_loops,rules,n,strict,
                               &candidate,&report);
    if(err!=RE4_OK)return err;
    *out=candidate;*stats=report;
    return RE4_OK;
}
