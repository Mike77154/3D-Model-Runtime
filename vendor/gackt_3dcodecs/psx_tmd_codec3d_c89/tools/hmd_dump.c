#include <stdio.h>
#include "tmd_hmd3d.h"

#define HMD_DUMP_MAX_BLOCKS 256
#define HMD_DUMP_MAX_FILE_BYTES (8UL * 1024UL * 1024UL)

static unsigned char g_hmd_dump_bytes[HMD_DUMP_MAX_FILE_BYTES];

static unsigned char *read_file(const char *path, unsigned long *out_size)
{
    FILE *f;
    long sz;
    if (path == NULL || out_size == NULL) return NULL;
    f = fopen(path, "rb");
    if (f == NULL) return NULL;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0L || (unsigned long)sz > HMD_DUMP_MAX_FILE_BYTES) { fclose(f); return NULL; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return NULL; }
    if (sz > 0L && fread(g_hmd_dump_bytes, 1U, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (unsigned long)sz;
    return g_hmd_dump_bytes;
}

int main(int argc, char **argv)
{
    unsigned char *bytes;
    unsigned long size;
    TMD_HMD_Context ctx;
    TMD_HMD_Block blocks[HMD_DUMP_MAX_BLOCKS];
    tmd_u32 i;
    tmd_u32 off;
    tmd_u32 len;
    int r;

    if (argc < 2) {
        printf("usage: hmd_dump file.hmd\n");
        return 1;
    }

    bytes = read_file(argv[1], &size);
    if (bytes == NULL) {
        printf("error: could not read file or file exceeds %lu bytes\n", (unsigned long)HMD_DUMP_MAX_FILE_BYTES);
        return 1;
    }

    r = tmd_hmd_parse(&ctx, bytes, (tmd_u32)size, blocks, HMD_DUMP_MAX_BLOCKS);
    if (r != TMD_ERR_OK) {
        printf("error: %s (%d)\n", tmd_result_string(r), r);
        return 1;
    }

    printf("HMD id=0x%08lx map=%lu prim_header_raw=%lu prim_header_off=%lu blocks=%lu warnings=0x%08lx\n",
           (unsigned long)ctx.header.id,
           (unsigned long)ctx.header.map_flag,
           (unsigned long)ctx.header.primitive_header_top_raw,
           (unsigned long)ctx.header.primitive_header_offset,
           (unsigned long)ctx.header.block_count,
           (unsigned long)ctx.warnings);

    for (i = 0UL; i < ctx.parsed_block_count; ++i) {
        off = 0UL;
        len = 0UL;
        r = tmd_hmd_block_range(&ctx, i, &off, &len);
        if (r != TMD_ERR_OK) len = 0UL;
        printf("block[%lu] raw=%lu off=%lu size=%lu type=0x%08lx dev=%u cat=%u(%s) drv=%u code=0x%04x kind=%s\n",
               (unsigned long)i,
               (unsigned long)blocks[i].raw_pointer,
               (unsigned long)blocks[i].block_offset,
               (unsigned long)len,
               (unsigned long)blocks[i].primitive_type_word,
               (unsigned int)blocks[i].developer_id,
               (unsigned int)blocks[i].category,
               tmd_hmd_category_string(blocks[i].category),
               (unsigned int)blocks[i].driver,
               (unsigned int)blocks[i].primitive_code,
               tmd_hmd_kind_string(blocks[i].kind));
    }

    return 0;
}
