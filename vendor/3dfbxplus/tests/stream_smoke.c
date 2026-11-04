#include <stdio.h>
#include "3dfbx.h"

typedef struct StreamSmokeTag {
    FILE *fp;
    size_t chunk;
} StreamSmoke;

static size_t smoke_read(void *user, void *dst, size_t size)
{
    StreamSmoke *s;
    s = (StreamSmoke*)user;
    if (!s || !s->fp) return 0u;
    if (size > s->chunk) size = s->chunk;
    return fread(dst, 1u, size, s->fp);
}

static fbx_i64 file_size(FILE *fp)
{
    long here;
    long end;
    here = ftell(fp);
    if (here < 0) return (fbx_i64)-1;
    if (fseek(fp, 0, SEEK_END) != 0) return (fbx_i64)-1;
    end = ftell(fp);
    (void)fseek(fp, here, SEEK_SET);
    if (end < 0) return (fbx_i64)-1;
    return (fbx_i64)end;
}

int main(int argc, char **argv)
{
    StreamSmoke ss;
    FbxCodecStreamSource src;
    FbxCodecDecodeResult out;
    FbxStreamStats stats;
    fbx_i64 size;
    int scan_ok;

    if (argc < 2) {
        printf("Usage: %s <input.fbx> [chunk]\n", argv[0]);
        return 1;
    }

    ss.fp = fopen(argv[1], "rb");
    ss.chunk = 7u;
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

    size = file_size(ss.fp);
    src.user = &ss;
    src.read = smoke_read;
    src.known_size = size;
    scan_ok = fbx_stream_scan_binary(&src, NULL, &stats);
    (void)fseek(ss.fp, 0, SEEK_SET);

    src.known_size = (fbx_i64)-1;
    if (!fbx_codec_decode_stream(&src, 1, &out)) {
        printf("stream decode failed: %s\n", out.error_message ? out.error_message : "error");
        fclose(ss.fp);
        return 3;
    }

    printf("stream OK: binary=%d version=%d models=%d scan=%s nodes=%ld props=%ld arrays=%ld compressed=%ld max_depth=%d\n",
           out.is_binary,
           out.version,
           out.scene ? out.scene->model_count : 0,
           scan_ok ? "yes" : "skip",
           (long)stats.nodes,
           (long)stats.properties,
           (long)stats.arrays,
           (long)stats.compressed_arrays,
           stats.max_depth);
    fbx_codec_decode_close(&out);
    fclose(ss.fp);
    return 0;
}
