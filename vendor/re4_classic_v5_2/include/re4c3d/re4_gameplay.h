#ifndef RE4_GAMEPLAY_H
#define RE4_GAMEPLAY_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct re4_esl_entry_s {
    re4_u8 be_flag;
    re4_u8 id;
    re4_u8 type;
    re4_u8 set;
    re4_u32 flag;
    re4_i16 hp;
    re4_u8 emset_no;
    re4_u8 character;
    re4_u16 pos[3];
    re4_u16 ang[3];
    re4_u16 room;
    re4_i16 guard_r;
    re4_u16 percentage_motion_speed;
    re4_u16 percentage_scale;
} re4_esl_entry;

typedef struct re4_esl_s {
    re4_u32 entry_count;
    re4_esl_entry *entries;
} re4_esl;

re4_err re4_esl_read(re4_span src, re4_arena *arena, re4_esl *out);

#ifdef __cplusplus
}
#endif

#endif
