#include "q3d_codec.h"
#include <stdio.h>

#ifndef Q3D_TOOL_INPUT_MAX
#define Q3D_TOOL_INPUT_MAX (16u * 1024u * 1024u)
#endif

static unsigned char file_bytes[Q3D_TOOL_INPUT_MAX];
static Q3D_Model model;
static Q3D_Diag diag;

static int file_write(void *user, const char *text, unsigned long bytes)
{
    FILE *f;
    f = (FILE *)user;
    return fwrite(text, 1u, bytes, f) == bytes;
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned long n;
    int c;
    int rc;

    if (argc < 2) {
        fprintf(stderr, "usage: q3d2obj input.q3o > output.obj\n");
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (f == (FILE *)0) {
        fprintf(stderr, "could not open %s\n", argv[1]);
        return 2;
    }

    n = 0u;
    for (;;) {
        c = fgetc(f);
        if (c == EOF) break;
        if (n >= (unsigned long)sizeof(file_bytes)) {
            fclose(f);
            fprintf(stderr, "file too large for static q3d2obj buffer\n");
            return 3;
        }
        file_bytes[n++] = (unsigned char)c;
    }
    fclose(f);

    rc = q3d_parse_buffer(&model, &diag, file_bytes, n);
    if (rc != Q3D_OK) {
        fprintf(stderr, "parse failed: rc=%d offset=%lu chunk=%c msg=%s\n",
                rc, diag.offset, diag.chunk ? diag.chunk : '?', diag.message);
        return 4;
    }

    rc = q3d_emit_obj(&model, file_write, stdout);
    if (rc != Q3D_OK) {
        fprintf(stderr, "obj emit failed: rc=%d\n", rc);
        return 5;
    }
    return 0;
}
