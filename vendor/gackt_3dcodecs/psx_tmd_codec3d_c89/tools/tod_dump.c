#include "tmd_anim3d.h"
#include <stdio.h>

#define TOD_DUMP_MAX_BYTES (8UL * 1024UL * 1024UL)
static unsigned char g_file[TOD_DUMP_MAX_BYTES];

static unsigned long read_whole_file(const char *path)
{
    FILE *f;
    unsigned long n;
    f = fopen(path, "rb");
    if (f == NULL) return 0UL;
    n = (unsigned long)fread(g_file, 1, TOD_DUMP_MAX_BYTES, f);
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    TMD_TOD_Context ctx;
    TMD_TOD_Frame frame;
    TMD_TOD_Packet packet;
    TMD_Transform tr;
    unsigned long size;
    tmd_u32 i;
    tmd_u32 j;
    int r;

    if (argc < 2) {
        printf("usage: tod_dump file.tod\n");
        return 1;
    }

    size = read_whole_file(argv[1]);
    if (size == 0UL) {
        printf("could not read file or file is empty\n");
        return 1;
    }

    r = tmd_tod_parse(&ctx, g_file, (tmd_u32)size);
    if (r != TMD_ERR_OK) {
        printf("parse failed: %s\n", tmd_result_string(r));
        return 1;
    }

    printf("TOD id=0x%02x version=%u resolution_ticks=%u frames=%lu\n",
           (unsigned)ctx.header.id,
           (unsigned)ctx.header.version,
           (unsigned)ctx.header.resolution_ticks,
           (unsigned long)ctx.header.frame_count);

    for (i = 0UL; i < ctx.header.frame_count; ++i) {
        r = tmd_tod_get_frame(&ctx, i, &frame);
        if (r != TMD_ERR_OK) {
            printf("frame %lu: %s\n", (unsigned long)i, tmd_result_string(r));
            return 1;
        }
        printf("frame_index=%lu frame_no=%lu packets=%u size_words=%u\n",
               (unsigned long)i,
               (unsigned long)frame.frame_number,
               (unsigned)frame.packet_count,
               (unsigned)frame.frame_size_words);
        for (j = 0UL; j < (tmd_u32)frame.packet_count; ++j) {
            r = tmd_tod_get_packet(&ctx, &frame, j, &packet);
            if (r != TMD_ERR_OK) {
                printf("  packet %lu: %s\n", (unsigned long)j, tmd_result_string(r));
                return 1;
            }
            printf("  packet=%lu object=%u type=0x%x flag=0x%x words=%u",
                   (unsigned long)j,
                   (unsigned)packet.object_id,
                   (unsigned)packet.packet_type,
                   (unsigned)packet.flag,
                   (unsigned)packet.length_words);
            if (packet.packet_type == TMD_TOD_PACKET_COORD_RST) {
                r = tmd_tod_decode_rst_packet(&ctx, &packet, &tr);
                if (r == TMD_ERR_OK) {
                    printf(" RST");
                    if ((tr.flags & TMD_TRANSFORMF_HAS_ROT) != 0U) {
                        printf(" R=(%ld,%ld,%ld)/4096deg",
                               (long)tr.rx_deg_fx12, (long)tr.ry_deg_fx12, (long)tr.rz_deg_fx12);
                    }
                    if ((tr.flags & TMD_TRANSFORMF_HAS_SCALE) != 0U) {
                        printf(" S=(%ld,%ld,%ld)/4096",
                               (long)tr.sx_fx12, (long)tr.sy_fx12, (long)tr.sz_fx12);
                    }
                    if ((tr.flags & TMD_TRANSFORMF_HAS_TRANS) != 0U) {
                        printf(" T=(%ld,%ld,%ld)", (long)tr.tx, (long)tr.ty, (long)tr.tz);
                    }
                    if ((tr.flags & TMD_TRANSFORMF_DELTA) != 0U) printf(" delta");
                }
            }
            printf("\n");
        }
    }

    return 0;
}
