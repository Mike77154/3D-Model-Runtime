#include "cob_private.h"

int cob_decode_memory(cob_scene *scene, const void *bytes, unsigned long count)
{
    const unsigned char *p;
    char mode;
    int little;
    unsigned long start;

    if (scene == 0 || bytes == 0 || count < 18UL) {
        return COB_ERR_ARGUMENT;
    }
    cob_scene_clear(scene);
    p = (const unsigned char *)bytes;
    if (count < 32UL) {
        return COB_ERR_TRUNCATED;
    }
    if (memcmp(p, "Caligari ", 9) != 0) {
        return COB_ERR_BAD_MAGIC;
    }
    cob_copy_name(scene->header_version, sizeof(scene->header_version), (const char *)(p + 9), 6UL);
    mode = (char)p[15];
    little = (p[16] == 'L' && p[17] == 'H') ? 1 : 0;
    scene->little_endian = little;
    if (!little) {
        return COB_ERR_UNSUPPORTED_ENDIAN;
    }
    start = 32UL;
    if (mode == 'A') {
        scene->source_format = COB_FORMAT_ASCII;
        return cob_decode_ascii(scene, bytes, count, start);
    }
    if (mode == 'B') {
        scene->source_format = COB_FORMAT_BINARY;
        return cob_decode_binary(scene, bytes, count, start);
    }
    return COB_ERR_UNSUPPORTED_FORMAT;
}
