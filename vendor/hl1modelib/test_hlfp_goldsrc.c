#include "hlfp_goldsrc.h"

#include <stdio.h>
#include <string.h>

#define TEST_BUF_CAP 131072
#define TEXT_BUF_CAP 65536
#define RGBA_BUF_CAP 65536

static int ok(int rc, const hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        printf("FAIL goldsrc %s: code=%d msg=%s\n", what, err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return 0;
    }
    return 1;
}

static int has_text(const hlfp_u8 *buf, size_t n, const char *needle) {
    size_t i;
    size_t m;
    m = strlen(needle);
    if (m == 0U || n < m) {
        return 0;
    }
    for (i = 0U; i + m <= n; ++i) {
        if (memcmp(buf + i, needle, m) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(void) {
    static hlfp_u8 mdl_bytes[TEST_BUF_CAP];
    static hlfp_u8 rgba[RGBA_BUF_CAP];
    static hlfp_u8 obj_text[TEXT_BUF_CAP];
    static hlfp_u8 qc_text[TEXT_BUF_CAP];
    static hlfp_u8 report_text[TEXT_BUF_CAP];
    const char *reference_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "end\n"
        "triangles\n"
        "mat\n"
        "0 1 0 0 0 0 1 0 0\n"
        "0 0 1 0 0 0 1 1 0\n"
        "0 0 0 0 0 0 1 0 1\n"
        "end\n";
    const char *seq_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "end\n";
    hlfp_smd_model ref;
    hlfp_smd_model seq;
    hlfp_mdl_compile_sequence seqs[1];
    hlfp_mdl_compile_options opts;
    hlfp_mdl mdl;
    hlfp_mdl_mesh_expanded mesh;
    hlfp_mdl_pose_set pose;
    hlfp_mat3x4 mats[HLFP_MDL_MAX_BONES];
    hlfp_mem_writer w;
    hlfp_error err;
    size_t mdl_size;
    size_t rgba_size;
    size_t obj_size;
    size_t qc_size;
    size_t report_size;
    int body_value;
    int body_choice;
    int model_index;
    int tex_index;
    int width;
    int height;
    char companion[128];
    int rc;

    hlfp_smd_init(&ref);
    hlfp_smd_init(&seq);
    hlfp_error_clear(&err);
    rc = hlfp_smd_parse_buffer(reference_text, strlen(reference_text), &ref, &err);
    if (!ok(rc, &err, "parse ref")) return 1;
    rc = hlfp_smd_parse_buffer(seq_text, strlen(seq_text), &seq, &err);
    if (!ok(rc, &err, "parse seq")) return 1;
    memset(&opts, 0, sizeof(opts));
    memset(&seqs, 0, sizeof(seqs));
    strcpy(opts.name, "goldsrc_steroids.mdl");
    opts.create_dummy_textures = HLFP_TRUE;
    strcpy(seqs[0].label, "idle");
    seqs[0].fps = 30;
    seqs[0].flags = HLFP_STUDIO_LOOPING;
    seqs[0].animation = &seq;
    rc = hlfp_mdl_compile_from_smd(&ref, seqs, 1, 0, 0, &opts, mdl_bytes, sizeof(mdl_bytes), &mdl_size, &err);
    if (!ok(rc, &err, "compile")) return 1;
    rc = hlfp_mdl_parse_main(mdl_bytes, mdl_size, &mdl, &err);
    if (!ok(rc, &err, "parse mdl")) return 1;

    rc = hlfp_goldsrc_make_texture_filename("models/player/barney/barney", HLFP_FALSE, companion, (int)sizeof(companion), &err);
    if (!ok(rc, &err, "texture filename")) return 1;
    if (strcmp(companion, "models/player/barney/barneyT.mdl") != 0) return 1;
    rc = hlfp_goldsrc_make_seqgroup_filename("models/player/barney/barney", 1, HLFP_FALSE, companion, (int)sizeof(companion), &err);
    if (!ok(rc, &err, "seqgroup filename")) return 1;
    if (strcmp(companion, "models/player/barney/barney01.mdl") != 0) return 1;

    body_value = 0;
    rc = hlfp_goldsrc_get_bodypart_value(&mdl, body_value, 0, &body_choice, &err);
    if (!ok(rc, &err, "body get")) return 1;
    rc = hlfp_goldsrc_set_bodypart_value(&mdl, 0, body_choice, &body_value, &err);
    if (!ok(rc, &err, "body set")) return 1;
    rc = hlfp_goldsrc_model_for_bodypart(&mdl, body_value, 0, &model_index, &err);
    if (!ok(rc, &err, "body model")) return 1;
    rc = hlfp_goldsrc_resolve_texture_for_mesh(&mdl, mdl.models[model_index].first_mesh, 0, &tex_index, &err);
    if (!ok(rc, &err, "texture resolve")) return 1;
    rc = hlfp_goldsrc_expand_mesh_skin(&mdl, model_index, mdl.models[model_index].first_mesh, 0, &mesh, &err);
    if (!ok(rc, &err, "mesh skin expand")) return 1;
    rc = hlfp_goldsrc_texture_to_rgba32(&mdl, tex_index, HLFP_FALSE, rgba, sizeof(rgba), &rgba_size, &width, &height, &err);
    if (!ok(rc, &err, "rgba")) return 1;
    rc = hlfp_goldsrc_make_bind_pose(&mdl, &pose, &err);
    if (!ok(rc, &err, "bind pose")) return 1;
    rc = hlfp_mdl_pose_to_world_matrices(&mdl, &pose, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "bind mats")) return 1;
    rc = hlfp_goldsrc_bake_mesh_with_matrices(&mdl, model_index, mdl.models[model_index].first_mesh, 0, mats, HLFP_MDL_MAX_BONES, &mesh, &err);
    if (!ok(rc, &err, "bake mesh")) return 1;

    hlfp_mem_writer_init(&w, obj_text, sizeof(obj_text));
    rc = hlfp_goldsrc_export_obj_sequence_frame(&mdl, 0, 0, 0, 0, 0, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "obj export")) return 1;
    obj_size = w.size;
    if (!has_text(obj_text, w.size, "f ")) {
        printf("FAIL goldsrc obj lacks faces\n");
        return 1;
    }

    hlfp_mem_writer_init(&w, qc_text, sizeof(qc_text));
    rc = hlfp_goldsrc_write_qc(&mdl, "goldsrc_steroids.mdl", hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "qc writer")) return 1;
    qc_size = w.size;
    if (!has_text(qc_text, w.size, "$sequence")) {
        printf("FAIL goldsrc qc lacks sequence\n");
        return 1;
    }

    hlfp_mem_writer_init(&w, report_text, sizeof(report_text));
    rc = hlfp_goldsrc_write_report(&mdl, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "report")) return 1;
    report_size = w.size;
    if (!has_text(report_text, w.size, "diag:")) {
        printf("FAIL goldsrc report lacks diag\n");
        return 1;
    }

    printf("goldsrc_steroids_ok=1 mdl_size=%lu body=%d model=%d mesh_v=%d mesh_t=%d tex=%d rgba=%lux%lu/%lu obj=%lu qc=%lu report=%lu\n",
           (unsigned long)mdl_size, body_value, model_index, mesh.num_vertices, mesh.num_triangles, tex_index,
           (unsigned long)width, (unsigned long)height, (unsigned long)rgba_size,
           (unsigned long)obj_size, (unsigned long)qc_size, (unsigned long)report_size);
    printf("goldsrc_export_checks=companion_names bodygroups skins rgba bindpose bakedmesh obj qc report\n");
    return 0;
}
