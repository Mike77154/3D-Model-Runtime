#include "fbx_scene.h"
#include "fbx_gackt.h"

#include <string.h>

static fbx_u32 gackt_align4(fbx_u32 v)
{
    return (v + 3u) & ~3u;
}

static int gackt_add_u32(fbx_u32 *io, fbx_u32 add)
{
    if (!io) return 0;
    if (add > (fbx_u32)0xffffffffu - *io) return 0;
    *io += add;
    return 1;
}

static void gackt_wr_u32(unsigned char *p, fbx_u32 v)
{
    p[0] = (unsigned char)(v & 255u);
    p[1] = (unsigned char)((v >> 8) & 255u);
    p[2] = (unsigned char)((v >> 16) & 255u);
    p[3] = (unsigned char)((v >> 24) & 255u);
}

static void gackt_wr_i64(unsigned char *p, fbx_i64 v)
{
    fbx_u64 u;
    u = (fbx_u64)v;
    gackt_wr_u32(p + 0u, (fbx_u32)(u & 0xffffffffu));
    gackt_wr_u32(p + 4u, (fbx_u32)((u >> 32) & 0xffffffffu));
}

static fbx_u32 gackt_mesh_flags(const FbxMesh *mesh, fbx_u32 flags)
{
    fbx_u32 out;
    out = 0u;
    if (mesh && mesh->positions) out |= 1u;
    if (mesh && mesh->normals && (flags & FBX_GACKT_FLAG_WRITE_NORMALS)) out |= 2u;
    if (mesh && mesh->uvs && (flags & FBX_GACKT_FLAG_WRITE_UV0)) out |= 4u;
    if (mesh && mesh->tangents && (flags & FBX_GACKT_FLAG_WRITE_TANGENTS)) out |= 8u;
    if (mesh && mesh->bitangents && (flags & FBX_GACKT_FLAG_WRITE_TANGENTS)) out |= 16u;
    if (mesh && mesh->colors && (flags & FBX_GACKT_FLAG_WRITE_COLORS)) out |= 32u;
    return out;
}

static int gackt_count_scene(const FbxScene *scene,
                             fbx_u32 flags,
                             fbx_u32 *out_mesh_count,
                             fbx_u32 *out_vtx,
                             fbx_u32 *out_idx,
                             fbx_u32 *out_name_bytes,
                             fbx_u32 *out_data_bytes)
{
    const FbxModel *m;
    fbx_u32 mesh_count;
    fbx_u32 vtx;
    fbx_u32 idx;
    fbx_u32 name_bytes;
    fbx_u32 data_bytes;

    if (!scene || !out_mesh_count || !out_vtx || !out_idx || !out_name_bytes || !out_data_bytes) return 0;

    mesh_count = 0u;
    vtx = 0u;
    idx = 0u;
    name_bytes = 0u;
    data_bytes = 0u;

    for (m = scene->models; m; m = m->next) {
        const FbxMesh *mesh;
        fbx_u32 mf;
        fbx_u32 nlen;
        fbx_u32 vc;
        fbx_u32 ic;

        mesh = m->mesh;
        if (!mesh || !mesh->positions || !mesh->indices) continue;

        vc = mesh->vertex_count;
        ic = mesh->index_count;
        mf = gackt_mesh_flags(mesh, flags);
        if ((mf & 1u) == 0u) continue;

        if (!gackt_add_u32(&mesh_count, 1u)) return 0;
        if (!gackt_add_u32(&vtx, vc)) return 0;
        if (!gackt_add_u32(&idx, ic)) return 0;

        nlen = m->name ? (fbx_u32)strlen(m->name) : 0u;
        if (!gackt_add_u32(&name_bytes, gackt_align4(nlen + 1u))) return 0;

        if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (mf & 2u) if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (mf & 4u) if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 2u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (mf & 8u) if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (mf & 16u) if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (mf & 32u) if (!gackt_add_u32(&data_bytes, gackt_align4(vc * 4u * (fbx_u32)sizeof(fbx_real)))) return 0;
        if (!gackt_add_u32(&data_bytes, gackt_align4(ic * (fbx_u32)sizeof(fbx_u32)))) return 0;
    }

    *out_mesh_count = mesh_count;
    *out_vtx = vtx;
    *out_idx = idx;
    *out_name_bytes = name_bytes;
    *out_data_bytes = data_bytes;
    return 1;
}

fbx_u32 fbx_gackt_calc_scene_size(const FbxScene *scene, fbx_u32 flags)
{
    fbx_u32 mc, vc, ic, nb, db;
    fbx_u32 size;
    if (!gackt_count_scene(scene, flags, &mc, &vc, &ic, &nb, &db)) return 0u;
    size = FBX_GACKT_HEADER_SIZE;
    if (!gackt_add_u32(&size, mc * FBX_GACKT_MESH_RECORD_SIZE)) return 0u;
    if (!gackt_add_u32(&size, nb)) return 0u;
    if (!gackt_add_u32(&size, db)) return 0u;
    return size;
}

static void gackt_write_reals(unsigned char *dst, const fbx_real *src, fbx_u32 count)
{
    fbx_u32 i;
    for (i = 0; i < count; ++i) gackt_wr_u32(dst + i * 4u, (fbx_u32)src[i]);
}

static fbx_real gackt_vec3_len(fbx_real x, fbx_real y, fbx_real z)
{
    return fbx_real_sqrt(fbx_real_mul(x, x) + fbx_real_mul(y, y) + fbx_real_mul(z, z));
}

static void gackt_transform_vec3(const fbx_real mat[16], const fbx_real in_v[3], fbx_real out_v[3], int point)
{
    fbx_real x, y, z;
    x = in_v[0];
    y = in_v[1];
    z = in_v[2];
    out_v[0] = fbx_real_mul(x, mat[0]) + fbx_real_mul(y, mat[4]) + fbx_real_mul(z, mat[8]);
    out_v[1] = fbx_real_mul(x, mat[1]) + fbx_real_mul(y, mat[5]) + fbx_real_mul(z, mat[9]);
    out_v[2] = fbx_real_mul(x, mat[2]) + fbx_real_mul(y, mat[6]) + fbx_real_mul(z, mat[10]);
    if (point) {
        out_v[0] += mat[12];
        out_v[1] += mat[13];
        out_v[2] += mat[14];
    } else {
        fbx_real len;
        len = gackt_vec3_len(out_v[0], out_v[1], out_v[2]);
        if (len > FBX_REAL_EPSILON) {
            out_v[0] = fbx_real_div(out_v[0], len);
            out_v[1] = fbx_real_div(out_v[1], len);
            out_v[2] = fbx_real_div(out_v[2], len);
        }
    }
}

static void gackt_mat4_mul(const fbx_real a[16], const fbx_real b[16], fbx_real out[16])
{
    int r, c;
    fbx_real tmp[16];
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            tmp[r * 4 + c] =
                fbx_real_mul(a[r * 4 + 0], b[0 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 1], b[1 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 2], b[2 * 4 + c]) +
                fbx_real_mul(a[r * 4 + 3], b[3 * 4 + c]);
        }
    }
    memcpy(out, tmp, sizeof(tmp));
}

static void gackt_write_vec3_array(unsigned char *dst,
                                   const fbx_real *src,
                                   fbx_u32 count,
                                   const fbx_real mat[16],
                                   int point)
{
    fbx_u32 i;
    if (!mat) {
        gackt_write_reals(dst, src, count * 3u);
        return;
    }
    for (i = 0; i < count; ++i) {
        fbx_real out_v[3];
        gackt_transform_vec3(mat, src + (size_t)i * 3u, out_v, point);
        gackt_wr_u32(dst + (i * 3u + 0u) * 4u, (fbx_u32)out_v[0]);
        gackt_wr_u32(dst + (i * 3u + 1u) * 4u, (fbx_u32)out_v[1]);
        gackt_wr_u32(dst + (i * 3u + 2u) * 4u, (fbx_u32)out_v[2]);
    }
}

int fbx_gackt_write_scene(const FbxScene *scene,
                          void *dst,
                          fbx_u32 capacity,
                          fbx_u32 *out_size,
                          fbx_u32 flags)
{
    unsigned char *base;
    unsigned char *mesh_table;
    unsigned char *name_table;
    unsigned char *data;
    const FbxModel *m;
    fbx_u32 mc, vc_total, ic_total, nb, db;
    fbx_u32 file_size;
    fbx_u32 mesh_i;
    fbx_u32 name_cursor;
    fbx_u32 data_cursor;
    fbx_u32 mesh_table_offset;
    fbx_u32 name_table_offset;
    fbx_u32 data_offset;

    if (!scene || !dst) return FBX_GACKT_ERR_ARGUMENT;
    if (flags == 0u) flags = FBX_GACKT_FLAG_DEFAULT;

    if (!gackt_count_scene(scene, flags, &mc, &vc_total, &ic_total, &nb, &db)) return FBX_GACKT_ERR_LIMIT;
    file_size = fbx_gackt_calc_scene_size(scene, flags);
    if (file_size == 0u) return FBX_GACKT_ERR_LIMIT;
    if (capacity < file_size) {
        if (out_size) *out_size = file_size;
        return FBX_GACKT_ERR_CAPACITY;
    }

    base = (unsigned char*)dst;
    memset(base, 0, file_size);

    mesh_table_offset = FBX_GACKT_HEADER_SIZE;
    name_table_offset = mesh_table_offset + mc * FBX_GACKT_MESH_RECORD_SIZE;
    data_offset = name_table_offset + nb;

    base[0] = 'G'; base[1] = 'A'; base[2] = 'C'; base[3] = 'K';
    base[4] = 'T'; base[5] = '3'; base[6] = 'D'; base[7] = 0;
    gackt_wr_u32(base + 8u, FBX_GACKT_VERSION);
    gackt_wr_u32(base + 12u, FBX_GACKT_HEADER_SIZE);
    gackt_wr_u32(base + 16u, flags);
    gackt_wr_u32(base + 20u, mc);
    gackt_wr_u32(base + 24u, vc_total);
    gackt_wr_u32(base + 28u, ic_total);
    gackt_wr_u32(base + 32u, mesh_table_offset);
    gackt_wr_u32(base + 36u, name_table_offset);
    gackt_wr_u32(base + 40u, nb);
    gackt_wr_u32(base + 44u, file_size);
    gackt_wr_u32(base + 48u, data_offset);
    gackt_wr_u32(base + 52u, db);

    mesh_table = base + mesh_table_offset;
    name_table = base + name_table_offset;
    data = base + data_offset;
    mesh_i = 0u;
    name_cursor = 0u;
    data_cursor = 0u;

    for (m = scene->models; m; m = m->next) {
        const FbxMesh *mesh;
        fbx_u32 mf;
        fbx_u32 nlen;
        fbx_u32 vc;
        fbx_u32 ic;
        fbx_u32 rec;
        fbx_u32 pos_off, nrm_off, uv_off, tan_off, bin_off, col_off, idx_off;
        fbx_real bake_mat[16];
        fbx_real *bake_ptr;

        mesh = m->mesh;
        if (!mesh || !mesh->positions || !mesh->indices) continue;
        mf = gackt_mesh_flags(mesh, flags);
        if ((mf & 1u) == 0u) continue;

        vc = mesh->vertex_count;
        ic = mesh->index_count;
        rec = mesh_i * FBX_GACKT_MESH_RECORD_SIZE;

        nlen = m->name ? (fbx_u32)strlen(m->name) : 0u;
        if (nlen > 0u) memcpy(name_table + name_cursor, m->name, nlen);
        name_table[name_cursor + nlen] = 0;

        bake_ptr = (fbx_real*)0;
        if (flags & FBX_GACKT_FLAG_BAKE_TRANSFORMS) {
            gackt_mat4_mul(m->geom_matrix, m->global_matrix, bake_mat);
            bake_ptr = bake_mat;
        }

        pos_off = data_offset + data_cursor;
        gackt_write_vec3_array(data + data_cursor, mesh->positions, vc, bake_ptr, 1);
        data_cursor += gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real));

        nrm_off = 0u;
        if (mf & 2u) {
            nrm_off = data_offset + data_cursor;
            gackt_write_vec3_array(data + data_cursor, mesh->normals, vc, bake_ptr, 0);
            data_cursor += gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real));
        }
        uv_off = 0u;
        if (mf & 4u) {
            uv_off = data_offset + data_cursor;
            gackt_write_reals(data + data_cursor, mesh->uvs, vc * 2u);
            data_cursor += gackt_align4(vc * 2u * (fbx_u32)sizeof(fbx_real));
        }
        tan_off = 0u;
        if (mf & 8u) {
            tan_off = data_offset + data_cursor;
            gackt_write_vec3_array(data + data_cursor, mesh->tangents, vc, bake_ptr, 0);
            data_cursor += gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real));
        }
        bin_off = 0u;
        if (mf & 16u) {
            bin_off = data_offset + data_cursor;
            gackt_write_vec3_array(data + data_cursor, mesh->bitangents, vc, bake_ptr, 0);
            data_cursor += gackt_align4(vc * 3u * (fbx_u32)sizeof(fbx_real));
        }
        col_off = 0u;
        if (mf & 32u) {
            col_off = data_offset + data_cursor;
            gackt_write_reals(data + data_cursor, mesh->colors, vc * 4u);
            data_cursor += gackt_align4(vc * 4u * (fbx_u32)sizeof(fbx_real));
        }
        idx_off = data_offset + data_cursor;
        {
            fbx_u32 i;
            for (i = 0; i < ic; ++i) gackt_wr_u32(data + data_cursor + i * 4u, mesh->indices[i]);
        }
        data_cursor += gackt_align4(ic * (fbx_u32)sizeof(fbx_u32));

        gackt_wr_i64(mesh_table + rec + 0u, m->id);
        gackt_wr_u32(mesh_table + rec + 8u, name_table_offset + name_cursor);
        gackt_wr_u32(mesh_table + rec + 12u, nlen);
        gackt_wr_u32(mesh_table + rec + 16u, vc);
        gackt_wr_u32(mesh_table + rec + 20u, ic);
        gackt_wr_u32(mesh_table + rec + 24u, mf);
        gackt_wr_u32(mesh_table + rec + 28u, pos_off);
        gackt_wr_u32(mesh_table + rec + 32u, nrm_off);
        gackt_wr_u32(mesh_table + rec + 36u, uv_off);
        gackt_wr_u32(mesh_table + rec + 40u, tan_off);
        gackt_wr_u32(mesh_table + rec + 44u, bin_off);
        gackt_wr_u32(mesh_table + rec + 48u, col_off);
        gackt_wr_u32(mesh_table + rec + 52u, idx_off);
        gackt_wr_u32(mesh_table + rec + 56u, 0xffffffffu); /* material index placeholder */

        name_cursor += gackt_align4(nlen + 1u);
        mesh_i += 1u;
    }

    if (out_size) *out_size = file_size;
    return FBX_GACKT_OK;
}
