/* Synthetic-only event semantics. No RE4 opcode meanings asserted. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define T(x) do{if(!(x)){fprintf(stderr,"STATE_TEST_FAIL:%d\n",__LINE__);return 1;}}while(0)
static void key(re4_u8 *s,re4_u32 i,re4_u8 id,re4_u8 aux,re4_u16 q6)
{
    re4_u32 pos=4U+4U*i;
    s[pos]=id;s[pos+1U]=aux;s[pos+2U]=(re4_u8)q6;
    s[pos+3U]=(re4_u8)(q6>>8);
}
int main(void)
{
    re4_u8 bytes[24];re4_span buf;re4_seq_timeline timeline;
    re4_seq_state state,other,original;re4_seq_state_stats stats;
    re4_seq_state_rule rules[4];re4_err e;
    memset(bytes,0,sizeof(bytes));bytes[0]=4U;
    key(bytes,0,7U,1U,0U);key(bytes,1,9U,0U,32U);
    key(bytes,2,10U,0U,32U);key(bytes,3,7U,2U,64U);
    buf.data=bytes;buf.size=20U;
    T(re4_seq_timeline_read(buf,RE4_ENDIAN_LITTLE,&timeline)==RE4_OK);
    memset(rules,0,sizeof(rules));
    rules[0].event_code=7U;rules[0].auxiliary=1U;rules[0].match_auxiliary=1U;
    rules[0].action=RE4_SEQ_ACTION_SHOW;rules[0].target=3U;
    rules[1].event_code=7U;rules[1].auxiliary=2U;rules[1].match_auxiliary=1U;
    rules[1].action=RE4_SEQ_ACTION_HIDE;rules[1].target=3U;
    rules[2].event_code=9U;rules[2].action=RE4_SEQ_ACTION_SET_POSE;rules[2].value=4U;
    rules[3].event_code=10U;rules[3].action=RE4_SEQ_ACTION_SET_PARENT;
    rules[3].target=3U;rules[3].value=6U;
    /* Pose action does not use target; 255 must never shift a uint32. */
    rules[2].target=255U;
    T(re4_seq_state_rules_validate(rules,4U)==RE4_OK);
    re4_seq_state_init(&state,0U,0U);
    e=re4_seq_state_advance(&timeline,128U,0U,33U,2U,rules,4U,
                            RE4_TRUE,&state,&stats);
    T(e==RE4_OK && stats.applied==3U && stats.unmapped==0U);
    T((state.visible_mask&(1U<<3))!=0U && state.pose_id==4U &&
      state.parent[3]==6U && state.cursor_q6==33U);
    T(re4_seq_state_advance(&timeline,128U,33U,65U,2U,rules,4U,
                            RE4_TRUE,&state,&stats)==RE4_OK);
    T((state.visible_mask&(1U<<3))==0U && stats.applied==1U);
    /* Seeking with tie and two loops reproduces sequential advancement. */
    T(re4_seq_state_advance(&timeline,128U,65U,257U,3U,rules,4U,
                            RE4_TRUE,&state,&stats)==RE4_OK);
    T(re4_seq_state_seek(&timeline,128U,257U,3U,rules,4U,RE4_TRUE,
                         0U,0U,&other,&stats)==RE4_OK);
    T(memcmp(&state,&other,sizeof(state))==0);
    /* Strict unknown event must not alter ANY part of state. */
    original=state;key(bytes,2,55U,0U,32U);
    T(re4_seq_state_advance(&timeline,128U,257U,385U,2U,rules,4U,
                            RE4_TRUE,&state,&stats)==RE4_ERR_UNSUPPORTED);
    T(memcmp(&state,&original,sizeof(state))==0);
    T(re4_seq_state_advance(&timeline,128U,257U,385U,2U,rules,4U,
                            RE4_FALSE,&state,&stats)==RE4_OK);
    T(stats.unmapped==1U);
    /* Ambiguous wildcard/exact same code, invalid parent and rule cap. */
    rules[1].match_auxiliary=0U;
    T(re4_seq_state_rules_validate(rules,4U)==RE4_ERR_BAD_FORMAT);
    rules[1].match_auxiliary=1U;
    rules[3].value=33U;
    T(re4_seq_state_rules_validate(rules,4U)==RE4_ERR_RANGE);
    rules[3].value=6U;
    T(re4_seq_state_rules_validate(rules,257U)==RE4_ERR_RANGE);
    T(re4_seq_state_advance(&timeline,128U,0U,10U,2U,rules,4U,
                            RE4_FALSE,&state,&stats)==RE4_ERR_RANGE);
    puts("SEQ state bridge: loop, seek, ties, strict transactional rollback, aux, parents OK");
    return 0;
}
