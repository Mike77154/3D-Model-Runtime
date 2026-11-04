#include "lws3d.h"

#ifndef LWS3D_NO_STDIO
#include <stdio.h>

int lws3d_load_file_static(LWS3D_Scene *scene, const char *filename, char *scratch, unsigned long scratch_size, LWS3D_ParseStats *stats)
{
    FILE *fp;
    unsigned long n;
    size_t got;
    int r;
    if (!scene || !filename || !scratch || scratch_size == 0UL) {
        return LWS3D_ERR_BAD_ARG;
    }
    fp = fopen(filename, "rb");
    if (!fp) {
        return LWS3D_ERR_BAD_ARG;
    }
    got = fread(scratch, 1, (size_t)scratch_size, fp);
    if (ferror(fp)) {
        fclose(fp);
        return LWS3D_ERR_BAD_ARG;
    }
    if (!feof(fp)) {
        fclose(fp);
        return LWS3D_ERR_TRUNCATED;
    }
    fclose(fp);
    n = (unsigned long)got;
    r = lws3d_parse_buffer(scene, scratch, n, stats);
    return r;
}

#else
int lws3d_load_file_static(LWS3D_Scene *scene, const char *filename, char *scratch, unsigned long scratch_size, LWS3D_ParseStats *stats)
{
    (void)scene;
    (void)filename;
    (void)scratch;
    (void)scratch_size;
    (void)stats;
    return LWS3D_ERR_BAD_ARG;
}
#endif
