/*
   emd_anim_report.c - RE1 EMD EDD/EMR deep animation audit.

   This is a deterministic C89 probe for the exact question that keeps biting
   RE1 enemy/player animation viewers: where does each animation begin/end,
   which EMR poses are referenced by each EDD clip, and does a strip look wrong
   because the byte table is wrong or because the preview camera/autofit is lying.

   No heap, no malloc/realloc/calloc, no float/double, fixed input buffer.
*/
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#define RE1_EMD_MAX_FILE (4u * 1024u * 1024u)
#define RE1_MAX_SECTIONS 6u
#define RE1_MAX_CLIPS    256u
#define RE1_MAX_POSES    8192u
#define RE1_MAX_FRAMES   8192u

static unsigned char g_file[RE1_EMD_MAX_FILE];

typedef struct re1_sec_s { unsigned int off; unsigned int size; } re1_sec;

typedef struct clip_info_s {
    unsigned int valid;
    unsigned int frames;
    unsigned int stream;
    unsigned int track;
    unsigned int pose_first;
    unsigned int pose_last;
    unsigned int pose_min;
    unsigned int pose_max;
    unsigned int step_total;
    unsigned int repeated_edges;
    unsigned int backward_edges;
    unsigned int direction_flips;
    unsigned int bad_pose;
    int root_start[3];
    int root_end[3];
    int root_min[3];
    int root_max[3];
    int root_delta[3];
    unsigned int root_manhattan;
    const char *shape;
} clip_info;

static unsigned int rd16le(const unsigned char *p)
{ return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8); }

static unsigned int rd32le(const unsigned char *p)
{ return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24); }

static int range_ok(unsigned int pos, unsigned int len, unsigned int size)
{ return pos <= size && len <= size - pos; }

static unsigned int iabs_u(int v)
{ return (v < 0) ? (unsigned int)(-v) : (unsigned int)v; }

/* RE1MV Calculate_Pos() in integer centi-units. RE1MV divides by 100 for GL. */
static int calc_centi(unsigned int w)
{
    w &= 0xFFFFu;
    if (w > 32767u) return (int)(65535u - w);
    return -(int)w;
}

static int load_file_fixed(const char *path, unsigned int *out_size)
{
    FILE *f; size_t got; long n;
    if (path == NULL || out_size == NULL) return 0;
    *out_size = 0u;
    f = fopen(path, "rb"); if (f == NULL) return 0;
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
    unsigned int o[5]; unsigned int dir_start; unsigned int i;
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
    unsigned int part_list_off; unsigned int object_count; unsigned int i; unsigned int result;
    if (emr == NULL || emr_size < 8u) return 0u;
    part_list_off = rd16le(emr + 0u);
    object_count = rd16le(emr + 4u);
    if (object_count == 0u || object_count > 64u) return 0u;
    if (!range_ok(part_list_off, object_count * 4u, emr_size)) return 0u;
    result = 0u;
    for (i = 0u; i < object_count; ++i) result ^= rd32le(emr + part_list_off + i * 4u);
    return result;
}

static void read_root_xyz(const unsigned char *frame, int xyz[3])
{
    /* File has X,Y,Z; RE1MV displays glTranslatef(Z,Y,X). Store display X,Y,Z. */
    xyz[0] = calc_centi(rd16le(frame + 4u));
    xyz[1] = calc_centi(rd16le(frame + 2u));
    xyz[2] = calc_centi(rd16le(frame + 0u));
}

static const char *classify_clip(const clip_info *c)
{
    if (c->bad_pose) return "bad_pose";
    if (c->frames <= 1u) return "single_pose/static_or_pose_marker";
    if (c->root_manhattan >= 600u && c->backward_edges <= 1u) return "travelling/root_motion_candidate";
    if (c->backward_edges > 2u && c->direction_flips > 0u) return "pingpong/reused_pose_candidate";
    if (c->root_manhattan < 80u && c->pose_max - c->pose_min <= 6u) return "small_loop_or_idle_candidate";
    if (c->root_manhattan < 160u) return "mostly_in_place_candidate";
    return "one_shot_pose_sequence_candidate";
}

static void analyze_clip(const unsigned char *emr, unsigned int emr_size, const unsigned char *edd, unsigned int edd_size,
                         unsigned int anim_off, unsigned int frame_size, unsigned int total_poses,
                         unsigned int first_stream, unsigned int ci, clip_info *out)
{
    unsigned int frame_count; unsigned int stream_off; unsigned int fi; unsigned int prev_pose; int prev_dir;
    int xyz[3]; unsigned int pose; unsigned int rec; unsigned int frame_pos; unsigned int step; unsigned int k;
    memset(out, 0, sizeof(*out));
    out->pose_min = 0xFFFFFFFFu;
    frame_count = rd16le(edd + ci * 4u + 0u);
    stream_off = rd16le(edd + ci * 4u + 2u);
    out->frames = frame_count; out->stream = stream_off; out->track = 0u;
    if (stream_off >= first_stream) out->track = (stream_off - first_stream) / 4u;
    if (frame_count == 0u || stream_off < first_stream || !range_ok(stream_off, frame_count * 4u, edd_size)) return;
    out->valid = 1u;
    prev_pose = 0u; prev_dir = 0;
    for (fi = 0u; fi < frame_count; ++fi) {
        rec = stream_off + fi * 4u;
        pose = rd16le(edd + rec + 0u);
        step = rd16le(edd + rec + 2u);
        if (fi == 0u) out->pose_first = pose;
        out->pose_last = pose;
        if (pose < out->pose_min) out->pose_min = pose;
        if (pose > out->pose_max) out->pose_max = pose;
        out->step_total += step;
        if (pose >= total_poses) { out->bad_pose = 1u; continue; }
        frame_pos = anim_off + pose * frame_size;
        if (!range_ok(frame_pos, frame_size, emr_size)) { out->bad_pose = 1u; continue; }
        read_root_xyz(emr + frame_pos, xyz);
        if (fi == 0u) {
            for (k = 0u; k < 3u; ++k) { out->root_start[k] = xyz[k]; out->root_min[k] = xyz[k]; out->root_max[k] = xyz[k]; }
        }
        for (k = 0u; k < 3u; ++k) {
            out->root_end[k] = xyz[k];
            if (xyz[k] < out->root_min[k]) out->root_min[k] = xyz[k];
            if (xyz[k] > out->root_max[k]) out->root_max[k] = xyz[k];
        }
        if (fi > 0u) {
            int dir;
            if (pose == prev_pose) out->repeated_edges++;
            if (pose < prev_pose) { out->backward_edges++; dir = -1; }
            else if (pose > prev_pose) dir = 1;
            else dir = 0;
            if (dir != 0 && prev_dir != 0 && dir != prev_dir) out->direction_flips++;
            if (dir != 0) prev_dir = dir;
        }
        prev_pose = pose;
    }
    for (k = 0u; k < 3u; ++k) out->root_delta[k] = out->root_end[k] - out->root_start[k];
    out->root_manhattan = iabs_u(out->root_delta[0]) + iabs_u(out->root_delta[1]) + iabs_u(out->root_delta[2]);
    out->shape = classify_clip(out);
}

static int ranges_overlap(unsigned int a0, unsigned int a1, unsigned int b0, unsigned int b1)
{
    if (a0 > a1 || b0 > b1) return 0;
    return !(a1 < b0 || b1 < a0);
}

static void print_bank(const char *name, const unsigned char *emr, unsigned int emr_size, const unsigned char *edd, unsigned int edd_size)
{
    unsigned int part_list_off; unsigned int anim_off; unsigned int object_count; unsigned int frame_size;
    unsigned int emd_type; unsigned int total_poses; unsigned int first_stream; unsigned int clip_count; unsigned int ci; unsigned int cj;
    clip_info clips[RE1_MAX_CLIPS];
    printf("bank %s\n", name);
    if (emr == NULL || edd == NULL || emr_size < 8u || edd_size < 4u) { printf("status,invalid_bank\n"); return; }
    part_list_off = rd16le(emr + 0u); anim_off = rd16le(emr + 2u); object_count = rd16le(emr + 4u); frame_size = rd16le(emr + 6u);
    emd_type = re1mv_check_id(emr, emr_size);
    printf("emr_header,part_list=0x%04X,animation=0x%04X,objects=%u,frame_size=0x%04X,emd_type=0x%06X\n", part_list_off, anim_off, object_count, frame_size, emd_type);
    if (object_count == 0u || object_count > 64u || frame_size == 0u || anim_off >= emr_size || frame_size < 12u + object_count * 6u) { printf("status,invalid_emr_header\n"); return; }
    total_poses = ((emr_size - 4u) - anim_off) / frame_size;
    first_stream = rd16le(edd + 2u);
    if (first_stream < 4u || first_stream > edd_size || (first_stream & 3u) != 0u) { printf("status,invalid_edd_first_stream=0x%04X\n", first_stream); return; }
    clip_count = first_stream / 4u; if (clip_count > RE1_MAX_CLIPS) clip_count = RE1_MAX_CLIPS;
    printf("poses,%u\n", total_poses);
    printf("clips,%u\n", clip_count);
    printf("clip,frames,stream,track,pose_first,pose_last,pose_min,pose_max,step_total,root_start_xyz_centi,root_end_xyz_centi,root_delta_xyz_centi,root_motion,repeat_edges,back_edges,dir_flips,overlaps,classification,status\n");
    for (ci = 0u; ci < clip_count; ++ci) analyze_clip(emr, emr_size, edd, edd_size, anim_off, frame_size, total_poses, first_stream, ci, &clips[ci]);
    for (ci = 0u; ci < clip_count; ++ci) {
        unsigned int overlaps; const char *status;
        overlaps = 0u;
        for (cj = 0u; cj < clip_count; ++cj) {
            if (ci == cj) continue;
            if (clips[ci].valid && clips[cj].valid && ranges_overlap(clips[ci].pose_min, clips[ci].pose_max, clips[cj].pose_min, clips[cj].pose_max)) overlaps++;
        }
        status = clips[ci].valid ? (clips[ci].bad_pose ? "bad_pose" : "ok") : "bad_range";
        printf("%02u,%u,0x%04X,%u,%u,%u,%u,%u,%u,%d/%d/%d,%d/%d/%d,%d/%d/%d,%u,%u,%u,%u,%u,%s,%s\n",
               ci, clips[ci].frames, clips[ci].stream, clips[ci].track, clips[ci].pose_first, clips[ci].pose_last,
               clips[ci].pose_min, clips[ci].pose_max, clips[ci].step_total,
               clips[ci].root_start[0], clips[ci].root_start[1], clips[ci].root_start[2],
               clips[ci].root_end[0], clips[ci].root_end[1], clips[ci].root_end[2],
               clips[ci].root_delta[0], clips[ci].root_delta[1], clips[ci].root_delta[2],
               clips[ci].root_manhattan, clips[ci].repeated_edges, clips[ci].backward_edges, clips[ci].direction_flips,
               overlaps, clips[ci].shape ? clips[ci].shape : "unknown", status);
    }
}

int main(int argc, char **argv)
{
    unsigned int size; re1_sec secs[RE1_MAX_SECTIONS]; unsigned int sec_count;
    (void)argv;
    if (argc < 2) { printf("usage: emd_anim_report input.emd\n"); return 2; }
    if (!load_file_fixed(argv[1], &size)) { fprintf(stderr, "load failed or file exceeds fixed buffer\n"); return 1; }
    if (!read_re1_end_sections(g_file, size, secs, &sec_count) || sec_count < 6u) { fprintf(stderr, "not a six-section RE1 EMD\n"); return 1; }
    printf("file_size,%u\n", size);
    printf("sections,EMR_A=0x%08X/%u,EDD_A=0x%08X/%u,EMR_B=0x%08X/%u,EDD_B=0x%08X/%u,TMD=0x%08X/%u,TIM=0x%08X/%u\n",
           secs[0].off, secs[0].size, secs[1].off, secs[1].size, secs[2].off, secs[2].size, secs[3].off, secs[3].size, secs[4].off, secs[4].size, secs[5].off, secs[5].size);
    print_bank("A", g_file + secs[0].off, secs[0].size, g_file + secs[1].off, secs[1].size);
    print_bank("B", g_file + secs[2].off, secs[2].size, g_file + secs[3].off, secs[3].size);
    return 0;
}
