#include "dts89.h"
#include <stdio.h>

#define DTS89_VAL_MAX_INPUT (32u * 1024u * 1024u)
#define DTS89_VAL_WORKSPACE (32u * 1024u * 1024u)

static unsigned char g_input[DTS89_VAL_MAX_INPUT];
static unsigned char g_workspace[DTS89_VAL_WORKSPACE];

static long val_file_size(FILE *f)
{
    long cur;
    long end;
    cur = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static void val_print_report(const dts89_validate_report *r)
{
    printf("errors=%d warnings=%d first=%d\n", (int)r->errors, (int)r->warnings, (int)r->first_error_code);
    printf("bad_count=%d bad_name=%d bad_node=%d bad_object=%d bad_subshape=%d bad_detail=%d\n", (int)r->bad_count, (int)r->bad_name_index, (int)r->bad_node_index, (int)r->bad_object_index, (int)r->bad_subshape_range, (int)r->bad_detail_range);
    printf("bad_mesh=%d bad_material=%d bad_primitive=%d bad_sequence=%d bad_trigger=%d unsupported_meshes=%d warning_flags=0x%08X\n", (int)r->bad_mesh_range, (int)r->bad_material_index, (int)r->bad_primitive_range, (int)r->bad_sequence_range, (int)r->bad_trigger_range, (int)r->unsupported_meshes, (unsigned)r->warning_flags);
}

int main(int argc, char **argv)
{
    FILE *f;
    long sz;
    dts89_probe_result probe;
    dts89_arena arena;
    dts89_validate_report report;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dts-or-dsq\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = val_file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_VAL_MAX_INPUT) {
        printf("input too large\n");
        fclose(f);
        return 3;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 4;
    }
    fclose(f);
    rc = dts89_probe(g_input, (dts89_u32)sz, &probe);
    if (rc) {
        printf("probe failed: %d\n", rc);
        return 5;
    }
    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    if (probe.kind == DTS89_PROBE_KIND_DTS) {
        dts89_shape shape;
        rc = dts89_decode_shape(g_input, (dts89_u32)sz, &arena, &shape);
        if (rc) {
            printf("decode DTS failed: %d\n", rc);
            return 6;
        }
        rc = dts89_validate_shape(&shape, &report);
        printf("kind=DTS version=%u nodes=%d objects=%d details=%d meshes=%d sequences=%d materials=%d rc=%d\n", (unsigned)shape.header.read_version, (int)shape.num_nodes, (int)shape.num_objects, (int)shape.num_details, (int)shape.num_meshes, (int)shape.num_sequences, (int)shape.num_materials, rc);
        val_print_report(&report);
        return report.errors ? 20 : 0;
    }
    if (probe.kind == DTS89_PROBE_KIND_DSQ) {
        dts89_dsq_file dsq;
        rc = dts89_decode_dsq(g_input, (dts89_u32)sz, &arena, &dsq);
        if (rc) {
            printf("decode DSQ failed: %d\n", rc);
            return 7;
        }
        rc = dts89_validate_dsq(&dsq, &report);
        printf("kind=DSQ version=%d nodes=%d sequences=%d triggers=%d rc=%d\n", (int)dsq.version, (int)dsq.num_nodes, (int)dsq.num_sequences, (int)dsq.num_triggers, rc);
        val_print_report(&report);
        return report.errors ? 21 : 0;
    }
    printf("unknown kind: %s\n", dts89_probe_kind_name(probe.kind));
    return 8;
}
