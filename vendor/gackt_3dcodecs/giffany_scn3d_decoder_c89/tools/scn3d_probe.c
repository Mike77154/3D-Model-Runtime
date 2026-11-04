#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scn3d.h"
#include "scn3d_stdio.h"

typedef struct probe_s {
    long objects;
    long vertices;
    long uvs;
    long faces;
    long triangles;
    long voxels;
    long chunks;
    int obj_mode;
    int verbose;
} probe;

static void print_fix(FILE *fp, scn3d_fix v)
{
    char tmp[64];
    scn3d_fix_to_text(v, tmp, (int)sizeof(tmp), 4);
    fputs(tmp, fp);
}

static void cb_format(void *user, int dialect, const char *name)
{
    probe *p;
    p = (probe *)user;
    if (!p || !p->obj_mode) {
        printf("format: %s (%d)\n", name, dialect);
    }
}

static void cb_warning(void *user, int code, const char *msg)
{
    (void)user;
    fprintf(stderr, "warning %d: %s\n", code, msg);
}

static void cb_chunk(void *user, const char *chunk_type, long major, long minor, long chunk_id, long parent_id, long payload_bytes)
{
    probe *p;
    p = (probe *)user;
    p->chunks++;
    if (p->verbose) {
        printf("chunk %ld: %s v%ld.%ld id=%ld parent=%ld bytes=%ld\n",
               p->chunks, chunk_type, major, minor, chunk_id, parent_id, payload_bytes);
    }
}

static void cb_object_begin(void *user, long object_index, long chunk_id, long parent_id, const char *chunk_type)
{
    probe *p;
    p = (probe *)user;
    p->objects++;
    if (!p->obj_mode) {
        printf("object %ld: chunk=%s id=%ld parent=%ld\n", object_index, chunk_type, chunk_id, parent_id);
    } else {
        printf("o scn3d_object_%ld\n", object_index);
    }
}

static void cb_object_name(void *user, long object_index, const char *name)
{
    probe *p;
    p = (probe *)user;
    if (!p->obj_mode && name && name[0]) {
        printf("  name[%ld] = %s\n", object_index, name);
    }
}

static void cb_transform(void *user, long object_index, int transform_kind, const scn3d_fix m12[12])
{
    probe *p;
    int i;
    p = (probe *)user;
    if (p->verbose && !p->obj_mode) {
        printf("  transform[%ld] kind=%d", object_index, transform_kind);
        for (i = 0; i < 12; ++i) {
            fputc(' ', stdout);
            print_fix(stdout, m12[i]);
        }
        fputc('\n', stdout);
    }
}

static void cb_object_end(void *user, long object_index)
{
    (void)user;
    (void)object_index;
}

static void cb_vertex(void *user, long object_index, long vertex_index, scn3d_fix x, scn3d_fix y, scn3d_fix z)
{
    probe *p;
    p = (probe *)user;
    p->vertices++;
    if (p->obj_mode) {
        fputs("v ", stdout);
        print_fix(stdout, x); fputc(' ', stdout);
        print_fix(stdout, y); fputc(' ', stdout);
        print_fix(stdout, z); fputc('\n', stdout);
    } else if (vertex_index < 3) {
        printf("  v[%ld,%ld] = ", object_index, vertex_index);
        print_fix(stdout, x); fputc(' ', stdout);
        print_fix(stdout, y); fputc(' ', stdout);
        print_fix(stdout, z); fputc('\n', stdout);
    }
}

static void cb_uv(void *user, long object_index, long uv_index, scn3d_fix u, scn3d_fix v)
{
    probe *p;
    p = (probe *)user;
    p->uvs++;
    if (p->obj_mode) {
        fputs("vt ", stdout);
        print_fix(stdout, u); fputc(' ', stdout);
        print_fix(stdout, v); fputc('\n', stdout);
    } else {
        (void)object_index;
        (void)uv_index;
        (void)u;
        (void)v;
    }
}

static void cb_face(void *user, long object_index, long material_index, long vertex_count, const long *vertex_indices, const long *uv_indices, int flags)
{
    probe *p;
    (void)object_index;
    (void)material_index;
    (void)vertex_indices;
    (void)uv_indices;
    (void)flags;
    p = (probe *)user;
    p->faces++;
    if (!p->obj_mode && p->verbose) {
        printf("  face verts=%ld mat=%ld flags=%d\n", vertex_count, material_index, flags);
    }
}

static void cb_triangle(void *user, long object_index, long material_index, long a, long b, long c)
{
    probe *p;
    p = (probe *)user;
    p->triangles++;
    if (p->obj_mode) {
        (void)object_index;
        (void)material_index;
        /* OBJ is 1-based. This simple sink assumes one object or duplicated streaming vertices. */
        printf("f %ld %ld %ld\n", a + 1L, b + 1L, c + 1L);
    } else if (p->triangles <= 3) {
        printf("  tri[%ld] mat=%ld: %ld %ld %ld\n", object_index, material_index, a, b, c);
    }
}

static void cb_volume_info(void *user, long w, long h, long d, scn3d_fix sx, scn3d_fix sy, scn3d_fix sz, long bpv)
{
    probe *p;
    p = (probe *)user;
    if (p && p->obj_mode) {
        return;
    }
    printf("volume: %ld x %ld x %ld, bpv=%ld, spacing=", w, h, d, bpv);
    print_fix(stdout, sx); fputc(' ', stdout);
    print_fix(stdout, sy); fputc(' ', stdout);
    print_fix(stdout, sz); fputc('\n', stdout);
}

static void cb_voxel(void *user, long x, long y, long z, unsigned long sample, scn3d_fix wx, scn3d_fix wy, scn3d_fix wz)
{
    probe *p;
    p = (probe *)user;
    p->voxels++;
    if (!p->obj_mode && p->voxels <= 8) {
        printf("  voxel %ld,%ld,%ld sample=%lu world=", x, y, z, sample);
        print_fix(stdout, wx); fputc(' ', stdout);
        print_fix(stdout, wy); fputc(' ', stdout);
        print_fix(stdout, wz); fputc('\n', stdout);
    }
}

static void usage(void)
{
    printf("usage: scn3d_probe [-obj] [-cubes] [-xform] [-v] [-threshold N] [-maxvox N] file.scn\n");
}

int main(int argc, char **argv)
{
    FILE *fp;
    scn3d_io io;
    scn3d_options opt;
    scn3d_callbacks cb;
    probe p;
    const char *path;
    int i;
    int status;

    if (argc < 2) {
        usage();
        return 2;
    }

    path = 0;
    memset(&p, 0, sizeof(p));
    scn3d_default_options(&opt);

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-obj") == 0) {
            p.obj_mode = 1;
            opt.emit_volume_voxels = 0;
        } else if (strcmp(argv[i], "-cubes") == 0) {
            opt.emit_volume_cubes = 1;
        } else if (strcmp(argv[i], "-xform") == 0) {
            opt.apply_truespace_position = 1;
        } else if (strcmp(argv[i], "-v") == 0) {
            p.verbose = 1;
        } else if (strcmp(argv[i], "-threshold") == 0 && i + 1 < argc) {
            ++i;
            opt.volume_threshold = atol(argv[i]);
        } else if (strcmp(argv[i], "-maxvox") == 0 && i + 1 < argc) {
            ++i;
            opt.max_volume_voxels = atol(argv[i]);
        } else {
            path = argv[i];
        }
    }

    if (!path) {
        usage();
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cannot open: %s\n", path);
        return 1;
    }

    scn3d_io_from_FILE(&io, fp);
    memset(&cb, 0, sizeof(cb));
    cb.user = &p;
    cb.on_format = cb_format;
    cb.on_warning = cb_warning;
    cb.on_chunk = cb_chunk;
    cb.on_object_begin = cb_object_begin;
    cb.on_object_name = cb_object_name;
    cb.on_transform = cb_transform;
    cb.on_object_end = cb_object_end;
    cb.on_vertex = cb_vertex;
    cb.on_uv = cb_uv;
    cb.on_face = cb_face;
    cb.on_triangle = cb_triangle;
    cb.on_volume_info = cb_volume_info;
    cb.on_voxel = cb_voxel;

    status = scn3d_decode(&io, &opt, &cb);
    fclose(fp);

    if (!p.obj_mode) {
        printf("summary: chunks=%ld objects=%ld vertices=%ld uvs=%ld faces=%ld triangles=%ld voxels=%ld status=%d\n",
               p.chunks, p.objects, p.vertices, p.uvs, p.faces, p.triangles, p.voxels, status);
    }
    return status == SCN3D_OK ? 0 : 1;
}
