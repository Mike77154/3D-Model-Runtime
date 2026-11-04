#include "md3_internal.h"

static unsigned int md3_arena_align_up(unsigned int value,
    unsigned int alignment)
{
    unsigned int remainder;

    if (alignment <= 1U)
    {
        return value;
    }

    remainder = value % alignment;
    if (remainder == 0U)
    {
        return value;
    }

    if (value > 0xFFFFFFFFU - (alignment - remainder))
    {
        return 0xFFFFFFFFU;
    }

    return value + (alignment - remainder);
}

void md3_arena_init(md3_arena_t *arena, void *buffer,
    unsigned int capacity_bytes)
{
    if (arena == 0)
    {
        return;
    }

    arena->base = (md3_u8 *) buffer;
    arena->capacity = (buffer == 0) ? 0U : capacity_bytes;
    arena->used = 0U;
}

void md3_arena_reset(md3_arena_t *arena)
{
    if (arena != 0)
    {
        arena->used = 0U;
    }
}

unsigned int md3_arena_mark(const md3_arena_t *arena)
{
    if (arena == 0)
    {
        return 0U;
    }
    return arena->used;
}

void md3_arena_pop(md3_arena_t *arena, unsigned int mark)
{
    if (arena == 0)
    {
        return;
    }
    if (mark <= arena->used)
    {
        arena->used = mark;
    }
}

unsigned int md3_arena_used(const md3_arena_t *arena)
{
    if (arena == 0)
    {
        return 0U;
    }
    return arena->used;
}

unsigned int md3_arena_capacity(const md3_arena_t *arena)
{
    if (arena == 0)
    {
        return 0U;
    }
    return arena->capacity;
}

unsigned int md3_arena_remaining(const md3_arena_t *arena)
{
    if (arena == 0 || arena->used > arena->capacity)
    {
        return 0U;
    }
    return arena->capacity - arena->used;
}

void *md3_arena_alloc(md3_arena_t *arena, unsigned int size_bytes,
    unsigned int alignment)
{
    unsigned int aligned_used;
    unsigned int new_used;
    void *ptr;

    if (arena == 0 || arena->base == 0)
    {
        return 0;
    }

    if (alignment == 0U)
    {
        alignment = 1U;
    }

    aligned_used = md3_arena_align_up(arena->used, alignment);
    if (aligned_used > arena->capacity)
    {
        return 0;
    }
    if (size_bytes > arena->capacity - aligned_used)
    {
        return 0;
    }

    new_used = aligned_used + size_bytes;
    ptr = (void *) (arena->base + aligned_used);
    arena->used = new_used;

    if (size_bytes != 0U)
    {
        md3_zero_memory(ptr, size_bytes);
    }

    return ptr;
}

int md3_arena_alloc_array(md3_arena_t *arena, unsigned int count,
    unsigned int elem_size, unsigned int alignment, void **out_ptr)
{
    unsigned int total;
    void *ptr;

    if (out_ptr == 0)
    {
        return MD3_EINVAL;
    }
    *out_ptr = 0;

    if (elem_size != 0U && count > 0xFFFFFFFFU / elem_size)
    {
        return MD3_ERANGE;
    }

    total = count * elem_size;
    ptr = md3_arena_alloc(arena, total, alignment);
    if (ptr == 0 && total != 0U)
    {
        return MD3_ENOSPACE;
    }

    *out_ptr = ptr;
    return MD3_OK;
}
