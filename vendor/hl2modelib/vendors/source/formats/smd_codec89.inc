/* SMD decoder and writer; private C89 core text helpers. */
static int hl2m_smd_material_id(hl2m_smd *out_smd, const char *name)
{
    int i;
    for (i = 0; i < out_smd->material_count; ++i) {
        if (hl2m_text_eq(out_smd->materials[i], name)) return i;
    }
    if (out_smd->material_count >= HL2M_MAX_SMD_MATERIALS) return -1;
    hl2m_copy_token(out_smd->materials[out_smd->material_count], HL2M_PATH_MAX, name);
    ++out_smd->material_count;
    return out_smd->material_count - 1;
}

static int hl2m_parse_smd_vertex(const char *line, int material, hl2m_smd_vertex *v)
{
    const char *p;
    int r;
    int weight_count;
    int i;
    p = line;
    v->material = material;
    r = hl2m_parse_int_ptr(&p, &v->parent_bone);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->position.x);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->position.y);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->position.z);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->normal.x);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->normal.y);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->normal.z);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->texcoord.x);
    if (r != HL2M_OK) return r;
    r = hl2m_parse_fixed_ptr(&p, &v->texcoord.y);
    if (r != HL2M_OK) return r;
    p = hl2m_skip_spaces(p);
    if (*p == 0) {
        v->weight_count = 1;
        v->weights[0].bone = v->parent_bone;
        v->weights[0].weight = HL2M_FIXED_ONE;
        return HL2M_OK;
    }
    r = hl2m_parse_int_ptr(&p, &weight_count);
    if (r != HL2M_OK) return r;
    if (weight_count < 0) weight_count = 0;
    v->weight_count = weight_count;
    if (v->weight_count > HL2M_MAX_BONES_PER_VERT) v->weight_count = HL2M_MAX_BONES_PER_VERT;
    for (i = 0; i < v->weight_count; ++i) {
        r = hl2m_parse_int_ptr(&p, &v->weights[i].bone);
        if (r != HL2M_OK) return r;
        r = hl2m_parse_fixed_ptr(&p, &v->weights[i].weight);
        if (r != HL2M_OK) return r;
    }
    return (weight_count > HL2M_MAX_BONES_PER_VERT) ? HL2M_ERR_LIMIT : HL2M_OK;
}

int hl2m_decode_smd(const hl2m_u8 *data, unsigned int size, hl2m_smd *out_smd)
{
    hl2m_stream s;
    char line[HL2M_LINE_MAX];
    char word[HL2M_TOKEN_MAX];
    char material[HL2M_PATH_MAX];
    const char *p;
    int section;
    int current_time;
    int r;
    int mid;
    int k;

    if (data == 0 || out_smd == 0) return HL2M_ERR_NULL;
    hl2m_clear_smd(out_smd);
    s.data = data;
    s.size = size;
    s.pos = 0u;
    section = 0;
    current_time = 0;
    while (hl2m_stream_line(&s, line, (unsigned int)sizeof(line))) {
        p = hl2m_skip_spaces(line);
        if (*p == 0) continue;
        if (hl2m_text_eq(p, "nodes")) { section = 1; continue; }
        if (hl2m_text_eq(p, "skeleton")) { section = 2; continue; }
        if (hl2m_text_eq(p, "triangles")) { section = 3; continue; }
        if (hl2m_text_eq(p, "end")) { section = 0; continue; }
        if (section == 0) {
            if (hl2m_parse_word_ptr(&p, word, HL2M_TOKEN_MAX) == HL2M_OK && hl2m_text_eq(word, "version")) {
                (void)hl2m_parse_int_ptr(&p, &out_smd->version);
            }
        } else if (section == 1) {
            if (out_smd->node_count >= HL2M_MAX_SMD_NODES) return HL2M_ERR_LIMIT;
            r = hl2m_parse_int_ptr(&p, &out_smd->nodes[out_smd->node_count].id);
            if (r != HL2M_OK) return r;
            r = hl2m_parse_word_ptr(&p, out_smd->nodes[out_smd->node_count].name, HL2M_NAME_MAX);
            if (r != HL2M_OK) return r;
            r = hl2m_parse_int_ptr(&p, &out_smd->nodes[out_smd->node_count].parent);
            if (r != HL2M_OK) return r;
            ++out_smd->node_count;
        } else if (section == 2) {
            if (hl2m_parse_word_ptr(&p, word, HL2M_TOKEN_MAX) != HL2M_OK) continue;
            if (hl2m_text_eq(word, "time")) {
                r = hl2m_parse_int_ptr(&p, &current_time);
                if (r != HL2M_OK) return r;
            } else {
                if (out_smd->pose_count >= HL2M_MAX_SMD_POSES) return HL2M_ERR_LIMIT;
                p = line;
                out_smd->poses[out_smd->pose_count].time = current_time;
                r = hl2m_parse_int_ptr(&p, &out_smd->poses[out_smd->pose_count].node);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].pos.x);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].pos.y);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].pos.z);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].rot.x);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].rot.y);
                if (r != HL2M_OK) return r;
                r = hl2m_parse_fixed_ptr(&p, &out_smd->poses[out_smd->pose_count].rot.z);
                if (r != HL2M_OK) return r;
                ++out_smd->pose_count;
            }
        } else if (section == 3) {
            hl2m_copy_token(material, HL2M_PATH_MAX, p);
            mid = hl2m_smd_material_id(out_smd, material);
            if (mid < 0) return HL2M_ERR_LIMIT;
            if (out_smd->triangle_count >= HL2M_MAX_SMD_TRIANGLES) return HL2M_ERR_LIMIT;
            if (out_smd->vertex_count + 3 > HL2M_MAX_SMD_VERTICES) return HL2M_ERR_LIMIT;
            out_smd->triangles[out_smd->triangle_count].material = mid;
            out_smd->triangles[out_smd->triangle_count].first_vertex = out_smd->vertex_count;
            for (k = 0; k < 3; ++k) {
                if (!hl2m_stream_line(&s, line, (unsigned int)sizeof(line))) return HL2M_ERR_PARSE;
                r = hl2m_parse_smd_vertex(line, mid, &out_smd->vertices[out_smd->vertex_count]);
                if (r != HL2M_OK && r != HL2M_ERR_LIMIT) return r;
                ++out_smd->vertex_count;
            }
            ++out_smd->triangle_count;
        }
    }
    return HL2M_OK;
}

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/vmt_codec89.inc"

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/phy_codec89.inc"

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/ani_codec89.inc"


int hl2m_smd_write(const hl2m_smd *smd, char *out_text, unsigned int out_size, unsigned int *out_used)
{
    unsigned int used;
    int ret;
    int i;
    int k;
    int t;
    int last_time;
    if (smd == 0) return HL2M_ERR_NULL;
    used = 0u;
    ret = HL2M_OK;
    if (out_text != 0 && out_size != 0u) out_text[0] = 0;
    if (hl2m_out_append_text(out_text, out_size, &used, "version 1\nnodes\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    for (i = 0; i < smd->node_count; ++i) {
        if (hl2m_out_append_int(out_text, out_size, &used, smd->nodes[i].id) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_text(out_text, out_size, &used, " \"") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_text(out_text, out_size, &used, smd->nodes[i].name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_text(out_text, out_size, &used, "\" ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, smd->nodes[i].parent) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (hl2m_out_append_text(out_text, out_size, &used, "end\nskeleton\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    last_time = -2147483647;
    for (i = 0; i < smd->pose_count; ++i) {
        if (smd->poses[i].time != last_time) {
            if (hl2m_out_append_text(out_text, out_size, &used, "time ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_int(out_text, out_size, &used, smd->poses[i].time) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            last_time = smd->poses[i].time;
        }
        if (hl2m_out_append_int(out_text, out_size, &used, smd->poses[i].node) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].pos.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].pos.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].pos.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].rot.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].rot.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, smd->poses[i].rot.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (hl2m_out_append_text(out_text, out_size, &used, "end\ntriangles\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    for (t = 0; t < smd->triangle_count; ++t) {
        int first;
        int material;
        material = smd->triangles[t].material;
        if (material < 0 || material >= smd->material_count) material = 0;
        if (hl2m_out_append_text(out_text, out_size, &used, smd->materials[material]) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        first = smd->triangles[t].first_vertex;
        if (first < 0 || first + 2 >= smd->vertex_count) return HL2M_ERR_RANGE;
        for (k = 0; k < 3; ++k) {
            const hl2m_smd_vertex *v;
            int w;
            v = &smd->vertices[first + k];
            if (hl2m_out_append_int(out_text, out_size, &used, v->parent_bone) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->position.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->position.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->position.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->normal.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->normal.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->normal.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->texcoord.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_append_fixed_qc(out_text, out_size, &used, v->texcoord.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (v->weight_count > 0) {
                if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
                if (hl2m_out_append_int(out_text, out_size, &used, v->weight_count) != HL2M_OK) ret = HL2M_ERR_LIMIT;
                for (w = 0; w < v->weight_count && w < HL2M_MAX_BONES_PER_VERT; ++w) {
                    if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
                    if (hl2m_out_append_int(out_text, out_size, &used, v->weights[w].bone) != HL2M_OK) ret = HL2M_ERR_LIMIT;
                    if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
                    if (hl2m_append_fixed_qc(out_text, out_size, &used, v->weights[w].weight) != HL2M_OK) ret = HL2M_ERR_LIMIT;
                }
            }
            if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
    }
    if (hl2m_out_append_text(out_text, out_size, &used, "end\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (out_used != 0) *out_used = used;
    return ret;
}

