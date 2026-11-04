#include <string.h>
#include "scn3d.h"
#include "scn3d_internal.h"

static int vol_read_line(scn3d_io *io, char *buf, int cap)
{
    int n;
    scn3d_u8 c;
    n = 0;
    if (!buf || cap <= 1) return 0;
    while (n < cap - 1) {
        if (!scn3d_read_u8(io, &c)) return 0;
        if (c == '\n') {
            buf[n] = '\0';
            return 1;
        }
        if (c != '\r') {
            buf[n++] = (char)c;
        }
    }
    buf[n] = '\0';
    return 0;
}

static const char *vol_skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}

static int vol_parse_long(const char **ps, long *out)
{
    const char *s;
    int neg;
    long v;
    int any;
    s = vol_skip_ws(*ps);
    neg = 0;
    v = 0;
    any = 0;
    if (*s == '-') {
        neg = 1;
        ++s;
    } else if (*s == '+') {
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        any = 1;
        v = v * 10L + (long)(*s - '0');
        ++s;
    }
    if (!any) return 0;
    *out = neg ? -v : v;
    *ps = s;
    return 1;
}

static int vol_parse_fix3(const char *line, scn3d_fix *a, scn3d_fix *b, scn3d_fix *c)
{
    const char *s;
    s = line;
    *a = scn3d_fix_from_decimal_text(s, &s);
    s = vol_skip_ws(s);
    if (*s == '\0') return 0;
    *b = scn3d_fix_from_decimal_text(s, &s);
    s = vol_skip_ws(s);
    if (*s == '\0') return 0;
    *c = scn3d_fix_from_decimal_text(s, &s);
    return 1;
}

static int vol_parse_long3(const char *line, long *a, long *b, long *c)
{
    const char *s;
    s = line;
    if (!vol_parse_long(&s, a)) return 0;
    if (!vol_parse_long(&s, b)) return 0;
    if (!vol_parse_long(&s, c)) return 0;
    return 1;
}

static int vol_read_sample(scn3d_io *io, long bytes_per_sample, unsigned long *sample)
{
    scn3d_u8 b[4];
    long i;
    unsigned long v;
    if (bytes_per_sample < 1 || bytes_per_sample > 4) return 0;
    if (scn3d_read_bytes(io, b, bytes_per_sample) != bytes_per_sample) return 0;
    v = 0UL;
    for (i = 0; i < bytes_per_sample; ++i) {
        v |= ((unsigned long)b[i]) << (8 * i);
    }
    *sample = v;
    return 1;
}

static void vol_emit_cube(scn3d_callbacks *cb,
                          long object_index,
                          long *vertex_base,
                          scn3d_fix wx,
                          scn3d_fix wy,
                          scn3d_fix wz,
                          scn3d_fix sx,
                          scn3d_fix sy,
                          scn3d_fix sz)
{
    scn3d_fix x0;
    scn3d_fix y0;
    scn3d_fix z0;
    scn3d_fix x1;
    scn3d_fix y1;
    scn3d_fix z1;
    long b;

    if (!cb) return;
    x0 = wx;
    y0 = wy;
    z0 = wz;
    x1 = wx + sx;
    y1 = wy + sy;
    z1 = wz + sz;
    b = *vertex_base;

    if (cb->on_vertex) {
        cb->on_vertex(cb->user, object_index, b + 0L, x0, y0, z0);
        cb->on_vertex(cb->user, object_index, b + 1L, x1, y0, z0);
        cb->on_vertex(cb->user, object_index, b + 2L, x1, y1, z0);
        cb->on_vertex(cb->user, object_index, b + 3L, x0, y1, z0);
        cb->on_vertex(cb->user, object_index, b + 4L, x0, y0, z1);
        cb->on_vertex(cb->user, object_index, b + 5L, x1, y0, z1);
        cb->on_vertex(cb->user, object_index, b + 6L, x1, y1, z1);
        cb->on_vertex(cb->user, object_index, b + 7L, x0, y1, z1);
    }
    if (cb->on_triangle) {
        cb->on_triangle(cb->user, object_index, -1L, b + 0L, b + 1L, b + 2L);
        cb->on_triangle(cb->user, object_index, -1L, b + 0L, b + 2L, b + 3L);
        cb->on_triangle(cb->user, object_index, -1L, b + 4L, b + 6L, b + 5L);
        cb->on_triangle(cb->user, object_index, -1L, b + 4L, b + 7L, b + 6L);
        cb->on_triangle(cb->user, object_index, -1L, b + 0L, b + 4L, b + 5L);
        cb->on_triangle(cb->user, object_index, -1L, b + 0L, b + 5L, b + 1L);
        cb->on_triangle(cb->user, object_index, -1L, b + 1L, b + 5L, b + 6L);
        cb->on_triangle(cb->user, object_index, -1L, b + 1L, b + 6L, b + 2L);
        cb->on_triangle(cb->user, object_index, -1L, b + 2L, b + 6L, b + 7L);
        cb->on_triangle(cb->user, object_index, -1L, b + 2L, b + 7L, b + 3L);
        cb->on_triangle(cb->user, object_index, -1L, b + 3L, b + 7L, b + 4L);
        cb->on_triangle(cb->user, object_index, -1L, b + 3L, b + 4L, b + 0L);
    }
    *vertex_base = b + 8L;
}

int scn3d_decode_simple_volume(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb)
{
    char line[256];
    long w;
    long h;
    long d;
    long bpv;
    long bps;
    long x;
    long y;
    long z;
    long emitted_voxels;
    long vertex_base;
    scn3d_fix sx;
    scn3d_fix sy;
    scn3d_fix sz;

    if (!io || !opt) return SCN3D_ERR_BAD_ARG;
    if (!scn3d_seek_abs(io, 0)) return SCN3D_ERR_IO;

    if (!vol_read_line(io, line, (int)sizeof(line))) return SCN3D_ERR_IO;
    if (strncmp(line, "SCN", 3) != 0) return SCN3D_ERR_BAD_FORMAT;

    if (!vol_read_line(io, line, (int)sizeof(line))) return SCN3D_ERR_IO;
    if (!vol_parse_long3(line, &w, &h, &d)) return SCN3D_ERR_BAD_FORMAT;
    if (w <= 0 || h <= 0 || d <= 0) return SCN3D_ERR_BAD_FORMAT;

    if (!vol_read_line(io, line, (int)sizeof(line))) return SCN3D_ERR_IO;
    if (!vol_parse_fix3(line, &sx, &sy, &sz)) return SCN3D_ERR_BAD_FORMAT;

    if (!vol_read_line(io, line, (int)sizeof(line))) return SCN3D_ERR_IO;
    {
        const char *s;
        s = line;
        if (!vol_parse_long(&s, &bpv)) return SCN3D_ERR_BAD_FORMAT;
    }
    if (bpv <= 0 || (bpv % 8L) != 0) return SCN3D_ERR_BAD_FORMAT;
    bps = bpv / 8L;
    if (bps > 4) return SCN3D_ERR_UNSUPPORTED;

    if (cb && cb->on_format) {
        cb->on_format(cb->user, SCN3D_DIALECT_SIMPLE_VOLUME, "SCN simple 3D grayscale volume");
    }
    if (cb && cb->on_volume_info) {
        cb->on_volume_info(cb->user, w, h, d, sx, sy, sz, bpv);
    }
    if (opt->emit_volume_cubes && cb && cb->on_object_begin) {
        cb->on_object_begin(cb->user, 0L, 0L, -1L, "VOX ");
    }

    emitted_voxels = 0;
    vertex_base = 0;
    for (z = 0; z < d; ++z) {
        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                unsigned long sample;
                if (!vol_read_sample(io, bps, &sample)) return SCN3D_ERR_IO;
                if (sample >= (unsigned long)opt->volume_threshold) {
                    scn3d_fix wx;
                    scn3d_fix wy;
                    scn3d_fix wz;
                    if (opt->max_volume_voxels > 0 && emitted_voxels >= opt->max_volume_voxels) {
                        scn3d_emit_warning(cb, SCN3D_WARN_VOLUME_LIMIT, "occupied voxel emission stopped by max_volume_voxels.");
                        continue;
                    }
                    wx = scn3d_fix_mul(scn3d_fix_from_int(x), sx);
                    wy = scn3d_fix_mul(scn3d_fix_from_int(y), sy);
                    wz = scn3d_fix_mul(scn3d_fix_from_int(z), sz);
                    if (opt->emit_volume_voxels && cb && cb->on_voxel) {
                        cb->on_voxel(cb->user, x, y, z, sample, wx, wy, wz);
                    }
                    if (opt->emit_volume_cubes) {
                        vol_emit_cube(cb, 0L, &vertex_base, wx, wy, wz, sx, sy, sz);
                    }
                    ++emitted_voxels;
                }
            }
        }
    }

    if (opt->emit_volume_cubes && cb && cb->on_object_end) {
        cb->on_object_end(cb->user, 0L);
    }

    return SCN3D_OK;
}
