#ifndef XPS_H
#define XPS_H

#include "xps_types.h"
#include "xps_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

int xps_probe_file(const char *path, xps_probe_result *out_probe);

int xps_header_detect_file(const char *path, xps_header_info *out_header);
int xps_header_parse_file(const char *path, const xps_header_sink *sink, void *user);

int xps_detect_file(const char *path, xps_model_info *out_model);
int xps_scan_file(const char *path, xps_scan_result *out_scan);
int xps_parse_file(const char *path, const xps_sink *sink, void *user);

int xps_pose_detect_file(const char *path, xps_pose_info *out_pose);
int xps_pose_scan_file(const char *path, xps_pose_scan_result *out_scan);
int xps_pose_parse_file(const char *path, const xps_pose_sink *sink, void *user);

int xps_bonedict_detect_file(const char *path, xps_bonedict_info *out_dict);
int xps_bonedict_scan_file(const char *path, xps_bonedict_scan_result *out_scan);
int xps_bonedict_parse_file(const char *path, const xps_bonedict_sink *sink, void *user);

int xps_scene_detect_file(const char *path, xps_scene_info *out_scene);
int xps_scene_scan_file(const char *path, xps_scene_scan_result *out_scan);
int xps_scene_parse_file(const char *path, const xps_scene_sink *sink, void *user);

const char *xps_error_string(int err);
const char *xps_format_string(enum xps_format fmt);

xps_fx xps_fx_from_int(int value);
int xps_fx_to_int_floor(xps_fx value);
int xps_fx_to_int_round(xps_fx value);

#ifdef __cplusplus
}
#endif

#endif
