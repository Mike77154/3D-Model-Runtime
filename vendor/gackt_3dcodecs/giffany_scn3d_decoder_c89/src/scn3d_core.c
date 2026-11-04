#include <string.h>
#include "scn3d.h"
#include "scn3d_internal.h"

void scn3d_default_options(scn3d_options *opt)
{
    if (!opt) return;
    opt->volume_threshold = 1;
    opt->emit_volume_voxels = 1;
    opt->emit_volume_cubes = 0;
    opt->max_volume_voxels = 0;
    opt->triangulate_faces = 1;
    opt->apply_truespace_position = 0;
    opt->max_chunks = 250000L;
}

void scn3d_emit_warning(scn3d_callbacks *cb, int code, const char *message)
{
    if (cb && cb->on_warning) {
        cb->on_warning(cb->user, code, message);
    }
}

static int scn3d_starts_at(const unsigned char *b, int n, int pos, const char *s)
{
    int i;
    if (pos < 0 || pos >= n) return 0;
    for (i = 0; s[i]; ++i) {
        if (pos + i >= n) return 0;
        if (b[pos + i] != (unsigned char)s[i]) return 0;
    }
    return 1;
}


static int scn3d_contains_text(const unsigned char *b, int n, const char *s)
{
    int i;
    int j;
    if (!s || !s[0]) return 1;
    for (i = 0; i < n; ++i) {
        j = 0;
        while (s[j] && i + j < n && b[i + j] == (unsigned char)s[j]) {
            ++j;
        }
        if (!s[j]) return 1;
    }
    return 0;
}

static int scn3d_find_prefix(const unsigned char *b, int n, const char *s)
{
    int pos;
    for (pos = 0; pos < n; ++pos) {
        if (b[pos] == ' ' || b[pos] == '\t' || b[pos] == '\r' || b[pos] == '\n') {
            continue;
        }
        if (pos + 3 <= n && b[pos] == 0xef && b[pos + 1] == 0xbb && b[pos + 2] == 0xbf) {
            pos += 2;
            continue;
        }
        return scn3d_starts_at(b, n, pos, s);
    }
    return 0;
}

int scn3d_detect(scn3d_io *io)
{
    unsigned char b[128];
    long got;

    if (!io || !io->seek || !io->read) {
        return SCN3D_DIALECT_UNKNOWN;
    }
    if (!scn3d_seek_abs(io, 0)) {
        return SCN3D_DIALECT_UNKNOWN;
    }
    got = scn3d_read_bytes(io, b, (long)sizeof(b));
    scn3d_seek_abs(io, 0);

    if (got >= 18 && memcmp(b, "Caligari ", 9) == 0) {
        if (b[15] == 'A') return SCN3D_DIALECT_TRUESPACE_ASCII;
        return SCN3D_DIALECT_TRUESPACE_BIN;
    }
    if (got >= 4 && scn3d_starts_at(b, (int)got, 0, "SCN\n")) {
        return SCN3D_DIALECT_SIMPLE_VOLUME;
    }
    if (got >= 5 && b[0] == 'S' && b[1] == 'C' && b[2] == 'N' && (b[3] == '\r' || b[3] == ' ' || b[3] == '\t')) {
        return SCN3D_DIALECT_SIMPLE_VOLUME;
    }
    if (got >= 8 && memcmp(b, "bplist00", 8) == 0) {
        return SCN3D_DIALECT_APPLE_SCENEKIT;
    }
    if (got >= 6 && scn3d_find_prefix(b, (int)got, "<?xml")) {
        if (scn3d_contains_text(b, (int)got, "<plist") ||
            scn3d_contains_text(b, (int)got, "NSKeyedArchiver") ||
            scn3d_contains_text(b, (int)got, "$archiver")) {
            return SCN3D_DIALECT_APPLE_SCENEKIT;
        }
    }
    if (got >= 9 && scn3d_find_prefix(b, (int)got, "[gd_scene")) {
        return SCN3D_DIALECT_GODOT_TEXT;
    }
    if (got >= 9 && scn3d_find_prefix(b, (int)got, "[gd_resource")) {
        return SCN3D_DIALECT_GODOT_TEXT;
    }
    if (got >= 7 && scn3d_starts_at(b, (int)got, 0, "RSRC\0\0")) {
        return SCN3D_DIALECT_GODOT_BINARY;
    }
    return SCN3D_DIALECT_UNKNOWN;
}

int scn3d_decode(scn3d_io *io, const scn3d_options *opt, scn3d_callbacks *cb)
{
    int dialect;
    scn3d_options local_opt;

    if (!io) return SCN3D_ERR_BAD_ARG;
    if (!opt) {
        scn3d_default_options(&local_opt);
        opt = &local_opt;
    }

    dialect = scn3d_detect(io);
    if (!scn3d_seek_abs(io, 0)) return SCN3D_ERR_IO;

    if (dialect == SCN3D_DIALECT_TRUESPACE_BIN) {
        return scn3d_decode_truespace_binary(io, opt, cb);
    }
    if (dialect == SCN3D_DIALECT_SIMPLE_VOLUME) {
        return scn3d_decode_simple_volume(io, opt, cb);
    }
    if (dialect == SCN3D_DIALECT_TRUESPACE_ASCII) {
        if (cb && cb->on_format) cb->on_format(cb->user, dialect, "Caligari trueSpace ASCII .scn/.cob");
        scn3d_emit_warning(cb, SCN3D_WARN_ASCII_TS, "Caligari ASCII .scn/.cob detected; binary mode implemented in this decoder.");
        return SCN3D_ERR_UNSUPPORTED;
    }
    if (dialect == SCN3D_DIALECT_APPLE_SCENEKIT) {
        if (cb && cb->on_format) cb->on_format(cb->user, dialect, "Apple SceneKit .scn archive");
        scn3d_emit_warning(cb, SCN3D_WARN_UNSUPPORTED, "Apple SceneKit .scn archives are detected, not decoded by this no-heap C89 module.");
        return SCN3D_ERR_UNSUPPORTED;
    }
    if (dialect == SCN3D_DIALECT_GODOT_BINARY) {
        if (cb && cb->on_format) cb->on_format(cb->user, dialect, "Godot binary .scn");
        scn3d_emit_warning(cb, SCN3D_WARN_UNSUPPORTED, "Godot binary .scn resources are detected, not decoded; use .tscn/.escn text exports.");
        return SCN3D_ERR_UNSUPPORTED;
    }
    if (dialect == SCN3D_DIALECT_GODOT_TEXT) {
        if (cb && cb->on_format) cb->on_format(cb->user, dialect, "Godot TSCN/ESCN text scene");
        scn3d_emit_warning(cb, SCN3D_WARN_TEXT_GODOT, "Godot text scene detected. Keep it as a separate text-resource pipeline, not as Caligari/volume SCN.");
        return SCN3D_ERR_UNSUPPORTED;
    }
    return SCN3D_ERR_BAD_FORMAT;
}
