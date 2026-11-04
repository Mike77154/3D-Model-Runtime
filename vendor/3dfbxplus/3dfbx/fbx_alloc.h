#ifndef FBX_ALLOC_H
#define FBX_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxArenaTag {
    unsigned char *base;
    size_t capacity;
    size_t used;
    size_t peak_used;
    unsigned int failed;
} FbxArena;

void  fbx_arena_init(FbxArena *arena, void *memory, size_t capacity);
void  fbx_arena_reset(FbxArena *arena);
void  fbx_alloc_set_arena(FbxArena *arena);
FbxArena *fbx_alloc_get_arena(void);

void *fbx_alloc(size_t size);
void *fbx_alloc_zero_n(size_t count, size_t size);
void *fbx_resize(void *ptr, size_t new_size);
void  fbx_release(void *ptr);
char *fbx_strdup(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* FBX_ALLOC_H */
