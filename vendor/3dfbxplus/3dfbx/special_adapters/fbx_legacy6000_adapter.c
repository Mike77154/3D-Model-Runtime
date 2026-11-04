#include "fbx_legacy6000_adapter.h"

#include <string.h>

typedef unsigned char  u8;
typedef unsigned int   u32;
typedef signed int     s32;

typedef struct node_s {
    u32 end_offset;
    u32 prop_count;
    u32 prop_len;
    u32 name_pos;
    u32 name_len;
    u32 props_pos;
    u32 children_pos;
    u32 next_pos;
} node_t;

typedef struct prop_s {
    char type;
    u32 data_pos;
    u32 data_len;
    u32 count;
    u32 encoding;
} prop_t;

static u32 rd_u32(const u8 *p) {
    return ((u32)p[0]) | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static s32 rd_s32(const u8 *p) {
    return (s32)rd_u32(p);
}

static fbx_real rd_real32(const u8 *p) {
    return fbx_real_from_ieee32_bits((fbx_u32)rd_u32(p));
}

static fbx_real rd_real64(const u8 *p) {
    fbx_u64 v;
    v = ((fbx_u64)p[0])       | ((fbx_u64)p[1] << 8)  |
        ((fbx_u64)p[2] << 16) | ((fbx_u64)p[3] << 24) |
        ((fbx_u64)p[4] << 32) | ((fbx_u64)p[5] << 40) |
        ((fbx_u64)p[6] << 48) | ((fbx_u64)p[7] << 56);
    return fbx_real_from_ieee64_bits(v);
}

static int memeq_str(const u8 *data, u32 pos, u32 len, const char *s) {
    u32 i;
    for (i = 0; s[i]; ++i) {
        if (i >= len) return 0;
        if ((char)data[pos + i] != s[i]) return 0;
    }
    return i == len;
}

static int name_is(const u8 *data, const node_t *n, const char *s) {
    return memeq_str(data, n->name_pos, n->name_len, s);
}

static void copy_clean_name(char *dst, unsigned int cap, const u8 *src, u32 len) {
    unsigned int i, j;
    if (cap == 0) return;
    j = 0;
    for (i = 0; i < len && j + 1 < cap; ++i) {
        if (src[i] == 0) break;
        dst[j++] = (char)src[i];
    }
    dst[j] = 0;
}

static int parse_node(const u8 *data, u32 size, u32 pos, node_t *out) {
    u32 nlen;
    if (!out) return GACKT_FBX6_ERR_ARG;
    if (pos + 13 > size) return GACKT_FBX6_ERR_TRUNCATED;

    out->end_offset = rd_u32(data + pos + 0);
    out->prop_count = rd_u32(data + pos + 4);
    out->prop_len   = rd_u32(data + pos + 8);
    nlen            = (u32)data[pos + 12];

    if (out->end_offset == 0 && out->prop_count == 0 && out->prop_len == 0 && nlen == 0) {
        return GACKT_FBX6_ERR_NO_MESH;
    }

    if (pos + 13 + nlen > size) return GACKT_FBX6_ERR_TRUNCATED;
    if (out->end_offset > size) return GACKT_FBX6_ERR_TRUNCATED;

    out->name_pos = pos + 13;
    out->name_len = nlen;
    out->props_pos = out->name_pos + nlen;
    out->children_pos = out->props_pos + out->prop_len;
    out->next_pos = out->end_offset;
    return GACKT_FBX6_OK;
}

static int parse_prop(const u8 *data, u32 size, u32 pos, prop_t *out, u32 *next_pos) {
    char t;
    u32 count, enc, clen;
    if (pos >= size) return GACKT_FBX6_ERR_TRUNCATED;
    t = (char)data[pos++];
    out->type = t;
    out->count = 1;
    out->encoding = 0;
    out->data_pos = pos;
    out->data_len = 0;

    switch (t) {
        case 'Y': out->data_len = 2; break;
        case 'C': out->data_len = 1; break;
        case 'I': out->data_len = 4; break;
        case 'F': out->data_len = 4; break;
        case 'D': out->data_len = 8; break;
        case 'L': out->data_len = 8; break;
        case 'S':
        case 'R':
            if (pos + 4 > size) return GACKT_FBX6_ERR_TRUNCATED;
            out->data_len = rd_u32(data + pos);
            out->data_pos = pos + 4;
            if (out->data_pos + out->data_len > size) return GACKT_FBX6_ERR_TRUNCATED;
            *next_pos = out->data_pos + out->data_len;
            return GACKT_FBX6_OK;
        case 'f':
        case 'd':
        case 'i':
        case 'l':
        case 'b':
            if (pos + 12 > size) return GACKT_FBX6_ERR_TRUNCATED;
            count = rd_u32(data + pos);
            enc   = rd_u32(data + pos + 4);
            clen  = rd_u32(data + pos + 8);
            out->count = count;
            out->encoding = enc;
            out->data_pos = pos + 12;
            out->data_len = clen;
            if (out->data_pos + out->data_len > size) return GACKT_FBX6_ERR_TRUNCATED;
            *next_pos = out->data_pos + out->data_len;
            return GACKT_FBX6_OK;
        default:
            return GACKT_FBX6_ERR_UNSUPPORTED;
    }

    if (out->data_pos + out->data_len > size) return GACKT_FBX6_ERR_TRUNCATED;
    *next_pos = out->data_pos + out->data_len;
    return GACKT_FBX6_OK;
}

static int prop_n(const u8 *data, u32 size, const node_t *node, u32 index, prop_t *out) {
    u32 pos, i, next;
    int rc;
    if (index >= node->prop_count) return GACKT_FBX6_ERR_ARG;
    pos = node->props_pos;
    for (i = 0; i <= index; ++i) {
        rc = parse_prop(data, size, pos, out, &next);
        if (rc != GACKT_FBX6_OK) return rc;
        pos = next;
    }
    return GACKT_FBX6_OK;
}

static int first_string_prop(const u8 *data, u32 size, const node_t *node, char *dst, unsigned int cap) {
    prop_t p;
    int rc = prop_n(data, size, node, 0, &p);
    if (rc != GACKT_FBX6_OK) return rc;
    if (p.type != 'S') return GACKT_FBX6_ERR_UNSUPPORTED;
    copy_clean_name(dst, cap, data + p.data_pos, p.data_len);
    return GACKT_FBX6_OK;
}

static int prop_is_string(const u8 *data, u32 size, const node_t *node, u32 idx, const char *s) {
    prop_t p;
    if (prop_n(data, size, node, idx, &p) != GACKT_FBX6_OK) return 0;
    if (p.type != 'S') return 0;
    return memeq_str(data, p.data_pos, p.data_len, s);
}

static int find_child(const u8 *data, u32 size, const node_t *parent, const char *name, node_t *out) {
    u32 pos = parent->children_pos;
    node_t n;
    int rc;
    while (pos + 13 <= parent->end_offset) {
        if (data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 0 && data[pos+3] == 0) break;
        rc = parse_node(data, size, pos, &n);
        if (rc != GACKT_FBX6_OK) break;
        if (name_is(data, &n, name)) {
            *out = n;
            return GACKT_FBX6_OK;
        }
        if (n.next_pos <= pos) break;
        pos = n.next_pos;
    }
    return GACKT_FBX6_ERR_NO_MESH;
}

static int find_objects(const u8 *data, u32 size, node_t *objects) {
    u32 pos = 27;
    node_t n;
    int rc;
    while (pos + 13 <= size) {
        if (data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 0 && data[pos+3] == 0) break;
        rc = parse_node(data, size, pos, &n);
        if (rc != GACKT_FBX6_OK) break;
        if (name_is(data, &n, "Objects")) {
            *objects = n;
            return GACKT_FBX6_OK;
        }
        if (n.next_pos <= pos) break;
        pos = n.next_pos;
    }
    return GACKT_FBX6_ERR_NO_MESH;
}

static int find_mesh_model(const u8 *data, u32 size, const node_t *objects, node_t *mesh) {
    u32 pos = objects->children_pos;
    node_t n;
    int rc;
    while (pos + 13 <= objects->end_offset) {
        if (data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 0 && data[pos+3] == 0) break;
        rc = parse_node(data, size, pos, &n);
        if (rc != GACKT_FBX6_OK) break;
        if (name_is(data, &n, "Model") && prop_is_string(data, size, &n, 1, "Mesh")) {
            *mesh = n;
            return GACKT_FBX6_OK;
        }
        if (n.next_pos <= pos) break;
        pos = n.next_pos;
    }
    return GACKT_FBX6_ERR_NO_MESH;
}

static int child_node(const u8 *data, u32 size, const node_t *parent, const char *child_name, node_t *out) {
    return find_child(data, size, parent, child_name, out);
}


static int read_node_reals_to_vec3(
    const u8 *data, u32 size, const node_t *node,
    gackt_fbx6_vec3 *dst, unsigned int maxv,
    unsigned int *out_count
) {
    prop_t p;
    unsigned int total, count3, i;
    int rc;

    rc = prop_n(data, size, node, 0, &p);
    if (rc != GACKT_FBX6_OK) return rc;

    if (p.type == 'd' || p.type == 'f') {
        if (p.encoding != 0) return GACKT_FBX6_ERR_COMPRESSED;
        total = p.count;
        if (total % 3 != 0) return GACKT_FBX6_ERR_UNSUPPORTED;
        count3 = total / 3;
        if (count3 > maxv) return GACKT_FBX6_ERR_LIMIT;
        for (i = 0; i < count3; ++i) {
            if (p.type == 'd') {
                dst[i].x = rd_real64(data + p.data_pos + (i*3+0)*8);
                dst[i].y = rd_real64(data + p.data_pos + (i*3+1)*8);
                dst[i].z = rd_real64(data + p.data_pos + (i*3+2)*8);
            } else {
                dst[i].x = rd_real32(data + p.data_pos + (i*3+0)*4);
                dst[i].y = rd_real32(data + p.data_pos + (i*3+1)*4);
                dst[i].z = rd_real32(data + p.data_pos + (i*3+2)*4);
            }
        }
        *out_count = count3;
        return GACKT_FBX6_OK;
    }

    if (p.type == 'D' || p.type == 'F') {
        total = node->prop_count;
        if (total % 3 != 0) return GACKT_FBX6_ERR_UNSUPPORTED;
        count3 = total / 3;
        if (count3 > maxv) return GACKT_FBX6_ERR_LIMIT;
        for (i = 0; i < count3; ++i) {
            prop_t px, py, pz;
            prop_n(data, size, node, i*3+0, &px);
            prop_n(data, size, node, i*3+1, &py);
            prop_n(data, size, node, i*3+2, &pz);
            if (px.type == 'D') dst[i].x = rd_real64(data + px.data_pos);
            else dst[i].x = rd_real32(data + px.data_pos);
            if (py.type == 'D') dst[i].y = rd_real64(data + py.data_pos);
            else dst[i].y = rd_real32(data + py.data_pos);
            if (pz.type == 'D') dst[i].z = rd_real64(data + pz.data_pos);
            else dst[i].z = rd_real32(data + pz.data_pos);
        }
        *out_count = count3;
        return GACKT_FBX6_OK;
    }

    return GACKT_FBX6_ERR_UNSUPPORTED;
}

static int read_node_reals_to_uv(
    const u8 *data, u32 size, const node_t *node,
    gackt_fbx6_uv *dst, unsigned int maxuv,
    unsigned int *out_count
) {
    prop_t p;
    unsigned int total, count2, i;
    int rc;

    rc = prop_n(data, size, node, 0, &p);
    if (rc != GACKT_FBX6_OK) return rc;

    if (p.type == 'd' || p.type == 'f') {
        if (p.encoding != 0) return GACKT_FBX6_ERR_COMPRESSED;
        total = p.count;
        if (total % 2 != 0) return GACKT_FBX6_ERR_UNSUPPORTED;
        count2 = total / 2;
        if (count2 > maxuv) return GACKT_FBX6_ERR_LIMIT;
        for (i = 0; i < count2; ++i) {
            if (p.type == 'd') {
                dst[i].u = rd_real64(data + p.data_pos + (i*2+0)*8);
                dst[i].v = rd_real64(data + p.data_pos + (i*2+1)*8);
            } else {
                dst[i].u = rd_real32(data + p.data_pos + (i*2+0)*4);
                dst[i].v = rd_real32(data + p.data_pos + (i*2+1)*4);
            }
        }
        *out_count = count2;
        return GACKT_FBX6_OK;
    }

    if (p.type == 'D' || p.type == 'F') {
        total = node->prop_count;
        if (total % 2 != 0) return GACKT_FBX6_ERR_UNSUPPORTED;
        count2 = total / 2;
        if (count2 > maxuv) return GACKT_FBX6_ERR_LIMIT;
        for (i = 0; i < count2; ++i) {
            prop_t pu, pv;
            prop_n(data, size, node, i*2+0, &pu);
            prop_n(data, size, node, i*2+1, &pv);
            if (pu.type == 'D') dst[i].u = rd_real64(data + pu.data_pos);
            else dst[i].u = rd_real32(data + pu.data_pos);
            if (pv.type == 'D') dst[i].v = rd_real64(data + pv.data_pos);
            else dst[i].v = rd_real32(data + pv.data_pos);
        }
        *out_count = count2;
        return GACKT_FBX6_OK;
    }

    return GACKT_FBX6_ERR_UNSUPPORTED;
}


static int int_array_value(const u8 *data, const prop_t *p, unsigned int idx, s32 *out) {
    if (p->type == 'i') {
        *out = rd_s32(data + p->data_pos + idx * 4);
        return GACKT_FBX6_OK;
    }
    return GACKT_FBX6_ERR_UNSUPPORTED;
}

static int scalar_int_node_value(const u8 *data, u32 size, const node_t *node, unsigned int idx, s32 *out) {
    prop_t p;
    int rc;
    rc = prop_n(data, size, node, idx, &p);
    if (rc != GACKT_FBX6_OK) return rc;
    if (p.type == 'I') {
        *out = rd_s32(data + p.data_pos);
        return GACKT_FBX6_OK;
    }
    return GACKT_FBX6_ERR_UNSUPPORTED;
}

static int int_list_count(const node_t *node, const prop_t *array_prop) {
    if (array_prop->type == 'i') return (int)array_prop->count;
    if (array_prop->type == 'I') return (int)node->prop_count;
    return (int)node->prop_count;
}

static int find_layer_uv(const u8 *data, u32 size, const node_t *mesh, node_t *luv) {
    return find_child(data, size, mesh, "LayerElementUV", luv);
}

static int find_texture_path(const u8 *data, u32 size, const node_t *objects, char *dst, unsigned int cap) {
    u32 pos = objects->children_pos;
    node_t n, file_node;
    int rc;
    if (cap) dst[0] = 0;
    while (pos + 13 <= objects->end_offset) {
        if (data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 0 && data[pos+3] == 0) break;
        rc = parse_node(data, size, pos, &n);
        if (rc != GACKT_FBX6_OK) break;
        if (name_is(data, &n, "Texture")) {
            if (find_child(data, size, &n, "FileName", &file_node) == GACKT_FBX6_OK) {
                first_string_prop(data, size, &file_node, dst, cap);
                return GACKT_FBX6_OK;
            }
            if (find_child(data, size, &n, "RelativeFilename", &file_node) == GACKT_FBX6_OK) {
                first_string_prop(data, size, &file_node, dst, cap);
                return GACKT_FBX6_OK;
            }
        }
        if (n.next_pos <= pos) break;
        pos = n.next_pos;
    }
    return GACKT_FBX6_ERR_NO_MESH;
}

int gackt_fbx6_probe(const unsigned char *data, unsigned long size, int *out_version) {
    int version;
    if (!data || size < 27) return GACKT_FBX6_ERR_ARG;
    if (memcmp(data, "Kaydara FBX Binary", 18) != 0) return GACKT_FBX6_ERR_NOT_FBX;
    version = (int)rd_u32(data + 23);
    if (out_version) *out_version = version;
    if (version > 6000) return GACKT_FBX6_ERR_UNSUPPORTED;
    return GACKT_FBX6_OK;
}

int gackt_fbx6_load_mesh(
    const unsigned char *data,
    unsigned long usize,
    gackt_fbx6_mesh *out_mesh,
    const gackt_fbx6_limits *limits
) {
    u32 size;
    int version, rc;
    node_t objects, mesh, luv;
    prop_t ppvi, puvi;
    node_t nverts, npvi, nuv, nuvi;
    s32 poly[64], uvpoly[64];
    unsigned int poly_n, corner, i;
    unsigned int uv_index_count;
    s32 idx, cp, ui;

    if (!data || !out_mesh || !limits) return GACKT_FBX6_ERR_ARG;
    if (!out_mesh->vertices || !out_mesh->uvs || !out_mesh->tris) return GACKT_FBX6_ERR_ARG;
    if (usize > 0x7fffffffUL) return GACKT_FBX6_ERR_LIMIT;
    size = (u32)usize;

    rc = gackt_fbx6_probe(data, usize, &version);
    if (rc != GACKT_FBX6_OK) return rc;

    out_mesh->vertex_count = 0;
    out_mesh->uv_count = 0;
    out_mesh->tri_count = 0;
    out_mesh->name[0] = 0;
    out_mesh->texture_path[0] = 0;
    out_mesh->fbx_version = version;
    out_mesh->uv_flip_v_recommended = 1;
    out_mesh->axis_x_right_y_up_neg_z_forward = 1;

    rc = find_objects(data, size, &objects);
    if (rc != GACKT_FBX6_OK) return rc;
    rc = find_mesh_model(data, size, &objects, &mesh);
    if (rc != GACKT_FBX6_OK) return rc;

    first_string_prop(data, size, &mesh, out_mesh->name, GACKT_FBX6_MAX_NAME);

    rc = child_node(data, size, &mesh, "Vertices", &nverts);
    if (rc != GACKT_FBX6_OK) return rc;
    rc = read_node_reals_to_vec3(data, size, &nverts, out_mesh->vertices, limits->max_vertices, &out_mesh->vertex_count);
    if (rc != GACKT_FBX6_OK) return rc;

    rc = child_node(data, size, &mesh, "PolygonVertexIndex", &npvi);
    if (rc != GACKT_FBX6_OK) return rc;
    rc = prop_n(data, size, &npvi, 0, &ppvi);
    if (rc != GACKT_FBX6_OK) return rc;
    if (ppvi.type == 'i' && ppvi.encoding != 0) return GACKT_FBX6_ERR_COMPRESSED;
    if (ppvi.type != 'i' && ppvi.type != 'I') return GACKT_FBX6_ERR_UNSUPPORTED;

    rc = find_layer_uv(data, size, &mesh, &luv);
    if (rc == GACKT_FBX6_OK) {
        rc = child_node(data, size, &luv, "UV", &nuv);
        if (rc != GACKT_FBX6_OK) rc = child_node(data, size, &luv, "UVVertices", &nuv);
        if (rc == GACKT_FBX6_OK) {
            rc = read_node_reals_to_uv(data, size, &nuv, out_mesh->uvs, limits->max_uvs, &out_mesh->uv_count);
            if (rc != GACKT_FBX6_OK) return rc;
        }

        rc = child_node(data, size, &luv, "UVIndex", &nuvi);
        if (rc != GACKT_FBX6_OK) rc = child_node(data, size, &luv, "UVVertexIndex", &nuvi);
        if (rc == GACKT_FBX6_OK) {
            rc = prop_n(data, size, &nuvi, 0, &puvi);
            if (rc != GACKT_FBX6_OK) return rc;
            if (puvi.type == 'i' && puvi.encoding != 0) return GACKT_FBX6_ERR_COMPRESSED;
            uv_index_count = (puvi.type == 'i') ? puvi.count : nuvi.prop_count;
        } else {
            uv_index_count = 0;
            puvi.type = 0;
            puvi.data_pos = 0;
        }
    } else {
        uv_index_count = 0;
        puvi.type = 0;
        puvi.data_pos = 0;
    }

    poly_n = 0;
    corner = 0;
    for (i = 0; i < (unsigned int)int_list_count(&npvi, &ppvi); ++i) {
        if (ppvi.type == 'i') rc = int_array_value(data, &ppvi, i, &idx);
        else rc = scalar_int_node_value(data, size, &npvi, i, &idx);
        if (rc != GACKT_FBX6_OK) return rc;

        if (idx < 0) cp = -idx - 1;
        else cp = idx;

        if (poly_n >= 64) return GACKT_FBX6_ERR_LIMIT;
        poly[poly_n] = cp;

        ui = (s32)corner;
        if (uv_index_count && corner < uv_index_count) {
            if (puvi.type == 'i') rc = int_array_value(data, &puvi, corner, &ui);
            else rc = scalar_int_node_value(data, size, &nuvi, corner, &ui);
            if (rc != GACKT_FBX6_OK) return rc;
        }
        uvpoly[poly_n] = ui;
        poly_n++;
        corner++;

        if (idx < 0) {
            unsigned int k;
            if (poly_n >= 3) {
                for (k = 1; k + 1 < poly_n; ++k) {
                    gackt_fbx6_tri *t;
                    if (out_mesh->tri_count >= limits->max_tris) return GACKT_FBX6_ERR_LIMIT;
                    t = &out_mesh->tris[out_mesh->tri_count++];
                    t->v[0] = (unsigned int)poly[0];
                    t->v[1] = (unsigned int)poly[k];
                    t->v[2] = (unsigned int)poly[k+1];
                    t->uv[0] = (unsigned int)uvpoly[0];
                    t->uv[1] = (unsigned int)uvpoly[k];
                    t->uv[2] = (unsigned int)uvpoly[k+1];
                }
            }
            poly_n = 0;
        }
    }

    find_texture_path(data, size, &objects, out_mesh->texture_path, GACKT_FBX6_MAX_TEXTURE_PATH);
    return GACKT_FBX6_OK;
}

const char *gackt_fbx6_error_string(int code) {
    switch (code) {
        case GACKT_FBX6_OK: return "OK";
        case GACKT_FBX6_ERR_ARG: return "bad argument";
        case GACKT_FBX6_ERR_NOT_FBX: return "not a binary FBX";
        case GACKT_FBX6_ERR_UNSUPPORTED: return "unsupported FBX legacy feature";
        case GACKT_FBX6_ERR_TRUNCATED: return "truncated/corrupt FBX";
        case GACKT_FBX6_ERR_LIMIT: return "caller buffer limit reached";
        case GACKT_FBX6_ERR_NO_MESH: return "no legacy mesh found";
        case GACKT_FBX6_ERR_COMPRESSED: return "compressed FBX array needs zlib hook/path";
        default: return "unknown error";
    }
}
