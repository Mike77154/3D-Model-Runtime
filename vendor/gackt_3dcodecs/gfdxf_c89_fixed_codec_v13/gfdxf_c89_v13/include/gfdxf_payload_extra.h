#ifndef GFDXF_PAYLOAD_EXTRA_H
#define GFDXF_PAYLOAD_EXTRA_H

#include "gfdxf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfdxf_payload_semantic_s {
    int valid;
    int ascii_bytes;
    int control_bytes;
    int zero_bytes;
    int decoded_bytes;
    int ascii_like_score;
    int has_acdb_token;
    int has_objectdbx_token;
    int has_acis_token;
    int has_sat_token;
    int has_sab_hint;
    int standard_entity_hits;
    int standard_object_hits;
    int needs_object_enabler;
    int keep_bytes_required;
    int class_is_resolved;
    int class_is_custom;
    int class_is_acdb;
    int class_is_objectdbx;
} gfdxf_payload_semantic;

typedef struct gfdxf_payload_manifest_s {
    int payloads;
    int proxy_payloads;
    int acis_payloads;
    int binary_payloads;
    int resolved_classes;
    int unresolved_classes;
    int handles_total;
    int need_enabler;
    int keep_bytes;
    int ascii_like_payloads;
    int embedded_standard_hits;
    int embedded_acdb_hits;
    int embedded_objectdbx_hits;
} gfdxf_payload_manifest;

void gfdxf_payload_semantic_clear(gfdxf_payload_semantic *out_semantic);
void gfdxf_payload_semantic_scan(const gfdxf_payload *payload, gfdxf_payload_semantic *out_semantic);
int gfdxf_payload_needs_object_enabler(const gfdxf_payload *payload);
int gfdxf_payload_get_handle(const gfdxf_payload *payload, int index, char *out_handle, int out_cap);
int gfdxf_payload_make_manifest_line(const gfdxf_scene *scene, int payload_index, char *out_text, int out_cap);
void gfdxf_scene_payload_manifest(gfdxf_scene *scene, gfdxf_payload_manifest *out_manifest);
int gfdxf_scene_payload_manifest_line(const gfdxf_scene *scene, char *out_text, int out_cap);

#ifdef __cplusplus
}
#endif

#endif
