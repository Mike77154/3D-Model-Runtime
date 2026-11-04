#include <stdio.h>
#include <string.h>
#include "dbo_tool_common.h"

static dbo_u8 g_input[DBO_TOOL_MAX_FILE_BYTES];

static void print_triplet(const dbo_fx32 v[3])
{
    char a[32];
    char b[32];
    char c[32];
    dbo_tool_fx_to_string(v[0], a, (dbo_u32)sizeof(a));
    dbo_tool_fx_to_string(v[1], b, (dbo_u32)sizeof(b));
    dbo_tool_fx_to_string(v[2], c, (dbo_u32)sizeof(c));
    printf("(%s, %s, %s)", a, b, c);
}

static void print_aabb_line(const dbo_scene* scene)
{
    dbo_fx32 minv[3];
    dbo_fx32 maxv[3];
    if (dbo_scene_compute_aabb(scene, minv, maxv))
    {
        printf("scene aabb: min=");
        print_triplet(minv);
        printf(" max=");
        print_triplet(maxv);
        printf("\n");
    }
    else
    {
        printf("scene aabb: unavailable\n");
    }
}

static void print_unknown_human(const dbo_scene* scene)
{
    dbo_u32 i;
    if (scene->unknown_chunk_count == 0UL)
    {
        return;
    }

    printf("\nunknown chunks:\n");
    for (i = 0UL; i < scene->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* chunk = &scene->unknown_chunks[i];
        printf("  [%lu] scope=%s owner=%lu code=%lu size=%lu\n",
            (unsigned long)i,
            dbo_tool_scope_name(chunk->scope_kind),
            (unsigned long)chunk->owner_index,
            (unsigned long)chunk->code,
            (unsigned long)chunk->payload.size);
    }
}

static void print_chunk_events_human(const dbo_scene* scene)
{
    dbo_u32 i;
    if (scene->chunk_event_count == 0UL)
    {
        return;
    }

    printf("\nchunk order events:\n");
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        printf("  [%lu] scope=%s owner=%lu code=%lu kind=%s\n",
            (unsigned long)i,
            dbo_tool_scope_name(event->scope_kind),
            (unsigned long)event->owner_index,
            (unsigned long)event->code,
            event->aux_index == DBO_INDEX_NONE ? "known" : "unknown");
    }
}

static void print_human(const char* path, const dbo_scene* scene, const dbo_validation_report* report)
{
    dbo_u32 i;
    printf("file: %s\n", path);
    printf("version: %lu reserved1=%lu reserved2=%lu\n",
        (unsigned long)scene->version,
        (unsigned long)scene->reserved1,
        (unsigned long)scene->reserved2);
    printf("counts: frames=%lu meshes=%lu textures=%lu bones=%lu multimaterials=%lu animations=%lu animsets=%lu\n",
        (unsigned long)scene->frame_count,
        (unsigned long)scene->mesh_count,
        (unsigned long)scene->texture_count,
        (unsigned long)scene->bone_count,
        (unsigned long)scene->multimaterial_count,
        (unsigned long)scene->animation_count,
        (unsigned long)scene->animation_set_count);
    printf("totals: vertices=%lu indices=%lu blob=%lu bytes\n",
        (unsigned long)dbo_scene_total_vertices(scene),
        (unsigned long)dbo_scene_total_indices(scene),
        (unsigned long)scene->blob_used);
    printf("unknown: chunks=%lu bytes=%lu\n",
        (unsigned long)dbo_scene_unknown_chunk_count(scene),
        (unsigned long)dbo_scene_unknown_bytes(scene));
    printf("chunk order: events=%lu\n",
        (unsigned long)dbo_scene_chunk_event_count(scene));
    if (scene->root_frame != DBO_INDEX_NONE)
    {
        printf("root frame: %lu \"%s\"\n",
            (unsigned long)scene->root_frame,
            dbo_tool_cstr_or_empty(scene, scene->frames[scene->root_frame].name));
    }
    else
    {
        printf("root frame: none\n");
    }
    if (scene->root_animation_set != DBO_INDEX_NONE)
    {
        printf("root anim set: %lu \"%s\"\n",
            (unsigned long)scene->root_animation_set,
            dbo_tool_cstr_or_empty(scene, scene->animation_sets[scene->root_animation_set].name));
    }
    else
    {
        printf("root anim set: none\n");
    }
    print_aabb_line(scene);
    printf("validation: %lu errors, %lu warnings\n",
        (unsigned long)report->errors,
        (unsigned long)report->warnings);
    if (report->errors > 0UL)
    {
        printf("first error: %s\n", report->first_error);
    }
    if (report->warnings > 0UL)
    {
        printf("first warning: %s\n", report->first_warning);
    }

    printf("\nframes:\n");
    for (i = 0UL; i < scene->frame_count; ++i)
    {
        const dbo_frame* f = &scene->frames[i];
        dbo_u32 unknown_count;
        dbo_u32 event_count;
        unknown_count = dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_FRAME, i);
        event_count = dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_FRAME, i);
        printf("  [%lu] name=\"%s\" parent=", (unsigned long)i, dbo_tool_cstr_or_empty(scene, f->name));
        if (f->parent == DBO_INDEX_NONE) printf("none"); else printf("%lu", (unsigned long)f->parent);
        printf(" child=");
        if (f->child == DBO_INDEX_NONE) printf("none"); else printf("%lu", (unsigned long)f->child);
        printf(" sibling=");
        if (f->sibling == DBO_INDEX_NONE) printf("none"); else printf("%lu", (unsigned long)f->sibling);
        printf(" mesh=");
        if (f->mesh == DBO_INDEX_NONE) printf("none"); else printf("%lu", (unsigned long)f->mesh);
        if (unknown_count > 0UL)
        {
            printf(" unknown=%lu", (unsigned long)unknown_count);
        }
        if (event_count > 0UL)
        {
            printf(" events=%lu", (unsigned long)event_count);
        }
        printf("\n");
    }

    printf("\nmeshes:\n");
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        const dbo_mesh* m = &scene->meshes[i];
        dbo_u32 unknown_count;
        dbo_u32 event_count;
        dbo_fx32 minv[3];
        dbo_fx32 maxv[3];
        unknown_count = dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_MESH, i);
        event_count = dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_MESH, i);
        printf("  [%lu] fvf=0x%08lx stride=%lu vertices=%lu indices=%lu prim=%lu drawv=%lu drawp=%lu textures=%lu bones=%lu mm=%lu visible=%d",
            (unsigned long)i,
            (unsigned long)m->fvf,
            (unsigned long)(m->fvf_size != 0UL ? m->fvf_size : dbo_fvf_vertex_stride(m->fvf)),
            (unsigned long)m->vertex_count,
            (unsigned long)m->index_count,
            (unsigned long)m->primitive_type,
            (unsigned long)m->draw_vertex_count,
            (unsigned long)m->draw_primitive_count,
            (unsigned long)m->texture_count,
            (unsigned long)m->bone_count,
            (unsigned long)m->multimaterial_count,
            (int)m->visible);
        if (unknown_count > 0UL)
        {
            printf(" unknown=%lu", (unsigned long)unknown_count);
        }
        if (event_count > 0UL)
        {
            printf(" events=%lu", (unsigned long)event_count);
        }
        printf("\n");
        if (dbo_mesh_compute_aabb(scene, m, minv, maxv))
        {
            printf("      aabb min=");
            print_triplet(minv);
            printf(" max=");
            print_triplet(maxv);
            printf("\n");
        }
        if (m->effect_name.offset != DBO_INDEX_NONE)
        {
            printf("      effect=\"%s\"\n", dbo_tool_cstr_or_empty(scene, m->effect_name));
        }
    }

    if (scene->texture_count > 0UL)
    {
        printf("\ntextures:\n");
        for (i = 0UL; i < scene->texture_count; ++i)
        {
            const dbo_texture* t = &scene->textures[i];
            dbo_u32 unknown_count;
            dbo_u32 event_count;
            unknown_count = dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_TEXTURE, i);
            event_count = dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_TEXTURE, i);
            printf("  [%lu] stage=%lu name=\"%s\"",
                (unsigned long)i,
                (unsigned long)t->stage,
                dbo_tool_cstr_or_empty(scene, t->name));
            if (unknown_count > 0UL)
            {
                printf(" unknown=%lu", (unsigned long)unknown_count);
            }
            if (event_count > 0UL)
            {
                printf(" events=%lu", (unsigned long)event_count);
            }
            printf("\n");
        }
    }

    print_unknown_human(scene);
    print_chunk_events_human(scene);
}

static void print_unknown_json(const dbo_scene* scene)
{
    dbo_u32 i;
    printf("  \"unknown_chunks\": [\n");
    for (i = 0UL; i < scene->unknown_chunk_count; ++i)
    {
        const dbo_unknown_chunk* chunk = &scene->unknown_chunks[i];
        printf("    {\"index\": %lu, \"scope\": ", (unsigned long)i);
        dbo_tool_json_write_string(stdout, dbo_tool_scope_name(chunk->scope_kind));
        printf(", \"owner\": %lu, \"code\": %lu, \"size\": %lu}",
            (unsigned long)chunk->owner_index,
            (unsigned long)chunk->code,
            (unsigned long)chunk->payload.size);
        if (i + 1UL < scene->unknown_chunk_count) printf(",");
        printf("\n");
    }
    printf("  ],\n");
}

static void print_chunk_events_json(const dbo_scene* scene)
{
    dbo_u32 i;
    printf("  \"chunk_events\": [\n");
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        printf("    {\"index\": %lu, \"scope\": ", (unsigned long)i);
        dbo_tool_json_write_string(stdout, dbo_tool_scope_name(event->scope_kind));
        printf(", \"owner\": %lu, \"code\": %lu, \"kind\": ",
            (unsigned long)event->owner_index,
            (unsigned long)event->code);
        dbo_tool_json_write_string(stdout, event->aux_index == DBO_INDEX_NONE ? "known" : "unknown");
        printf("}");
        if (i + 1UL < scene->chunk_event_count) printf(",");
        printf("\n");
    }
    printf("  ]\n");
}

static void print_json(const char* path, const dbo_scene* scene, const dbo_validation_report* report)
{
    dbo_u32 i;
    dbo_fx32 minv[3];
    dbo_fx32 maxv[3];
    printf("{\n");
    printf("  \"file\": ");
    dbo_tool_json_write_string(stdout, path);
    printf(",\n");
    printf("  \"version\": %lu,\n", (unsigned long)scene->version);
    printf("  \"reserved1\": %lu,\n", (unsigned long)scene->reserved1);
    printf("  \"reserved2\": %lu,\n", (unsigned long)scene->reserved2);
    printf("  \"counts\": {\n");
    printf("    \"frames\": %lu,\n", (unsigned long)scene->frame_count);
    printf("    \"meshes\": %lu,\n", (unsigned long)scene->mesh_count);
    printf("    \"textures\": %lu,\n", (unsigned long)scene->texture_count);
    printf("    \"bones\": %lu,\n", (unsigned long)scene->bone_count);
    printf("    \"multimaterials\": %lu,\n", (unsigned long)scene->multimaterial_count);
    printf("    \"animations\": %lu,\n", (unsigned long)scene->animation_count);
    printf("    \"animation_sets\": %lu\n", (unsigned long)scene->animation_set_count);
    printf("  },\n");
    printf("  \"totals\": {\n");
    printf("    \"vertices\": %lu,\n", (unsigned long)dbo_scene_total_vertices(scene));
    printf("    \"indices\": %lu,\n", (unsigned long)dbo_scene_total_indices(scene));
    printf("    \"blob_bytes\": %lu,\n", (unsigned long)scene->blob_used);
    printf("    \"unknown_chunks\": %lu,\n", (unsigned long)dbo_scene_unknown_chunk_count(scene));
    printf("    \"unknown_bytes\": %lu,\n", (unsigned long)dbo_scene_unknown_bytes(scene));
    printf("    \"chunk_events\": %lu\n", (unsigned long)dbo_scene_chunk_event_count(scene));
    printf("  },\n");
    printf("  \"root_frame\": ");
    if (scene->root_frame == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)scene->root_frame);
    printf(",\n");
    printf("  \"root_frame_name\": ");
    if (scene->root_frame == DBO_INDEX_NONE) printf("null"); else dbo_tool_json_write_string(stdout, dbo_tool_cstr_or_empty(scene, scene->frames[scene->root_frame].name));
    printf(",\n");
    printf("  \"root_animation_set\": ");
    if (scene->root_animation_set == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)scene->root_animation_set);
    printf(",\n");
    printf("  \"scene_aabb\": ");
    if (dbo_scene_compute_aabb(scene, minv, maxv))
    {
        printf("{\"min\":[");
        dbo_tool_write_fx_json(stdout, minv[0]); printf(","); dbo_tool_write_fx_json(stdout, minv[1]); printf(","); dbo_tool_write_fx_json(stdout, minv[2]);
        printf("],\"max\":[");
        dbo_tool_write_fx_json(stdout, maxv[0]); printf(","); dbo_tool_write_fx_json(stdout, maxv[1]); printf(","); dbo_tool_write_fx_json(stdout, maxv[2]);
        printf("]}");
    }
    else
    {
        printf("null");
    }
    printf(",\n");
    printf("  \"validation\": {\"errors\": %lu, \"warnings\": %lu, \"first_error\": ",
        (unsigned long)report->errors,
        (unsigned long)report->warnings);
    dbo_tool_json_write_string(stdout, report->first_error);
    printf(", \"first_warning\": ");
    dbo_tool_json_write_string(stdout, report->first_warning);
    printf("},\n");

    printf("  \"frames\": [\n");
    for (i = 0UL; i < scene->frame_count; ++i)
    {
        const dbo_frame* f = &scene->frames[i];
        printf("    {\"index\": %lu, \"name\": ", (unsigned long)i);
        dbo_tool_json_write_string(stdout, dbo_tool_cstr_or_empty(scene, f->name));
        printf(", \"parent\": ");
        if (f->parent == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)f->parent);
        printf(", \"child\": ");
        if (f->child == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)f->child);
        printf(", \"sibling\": ");
        if (f->sibling == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)f->sibling);
        printf(", \"mesh\": ");
        if (f->mesh == DBO_INDEX_NONE) printf("null"); else printf("%lu", (unsigned long)f->mesh);
        printf(", \"unknown\": %lu, \"events\": %lu}",
            (unsigned long)dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_FRAME, i),
            (unsigned long)dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_FRAME, i));
        if (i + 1UL < scene->frame_count) printf(",");
        printf("\n");
    }
    printf("  ],\n");

    printf("  \"meshes\": [\n");
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        const dbo_mesh* m = &scene->meshes[i];
        printf("    {\"index\": %lu, \"fvf\": %lu, \"stride\": %lu, \"vertex_count\": %lu, \"index_count\": %lu, \"primitive_type\": %lu, \"draw_vertex_count\": %lu, \"draw_primitive_count\": %lu, \"texture_count\": %lu, \"bone_count\": %lu, \"multimaterial_count\": %lu, \"visible\": %d, \"unknown\": %lu, \"events\": %lu, \"aabb\": ",
            (unsigned long)i,
            (unsigned long)m->fvf,
            (unsigned long)(m->fvf_size != 0UL ? m->fvf_size : dbo_fvf_vertex_stride(m->fvf)),
            (unsigned long)m->vertex_count,
            (unsigned long)m->index_count,
            (unsigned long)m->primitive_type,
            (unsigned long)m->draw_vertex_count,
            (unsigned long)m->draw_primitive_count,
            (unsigned long)m->texture_count,
            (unsigned long)m->bone_count,
            (unsigned long)m->multimaterial_count,
            (int)m->visible,
            (unsigned long)dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_MESH, i),
            (unsigned long)dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_MESH, i));
        if (dbo_mesh_compute_aabb(scene, m, minv, maxv))
        {
            printf("{\"min\":[");
            dbo_tool_write_fx_json(stdout, minv[0]); printf(","); dbo_tool_write_fx_json(stdout, minv[1]); printf(","); dbo_tool_write_fx_json(stdout, minv[2]);
            printf("],\"max\":[");
            dbo_tool_write_fx_json(stdout, maxv[0]); printf(","); dbo_tool_write_fx_json(stdout, maxv[1]); printf(","); dbo_tool_write_fx_json(stdout, maxv[2]);
            printf("]}");
        }
        else
        {
            printf("null");
        }
        printf(", \"effect_name\": ");
        if (m->effect_name.offset == DBO_INDEX_NONE) printf("null"); else dbo_tool_json_write_string(stdout, dbo_tool_cstr_or_empty(scene, m->effect_name));
        printf("}");
        if (i + 1UL < scene->mesh_count) printf(",");
        printf("\n");
    }
    printf("  ],\n");

    printf("  \"textures\": [\n");
    for (i = 0UL; i < scene->texture_count; ++i)
    {
        const dbo_texture* t = &scene->textures[i];
        printf("    {\"index\": %lu, \"stage\": %lu, \"name\": ",
            (unsigned long)i,
            (unsigned long)t->stage);
        dbo_tool_json_write_string(stdout, dbo_tool_cstr_or_empty(scene, t->name));
        printf(", \"unknown\": %lu, \"events\": %lu}",
            (unsigned long)dbo_scene_unknown_chunks_for_scope(scene, DBO_SCOPE_TEXTURE, i),
            (unsigned long)dbo_scene_chunk_events_for_scope(scene, DBO_SCOPE_TEXTURE, i));
        if (i + 1UL < scene->texture_count) printf(",");
        printf("\n");
    }
    printf("  ],\n");

    print_unknown_json(scene);
    print_chunk_events_json(scene);
    printf("}\n");
}

int main(int argc, char** argv)
{
    const char* path;
    int json_mode;
    char err[DBO_MAX_ERROR_TEXT];
    dbo_u32 size;
    static dbo_scene scene;
    static dbo_validation_report report;

    path = 0;
    json_mode = 0;
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [--json] <input.dbo>\n", argv[0]);
        return 1;
    }

    if (argc == 3 && strcmp(argv[1], "--json") == 0)
    {
        json_mode = 1;
        path = argv[2];
    }
    else if (argc == 2)
    {
        path = argv[1];
    }
    else
    {
        fprintf(stderr, "usage: %s [--json] <input.dbo>\n", argv[0]);
        return 1;
    }

    if (!dbo_tool_load_scene(path, &scene, g_input, (dbo_u32)sizeof(g_input), &size, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "load failed: %s\n", err);
        return 1;
    }

    dbo_validate_scene(&scene, &report);

    if (json_mode)
    {
        print_json(path, &scene, &report);
    }
    else
    {
        print_human(path, &scene, &report);
    }

    return report.errors > 0UL ? 2 : 0;
}
