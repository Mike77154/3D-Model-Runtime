#include "dts89.h"
#include <stdio.h>
#include <string.h>

static unsigned char g_blob[8192];
static unsigned char g_blob2[8192];
static unsigned char g_workspace[16384];
static unsigned char g_workspace2[16384];
static unsigned char g_text[4096];

static int has_substr(const char *buf, dts89_u32 len, const char *needle)
{
    dts89_u32 i;
    dts89_u32 j;
    dts89_u32 n;
    if (!buf || !needle) return 0;
    n = 0u;
    while (needle[n] != '\0') n++;
    if (n == 0u) return 1;
    if (len < n) return 0;
    for (i = 0u; i + n <= len; ++i) {
        for (j = 0u; j < n; ++j) {
            if (buf[i + j] != needle[j]) break;
        }
        if (j == n) return 1;
    }
    return 0;
}

int main(void)
{
    dts89_shape shape;
    dts89_mesh_view mesh;
    dts89_material mats[2];
    dts89_writer w;
    dts89_arena arena;
    dts89_shape decoded;
    dts89_probe_result probe;
    dts89_runtime_options rtopt;
    dts89_runtime_shape rtshape;
    dts89_detail_choice rtchoice;
    dts89_validate_report vreport;
    dts89_sequence_cursor cursor;
    dts89_u32 event_count;

    dts89_dsq_file dsq;
    dts89_string dsq_node_names[1];
    dts89_dsq_sequence dsq_seq[1];
    dts89_writer w2;
    dts89_arena arena2;
    dts89_dsq_file dsq_dec;
    dts89_dsq_import_plan plan;
    dts89_shape merged;

    int rc;

    memset(&shape, 0, sizeof(shape));
    memset(&mesh, 0, sizeof(mesh));
    memset(mats, 0, sizeof(mats));
    shape.header.read_version = DTS89_VERSION_CURRENT;
    shape.header.exporter_version = DTS89_EXPORTER_CURRENT;
    shape.num_meshes = 1;
    shape.meshes = &mesh;
    shape.num_materials = 2;
    shape.materials = mats;
    mesh.mesh_type = DTS89_MESH_NULL;

    mats[0].name.ptr = "body";
    mats[0].name.len = 4u;
    mats[0].flags = DTS89_MATF_S_WRAP | DTS89_MATF_T_WRAP;
    mats[0].reflectance_map = 0u;
    mats[0].bump_map = 0xFFFFFFFFu;
    mats[0].detail_map = 0xFFFFFFFFu;
    mats[0].detail_scale_bits = DTS89_F32_BITS_ONE;
    mats[0].reflection_amount_bits = DTS89_F32_BITS_ONE;

    mats[1].name.ptr = "glass";
    mats[1].name.len = 5u;
    mats[1].flags = DTS89_MATF_S_WRAP | DTS89_MATF_T_WRAP | DTS89_MATF_TRANSLUCENT;
    mats[1].reflectance_map = 1u;
    mats[1].bump_map = 0xFFFFFFFFu;
    mats[1].detail_map = 0xFFFFFFFFu;
    mats[1].detail_scale_bits = DTS89_F32_BITS_ONE;
    mats[1].reflection_amount_bits = DTS89_F32_BITS_ONE;

    dts89_writer_init(&w, g_blob, sizeof(g_blob));
    rc = dts89_encode_shape(&shape, &w, 0);
    if (rc) {
        printf("selftest shape encode failed: %d\n", rc);
        return 1;
    }

    rc = dts89_probe(g_blob, w.pos, &probe);
    if (rc || probe.kind != DTS89_PROBE_KIND_DTS || !(probe.flags & DTS89_PROBE_FLAG_SUPPORTED)) {
        printf("selftest shape probe failed: %d\n", rc);
        return 15;
    }

    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_decode_shape(g_blob, w.pos, &arena, &decoded);
    if (rc) {
        printf("selftest shape decode failed: %d\n", rc);
        return 2;
    }
    if (decoded.num_meshes != 1 || decoded.meshes[0].mesh_type != DTS89_MESH_NULL) {
        printf("selftest shape mismatch\n");
        return 3;
    }
    if (decoded.num_materials != 2) {
        printf("selftest material count mismatch\n");
        return 4;
    }
    if (!decoded.materials[0].name.ptr || strcmp(decoded.materials[0].name.ptr, "body") != 0) {
        printf("selftest material[0] mismatch\n");
        return 5;
    }
    if (!decoded.materials[1].name.ptr || strcmp(decoded.materials[1].name.ptr, "glass") != 0) {
        printf("selftest material[1] mismatch\n");
        return 6;
    }
    dts89_runtime_options_default(&rtopt);
    rc = dts89_runtime_build(&decoded, &rtopt, &arena, &rtshape);
    if (rc) {
        printf("selftest runtime build failed: %d\n", rc);
        return 18;
    }
    rc = dts89_runtime_select_detail_from_pixel_size(&decoded, &rtshape, dts89_fx_from_int(1), &rtchoice);
    if (rc || rtchoice.detail_level != -1) {
        printf("selftest runtime select failed: %d\n", rc);
        return 19;
    }
    rc = dts89_validate_shape(&decoded, &vreport);
    if (rc || vreport.errors != 0) {
        printf("selftest shape validate failed: %d errors=%d\n", rc, (int)vreport.errors);
        return 22;
    }
    dts89_writer_init(&w2, g_text, sizeof(g_text));
    rc = dts89_emit_materials_cs(&decoded, ".png", &w2);
    if (rc) {
        printf("selftest materials emit failed: %d\n", rc);
        return 7;
    }
    if (!has_substr((const char*)g_text, w2.pos, "mapTo = \"body\";") ||
        !has_substr((const char*)g_text, w2.pos, "translucent = true;")) {
        printf("selftest materials script mismatch\n");
        return 8;
    }

    memset(&dsq, 0, sizeof(dsq));
    memset(dsq_node_names, 0, sizeof(dsq_node_names));
    memset(dsq_seq, 0, sizeof(dsq_seq));
    dsq.version = DTS89_DSQ_VERSION_CURRENT;
    dsq.exporter_version = DTS89_EXPORTER_CURRENT;
    dsq.num_nodes = 1;
    dsq.node_names = dsq_node_names;
    dsq.node_names[0].ptr = "Bip01";
    dsq.node_names[0].len = 5u;
    dsq.num_objects = 0;
    dsq.num_sequences = 1;
    dsq.sequences = dsq_seq;
    dsq.sequences[0].name.ptr = "idle";
    dsq.sequences[0].name.len = 4u;
    dsq.sequences[0].num_keyframes = 1;
    dsq.sequences[0].duration_bits = DTS89_F32_BITS_ONE;
    dsq.sequences[0].priority = 0;
    dsq.sequences[0].flags = 0u;
    dsq.sequences[0].tool_begin_bits = 0u;

    dts89_writer_init(&w, g_blob2, sizeof(g_blob2));
    rc = dts89_encode_dsq(&dsq, &w);
    if (rc) {
        printf("selftest dsq encode failed: %d\n", rc);
        return 9;
    }
    rc = dts89_probe(g_blob2, w.pos, &probe);
    if (rc || probe.kind != DTS89_PROBE_KIND_DSQ || !(probe.flags & DTS89_PROBE_FLAG_SUPPORTED)) {
        printf("selftest dsq probe failed: %d\n", rc);
        return 16;
    }

    dts89_arena_init(&arena2, g_workspace2, sizeof(g_workspace2));
    rc = dts89_decode_dsq(g_blob2, w.pos, &arena2, &dsq_dec);
    if (rc) {
        printf("selftest dsq decode failed: %d\n", rc);
        return 10;
    }
    if (dsq_dec.num_nodes != 1 || dsq_dec.num_sequences != 1) {
        printf("selftest dsq count mismatch\n");
        return 11;
    }
    if (!dsq_dec.sequences[0].name.ptr || strcmp(dsq_dec.sequences[0].name.ptr, "idle") != 0) {
        printf("selftest dsq sequence mismatch\n");
        return 12;
    }
    rc = dts89_dsq_build_import_plan(&decoded, &dsq_dec, &arena2, &plan);
    if (rc || plan.ignored_nodes != 1 || plan.imported_sequences != 1) {
        printf("selftest dsq import plan failed: %d\n", rc);
        return 20;
    }
    rc = dts89_validate_dsq(&dsq_dec, &vreport);
    if (rc || vreport.errors != 0) {
        printf("selftest dsq validate failed: %d errors=%d\n", rc, (int)vreport.errors);
        return 23;
    }
    rc = dts89_shape_merge_dsq(&decoded, &dsq_dec, &arena2, &merged);
    if (rc || merged.num_sequences != decoded.num_sequences + 1 || merged.num_names != decoded.num_names + 1) {
        printf("selftest dsq merge failed: %d\n", rc);
        return 21;
    }
    rc = dts89_sequence_select_keyframes(&merged.sequences[merged.num_sequences - 1], DTS89_FX_ONE, &cursor);
    if (rc || cursor.key0 != 0 || cursor.key1 != 0) {
        printf("selftest keyframe select failed: %d\n", rc);
        return 24;
    }
    rc = dts89_sequence_scan_triggers(&merged, merged.num_sequences - 1, 0, DTS89_FX_ONE, 0, 0u, &event_count);
    if (rc || event_count != 0u) {
        printf("selftest trigger scan failed: %d\n", rc);
        return 25;
    }
    dts89_writer_init(&w2, g_text, sizeof(g_text));
    rc = dts89_emit_shape_constructor_cs("ShapeAuto", "player.dts", &dsq_dec, "player_idle.dsq", &w2);
    if (rc) {
        printf("selftest constructor emit failed: %d\n", rc);
        return 13;
    }
    if (!has_substr((const char*)g_text, w2.pos, "%this.addSequence(\"player_idle.dsq\", \"idle\");")) {
        printf("selftest constructor script mismatch\n");
        return 14;
    }
    rc = dts89_probe(g_text, w2.pos, &probe);
    if (rc || probe.kind != DTS89_PROBE_KIND_TORQUESCRIPT) {
        printf("selftest torquescript probe failed: %d\n", rc);
        return 17;
    }

    printf("selftest ok\n");
    return 0;
}
