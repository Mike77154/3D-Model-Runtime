#include "re4c3d/re4_gameplay.h"
#include "re4c3d/re4_base.h"
#include <string.h>

#define RE4_ESL_ENTRY_SIZE 32UL
#define RE4_ESL_MAX_ENTRIES 255UL

re4_err re4_esl_read(re4_span src, re4_arena *arena, re4_esl *out)
{
    re4_u32 n;
    re4_u32 i;
    re4_esl_entry *entries;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (src.size < RE4_ESL_ENTRY_SIZE) return RE4_ERR_RANGE;
    n = (re4_u32)(src.size / RE4_ESL_ENTRY_SIZE);
    if (n > RE4_ESL_MAX_ENTRIES) n = (re4_u32)RE4_ESL_MAX_ENTRIES;
    entries = (re4_esl_entry *)re4_arena_push(arena, (re4_size)n * sizeof(re4_esl_entry), sizeof(re4_u32));
    if (n != 0 && entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < n; ++i) {
        const re4_u8 *p;
        p = src.data + ((re4_size)i * RE4_ESL_ENTRY_SIZE);
        entries[i].be_flag = p[0];
        entries[i].id = p[1];
        entries[i].type = p[2];
        entries[i].set = p[3];
        entries[i].flag = re4_read_u32_le(p + 4);
        entries[i].hp = (re4_i16)re4_read_u16_le(p + 8);
        entries[i].emset_no = p[10];
        entries[i].character = p[11];
        entries[i].pos[0] = re4_read_u16_le(p + 12);
        entries[i].pos[1] = re4_read_u16_le(p + 14);
        entries[i].pos[2] = re4_read_u16_le(p + 16);
        entries[i].ang[0] = re4_read_u16_le(p + 18);
        entries[i].ang[1] = re4_read_u16_le(p + 20);
        entries[i].ang[2] = re4_read_u16_le(p + 22);
        entries[i].room = re4_read_u16_le(p + 24);
        entries[i].guard_r = (re4_i16)re4_read_u16_le(p + 26);
        entries[i].percentage_motion_speed = re4_read_u16_le(p + 28);
        entries[i].percentage_scale = re4_read_u16_le(p + 30);
    }
    out->entry_count = n;
    out->entries = entries;
    return RE4_OK;
}
