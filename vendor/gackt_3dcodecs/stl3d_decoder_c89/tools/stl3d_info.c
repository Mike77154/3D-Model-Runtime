#include <stdio.h>
#include <string.h>
#include "stl3d_decoder.h"

typedef struct file_reader_s {
    FILE *fp;
} file_reader;

static int file_read_cb(void *user, stl3d_u8 *dst, unsigned long max_bytes, unsigned long *got_bytes)
{
    file_reader *fr;
    fr = (file_reader *)user;
    if (!fr || !fr->fp || !dst || !got_bytes) return STL3D_ERR_ARG;
    *got_bytes = (unsigned long)fread(dst, 1U, (size_t)max_bytes, fr->fp);
    if (ferror(fr->fp)) return STL3D_ERR_IO;
    return STL3D_OK;
}

static void print_fixed(const char *name, stl3d_fixed v)
{
    char s[48];
    stl3d_fixed_to_ascii(v, s, sizeof(s), 5U);
    printf("%s%s", name, s);
}

static void print_vec3(const char *name, const stl3d_vec3 *v)
{
    printf("%s(", name);
    print_fixed("", v->x);
    printf(", ");
    print_fixed("", v->y);
    printf(", ");
    print_fixed("", v->z);
    printf(")\n");
}

static void print_color(const char *name, const stl3d_color *c)
{
    if (!c || !c->valid) return;
    printf("%srgba(%u,%u,%u,%u) kind=%u\n", name,
           (unsigned int)c->r, (unsigned int)c->g, (unsigned int)c->b,
           (unsigned int)c->a, (unsigned int)c->kind);
}

static void usage(void)
{
    printf("usage: stl3d_info [--ascii|--binary] [--no-validate] file.stl\n");
}

int main(int argc, char **argv)
{
    static stl3d_u8 work[4096];
    stl3d_ctx ctx;
    stl3d_stats stats;
    stl3d_color obj_color;
    file_reader fr;
    FILE *fp;
    long size;
    int rc;
    int fmt;
    int argi;
    int forced_format;
    int validate;
    const char *path;

    if (argc < 2) {
        usage();
        return 2;
    }

    forced_format = STL3D_FORMAT_UNKNOWN;
    validate = 1;
    path = 0;
    argi = 1;
    while (argi < argc) {
        if (strcmp(argv[argi], "--ascii") == 0) {
            forced_format = STL3D_FORMAT_ASCII;
        } else if (strcmp(argv[argi], "--binary") == 0) {
            forced_format = STL3D_FORMAT_BINARY;
        } else if (strcmp(argv[argi], "--no-validate") == 0) {
            validate = 0;
        } else {
            path = argv[argi];
        }
        ++argi;
    }

    if (!path) {
        usage();
        return 2;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        printf("could not open: %s\n", path);
        return 2;
    }

    size = 0L;
    if (fseek(fp, 0L, SEEK_END) == 0) {
        size = ftell(fp);
        (void)fseek(fp, 0L, SEEK_SET);
    }

    fr.fp = fp;
    rc = stl3d_init(&ctx, file_read_cb, &fr, work, sizeof(work));
    if (rc != STL3D_OK) {
        printf("init failed: %s\n", stl3d_error_string(rc));
        fclose(fp);
        return 1;
    }
    if (size > 0L) stl3d_set_total_size(&ctx, (stl3d_u32)size);
    stl3d_set_validation(&ctx, validate);
    if (forced_format != STL3D_FORMAT_UNKNOWN) {
        rc = stl3d_force_format(&ctx, forced_format);
        if (rc != STL3D_OK) {
            printf("force format failed: %s\n", stl3d_error_string(rc));
            fclose(fp);
            return 1;
        }
    }

    rc = stl3d_detect_format(&ctx, &fmt);
    if (rc != STL3D_OK) {
        printf("detect failed: %s\n", stl3d_error_string(rc));
        fclose(fp);
        return 1;
    }
    printf("format: %s\n", fmt == STL3D_FORMAT_BINARY ? "binary" : "ascii");
    if (size > 0L) printf("bytes: %ld\n", size);

    rc = stl3d_decode_all(&ctx, 0, 0, &stats);
    fclose(fp);
    if (rc != STL3D_OK) {
        printf("decode failed: %s\n", stl3d_error_string(rc));
        return 1;
    }

    printf("triangles: %lu\n", (unsigned long)stats.triangles);
    if (stats.has_bounds) {
        print_vec3("min: ", &stats.minv);
        print_vec3("max: ", &stats.maxv);
    }

    if (fmt == STL3D_FORMAT_BINARY) {
        rc = stl3d_get_magics_object_color(&ctx, &obj_color);
        if (rc == STL3D_OK) print_color("magics_object_color: ", &obj_color);
    }

    printf("degenerate: %lu\n", (unsigned long)stats.degenerate_triangles);
    printf("zero_normals: %lu\n", (unsigned long)stats.zero_normals);
    printf("normal_flipped: %lu\n", (unsigned long)stats.normal_flipped);
    printf("normal_mismatch: %lu\n", (unsigned long)stats.normal_mismatch);
    printf("nonzero_attributes: %lu\n", (unsigned long)stats.nonzero_attributes);
    printf("non_finite_values: %lu\n", (unsigned long)stats.non_finite_values);
    printf("saturated_values: %lu\n", (unsigned long)stats.saturated_values);
    printf("viscam_color_facets: %lu\n", (unsigned long)stats.viscam_color_facets);
    printf("magics_color_facets: %lu\n", (unsigned long)stats.magics_color_facets);
    return 0;
}
