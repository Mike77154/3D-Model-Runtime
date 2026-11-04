#include "gfdxf_payload_extra.h"

#include <string.h>
#include <stdio.h>

static int gfdxf_extra_is_hex(char c)
{
    if (c >= '0' && c <= '9') { return 1; }
    if (c >= 'a' && c <= 'f') { return 1; }
    if (c >= 'A' && c <= 'F') { return 1; }
    return 0;
}

static int gfdxf_extra_hex_val(char c)
{
    if (c >= '0' && c <= '9') { return (int)(c - '0'); }
    if (c >= 'a' && c <= 'f') { return (int)(c - 'a') + 10; }
    if (c >= 'A' && c <= 'F') { return (int)(c - 'A') + 10; }
    return 0;
}

static int gfdxf_extra_tolower(int c)
{
    if (c >= 'A' && c <= 'Z') { return c - 'A' + 'a'; }
    return c;
}

static int gfdxf_extra_str_contains_i(const char *hay, const char *needle)
{
    int i;
    int j;
    int ok;
    if (hay == 0 || needle == 0 || needle[0] == '\0') { return 0; }
    for (i = 0; hay[i] != '\0'; i++) {
        ok = 1;
        for (j = 0; needle[j] != '\0'; j++) {
            if (hay[i + j] == '\0') { ok = 0; break; }
            if (gfdxf_extra_tolower((int)hay[i + j]) != gfdxf_extra_tolower((int)needle[j])) { ok = 0; break; }
        }
        if (ok) { return 1; }
    }
    return 0;
}

static int gfdxf_extra_decode_hex_ascii(const char *hex, char *out, int cap)
{
    int i;
    int n;
    int hi;
    int lo;
    int b;
    if (out == 0 || cap <= 0) { return 0; }
    out[0] = '\0';
    if (hex == 0) { return 0; }
    i = 0;
    n = 0;
    while (hex[i] != '\0' && hex[i + 1] != '\0' && n + 1 < cap) {
        if (!gfdxf_extra_is_hex(hex[i]) || !gfdxf_extra_is_hex(hex[i + 1])) { break; }
        hi = gfdxf_extra_hex_val(hex[i]);
        lo = gfdxf_extra_hex_val(hex[i + 1]);
        b = (hi << 4) | lo;
        if (b == 0) { out[n++] = ' '; }
        else if (b >= 32 && b <= 126) { out[n++] = (char)b; }
        else { out[n++] = '.'; }
        i += 2;
    }
    out[n] = '\0';
    return n;
}

static void gfdxf_extra_append_char(char *out, int cap, int *pos, char c)
{
    if (out == 0 || pos == 0 || cap <= 0) { return; }
    if (*pos + 1 < cap) {
        out[*pos] = c;
        *pos = *pos + 1;
        out[*pos] = '\0';
    }
}

static void gfdxf_extra_append_text(char *out, int cap, int *pos, const char *text)
{
    int i;
    if (text == 0) { return; }
    for (i = 0; text[i] != '\0'; i++) { gfdxf_extra_append_char(out, cap, pos, text[i]); }
}

static void gfdxf_extra_append_int(char *out, int cap, int *pos, long v)
{
    char tmp[32];
    sprintf(tmp, "%ld", v);
    gfdxf_extra_append_text(out, cap, pos, tmp);
}

static int gfdxf_extra_class_is_acdb(const gfdxf_payload *p)
{
    if (p == 0) { return 0; }
    if (gfdxf_extra_str_contains_i(p->resolved_cpp_name, "AcDb")) { return 1; }
    if (gfdxf_extra_str_contains_i(p->resolved_dxf_record, "ACDB")) { return 1; }
    if (gfdxf_extra_str_contains_i(p->resolved_app_name, "ACDB")) { return 1; }
    return 0;
}

static int gfdxf_extra_class_is_objectdbx(const gfdxf_payload *p)
{
    if (p == 0) { return 0; }
    if (gfdxf_extra_str_contains_i(p->resolved_cpp_name, "ObjectDBX")) { return 1; }
    if (gfdxf_extra_str_contains_i(p->resolved_app_name, "ObjectDBX")) { return 1; }
    if (gfdxf_extra_str_contains_i(p->resolved_dxf_record, "OBJECTDBX")) { return 1; }
    return 0;
}

void gfdxf_payload_semantic_clear(gfdxf_payload_semantic *out_semantic)
{
    if (out_semantic != 0) { memset(out_semantic, 0, sizeof(gfdxf_payload_semantic)); }
}

void gfdxf_payload_semantic_scan(const gfdxf_payload *payload, gfdxf_payload_semantic *out_semantic)
{
    char ascii[GFDXF_MAX_PAYLOAD_DECODE_CHARS];
    int i;
    int b;
    if (out_semantic == 0) { return; }
    gfdxf_payload_semantic_clear(out_semantic);
    if (payload == 0) { return; }
    out_semantic->valid = 1;
    out_semantic->class_is_resolved = payload->proxy_class_index >= 0;
    out_semantic->class_is_acdb = gfdxf_extra_class_is_acdb(payload);
    out_semantic->class_is_objectdbx = gfdxf_extra_class_is_objectdbx(payload);
    out_semantic->class_is_custom = out_semantic->class_is_resolved && !out_semantic->class_is_acdb && !out_semantic->class_is_objectdbx;
    out_semantic->decoded_bytes = gfdxf_extra_decode_hex_ascii(payload->first_hex, ascii, GFDXF_MAX_PAYLOAD_DECODE_CHARS);
    for (i = 0; i < out_semantic->decoded_bytes; i++) {
        b = (int)ascii[i];
        if (b >= 32 && b <= 126 && b != '.') { out_semantic->ascii_bytes++; }
        else if (b == ' ') { out_semantic->zero_bytes++; }
        else { out_semantic->control_bytes++; }
    }
    if (out_semantic->decoded_bytes > 0) {
        out_semantic->ascii_like_score = (out_semantic->ascii_bytes * 100) / out_semantic->decoded_bytes;
    }
    out_semantic->has_acdb_token = gfdxf_extra_str_contains_i(ascii, "AcDb");
    out_semantic->has_objectdbx_token = gfdxf_extra_str_contains_i(ascii, "ObjectDBX");
    out_semantic->has_acis_token = gfdxf_extra_str_contains_i(ascii, "ACIS") || payload->acis_topology_score > 0;
    out_semantic->has_sat_token = gfdxf_extra_str_contains_i(ascii, "SAT") || payload->acis_line_count > 0;
    out_semantic->has_sab_hint = payload->kind == GFDXF_PAYLOAD_ACIS_BINARY || gfdxf_extra_str_contains_i(ascii, "SAB");
    if (gfdxf_extra_str_contains_i(ascii, "LINE")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "CIRCLE")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "ARC")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "POLYLINE")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "LWPOLYLINE")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "MESH")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "HATCH")) { out_semantic->standard_entity_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "DICTIONARY")) { out_semantic->standard_object_hits++; }
    if (gfdxf_extra_str_contains_i(ascii, "XRECORD")) { out_semantic->standard_object_hits++; }
    out_semantic->needs_object_enabler = gfdxf_payload_needs_object_enabler(payload);
    out_semantic->keep_bytes_required = payload->total_hex_chars > 0 || payload->kind == GFDXF_PAYLOAD_PROXY_ENTITY || payload->kind == GFDXF_PAYLOAD_PROXY_OBJECT;
}

int gfdxf_payload_needs_object_enabler(const gfdxf_payload *payload)
{
    if (payload == 0) { return 0; }
    if (payload->kind != GFDXF_PAYLOAD_PROXY_ENTITY && payload->kind != GFDXF_PAYLOAD_PROXY_OBJECT) { return 0; }
    if (payload->proxy_requires_enabler) { return 1; }
    if (payload->class_id >= 500) { return 1; }
    if (payload->proxy_class_index < 0) { return 1; }
    if (!gfdxf_extra_class_is_acdb(payload) && !gfdxf_extra_class_is_objectdbx(payload)) { return 1; }
    return 0;
}

int gfdxf_payload_get_handle(const gfdxf_payload *payload, int index, char *out_handle, int out_cap)
{
    int i;
    if (out_handle != 0 && out_cap > 0) { out_handle[0] = '\0'; }
    if (payload == 0 || out_handle == 0 || out_cap <= 0) { return 0; }
    if (index < 0 || index >= payload->handle_count || index >= GFDXF_MAX_HANDLES) { return 0; }
    for (i = 0; i + 1 < out_cap && payload->handles[index][i] != '\0'; i++) { out_handle[i] = payload->handles[index][i]; }
    out_handle[i] = '\0';
    return 1;
}

int gfdxf_payload_make_manifest_line(const gfdxf_scene *scene, int payload_index, char *out_text, int out_cap)
{
    const gfdxf_payload *p;
    gfdxf_payload_semantic sem;
    int pos;
    if (out_text == 0 || out_cap <= 0) { return 0; }
    out_text[0] = '\0';
    if (scene == 0 || payload_index < 0 || payload_index >= scene->payload_count) { return 0; }
    p = &scene->payloads[payload_index];
    gfdxf_payload_semantic_scan(p, &sem);
    pos = 0;
    gfdxf_extra_append_text(out_text, out_cap, &pos, "payload#");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)payload_index);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " kind=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)p->kind);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " name=");
    gfdxf_extra_append_text(out_text, out_cap, &pos, p->dxf_name);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " class=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)p->class_id);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " resolved=");
    gfdxf_extra_append_text(out_text, out_cap, &pos, p->resolved_dxf_record);
    gfdxf_extra_append_text(out_text, out_cap, &pos, "/");
    gfdxf_extra_append_text(out_text, out_cap, &pos, p->resolved_cpp_name);
    gfdxf_extra_append_text(out_text, out_cap, &pos, "/");
    gfdxf_extra_append_text(out_text, out_cap, &pos, p->resolved_app_name);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " hex=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)p->stored_hex_chars);
    gfdxf_extra_append_text(out_text, out_cap, &pos, "/");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)p->total_hex_chars);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " handles=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)p->handle_count);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " enabler=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)sem.needs_object_enabler);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " std_hits=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)(sem.standard_entity_hits + sem.standard_object_hits));
    gfdxf_extra_append_text(out_text, out_cap, &pos, " ascii_score=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)sem.ascii_like_score);
    return 1;
}

void gfdxf_scene_payload_manifest(gfdxf_scene *scene, gfdxf_payload_manifest *out_manifest)
{
    int i;
    gfdxf_payload_semantic sem;
    gfdxf_payload *p;
    if (out_manifest == 0) { return; }
    memset(out_manifest, 0, sizeof(gfdxf_payload_manifest));
    if (scene == 0) { return; }
    scene->v13_payload_manifest_runs++;
    out_manifest->payloads = scene->payload_count;
    for (i = 0; i < scene->payload_count; i++) {
        p = &scene->payloads[i];
        gfdxf_payload_semantic_scan(p, &sem);
        if (p->kind == GFDXF_PAYLOAD_PROXY_ENTITY || p->kind == GFDXF_PAYLOAD_PROXY_OBJECT) { out_manifest->proxy_payloads++; }
        if (p->kind == GFDXF_PAYLOAD_ACIS_TEXT || p->kind == GFDXF_PAYLOAD_ACIS_BINARY) { out_manifest->acis_payloads++; }
        if (p->total_hex_chars > 0) { out_manifest->binary_payloads++; scene->v13_payloads_binary_preserved++; }
        if (p->proxy_class_index >= 0) { out_manifest->resolved_classes++; }
        else if (p->class_id != 0) { out_manifest->unresolved_classes++; scene->v13_payload_unknown_class_keep_bytes++; }
        out_manifest->handles_total += p->handle_count;
        if (sem.needs_object_enabler) { out_manifest->need_enabler++; scene->v13_payloads_need_enabler++; }
        if (sem.keep_bytes_required) { out_manifest->keep_bytes++; }
        if (sem.ascii_like_score >= 65) { out_manifest->ascii_like_payloads++; scene->v13_payloads_ascii_like++; }
        if (sem.standard_entity_hits + sem.standard_object_hits > 0) { out_manifest->embedded_standard_hits += sem.standard_entity_hits + sem.standard_object_hits; scene->v13_payloads_embedded_standard++; }
        if (sem.has_acdb_token || sem.class_is_acdb) { out_manifest->embedded_acdb_hits++; scene->v13_payloads_embedded_acdb++; }
        if (sem.has_objectdbx_token || sem.class_is_objectdbx) { out_manifest->embedded_objectdbx_hits++; }
        scene->v13_payload_handles_exposed += p->handle_count;
    }
    scene->v13_class_resolution_passes++;
}

int gfdxf_scene_payload_manifest_line(const gfdxf_scene *scene, char *out_text, int out_cap)
{
    gfdxf_payload_manifest m;
    int pos;
    gfdxf_scene *mutable_scene;
    if (out_text == 0 || out_cap <= 0) { return 0; }
    out_text[0] = '\0';
    if (scene == 0) { return 0; }
    mutable_scene = (gfdxf_scene *)scene;
    gfdxf_scene_payload_manifest(mutable_scene, &m);
    pos = 0;
    gfdxf_extra_append_text(out_text, out_cap, &pos, "payload_manifest payloads=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.payloads);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " proxy=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.proxy_payloads);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " acis=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.acis_payloads);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " binary=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.binary_payloads);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " resolved=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.resolved_classes);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " unresolved=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.unresolved_classes);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " handles=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.handles_total);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " enabler=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.need_enabler);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " keep_bytes=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.keep_bytes);
    gfdxf_extra_append_text(out_text, out_cap, &pos, " std_hits=");
    gfdxf_extra_append_int(out_text, out_cap, &pos, (long)m.embedded_standard_hits);
    return 1;
}
