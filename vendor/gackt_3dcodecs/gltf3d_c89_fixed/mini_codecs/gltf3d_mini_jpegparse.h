#ifndef GLTF3D_MINI_JPEGPARSE_H
#define GLTF3D_MINI_JPEGPARSE_H

#include "gltf3d_mini_common.h"

#define GLTF3D_MINI_JPEG_MAX_COMPONENTS 4
#define GLTF3D_MINI_JPEG_MAX_QTABLES 4
#define GLTF3D_MINI_JPEG_MAX_HTABLES 8
#define GLTF3D_MINI_JPEG_MAX_SCANS 16
#define GLTF3D_MINI_JPEG_FLAG_JFIF 1
#define GLTF3D_MINI_JPEG_FLAG_EXIF 2
#define GLTF3D_MINI_JPEG_FLAG_ADOBE 4
#define GLTF3D_MINI_JPEG_FLAG_PROGRESSIVE 8
#define GLTF3D_MINI_JPEG_FLAG_BASELINE 16
#define GLTF3D_MINI_JPEG_FLAG_HAS_DRI 32
#define GLTF3D_MINI_JPEG_FLAG_HAS_EOI 64

typedef struct gltf3d_mini_jpeg_component_s {
    int id;
    int h_samp;
    int v_samp;
    int qtable;
} gltf3d_mini_jpeg_component;

typedef struct gltf3d_mini_jpeg_scan_s {
    unsigned long data_offset;
    unsigned long data_length;
    int component_count;
    int ss;
    int se;
    int ah;
    int al;
} gltf3d_mini_jpeg_scan;

typedef struct gltf3d_mini_jpeg_info_s {
    unsigned long width;
    unsigned long height;
    int precision;
    int frame_marker;
    int component_count;
    gltf3d_mini_jpeg_component components[GLTF3D_MINI_JPEG_MAX_COMPONENTS];
    unsigned long marker_count;
    unsigned long app_count;
    unsigned long com_count;
    unsigned long dqt_mask;
    unsigned long dht_mask;
    unsigned long restart_interval;
    unsigned long flags;
    int scan_count;
    gltf3d_mini_jpeg_scan scans[GLTF3D_MINI_JPEG_MAX_SCANS];
} gltf3d_mini_jpeg_info;

int gltf3d_mini_jpeg_parse(const unsigned char *data, unsigned long size, gltf3d_mini_jpeg_info *info);
int gltf3d_mini_jpeg_is_baseline_8bit_1_or_3(const gltf3d_mini_jpeg_info *info);

#endif
