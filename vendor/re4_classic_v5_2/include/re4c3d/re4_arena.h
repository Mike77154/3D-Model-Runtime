#ifndef RE4_ARENA_H
#define RE4_ARENA_H

#include "re4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct re4_arena_s {
    re4_u8 *base;
    re4_size cap;
    re4_size pos;
    re4_size high_water;
} re4_arena;

void re4_arena_init(re4_arena *a, void *memory, re4_size cap);
void re4_arena_reset(re4_arena *a);
void *re4_arena_push(re4_arena *a, re4_size amount, re4_size align_to);
re4_size re4_arena_remaining(const re4_arena *a);

#ifdef __cplusplus
}
#endif

#endif
