#include "x3d_codec.h"
#include <stdio.h>

#define X3D_TOOL_IN_CAP (4 * 1024 * 1024)
#define X3D_TOOL_OUT_CAP (4 * 1024 * 1024)

static char g_in[X3D_TOOL_IN_CAP + 1];
static char g_out[X3D_TOOL_OUT_CAP + 1];
static x3d_scene g_scene;

static int read_file(const char *path, char *buf, int cap, int *len_out)
{
    FILE *f;
    int n;
    int total;
    if (!path || !buf || !len_out) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    total = 0;
    while (total < cap) {
        n = (int)fread(buf + total, 1, (unsigned int)(cap - total), f);
        if (n <= 0) break;
        total += n;
    }
    fclose(f);
    buf[total] = 0;
    *len_out = total;
    return 1;
}

static int write_file(const char *path, const char *buf, int len)
{
    FILE *f;
    int n;
    if (!path || !buf || len < 0) return 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    n = (int)fwrite(buf, 1, (unsigned int)len, f);
    fclose(f);
    return n == len;
}

int main(int argc, char **argv)
{
    int len;
    int out_len;
    int rc;
    x3d_diag diag;
    if (argc < 3) {
        printf("usage: x3d_to_json input.x3d-or-x3dv-or-json output.json\n");
        return 2;
    }
    if (!read_file(argv[1], g_in, X3D_TOOL_IN_CAP, &len)) {
        printf("read failed\n");
        return 1;
    }
    rc = x3d_parse_any(&g_scene, (const unsigned char *)g_in, len, &diag);
    if (rc != X3D_STATUS_OK) {
        printf("parse failed rc=%d line=%d msg=%s\n", rc, diag.line, diag.message);
        return 1;
    }
    rc = x3d_write_json(&g_scene, g_out, X3D_TOOL_OUT_CAP, &out_len);
    if (rc != X3D_STATUS_OK) {
        printf("write failed rc=%d\n", rc);
        return 1;
    }
    if (!write_file(argv[2], g_out, out_len)) {
        printf("write file failed\n");
        return 1;
    }
    printf("wrote %d bytes\n", out_len);
    return 0;
}
