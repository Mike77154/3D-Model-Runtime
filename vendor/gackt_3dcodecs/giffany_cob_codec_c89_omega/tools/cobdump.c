#include "cob_chiqui3d.h"

#include <stdio.h>
#include <string.h>

#ifndef COB_TOOL_FILE_MAX
#define COB_TOOL_FILE_MAX (8UL * 1024UL * 1024UL)
#endif

#ifndef COB_TOOL_TRI_MAX
#define COB_TOOL_TRI_MAX 32768UL
#endif

#ifndef COB_TOOL_CHUNK_MAX
#define COB_TOOL_CHUNK_MAX 4096UL
#endif

static unsigned char g_file_buffer[COB_TOOL_FILE_MAX];
static cob_scene g_scene;
static cob_scene g_world_scene;
static cob_scene g_hworld_scene;
static cob_scene g_engine_scene;
static cob_engine_report g_engine_report;
static cob_triangle g_triangles[COB_TOOL_TRI_MAX];
static cob_vec3q g_normals[COB_MAX_FACES];
static cob_chunk_info g_chunks[COB_TOOL_CHUNK_MAX];
static cob_anim_pose g_poses[COB_MAX_ANIM_POSES];


static unsigned long file_size_or_capacity(const char *path)
{
    FILE *f;
    long n;

    if (path == 0) {
        return COB_TOOL_FILE_MAX;
    }
    f = fopen(path, "rb");
    if (f == 0) {
        return COB_TOOL_FILE_MAX;
    }
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return COB_TOOL_FILE_MAX;
    }
    n = ftell(f);
    fclose(f);
    if (n < 0L) {
        return COB_TOOL_FILE_MAX;
    }
    return (unsigned long)n;
}

static void print_fixed(cob_fixed v)
{
    char buf[64];
    cob_fixed_to_cstr(v, buf, sizeof(buf), 6);
    printf("%s", buf);
}

static void print_vec3(const cob_vec3q *v)
{
    print_fixed(v->x);
    printf(", ");
    print_fixed(v->y);
    printf(", ");
    print_fixed(v->z);
}

static void print_bounds(const char *label, const cob_bounds *b)
{
    printf("%s: ", label);
    if (b == 0 || !b->valid) {
        printf("empty\n");
        return;
    }
    printf("min=(");
    print_vec3(&b->minv);
    printf(") max=(");
    print_vec3(&b->maxv);
    printf(")\n");
}

static void print_summary(const cob_scene *s)
{
    unsigned long i;
    unsigned long j;
    const cob_mesh *m;
    const cob_face *f;

    printf("COB source: %s, version %s, endian %s\n",
        s->source_format == COB_FORMAT_ASCII ? "ASCII" : (s->source_format == COB_FORMAT_BINARY ? "binary" : "unknown"),
        s->header_version,
        s->little_endian ? "LH" : "HL");
    printf("meshes=%lu materials=%lu groups=%lu lights=%lu cameras=%lu shader_stages=%lu bones=%lu anim_channels=%lu anim_keys=%lu vertices=%lu uvs=%lu faces=%lu indices=%lu diagnostics=%lu\n",
        s->mesh_count, s->material_count, s->group_count, s->light_count, s->camera_count, s->shader_stage_count,
        s->bone_count, s->anim_channel_count, s->anim_key_count,
        s->vertex_count, s->uv_count, s->face_count, s->index_count, s->diagnostic_count);
    for (i = 0; i < s->mesh_count; ++i) {
        m = &s->meshes[i];
        printf("mesh[%lu] id=%ld parent=%ld name='%s' verts=%lu uvs=%lu faces=%lu draw=%lu\n",
            i, m->id, m->parent_id, m->name, m->vertex_count, m->uv_count, m->face_count, m->draw_flags);
        printf("  transform T=(");
        print_fixed(m->transform[3]);
        printf(", ");
        print_fixed(m->transform[7]);
        printf(", ");
        print_fixed(m->transform[11]);
        printf(")\n");
        for (j = 0; j < m->face_count && j < 4UL; ++j) {
            f = &s->faces[m->first_face + j];
            printf("  face[%lu] flags=%u mat=%d n=%u\n", j, (unsigned int)f->flags, (int)f->material, (unsigned int)f->index_count);
        }
    }
    for (i = 0; i < s->scene_layer_count && i < 8UL; ++i) {
        printf("scene_layer[%lu] index=%d name='%s' visible=%u locked=%u\n",
            i, (int)s->scene_layers[i].layer_index, s->scene_layers[i].name,
            (unsigned int)s->scene_layers[i].visible, (unsigned int)s->scene_layers[i].locked);
    }
    for (i = 0; i < s->object_layer_count && i < 8UL; ++i) {
        printf("object_layer[%lu] target=%ld layer=%d hidden=%u\n",
            i, s->object_layers[i].target_id, (int)s->object_layers[i].layer_index,
            (unsigned int)s->object_layers[i].hidden);
    }
    for (i = 0; i < s->shader_stage_count && i < 8UL; ++i) {
        printf("shader_stage[%lu] parent=%ld class='%s' name='%s' params=%u flags=%u\n",
            i, s->shader_stages[i].parent_id, s->shader_stages[i].shader_class,
            s->shader_stages[i].shader_name, (unsigned int)s->shader_stages[i].parameter_count,
            (unsigned int)s->shader_stages[i].flags);
    }
    for (i = 0; i < s->bone_count && i < 8UL; ++i) {
        printf("bone[%lu] id=%ld parent=%ld name='%s'\n", i, s->bones[i].id, s->bones[i].parent_id, s->bones[i].name);
    }
    for (i = 0; i < s->anim_channel_count && i < 8UL; ++i) {
        printf("anim_channel[%lu] id=%ld target=%ld name='%s' semantic=%u keys=%lu\n", i, s->anim_channels[i].id,
            s->anim_channels[i].target_id, s->anim_channels[i].name, (unsigned int)s->anim_channels[i].semantic,
            s->anim_channels[i].key_count);
    }
    for (i = 0; i < s->diagnostic_count; ++i) {
        printf("diag[%lu] code=%d chunk=%s id=%ld %s\n", i,
            s->diagnostics[i].code,
            s->diagnostics[i].chunk_type,
            s->diagnostics[i].chunk_id,
            s->diagnostics[i].text);
    }
}

static int print_validation(const cob_scene *s)
{
    cob_scene_stats stats;
    cob_bounds bounds;
    cob_bounds wbounds;
    unsigned long tri_count;
    unsigned long i;
    int st;

    st = cob_scene_validate(s, &stats);
    printf("validation: %s (%d)\n", cob_status_text(st), st);
    printf("stats: meshes=%lu materials=%lu groups=%lu lights=%lu cameras=%lu shader_stages=%lu bones=%lu anim_channels=%lu anim_keys=%lu vertices=%lu uvs=%lu faces=%lu indices=%lu triangles=%lu holes=%lu degenerate=%lu invalid_refs=%lu empty_meshes=%lu\n",
        stats.meshes, stats.materials, stats.groups, stats.lights, stats.cameras, stats.shader_stages,
        stats.bones, stats.anim_channels, stats.anim_keys,
        stats.vertices, stats.uvs, stats.faces, stats.indices, stats.triangles,
        stats.hole_faces, stats.degenerate_faces, stats.invalid_refs, stats.empty_meshes);
    if (cob_scene_compute_bounds(s, &bounds) == COB_OK) {
        print_bounds("local bounds", &bounds);
    }
    if (cob_scene_make_world_copy(&g_world_scene, s) == COB_OK && cob_scene_compute_bounds(&g_world_scene, &wbounds) == COB_OK) {
        print_bounds("world bounds", &wbounds);
    }
    tri_count = 0UL;
    {
        cob_triangulate_stats tstats;
        if (cob_scene_build_triangles_ex(s, g_triangles, COB_TOOL_TRI_MAX, &tri_count, &tstats) == COB_OK) {
            printf("triangulation: %lu triangles, concave=%lu holes=%lu bridges=%lu failed=%lu\n",
                tri_count, tstats.concave_faces, tstats.hole_groups, tstats.bridge_count, tstats.failed_faces);
        } else {
            printf("triangulation: failed or capacity-limited after %lu triangles\n", tri_count);
        }
    }
    if (cob_scene_build_face_normals(s, g_normals, COB_MAX_FACES) == COB_OK) {
        for (i = 0; i < s->face_count && i < 4UL; ++i) {
            printf("normal[%lu]=(", i);
            print_vec3(&g_normals[i]);
            printf(")\n");
        }
    }
    if (s->material_count != 0UL) {
        cob_shader_eval eval;
        if (cob_shader_tree_eval_material(s, s->mesh_count ? s->meshes[0].id : 0L, s->materials[0].matnum, &eval) == COB_OK) {
            printf("shader_eval: applied=%lu unknown=%lu opacity=", eval.applied_stages, eval.unknown_stages);
            print_fixed(eval.opacity);
            printf(" diffuse=(");
            print_fixed(eval.diffuse[0]); printf(", "); print_fixed(eval.diffuse[1]); printf(", "); print_fixed(eval.diffuse[2]);
            printf(")\n");
        }
    }
    return st;
}


static int print_chunks_from_file_buffer(unsigned long file_size)
{
    unsigned long n;
    unsigned long i;
    int st;

    n = 0UL;
    st = cob_scan_chunks_memory(g_file_buffer, file_size, g_chunks, COB_TOOL_CHUNK_MAX, &n);
    printf("chunk scan: %s (%d), chunks=%lu\n", cob_status_text(st), st, n);
    for (i = 0; i < n && i < COB_TOOL_CHUNK_MAX; ++i) {
        printf("chunk[%lu] type=%s v=%u.%02u id=%ld parent=%ld size=%ld\n",
            i, g_chunks[i].type, (unsigned int)g_chunks[i].major, (unsigned int)g_chunks[i].minor,
            g_chunks[i].id, g_chunks[i].parent_id, g_chunks[i].size);
    }
    return st;
}


static int print_quality(const cob_scene *s)
{
    cob_quality_report q;
    int st;
    st = cob_scene_quality_report(s, &q);
    printf("quality: %s (%d)\n", cob_status_text(st), st);
    printf("quality_report: meshes=%lu faces=%lu triangles=%lu concave=%lu holes=%lu bridges=%lu tri_failed=%lu shader_stages=%lu shader_unknown=%lu anim_channels=%lu rotation_channels=%lu nurbs_ready=%lu metaball_ready=%lu\n",
        q.mesh_count, q.face_count, q.triangle_count, q.concave_count, q.hole_count, q.bridge_count,
        q.failed_triangulations, q.shader_stage_count, q.shader_unknown_count, q.anim_channel_count,
        q.anim_rotation_channels, q.nurbs_ready, q.metaball_ready);
    return st;
}


static int print_hierarchy(const cob_scene *s)
{
    cob_hierarchy_report h;
    int st;
    st = cob_scene_build_hierarchy_report(s, &h);
    printf("hierarchy: %s (%d) nodes=%lu parent_links=%lu orphans=%lu cycles=%lu max_depth=%lu layers=%lu object_layers=%lu hidden=%lu\n",
        cob_status_text(st), st, h.node_count, h.parent_links, h.orphan_count, h.cycle_count,
        h.max_depth, h.layer_count, h.object_layer_count, h.hidden_objects);
    return st;
}

static int print_runtime_report(const cob_scene *s)
{
    cob_runtime_report r;
    int st;
    st = cob_scene_runtime_report(s, &r);
    printf("runtime_report: %s (%d) meshes=%lu materials=%lu textured=%lu bump=%lu env=%lu lights=%lu cameras=%lu layers=%lu object_layers=%lu vertices=%lu triangles=%lu tangents=%lu anim_channels=%lu anim_keys=%lu\n",
        cob_status_text(st), st, r.mesh_count, r.material_count, r.textured_materials, r.bump_materials,
        r.env_materials, r.light_count, r.camera_count, r.layer_count, r.object_layer_count,
        r.vertex_count, r.triangle_count, r.tangent_count, r.anim_channel_count, r.anim_key_count);
    return st;
}

static int print_shader_sample(const cob_scene *s)
{
    cob_shader_sample sample;
    cob_vec2q uv;
    cob_vec3q p;
    int st;
    uv.u = COB_FIXED_HALF;
    uv.v = COB_FIXED_HALF;
    p.x = COB_FIXED_HALF;
    p.y = COB_FIXED_HALF;
    p.z = 0;
    st = cob_shader_tree_sample_material(s, s->mesh_count ? s->meshes[0].id : 0L, s->material_count ? s->materials[0].matnum : 0, &uv, &p, &sample);
    printf("shader_sample: %s (%d) rgba=(", cob_status_text(st), st);
    print_fixed(sample.rgba[0]); printf(", "); print_fixed(sample.rgba[1]); printf(", "); print_fixed(sample.rgba[2]); printf(", "); print_fixed(sample.rgba[3]);
    printf(") roughness="); print_fixed(sample.roughness);
    printf(" metallic="); print_fixed(sample.metallic);
    printf(" flags=%lu\n", sample.flags);
    return st;
}

static int save_pose_report(const char *path, const cob_scene *s, cob_fixed time)
{
    FILE *f;
    unsigned long count;
    unsigned long i;
    int st;
    f = fopen(path, "wb");
    if (f == 0) return COB_ERR_ARGUMENT;
    count = 0UL;
    st = cob_anim_sample_pose(s, time, g_poses, COB_MAX_ANIM_POSES, &count);
    if (st == COB_OK) {
        fprintf(f, "cob_chiqui3d pose report\n");
        fprintf(f, "poses=%lu\n", count);
        for (i = 0UL; i < count; ++i) {
            fprintf(f, "pose[%lu] target=%ld applied=%lu\n", i, g_poses[i].target_id, g_poses[i].applied_channels);
            fprintf(f, "  T=");
            { char b0[32]; char b1[32]; char b2[32]; cob_fixed_to_cstr(g_poses[i].transform[3], b0, sizeof(b0), 6); cob_fixed_to_cstr(g_poses[i].transform[7], b1, sizeof(b1), 6); cob_fixed_to_cstr(g_poses[i].transform[11], b2, sizeof(b2), 6); fprintf(f, "%s,%s,%s\n", b0, b1, b2); }
        }
    }
    fclose(f);
    return st;
}

static cob_fixed parse_cli_fixed_int(const char *s)
{
    long v;
    v = 0L;
    if (s != 0) sscanf(s, "%ld", &v);
    return cob_fixed_from_int(v);
}


static int print_features(const cob_scene *s)
{
    cob_feature_report r;
    int st;
    st = cob_scene_feature_report(s, &r);
    printf("features: %s (%d) score=%lu ascii=%lu binary=%lu geometry=%lu materials=%lu textures=%lu shaders=%lu groups=%lu lights=%lu cameras=%lu layers=%lu object_layers=%lu hidden=%lu bones=%lu anim_channels=%lu anim_keys=%lu nurbs=%lu metaballs=%lu nmesh=%lu render=%lu unsupported=%lu concave=%lu holes=%lu tri_failed=%lu\n",
        cob_status_text(st), st,
        r.engine_ready_score, r.source_ascii, r.source_binary,
        r.geometry_chunks, r.material_chunks, r.texture_slots, r.shader_stages,
        r.groups, r.lights, r.cameras, r.layers, r.object_layers, r.hidden_objects,
        r.bones, r.anim_channels, r.anim_keys, r.noted_nurbs, r.noted_metaballs,
        r.noted_nmesh, r.noted_render, r.unsupported_chunks, r.concave_faces,
        r.hole_faces, r.failed_triangulations);
    return st;
}

static int save_visible_ascii(const char *path, const cob_scene *s)
{
    int st;
    st = cob_scene_filter_hidden(&g_engine_scene, s);
    if (st != COB_OK) return st;
    return cob_save_ascii_file(path, &g_engine_scene);
}

static void usage(void)
{
    printf("cobdump - C89 fixed-point trueSpace COB/SCN reader\n");
    printf("usage:\n");
    printf("  cobdump input.cob\n");
    printf("  cobdump input.cob --validate\n");
    printf("  cobdump input.cob --chunks\n");
    printf("  cobdump input.cob --to-obj output.obj\n");
    printf("  cobdump input.cob --to-world-obj output.obj\n");
    printf("  cobdump input.cob --to-hworld-obj output.obj\n");
    printf("  cobdump input.cob --to-tri-obj output.obj\n");
    printf("  cobdump input.cob --to-smooth-obj output.obj\n");
    printf("  cobdump input.cob --to-tangent-obj output.obj\n");
    printf("  cobdump input.cob --to-mtl output.mtl\n");
    printf("  cobdump input.cob --to-objmtl output.obj materials.mtl\n");
    printf("  cobdump input.cob --to-tri-objmtl output.obj materials.mtl\n");
    printf("  cobdump input.cob --to-ascii output.cob\n");
    printf("  cobdump input.cob --to-binary output.cob\n");
    printf("  cobdump input.cob --repair-to-ascii output.cob\n");
    printf("  cobdump input.cob --quality\n");
    printf("  cobdump input.cob --hierarchy\n");
    printf("  cobdump input.cob --runtime-report\n");
    printf("  cobdump input.cob --shader-sample\n");
    printf("  cobdump input.cob --pose integer_time output.txt\n");
    printf("  cobdump input.cob --anim-csv output.csv\n");
    printf("  cobdump input.cob --report output.txt\n");
    printf("  cobdump input.cob --features\n");
    printf("  cobdump input.cob --manifest output.json\n");
    printf("  cobdump input.cob --omega-report output.txt\n");
    printf("  cobdump input.cob --to-engine-obj output.obj\n");
    printf("  cobdump input.cob --to-unit-world-obj output.obj\n");
    printf("  cobdump input.cob --to-visible-ascii output.cob\n");
}

int main(int argc, char **argv)
{
    int st;
    unsigned long file_size;

    if (argc < 2) {
        usage();
        return 1;
    }
    file_size = file_size_or_capacity(argv[1]);
    st = cob_load_file(argv[1], g_file_buffer, COB_TOOL_FILE_MAX, &g_scene);
    if (st != COB_OK) {
        printf("load failed: %s (%d)\n", cob_status_text(st), st);
        return 2;
    }
    print_summary(&g_scene);
    if (argc == 3 && strcmp(argv[2], "--validate") == 0) {
        st = print_validation(&g_scene);
    } else if (argc == 3 && strcmp(argv[2], "--chunks") == 0) {
        st = print_chunks_from_file_buffer(file_size);
    } else if (argc == 4 && strcmp(argv[2], "--to-obj") == 0) {
        st = cob_save_obj_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-world-obj") == 0) {
        st = cob_scene_make_world_copy(&g_world_scene, &g_scene);
        if (st == COB_OK) {
            st = cob_save_obj_file(argv[3], &g_world_scene);
        }
    } else if (argc == 4 && strcmp(argv[2], "--to-hworld-obj") == 0) {
        st = cob_scene_make_hierarchical_world_copy(&g_hworld_scene, &g_scene);
        if (st == COB_OK) {
            st = cob_save_obj_file(argv[3], &g_hworld_scene);
        }
    } else if (argc == 4 && strcmp(argv[2], "--to-tri-obj") == 0) {
        st = cob_save_tri_obj_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-smooth-obj") == 0) {
        st = cob_save_smooth_tri_obj_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-tangent-obj") == 0) {
        st = cob_save_tangent_obj_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-mtl") == 0) {
        st = cob_save_mtl_file(argv[3], &g_scene);
    } else if (argc == 5 && strcmp(argv[2], "--to-objmtl") == 0) {
        st = cob_save_obj_mtl_file(argv[3], &g_scene, argv[4]);
    } else if (argc == 5 && strcmp(argv[2], "--to-tri-objmtl") == 0) {
        st = cob_save_tri_obj_mtl_file(argv[3], &g_scene, argv[4]);
    } else if (argc == 4 && strcmp(argv[2], "--to-ascii") == 0) {
        st = cob_save_ascii_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-binary") == 0) {
        st = cob_save_binary_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--repair-to-ascii") == 0) {
        st = cob_scene_repair(&g_scene, COB_REPAIR_ADD_DEFAULT_UV | COB_REPAIR_CLAMP_INDICES | COB_REPAIR_FILL_EMPTY_NAMES);
        if (st == COB_OK) {
            st = cob_save_ascii_file(argv[3], &g_scene);
        }
    } else if (argc == 3 && strcmp(argv[2], "--quality") == 0) {
        st = print_quality(&g_scene);
    } else if (argc == 3 && strcmp(argv[2], "--hierarchy") == 0) {
        st = print_hierarchy(&g_scene);
    } else if (argc == 3 && strcmp(argv[2], "--runtime-report") == 0) {
        st = print_runtime_report(&g_scene);
    } else if (argc == 3 && strcmp(argv[2], "--shader-sample") == 0) {
        st = print_shader_sample(&g_scene);
    } else if (argc == 5 && strcmp(argv[2], "--pose") == 0) {
        st = save_pose_report(argv[4], &g_scene, parse_cli_fixed_int(argv[3]));
    } else if (argc == 4 && strcmp(argv[2], "--anim-csv") == 0) {
        st = cob_save_anim_csv_file(argv[3], &g_scene);
    } else if (argc == 3 && strcmp(argv[2], "--features") == 0) {
        st = print_features(&g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--manifest") == 0) {
        st = cob_save_manifest_json_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--omega-report") == 0) {
        st = cob_save_feature_report_file(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--to-engine-obj") == 0) {
        st = cob_scene_make_engine_ready_copy(&g_engine_scene, &g_scene, COB_ENGINE_DEFAULT, &g_engine_report);
        if (st == COB_OK) {
            printf("engine_ready: meshes %lu->%lu vertices %lu->%lu hidden_removed=%lu unit_scaled=%lu triangles=%lu\n",
                g_engine_report.before_meshes, g_engine_report.after_meshes,
                g_engine_report.before_vertices, g_engine_report.after_vertices,
                g_engine_report.hidden_meshes_removed, g_engine_report.unit_scaled_meshes,
                g_engine_report.triangle_count);
            st = cob_save_engine_obj_file(argv[3], &g_scene, COB_ENGINE_DEFAULT);
        }
    } else if (argc == 4 && strcmp(argv[2], "--to-unit-world-obj") == 0) {
        st = cob_scene_make_engine_ready_copy(&g_engine_scene, &g_scene, COB_ENGINE_REPAIR | COB_ENGINE_BAKE_HIERARCHY | COB_ENGINE_APPLY_UNITS, &g_engine_report);
        if (st == COB_OK) {
            st = cob_save_smooth_tri_obj_file(argv[3], &g_engine_scene);
        }
    } else if (argc == 4 && strcmp(argv[2], "--to-visible-ascii") == 0) {
        st = save_visible_ascii(argv[3], &g_scene);
    } else if (argc == 4 && strcmp(argv[2], "--report") == 0) {
        st = cob_save_report_txt_file(argv[3], &g_scene);
    } else if (argc != 2) {
        usage();
        return 1;
    }
    if (st != COB_OK) {
        printf("operation failed: %s (%d)\n", cob_status_text(st), st);
        return 3;
    }
    return 0;
}
