#include "xps_internal.h"

static int xps_internal_callback_result(int rc)
{
    if (rc == 0) {
        return XPS_OK;
    }
    return XPS_ERR_CALLBACK;
}

const char *xps_error_string(int err)
{
    switch (err) {
    case XPS_OK:
        return "ok";
    case XPS_ERR_OPEN:
        return "open failed";
    case XPS_ERR_IO:
        return "io error";
    case XPS_ERR_FORMAT:
        return "invalid or unsupported format";
    case XPS_ERR_LIMIT:
        return "compile-time limit exceeded";
    case XPS_ERR_PARSE:
        return "parse error";
    case XPS_ERR_UNSUPPORTED:
        return "unsupported feature";
    case XPS_ERR_CALLBACK:
        return "callback aborted";
    case XPS_ERR_OVERFLOW:
        return "numeric overflow";
    case XPS_ERR_LINE_TOO_LONG:
        return "ascii line too long";
    case XPS_ERR_BAD_ARGUMENT:
        return "bad argument";
    default:
        return "unknown error";
    }
}

const char *xps_format_string(enum xps_format fmt)
{
    switch (fmt) {
    case XPS_FMT_MESH_ASCII:
        return "mesh.ascii";
    case XPS_FMT_MESH_BIN:
        return "mesh";
    case XPS_FMT_XPS:
        return "xps";
    case XPS_FMT_POSE:
        return "pose";
    case XPS_FMT_BONEDICT:
        return "BoneDict";
    case XPS_FMT_SCENE:
        return "scene";
    default:
        return "unknown";
    }
}

xps_fx xps_fx_from_int(int value)
{
    if (value > 32767) {
        return (xps_fx)0x7FFFFFFFL;
    }
    if (value < -32768) {
        return (xps_fx)(-2147483647L - 1L);
    }
    return (xps_fx)((xps_i32)value << 16);
}

int xps_fx_to_int_floor(xps_fx value)
{
    return (int)(value >> 16);
}

int xps_fx_to_int_round(xps_fx value)
{
    if (value >= 0) {
        return (int)((value + 0x8000L) >> 16);
    }
    return (int)(-(((-value) + 0x8000L) >> 16));
}

void xps_internal_copy_capped(char *dst, size_t cap, const char *src)
{
    size_t i;

    if (cap == 0U) {
        return;
    }
    i = 0U;
    while (src[i] != '\0' && i + 1U < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

void xps_internal_zero_header_info(xps_header_info *header)
{
    if (header != NULL) {
        memset(header, 0, sizeof(*header));
        header->format = XPS_FMT_XPS;
        header->magic_number = XPS_MAGIC_NUMBER;
    }
}

void xps_internal_header_flags_default(xps_header_flags *flags)
{
    if (flags != NULL) {
        memset(flags, 0, sizeof(*flags));
        flags->model_cast_shadows = 1U;
        flags->tangent_space_green = 1U;
        flags->gloss_raw = 10U;
        flags->gloss_is_nonzero = 1U;
    }
}

void xps_internal_zero_scan(xps_scan_result *scan)
{
    if (scan != NULL) {
        memset(scan, 0, sizeof(*scan));
    }
}

int xps_internal_invoke_header_begin(const xps_header_sink *sink, void *user, const xps_header_info *header)
{
    if (sink != NULL && sink->on_header_begin != NULL) {
        return xps_internal_callback_result(sink->on_header_begin(user, header));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_option(const xps_header_sink *sink, void *user, const xps_header_option *option)
{
    if (sink != NULL && sink->on_header_option != NULL) {
        return xps_internal_callback_result(sink->on_header_option(user, option));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_flag_pair(const xps_header_sink *sink, void *user, const xps_header_flag_pair *pair)
{
    if (sink != NULL && sink->on_header_flag_pair != NULL) {
        return xps_internal_callback_result(sink->on_header_flag_pair(user, pair));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_flags(const xps_header_sink *sink, void *user, const xps_header_flags *flags)
{
    if (sink != NULL && sink->on_header_flags != NULL) {
        return xps_internal_callback_result(sink->on_header_flags(user, flags));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_default_pose_begin(const xps_header_sink *sink, void *user, const xps_header_pose_info *pose)
{
    if (sink != NULL && sink->on_header_default_pose_begin != NULL) {
        return xps_internal_callback_result(sink->on_header_default_pose_begin(user, pose));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_default_pose_bone(const xps_header_sink *sink, void *user, const xps_pose_bone *bone)
{
    if (sink != NULL && sink->on_header_default_pose_bone != NULL) {
        return xps_internal_callback_result(sink->on_header_default_pose_bone(user, bone));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_default_pose_end(const xps_header_sink *sink, void *user, const xps_header_pose_info *pose)
{
    if (sink != NULL && sink->on_header_default_pose_end != NULL) {
        return xps_internal_callback_result(sink->on_header_default_pose_end(user, pose));
    }
    return XPS_OK;
}

int xps_internal_invoke_header_end(const xps_header_sink *sink, void *user, const xps_header_info *header)
{
    if (sink != NULL && sink->on_header_end != NULL) {
        return xps_internal_callback_result(sink->on_header_end(user, header));
    }
    return XPS_OK;
}

int xps_internal_invoke_model_begin(const xps_sink *sink, void *user, const xps_model_info *model)
{
    if (sink != NULL && sink->on_model_begin != NULL) {
        return xps_internal_callback_result(sink->on_model_begin(user, model));
    }
    return XPS_OK;
}

int xps_internal_invoke_model_end(const xps_sink *sink, void *user, const xps_model_info *model)
{
    if (sink != NULL && sink->on_model_end != NULL) {
        return xps_internal_callback_result(sink->on_model_end(user, model));
    }
    return XPS_OK;
}

int xps_internal_invoke_bone(const xps_sink *sink, void *user, const xps_bone *bone)
{
    if (sink != NULL && sink->on_bone != NULL) {
        return xps_internal_callback_result(sink->on_bone(user, bone));
    }
    return XPS_OK;
}

int xps_internal_invoke_mesh_begin(const xps_sink *sink, void *user, const xps_mesh_info *mesh)
{
    if (sink != NULL && sink->on_mesh_begin != NULL) {
        return xps_internal_callback_result(sink->on_mesh_begin(user, mesh));
    }
    return XPS_OK;
}

int xps_internal_invoke_texture(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_texture *texture)
{
    if (sink != NULL && sink->on_texture != NULL) {
        return xps_internal_callback_result(sink->on_texture(user, mesh_index, texture));
    }
    return XPS_OK;
}

int xps_internal_invoke_vertex(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_vertex *vertex)
{
    if (sink != NULL && sink->on_vertex != NULL) {
        return xps_internal_callback_result(sink->on_vertex(user, mesh_index, vertex));
    }
    return XPS_OK;
}

int xps_internal_invoke_face(const xps_sink *sink, void *user, xps_u32 mesh_index, const xps_face *face)
{
    if (sink != NULL && sink->on_face != NULL) {
        return xps_internal_callback_result(sink->on_face(user, mesh_index, face));
    }
    return XPS_OK;
}

int xps_internal_invoke_mesh_end(const xps_sink *sink, void *user, const xps_mesh_info *mesh)
{
    if (sink != NULL && sink->on_mesh_end != NULL) {
        return xps_internal_callback_result(sink->on_mesh_end(user, mesh));
    }
    return XPS_OK;
}

int xps_internal_invoke_pose_begin(const xps_pose_sink *sink, void *user, const xps_pose_info *pose)
{
    if (sink != NULL && sink->on_pose_begin != NULL) {
        return xps_internal_callback_result(sink->on_pose_begin(user, pose));
    }
    return XPS_OK;
}

int xps_internal_invoke_pose_bone(const xps_pose_sink *sink, void *user, const xps_pose_bone *bone)
{
    if (sink != NULL && sink->on_pose_bone != NULL) {
        return xps_internal_callback_result(sink->on_pose_bone(user, bone));
    }
    return XPS_OK;
}

int xps_internal_invoke_pose_end(const xps_pose_sink *sink, void *user, const xps_pose_info *pose)
{
    if (sink != NULL && sink->on_pose_end != NULL) {
        return xps_internal_callback_result(sink->on_pose_end(user, pose));
    }
    return XPS_OK;
}

int xps_internal_invoke_bonedict_begin(const xps_bonedict_sink *sink, void *user, const xps_bonedict_info *dict)
{
    if (sink != NULL && sink->on_bonedict_begin != NULL) {
        return xps_internal_callback_result(sink->on_bonedict_begin(user, dict));
    }
    return XPS_OK;
}

int xps_internal_invoke_bonedict_entry(const xps_bonedict_sink *sink, void *user, const xps_bonedict_entry *entry)
{
    if (sink != NULL && sink->on_bonedict_entry != NULL) {
        return xps_internal_callback_result(sink->on_bonedict_entry(user, entry));
    }
    return XPS_OK;
}

int xps_internal_invoke_bonedict_end(const xps_bonedict_sink *sink, void *user, const xps_bonedict_info *dict)
{
    if (sink != NULL && sink->on_bonedict_end != NULL) {
        return xps_internal_callback_result(sink->on_bonedict_end(user, dict));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_begin(const xps_scene_sink *sink, void *user, const xps_scene_info *scene)
{
    if (sink != NULL && sink->on_scene_begin != NULL) {
        return xps_internal_callback_result(sink->on_scene_begin(user, scene));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_item_begin(const xps_scene_sink *sink, void *user, const xps_scene_item *item)
{
    if (sink != NULL && sink->on_scene_item_begin != NULL) {
        return xps_internal_callback_result(sink->on_scene_item_begin(user, item));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_item_bone(const xps_scene_sink *sink, void *user, const xps_scene_bone_pose *bone)
{
    if (sink != NULL && sink->on_scene_item_bone != NULL) {
        return xps_internal_callback_result(sink->on_scene_item_bone(user, bone));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_accessory(const xps_scene_sink *sink, void *user, const xps_scene_accessory *accessory)
{
    if (sink != NULL && sink->on_scene_accessory != NULL) {
        return xps_internal_callback_result(sink->on_scene_accessory(user, accessory));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_item_end(const xps_scene_sink *sink, void *user, const xps_scene_item *item)
{
    if (sink != NULL && sink->on_scene_item_end != NULL) {
        return xps_internal_callback_result(sink->on_scene_item_end(user, item));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_camera(const xps_scene_sink *sink, void *user, const xps_scene_camera *camera)
{
    if (sink != NULL && sink->on_scene_camera != NULL) {
        return xps_internal_callback_result(sink->on_scene_camera(user, camera));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_light(const xps_scene_sink *sink, void *user, const xps_scene_light *light)
{
    if (sink != NULL && sink->on_scene_light != NULL) {
        return xps_internal_callback_result(sink->on_scene_light(user, light));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_post(const xps_scene_sink *sink, void *user, const xps_scene_post *post)
{
    if (sink != NULL && sink->on_scene_post != NULL) {
        return xps_internal_callback_result(sink->on_scene_post(user, post));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_background(const xps_scene_sink *sink, void *user, const xps_scene_background *background)
{
    if (sink != NULL && sink->on_scene_background != NULL) {
        return xps_internal_callback_result(sink->on_scene_background(user, background));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_sky(const xps_scene_sink *sink, void *user, const xps_scene_sky *sky)
{
    if (sink != NULL && sink->on_scene_sky != NULL) {
        return xps_internal_callback_result(sink->on_scene_sky(user, sky));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_window(const xps_scene_sink *sink, void *user, const xps_scene_window *window_info)
{
    if (sink != NULL && sink->on_scene_window != NULL) {
        return xps_internal_callback_result(sink->on_scene_window(user, window_info));
    }
    return XPS_OK;
}

int xps_internal_invoke_scene_end(const xps_scene_sink *sink, void *user, const xps_scene_info *scene)
{
    if (sink != NULL && sink->on_scene_end != NULL) {
        return xps_internal_callback_result(sink->on_scene_end(user, scene));
    }
    return XPS_OK;
}
