/* PS2 TPL header and RGBA decoder smoke test, caller-owned buffers only.
   Run on a TPL file extracted from a GCAX using re4_gca_catalog. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#define SOURCE_CAP (16UL * 1024UL * 1024UL)
#define PIXEL_CAP (4UL * 1024UL * 1024UL)
static re4_u8 source_mem[SOURCE_CAP];
static re4_u8 arena_mem[2UL * 1024UL * 1024UL];
static re4_u8 rgba[PIXEL_CAP];
int main(int argc, char **argv)
{
    FILE *file;
    re4_span src;
    re4_arena arena;
    re4_ps2_tpl tpl;
    re4_u32 i, ok, unsupported, errors;
    re4_err er;
    size_t n;
    if (argc != 2) {
        fprintf(stderr,"usage: re4_ps2tpl_smoke file.tpl\n");
        return 1;
    }
    file = fopen(argv[1],"rb");
    if (file == 0) return 2;
    n = fread(source_mem,1U,sizeof(source_mem),file);
    if (ferror(file)) { fclose(file); return 3; }
    fclose(file);
    src.data = source_mem;
    src.size = (re4_size)n;
    re4_arena_init(&arena,arena_mem,sizeof(arena_mem));
    er = re4_ps2_tpl_read(src, RE4_ENDIAN_LITTLE, &arena, &tpl);
    if (er != RE4_OK) {
        printf("TPL parser: %s\n",re4_err_name(er));
        return 4;
    }
    ok = 0U;
    unsupported = 0U;
    errors = 0U;
    for (i = 0U; i < tpl.image_count; ++i) {
        const re4_ps2_tpl_image_header *header = &tpl.images[i].main_header;
        er = re4_ps2_tpl_decode_image_rgba32(src, &tpl, i, rgba,
              sizeof(rgba), (re4_u32)header->width * 4U);
        if (er == RE4_OK) ++ok;
        else if (er == RE4_ERR_UNSUPPORTED) ++unsupported;
        else {
            ++errors;
            printf("Image %u: %s\n",(unsigned)i,re4_err_name(er));
        }
    }
    printf("images=%u decoded=%u unsupported=%u errors=%u\n",
           (unsigned)tpl.image_count, (unsigned)ok,
           (unsigned)unsupported, (unsigned)errors);
    return errors ? 5 : 0;
}
