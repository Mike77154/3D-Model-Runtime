#include <string.h>
#include "scn3d.h"
#include "scn3d_internal.h"

#define TS_HEADER_BYTES 32
#define TS_CHUNK_HEADER_BYTES 20
#define TS_FACE_FLAG_HOLE 0x08

typedef struct ts_chunk_s {
    char type[5];
    long major;
    long minor;
    long id;
    long parent;
    long data_bytes;
} ts_chunk;

static int ts_read_float_fix(scn3d_io *io, scn3d_fix *out)
{
    scn3d_u32 bits;
    if (!scn3d_read_u32_bits(io, &bits)) return 0;
    *out = scn3d_fix_from_ieee754_bits(bits);
    return 1;
}

static int ts_read_chunk(scn3d_io *io, ts_chunk *ch)
{
    unsigned char t[4];
    long v;
    if (scn3d_read_bytes(io, t, 4) != 4) return 0;
    ch->type[0] = (char)t[0];
    ch->type[1] = (char)t[1];
    ch->type[2] = (char)t[2];
    ch->type[3] = (char)t[3];
    ch->type[4] = '\0';
    if (!scn3d_read_le16(io, &v)) return 0;
    ch->major = v;
    if (!scn3d_read_le16(io, &v)) return 0;
    ch->minor = v;
    if (!scn3d_read_le32(io, &v)) return 0;
    ch->id = v;
    if (!scn3d_read_le32(io, &v)) return 0;
    ch->parent = v;
    if (!scn3d_read_le32(io, &v)) return 0;
    ch->data_bytes = v;
    return 1;
}

static int ts_read_fix_array(scn3d_io *io, scn3d_fix *m, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (!ts_read_float_fix(io, &m[i])) return 0;
    }
    return 1;
}

static scn3d_fix ts_dot3_add(const scn3d_fix *m, int row, scn3d_fix x, scn3d_fix y, scn3d_fix z)
{
    scn3d_fix r;
    int base;
    base = row * 4;
    r = scn3d_fix_mul(m[base], x);
    r += scn3d_fix_mul(m[base + 1], y);
    r += scn3d_fix_mul(m[base + 2], z);
    r += m[base + 3];
    return r;
}

static int ts_read_name(scn3d_io *io, long name_len, char *name, int cap, scn3d_callbacks *cb)
{
    long i;
    scn3d_u8 c;
    int pos;

    if (!name || cap <= 0) return 0;
    name[0] = '\0';
    pos = 0;
    for (i = 0; i < name_len; ++i) {
        if (!scn3d_read_u8(io, &c)) return 0;
        if (pos < cap - 1 && c != 0) {
            name[pos++] = (char)c;
        } else if (pos >= cap - 1 && c != 0) {
            scn3d_emit_warning(cb, SCN3D_WARN_TRUNCATED_NAME, "trueSpace object name truncated to SCN3D_MAX_NAME.");
        }
    }
    name[pos] = '\0';
    return 1;
}

static int ts_indices_sane(long count, const long *verts, long vertex_count)
{
    long i;
    if (!verts || vertex_count < 0) return 0;
    for (i = 0; i < count; ++i) {
        if (verts[i] < 0 || verts[i] >= vertex_count) return 0;
    }
    return 1;
}

static int ts_parse_polh(scn3d_io *io,
                         const ts_chunk *ch,
                         const scn3d_options *opt,
                         scn3d_callbacks *cb,
                         long object_index)
{
    long name_dup;
    long name_len;
    long vertex_count;
    long uv_count;
    long face_count;
    long i;
    long j;
    scn3d_fix x;
    scn3d_fix y;
    scn3d_fix z;
    scn3d_fix u;
    scn3d_fix v;
    scn3d_fix local_axes[12];
    scn3d_fix position[12];
    char name[SCN3D_MAX_NAME];

    if (cb && cb->on_object_begin) {
        cb->on_object_begin(cb->user, object_index, ch->id, ch->parent, ch->type);
    }

    if (!scn3d_read_le16(io, &name_dup)) return SCN3D_ERR_IO;
    if (!scn3d_read_le16(io, &name_len)) return SCN3D_ERR_IO;
    if (name_len < 0 || name_len > 65535L) return SCN3D_ERR_CORRUPT;
    if (!ts_read_name(io, name_len, name, (int)sizeof(name), cb)) return SCN3D_ERR_IO;
    if (cb && cb->on_object_name) {
        cb->on_object_name(cb->user, object_index, name);
    }
    (void)name_dup;

    /* Local axes: center + local x/y/z directions, 12 IEEE754 floats. */
    if (!ts_read_fix_array(io, local_axes, 12)) return SCN3D_ERR_IO;
    if (cb && cb->on_transform) {
        cb->on_transform(cb->user, object_index, SCN3D_TRANSFORM_LOCAL_AXES, local_axes);
    }

    /* Position: trueSpace binary stores 12 floats. It is commonly described
       as a 3x4 transform matrix: three rows, translation in the fourth column. */
    if (!ts_read_fix_array(io, position, 12)) return SCN3D_ERR_IO;
    if (cb && cb->on_transform) {
        cb->on_transform(cb->user, object_index, SCN3D_TRANSFORM_POSITION_3X4, position);
    }

    if (!scn3d_read_le32(io, &vertex_count)) return SCN3D_ERR_IO;
    if (vertex_count < 0) return SCN3D_ERR_CORRUPT;
    for (i = 0; i < vertex_count; ++i) {
        if (!ts_read_float_fix(io, &x)) return SCN3D_ERR_IO;
        if (!ts_read_float_fix(io, &y)) return SCN3D_ERR_IO;
        if (!ts_read_float_fix(io, &z)) return SCN3D_ERR_IO;
        if (opt->apply_truespace_position) {
            scn3d_fix tx;
            scn3d_fix ty;
            scn3d_fix tz;
            tx = ts_dot3_add(position, 0, x, y, z);
            ty = ts_dot3_add(position, 1, x, y, z);
            tz = ts_dot3_add(position, 2, x, y, z);
            x = tx;
            y = ty;
            z = tz;
        }
        if (cb && cb->on_vertex) {
            cb->on_vertex(cb->user, object_index, i, x, y, z);
        }
    }

    if (!scn3d_read_le32(io, &uv_count)) return SCN3D_ERR_IO;
    if (uv_count < 0) return SCN3D_ERR_CORRUPT;
    for (i = 0; i < uv_count; ++i) {
        if (!ts_read_float_fix(io, &u)) return SCN3D_ERR_IO;
        if (!ts_read_float_fix(io, &v)) return SCN3D_ERR_IO;
        if (cb && cb->on_uv) {
            cb->on_uv(cb->user, object_index, i, u, v);
        }
    }

    if (!scn3d_read_le32(io, &face_count)) return SCN3D_ERR_IO;
    if (face_count < 0) return SCN3D_ERR_CORRUPT;

    for (i = 0; i < face_count; ++i) {
        scn3d_u8 flags;
        long count;
        long material;
        long vi;
        long ui;
        long verts[SCN3D_MAX_FACE_VERTS];
        long uvs[SCN3D_MAX_FACE_VERTS];
        int store;
        int is_hole;

        if (!scn3d_read_u8(io, &flags)) return SCN3D_ERR_IO;
        if (!scn3d_read_le16(io, &count)) return SCN3D_ERR_IO;
        if (count < 0) return SCN3D_ERR_CORRUPT;

        is_hole = (flags & TS_FACE_FLAG_HOLE) ? 1 : 0;
        material = -1;
        if (!is_hole) {
            if (!scn3d_read_le16(io, &material)) return SCN3D_ERR_IO;
        } else {
            scn3d_emit_warning(cb, SCN3D_WARN_HOLE_FACE, "trueSpace hole face skipped as renderable geometry.");
        }

        store = 1;
        if (count > SCN3D_MAX_FACE_VERTS) {
            store = 0;
            scn3d_emit_warning(cb, SCN3D_WARN_FACE_TOO_BIG, "face vertex count exceeds SCN3D_MAX_FACE_VERTS; indices read but face not emitted.");
        }

        for (j = 0; j < count; ++j) {
            if (!scn3d_read_le32(io, &vi)) return SCN3D_ERR_IO;
            if (!scn3d_read_le32(io, &ui)) return SCN3D_ERR_IO;
            if (store) {
                verts[j] = vi;
                uvs[j] = ui;
            }
        }

        if (store && !is_hole) {
            if (!ts_indices_sane(count, verts, vertex_count)) {
                scn3d_emit_warning(cb, SCN3D_WARN_BAD_INDEX, "trueSpace face contains vertex indices outside vertex_count.");
            }
            if (cb && cb->on_face) {
                cb->on_face(cb->user, object_index, material, count, verts, uvs, (int)flags);
            }
            if (opt->triangulate_faces && count >= 3 && cb && cb->on_triangle) {
                for (j = 1; j < count - 1; ++j) {
                    cb->on_triangle(cb->user, object_index, material, verts[0], verts[j], verts[j + 1]);
                }
            }
        }
    }

    if (ch->minor > 4) {
        if (!scn3d_skip_bytes(io, 4)) return SCN3D_ERR_IO;
        if (ch->minor > 5 && ch->minor < 8) {
            if (!scn3d_skip_bytes(io, 2)) return SCN3D_ERR_IO;
        }
    }

    if (cb && cb->on_object_end) {
        cb->on_object_end(cb->user, object_index);
    }
    return SCN3D_OK;
}

int scn3d_decode_truespace_binary(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb)
{
    unsigned char header[TS_HEADER_BYTES];
    long chunk_count;
    long object_index;
    long max_chunks;
    long file_size;
    int status;

    if (!io || !opt) return SCN3D_ERR_BAD_ARG;
    if (!scn3d_seek_abs(io, 0)) return SCN3D_ERR_IO;
    if (scn3d_read_bytes(io, header, TS_HEADER_BYTES) != TS_HEADER_BYTES) {
        return SCN3D_ERR_IO;
    }
    if (memcmp(header, "Caligari ", 9) != 0) {
        return SCN3D_ERR_BAD_FORMAT;
    }
    if (header[15] != 'B') {
        scn3d_emit_warning(cb, SCN3D_WARN_ASCII_TS, "Caligari ASCII .scn/.cob detected; this module implements binary mode only.");
        return SCN3D_ERR_UNSUPPORTED;
    }
    if (!(header[16] == 'L' && header[17] == 'H')) {
        scn3d_emit_warning(cb, SCN3D_WARN_BIG_ENDIAN, "Caligari big-endian file detected; little-endian binary implemented.");
        return SCN3D_ERR_UNSUPPORTED;
    }

    if (cb && cb->on_format) {
        cb->on_format(cb->user, SCN3D_DIALECT_TRUESPACE_BIN, "Caligari trueSpace binary .scn/.cob");
    }

    chunk_count = 0;
    object_index = 0;
    max_chunks = opt->max_chunks > 0 ? opt->max_chunks : 250000L;
    file_size = io->size ? io->size(io->user) : -1;

    while (chunk_count < max_chunks) {
        ts_chunk ch;
        long payload_start;
        long payload_end;
        long pos;

        pos = scn3d_tell_pos(io);
        if (file_size >= 0 && pos >= file_size) {
            return SCN3D_OK;
        }
        if (file_size >= 0 && file_size - pos < TS_CHUNK_HEADER_BYTES) {
            return SCN3D_ERR_CORRUPT;
        }
        if (!ts_read_chunk(io, &ch)) {
            return SCN3D_ERR_IO;
        }
        ++chunk_count;
        payload_start = scn3d_tell_pos(io);
        if (ch.data_bytes < 0) {
            return SCN3D_ERR_CORRUPT;
        }
        if (file_size >= 0 && ch.data_bytes > file_size - payload_start) {
            return SCN3D_ERR_CORRUPT;
        }
        payload_end = payload_start + ch.data_bytes;

        if (cb && cb->on_chunk) {
            cb->on_chunk(cb->user, ch.type, ch.major, ch.minor, ch.id, ch.parent, ch.data_bytes);
        }

        if (strcmp(ch.type, "END ") == 0) {
            return SCN3D_OK;
        }

        if (strcmp(ch.type, "PolH") == 0) {
            status = ts_parse_polh(io, &ch, opt, cb, object_index);
            ++object_index;
            if (status != SCN3D_OK) {
                return status;
            }
            if (scn3d_tell_pos(io) < payload_end) {
                if (!scn3d_seek_abs(io, payload_end)) return SCN3D_ERR_IO;
            }
            if (scn3d_tell_pos(io) > payload_end) {
                return SCN3D_ERR_CORRUPT;
            }
        } else {
            scn3d_emit_warning(cb, SCN3D_WARN_SKIPPED_CHUNK, "non-PolH trueSpace chunk skipped.");
            if (!scn3d_seek_abs(io, payload_end)) return SCN3D_ERR_IO;
        }
    }

    return SCN3D_ERR_LIMIT;
}
