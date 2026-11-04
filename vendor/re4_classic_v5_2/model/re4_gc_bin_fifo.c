#include "re4c3d/re4_gcps2.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_bool gc_section(re4_span s, re4_u32 off, re4_u32 count, re4_size stride)
{
    if (count == 0U) return RE4_TRUE;
    if (off == 0U || (re4_size)count > ((re4_size)-1) / stride) return RE4_FALSE;
    return re4_span_ok(s, (re4_size)off, (re4_size)count * stride);
}
static re4_u32 gc_next_section(re4_span s, re4_u32 off, const re4_u32 *starts, re4_u32 n)
{
    re4_u32 i;
    re4_u32 next;
    next = (s.size > (re4_size)RE4_U32_MAX) ? RE4_U32_MAX : (re4_u32)s.size;
    for (i = 0; i < n; ++i) if (starts[i] > off && starts[i] < next) next = starts[i];
    return next;
}

re4_err re4_gc_bin_read(re4_span src, re4_gc_bin *out)
{
    re4_u32 offsets[11];
    re4_u32 stop;
    re4_u32 i;
    re4_gc_polygon part;
    if (out == 0 || src.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0U, 0x40UL)) return RE4_ERR_RANGE;
    out->version = re4_read_u32_be(src.data + 0x3CUL);
    if (out->version != 0x20010801U && out->version != 0x20030817U &&
        out->version != 0x20030818U) return RE4_ERR_BAD_MAGIC;
    if (out->version == 0x20030818U && !re4_span_ok(src, 0U, 0x48U)) return RE4_ERR_RANGE;
    out->file = src;
    out->colors_offset = re4_read_u32_be(src.data + 0x0CUL);
    out->texcoords_offset = re4_read_u32_be(src.data + 0x10UL);
    out->polygons_offset = re4_read_u32_be(src.data + 0x1CUL);
    out->flags = re4_read_u32_be(src.data + 0x20UL);
    out->shift = src.data[0x28UL];
    out->positions_offset = re4_read_u32_be(src.data + 0x30UL);
    out->normals_offset = re4_read_u32_be(src.data + 0x34UL);
    out->positions_count = re4_read_u16_be(src.data + 0x38UL);
    out->normals_count = re4_read_u16_be(src.data + 0x3AUL);
    out->polygon_count = re4_read_u16_be(src.data + 0x1AUL);
    if (out->shift > 30U) return RE4_ERR_UNSUPPORTED;
    if (!gc_section(src, out->positions_offset, out->positions_count, 8U) ||
        !gc_section(src, out->normals_offset, out->normals_count,
                    (out->flags & 0x20000000U) ? 4U : 8U)) return RE4_ERR_RANGE;
    offsets[0] = re4_read_u32_be(src.data + 0x00UL);
    offsets[1] = out->colors_offset;
    offsets[2] = out->texcoords_offset;
    offsets[3] = re4_read_u32_be(src.data + 0x14UL);
    offsets[4] = out->polygons_offset;
    offsets[5] = re4_read_u32_be(src.data + 0x2CUL);
    offsets[6] = out->positions_offset;
    offsets[7] = out->normals_offset;
    offsets[8] = out->version == 0x20030818U ? re4_read_u32_be(src.data + 0x40UL) : 0U;
    offsets[9] = out->version == 0x20030818U ? re4_read_u32_be(src.data + 0x44UL) : 0U;
    offsets[10] = 0U;
    if (out->colors_offset != 0U) {
        if (!re4_span_ok(src, (re4_size)out->colors_offset, 0U)) return RE4_ERR_RANGE;
        stop = gc_next_section(src, out->colors_offset, offsets, 10U);
        out->colors_count = (stop - out->colors_offset) / 4U;
    }
    if (out->texcoords_offset != 0U) {
        if (!re4_span_ok(src, (re4_size)out->texcoords_offset, 0U)) return RE4_ERR_RANGE;
        stop = gc_next_section(src, out->texcoords_offset, offsets, 10U);
        out->texcoords_count = (stop - out->texcoords_offset) / 4U;
    }
    if (out->polygon_count != 0U && out->polygons_offset == 0U) return RE4_ERR_BAD_FORMAT;
    for (i = 0U; i < out->polygon_count; ++i) {
        re4_err er;
        er = re4_gc_bin_polygon(out, i, &part);
        if (er != RE4_OK) return er;
    }
    return RE4_OK;
}

re4_err re4_gc_bin_polygon(const re4_gc_bin *bin, re4_u32 index, re4_gc_polygon *out)
{
    re4_size at;
    re4_u32 i;
    re4_u32 size;
    const re4_u8 *p;
    if (bin == 0 || out == 0 || bin->file.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (index >= bin->polygon_count) return RE4_ERR_RANGE;
    at = (re4_size)bin->polygons_offset;
    for (i = 0U; i <= index; ++i) {
        if (!re4_span_ok(bin->file, at, 0x20U)) return RE4_ERR_RANGE;
        p = bin->file.data + at;
        size = re4_read_u32_be(p + 0x18U);
        if (!re4_span_ok(bin->file, at + 0x20U, (re4_size)size)) return RE4_ERR_RANGE;
        if (i == index) {
            out->fifo.data = p + 0x20U;
            out->fifo.size = (re4_size)size;
            out->size = size;
            out->face_hint = re4_read_u32_be(p + 0x1CU);
            out->flags = p[0x0BU];
            out->diffuse_texture = p[0x0CU];
            out->bump_texture = p[0x0DU];
            out->alpha_texture = p[0x0EU];
            out->specular_texture = p[0x0FU];
            out->alpha_ref = p[0x14U];
        }
        at += (re4_size)size + 0x20U;
    }
    return RE4_OK;
}

static re4_bool gc_opcode(re4_u8 op)
{
    return (op == 0x80U || op == 0x90U || op == 0x98U || op == 0xA0U ||
            op == 0xA8U || op == 0xB0U || op == 0xB8U) ? RE4_TRUE : RE4_FALSE;
}
static re4_u32 gc_faces(re4_u8 op, re4_u32 n)
{
    if (op == 0x80U) return (n / 4U) * 2U;
    if (op == 0x90U) return n / 3U;
    if (op == 0x98U || op == 0xA0U) return n - 2U;
    return 0U;
}
static void gc_face(re4_gc_triangle *t, re4_u32 a, re4_u32 b, re4_u32 c)
{
    t->corner[0] = a; t->corner[1] = b; t->corner[2] = c;
}

/* No implicit allocation and no speculative vertex descriptor fallback. */
re4_err re4_gc_gx_decode(re4_span fifo, re4_bool with_color,
                         re4_u32 positions, re4_u32 normals,
                         re4_u32 colors, re4_u32 texcoords,
                         re4_gc_corner *corners, re4_u32 corner_cap,
                         re4_gc_triangle *triangles, re4_u32 triangle_cap,
                         re4_gc_gx_counts *out)
{
    re4_size at;
    re4_size stride;
    re4_u32 cmd, corner, face;
    re4_u32 n, nt, i, base, start_faces;
    re4_u8 op;
    const re4_u8 *p;
    re4_bool writing;
    if (out == 0 || fifo.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    writing = (corners != 0 || triangles != 0) ? RE4_TRUE : RE4_FALSE;
    if (writing && (corners == 0 || triangles == 0)) return RE4_ERR_NULL;
    stride = with_color ? 8UL : 6UL;
    at = 0U; cmd = 0U; corner = 0U; face = 0U;
    while (at < fifo.size) {
        op = fifo.data[at++];
        if (op == 0U) continue;
        if (!gc_opcode(op)) return RE4_ERR_UNSUPPORTED;
        if (!re4_span_ok(fifo, at, 2U)) return RE4_ERR_RANGE;
        n = re4_read_u16_be(fifo.data + at);
        at += 2U;
        if (n == 0U || (op == 0x80U && (n & 3U) != 0U) ||
            (op == 0x90U && (n % 3U) != 0U) ||
            ((op == 0x98U || op == 0xA0U) && n < 3U) ||
            (op == 0xA8U && (n & 1U) != 0U) ||
            (op == 0xB0U && n < 2U)) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(fifo, at, (re4_size)n * stride)) return RE4_ERR_RANGE;
        nt = gc_faces(op,n);
        if (cmd >= 131072U || corner > 2000000U - n ||
            face > 2000000U - nt) return RE4_ERR_RANGE;
        if (writing && (corner > corner_cap || n > corner_cap - corner ||
                        face > triangle_cap || nt > triangle_cap - face)) return RE4_ERR_NO_SPACE;
        base = corner;
        for (i = 0U; i < n; ++i) {
            re4_size k;
            re4_u16 pi, ni, ci, ti;
            k = at + (re4_size)i * stride;
            p = fifo.data + k;
            pi = re4_read_u16_be(p);
            ni = re4_read_u16_be(p + 2U);
            ci = with_color ? re4_read_u16_be(p + 4U) : 0U;
            ti = re4_read_u16_be(p + (with_color ? 6U : 4U));
            if (pi >= positions || ni >= normals ||
                (with_color && ci >= colors) || ti >= texcoords) return RE4_ERR_RANGE;
            if (writing) {
                corners[base + i].position = pi;
                corners[base + i].normal = ni;
                corners[base + i].color = ci;
                corners[base + i].uv = ti;
            }
        }
        at += (re4_size)n * stride;
        start_faces = face;
        if (writing) {
            if (op == 0x80U) {
                for (i = 0; i < n; i += 4U) {
                    gc_face(&triangles[face++], base+i,base+i+1U,base+i+2U);
                    gc_face(&triangles[face++], base+i,base+i+2U,base+i+3U);
                }
            } else if (op == 0x90U) {
                for (i = 0; i < n; i += 3U) gc_face(&triangles[face++],base+i,base+i+1U,base+i+2U);
            } else if (op == 0x98U) {
                for (i = 2; i < n; ++i) {
                    if (i & 1U) gc_face(&triangles[face++],base+i-1U,base+i-2U,base+i);
                    else gc_face(&triangles[face++],base+i-2U,base+i-1U,base+i);
                }
            } else if (op == 0xA0U) {
                for (i = 2; i < n; ++i) gc_face(&triangles[face++],base,base+i-1U,base+i);
            }
        }
        if (!writing) face += nt;
        else if (face - start_faces != nt) return RE4_ERR_BAD_FORMAT;
        corner += n;
        ++cmd;
    }
    out->commands = cmd;
    out->corners = corner;
    out->triangles = face;
    return RE4_OK;
}

static re4_q16 gc_q16_s16(re4_i16 v, re4_u32 frac)
{
    re4_u32 mag, n;
    re4_bool neg;
    neg = v < 0 ? RE4_TRUE : RE4_FALSE;
    mag = (re4_u32)(neg ? -(re4_i32)v : (re4_i32)v);
    if (frac > 16U) n = mag >> (frac - 16U);
    else if (mag > 0x7FFFFFFFU >> (16U - frac)) n = 0x80000000U;
    else n = mag << (16U - frac);
    return re4_clamp_i64_like_q16(n, neg);
}

re4_err re4_gc_corner_q16(const re4_gc_bin *bin, const re4_gc_corner *ref,
                          re4_q16 pos[3], re4_q16 normal[3],
                          re4_q16 uv[2], re4_u8 rgba[4])
{
    re4_u32 i;
    re4_size at;
    re4_u32 nrm_stride;
    re4_bool color_mode;
    const re4_u8 *p;
    if (!bin || !ref || !pos || !normal || !uv || !rgba) return RE4_ERR_NULL;
    if (ref->position >= bin->positions_count || ref->normal >= bin->normals_count ||
        ref->uv >= bin->texcoords_count) return RE4_ERR_RANGE;
    color_mode = (bin->flags & 0x80000000U) ? RE4_TRUE : RE4_FALSE;
    if (!color_mode || ref->color >= bin->colors_count) return RE4_ERR_UNSUPPORTED;
    at = (re4_size)bin->positions_offset + (re4_size)ref->position * 8U;
    if (!re4_span_ok(bin->file, at, 8U)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    for (i = 0; i < 3U; ++i) pos[i] = gc_q16_s16(re4_read_i16_e(p+i*2U,RE4_ENDIAN_BIG),bin->shift);
    nrm_stride = (bin->flags & 0x20000000U) ? 4U : 8U;
    at = (re4_size)bin->normals_offset + (re4_size)ref->normal * nrm_stride;
    if (!re4_span_ok(bin->file, at, nrm_stride)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    for (i = 0U; i < 3U; ++i) {
        if (nrm_stride == 4U) normal[i] = (re4_i32)(re4_i8)p[i] * 1024;
        else normal[i] = (re4_i32)re4_read_i16_e(p+i*2U,RE4_ENDIAN_BIG) * 4;
    }
    at = (re4_size)bin->texcoords_offset + (re4_size)ref->uv * 4U;
    if (!re4_span_ok(bin->file, at, 4U)) return RE4_ERR_RANGE;
    p = bin->file.data + at;
    for (i = 0U; i < 2U; ++i) {
        if (color_mode) uv[i] = (re4_i32)re4_read_i16_e(p+i*2U,RE4_ENDIAN_BIG) * 256;
        else uv[i] = (re4_i32)re4_read_u16_be(p+i*2U) * 2;
    }
    at = (re4_size)bin->colors_offset + (re4_size)ref->color * 4U;
    if (!re4_span_ok(bin->file, at, 4U)) return RE4_ERR_RANGE;
    for (i = 0U; i < 4U; ++i) rgba[i] = bin->file.data[at+i];
    return RE4_OK;
}
