#include <stdio.h>
#include <string.h>
#include "dbo_tool_common.h"

static dbo_u8 g_input_a[DBO_TOOL_MAX_FILE_BYTES];
static dbo_u8 g_input_b[DBO_TOOL_MAX_FILE_BYTES];

static int scene_shape_equal(const dbo_scene* a, const dbo_scene* b)
{
    if (a->version != b->version) return 0;
    if (a->reserved1 != b->reserved1) return 0;
    if (a->reserved2 != b->reserved2) return 0;
    if (a->frame_count != b->frame_count) return 0;
    if (a->mesh_count != b->mesh_count) return 0;
    if (a->texture_count != b->texture_count) return 0;
    if (a->bone_count != b->bone_count) return 0;
    if (a->multimaterial_count != b->multimaterial_count) return 0;
    if (a->animation_count != b->animation_count) return 0;
    if (a->animation_set_count != b->animation_set_count) return 0;
    if (dbo_scene_total_vertices(a) != dbo_scene_total_vertices(b)) return 0;
    if (dbo_scene_total_indices(a) != dbo_scene_total_indices(b)) return 0;
    return 1;
}

static int unknown_chunks_equal(const dbo_scene* a, const dbo_scene* b, char* detail)
{
    dbo_u32 i;
    if (detail != 0)
    {
        detail[0] = '\0';
    }
    if (a->unknown_chunk_count != b->unknown_chunk_count)
    {
        if (detail != 0)
        {
            sprintf(detail, "unknown chunk count differs: %lu vs %lu",
                (unsigned long)a->unknown_chunk_count,
                (unsigned long)b->unknown_chunk_count);
        }
        return 0;
    }
    for (i = 0UL; i < a->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* ca = &a->unknown_chunks[i];
        const dbo_unknown_chunk* cb = &b->unknown_chunks[i];
        const void* pa;
        const void* pb;
        if (ca->scope_kind != cb->scope_kind || ca->owner_index != cb->owner_index || ca->code != cb->code || ca->payload.size != cb->payload.size)
        {
            if (detail != 0)
            {
                sprintf(detail, "unknown chunk %lu differs: %s/%lu/code=%lu/size=%lu vs %s/%lu/code=%lu/size=%lu",
                    (unsigned long)i,
                    dbo_tool_scope_name(ca->scope_kind),
                    (unsigned long)ca->owner_index,
                    (unsigned long)ca->code,
                    (unsigned long)ca->payload.size,
                    dbo_tool_scope_name(cb->scope_kind),
                    (unsigned long)cb->owner_index,
                    (unsigned long)cb->code,
                    (unsigned long)cb->payload.size);
            }
            return 0;
        }
        pa = dbo_blob_data(a, ca->payload);
        pb = dbo_blob_data(b, cb->payload);
        if (ca->payload.size > 0UL && memcmp(pa, pb, ca->payload.size) != 0)
        {
            if (detail != 0)
            {
                sprintf(detail, "unknown chunk %lu payload differs", (unsigned long)i);
            }
            return 0;
        }
    }
    return 1;
}

static int chunk_events_equal(const dbo_scene* a, const dbo_scene* b, char* detail)
{
    dbo_u32 i;
    if (detail != 0)
    {
        detail[0] = '\0';
    }
    if (a->chunk_event_count != b->chunk_event_count)
    {
        if (detail != 0)
        {
            sprintf(detail, "chunk event count differs: %lu vs %lu",
                (unsigned long)a->chunk_event_count,
                (unsigned long)b->chunk_event_count);
        }
        return 0;
    }
    for (i = 0UL; i < a->chunk_event_count; ++i)
    {
        const dbo_chunk_event* ea = &a->chunk_events[i];
        const dbo_chunk_event* eb = &b->chunk_events[i];
        if (ea->scope_kind != eb->scope_kind || ea->owner_index != eb->owner_index || ea->code != eb->code
            || ((ea->aux_index == DBO_INDEX_NONE) != (eb->aux_index == DBO_INDEX_NONE)))
        {
            if (detail != 0)
            {
                sprintf(detail, "chunk event %lu differs: %s/%lu/code=%lu/%s vs %s/%lu/code=%lu/%s",
                    (unsigned long)i,
                    dbo_tool_scope_name(ea->scope_kind),
                    (unsigned long)ea->owner_index,
                    (unsigned long)ea->code,
                    ea->aux_index == DBO_INDEX_NONE ? "known" : "unknown",
                    dbo_tool_scope_name(eb->scope_kind),
                    (unsigned long)eb->owner_index,
                    (unsigned long)eb->code,
                    eb->aux_index == DBO_INDEX_NONE ? "known" : "unknown");
            }
            return 0;
        }
    }
    return 1;
}

static void print_human(const char* path_a, const char* path_b,
    const dbo_scene* a, const dbo_scene* b,
    const dbo_validation_report* report_a,
    const dbo_validation_report* report_b,
    dbo_u32 bytes_a, dbo_u32 bytes_b,
    int bytes_equal)
{
    char detail_unknown[256];
    char detail_events[256];
    int shape_equal;
    int unknown_equal;
    int events_equal;

    shape_equal = scene_shape_equal(a, b);
    unknown_equal = unknown_chunks_equal(a, b, detail_unknown);
    events_equal = chunk_events_equal(a, b, detail_events);

    printf("a: %s\n", path_a);
    printf("b: %s\n", path_b);
    printf("byte_equal: %s\n", bytes_equal ? "yes" : "no");
    printf("size: %lu vs %lu\n", (unsigned long)bytes_a, (unsigned long)bytes_b);
    printf("semantic_shape_equal: %s\n", shape_equal ? "yes" : "no");
    printf("unknown_chunks_equal: %s\n", unknown_equal ? "yes" : "no");
    printf("chunk_order_equal: %s\n", events_equal ? "yes" : "no");
    printf("version: %lu vs %lu\n", (unsigned long)a->version, (unsigned long)b->version);
    printf("counts: frames=%lu/%lu meshes=%lu/%lu textures=%lu/%lu bones=%lu/%lu multimaterials=%lu/%lu animations=%lu/%lu animsets=%lu/%lu\n",
        (unsigned long)a->frame_count, (unsigned long)b->frame_count,
        (unsigned long)a->mesh_count, (unsigned long)b->mesh_count,
        (unsigned long)a->texture_count, (unsigned long)b->texture_count,
        (unsigned long)a->bone_count, (unsigned long)b->bone_count,
        (unsigned long)a->multimaterial_count, (unsigned long)b->multimaterial_count,
        (unsigned long)a->animation_count, (unsigned long)b->animation_count,
        (unsigned long)a->animation_set_count, (unsigned long)b->animation_set_count);
    printf("totals: vertices=%lu/%lu indices=%lu/%lu unknown_chunks=%lu/%lu unknown_bytes=%lu/%lu chunk_events=%lu/%lu\n",
        (unsigned long)dbo_scene_total_vertices(a), (unsigned long)dbo_scene_total_vertices(b),
        (unsigned long)dbo_scene_total_indices(a), (unsigned long)dbo_scene_total_indices(b),
        (unsigned long)dbo_scene_unknown_chunk_count(a), (unsigned long)dbo_scene_unknown_chunk_count(b),
        (unsigned long)dbo_scene_unknown_bytes(a), (unsigned long)dbo_scene_unknown_bytes(b),
        (unsigned long)dbo_scene_chunk_event_count(a), (unsigned long)dbo_scene_chunk_event_count(b));
    printf("validation: errors=%lu/%lu warnings=%lu/%lu\n",
        (unsigned long)report_a->errors, (unsigned long)report_b->errors,
        (unsigned long)report_a->warnings, (unsigned long)report_b->warnings);
    if (!unknown_equal)
    {
        printf("first unknown difference: %s\n", detail_unknown);
    }
    if (!events_equal)
    {
        printf("first chunk-order difference: %s\n", detail_events);
    }
}

static void print_json(const char* path_a, const char* path_b,
    const dbo_scene* a, const dbo_scene* b,
    const dbo_validation_report* report_a,
    const dbo_validation_report* report_b,
    dbo_u32 bytes_a, dbo_u32 bytes_b,
    int bytes_equal)
{
    char detail_unknown[256];
    char detail_events[256];
    int shape_equal;
    int unknown_equal;
    int events_equal;

    shape_equal = scene_shape_equal(a, b);
    unknown_equal = unknown_chunks_equal(a, b, detail_unknown);
    events_equal = chunk_events_equal(a, b, detail_events);

    printf("{\n");
    printf("  \"a\": "); dbo_tool_json_write_string(stdout, path_a); printf(",\n");
    printf("  \"b\": "); dbo_tool_json_write_string(stdout, path_b); printf(",\n");
    printf("  \"byte_equal\": %s,\n", bytes_equal ? "true" : "false");
    printf("  \"semantic_shape_equal\": %s,\n", shape_equal ? "true" : "false");
    printf("  \"unknown_chunks_equal\": %s,\n", unknown_equal ? "true" : "false");
    printf("  \"chunk_order_equal\": %s,\n", events_equal ? "true" : "false");
    printf("  \"sizes\": {\"a\": %lu, \"b\": %lu},\n", (unsigned long)bytes_a, (unsigned long)bytes_b);
    printf("  \"versions\": {\"a\": %lu, \"b\": %lu},\n", (unsigned long)a->version, (unsigned long)b->version);
    printf("  \"counts\": {\"frames\": [%lu,%lu], \"meshes\": [%lu,%lu], \"textures\": [%lu,%lu], \"bones\": [%lu,%lu], \"multimaterials\": [%lu,%lu], \"animations\": [%lu,%lu], \"animation_sets\": [%lu,%lu]},\n",
        (unsigned long)a->frame_count, (unsigned long)b->frame_count,
        (unsigned long)a->mesh_count, (unsigned long)b->mesh_count,
        (unsigned long)a->texture_count, (unsigned long)b->texture_count,
        (unsigned long)a->bone_count, (unsigned long)b->bone_count,
        (unsigned long)a->multimaterial_count, (unsigned long)b->multimaterial_count,
        (unsigned long)a->animation_count, (unsigned long)b->animation_count,
        (unsigned long)a->animation_set_count, (unsigned long)b->animation_set_count);
    printf("  \"totals\": {\"vertices\": [%lu,%lu], \"indices\": [%lu,%lu], \"unknown_chunks\": [%lu,%lu], \"unknown_bytes\": [%lu,%lu], \"chunk_events\": [%lu,%lu]},\n",
        (unsigned long)dbo_scene_total_vertices(a), (unsigned long)dbo_scene_total_vertices(b),
        (unsigned long)dbo_scene_total_indices(a), (unsigned long)dbo_scene_total_indices(b),
        (unsigned long)dbo_scene_unknown_chunk_count(a), (unsigned long)dbo_scene_unknown_chunk_count(b),
        (unsigned long)dbo_scene_unknown_bytes(a), (unsigned long)dbo_scene_unknown_bytes(b),
        (unsigned long)dbo_scene_chunk_event_count(a), (unsigned long)dbo_scene_chunk_event_count(b));
    printf("  \"validation\": {\"errors\": [%lu,%lu], \"warnings\": [%lu,%lu]},\n",
        (unsigned long)report_a->errors, (unsigned long)report_b->errors,
        (unsigned long)report_a->warnings, (unsigned long)report_b->warnings);
    printf("  \"first_unknown_difference\": ");
    dbo_tool_json_write_string(stdout, detail_unknown);
    printf(",\n");
    printf("  \"first_chunk_order_difference\": ");
    dbo_tool_json_write_string(stdout, detail_events);
    printf("\n}\n");
}

int main(int argc, char** argv)
{
    const char* path_a;
    const char* path_b;
    int json_mode;
    char err[DBO_MAX_ERROR_TEXT];
    dbo_u32 size_a;
    dbo_u32 size_b;
    int bytes_equal;
    static dbo_scene scene_a;
    static dbo_scene scene_b;
    static dbo_validation_report report_a;
    static dbo_validation_report report_b;

    path_a = 0;
    path_b = 0;
    json_mode = 0;

    if (argc == 4 && strcmp(argv[1], "--json") == 0)
    {
        json_mode = 1;
        path_a = argv[2];
        path_b = argv[3];
    }
    else if (argc == 3)
    {
        path_a = argv[1];
        path_b = argv[2];
    }
    else
    {
        fprintf(stderr, "usage: %s [--json] <a.dbo> <b.dbo>\n", argv[0]);
        return 2;
    }

    if (!dbo_tool_load_scene(path_a, &scene_a, g_input_a, (dbo_u32)sizeof(g_input_a), &size_a, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "load failed for a: %s\n", err);
        return 2;
    }
    if (!dbo_tool_load_scene(path_b, &scene_b, g_input_b, (dbo_u32)sizeof(g_input_b), &size_b, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "load failed for b: %s\n", err);
        return 2;
    }

    dbo_validate_scene(&scene_a, &report_a);
    dbo_validate_scene(&scene_b, &report_b);

    bytes_equal = (size_a == size_b && memcmp(g_input_a, g_input_b, size_a) == 0) ? 1 : 0;

    if (json_mode)
    {
        print_json(path_a, path_b, &scene_a, &scene_b, &report_a, &report_b, size_a, size_b, bytes_equal);
    }
    else
    {
        print_human(path_a, path_b, &scene_a, &scene_b, &report_a, &report_b, size_a, size_b, bytes_equal);
    }

    return bytes_equal ? 0 : 1;
}
