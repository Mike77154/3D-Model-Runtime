#ifndef MD3_ARENA_H
#define MD3_ARENA_H

#include "md3_types.h"

typedef struct md3_arena_s
{
    md3_u8 *base;
    unsigned int capacity;
    unsigned int used;
} md3_arena_t;

void md3_arena_init(md3_arena_t *arena, void *buffer,
    unsigned int capacity_bytes);
void md3_arena_reset(md3_arena_t *arena);
unsigned int md3_arena_mark(const md3_arena_t *arena);
void md3_arena_pop(md3_arena_t *arena, unsigned int mark);
unsigned int md3_arena_used(const md3_arena_t *arena);
unsigned int md3_arena_capacity(const md3_arena_t *arena);
unsigned int md3_arena_remaining(const md3_arena_t *arena);
void *md3_arena_alloc(md3_arena_t *arena, unsigned int size_bytes,
    unsigned int alignment);
int md3_arena_alloc_array(md3_arena_t *arena, unsigned int count,
    unsigned int elem_size, unsigned int alignment, void **out_ptr);

#endif
