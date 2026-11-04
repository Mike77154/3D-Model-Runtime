#include "hlfp_internal.h"

static int hlfp__smd_token(const char *s, int len, int *pos, const char **tok, int *tok_len) {
    int p;
    if (s == 0 || pos == 0 || tok == 0 || tok_len == 0) {
        return 0;
    }
    p = *pos;
    while (p < len && hlfp__is_space((unsigned char)s[p])) {
        ++p;
    }
    if (p >= len) {
        *pos = p;
        return 0;
    }
    *tok = s + p;
    while (p < len && !hlfp__is_space((unsigned char)s[p])) {
        ++p;
    }
    *tok_len = (int)((s + p) - *tok);
    *pos = p;
    return 1;
}

static int hlfp__smd_parse_nodes_line(const char *line, int len, hlfp_smd_model *out, int line_no, hlfp_error *err) {
    int pos;
    const char *tok;
    int tok_len;
    int id;
    int parent;
    int used;
    hlfp_smd_node *node;

    if (out->num_nodes >= HLFP_SMD_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, line_no, 1, "too many SMD nodes");
    }

    pos = 0;
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len)) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "expected node id");
    }
    if (hlfp__parse_i32_token(tok, tok_len, &id) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "invalid node id");
    }

    while (pos < len && hlfp__is_space((unsigned char)line[pos])) {
        ++pos;
    }
    if (pos >= len || line[pos] != '"') {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "expected quoted node name");
    }
    if (hlfp__parse_quoted_string(line + pos, len - pos, &used, out->nodes[out->num_nodes].name, HLFP_SMD_MAX_NAME) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid quoted node name");
    }
    pos += used;

    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len)) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "expected parent id");
    }
    if (hlfp__parse_i32_token(tok, tok_len, &parent) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid parent id");
    }

    node = &out->nodes[out->num_nodes++];
    node->id = id;
    node->parent = parent;
    return HLFP_OK;
}

static int hlfp__smd_start_frame(hlfp_smd_model *out, int time_value, int line_no, hlfp_error *err) {
    hlfp_smd_frame *prev;
    hlfp_smd_frame *frame;
    if (out->num_frames >= HLFP_SMD_MAX_FRAMES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, line_no, 1, "too many SMD frames");
    }
    if (out->num_frames > 0) {
        prev = &out->frames[out->num_frames - 1];
        prev->pose_count = out->num_poses - prev->pose_start;
    }
    frame = &out->frames[out->num_frames++];
    frame->time = time_value;
    frame->pose_start = out->num_poses;
    frame->pose_count = 0;
    return HLFP_OK;
}

static int hlfp__smd_parse_pose_line(const char *line, int len, hlfp_smd_model *out, int line_no, hlfp_error *err) {
    int pos;
    const char *tok;
    int tok_len;
    hlfp_pose *pose;
    int value_i;
    int rc;

    if (out->num_frames <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "pose before time frame in skeleton block");
    }
    if (out->num_poses >= HLFP_SMD_MAX_POSES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, line_no, 1, "too many SMD poses");
    }

    pose = &out->poses[out->num_poses];
    pos = 0;

    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len)) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "expected pose bone id");
    }
    rc = hlfp__parse_i32_token(tok, tok_len, &value_i);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "invalid pose bone id");
    }
    pose->bone = value_i;

    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->pos.x) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose x");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->pos.y) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose y");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->pos.z) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose z");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->rot.x) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose rx");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->rot.y) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose ry");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &pose->rot.z) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid pose rz");
    }

    ++out->num_poses;
    return HLFP_OK;
}

static int hlfp__smd_parse_vertex_line(const char *line, int len, hlfp_smd_vertex *v, int line_no, int *discarded, hlfp_error *err) {
    int pos;
    const char *tok;
    int tok_len;
    int i;
    int count;

    pos = 0;
    hlfp__zero(v, sizeof(*v));

    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_i32_token(tok, tok_len, &v->bone) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "invalid vertex bone id");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->pos.x) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex x");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->pos.y) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex y");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->pos.z) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex z");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->normal.x) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex nx");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->normal.y) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex ny");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->normal.z) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex nz");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->u) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex u");
    }
    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &v->v) != HLFP_OK) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex v");
    }

    if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len)) {
        v->num_links = 1;
        v->links[0].bone = v->bone;
        v->links[0].weight = HLFP_FIXED_ONE;
        return HLFP_OK;
    }

    if (hlfp__parse_i32_token(tok, tok_len, &count) != HLFP_OK || count < 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid vertex link count");
    }
    if (count > 64) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, line_no, pos + 1, "SMD vertex exceeds 64 linked bones");
    }

    v->num_links = (count < HLFP_SMD_MAX_LINKS) ? count : HLFP_SMD_MAX_LINKS;
    for (i = 0; i < count; ++i) {
        hlfp_smd_weight link;
        int slot;
        link.bone = 0;
        link.weight = 0;
        if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_i32_token(tok, tok_len, &link.bone) != HLFP_OK) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid linked bone id");
        }
        if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_fixed_token(tok, tok_len, &link.weight) != HLFP_OK || link.weight < 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid linked bone weight");
        }
        if (i < HLFP_SMD_MAX_LINKS) {
            v->links[i] = link;
        } else {
            int k;
            slot = 0;
            for (k = 1; k < HLFP_SMD_MAX_LINKS; ++k) {
                if (v->links[k].weight < v->links[slot].weight) slot = k;
            }
            if (link.weight > v->links[slot].weight) v->links[slot] = link;
        }
    }
    if (count > HLFP_SMD_MAX_LINKS) {
        hlfp_fixed total;
        int j;
        total = 0;
        if (discarded != 0) *discarded += count - HLFP_SMD_MAX_LINKS;
        for (j = 0; j < v->num_links; ++j) {
            if (v->links[j].weight > HLFP_FIXED_ONE || total > HLFP_FIXED_ONE * 64 - v->links[j].weight) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid accumulated SMD weights");
            }
            total += v->links[j].weight;
        }
        if (total > 0) {
            for (j = 0; j < v->num_links; ++j) {
                hlfp_fixed normalized;
                if (hlfp_fixed_div(v->links[j].weight, total, &normalized) != HLFP_OK) {
                    return hlfp__set_error(err, HLFP_ERR_OVERFLOW, line_no, pos + 1, "SMD weight normalization overflow");
                }
                v->links[j].weight = normalized;
            }
        } else {
            v->num_links = 1;
            v->links[0].bone = v->bone;
            v->links[0].weight = HLFP_FIXED_ONE;
        }
    }
    return HLFP_OK;
}

void hlfp_smd_init(hlfp_smd_model *out) {
    if (out != 0) {
        hlfp__zero(out, sizeof(*out));
    }
}

int hlfp_smd_parse_buffer(const char *text, size_t size, hlfp_smd_model *out, hlfp_error *err) {
    hlfp_text_cursor cur;
    const char *line;
    int len;
    int line_no;
    int section;
    int rc;
    int pos;
    const char *tok;
    int tok_len;
    int time_value;
    hlfp_smd_triangle *tri;
    int pending_vertex_index;

    enum {
        HLFP_SMD_SEC_NONE = 0,
        HLFP_SMD_SEC_NODES = 1,
        HLFP_SMD_SEC_SKELETON = 2,
        HLFP_SMD_SEC_TRIANGLES = 3
    };

    if (text == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null SMD input or output");
    }

    hlfp_error_clear(err);
    hlfp_smd_init(out);

    cur.data = text;
    cur.size = size;
    cur.pos = 0;
    cur.line = 0;
    section = HLFP_SMD_SEC_NONE;
    pending_vertex_index = -1;

    while (hlfp__read_line(&cur, &line, &len)) {
        line_no = cur.line;
        hlfp__trim_line(&line, &len);
        if (len <= 0) {
            continue;
        }

        if (len == 3 && line[0] == 'e' && line[1] == 'n' && line[2] == 'd') {
            if (section == HLFP_SMD_SEC_TRIANGLES && pending_vertex_index >= 0) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "triangles block ended mid-triangle");
            }
            if (section == HLFP_SMD_SEC_SKELETON && out->num_frames > 0) {
                out->frames[out->num_frames - 1].pose_count = out->num_poses - out->frames[out->num_frames - 1].pose_start;
            }
            section = HLFP_SMD_SEC_NONE;
            pending_vertex_index = -1;
            continue;
        }

        pos = 0;
        if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len)) {
            continue;
        }

        if (section == HLFP_SMD_SEC_NONE) {
            if (tok_len == 7 && memcmp(tok, "version", 7) == 0) {
                if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_i32_token(tok, tok_len, &out->version) != HLFP_OK) {
                    return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid SMD version");
                }
                continue;
            }
            if (tok_len == 5 && memcmp(tok, "nodes", 5) == 0) {
                section = HLFP_SMD_SEC_NODES;
                continue;
            }
            if (tok_len == 8 && memcmp(tok, "skeleton", 8) == 0) {
                section = HLFP_SMD_SEC_SKELETON;
                continue;
            }
            if (tok_len == 9 && memcmp(tok, "triangles", 9) == 0) {
                section = HLFP_SMD_SEC_TRIANGLES;
                pending_vertex_index = -1;
                continue;
            }
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, 1, "unexpected top-level SMD line");
        }

        if (section == HLFP_SMD_SEC_NODES) {
            rc = hlfp__smd_parse_nodes_line(line, len, out, line_no, err);
            if (rc != HLFP_OK) {
                return rc;
            }
            continue;
        }

        if (section == HLFP_SMD_SEC_SKELETON) {
            if (tok_len == 4 && memcmp(tok, "time", 4) == 0) {
                if (!hlfp__smd_token(line, len, &pos, &tok, &tok_len) || hlfp__parse_i32_token(tok, tok_len, &time_value) != HLFP_OK) {
                    return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, line_no, pos + 1, "invalid skeleton time");
                }
                rc = hlfp__smd_start_frame(out, time_value, line_no, err);
                if (rc != HLFP_OK) {
                    return rc;
                }
            } else {
                rc = hlfp__smd_parse_pose_line(line, len, out, line_no, err);
                if (rc != HLFP_OK) {
                    return rc;
                }
            }
            continue;
        }

        if (section == HLFP_SMD_SEC_TRIANGLES) {
            if (pending_vertex_index < 0) {
                if (out->num_triangles >= HLFP_SMD_MAX_TRIANGLES) {
                    return hlfp__set_error(err, HLFP_ERR_CAPACITY, line_no, 1, "too many SMD triangles");
                }
                tri = &out->triangles[out->num_triangles];
                hlfp__zero(tri, sizeof(*tri));
                /* Some exporter-created SMDs omit the material name entirely.
                 * If a would-be material line is actually a full vertex record,
                 * use an explicit fallback label, then parse that same line as
                 * vertex 0. Ordinary material labels are left untouched. */
                {
                    int probe_id;
                    int probe_pos;
                    int tokens;
                    const char *probe_tok;
                    int probe_len;
                    probe_id = 0;
                    probe_pos = 0;
                    tokens = 0;
                    if (hlfp__parse_i32_token(tok, tok_len, &probe_id) == HLFP_OK) {
                        while (hlfp__smd_token(line, len, &probe_pos, &probe_tok, &probe_len)) {
                            ++tokens;
                        }
                    }
                    if (tokens >= 9) {
                        hlfp__copy_cstr(tri->material, HLFP_SMD_MAX_NAME, "unnamed_material", 16);
                        pending_vertex_index = 0;
                    } else {
                        hlfp__copy_cstr(tri->material, HLFP_SMD_MAX_NAME, line, len);
                        pending_vertex_index = 0;
                        continue;
                    }
                }
            }
            {
                tri = &out->triangles[out->num_triangles];
                rc = hlfp__smd_parse_vertex_line(line, len, &tri->v[pending_vertex_index], line_no, &out->discarded_weight_links, err);
                if (rc != HLFP_OK) {
                    return rc;
                }
                ++pending_vertex_index;
                if (pending_vertex_index >= 3) {
                    ++out->num_triangles;
                    pending_vertex_index = -1;
                }
            }
            continue;
        }
    }

    if (section != HLFP_SMD_SEC_NONE) {
        if (section == HLFP_SMD_SEC_SKELETON && out->num_frames > 0) {
            out->frames[out->num_frames - 1].pose_count = out->num_poses - out->frames[out->num_frames - 1].pose_start;
        } else if (section == HLFP_SMD_SEC_TRIANGLES && pending_vertex_index >= 0) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, cur.line, 1, "unexpected EOF inside SMD triangle");
        }
    }

    return HLFP_OK;
}
