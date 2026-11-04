#include <stdio.h>
#include "3dfbx.h"

typedef struct ScanSmokeTag {
    FILE *fp;
    size_t chunk;
} ScanSmoke;

static size_t scan_smoke_read(void *user, void *dst, size_t size)
{
    ScanSmoke *s;
    s = (ScanSmoke*)user;
    if (!s || !s->fp) return 0u;
    if (size > s->chunk) size = s->chunk;
    return fread(dst, 1u, size, s->fp);
}

static fbx_i64 scan_file_size(FILE *fp)
{
    long cur;
    long end;
    cur = ftell(fp);
    if (cur < 0) return (fbx_i64)-1;
    if (fseek(fp, 0, SEEK_END) != 0) return (fbx_i64)-1;
    end = ftell(fp);
    if (fseek(fp, cur, SEEK_SET) != 0) return (fbx_i64)-1;
    if (end < 0) return (fbx_i64)-1;
    return (fbx_i64)end;
}

int main(int argc, char **argv)
{
    ScanSmoke ss;
    FbxCodecStreamSource src;
    FbxStreamStats stats;
    fbx_i64 size;

    if (argc < 2) {
        printf("Usage: %s <binary.fbx> [chunk]\n", argv[0]);
        return 1;
    }

    ss.fp = fopen(argv[1], "rb");
    ss.chunk = 11u;
    if (argc >= 3) {
        int c;
        c = 0;
        while (*argv[2] >= '0' && *argv[2] <= '9') {
            c = c * 10 + (*argv[2] - '0');
            argv[2]++;
        }
        if (c > 0) ss.chunk = (size_t)c;
    }
    if (!ss.fp) {
        printf("open failed\n");
        return 2;
    }

    size = scan_file_size(ss.fp);
    if (size <= 0) {
        fclose(ss.fp);
        printf("size failed\n");
        return 3;
    }

    src.user = &ss;
    src.read = scan_smoke_read;
    src.known_size = size;
    if (!fbx_stream_scan_binary(&src, NULL, &stats)) {
        fclose(ss.fp);
        printf("stream scan failed\n");
        return 4;
    }

    printf("stream scan OK: version=%d nodes=%ld props=%ld arrays=%ld compressed=%ld bytes=%ld\n",
           stats.version,
           (long)stats.nodes,
           (long)stats.properties,
           (long)stats.arrays,
           (long)stats.compressed_arrays,
           (long)stats.bytes_read);
    fclose(ss.fp);
    return 0;
}
