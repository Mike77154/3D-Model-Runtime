#include "re4c3d/re4_arena.h"
#include <string.h>

void re4_arena_init(re4_arena *a, void *memory, re4_size cap)
{
    if (a == 0) return;
    a->base = (re4_u8 *)memory;
    a->cap = cap;
    a->pos = 0;
    a->high_water = 0;
}

void re4_arena_reset(re4_arena *a)
{
    if (a == 0) return;
    a->pos = 0;
}

/* Align the ABSOLUTE address, not merely the arena's logical offset.
   sizeof(void*) is used for structs containing pointers. The address-to-
   size_t conversion is implementation-defined C89 but supported by the
   targeted Windows / Unix toolchains; only the low alignment bits matter. */
void *re4_arena_push(re4_arena *a, re4_size amount, re4_size align_to)
{
    re4_size p,rem,pad,n;
    re4_size mask;
    void *out;
    if (a == 0 || a->base == 0) return 0;
    if (a->pos > a->cap) return 0;
    p = a->pos;
    if (align_to != 0UL) {
        mask = align_to - 1UL;
        if ((align_to & mask) != 0UL) return 0;
        rem = (re4_size)(((size_t)(const void *)a->base + (size_t)p) & (size_t)mask);
        pad = (rem == 0UL) ? 0UL : (align_to - rem);
        if (pad > a->cap - p) return 0;
        p += pad;
    }
    if (amount > a->cap - p) return 0;
    out = a->base + p;
    n = p + amount;
    memset(out, 0, (size_t)amount);
    a->pos = n;
    if (a->high_water < n) a->high_water = n;
    return out;
}

re4_size re4_arena_remaining(const re4_arena *a)
{
    if (a == 0) return 0;
    if (a->pos > a->cap) return 0;
    return a->cap - a->pos;
}
