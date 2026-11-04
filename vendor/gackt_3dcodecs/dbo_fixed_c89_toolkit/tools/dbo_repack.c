#include <stdio.h>
#include <string.h>
#include "dbo_tool_common.h"

static dbo_u8 g_input[DBO_TOOL_MAX_FILE_BYTES];
static dbo_u8 g_output[DBO_TOOL_MAX_FILE_BYTES];

static int scene_shape_equal(const dbo_scene* a, const dbo_scene* b)
{
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

static int unknown_chunks_equal(const dbo_scene* a, const dbo_scene* b)
{
    dbo_u32 i;
    if (a->unknown_chunk_count != b->unknown_chunk_count)
    {
        return 0;
    }
    for (i = 0UL; i < a->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* ca = &a->unknown_chunks[i];
        const dbo_unknown_chunk* cb = &b->unknown_chunks[i];
        const void* pa;
        const void* pb;
        if (ca->scope_kind != cb->scope_kind) return 0;
        if (ca->owner_index != cb->owner_index) return 0;
        if (ca->code != cb->code) return 0;
        if (ca->payload.size != cb->payload.size) return 0;
        pa = dbo_blob_data(a, ca->payload);
        pb = dbo_blob_data(b, cb->payload);
        if (ca->payload.size > 0UL && memcmp(pa, pb, ca->payload.size) != 0) return 0;
    }
    return 1;
}

static int chunk_events_equal(const dbo_scene* a, const dbo_scene* b)
{
    dbo_u32 i;
    if (a->chunk_event_count != b->chunk_event_count)
    {
        return 0;
    }
    for (i = 0UL; i < a->chunk_event_count; ++i)
    {
        const dbo_chunk_event* ea = &a->chunk_events[i];
        const dbo_chunk_event* eb = &b->chunk_events[i];
        if (ea->scope_kind != eb->scope_kind) return 0;
        if (ea->owner_index != eb->owner_index) return 0;
        if (ea->code != eb->code) return 0;
        if ((ea->aux_index == DBO_INDEX_NONE) != (eb->aux_index == DBO_INDEX_NONE)) return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    char err[DBO_MAX_ERROR_TEXT];
    dbo_u32 in_size;
    dbo_u32 out_size;
    dbo_u32 original_unknown_chunks;
    dbo_u32 original_unknown_bytes;
    dbo_u32 original_event_count;
    const char* input_path;
    const char* output_path;
    int strip_unknown;
    int canonical_order;
    int argi;
    int bytes_equal;
    int events_equal;
    static dbo_scene source;
    static dbo_scene verify;
    static dbo_validation_report report_a;
    static dbo_validation_report report_b;

    input_path = 0;
    output_path = 0;
    strip_unknown = 0;
    canonical_order = 0;
    original_unknown_chunks = 0UL;
    original_unknown_bytes = 0UL;
    original_event_count = 0UL;
    bytes_equal = 0;
    events_equal = 0;

    if (argc < 3)
    {
        fprintf(stderr, "usage: %s [--strip-unknown] [--canonical-order] <input.dbo> <output.dbo>\n", argv[0]);
        return 1;
    }

    for (argi = 1; argi < argc - 2; ++argi)
    {
        if (strcmp(argv[argi], "--strip-unknown") == 0)
        {
            strip_unknown = 1;
        }
        else if (strcmp(argv[argi], "--canonical-order") == 0)
        {
            canonical_order = 1;
        }
        else
        {
            fprintf(stderr, "unknown option: %s\n", argv[argi]);
            return 1;
        }
    }

    input_path = argv[argc - 2];
    output_path = argv[argc - 1];

    if (!dbo_tool_load_scene(input_path, &source, g_input, (dbo_u32)sizeof(g_input), &in_size, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "load failed: %s\n", err);
        return 1;
    }

    if (!dbo_validate_scene(&source, &report_a))
    {
        fprintf(stderr, "source validation failed: %s\n", report_a.first_error);
        return 2;
    }

    original_unknown_chunks = dbo_scene_unknown_chunk_count(&source);
    original_unknown_bytes = dbo_scene_unknown_bytes(&source);
    original_event_count = dbo_scene_chunk_event_count(&source);

    if (strip_unknown)
    {
        dbo_scene_strip_unknown_chunks(&source);
    }
    if (canonical_order)
    {
        dbo_scene_strip_chunk_order(&source);
    }

    if (!dbo_measure_encoded_size(&source, &out_size))
    {
        fprintf(stderr, "measure failed\n");
        return 1;
    }
    if (out_size > (dbo_u32)sizeof(g_output))
    {
        fprintf(stderr, "output too large for tool buffer\n");
        return 1;
    }
    if (!dbo_encode_memory(&source, g_output, (dbo_u32)sizeof(g_output), &out_size))
    {
        fprintf(stderr, "encode failed\n");
        return 1;
    }

    if (!dbo_parse_memory(&verify, g_output, out_size))
    {
        fprintf(stderr, "verify parse failed: %s\n", dbo_scene_last_error(&verify));
        return 1;
    }
    if (!dbo_validate_scene(&verify, &report_b))
    {
        fprintf(stderr, "verify validation failed: %s\n", report_b.first_error);
        return 2;
    }
    if (!scene_shape_equal(&source, &verify))
    {
        fprintf(stderr, "roundtrip shape mismatch after repack\n");
        return 2;
    }
    if (!strip_unknown && !unknown_chunks_equal(&source, &verify))
    {
        fprintf(stderr, "roundtrip unknown chunk mismatch after repack\n");
        return 2;
    }

    if (!dbo_tool_write_file(output_path, g_output, out_size, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "write failed: %s\n", err);
        return 1;
    }

    bytes_equal = (in_size == out_size && memcmp(g_input, g_output, in_size) == 0) ? 1 : 0;
    events_equal = chunk_events_equal(&source, &verify);

    printf("repacked: %s -> %s\n", input_path, output_path);
    printf("input bytes: %lu\n", (unsigned long)in_size);
    printf("output bytes: %lu\n", (unsigned long)out_size);
    printf("byte-identical: %s\n", bytes_equal ? "yes" : "no");
    printf("unknown chunks: original=%lu encoded=%lu verify=%lu\n",
        (unsigned long)original_unknown_chunks,
        (unsigned long)dbo_scene_unknown_chunk_count(&source),
        (unsigned long)dbo_scene_unknown_chunk_count(&verify));
    printf("unknown bytes: original=%lu encoded=%lu verify=%lu\n",
        (unsigned long)original_unknown_bytes,
        (unsigned long)dbo_scene_unknown_bytes(&source),
        (unsigned long)dbo_scene_unknown_bytes(&verify));
    printf("chunk-order events: original=%lu encoded=%lu verify=%lu\n",
        (unsigned long)original_event_count,
        (unsigned long)dbo_scene_chunk_event_count(&source),
        (unsigned long)dbo_scene_chunk_event_count(&verify));
    printf("chunk-order verify equal: %s\n", events_equal ? "yes" : "no");
    printf("warnings kept: source=%lu verify=%lu\n",
        (unsigned long)report_a.warnings,
        (unsigned long)report_b.warnings);
    if (strip_unknown)
    {
        printf("unknown policy: stripped unknown chunks before encode\n");
    }
    else
    {
        printf("unknown policy: preserved unknown chunks where supported\n");
    }
    if (canonical_order)
    {
        printf("order policy: canonicalized chunk order metadata before encode\n");
    }
    else if (dbo_scene_chunk_event_count(&source) > 0UL)
    {
        printf("order policy: order-preserving chunk replay enabled\n");
    }
    else
    {
        printf("order policy: no preserved chunk-order metadata available; canonical order used\n");
    }
    return 0;
}
