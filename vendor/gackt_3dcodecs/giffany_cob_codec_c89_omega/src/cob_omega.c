#include "cob_private.h"

/*
   cob_omega.c
   Final-pipeline helpers for trueSpace COB/SCN ingestion:
   - feature/compat reports
   - unit-scale baking
   - hidden/layer filtering
   - engine-ready smooth triangulated OBJ export
   - compact JSON manifest

   C89, fixed-point, static storage only.
*/

#ifndef COB_OMEGA_TRI_MAX
#define COB_OMEGA_TRI_MAX 32768UL
#endif

static cob_scene g_om_a;
static cob_scene g_om_b;
static cob_scene g_om_c;
static cob_triangle g_om_tris[COB_OMEGA_TRI_MAX];

static unsigned long om_abs(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed om_from_mag(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed om_mul(cob_fixed a, cob_fixed b)
{
    unsigned long am;
    unsigned long bm;
    unsigned long aw;
    unsigned long af;
    unsigned long bw;
    unsigned long bf;
    unsigned long out;
    int sign;

    sign = 1;
    if (a < 0) sign = -sign;
    if (b < 0) sign = -sign;
    am = om_abs(a);
    bm = om_abs(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += bw * af;
    out += (af * bf) >> 16;
    return om_from_mag(out, sign);
}

static int om_layer_visible(const cob_scene *scene, cob_i16 layer_index)
{
    unsigned long i;

    if (scene == 0) return 1;
    for (i = 0UL; i < scene->scene_layer_count; ++i) {
        if (scene->scene_layers[i].layer_index == layer_index) {
            return scene->scene_layers[i].visible ? 1 : 0;
        }
    }
    return 1;
}

int cob_scene_is_object_hidden(const cob_scene *scene, cob_i32 object_id)
{
    unsigned long i;
    const cob_object_layer *ol;

    if (scene == 0) return 0;
    for (i = 0UL; i < scene->object_layer_count; ++i) {
        ol = &scene->object_layers[i];
        if (ol->target_id == object_id || ol->parent_id == object_id || ol->id == object_id) {
            if (ol->hidden) return 1;
            if (ol->layer_index >= 0 && !om_layer_visible(scene, ol->layer_index)) return 1;
        }
    }
    return 0;
}

static unsigned long om_count_texture_slots(const cob_scene *scene)
{
    unsigned long i;
    unsigned long n;

    n = 0UL;
    if (scene == 0) return 0UL;
    for (i = 0UL; i < scene->material_count; ++i) {
        if (scene->materials[i].texture_color[0] != '\0') ++n;
        if (scene->materials[i].texture_bump[0] != '\0') ++n;
        if (scene->materials[i].texture_env[0] != '\0') ++n;
    }
    return n;
}

int cob_scene_feature_report(const cob_scene *scene, cob_feature_report *report)
{
    unsigned long i;
    cob_triangulate_stats ts;
    unsigned long tri_count;
    int st;
    long score;

    if (scene == 0 || report == 0) return COB_ERR_ARGUMENT;
    memset(report, 0, sizeof(*report));
    report->source_ascii = scene->source_format == COB_FORMAT_ASCII ? 1UL : 0UL;
    report->source_binary = scene->source_format == COB_FORMAT_BINARY ? 1UL : 0UL;
    report->little_endian = scene->little_endian ? 1UL : 0UL;
    report->geometry_chunks = scene->mesh_count;
    report->material_chunks = scene->material_count;
    report->texture_slots = om_count_texture_slots(scene);
    report->shader_stages = scene->shader_stage_count;
    report->groups = scene->group_count;
    report->lights = scene->light_count;
    report->cameras = scene->camera_count;
    report->layers = scene->scene_layer_count;
    report->object_layers = scene->object_layer_count;
    report->bones = scene->bone_count;
    report->anim_channels = scene->anim_channel_count;
    report->anim_keys = scene->anim_key_count;
    for (i = 0UL; i < scene->mesh_count; ++i) {
        if (cob_scene_is_object_hidden(scene, scene->meshes[i].id)) ++report->hidden_objects;
    }
    for (i = 0UL; i < scene->diagnostic_count; ++i) {
        if (scene->diagnostics[i].code == COB_ERR_UNSUPPORTED_FORMAT) ++report->unsupported_chunks;
        if (strcmp(scene->diagnostics[i].chunk_type, "Nurb") == 0 || strcmp(scene->diagnostics[i].chunk_type, "NCur") == 0 || strcmp(scene->diagnostics[i].chunk_type, "NPat") == 0) ++report->noted_nurbs;
        if (strcmp(scene->diagnostics[i].chunk_type, "Mbal") == 0) ++report->noted_metaballs;
        if (strcmp(scene->diagnostics[i].chunk_type, "NMes") == 0 || strcmp(scene->diagnostics[i].chunk_type, "NMES") == 0) ++report->noted_nmesh;
        if (strcmp(scene->diagnostics[i].chunk_type, "Vews") == 0 || strcmp(scene->diagnostics[i].chunk_type, "ROpt") == 0) ++report->noted_render;
    }
    memset(&ts, 0, sizeof(ts));
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_om_tris, COB_OMEGA_TRI_MAX, &tri_count, &ts);
    if (st != COB_OK && tri_count == 0UL) return st;
    report->concave_faces = ts.concave_faces;
    report->hole_faces = ts.hole_groups;
    report->failed_triangulations = ts.failed_faces;
    score = 100L;
    score -= (long)report->unsupported_chunks * 2L;
    score -= (long)report->failed_triangulations * 10L;
    if (scene->mesh_count == 0UL) score -= 50L;
    if (!scene->little_endian) score -= 25L;
    if (score < 0L) score = 0L;
    report->engine_ready_score = (cob_u32)score;
    return COB_OK;
}

int cob_scene_apply_units(cob_scene *dst, const cob_scene *src)
{
    unsigned long i;
    unsigned long j;
    unsigned long base;
    unsigned long count;
    cob_fixed scale;

    if (dst == 0 || src == 0) return COB_ERR_ARGUMENT;
    if (dst != src) *dst = *src;
    for (i = 0UL; i < dst->mesh_count; ++i) {
        scale = dst->meshes[i].unit_scale;
        if (scale == 0) scale = COB_FIXED_ONE;
        if (scale != COB_FIXED_ONE) {
            base = dst->meshes[i].first_vertex;
            count = dst->meshes[i].vertex_count;
            if (base + count > dst->vertex_count) return COB_ERR_PARSE;
            for (j = 0UL; j < count; ++j) {
                dst->vertices[base + j].x = om_mul(dst->vertices[base + j].x, scale);
                dst->vertices[base + j].y = om_mul(dst->vertices[base + j].y, scale);
                dst->vertices[base + j].z = om_mul(dst->vertices[base + j].z, scale);
            }
        }
        dst->meshes[i].unit_scale = COB_FIXED_ONE;
        dst->meshes[i].unit_code = 2UL;
    }
    return COB_OK;
}

static int om_copy_mesh_payload(cob_scene *dst, const cob_scene *src, const cob_mesh *sm)
{
    cob_mesh *dm;
    unsigned long i;
    unsigned long base_src;
    unsigned long count_src;
    unsigned long face_src;
    unsigned long idx_src;
    cob_face *df;
    const cob_face *sf;

    if (dst->mesh_count >= COB_MAX_MESHES) return COB_ERR_CAPACITY;
    if (sm->first_vertex + sm->vertex_count > src->vertex_count) return COB_ERR_PARSE;
    if (sm->first_uv + sm->uv_count > src->uv_count) return COB_ERR_PARSE;
    if (sm->first_face + sm->face_count > src->face_count) return COB_ERR_PARSE;
    if (dst->vertex_count + sm->vertex_count > COB_MAX_VERTICES) return COB_ERR_CAPACITY;
    if (dst->uv_count + sm->uv_count > COB_MAX_UVS) return COB_ERR_CAPACITY;
    if (dst->face_count + sm->face_count > COB_MAX_FACES) return COB_ERR_CAPACITY;

    dm = &dst->meshes[dst->mesh_count++];
    *dm = *sm;
    dm->first_vertex = dst->vertex_count;
    dm->first_uv = dst->uv_count;
    dm->first_face = dst->face_count;
    for (i = 0UL; i < sm->vertex_count; ++i) {
        dst->vertices[dst->vertex_count++] = src->vertices[sm->first_vertex + i];
    }
    for (i = 0UL; i < sm->uv_count; ++i) {
        dst->uvs[dst->uv_count++] = src->uvs[sm->first_uv + i];
    }
    for (i = 0UL; i < sm->face_count; ++i) {
        face_src = sm->first_face + i;
        sf = &src->faces[face_src];
        if (sf->first_index + sf->index_count > src->index_count) return COB_ERR_PARSE;
        if (dst->index_count + sf->index_count > COB_MAX_INDICES) return COB_ERR_CAPACITY;
        df = &dst->faces[dst->face_count++];
        *df = *sf;
        df->first_index = dst->index_count;
        base_src = sf->first_index;
        count_src = sf->index_count;
        for (idx_src = 0UL; idx_src < count_src; ++idx_src) {
            dst->indices[dst->index_count++] = src->indices[base_src + idx_src];
        }
    }
    return COB_OK;
}

int cob_scene_filter_hidden(cob_scene *dst, const cob_scene *src)
{
    unsigned long i;
    int st;

    if (dst == 0 || src == 0) return COB_ERR_ARGUMENT;
    if (dst == src) return COB_ERR_ARGUMENT;
    *dst = *src;
    dst->mesh_count = 0UL;
    dst->vertex_count = 0UL;
    dst->uv_count = 0UL;
    dst->face_count = 0UL;
    dst->index_count = 0UL;
    for (i = 0UL; i < src->mesh_count; ++i) {
        if (cob_scene_is_object_hidden(src, src->meshes[i].id)) {
            continue;
        }
        st = om_copy_mesh_payload(dst, src, &src->meshes[i]);
        if (st != COB_OK) return st;
    }
    return COB_OK;
}

static void om_fill_engine_report(const cob_scene *before, const cob_scene *after, unsigned int flags, cob_engine_report *report, unsigned long hidden_removed, unsigned long unit_scaled, int repaired)
{
    unsigned long tri_count;
    int st;

    if (report == 0 || before == 0 || after == 0) return;
    memset(report, 0, sizeof(*report));
    report->before_meshes = before->mesh_count;
    report->after_meshes = after->mesh_count;
    report->before_vertices = before->vertex_count;
    report->after_vertices = after->vertex_count;
    report->hidden_meshes_removed = hidden_removed;
    report->unit_scaled_meshes = unit_scaled;
    report->repaired = repaired ? 1UL : 0UL;
    report->hierarchy_baked = (flags & COB_ENGINE_BAKE_HIERARCHY) ? 1UL : 0UL;
    report->units_baked = (flags & COB_ENGINE_APPLY_UNITS) ? 1UL : 0UL;
    report->visible_filtered = (flags & COB_ENGINE_VISIBLE_ONLY) ? 1UL : 0UL;
    report->diagnostic_count = after->diagnostic_count;
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(after, g_om_tris, COB_OMEGA_TRI_MAX, &tri_count, 0);
    if (st == COB_OK || tri_count != 0UL) report->triangle_count = (cob_u32)tri_count;
}

int cob_scene_make_engine_ready_copy(cob_scene *dst, const cob_scene *src, unsigned int flags, cob_engine_report *report)
{
    const cob_scene *active;
    unsigned long i;
    unsigned long hidden_removed;
    unsigned long unit_scaled;
    int repaired;
    int st;

    if (dst == 0 || src == 0) return COB_ERR_ARGUMENT;
    if (flags == 0U) flags = COB_ENGINE_DEFAULT;
    g_om_a = *src;
    active = &g_om_a;
    hidden_removed = 0UL;
    unit_scaled = 0UL;
    repaired = 0;
    if (flags & COB_ENGINE_REPAIR) {
        st = cob_scene_repair(&g_om_a, COB_REPAIR_ADD_DEFAULT_UV | COB_REPAIR_CLAMP_INDICES | COB_REPAIR_FILL_EMPTY_NAMES);
        if (st != COB_OK) return st;
        repaired = 1;
    }
    if (flags & COB_ENGINE_BAKE_HIERARCHY) {
        st = cob_scene_make_hierarchical_world_copy(&g_om_b, active);
        if (st != COB_OK) return st;
        active = &g_om_b;
    }
    if (flags & COB_ENGINE_APPLY_UNITS) {
        for (i = 0UL; i < active->mesh_count; ++i) {
            if (active->meshes[i].unit_scale != 0 && active->meshes[i].unit_scale != COB_FIXED_ONE) ++unit_scaled;
        }
        st = cob_scene_apply_units(&g_om_c, active);
        if (st != COB_OK) return st;
        active = &g_om_c;
    }
    if (flags & COB_ENGINE_VISIBLE_ONLY) {
        for (i = 0UL; i < active->mesh_count; ++i) {
            if (cob_scene_is_object_hidden(active, active->meshes[i].id)) ++hidden_removed;
        }
        st = cob_scene_filter_hidden(dst, active);
        if (st != COB_OK) return st;
    } else {
        *dst = *active;
    }
    om_fill_engine_report(src, dst, flags, report, hidden_removed, unit_scaled, repaired);
    return COB_OK;
}

static int om_emit_json_string(cob_write_fn fn, void *user, const char *s)
{
    unsigned long i;
    char c[2];
    int st;

    st = cob_emit(fn, user, "\""); if (st != COB_OK) return st;
    if (s != 0) {
        c[1] = '\0';
        for (i = 0UL; s[i] != '\0'; ++i) {
            if (s[i] == '\\') { st = cob_emit(fn, user, "\\\\"); if (st != COB_OK) return st; }
            else if (s[i] == '"') { st = cob_emit(fn, user, "\\\""); if (st != COB_OK) return st; }
            else if ((unsigned char)s[i] < 32U) { st = cob_emit(fn, user, " "); if (st != COB_OK) return st; }
            else { c[0] = s[i]; st = cob_emit(fn, user, c); if (st != COB_OK) return st; }
        }
    }
    return cob_emit(fn, user, "\"");
}

static int om_emit_json_pair_u(cob_write_fn fn, void *user, const char *name, unsigned long value, int comma)
{
    int st;
    st = cob_emit(fn, user, "  "); if (st != COB_OK) return st;
    st = om_emit_json_string(fn, user, name); if (st != COB_OK) return st;
    st = cob_emit(fn, user, ": "); if (st != COB_OK) return st;
    st = cob_emit_u(fn, user, value); if (st != COB_OK) return st;
    return cob_emit(fn, user, comma ? ",\n" : "\n");
}

int cob_export_manifest_json(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    unsigned long i;
    int st;
    cob_feature_report f;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_scene_feature_report(scene, &f); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "{\n"); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "source_ascii", f.source_ascii, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "source_binary", f.source_binary, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "engine_ready_score", f.engine_ready_score, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "meshes", scene->mesh_count, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "materials", scene->material_count, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "texture_slots", f.texture_slots, 1); if (st != COB_OK) return st;
    st = om_emit_json_pair_u(write_fn, user, "triangulation_failures", f.failed_triangulations, 1); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "  \"mesh_names\": ["); if (st != COB_OK) return st;
    for (i = 0UL; i < scene->mesh_count; ++i) {
        if (i != 0UL) { st = cob_emit(write_fn, user, ", "); if (st != COB_OK) return st; }
        st = om_emit_json_string(write_fn, user, scene->meshes[i].name); if (st != COB_OK) return st;
    }
    st = cob_emit(write_fn, user, "],\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "  \"materials_detail\": [\n"); if (st != COB_OK) return st;
    for (i = 0UL; i < scene->material_count; ++i) {
        st = cob_emit(write_fn, user, "    {\"matnum\": "); if (st != COB_OK) return st;
        st = cob_emit_i(write_fn, user, scene->materials[i].matnum); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ", \"texture_color\": "); if (st != COB_OK) return st;
        st = om_emit_json_string(write_fn, user, scene->materials[i].texture_color); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ", \"texture_bump\": "); if (st != COB_OK) return st;
        st = om_emit_json_string(write_fn, user, scene->materials[i].texture_bump); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ", \"texture_env\": "); if (st != COB_OK) return st;
        st = om_emit_json_string(write_fn, user, scene->materials[i].texture_env); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, i + 1UL < scene->material_count ? "},\n" : "}\n"); if (st != COB_OK) return st;
    }
    st = cob_emit(write_fn, user, "  ],\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "  \"notes\": {"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\"nurbs\": "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, f.noted_nurbs); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, ", \"metaballs\": "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, f.noted_metaballs); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, ", \"nmesh\": "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, f.noted_nmesh); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, ", \"render\": "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, f.noted_render); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "}\n"); if (st != COB_OK) return st;
    return cob_emit(write_fn, user, "}\n");
}

int cob_export_feature_report_txt(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    cob_feature_report r;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_scene_feature_report(scene, &r); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "cob_chiqui3d OMEGA feature report\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "engine_ready_score="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.engine_ready_score); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " geometry="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.geometry_chunks); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " materials="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.material_chunks); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " textures="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.texture_slots); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " shaders="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.shader_stages); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " hidden="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.hidden_objects); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\ntriangulation concave="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.concave_faces); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " holes="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.hole_faces); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " failures="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.failed_triangulations); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\nnoted nurbs="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.noted_nurbs); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " metaballs="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.noted_metaballs); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " nmesh="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.noted_nmesh); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " render="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.noted_render); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " unsupported_chunks="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.unsupported_chunks); if (st != COB_OK) return st;
    return cob_emit(write_fn, user, "\n");
}

int cob_export_engine_obj(const cob_scene *scene, unsigned int flags, cob_write_fn write_fn, void *user)
{
    cob_engine_report report;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_scene_make_engine_ready_copy(&g_om_a, scene, flags, &report);
    if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "# cob_chiqui3d OMEGA engine-ready OBJ\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "# hidden_removed "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, report.hidden_meshes_removed); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " unit_scaled "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, report.unit_scaled_meshes); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " triangles "); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, report.triangle_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    return cob_export_smooth_tri_obj(&g_om_a, write_fn, user);
}

typedef struct om_file_writer_s {
    FILE *f;
} om_file_writer;

static int om_file_write(void *user, const void *bytes, unsigned long count)
{
    om_file_writer *w;

    w = (om_file_writer *)user;
    if (w == 0 || w->f == 0) return COB_ERR_WRITE;
    if (count == 0UL) return COB_OK;
    return fwrite(bytes, 1, (size_t)count, w->f) == (size_t)count ? COB_OK : COB_ERR_WRITE;
}

static int om_save_with(const char *path, const cob_scene *scene, int (*exporter)(const cob_scene *, cob_write_fn, void *))
{
    FILE *f;
    om_file_writer w;
    int st;

    if (path == 0 || scene == 0 || exporter == 0) return COB_ERR_ARGUMENT;
    f = fopen(path, "wb");
    if (f == 0) return COB_ERR_ARGUMENT;
    w.f = f;
    st = exporter(scene, om_file_write, &w);
    fclose(f);
    return st;
}

int cob_save_manifest_json_file(const char *path, const cob_scene *scene)
{
    return om_save_with(path, scene, cob_export_manifest_json);
}

int cob_save_feature_report_file(const char *path, const cob_scene *scene)
{
    return om_save_with(path, scene, cob_export_feature_report_txt);
}

int cob_save_engine_obj_file(const char *path, const cob_scene *scene, unsigned int flags)
{
    FILE *f;
    om_file_writer w;
    int st;

    if (path == 0 || scene == 0) return COB_ERR_ARGUMENT;
    f = fopen(path, "wb");
    if (f == 0) return COB_ERR_ARGUMENT;
    w.f = f;
    st = cob_export_engine_obj(scene, flags, om_file_write, &w);
    fclose(f);
    return st;
}
