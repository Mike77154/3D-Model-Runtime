#include "gltf3d_mini_jpegparse.h"

static unsigned int jpg_be16(const unsigned char *p)
{
    return (unsigned int)(((unsigned int)p[0] << 8) | (unsigned int)p[1]);
}

static int jpg_is_sof(int m)
{
    if (m >= 0xc0 && m <= 0xcf && m != 0xc4 && m != 0xc8 && m != 0xcc) return 1;
    return 0;
}

static int jpg_standalone(int m)
{
    if (m == 0xd8 || m == 0xd9) return 1;
    if (m >= 0xd0 && m <= 0xd7) return 1;
    if (m == 0x01) return 1;
    return 0;
}

static void jpg_zero(void *p, unsigned long n)
{
    unsigned long i;
    for (i = 0UL; i < n; ++i) ((unsigned char *)p)[i] = 0;
}

static unsigned long jpg_find_eoi_after_scan(const unsigned char *data, unsigned long size, unsigned long p)
{
    while (p + 1UL < size) {
        if (data[p] == 0xff) {
            if (data[p + 1UL] == 0x00) { p += 2UL; continue; }
            if (data[p + 1UL] == 0xff) { p++; continue; }
            if (data[p + 1UL] == 0xd9) return p;
            if (data[p + 1UL] >= 0xd0 && data[p + 1UL] <= 0xd7) { p += 2UL; continue; }
            return p;
        }
        p++;
    }
    return size;
}

int gltf3d_mini_jpeg_parse(const unsigned char *data, unsigned long size, gltf3d_mini_jpeg_info *info)
{
    unsigned long p;
    unsigned long seg_start;
    unsigned long scan_start;
    unsigned long scan_end;
    unsigned int len;
    int m;
    int i;
    int tc;
    int th;
    if (!data || !info) return GLTF3D_MINI_ERR_BAD_ARG;
    if (size < 4UL || data[0] != 0xff || data[1] != 0xd8) return GLTF3D_MINI_ERR_BAD_DATA;
    jpg_zero(info, (unsigned long)sizeof(*info));
    p = 2UL;
    info->marker_count = 1UL;
    while (p < size) {
        while (p < size && data[p] != 0xff) p++;
        if (p >= size) break;
        while (p < size && data[p] == 0xff) p++;
        if (p >= size) break;
        m = (int)data[p++];
        if (m == 0x00) continue;
        info->marker_count++;
        if (m == 0xd9) { info->flags |= GLTF3D_MINI_JPEG_FLAG_HAS_EOI; return GLTF3D_MINI_OK; }
        if (jpg_standalone(m)) continue;
        if (p + 2UL > size) return GLTF3D_MINI_ERR_BAD_DATA;
        len = jpg_be16(data + p);
        if (len < 2U || p + (unsigned long)len > size) return GLTF3D_MINI_ERR_BAD_DATA;
        seg_start = p + 2UL;
        if (m >= 0xe0 && m <= 0xef) {
            info->app_count++;
            if (m == 0xe0 && len >= 7U && data[seg_start + 0UL] == 'J' && data[seg_start + 1UL] == 'F' && data[seg_start + 2UL] == 'I' && data[seg_start + 3UL] == 'F' && data[seg_start + 4UL] == 0) info->flags |= GLTF3D_MINI_JPEG_FLAG_JFIF;
            if (m == 0xe1 && len >= 8U && data[seg_start + 0UL] == 'E' && data[seg_start + 1UL] == 'x' && data[seg_start + 2UL] == 'i' && data[seg_start + 3UL] == 'f' && data[seg_start + 4UL] == 0) info->flags |= GLTF3D_MINI_JPEG_FLAG_EXIF;
            if (m == 0xee && len >= 7U && data[seg_start + 0UL] == 'A' && data[seg_start + 1UL] == 'd' && data[seg_start + 2UL] == 'o' && data[seg_start + 3UL] == 'b' && data[seg_start + 4UL] == 'e') info->flags |= GLTF3D_MINI_JPEG_FLAG_ADOBE;
        } else if (m == 0xfe) {
            info->com_count++;
        } else if (m == 0xdb) {
            unsigned long q;
            q = seg_start;
            while (q < p + (unsigned long)len) {
                if (q >= p + (unsigned long)len) return GLTF3D_MINI_ERR_BAD_DATA;
                th = (int)(data[q] & 15U);
                tc = (int)(data[q] >> 4);
                q++;
                if (th < 0 || th > 3 || tc > 1) return GLTF3D_MINI_ERR_BAD_DATA;
                info->dqt_mask |= (1UL << th);
                q += (tc ? 128UL : 64UL);
                if (q > p + (unsigned long)len) return GLTF3D_MINI_ERR_BAD_DATA;
            }
        } else if (m == 0xc4) {
            unsigned long q;
            unsigned long codes;
            q = seg_start;
            while (q < p + (unsigned long)len) {
                if (q + 17UL > p + (unsigned long)len) return GLTF3D_MINI_ERR_BAD_DATA;
                th = (int)(data[q] & 15U);
                tc = (int)(data[q] >> 4);
                if (th < 0 || th > 3 || tc > 1) return GLTF3D_MINI_ERR_BAD_DATA;
                q++;
                codes = 0UL;
                for (i = 0; i < 16; ++i) codes += (unsigned long)data[q + (unsigned long)i];
                q += 16UL;
                if (codes > p + (unsigned long)len - q) return GLTF3D_MINI_ERR_BAD_DATA;
                info->dht_mask |= (1UL << (tc * 4 + th));
                q += codes;
            }
        } else if (m == 0xdd) {
            if (len != 4U) return GLTF3D_MINI_ERR_BAD_DATA;
            info->restart_interval = (unsigned long)jpg_be16(data + seg_start);
            info->flags |= GLTF3D_MINI_JPEG_FLAG_HAS_DRI;
        } else if (jpg_is_sof(m)) {
            if (len < 8U) return GLTF3D_MINI_ERR_BAD_DATA;
            info->frame_marker = m;
            info->precision = (int)data[seg_start];
            info->height = (unsigned long)jpg_be16(data + seg_start + 1UL);
            info->width = (unsigned long)jpg_be16(data + seg_start + 3UL);
            info->component_count = (int)data[seg_start + 5UL];
            if (info->component_count < 1 || info->component_count > GLTF3D_MINI_JPEG_MAX_COMPONENTS) return GLTF3D_MINI_ERR_UNSUPPORTED;
            if (len < (unsigned int)(8 + info->component_count * 3)) return GLTF3D_MINI_ERR_BAD_DATA;
            for (i = 0; i < info->component_count; ++i) {
                info->components[i].id = (int)data[seg_start + 6UL + (unsigned long)i * 3UL];
                info->components[i].h_samp = (int)(data[seg_start + 7UL + (unsigned long)i * 3UL] >> 4);
                info->components[i].v_samp = (int)(data[seg_start + 7UL + (unsigned long)i * 3UL] & 15U);
                info->components[i].qtable = (int)data[seg_start + 8UL + (unsigned long)i * 3UL];
            }
            if (m == 0xc0) info->flags |= GLTF3D_MINI_JPEG_FLAG_BASELINE;
            if (m == 0xc2) info->flags |= GLTF3D_MINI_JPEG_FLAG_PROGRESSIVE;
        } else if (m == 0xda) {
            if (len < 6U) return GLTF3D_MINI_ERR_BAD_DATA;
            if (info->scan_count < GLTF3D_MINI_JPEG_MAX_SCANS) {
                gltf3d_mini_jpeg_scan *s;
                s = &info->scans[info->scan_count];
                s->component_count = (int)data[seg_start];
                s->ss = (int)data[p + (unsigned long)len - 3UL];
                s->se = (int)data[p + (unsigned long)len - 2UL];
                s->ah = (int)(data[p + (unsigned long)len - 1UL] >> 4);
                s->al = (int)(data[p + (unsigned long)len - 1UL] & 15U);
                scan_start = p + (unsigned long)len;
                scan_end = jpg_find_eoi_after_scan(data, size, scan_start);
                s->data_offset = scan_start;
                s->data_length = (scan_end >= scan_start) ? (scan_end - scan_start) : 0UL;
                info->scan_count++;
            }
            p = jpg_find_eoi_after_scan(data, size, p + (unsigned long)len);
            continue;
        }
        p += (unsigned long)len;
    }
    if (info->width != 0UL && info->height != 0UL) return GLTF3D_MINI_OK;
    return GLTF3D_MINI_ERR_BAD_DATA;
}

int gltf3d_mini_jpeg_is_baseline_8bit_1_or_3(const gltf3d_mini_jpeg_info *info)
{
    if (!info) return 0;
    if (!(info->flags & GLTF3D_MINI_JPEG_FLAG_BASELINE)) return 0;
    if (info->precision != 8) return 0;
    if (info->component_count != 1 && info->component_count != 3) return 0;
    if (info->scan_count < 1) return 0;
    return 1;
}
