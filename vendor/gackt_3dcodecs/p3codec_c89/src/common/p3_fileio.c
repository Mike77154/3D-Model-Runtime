#include <stdio.h>
#include "p3_error.h"
#include "p3_fileio.h"

int p3_file_read_all(const char *path, p3_u8 *dst, p3_u32 cap, p3_u32 *out_size) {
    FILE *fp;
    long size_l;
    size_t got;

    if (path == 0 || dst == 0 || out_size == 0) {
        return P3_ERR_INVALID;
    }

    fp = fopen(path, "rb");
    if (fp == 0) {
        return P3_ERR_IO;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return P3_ERR_IO;
    }
    size_l = ftell(fp);
    if (size_l < 0) {
        fclose(fp);
        return P3_ERR_IO;
    }
    if ((p3_u32)size_l > cap) {
        fclose(fp);
        return P3_ERR_CAPACITY;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return P3_ERR_IO;
    }

    got = fread(dst, 1, (size_t)size_l, fp);
    fclose(fp);

    if (got != (size_t)size_l) {
        return P3_ERR_IO;
    }

    *out_size = (p3_u32)got;
    return P3_OK;
}

int p3_file_write_all(const char *path, const p3_u8 *src, p3_u32 size) {
    FILE *fp;
    size_t wrote;

    if (path == 0 || src == 0) {
        return P3_ERR_INVALID;
    }

    fp = fopen(path, "wb");
    if (fp == 0) {
        return P3_ERR_IO;
    }

    wrote = fwrite(src, 1, (size_t)size, fp);
    fclose(fp);

    if (wrote != (size_t)size) {
        return P3_ERR_IO;
    }

    return P3_OK;
}
