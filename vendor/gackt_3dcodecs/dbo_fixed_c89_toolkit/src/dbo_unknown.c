#include "dbo_int.h"

dbo_u32 dbo_scene_unknown_chunk_count(const dbo_scene* scene)
{
    if (scene == 0)
    {
        return 0UL;
    }
    return scene->unknown_chunk_count;
}

dbo_u32 dbo_scene_unknown_bytes(const dbo_scene* scene)
{
    dbo_u32 i;
    dbo_u32 total;
    if (scene == 0)
    {
        return 0UL;
    }
    total = 0UL;
    for (i = 0UL; i < scene->unknown_chunk_count; ++i)
    {
        total += scene->unknown_chunks[i].payload.size;
    }
    return total;
}

dbo_u32 dbo_scene_unknown_chunks_for_scope(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index)
{
    dbo_u32 i;
    dbo_u32 total;
    if (scene == 0)
    {
        return 0UL;
    }
    total = 0UL;
    for (i = 0UL; i < scene->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* chunk = &scene->unknown_chunks[i];
        if (chunk->scope_kind == scope_kind && chunk->owner_index == owner_index)
        {
            ++total;
        }
    }
    return total;
}

dbo_u32 dbo_scene_chunk_event_count(const dbo_scene* scene)
{
    if (scene == 0)
    {
        return 0UL;
    }
    return scene->chunk_event_count;
}

dbo_u32 dbo_scene_chunk_events_for_scope(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index)
{
    dbo_u32 i;
    dbo_u32 total;
    if (scene == 0)
    {
        return 0UL;
    }
    total = 0UL;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind == scope_kind && event->owner_index == owner_index)
        {
            ++total;
        }
    }
    return total;
}

void dbo_scene_strip_unknown_chunks(dbo_scene* scene)
{
    dbo_u32 src;
    dbo_u32 dst;
    if (scene == 0)
    {
        return;
    }

    dst = 0UL;
    for (src = 0UL; src < scene->chunk_event_count; ++src)
    {
        const dbo_chunk_event* event = &scene->chunk_events[src];
        if (event->aux_index != DBO_INDEX_NONE)
        {
            continue;
        }
        if (dst != src)
        {
            scene->chunk_events[dst] = scene->chunk_events[src];
        }
        ++dst;
    }
    scene->chunk_event_count = dst;
    scene->unknown_chunk_count = 0UL;
}

void dbo_scene_strip_chunk_order(dbo_scene* scene)
{
    if (scene == 0)
    {
        return;
    }
    scene->chunk_event_count = 0UL;
}

dbo_bool dbo__scene_store_unknown_chunk(dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code, const void* payload, dbo_u32 size)
{
    dbo_unknown_chunk* chunk;
    dbo_blob_ref ref;

    if (scene == 0)
    {
        return DBO_FALSE;
    }
    if (scene->unknown_chunk_count >= DBO_MAX_UNKNOWN_CHUNKS)
    {
        dbo__set_error(scene, "unknown chunk table full");
        return DBO_FALSE;
    }
    if (!dbo_scene_copy_bytes(scene, payload, size, 4UL, &ref))
    {
        return DBO_FALSE;
    }

    chunk = &scene->unknown_chunks[scene->unknown_chunk_count++];
    chunk->scope_kind = scope_kind;
    chunk->owner_index = owner_index;
    chunk->code = code;
    chunk->payload = ref;
    return DBO_TRUE;
}

dbo_bool dbo__scene_store_chunk_event(dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code, dbo_u32 aux_index, dbo_u32* out_index)
{
    dbo_u32 idx;
    dbo_chunk_event* event;

    if (scene == 0)
    {
        return DBO_FALSE;
    }
    if (scene->chunk_event_count >= DBO_MAX_CHUNK_EVENTS)
    {
        dbo__set_error(scene, "chunk event table full");
        return DBO_FALSE;
    }

    idx = scene->chunk_event_count++;
    event = &scene->chunk_events[idx];
    event->scope_kind = scope_kind;
    event->owner_index = owner_index;
    event->code = code;
    event->aux_index = aux_index;

    if (out_index != 0)
    {
        *out_index = idx;
    }
    return DBO_TRUE;
}

dbo_bool dbo__write_unknown_chunks_for_scope(dbo_writer* w, dbo_u32 scope_kind, dbo_u32 owner_index)
{
    dbo_u32 i;
    if (w == 0 || w->scene == 0)
    {
        return DBO_FALSE;
    }

    for (i = 0UL; i < w->scene->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* chunk = &w->scene->unknown_chunks[i];
        const void* data;
        if (chunk->scope_kind != scope_kind || chunk->owner_index != owner_index)
        {
            continue;
        }
        data = dbo_blob_data(w->scene, chunk->payload);
        if (!dbo__write_u32(w, chunk->code)) return DBO_FALSE;
        if (!dbo__write_u32(w, chunk->payload.size)) return DBO_FALSE;
        if (!dbo__write_bytes(w, data, chunk->payload.size)) return DBO_FALSE;
    }
    return DBO_TRUE;
}
