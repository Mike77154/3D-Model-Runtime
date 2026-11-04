#include "giffy_recodec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static grc_u32 rd32le(const grc_u8 *p)
{
    return (grc_u32)((grc_u32)p[0] | ((grc_u32)p[1] << 8) | ((grc_u32)p[2] << 16) | ((grc_u32)p[3] << 24));
}

static int load_bytes(const char *path, grc_u8 **out_data, size_t *out_size)
{
    FILE *f;
    long len;
    grc_u8 *data;
    size_t got;
    if (path == NULL || out_data == NULL || out_size == NULL) return GRC_ERR_BAD_ARGUMENT;
    *out_data = NULL;
    *out_size = 0u;
    f = fopen(path, "rb");
    if (f == NULL) return GRC_ERR_IO;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return GRC_ERR_IO; }
    len = ftell(f);
    if (len < 0L) { fclose(f); return GRC_ERR_IO; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return GRC_ERR_IO; }
    data = (grc_u8 *)malloc((size_t)len + 1u);
    if (data == NULL) { fclose(f); return GRC_ERR_MEMORY; }
    got = fread(data, 1u, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) { free(data); return GRC_ERR_IO; }
    data[(size_t)len] = 0u;
    *out_data = data;
    *out_size = (size_t)len;
    return GRC_OK;
}

static void grc_print_usage(void)
{
    printf("Giffy RE Codec CLI v%d.%d.%d\n", GRC_VERSION_MAJOR, GRC_VERSION_MINOR, GRC_VERSION_PATCH);
    printf("usage:\n");
    printf("  grc info <model> [--scale n]\n");
    printf("  grc validate <model> [--scale n]\n");
    printf("  grc convert <input> <output> [--game re1|re2|re3] [--scale n] [--clean] [--triangulate] [--normals] [--weld eps] [--psx] [--flipv]\n");
    printf("  grc clean <input> <output> [--triangulate] [--normals] [--weld eps] [--psx] [--flipv]\n");
    printf("  grc sections <emd/pld/plw>\n");
    printf("  grc extract <emd/pld/plw> <prefix>\n");
    printf("  grc tim2ppm <input.tim> <output.ppm> [palette]\n");
}

static void grc_print_model_info(const grc_model *m)
{
    grc_model_stats st;
    grc_u32 i;
    grc_model_analyze(m, &st);
    printf("name: %s\n", m->name);
    printf("source_game: %s (%u)\n", grc_game_string(m->source_game), (unsigned int)m->source_game);
    printf("source_format: %s\n", grc_format_string(m->source_format));
    printf("vertices: %u\n", (unsigned int)st.vertex_count);
    printf("normals: %u\n", (unsigned int)st.normal_count);
    printf("uvs: %u\n", (unsigned int)st.uv_count);
    printf("faces: %u\n", (unsigned int)st.face_count);
    printf("  triangles: %u\n", (unsigned int)st.triangle_count);
    printf("  quads: %u\n", (unsigned int)st.quad_count);
    printf("  textured: %u\n", (unsigned int)st.textured_face_count);
    printf("  invalid: %u\n", (unsigned int)st.invalid_face_count);
    printf("  degenerate: %u\n", (unsigned int)st.degenerate_face_count);
    printf("unused_vertices: %u\n", (unsigned int)st.unused_vertex_count);
    printf("materials: %u\n", (unsigned int)m->material_count);
    printf("textures: %u\n", (unsigned int)m->texture_count);
    printf("pieces: %u\n", (unsigned int)m->piece_count);
    printf("bounds_min: %.3f %.3f %.3f\n", st.bounds_min.x, st.bounds_min.y, st.bounds_min.z);
    printf("bounds_max: %.3f %.3f %.3f\n", st.bounds_max.x, st.bounds_max.y, st.bounds_max.z);
    if (m->piece_count > 0u) {
        printf("piece_table:\n");
        for (i = 0u; i < m->piece_count; ++i) {
            printf("  %02u %-24s faces=%u first_face=%u verts=%u first_vert=%u native_id=%u parent=%d\n",
                   (unsigned int)i,
                   m->pieces[i].name,
                   (unsigned int)m->pieces[i].face_count,
                   (unsigned int)m->pieces[i].first_face,
                   (unsigned int)m->pieces[i].vertex_count,
                   (unsigned int)m->pieces[i].first_vertex,
                   (unsigned int)m->pieces[i].native_id,
                   (int)m->pieces[i].parent_index);
        }
    }
}

static void grc_parse_opts(int argc, char **argv, int start, grc_options *opt, int *flip_v)
{
    int i;
    grc_options_default(opt);
    if (flip_v != NULL) *flip_v = 0;
    i = start;
    while (i < argc) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "re1") == 0) opt->native_game_hint = GRC_GAME_RE1;
            else if (strcmp(argv[i + 1], "re2") == 0) opt->native_game_hint = GRC_GAME_RE2;
            else if (strcmp(argv[i + 1], "re3") == 0) opt->native_game_hint = GRC_GAME_RE3;
            i += 2;
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            opt->scale = (float)atof(argv[i + 1]);
            if (opt->scale == 0.0f) opt->scale = 1.0f;
            i += 2;
        } else if (strcmp(argv[i], "--clean") == 0) {
            opt->flags |= GRC_CLEAN_DROP_INVALID | GRC_CLEAN_DROP_DEGENERATE | GRC_CLEAN_REMOVE_UNUSED_VERTICES;
            i++;
        } else if (strcmp(argv[i], "--triangulate") == 0) {
            opt->flags |= GRC_CLEAN_TRIANGULATE_QUADS;
            i++;
        } else if (strcmp(argv[i], "--normals") == 0) {
            opt->flags |= GRC_CLEAN_GENERATE_NORMALS;
            i++;
        } else if (strcmp(argv[i], "--psx") == 0) {
            opt->flags |= GRC_CLEAN_PSX_QUANTIZE;
            i++;
        } else if (strcmp(argv[i], "--flipv") == 0) {
            if (flip_v != NULL) *flip_v = 1;
            i++;
        } else if (strcmp(argv[i], "--weld") == 0 && i + 1 < argc) {
            opt->flags |= GRC_CLEAN_WELD_VERTICES;
            opt->weld_epsilon = (float)atof(argv[i + 1]);
            if (opt->weld_epsilon <= 0.0f) opt->weld_epsilon = 0.0001f;
            i += 2;
        } else {
            i++;
        }
    }
}

static int command_info(int argc, char **argv)
{
    grc_model m;
    grc_options opt;
    int rc;
    int flip_v;
    if (argc < 3) { grc_print_usage(); return 1; }
    grc_parse_opts(argc, argv, 3, &opt, &flip_v);
    rc = grc_load_model(argv[2], &m, &opt);
    if (rc != GRC_OK) {
        fprintf(stderr, "info failed: %s\n", grc_result_string(rc));
        return 1;
    }
    grc_print_model_info(&m);
    grc_model_free(&m);
    (void)flip_v;
    return 0;
}

static int command_validate(int argc, char **argv)
{
    grc_model m;
    grc_options opt;
    grc_model_stats st;
    char report[1024];
    int rc;
    int vrc;
    int flip_v;
    if (argc < 3) { grc_print_usage(); return 1; }
    grc_parse_opts(argc, argv, 3, &opt, &flip_v);
    rc = grc_load_model(argv[2], &m, &opt);
    if (rc != GRC_OK) {
        fprintf(stderr, "load failed: %s\n", grc_result_string(rc));
        return 1;
    }
    vrc = grc_model_validate(&m, &st, report, sizeof(report));
    printf("%s", report);
    grc_model_free(&m);
    (void)flip_v;
    return (vrc == GRC_OK) ? 0 : 2;
}

static int command_convert_or_clean(int argc, char **argv, int is_clean)
{
    grc_model m;
    grc_options opt;
    int rc;
    int flip_v;
    if (argc < 4) { grc_print_usage(); return 1; }
    grc_parse_opts(argc, argv, 4, &opt, &flip_v);
    if (is_clean) opt.flags |= GRC_CLEAN_DROP_INVALID | GRC_CLEAN_DROP_DEGENERATE | GRC_CLEAN_REMOVE_UNUSED_VERTICES;
    rc = grc_load_model(argv[2], &m, &opt);
    if (rc != GRC_OK) {
        fprintf(stderr, "load failed: %s\n", grc_result_string(rc));
        return 1;
    }
    if (flip_v) grc_model_flip_v(&m);
    if (opt.flags != 0u) {
        rc = grc_model_prepare(&m, &opt);
        if (rc != GRC_OK) {
            grc_model_free(&m);
            fprintf(stderr, "prepare failed: %s\n", grc_result_string(rc));
            return 1;
        }
    }
    rc = grc_save_model(argv[3], &m, &opt);
    grc_model_free(&m);
    if (rc != GRC_OK) {
        fprintf(stderr, "save failed: %s\n", grc_result_string(rc));
        return 1;
    }
    printf("%s: %s -> %s\n", is_clean ? "cleaned" : "converted", argv[2], argv[3]);
    return 0;
}

static int command_sections(int argc, char **argv)
{
    grc_u8 *data;
    size_t size;
    grc_section_table table;
    grc_u32 fmt;
    int rc;
    grc_u32 i;
    if (argc < 3) { grc_print_usage(); return 1; }
    rc = grc_probe_file(argv[2], &fmt);
    if (rc != GRC_OK) { fprintf(stderr, "probe failed: %s\n", grc_result_string(rc)); return 1; }
    rc = load_bytes(argv[2], &data, &size);
    if (rc != GRC_OK) { fprintf(stderr, "read failed: %s\n", grc_result_string(rc)); return 1; }
    table.sections = NULL;
    table.section_count = 0u;
    rc = grc_re_read_section_table(data, size, &table, fmt);
    if (rc != GRC_OK) { free(data); fprintf(stderr, "section table failed: %s\n", grc_result_string(rc)); return 1; }
    printf("format_hint: %s\n", grc_format_string(fmt));
    printf("file_size: %u\n", (unsigned int)size);
    printf("sections: %u\n", (unsigned int)table.section_count);
    for (i = 0u; i < table.section_count; ++i) {
        const char *kind;
        kind = "bin";
        if (table.sections[i].size >= 8u && (size_t)table.sections[i].offset + 8u <= size) {
            grc_u32 id;
            id = rd32le(data + table.sections[i].offset);
            if (id == 0x00000041u || id == 0x41000000u) kind = "tmd";
            else if (data[table.sections[i].offset] == 0x10u) kind = "tim";
        }
        printf("  %02u offset=%u size=%u kind=%s\n", (unsigned int)i, (unsigned int)table.sections[i].offset, (unsigned int)table.sections[i].size, kind);
    }
    grc_section_table_free(&table);
    free(data);
    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    if (!grc_check_host()) {
        fprintf(stderr, "unsupported host: this build expects 8/16/32-bit integer sizes and 32-bit float\n");
        return 2;
    }
    if (argc < 2) {
        grc_print_usage();
        return 1;
    }
    if (strcmp(argv[1], "info") == 0) return command_info(argc, argv);
    if (strcmp(argv[1], "validate") == 0) return command_validate(argc, argv);
    if (strcmp(argv[1], "convert") == 0) return command_convert_or_clean(argc, argv, 0);
    if (strcmp(argv[1], "clean") == 0) return command_convert_or_clean(argc, argv, 1);
    if (strcmp(argv[1], "sections") == 0) return command_sections(argc, argv);
    if (strcmp(argv[1], "extract") == 0) {
        grc_u32 fmt;
        if (argc < 4) { grc_print_usage(); return 1; }
        grc_probe_file(argv[2], &fmt);
        rc = grc_re_extract_sections(argv[2], argv[3], fmt);
        if (rc != GRC_OK) {
            fprintf(stderr, "extract failed: %s\n", grc_result_string(rc));
            return 1;
        }
        printf("sections extracted with prefix: %s\n", argv[3]);
        return 0;
    } else if (strcmp(argv[1], "tim2ppm") == 0) {
        grc_texture tex;
        int pal;
        pal = 0;
        if (argc < 4) { grc_print_usage(); return 1; }
        if (argc >= 5) pal = atoi(argv[4]);
        rc = grc_tim_load_rgba(argv[2], &tex, pal);
        if (rc != GRC_OK) {
            fprintf(stderr, "tim decode failed: %s\n", grc_result_string(rc));
            return 1;
        }
        rc = grc_texture_save_ppm(argv[3], &tex);
        grc_texture_free(&tex);
        if (rc != GRC_OK) {
            fprintf(stderr, "ppm save failed: %s\n", grc_result_string(rc));
            return 1;
        }
        printf("wrote: %s\n", argv[3]);
        return 0;
    }
    grc_print_usage();
    return 1;
}
