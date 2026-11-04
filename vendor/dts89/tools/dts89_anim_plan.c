#include "dts89.h"
#include <stdio.h>

#define DTS89_ANIM_MAX_INPUT (32u * 1024u * 1024u)
#define DTS89_ANIM_WORKSPACE (32u * 1024u * 1024u)
#define DTS89_ANIM_MAX_TRIGGERS 64u

static unsigned char g_input[DTS89_ANIM_MAX_INPUT];
static unsigned char g_workspace[DTS89_ANIM_WORKSPACE];
static dts89_trigger_event g_events[DTS89_ANIM_MAX_TRIGGERS];

static long anim_file_size(FILE *f)
{
    long cur;
    long end;
    cur = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static int parse_s32_arg(const char *s, dts89_s32 *out)
{
    dts89_s32 v;
    dts89_s32 neg;
    if (!s || !out) return 0;
    v = 0;
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    if (!*s) return 0;
    while (*s) {
        dts89_s32 d;
        if (*s < '0' || *s > '9') return 0;
        d = (dts89_s32)(*s - '0');
        if (v > (0x7FFFFFFF - d) / 10) return 0;
        v = v * 10 + d;
        s++;
    }
    *out = neg ? -v : v;
    return 1;
}

static const char *scale_kind_name(dts89_u32 k)
{
    if (k == DTS89_SCALE_KIND_UNIFORM) return "uniform";
    if (k == DTS89_SCALE_KIND_ALIGNED) return "aligned";
    if (k == DTS89_SCALE_KIND_ARBITRARY) return "arbitrary";
    return "none";
}

int main(int argc, char **argv)
{
    FILE *f;
    long sz;
    dts89_arena arena;
    dts89_shape shape;
    dts89_sequence_cursor cur;
    dts89_s32 seq_index;
    dts89_s32 pos_q16;
    dts89_s32 i;
    dts89_u32 event_count;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dts [sequence_index] [position_q16]\n", argv[0]);
        return 1;
    }
    seq_index = 0;
    pos_q16 = DTS89_FX_ONE / 2;
    if (argc > 2 && !parse_s32_arg(argv[2], &seq_index)) {
        printf("bad sequence index\n");
        return 2;
    }
    if (argc > 3 && !parse_s32_arg(argv[3], &pos_q16)) {
        printf("bad position\n");
        return 3;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 4;
    }
    sz = anim_file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_ANIM_MAX_INPUT) {
        printf("input too large\n");
        fclose(f);
        return 5;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 6;
    }
    fclose(f);
    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_decode_shape(g_input, (dts89_u32)sz, &arena, &shape);
    if (rc) {
        printf("decode failed: %d\n", rc);
        return 7;
    }
    if (seq_index < 0 || seq_index >= shape.num_sequences) {
        printf("sequence out of range, sequences=%d\n", (int)shape.num_sequences);
        return 8;
    }
    rc = dts89_sequence_select_keyframes(&shape.sequences[seq_index], (dts89_fx16_16)pos_q16, &cur);
    if (rc) {
        printf("select keyframes failed: %d\n", rc);
        return 9;
    }
    cur.sequence_index = seq_index;
    printf("sequence=%d name=%s pos_q16=%d key0=%d key1=%d key_pos=%d flags=0x%08X ground0=%d ground1=%d ground_pos=%d\n", (int)seq_index, dts89_name_at(&shape, shape.sequences[seq_index].name_index), (int)cur.position, (int)cur.key0, (int)cur.key1, (int)cur.key_pos, (unsigned)cur.flags, (int)cur.ground_index0, (int)cur.ground_index1, (int)cur.ground_pos);
    for (i = 0; i < shape.num_nodes && i < 16; ++i) {
        dts89_node_anim_indices ni;
        rc = dts89_sequence_get_node_tracks(&shape, seq_index, i, &cur, &ni);
        if (rc == DTS89_OK && ni.dirty_flags) {
            printf("node[%d]: rot=%d/%d trans=%d/%d scale=%d/%d %s dirty=0x%08X\n", (int)i, (int)ni.rotation_index0, (int)ni.rotation_index1, (int)ni.translation_index0, (int)ni.translation_index1, (int)ni.scale_index0, (int)ni.scale_index1, scale_kind_name(ni.scale_kind), (unsigned)ni.dirty_flags);
        }
    }
    for (i = 0; i < shape.num_objects && i < 16; ++i) {
        dts89_object_anim_indices oi;
        rc = dts89_sequence_get_object_tracks(&shape, seq_index, i, &cur, &oi);
        if (rc == DTS89_OK && oi.dirty_flags) {
            printf("object[%d]: vis=%d/%d frame=%d/%d mat=%d/%d dirty=0x%08X\n", (int)i, (int)oi.vis_state_index0, (int)oi.vis_state_index1, (int)oi.frame_state_index0, (int)oi.frame_state_index1, (int)oi.mat_frame_state_index0, (int)oi.mat_frame_state_index1, (unsigned)oi.dirty_flags);
        }
    }
    rc = dts89_sequence_scan_triggers(&shape, seq_index, 0, (dts89_fx16_16)pos_q16, g_events, DTS89_ANIM_MAX_TRIGGERS, &event_count);
    if (rc && rc != DTS89_ERR_NO_SPACE) {
        printf("trigger scan failed: %d\n", rc);
        return 10;
    }
    printf("trigger_events=%u\n", (unsigned)event_count);
    if (event_count > DTS89_ANIM_MAX_TRIGGERS) event_count = DTS89_ANIM_MAX_TRIGGERS;
    for (i = 0; i < (dts89_s32)event_count; ++i) {
        printf("trigger[%d]: index=%d mask=0x%08X enabled=%u reverse=%u pos_bits=0x%08X\n", (int)i, (int)g_events[i].trigger_index, (unsigned)g_events[i].state_mask, (unsigned)g_events[i].enabled, (unsigned)g_events[i].reverse, (unsigned)g_events[i].pos_bits);
    }
    return 0;
}
