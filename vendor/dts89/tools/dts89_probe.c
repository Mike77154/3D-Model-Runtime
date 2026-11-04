#include "dts89.h"
#include <stdio.h>
#include <string.h>

static unsigned char file_buffer[64u * 1024u * 1024u];

static const char *yesno(dts89_u32 v)
{
    return v ? "yes" : "no";
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    size_t got;
    dts89_probe_result pr;
    int rc;

    if (argc != 2) {
        fprintf(stderr, "usage: dts89_probe file\n");
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "open failed\n");
        return 2;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "seek failed\n");
        return 2;
    }
    n = ftell(f);
    if (n < 0 || (unsigned long)n > (unsigned long)sizeof(file_buffer)) {
        fclose(f);
        fprintf(stderr, "file too large for static buffer\n");
        return 2;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        fprintf(stderr, "seek failed\n");
        return 2;
    }
    got = fread(file_buffer, 1u, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) {
        fprintf(stderr, "read failed\n");
        return 2;
    }

    memset(&pr, 0, sizeof(pr));
    rc = dts89_probe(file_buffer, (dts89_u32)got, &pr);
    printf("kind=%s\n", dts89_probe_kind_name(pr.kind));
    printf("rc=%d\n", rc);
    printf("error_code=%d\n", pr.error_code);
    printf("confidence=%u\n", (unsigned)pr.confidence);
    printf("version=%u\n", (unsigned)pr.version);
    printf("exporter_version=%u\n", (unsigned)pr.exporter_version);
    printf("supported=%s\n", yesno(pr.flags & DTS89_PROBE_FLAG_SUPPORTED));
    printf("binary=%s\n", yesno(pr.flags & DTS89_PROBE_FLAG_BINARY));
    printf("text=%s\n", yesno(pr.flags & DTS89_PROBE_FLAG_TEXT));
    printf("animation_only=%s\n", yesno(pr.flags & DTS89_PROBE_FLAG_ANIMATION_ONLY));
    printf("num_nodes=%ld\n", (long)pr.num_nodes);
    printf("num_objects=%ld\n", (long)pr.num_objects);
    printf("num_subshapes=%ld\n", (long)pr.num_subshapes);
    printf("num_meshes=%ld\n", (long)pr.num_meshes);
    printf("num_names=%ld\n", (long)pr.num_names);
    printf("num_sequences=%ld\n", (long)pr.num_sequences);
    return rc == DTS89_OK ? 0 : 1;
}
