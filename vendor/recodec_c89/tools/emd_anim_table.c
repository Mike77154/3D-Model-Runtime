/*
   emd_anim_table.c - RE1 EMD EDD/EMR animation bank/clip/frame table probe.

   Purpose:
     - Verify the six-section RE1 EMD layout: EMR_A, EDD_A, EMR_B, EDD_B, TMD, TIM.
     - Print clip/frame ranges using RE1MV-compatible EDD/EMR byte semantics.
     - Keep this probe deterministic and industrial-friendly: no heap, no float,
       no double, no realloc. The file is read into a fixed static buffer.

   C89, no external dependencies.
*/
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define RE1_EMD_MAX_FILE (4u * 1024u * 1024u)
#define RE1_MAX_SECTIONS 6u
#define RE1_MAX_CLIPS    256u
#define RE1_MAX_TRACKS   8192u

static unsigned char g_file[RE1_EMD_MAX_FILE];

typedef struct re1_sec_s {
    unsigned int off;
    unsigned int size;
} re1_sec;

static unsigned int rd16le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static unsigned int rd32le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int range_ok(unsigned int pos, unsigned int len, unsigned int size)
{
    return pos <= size && len <= size - pos;
}

static int load_file_fixed(const char *path, unsigned int *out_size)
{
    FILE *f;
    size_t got;
    long n;
    if (path == NULL || out_size == NULL) return 0;
    *out_size = 0u;
    f = fopen(path, "rb");
    if (f == NULL) return 0;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n <= 0L || (unsigned long)n > (unsigned long)RE1_EMD_MAX_FILE) { fclose(f); return 0; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return 0; }
    got = fread(g_file, 1u, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) return 0;
    *out_size = (unsigned int)got;
    return 1;
}

static int read_re1_end_sections(const unsigned char *data, unsigned int size, re1_sec secs[RE1_MAX_SECTIONS], unsigned int *sec_count)
{
    unsigned int o[5];
    unsigned int dir_start;
    unsigned int i;
    if (data == NULL || secs == NULL || sec_count == NULL) return 0;
    *sec_count = 0u;
    if (size < 20u) return 0;
    dir_start = size - 20u;
    for (i = 0u; i < 5u; ++i) o[i] = rd32le(data + dir_start + i * 4u);
    for (i = 0u; i < 5u; ++i) {
        if (o[i] >= dir_start) return 0;
        if (i > 0u && o[i] < o[i - 1u]) return 0;
    }
    if (o[0] == 0u) return 0;
    secs[0].off = 0u;    secs[0].size = o[0];
    secs[1].off = o[0];  secs[1].size = o[1] - o[0];
    secs[2].off = o[1];  secs[2].size = o[2] - o[1];
    secs[3].off = o[2];  secs[3].size = o[3] - o[2];
    secs[4].off = o[3];  secs[4].size = o[4] - o[3];
    secs[5].off = o[4];  secs[5].size = dir_start - o[4];
    *sec_count = 6u;
    return 1;
}

static unsigned int re1mv_check_id(const unsigned char *emr, unsigned int emr_size)
{
    unsigned int part_list_off;
    unsigned int object_count;
    unsigned int i;
    unsigned int result;
    if (emr == NULL || emr_size < 8u) return 0u;
    part_list_off = rd16le(emr + 0u);
    object_count = rd16le(emr + 4u);
    if (object_count == 0u || object_count > 64u) return 0u;
    if (!range_ok(part_list_off, object_count * 4u, emr_size)) return 0u;
    result = 0u;
    for (i = 0u; i < object_count; ++i) result ^= rd32le(emr + part_list_off + i * 4u);
    return result;
}

static void print_bank(const char *name, const unsigned char *emr, unsigned int emr_size, const unsigned char *edd, unsigned int edd_size, int print_frames)
{
    unsigned int part_list_off;
    unsigned int anim_off;
    unsigned int object_count;
    unsigned int frame_size;
    unsigned int emd_type;
    unsigned int total_poses;
    unsigned int first_stream;
    unsigned int clip_count;
    unsigned int ci;

    printf("bank %s\n", name);
    if (emr == NULL || edd == NULL || emr_size < 8u || edd_size < 4u) {
        printf("  status: invalid bank data\n");
        return;
    }

    part_list_off = rd16le(emr + 0u);
    anim_off = rd16le(emr + 2u);
    object_count = rd16le(emr + 4u);
    frame_size = rd16le(emr + 6u);
    emd_type = re1mv_check_id(emr, emr_size);

    printf("  emr_header: part_list=0x%04X animation=0x%04X objects=%u frame_size=0x%04X\n",
           part_list_off, anim_off, object_count, frame_size);
    printf("  emd_type: 0x%06X\n", emd_type);
    if (part_list_off > 0u && range_ok(part_list_off, object_count * 4u, emr_size)) {
        unsigned int hi;
        printf("  hierarchy_partlist:\n");
        for (hi = 0u; hi < object_count; ++hi) {
            unsigned int hc;
            unsigned int ho;
            unsigned int hj;
            hc = rd16le(emr + part_list_off + hi * 4u + 0u);
            ho = rd16le(emr + part_list_off + hi * 4u + 2u);
            printf("    %02u count=%u off=0x%04X children=", hi, hc, ho);
            if (hc <= 64u && range_ok(part_list_off + ho, hc, emr_size)) {
                for (hj = 0u; hj < hc; ++hj) {
                    printf("%s%u", (hj == 0u) ? "" : ",", (unsigned int)emr[part_list_off + ho + hj]);
                }
            } else {
                printf("bad_range");
            }
            printf("\n");
        }
    }

    if (object_count == 0u || object_count > 64u || frame_size == 0u || anim_off >= emr_size) {
        printf("  status: invalid EMR header\n");
        return;
    }
    if (frame_size < 12u + object_count * 6u) {
        printf("  status: invalid frame_size for object count\n");
        return;
    }
    if (emr_size <= anim_off + 4u) {
        printf("  status: no pose stream\n");
        return;
    }

    total_poses = ((emr_size - 4u) - anim_off) / frame_size;
    printf("  poses: %u\n", total_poses);

    first_stream = rd16le(edd + 2u);
    if (first_stream < 4u || first_stream > edd_size || (first_stream & 3u) != 0u) {
        printf("  status: invalid EDD first_stream=0x%04X\n", first_stream);
        return;
    }
    clip_count = first_stream / 4u;
    if (clip_count > RE1_MAX_CLIPS) clip_count = RE1_MAX_CLIPS;
    printf("  clips: %u\n", clip_count);
    printf("  clip_table:\n");

    for (ci = 0u; ci < clip_count; ++ci) {
        unsigned int frame_count;
        unsigned int stream_off;
        unsigned int track_index;
        unsigned int fi;
        unsigned int min_pose;
        unsigned int max_pose;
        unsigned int bad;
        frame_count = rd16le(edd + ci * 4u + 0u);
        stream_off = rd16le(edd + ci * 4u + 2u);
        track_index = 0u;
        min_pose = 0xFFFFFFFFu;
        max_pose = 0u;
        bad = 0u;
        if (stream_off >= first_stream) track_index = (stream_off - first_stream) / 4u;
        if (frame_count == 0u || stream_off < first_stream || !range_ok(stream_off, frame_count * 4u, edd_size)) {
            printf("    %02u frames=%u stream=0x%04X track=%u status=bad_range\n",
                   ci, frame_count, stream_off, track_index);
            continue;
        }
        for (fi = 0u; fi < frame_count; ++fi) {
            unsigned int rec;
            unsigned int pose;
            rec = stream_off + fi * 4u;
            pose = rd16le(edd + rec + 0u);
            if (pose >= total_poses) bad = 1u;
            if (pose < min_pose) min_pose = pose;
            if (pose > max_pose) max_pose = pose;
        }
        printf("    %02u frames=%u stream=0x%04X track=%u pose_range=%u..%u status=%s\n",
               ci, frame_count, stream_off, track_index, min_pose, max_pose, bad ? "bad_pose" : "ok");
        if (print_frames) {
            printf("      frames:");
            for (fi = 0u; fi < frame_count; ++fi) {
                unsigned int rec;
                unsigned int pose;
                unsigned int step;
                rec = stream_off + fi * 4u;
                pose = rd16le(edd + rec + 0u);
                step = rd16le(edd + rec + 2u);
                printf(" %u:%u/%u", fi, pose, step);
            }
            printf("\n");
        }
    }
}

int main(int argc, char **argv)
{
    unsigned int size;
    re1_sec secs[RE1_MAX_SECTIONS];
    unsigned int sec_count;
    int print_frames;
    if (argc < 2) {
        printf("usage: emd_anim_table input.emd [--frames]\n");
        return 2;
    }
    print_frames = (argc > 2 && strcmp(argv[2], "--frames") == 0) ? 1 : 0;
    if (!load_file_fixed(argv[1], &size)) {
        fprintf(stderr, "load failed or file exceeds fixed %u-byte buffer\n", RE1_EMD_MAX_FILE);
        return 1;
    }
    if (!read_re1_end_sections(g_file, size, secs, &sec_count) || sec_count < 6u) {
        fprintf(stderr, "not a six-section RE1 EMD with trailing 5-offset directory\n");
        return 1;
    }

    printf("file_size: %u\n", size);
    printf("sections:\n");
    printf("  0 EMR_A off=0x%08X size=%u\n", secs[0].off, secs[0].size);
    printf("  1 EDD_A off=0x%08X size=%u\n", secs[1].off, secs[1].size);
    printf("  2 EMR_B off=0x%08X size=%u\n", secs[2].off, secs[2].size);
    printf("  3 EDD_B off=0x%08X size=%u\n", secs[3].off, secs[3].size);
    printf("  4 TMD   off=0x%08X size=%u\n", secs[4].off, secs[4].size);
    printf("  5 TIM   off=0x%08X size=%u\n", secs[5].off, secs[5].size);

    print_bank("A", g_file + secs[0].off, secs[0].size, g_file + secs[1].off, secs[1].size, print_frames);
    print_bank("B", g_file + secs[2].off, secs[2].size, g_file + secs[3].off, secs[3].size, print_frames);
    return 0;
}
