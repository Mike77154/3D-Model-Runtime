#include <stdio.h>
#include <string.h>
#include "tmd_hmd3d.h"
#include "tmd_hmd_cat3.h"

#define CAT3_DUMP_MAX_BLOCKS 256
#define CAT3_DUMP_MAX_PRIMS 512
#define CAT3_DUMP_MAX_SECTIONS 512
#define CAT3_DUMP_MAX_FILE_BYTES (8UL * 1024UL * 1024UL)

static unsigned char g_bytes[CAT3_DUMP_MAX_FILE_BYTES];

static unsigned char *read_file(const char *path, unsigned long *out_size)
{
    FILE *f;
    long sz;
    if (path == NULL || out_size == NULL) return NULL;
    f = fopen(path, "rb");
    if (f == NULL) return NULL;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0L || (unsigned long)sz > CAT3_DUMP_MAX_FILE_BYTES) { fclose(f); return NULL; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return NULL; }
    if (sz > 0L && fread(g_bytes, 1U, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (unsigned long)sz;
    return g_bytes;
}

static unsigned long parse_ulong(const char *s)
{
    unsigned long v;
    if (s == NULL) return 0UL;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        sscanf(s + 2, "%lx", &v);
    } else {
        sscanf(s, "%lu", &v);
    }
    return v;
}

static void dump_desc(const TMD_HMD_Cat3_Descriptor *d, unsigned long idx)
{
    printf("desc[%lu] raw=0x%08lx kind=%s",
           idx,
           (unsigned long)d->raw,
           tmd_hmd_cat3_descriptor_string(d->kind));
    if (d->kind == TMD_HMD_CAT3_DESC_NORMAL) {
        printf(" type_idx=%u tframe=%u param=%u",
               (unsigned int)d->type_index,
               (unsigned int)d->tframe,
               (unsigned int)d->parameter_index);
    } else if (d->kind == TMD_HMD_CAT3_DESC_JUMP) {
        printf(" dst_sid=%u cnd_sid=%u seq_idx=%u",
               (unsigned int)d->stream_dst,
               (unsigned int)d->stream_condition,
               (unsigned int)d->sequence_index);
    } else if (d->kind == TMD_HMD_CAT3_DESC_CONTROL) {
        printf(" code=%u p2=%u p1=%u",
               (unsigned int)d->control_code,
               (unsigned int)d->p2,
               (unsigned int)d->p1);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    unsigned char *bytes;
    unsigned long size;
    TMD_HMD_Context ctx;
    TMD_HMD_Block blocks[CAT3_DUMP_MAX_BLOCKS];
    TMD_HMD_Cat3_Primitive prims[CAT3_DUMP_MAX_PRIMS];
    TMD_HMD_Cat3_DataSection sections[CAT3_DUMP_MAX_SECTIONS];
    TMD_HMD_Cat3_Descriptor desc;
    tmd_u32 i;
    tmd_u32 j;
    tmd_u32 prim_count;
    tmd_u32 cat3_count;
    tmd_u32 warnings;
    unsigned long control_off;
    unsigned long control_count;
    int r;

    if (argc < 2) {
        printf("usage: hmd_cat3_dump file.hmd [--control offset count]\n");
        return 1;
    }

    bytes = read_file(argv[1], &size);
    if (bytes == NULL) {
        printf("error: could not read file or file exceeds %lu bytes\n", (unsigned long)CAT3_DUMP_MAX_FILE_BYTES);
        return 1;
    }

    r = tmd_hmd_parse(&ctx, bytes, (tmd_u32)size, blocks, CAT3_DUMP_MAX_BLOCKS);
    if (r != TMD_ERR_OK) {
        printf("error: %s (%d)\n", tmd_result_string(r), r);
        return 1;
    }

    printf("HMD Category 3 scan: blocks=%lu warnings=0x%08lx\n",
           (unsigned long)ctx.parsed_block_count,
           (unsigned long)ctx.warnings);

    for (i = 0UL; i < ctx.parsed_block_count; ++i) {
        r = tmd_hmd_cat3_scan_block(&ctx, i, prims, CAT3_DUMP_MAX_PRIMS,
                                    sections, CAT3_DUMP_MAX_SECTIONS,
                                    &prim_count, &cat3_count, &warnings);
        if (r != TMD_ERR_OK) {
            printf("block[%lu] cat3_scan_error=%s (%d)\n", (unsigned long)i, tmd_result_string(r), r);
            continue;
        }
        printf("block[%lu] primitives=%lu cat3_sections=%lu warnings=0x%08lx\n",
               (unsigned long)i,
               (unsigned long)prim_count,
               (unsigned long)cat3_count,
               (unsigned long)warnings);
        for (j = 0UL; j < cat3_count; ++j) {
            printf("  cat3[%lu] prim_off=%lu sec_off=%lu data_off=%lu data_bytes=%lu type=0x%08lx target=%u rot=%u scale=%u trans=%u order=%u general_len=%u write=0x%x alg=%u\n",
                   (unsigned long)j,
                   (unsigned long)sections[j].primitive_offset,
                   (unsigned long)sections[j].section_offset,
                   (unsigned long)sections[j].data_offset,
                   (unsigned long)sections[j].data_bytes,
                   (unsigned long)sections[j].type_info.raw_type,
                   (unsigned int)sections[j].type_info.target,
                   (unsigned int)sections[j].type_info.rot_intr,
                   (unsigned int)sections[j].type_info.scale_intr,
                   (unsigned int)sections[j].type_info.trans_intr,
                   (unsigned int)sections[j].type_info.rot_order,
                   (unsigned int)sections[j].type_info.general_length,
                   (unsigned int)sections[j].type_info.general_write_mask,
                   (unsigned int)sections[j].type_info.general_intr_alg);
        }
    }

    if (argc == 5 && strcmp(argv[2], "--control") == 0) {
        control_off = parse_ulong(argv[3]);
        control_count = parse_ulong(argv[4]);
        printf("Sequence control dump at offset=%lu count=%lu\n", control_off, control_count);
        for (i = 0UL; i < (tmd_u32)control_count; ++i) {
            r = tmd_hmd_cat3_read_descriptor(&ctx, (tmd_u32)control_off, i, &desc);
            if (r != TMD_ERR_OK) {
                printf("desc[%lu] error=%s (%d)\n", (unsigned long)i, tmd_result_string(r), r);
                break;
            }
            dump_desc(&desc, (unsigned long)i);
        }
    }

    return 0;
}
