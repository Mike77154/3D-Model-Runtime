#include "lws3d_lwo.h"

#ifndef LWS3D_NO_STDIO
#include <stdio.h>

int lws3d_lwo_load_file_static(LWS3D_LWO_Object *obj, const char *filename, unsigned char *scratch, unsigned long scratch_size, LWS3D_LWO_Stats *stats)
{
    FILE *fp;
    unsigned long got;
    int ch;
    if (!obj || !filename || !scratch || scratch_size == 0UL) {
        return LWS3D_LWO_ERR_BAD_ARG;
    }
    fp = fopen(filename, "rb");
    if (!fp) {
        return LWS3D_LWO_ERR_BAD_ARG;
    }
    got = (unsigned long)fread(scratch, 1, (size_t)scratch_size, fp);
    ch = fgetc(fp);
    fclose(fp);
    if (ch != EOF) {
        if (stats) {
            ++stats->truncated_chunks;
        }
        return LWS3D_LWO_ERR_TRUNCATED;
    }
    return lws3d_lwo_parse_buffer(obj, scratch, got, stats);
}
#endif
