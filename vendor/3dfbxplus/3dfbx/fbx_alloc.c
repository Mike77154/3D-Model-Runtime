#include "fbx_alloc.h"

#include <string.h>

#ifndef FBX_DEFAULT_ARENA_SIZE
#define FBX_DEFAULT_ARENA_SIZE (32u * 1024u * 1024u)
#endif

typedef struct FbxAllocHeaderTag {
    size_t size;
} FbxAllocHeader;

static unsigned char g_fbx_default_block[FBX_DEFAULT_ARENA_SIZE];
static FbxArena g_fbx_default_arena = { g_fbx_default_block, (size_t)FBX_DEFAULT_ARENA_SIZE, 0u, 0u, 0u };
static FbxArena *g_fbx_current_arena = &g_fbx_default_arena;

static size_t fbx_align_size(size_t v)
{
    size_t a;
    a = sizeof(void *);
    if (a < sizeof(size_t)) a = sizeof(size_t);
    return (v + (a - 1u)) & ~(a - 1u);
}

void fbx_arena_init(FbxArena *arena, void *memory, size_t capacity)
{
    if (!arena) return;
    arena->base = (unsigned char *)memory;
    arena->capacity = capacity;
    arena->used = 0u;
    arena->peak_used = 0u;
    arena->failed = 0u;
}

void fbx_arena_reset(FbxArena *arena)
{
    if (!arena) return;
    arena->used = 0u;
    arena->peak_used = 0u;
    arena->failed = 0u;
}

void fbx_alloc_set_arena(FbxArena *arena)
{
    if (arena && arena->base && arena->capacity > 0u) {
        g_fbx_current_arena = arena;
    } else {
        g_fbx_current_arena = &g_fbx_default_arena;
    }
}

FbxArena *fbx_alloc_get_arena(void)
{
    return g_fbx_current_arena;
}

void *fbx_alloc(size_t size)
{
    FbxArena *arena;
    size_t total;
    size_t pos;
    FbxAllocHeader *hdr;

    if (size == 0u) size = 1u;
    arena = g_fbx_current_arena;
    if (!arena || !arena->base) return NULL;

    total = fbx_align_size(sizeof(FbxAllocHeader)) + fbx_align_size(size);
    pos = fbx_align_size(arena->used);

    if (pos > arena->capacity || total > arena->capacity - pos) {
        arena->failed += 1u;
        return NULL;
    }

    hdr = (FbxAllocHeader *)(void *)(arena->base + pos);
    hdr->size = size;
    arena->used = pos + total;
    if (arena->used > arena->peak_used) arena->peak_used = arena->used;

    return (void *)(arena->base + pos + fbx_align_size(sizeof(FbxAllocHeader)));
}

void *fbx_alloc_zero_n(size_t count, size_t size)
{
    void *p;
    size_t total;

    if (count != 0u && size > ((size_t)-1) / count) {
        FbxArena *arena = g_fbx_current_arena;
        if (arena) arena->failed += 1u;
        return NULL;
    }
    total = count * size;
    p = fbx_alloc(total);
    if (p && total > 0u) memset(p, 0, total);
    return p;
}

void *fbx_resize(void *ptr, size_t new_size)
{
    void *new_ptr;
    size_t old_size;
    FbxAllocHeader *hdr;

    if (!ptr) return fbx_alloc(new_size);
    if (new_size == 0u) new_size = 1u;

    hdr = (FbxAllocHeader *)((unsigned char *)ptr - fbx_align_size(sizeof(FbxAllocHeader)));
    old_size = hdr->size;

    new_ptr = fbx_alloc(new_size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
    return new_ptr;
}

void fbx_release(void *ptr)
{
    (void)ptr;
}

char *fbx_strdup(const char *s)
{
    size_t len;
    char *out;
    if (!s) return NULL;
    len = strlen(s);
    out = (char *)fbx_alloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}
