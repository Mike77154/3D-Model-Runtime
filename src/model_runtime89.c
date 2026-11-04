#include "modelrt_internal.h"
#include "image/model_image89.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static int modelrt_register_provider(modelrt_runtime *rt, const modelrt_provider *p)
{
    if (rt == 0 || p == 0) return MODELRT_ERR_BAD_ARG;
    if (rt->provider_count >= MODELRT_MAX_PROVIDERS) return MODELRT_ERR_LIMIT;
    rt->providers[rt->provider_count++] = p;
    return MODELRT_OK;
}

void modelrt_runtime_init(modelrt_runtime *rt)
{
    if (rt == 0) return;
    modelrt_zero(rt, sizeof(*rt));
    modelrt_runtime_set_image_decoder_mask(rt, MODELRT_IMAGE_ALL);
    modelrt_copy_string(rt->last_error, sizeof(rt->last_error), "OK");
}

int modelrt_register_builtin_providers(modelrt_runtime *rt)
{
    int rc;
    if (rt == 0) return MODELRT_ERR_BAD_ARG;
    rc = modelrt_register_provider(rt, &modelrt_provider_fbx89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_xps89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_x89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_hl2modelib89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_hl1modelib89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_mmd89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_g3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_re3_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gunsurvivor89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_hod2_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_re4classic89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_rtcw89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_3dgs89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_frag_arg89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_frag_ugh89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_blend89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_blade_xmdl89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_blade_xir89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_re89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_serioussam89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_pskpsa89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_ms3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_c4d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_dts89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_off89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_obj89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_ply89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_irr89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_stl89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_md289); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_ac3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_3ds89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_lwo89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_ogex89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_raw89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_nff89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_b3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_q3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_gackt_hmb89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ase89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_md389); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_vrml1_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_vrml97_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ogre89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_gltf89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_acc89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_sib89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_cob89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_md5_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_amf89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ter89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ndo89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ask89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_dbo89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_amj89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_quake89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_mdc89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_x3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_lxo89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_xgl89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_dxf89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_xsi89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ifc89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_bvh_anima89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_bvh_bravure89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_md4_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_md7_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_uc89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_ue1_89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_panda89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_3mf89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_mpkf89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_dae89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_tmd89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_vta89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_m3d89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_csm89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_lws89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_poser89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_dff89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_mot89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_step89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_scn89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_usd89); if (rc != MODELRT_OK) return rc;
    rc = modelrt_register_provider(rt, &modelrt_provider_native_iqm89); if (rc != MODELRT_OK) return rc;
    return MODELRT_OK;
}

void modelrt_handle_init(modelrt_handle *handle, void *workspace, size_t workspace_size)
{
    if (handle == 0) return;
    modelrt_zero(handle, sizeof(*handle));
    handle->workspace = (modelrt_u8 *)workspace;
    handle->workspace_size = workspace_size;
}

void *modelrt_arena_alloc(modelrt_handle *handle, size_t bytes, size_t alignment)
{
    size_t p;
    size_t mask;
    if (handle == 0 || handle->workspace == 0 || bytes == 0u) return 0;
    if (alignment == 0u) alignment = 1u;
    mask = alignment - 1u;
    p = (handle->workspace_used + mask) & ~mask;
    if (p > handle->workspace_size || bytes > handle->workspace_size - p) return 0;
    handle->workspace_used = p + bytes;
    return handle->workspace + p;
}

void *modelrt_binding_alloc(modelrt_animation_binding *binding, size_t bytes, size_t alignment)
{
    size_t p;
    size_t mask;
    if (binding == 0 || binding->workspace == 0 || bytes == 0u) return 0;
    if (alignment == 0u) alignment = 1u;
    mask = alignment - 1u;
    p = (binding->workspace_used + mask) & ~mask;
    if (p > binding->workspace_size || bytes > binding->workspace_size - p) return 0;
    binding->workspace_used = p + bytes;
    return binding->workspace + p;
}

void modelrt_zero(void *p, size_t bytes)
{
    if (p != 0 && bytes != 0u) memset(p, 0, bytes);
}

void modelrt_copy_string(char *dst, size_t cap, const char *src)
{
    size_t i;
    if (dst == 0 || cap == 0u) return;
    if (src == 0) { dst[0] = '\0'; return; }
    i = 0u;
    while (i + 1u < cap && src[i] != '\0') { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

static int modelrt_ascii_ieq(char a, char b)
{
    unsigned char ua;
    unsigned char ub;
    ua = (unsigned char)a;
    ub = (unsigned char)b;
    return tolower(ua) == tolower(ub);
}

int modelrt_path_has_suffix(const char *path, const char *suffix)
{
    size_t a;
    size_t b;
    size_t i;
    if (path == 0 || suffix == 0) return 0;
    a = strlen(path);
    b = strlen(suffix);
    if (b > a) return 0;
    for (i = 0u; i < b; ++i) {
        if (!modelrt_ascii_ieq(path[a - b + i], suffix[i])) return 0;
    }
    return 1;
}

int modelrt_ext_eq(const char *path, const char *ext)
{
    return modelrt_path_has_suffix(path, ext);
}

int modelrt_read_file_arena(modelrt_handle *handle, const char *path,
                            modelrt_u8 **out_data, size_t *out_size)
{
    FILE *f;
    long len;
    modelrt_u8 *data;
    size_t got;
    if (handle == 0 || path == 0 || out_data == 0 || out_size == 0) return MODELRT_ERR_BAD_ARG;
    f = fopen(path, "rb");
    if (f == 0) return MODELRT_ERR_IO;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return MODELRT_ERR_IO; }
    len = ftell(f);
    if (len < 0L) { fclose(f); return MODELRT_ERR_IO; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return MODELRT_ERR_IO; }
    data = (modelrt_u8 *)modelrt_arena_alloc(handle, (size_t)len + 1u, 8u);
    if (data == 0) { fclose(f); return MODELRT_ERR_NO_SPACE; }
    got = fread(data, 1u, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) return MODELRT_ERR_IO;
    data[(size_t)len] = 0u;
    *out_data = data;
    *out_size = (size_t)len;
    return MODELRT_OK;
}

int modelrt_read_file_head(const char *path, modelrt_u8 *dst, size_t cap, size_t *out_size)
{
    FILE *f;
    size_t n;
    if (path == 0 || dst == 0 || cap == 0u || out_size == 0) return MODELRT_ERR_BAD_ARG;
    f = fopen(path, "rb");
    if (f == 0) return MODELRT_ERR_IO;
    n = fread(dst, 1u, cap, f);
    fclose(f);
    *out_size = n;
    return MODELRT_OK;
}

void modelrt_set_error(modelrt_runtime *rt, const char *provider_name, int code, const char *detail)
{
    char temp[192];
    if (rt == 0) return;
    if (provider_name == 0) provider_name = "runtime";
    if (detail == 0) detail = modelrt_error_string(code);
    sprintf(temp, "%s: %s (%d)", provider_name, detail, code);
    modelrt_copy_string(rt->last_error, sizeof(rt->last_error), temp);
}

int modelrt_open_file(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    modelrt_u8 head[96];
    size_t head_size;
    modelrt_u32 i;
    modelrt_u32 attempt;
    int probe_score[MODELRT_MAX_PROVIDERS];
    modelrt_u8 attempted[MODELRT_MAX_PROVIDERS];
    int best_score;
    int best_index;
    const modelrt_provider *best;
    int rc;
    int first_fail_rc;
    char first_fail_error[256];

    if (rt == 0 || handle == 0 || path == 0 || handle->workspace == 0 || handle->workspace_size == 0u)
        return MODELRT_ERR_BAD_ARG;
    if (handle->is_open) modelrt_close(handle);

    handle->workspace_used = 0u;
    handle->native_state = 0;
    handle->provider = 0;
    handle->last_codec_error = 0;
    handle->image_decoder_mask = rt->image_decoder_mask;
    modelrt_zero(&handle->info, sizeof(handle->info));
    modelrt_copy_string(handle->path, sizeof(handle->path), path);

    head_size = 0u;
    rc = modelrt_read_file_head(path, head, sizeof(head), &head_size);
    if (rc != MODELRT_OK) {
        modelrt_set_error(rt, "runtime", rc, "could not open file");
        return rc;
    }

    /* Several native formats legitimately share the same extension (.emd is
       the important case: RE1 and RE3).  A probe is only a routing hint, not
       proof.  Keep all candidates and, when a stronger probe later rejects
       the file as FORMAT/CODEC, fall through to the next candidate instead of
       turning a false-positive sniff into a user-visible BAD FORMAT. */
    for (i = 0u; i < MODELRT_MAX_PROVIDERS; ++i) {
        probe_score[i] = MODELRT_PROBE_NO;
        attempted[i] = 0u;
    }
    for (i = 0u; i < rt->provider_count; ++i) {
        const modelrt_provider *p;
        p = rt->providers[i];
        if (p != 0 && p->probe != 0) probe_score[i] = p->probe(path, head, head_size);
    }

    first_fail_rc = MODELRT_ERR_UNSUPPORTED;
    first_fail_error[0] = '\0';

    for (attempt = 0u; attempt < rt->provider_count; ++attempt) {
        best_score = MODELRT_PROBE_NO;
        best_index = -1;
        best = 0;
        for (i = 0u; i < rt->provider_count; ++i) {
            if (attempted[i] != 0u) continue;
            if (probe_score[i] > best_score) {
                best_score = probe_score[i];
                best_index = (int)i;
                best = rt->providers[i];
            }
        }
        if (best == 0 || best_index < 0 || best_score <= MODELRT_PROBE_NO) break;
        attempted[(modelrt_u32)best_index] = 1u;

        handle->workspace_used = 0u;
        handle->native_state = 0;
        handle->provider = best;
        handle->last_codec_error = 0;
        handle->is_open = 0;
        modelrt_zero(&handle->info, sizeof(handle->info));
        modelrt_copy_string(handle->path, sizeof(handle->path), path);
        modelrt_copy_string(rt->last_error, sizeof(rt->last_error), "OK");

        rc = best->open(rt, handle, path);
        if (rc == MODELRT_OK) {
            handle->info.codec_id = best->codec_id;
            modelrt_copy_string(handle->info.codec_name, sizeof(handle->info.codec_name), best->name);
            handle->is_open = 1;
            modelrt_copy_string(rt->last_error, sizeof(rt->last_error), "OK");
            return MODELRT_OK;
        }

        if (first_fail_error[0] == '\0') {
            first_fail_rc = rc;
            if (rt->last_error[0] != '\0' && strcmp(rt->last_error, "OK") != 0)
                modelrt_copy_string(first_fail_error, sizeof(first_fail_error), rt->last_error);
            else {
                modelrt_set_error(rt, best->name, rc, 0);
                modelrt_copy_string(first_fail_error, sizeof(first_fail_error), rt->last_error);
            }
        }

        handle->workspace_used = 0u;
        handle->native_state = 0;
        handle->provider = 0;
        handle->is_open = 0;
        handle->last_codec_error = 0;
        modelrt_zero(&handle->info, sizeof(handle->info));

        /* IO/space/argument failures are not format ambiguity.  Do not mask
           them by trying unrelated codecs. */
        if (rc != MODELRT_ERR_FORMAT && rc != MODELRT_ERR_CODEC &&
            rc != MODELRT_ERR_UNSUPPORTED)
            break;
    }

    if (first_fail_error[0] != '\0') {
        modelrt_copy_string(rt->last_error, sizeof(rt->last_error), first_fail_error);
        return first_fail_rc;
    }
    modelrt_set_error(rt, "runtime", MODELRT_ERR_UNSUPPORTED, "no provider recognized this file");
    return MODELRT_ERR_UNSUPPORTED;
}

void modelrt_close(modelrt_handle *handle)
{
    if (handle == 0) return;
    if (handle->is_open && handle->provider != 0 && handle->provider->close != 0)
        handle->provider->close(handle);
    handle->provider = 0;
    handle->native_state = 0;
    handle->workspace_used = 0u;
    handle->is_open = 0;
    handle->last_codec_error = 0;
    modelrt_zero(&handle->info, sizeof(handle->info));
    handle->path[0] = '\0';
}

const modelrt_model_info *modelrt_get_info(const modelrt_handle *handle)
{
    if (handle == 0 || !handle->is_open) return 0;
    return &handle->info;
}

modelrt_u32 modelrt_get_capabilities(const modelrt_handle *handle)
{
    if (handle == 0 || !handle->is_open) return 0u;
    return handle->info.capabilities;
}

const char *modelrt_get_provider_name(const modelrt_handle *handle)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0) return "none";
    return handle->provider->name;
}

const char *modelrt_last_error(const modelrt_runtime *rt)
{
    if (rt == 0) return "bad runtime";
    return rt->last_error;
}

const char *modelrt_error_string(int code)
{
    switch (code) {
    case MODELRT_OK: return "OK";
    case MODELRT_ERR_BAD_ARG: return "bad argument";
    case MODELRT_ERR_IO: return "I/O error";
    case MODELRT_ERR_FORMAT: return "bad format";
    case MODELRT_ERR_UNSUPPORTED: return "unsupported";
    case MODELRT_ERR_NO_SPACE: return "workspace exhausted";
    case MODELRT_ERR_CODEC: return "codec error";
    case MODELRT_ERR_LIMIT: return "runtime limit hit";
    case MODELRT_ERR_CALLBACK: return "visitor callback stopped";
    default: return "unknown error";
    }
}

int modelrt_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                  const modelrt_visit_sink *sink, void *user)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0 || sink == 0)
        return MODELRT_ERR_BAD_ARG;
    if (handle->provider->visit == 0) return MODELRT_ERR_UNSUPPORTED;
    return handle->provider->visit(handle, animation_index, time, sink, user);
}

int modelrt_animation_count(const modelrt_handle *handle)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0) return 0;
    if (handle->provider->animation_count == 0) return (int)handle->info.animation_count;
    return handle->provider->animation_count(handle);
}

int modelrt_animation_info_get(modelrt_handle *handle, int animation_index,
                               modelrt_animation_info *out_info)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0 || out_info == 0)
        return MODELRT_ERR_BAD_ARG;
    if (handle->provider->animation_info == 0) return MODELRT_ERR_UNSUPPORTED;
    return handle->provider->animation_info(handle, animation_index, out_info);
}

int modelrt_animation_frame_info_get(modelrt_handle *handle, int animation_index,
                                     modelrt_u32 frame_index,
                                     modelrt_animation_frame_info *out_info)
{
    modelrt_animation_info ai;
    modelrt_i32 fi;
    if (out_info == 0) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_info, sizeof(*out_info));
    if (frame_index > 0x7fffffffu) return MODELRT_ERR_LIMIT;
    {
        int rc;
        rc = modelrt_animation_info_get(handle, animation_index, &ai);
        if (rc != MODELRT_OK) return rc;
    }
    if (ai.frame_count == 0u || frame_index >= ai.frame_count) return MODELRT_ERR_BAD_ARG;
    fi = (modelrt_i32)frame_index;
    if (ai.first_tick > 2147483647 - fi) return MODELRT_ERR_LIMIT;
    out_info->frame_index = frame_index;
    out_info->tick = ai.first_tick + fi;
    if ((ai.flags & MODELRT_ANIM_TICK_RATE_KNOWN) != 0u && ai.ticks_per_second != 0u &&
        ai.ticks_per_second <= 32767u && out_info->tick >= -32768 && out_info->tick <= 32767) {
        out_info->time = modelrt_q16_div(modelrt_q16_from_int(out_info->tick),
                                         modelrt_q16_from_int((modelrt_i32)ai.ticks_per_second));
        out_info->flags |= MODELRT_ANIM_FRAME_TIME_KNOWN;
    }
    return MODELRT_OK;
}

int modelrt_visit_animation_frame(modelrt_handle *handle, int animation_index,
                                  modelrt_u32 frame_index,
                                  const modelrt_visit_sink *sink, void *user)
{
    modelrt_animation_frame_info fi;
    int rc;
    if (handle == 0 || !handle->is_open || handle->provider == 0 || sink == 0 ||
        animation_index < 0) return MODELRT_ERR_BAD_ARG;
    if (handle->provider->visit_animation_frame != 0)
        return handle->provider->visit_animation_frame(handle, animation_index,
                                                       frame_index, sink, user);
    rc = modelrt_animation_frame_info_get(handle, animation_index, frame_index, &fi);
    if (rc != MODELRT_OK) return rc;
    if ((fi.flags & MODELRT_ANIM_FRAME_TIME_KNOWN) == 0u)
        return MODELRT_ERR_UNSUPPORTED;
    return modelrt_visit(handle, animation_index, fi.time, sink, user);
}

void modelrt_animation_binding_init(modelrt_animation_binding *binding,
                                    void *workspace, size_t workspace_size)
{
    if (binding == 0) return;
    modelrt_zero(binding, sizeof(*binding));
    binding->workspace = (modelrt_u8 *)workspace;
    binding->workspace_size = workspace_size;
}

void modelrt_animation_unbind(modelrt_animation_binding *binding)
{
    modelrt_u8 *workspace;
    size_t workspace_size;
    if (binding == 0) return;
    workspace = binding->workspace;
    workspace_size = binding->workspace_size;
    modelrt_zero(binding, sizeof(*binding));
    binding->workspace = workspace;
    binding->workspace_size = workspace_size;
}

int modelrt_animation_bind(modelrt_animation_binding *binding,
                           modelrt_handle *target, modelrt_handle *source)
{
    int rc;
    if (binding == 0 || target == 0 || source == 0 || target == source)
        return MODELRT_ERR_BAD_ARG;
    if (!target->is_open || !source->is_open || target->provider == 0 || source->provider == 0)
        return MODELRT_ERR_BAD_ARG;
    if (binding->workspace == 0 || binding->workspace_size == 0u) return MODELRT_ERR_NO_SPACE;
    modelrt_animation_unbind(binding);
    binding->target = target;
    binding->source = source;
    binding->info.target_codec_id = target->info.codec_id;
    binding->info.source_codec_id = source->info.codec_id;
    binding->info.source_animation_count = (modelrt_u32)modelrt_animation_count(source);
    binding->info.target_bone_count = target->info.bone_count;
    modelrt_copy_string(binding->info.source_name, sizeof(binding->info.source_name), source->info.name);
    if (binding->info.source_animation_count == 0u) return MODELRT_ERR_UNSUPPORTED;
    if (target->provider->animation_bind == 0 || target->provider->visit_bound_animation == 0)
        return MODELRT_ERR_UNSUPPORTED;
    rc = target->provider->animation_bind(target, source, binding);
    if (rc != MODELRT_OK) { modelrt_animation_unbind(binding); return rc; }
    binding->is_bound = 1;
    return MODELRT_OK;
}

const modelrt_animation_binding_info *modelrt_animation_binding_get_info(
    const modelrt_animation_binding *binding)
{
    if (binding == 0 || !binding->is_bound) return 0;
    return &binding->info;
}

int modelrt_animation_bound_count(const modelrt_animation_binding *binding)
{
    if (binding == 0 || !binding->is_bound || binding->source == 0) return 0;
    return modelrt_animation_count(binding->source);
}

int modelrt_animation_bound_info_get(modelrt_animation_binding *binding,
                                     int animation_index,
                                     modelrt_animation_info *out_info)
{
    if (binding == 0 || !binding->is_bound || binding->source == 0) return MODELRT_ERR_BAD_ARG;
    return modelrt_animation_info_get(binding->source, animation_index, out_info);
}

int modelrt_visit_bound_animation(modelrt_animation_binding *binding,
                                  int animation_index, modelrt_q16 time,
                                  const modelrt_visit_sink *sink, void *user)
{
    if (binding == 0 || !binding->is_bound || binding->target == 0 ||
        binding->target->provider == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    if (binding->target->provider->visit_bound_animation == 0) return MODELRT_ERR_UNSUPPORTED;
    return binding->target->provider->visit_bound_animation(binding, animation_index, time, sink, user);
}

int modelrt_texture_count(const modelrt_handle *handle)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0) return 0;
    if (handle->provider->texture_count == 0) return (int)handle->info.texture_count;
    return handle->provider->texture_count(handle);
}

int modelrt_texture_info_get(modelrt_handle *handle, int texture_index,
                             modelrt_texture_info *out_info)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0 || out_info == 0)
        return MODELRT_ERR_BAD_ARG;
    if (handle->provider->texture_info == 0) return MODELRT_ERR_UNSUPPORTED;
    return handle->provider->texture_info(handle, texture_index, out_info);
}

static int modelrt_file_exists89(const char *path)
{
    FILE *f;
    if (path == 0 || path[0] == '\0') return 0;
    f = fopen(path, "rb");
    if (f == 0) return 0;
    fclose(f);
    return 1;
}

static int modelrt_path_absolute89(const char *path)
{
    if (path == 0 || path[0] == '\0') return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    return path[0] != '\0' && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
}

static void modelrt_dirname89(const char *path, char *dst, size_t cap)
{
    size_t n;
    size_t i;
    size_t cut;
    if (dst == 0 || cap == 0u) return;
    dst[0] = '\0';
    if (path == 0) return;
    n = strlen(path);
    cut = 0u;
    for (i = 0u; i < n; ++i)
        if (path[i] == '/' || path[i] == '\\') cut = i + 1u;
    if (cut >= cap) cut = cap - 1u;
    for (i = 0u; i < cut; ++i) dst[i] = path[i];
    dst[cut] = '\0';
}

static const char *modelrt_basename89(const char *path)
{
    const char *p;
    const char *base;
    if (path == 0) return "";
    base = path;
    p = path;
    while (*p != '\0') {
        if (*p == '/' || *p == '\\') base = p + 1;
        ++p;
    }
    return base;
}

static void modelrt_join89(char *dst, size_t cap, const char *a, const char *b)
{
    size_t n;
    if (dst == 0 || cap == 0u) return;
    dst[0] = '\0';
    modelrt_copy_string(dst, cap, a != 0 ? a : "");
    n = strlen(dst);
    if (n != 0u && dst[n - 1u] != '/' && dst[n - 1u] != '\\' && n + 1u < cap) {
        dst[n++] = '/';
        dst[n] = '\0';
    }
    if (b != 0 && n + 1u < cap) {
        size_t i;
        i = 0u;
        while (b[i] != '\0' && n + i + 1u < cap) {
            dst[n + i] = b[i] == '\\' ? '/' : b[i];
            ++i;
        }
        dst[n + i] = '\0';
    }
}

static int modelrt_decode_image_path89(modelrt_handle *handle, const char *path,
                                       modelrt_u8 *dst, size_t dst_capacity,
                                       modelrt_u32 *out_width, modelrt_u32 *out_height,
                                       size_t *out_bytes)
{
    size_t mark;
    modelrt_u8 *encoded;
    size_t encoded_size;
    modelrt_image_info ii;
    int rc;
    if (!modelrt_file_exists89(path)) return MODELRT_ERR_IO;
    mark = handle->workspace_used;
    encoded = 0;
    encoded_size = 0u;
    rc = modelrt_read_file_arena(handle, path, &encoded, &encoded_size);
    if (rc == MODELRT_OK) {
        rc = modelrt_image_decode_memory_mask(handle->image_decoder_mask, encoded, encoded_size,
                                              dst, dst_capacity, &ii);
        if (rc == MODELRT_OK) {
            if (out_width != 0) *out_width = ii.width;
            if (out_height != 0) *out_height = ii.height;
            if (out_bytes != 0) *out_bytes = (size_t)ii.rgba_bytes;
        }
    }
    handle->workspace_used = mark;
    return rc;
}

static int modelrt_decode_external_texture89(modelrt_handle *handle,
                                             const modelrt_texture_info *ti,
                                             modelrt_u8 *dst, size_t dst_capacity,
                                             modelrt_u32 *out_width, modelrt_u32 *out_height,
                                             size_t *out_bytes)
{
    static const char *folders[] = {
        "", "textures", "Textures", "texture", "Texture",
        "maps", "Maps", "images", "Images"
    };
    char dir[MODELRT_PATH_MAX];
    char cand[MODELRT_PATH_MAX * 2];
    const char *refs[3];
    const char *base;
    unsigned int i;
    unsigned int j;
    int rc;
    refs[0] = ti->source_path;
    refs[1] = ti->relative_path;
    refs[2] = ti->path;
    modelrt_dirname89(handle->path, dir, sizeof(dir));
    for (i = 0u; i < 3u; ++i) {
        if (refs[i] == 0 || refs[i][0] == '\0') continue;
        if (modelrt_path_absolute89(refs[i])) {
            rc = modelrt_decode_image_path89(handle, refs[i], dst, dst_capacity,
                                             out_width, out_height, out_bytes);
            if (rc == MODELRT_OK) return rc;
        } else {
            modelrt_join89(cand, sizeof(cand), dir, refs[i]);
            rc = modelrt_decode_image_path89(handle, cand, dst, dst_capacity,
                                             out_width, out_height, out_bytes);
            if (rc == MODELRT_OK) return rc;
        }
    }
    base = modelrt_basename89(ti->relative_path[0] != '\0' ? ti->relative_path :
                              (ti->source_path[0] != '\0' ? ti->source_path : ti->path));
    if (base[0] != '\0') {
        for (j = 0u; j < (unsigned int)(sizeof(folders) / sizeof(folders[0])); ++j) {
            if (folders[j][0] == '\0') modelrt_join89(cand, sizeof(cand), dir, base);
            else {
                char sub[MODELRT_PATH_MAX];
                modelrt_join89(sub, sizeof(sub), dir, folders[j]);
                modelrt_join89(cand, sizeof(cand), sub, base);
            }
            rc = modelrt_decode_image_path89(handle, cand, dst, dst_capacity,
                                             out_width, out_height, out_bytes);
            if (rc == MODELRT_OK) return rc;
        }
    }
    return MODELRT_ERR_IO;
}

int modelrt_texture_decode_rgba(modelrt_handle *handle, int texture_index,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_u32 *out_width, modelrt_u32 *out_height,
                                size_t *out_bytes)
{
    int rc;
    int native_rc;
    modelrt_texture_blob_view blob;
    modelrt_texture_info ti;
    modelrt_image_info ii;
    if (handle == 0 || !handle->is_open || handle->provider == 0 || dst == 0)
        return MODELRT_ERR_BAD_ARG;
    native_rc = MODELRT_ERR_UNSUPPORTED;
    if (handle->provider->texture_decode_rgba != 0) {
        native_rc = handle->provider->texture_decode_rgba(handle, texture_index, dst, dst_capacity,
                                                           out_width, out_height, out_bytes);
        if (native_rc == MODELRT_OK) return MODELRT_OK;
        if (native_rc == MODELRT_ERR_NO_SPACE || native_rc == MODELRT_ERR_BAD_ARG)
            return native_rc;
    }
    blob.data = 0;
    blob.size = 0u;
    blob.flags = 0u;
    if (handle->provider->texture_encoded_view != 0 &&
        handle->provider->texture_encoded_view(handle, texture_index, &blob) == MODELRT_OK &&
        blob.data != 0 && blob.size != 0u) {
        rc = modelrt_image_decode_memory_mask(handle->image_decoder_mask, blob.data, blob.size,
                                              dst, dst_capacity, &ii);
        if (rc == MODELRT_OK) {
            if (out_width != 0) *out_width = ii.width;
            if (out_height != 0) *out_height = ii.height;
            if (out_bytes != 0) *out_bytes = (size_t)ii.rgba_bytes;
            return MODELRT_OK;
        }
        if (rc == MODELRT_ERR_NO_SPACE) return rc;
    }
    modelrt_zero(&ti, sizeof(ti));
    if (handle->provider->texture_info != 0 &&
        handle->provider->texture_info(handle, texture_index, &ti) == MODELRT_OK &&
        ti.kind == MODELRT_TEXTURE_EXTERNAL) {
        rc = modelrt_decode_external_texture89(handle, &ti, dst, dst_capacity,
                                               out_width, out_height, out_bytes);
        if (rc == MODELRT_OK || rc == MODELRT_ERR_NO_SPACE) return rc;
    }
    return native_rc;
}

int modelrt_texture_encoded_view_get(modelrt_handle *handle, int texture_index,
                                     modelrt_texture_blob_view *out_view)
{
    if (handle == 0 || !handle->is_open || handle->provider == 0 || out_view == 0)
        return MODELRT_ERR_BAD_ARG;
    out_view->data = 0;
    out_view->size = 0u;
    out_view->flags = 0u;
    if (handle->provider->texture_encoded_view == 0) return MODELRT_ERR_UNSUPPORTED;
    return handle->provider->texture_encoded_view(handle, texture_index, out_view);
}

modelrt_q16 modelrt_q16_from_int(modelrt_i32 v)
{
    if (v > 32767) return 2147483647;
    if (v < -32768) return (-2147483647 - 1);
    return (modelrt_q16)(v << 16);
}

modelrt_i32 modelrt_q16_to_int(modelrt_q16 v)
{
    return (modelrt_i32)(v / MODELRT_Q16_ONE);
}

modelrt_q16 modelrt_q16_mul(modelrt_q16 a, modelrt_q16 b)
{
#if defined(_MSC_VER)
    __int64 x;
    x = (__int64)a * (__int64)b;
    x >>= 16;
    if (x > 2147483647i64) return 2147483647;
    if (x < (-2147483647i64 - 1i64)) return (-2147483647 - 1);
    return (modelrt_q16)x;
#else
    __extension__ typedef long long modelrt_i64_local;
    modelrt_i64_local x;
    x = (modelrt_i64_local)a * (modelrt_i64_local)b;
    x >>= 16;
    if (x > ((modelrt_i64_local)2147483647)) return 2147483647;
    if (x < (-((modelrt_i64_local)2147483647) - ((modelrt_i64_local)1))) return (-2147483647 - 1);
    return (modelrt_q16)x;
#endif
}

modelrt_q16 modelrt_q16_div(modelrt_q16 a, modelrt_q16 b)
{
#if defined(_MSC_VER)
    __int64 x;
    if (b == 0) return 0;
    x = ((__int64)a << 16) / (__int64)b;
    if (x > 2147483647i64) return 2147483647;
    if (x < (-2147483647i64 - 1i64)) return (-2147483647 - 1);
    return (modelrt_q16)x;
#else
    __extension__ typedef long long modelrt_i64_local;
    modelrt_i64_local x;
    if (b == 0) return 0;
    x = ((modelrt_i64_local)a << 16) / (modelrt_i64_local)b;
    if (x > ((modelrt_i64_local)2147483647)) return 2147483647;
    if (x < (-((modelrt_i64_local)2147483647) - ((modelrt_i64_local)1))) return (-2147483647 - 1);
    return (modelrt_q16)x;
#endif
}
