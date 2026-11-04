#include "ase3d.h"

/* No malloc, no stdio, no atof/strtod, no floats/doubles. */

#define ASE3D_BLOCK_UNKNOWN     0
#define ASE3D_BLOCK_GEOM        1
#define ASE3D_BLOCK_MATERIAL    2
#define ASE3D_BLOCK_NODETM      3
#define ASE3D_BLOCK_MAPPING     4
#define ASE3D_BLOCK_MESH        5
#define ASE3D_BLOCK_NORMALS     6
#define ASE3D_BLOCK_TMANIM      7
#define ASE3D_BLOCK_POS_TRACK   8
#define ASE3D_BLOCK_ROT_TRACK   9
#define ASE3D_BLOCK_SCALE_TRACK 10
#define ASE3D_BLOCK_MAP         11
#define ASE3D_BLOCK_LIGHTSET    12
#define ASE3D_BLOCK_CAMERASET   13

#define ASE3D_TRACK_NONE        0
#define ASE3D_TRACK_POS         1
#define ASE3D_TRACK_ROT         2
#define ASE3D_TRACK_SCALE       3

#define ASE3D_PI_FIXED       205887L
#define ASE3D_TWO_PI_FIXED   411775L
#define ASE3D_HALF_PI_FIXED  102944L
#define ASE3D_SLERP_CLOSE_DOT 65470L

static int ase3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int ase3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int ase3d_is_value_start(char c)
{
    return ase3d_is_digit(c) || c == '-' || c == '+' || c == '.';
}


static const char *ase3d_skip_ws(const char *s)
{
    while (*s && ase3d_is_space(*s)) s++;
    return s;
}

static int ase3d_has_more_value(const char *s)
{
    s = ase3d_skip_ws(s);
    return ase3d_is_value_start(*s);
}

static int ase3d_token_boundary(char c)
{
    return c == 0 || ase3d_is_space(c) || c == '{' || c == '}' || c == ':';
}

static int ase3d_starts_token(const char *s, const char *tok)
{
    const char *a;
    const char *b;
    a = ase3d_skip_ws(s);
    b = tok;
    while (*b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return ase3d_token_boundary(*a);
}

static const char *ase3d_after_token(const char *s, const char *tok)
{
    const char *a;
    const char *b;
    a = ase3d_skip_ws(s);
    b = tok;
    while (*b && *a == *b) {
        a++;
        b++;
    }
    if (*b) return (const char *)0;
    if (!ase3d_token_boundary(*a)) return (const char *)0;
    return ase3d_skip_ws(a);
}

static const char *ase3d_find_substr(const char *s, const char *needle)
{
    const char *p;
    const char *a;
    const char *b;
    for (p = s; *p; ++p) {
        a = p;
        b = needle;
        while (*a && *b && *a == *b) {
            a++;
            b++;
        }
        if (!*b) return p;
    }
    return (const char *)0;
}

static int ase3d_has_char(const char *s, char ch)
{
    while (*s) {
        if (*s == ch) return 1;
        s++;
    }
    return 0;
}

static long ase3d_parse_int_ptr(const char **ps)
{
    const char *s;
    long sign;
    long v;
    s = ase3d_skip_ws(*ps);
    sign = 1L;
    if (*s == '-') { sign = -1L; s++; }
    else if (*s == '+') { s++; }
    v = 0L;
    while (ase3d_is_digit(*s)) {
        v = (v * 10L) + (long)(*s - '0');
        s++;
    }
    *ps = s;
    return v * sign;
}

static ase3d_fixed ase3d_parse_fixed_ptr(const char **ps)
{
    const char *s;
    long sign;
    long ip;
    long fp;
    long place;
    int digits;
    long exp_sign;
    long exp_val;
    ase3d_fixed v;

    s = ase3d_skip_ws(*ps);
    sign = 1L;
    if (*s == '-') { sign = -1L; s++; }
    else if (*s == '+') { s++; }

    ip = 0L;
    while (ase3d_is_digit(*s)) {
        ip = (ip * 10L) + (long)(*s - '0');
        s++;
    }

    fp = 0L;
    place = ASE3D_FIXED_ONE / 10L;
    digits = 0;
    if (*s == '.') {
        s++;
        while (ase3d_is_digit(*s)) {
            if (digits < 8 && place > 0L) {
                fp += ((long)(*s - '0')) * place;
                place /= 10L;
            }
            digits++;
            s++;
        }
    }

    v = (ip << ASE3D_FIXED_SHIFT) + fp;

    if (*s == 'e' || *s == 'E') {
        s++;
        exp_sign = 1L;
        if (*s == '-') { exp_sign = -1L; s++; }
        else if (*s == '+') { s++; }
        exp_val = 0L;
        while (ase3d_is_digit(*s)) {
            exp_val = exp_val * 10L + (long)(*s - '0');
            s++;
        }
        while (exp_val > 0L) {
            if (exp_sign > 0L) v *= 10L;
            else v /= 10L;
            exp_val--;
        }
    }

    *ps = s;
    if (sign < 0L) v = -v;
    return v;
}

static void ase3d_zero_chars(char *dst, unsigned int cap)
{
    unsigned int i;
    if (!dst) return;
    for (i = 0U; i < cap; ++i) dst[i] = 0;
}

static void ase3d_copy_quoted(char *dst, unsigned int cap, const char *s)
{
    unsigned int n;
    const char *q;
    if (!dst || cap == 0U) return;
    dst[0] = 0;
    q = s;
    while (*q && *q != '"') q++;
    if (*q == '"') q++;
    n = 0U;
    while (*q && *q != '"' && n + 1U < cap) {
        dst[n++] = *q++;
    }
    dst[n] = 0;
}

static int ase3d_find_label_int(const char *line, const char *label, int fallback)
{
    const char *p;
    p = ase3d_find_substr(line, label);
    if (!p) return fallback;
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    return (int)ase3d_parse_int_ptr(&p);
}

static int ase3d_find_token_int(const char *line, const char *tok, int fallback)
{
    const char *p;
    p = ase3d_find_substr(line, tok);
    if (!p) return fallback;
    while (*p && !ase3d_is_space(*p)) p++;
    return (int)ase3d_parse_int_ptr(&p);
}

static ase3d_u32 ase3d_parse_smoothing(const char *line)
{
    const char *p;
    long n;
    ase3d_u32 mask;
    p = ase3d_find_substr(line, "*MESH_SMOOTHING");
    if (!p) return 0UL;
    p += 15;
    mask = 0UL;
    for (;;) {
        p = ase3d_skip_ws(p);
        if (!ase3d_is_digit(*p)) break;
        n = ase3d_parse_int_ptr(&p);
        if (n > 0L && n <= 32L) mask |= (1UL << (unsigned int)(n - 1L));
        p = ase3d_skip_ws(p);
        if (*p != ',') break;
        p++;
    }
    return mask;
}

static void ase3d_vec3_zero(ase3d_vec3 *v)
{
    v->x = 0L; v->y = 0L; v->z = 0L;
}

static void ase3d_rgb_zero(ase3d_rgb *v)
{
    v->r = 0L; v->g = 0L; v->b = 0L;
}

static void ase3d_scene_zero(ase3d_scene *s)
{
    ase3d_zero_chars(s->filename, ASE3D_PATH_MAX);
    s->first_frame = 0L;
    s->last_frame = 0L;
    s->frame_speed = 30L;
    s->ticks_per_frame = 160L;
    ase3d_rgb_zero(&s->background);
    ase3d_rgb_zero(&s->ambient);
    s->has_background = 0;
    s->has_ambient = 0;
}

static void ase3d_tm_identity(ase3d_node_tm *tm)
{
    ase3d_vec3_zero(&tm->row0);
    ase3d_vec3_zero(&tm->row1);
    ase3d_vec3_zero(&tm->row2);
    ase3d_vec3_zero(&tm->row3);
    ase3d_vec3_zero(&tm->pos);
    ase3d_vec3_zero(&tm->rot_axis);
    ase3d_vec3_zero(&tm->scale);
    ase3d_vec3_zero(&tm->scale_axis);
    tm->row0.x = ASE3D_FIXED_ONE;
    tm->row1.y = ASE3D_FIXED_ONE;
    tm->row2.z = ASE3D_FIXED_ONE;
    tm->scale.x = ASE3D_FIXED_ONE;
    tm->scale.y = ASE3D_FIXED_ONE;
    tm->scale.z = ASE3D_FIXED_ONE;
    tm->rot_angle = 0L;
    tm->scale_axis_angle = 0L;
    tm->has_row0 = 0;
    tm->has_row1 = 0;
    tm->has_row2 = 0;
    tm->has_row3 = 0;
    tm->has_pos = 0;
    tm->has_rot = 0;
    tm->has_scale = 0;
    tm->has_scale_axis = 0;
}

static void ase3d_material_zero(ase3d_material *mat)
{
    mat->index = -1;
    mat->parent_index = -1;
    ase3d_zero_chars(mat->name, ASE3D_NAME_MAX);
    ase3d_zero_chars(mat->class_name, ASE3D_NAME_MAX);
    ase3d_zero_chars(mat->bitmap, ASE3D_PATH_MAX);
    ase3d_zero_chars(mat->map_type, ASE3D_NAME_MAX);
    ase3d_rgb_zero(&mat->ambient);
    ase3d_rgb_zero(&mat->diffuse);
    ase3d_rgb_zero(&mat->specular);
    ase3d_rgb_zero(&mat->color);
    mat->shine = 0L;
    mat->shine_strength = 0L;
    mat->transparency = 0L;
    mat->has_bitmap = 0;
    mat->num_submaterials = 0;
}

static void ase3d_bounds_zero(ase3d_bounds *b)
{
    if (!b) return;
    ase3d_vec3_zero(&b->min);
    ase3d_vec3_zero(&b->max);
    b->valid = 0;
}

static void ase3d_map_zero(ase3d_texture_map *map)
{
    map->material_index = -1;
    map->parent_material_index = -1;
    map->kind = ASE3D_MAP_UNKNOWN;
    ase3d_zero_chars(map->name, ASE3D_NAME_MAX);
    ase3d_zero_chars(map->class_name, ASE3D_NAME_MAX);
    ase3d_zero_chars(map->bitmap, ASE3D_PATH_MAX);
    ase3d_zero_chars(map->texture, ASE3D_NAME_MAX);
    map->amount = ASE3D_FIXED_ONE;
    map->u_offset = 0L;
    map->v_offset = 0L;
    map->u_tiling = ASE3D_FIXED_ONE;
    map->v_tiling = ASE3D_FIXED_ONE;
    map->angle = 0L;
    map->blur = 0L;
    map->invert = 0;
    map->has_bitmap = 0;
}

static void ase3d_light_zero(ase3d_light *lt)
{
    lt->object_id = -1;
    ase3d_zero_chars(lt->type_name, ASE3D_NAME_MAX);
    lt->timevalue = 0L;
    ase3d_rgb_zero(&lt->color);
    lt->intensity = ASE3D_FIXED_ONE;
    lt->contrast = 0L;
    lt->soften_diff_edge = 0L;
    lt->hotspot = 0L;
    lt->falloff = 0L;
    lt->aspect = ASE3D_FIXED_ONE;
    lt->attenuation_start = 0L;
    lt->attenuation_end = 0L;
    lt->map_bias = 0L;
    lt->map_size = 0L;
    lt->map_range = 0L;
    lt->casts_shadows = 0;
    lt->has_color = 0;
}

static void ase3d_camera_zero(ase3d_camera *cam)
{
    cam->object_id = -1;
    ase3d_zero_chars(cam->target_name, ASE3D_NAME_MAX);
    cam->timevalue = 0L;
    cam->fov = 0L;
    cam->near_range = 0L;
    cam->far_range = 0L;
    cam->target_distance = 0L;
    cam->hither = 0L;
    cam->yon = 0L;
    cam->has_fov = 0;
}

static void ase3d_object_zero(ase3d_object *obj)
{
    ase3d_zero_chars(obj->name, ASE3D_NAME_MAX);
    ase3d_zero_chars(obj->parent_name, ASE3D_NAME_MAX);
    obj->type = ASE3D_OBJECT_GEOM;
    obj->material_ref = -1;
    obj->first_vertex = -1;
    obj->vertex_count = 0;
    obj->first_face = -1;
    obj->face_count = 0;
    obj->first_tvert = -1;
    obj->tvert_count = 0;
    obj->first_tface = -1;
    obj->tface_count = 0;
    obj->first_cvert = -1;
    obj->cvert_count = 0;
    obj->first_cface = -1;
    obj->cface_count = 0;
    obj->first_mesh_span = -1;
    obj->mesh_span_count = 0;
    obj->first_pos_key = -1;
    obj->pos_key_count = 0;
    obj->first_rot_key = -1;
    obj->rot_key_count = 0;
    obj->first_scale_key = -1;
    obj->scale_key_count = 0;
    ase3d_bounds_zero(&obj->local_bounds);
    ase3d_bounds_zero(&obj->world_bounds);
    ase3d_tm_identity(&obj->tm);
}

static void ase3d_mesh_span_zero(ase3d_mesh_span *sp)
{
    sp->object_id = -1;
    sp->timevalue = 0L;
    sp->source_mesh_index = 0;
    sp->first_vertex = -1;
    sp->vertex_count = 0;
    sp->first_face = -1;
    sp->face_count = 0;
    sp->first_tvert = -1;
    sp->tvert_count = 0;
    sp->first_tface = -1;
    sp->tface_count = 0;
    sp->first_cvert = -1;
    sp->cvert_count = 0;
    sp->first_cface = -1;
    sp->cface_count = 0;
}

void ase3d_model_zero(ase3d_model *m)
{
    if (!m) return;
    ase3d_scene_zero(&m->scene);
    m->vertices = (ase3d_vec3 *)0; m->vertex_capacity = 0U; m->vertex_count = 0U;
    m->faces = (ase3d_face *)0; m->face_capacity = 0U; m->face_count = 0U;
    m->tverts = (ase3d_vec3 *)0; m->tvert_capacity = 0U; m->tvert_count = 0U;
    m->tfaces = (ase3d_tface *)0; m->tface_capacity = 0U; m->tface_count = 0U;
    m->cverts = (ase3d_rgb *)0; m->cvert_capacity = 0U; m->cvert_count = 0U;
    m->cfaces = (ase3d_cface *)0; m->cface_capacity = 0U; m->cface_count = 0U;
    m->materials = (ase3d_material *)0; m->material_capacity = 0U; m->material_count = 0U;
    m->maps = (ase3d_texture_map *)0; m->map_capacity = 0U; m->map_count = 0U;
    m->lights = (ase3d_light *)0; m->light_capacity = 0U; m->light_count = 0U;
    m->cameras = (ase3d_camera *)0; m->camera_capacity = 0U; m->camera_count = 0U;
    m->objects = (ase3d_object *)0; m->object_capacity = 0U; m->object_count = 0U;
    m->mesh_spans = (ase3d_mesh_span *)0; m->mesh_span_capacity = 0U; m->mesh_span_count = 0U;
    m->face_normals = (ase3d_vec3 *)0; m->face_normal_capacity = 0U; m->face_normal_count = 0U;
    m->corner_normals = (ase3d_corner_normal *)0; m->corner_normal_capacity = 0U; m->corner_normal_count = 0U;
    m->pos_keys = (ase3d_pos_key *)0; m->pos_key_capacity = 0U; m->pos_key_count = 0U;
    m->rot_keys = (ase3d_rot_key *)0; m->rot_key_capacity = 0U; m->rot_key_count = 0U;
    m->scale_keys = (ase3d_scale_key *)0; m->scale_key_capacity = 0U; m->scale_key_count = 0U;
    m->warnings = 0UL;
    m->parsed_lines = 0UL;
}

void ase3d_parser_init(ase3d_parser *p, ase3d_model *m)
{
    int i;
    if (!p) return;
    p->model = m;
    p->current_object = -1;
    p->current_material = -1;
    p->parent_material = -1;
    p->current_mapping_channel = 1;
    p->current_normal_face = -1;
    p->current_normal_local_face = -1;
    p->current_normal_corner = 0;
    p->current_mesh_span = -1;
    p->current_anim_track = ASE3D_TRACK_NONE;
    p->current_key_kind = ASE3D_KEY_LINEAR;
    p->current_map_block = 0;
    p->current_map = -1;
    p->current_light = -1;
    p->current_camera = -1;
    p->stack_top = 0;
    p->line = 0UL;
    for (i = 0; i < ASE3D_STACK_MAX; ++i) {
        p->stack[i].type = ASE3D_BLOCK_UNKNOWN;
        p->stack[i].saved_object = -1;
        p->stack[i].saved_material = -1;
        p->stack[i].saved_mapping_channel = 1;
        p->stack[i].saved_parent_material = -1;
        p->stack[i].saved_mesh_span = -1;
        p->stack[i].saved_anim_track = ASE3D_TRACK_NONE;
        p->stack[i].saved_key_kind = ASE3D_KEY_LINEAR;
        p->stack[i].saved_map = -1;
        p->stack[i].saved_light = -1;
        p->stack[i].saved_camera = -1;
    }
}

static int ase3d_top_block(const ase3d_parser *p)
{
    if (!p || p->stack_top <= 0) return ASE3D_BLOCK_UNKNOWN;
    return p->stack[p->stack_top - 1].type;
}

static int ase3d_push_block(ase3d_parser *p, int type)
{
    ase3d_block *b;
    if (p->stack_top >= ASE3D_STACK_MAX) {
        if (p->model) p->model->warnings |= ASE3D_WARN_STACK_OVERFLOW;
        return ASE3D_ERR_STACK_OVER;
    }
    b = &p->stack[p->stack_top++];
    b->type = type;
    b->saved_object = p->current_object;
    b->saved_material = p->current_material;
    b->saved_mapping_channel = p->current_mapping_channel;
    b->saved_parent_material = p->parent_material;
    b->saved_mesh_span = p->current_mesh_span;
    b->saved_anim_track = p->current_anim_track;
    b->saved_key_kind = p->current_key_kind;
    b->saved_map = p->current_map;
    b->saved_light = p->current_light;
    b->saved_camera = p->current_camera;
    return ASE3D_OK;
}

static int ase3d_pop_block(ase3d_parser *p)
{
    ase3d_block *b;
    if (p->stack_top <= 0) return ASE3D_ERR_STACK_UNDER;
    b = &p->stack[--p->stack_top];
    if (b->type == ASE3D_BLOCK_GEOM) {
        p->current_object = b->saved_object;
        p->current_light = b->saved_light;
        p->current_camera = b->saved_camera;
    } else if (b->type == ASE3D_BLOCK_MATERIAL) {
        p->current_material = b->saved_material;
        p->parent_material = b->saved_parent_material;
    } else if (b->type == ASE3D_BLOCK_MAPPING) {
        p->current_mapping_channel = b->saved_mapping_channel;
    } else if (b->type == ASE3D_BLOCK_NORMALS) {
        p->current_normal_face = -1;
        p->current_normal_local_face = -1;
        p->current_normal_corner = 0;
    } else if (b->type == ASE3D_BLOCK_MESH) {
        p->current_mesh_span = b->saved_mesh_span;
    } else if (b->type == ASE3D_BLOCK_POS_TRACK || b->type == ASE3D_BLOCK_ROT_TRACK || b->type == ASE3D_BLOCK_SCALE_TRACK) {
        p->current_anim_track = b->saved_anim_track;
        p->current_key_kind = b->saved_key_kind;
    } else if (b->type == ASE3D_BLOCK_MAP) {
        p->current_map_block = 0;
        p->current_map = b->saved_map;
    } else if (b->type == ASE3D_BLOCK_LIGHTSET) {
        p->current_light = b->saved_light;
    } else if (b->type == ASE3D_BLOCK_CAMERASET) {
        p->current_camera = b->saved_camera;
    }
    return ASE3D_OK;
}

static int ase3d_add_object(ase3d_model *m, int type)
{
    unsigned int i;
    ase3d_object *obj;
    if (!m || !m->objects || m->object_count >= m->object_capacity) {
        if (m) m->warnings |= ASE3D_WARN_OBJ_OVERFLOW;
        return -1;
    }
    i = m->object_count++;
    obj = &m->objects[i];
    ase3d_object_zero(obj);
    obj->type = type;
    obj->first_vertex = (int)m->vertex_count;
    obj->first_face = (int)m->face_count;
    obj->first_tvert = (int)m->tvert_count;
    obj->first_tface = (int)m->tface_count;
    obj->first_cvert = (int)m->cvert_count;
    obj->first_cface = (int)m->cface_count;
    obj->first_mesh_span = (int)m->mesh_span_count;
    obj->first_pos_key = (int)m->pos_key_count;
    obj->first_rot_key = (int)m->rot_key_count;
    obj->first_scale_key = (int)m->scale_key_count;
    return (int)i;
}

static void ase3d_refresh_object_counts(ase3d_model *m, int object_id)
{
    ase3d_object *obj;
    if (!m || object_id < 0 || (unsigned int)object_id >= m->object_count) return;
    obj = &m->objects[object_id];
    if (obj->first_vertex >= 0) obj->vertex_count = (int)m->vertex_count - obj->first_vertex;
    if (obj->first_face >= 0) obj->face_count = (int)m->face_count - obj->first_face;
    if (obj->first_tvert >= 0) obj->tvert_count = (int)m->tvert_count - obj->first_tvert;
    if (obj->first_tface >= 0) obj->tface_count = (int)m->tface_count - obj->first_tface;
    if (obj->first_cvert >= 0) obj->cvert_count = (int)m->cvert_count - obj->first_cvert;
    if (obj->first_cface >= 0) obj->cface_count = (int)m->cface_count - obj->first_cface;
    if (obj->first_mesh_span >= 0) obj->mesh_span_count = (int)m->mesh_span_count - obj->first_mesh_span;
    if (obj->first_pos_key >= 0) obj->pos_key_count = (int)m->pos_key_count - obj->first_pos_key;
    if (obj->first_rot_key >= 0) obj->rot_key_count = (int)m->rot_key_count - obj->first_rot_key;
    if (obj->first_scale_key >= 0) obj->scale_key_count = (int)m->scale_key_count - obj->first_scale_key;
}

static void ase3d_refresh_mesh_span_counts(ase3d_model *m, int mesh_span_id)
{
    ase3d_mesh_span *sp;
    if (!m || mesh_span_id < 0 || (unsigned int)mesh_span_id >= m->mesh_span_count) return;
    sp = &m->mesh_spans[mesh_span_id];
    if (sp->first_vertex >= 0) sp->vertex_count = (int)m->vertex_count - sp->first_vertex;
    if (sp->first_face >= 0) sp->face_count = (int)m->face_count - sp->first_face;
    if (sp->first_tvert >= 0) sp->tvert_count = (int)m->tvert_count - sp->first_tvert;
    if (sp->first_tface >= 0) sp->tface_count = (int)m->tface_count - sp->first_tface;
    if (sp->first_cvert >= 0) sp->cvert_count = (int)m->cvert_count - sp->first_cvert;
    if (sp->first_cface >= 0) sp->cface_count = (int)m->cface_count - sp->first_cface;
}

static int ase3d_add_mesh_span(ase3d_parser *p)
{
    ase3d_model *m;
    ase3d_mesh_span *sp;
    ase3d_object *obj;
    unsigned int i;
    int span_count;
    if (!p) return -1;
    m = p->model;
    if (!m || !m->mesh_spans || m->mesh_span_count >= m->mesh_span_capacity) {
        if (m) m->warnings |= ASE3D_WARN_MESHSPAN_OVERFLOW;
        return -1;
    }
    i = m->mesh_span_count++;
    sp = &m->mesh_spans[i];
    ase3d_mesh_span_zero(sp);
    sp->object_id = p->current_object;
    sp->first_vertex = (int)m->vertex_count;
    sp->first_face = (int)m->face_count;
    sp->first_tvert = (int)m->tvert_count;
    sp->first_tface = (int)m->tface_count;
    sp->first_cvert = (int)m->cvert_count;
    sp->first_cface = (int)m->cface_count;
    if (p->current_object >= 0 && (unsigned int)p->current_object < m->object_count) {
        obj = &m->objects[p->current_object];
        span_count = obj->mesh_span_count;
        sp->source_mesh_index = span_count;
    }
    ase3d_refresh_object_counts(m, p->current_object);
    return (int)i;
}

static int ase3d_add_material(ase3d_parser *p, int declared_index, int parent_index)
{
    ase3d_model *m;
    unsigned int i;
    if (!p) return -1;
    m = p->model;
    if (!m || !m->materials || m->material_count >= m->material_capacity) {
        if (m) m->warnings |= ASE3D_WARN_MAT_OVERFLOW;
        return -1;
    }
    i = m->material_count++;
    ase3d_material_zero(&m->materials[i]);
    m->materials[i].index = declared_index;
    m->materials[i].parent_index = parent_index;
    return (int)i;
}

static int ase3d_add_map(ase3d_parser *p, int kind)
{
    ase3d_model *m;
    unsigned int i;
    if (!p) return -1;
    m = p->model;
    if (!m || !m->maps || m->map_count >= m->map_capacity) {
        if (m) m->warnings |= ASE3D_WARN_MAP_OVERFLOW;
        return -1;
    }
    i = m->map_count++;
    ase3d_map_zero(&m->maps[i]);
    m->maps[i].material_index = p->current_material;
    m->maps[i].parent_material_index = p->parent_material;
    m->maps[i].kind = kind;
    return (int)i;
}

static int ase3d_add_light(ase3d_model *m, int object_id)
{
    unsigned int i;
    if (!m || !m->lights || m->light_count >= m->light_capacity) {
        if (m) m->warnings |= ASE3D_WARN_LIGHT_OVERFLOW;
        return -1;
    }
    i = m->light_count++;
    ase3d_light_zero(&m->lights[i]);
    m->lights[i].object_id = object_id;
    return (int)i;
}

static int ase3d_add_camera(ase3d_model *m, int object_id)
{
    unsigned int i;
    if (!m || !m->cameras || m->camera_count >= m->camera_capacity) {
        if (m) m->warnings |= ASE3D_WARN_CAMERA_OVERFLOW;
        return -1;
    }
    i = m->camera_count++;
    ase3d_camera_zero(&m->cameras[i]);
    m->cameras[i].object_id = object_id;
    return (int)i;
}

static int ase3d_find_object_by_name(ase3d_model *m, const char *quoted_or_plain)
{
    char name[ASE3D_NAME_MAX];
    unsigned int i;
    if (!m || !quoted_or_plain) return -1;
    ase3d_copy_quoted(name, ASE3D_NAME_MAX, quoted_or_plain);
    if (name[0] == 0) return -1;
    for (i = 0U; i < m->object_count; ++i) {
        if (ase3d_find_substr(m->objects[i].name, name) == m->objects[i].name && m->objects[i].name[0] == name[0]) {
            const char *a;
            const char *b;
            a = m->objects[i].name;
            b = name;
            while (*a && *b && *a == *b) { a++; b++; }
            if (*a == 0 && *b == 0) return (int)i;
        }
    }
    return -1;
}

static void ase3d_parse_vec3(const char *p, ase3d_vec3 *out)
{
    out->x = ase3d_parse_fixed_ptr(&p);
    out->y = ase3d_parse_fixed_ptr(&p);
    out->z = ase3d_parse_fixed_ptr(&p);
}

static void ase3d_parse_rgb(const char *p, ase3d_rgb *out)
{
    out->r = ase3d_parse_fixed_ptr(&p);
    out->g = ase3d_parse_fixed_ptr(&p);
    out->b = ase3d_parse_fixed_ptr(&p);
}

static int ase3d_current_vertex_base(const ase3d_parser *p)
{
    const ase3d_model *m;
    if (!p || !p->model) return 0;
    m = p->model;
    if (p->current_mesh_span >= 0 && (unsigned int)p->current_mesh_span < m->mesh_span_count) {
        return m->mesh_spans[p->current_mesh_span].first_vertex;
    }
    if (p->current_object >= 0 && (unsigned int)p->current_object < m->object_count) {
        return m->objects[p->current_object].first_vertex;
    }
    return 0;
}

static int ase3d_current_tvert_base(const ase3d_parser *p)
{
    const ase3d_model *m;
    if (!p || !p->model) return 0;
    m = p->model;
    if (p->current_mesh_span >= 0 && (unsigned int)p->current_mesh_span < m->mesh_span_count) {
        return m->mesh_spans[p->current_mesh_span].first_tvert;
    }
    if (p->current_object >= 0 && (unsigned int)p->current_object < m->object_count) {
        return m->objects[p->current_object].first_tvert;
    }
    return 0;
}

static int ase3d_current_cvert_base(const ase3d_parser *p)
{
    const ase3d_model *m;
    if (!p || !p->model) return 0;
    m = p->model;
    if (p->current_mesh_span >= 0 && (unsigned int)p->current_mesh_span < m->mesh_span_count) {
        return m->mesh_spans[p->current_mesh_span].first_cvert;
    }
    if (p->current_object >= 0 && (unsigned int)p->current_object < m->object_count) {
        return m->objects[p->current_object].first_cvert;
    }
    return 0;
}

static int ase3d_current_face_base(const ase3d_parser *p)
{
    const ase3d_model *m;
    if (!p || !p->model) return 0;
    m = p->model;
    if (p->current_mesh_span >= 0 && (unsigned int)p->current_mesh_span < m->mesh_span_count) {
        return m->mesh_spans[p->current_mesh_span].first_face;
    }
    if (p->current_object >= 0 && (unsigned int)p->current_object < m->object_count) {
        return m->objects[p->current_object].first_face;
    }
    return 0;
}

static void ase3d_parse_scene_line(ase3d_parser *p, const char *line)
{
    ase3d_model *m;
    const char *a;
    if (!p || !p->model) return;
    m = p->model;
    a = ase3d_after_token(line, "*SCENE_FILENAME");
    if (a) { ase3d_copy_quoted(m->scene.filename, ASE3D_PATH_MAX, a); return; }
    a = ase3d_after_token(line, "*SCENE_FIRSTFRAME");
    if (a) { m->scene.first_frame = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*SCENE_LASTFRAME");
    if (a) { m->scene.last_frame = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*SCENE_FRAMESPEED");
    if (a) { m->scene.frame_speed = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*SCENE_TICKSPERFRAME");
    if (a) { m->scene.ticks_per_frame = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*SCENE_BACKGROUND_STATIC");
    if (a) { ase3d_parse_rgb(a, &m->scene.background); m->scene.has_background = 1; return; }
    a = ase3d_after_token(line, "*SCENE_AMBIENT_STATIC");
    if (a) { ase3d_parse_rgb(a, &m->scene.ambient); m->scene.has_ambient = 1; return; }
}

static void ase3d_parse_material_line(ase3d_parser *p, const char *line)
{
    ase3d_model *m;
    ase3d_material *mat;
    const char *a;
    if (!p || !p->model || p->current_material < 0) return;
    m = p->model;
    if ((unsigned int)p->current_material >= m->material_count) return;
    mat = &m->materials[p->current_material];

    a = ase3d_after_token(line, "*MATERIAL_NAME");
    if (a) { ase3d_copy_quoted(mat->name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*MATERIAL_CLASS");
    if (a) { ase3d_copy_quoted(mat->class_name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*MATERIAL_AMBIENT");
    if (a) { ase3d_parse_rgb(a, &mat->ambient); return; }
    a = ase3d_after_token(line, "*MATERIAL_DIFFUSE");
    if (a) { ase3d_parse_rgb(a, &mat->diffuse); return; }
    a = ase3d_after_token(line, "*MATERIAL_SPECULAR");
    if (a) { ase3d_parse_rgb(a, &mat->specular); return; }
    a = ase3d_after_token(line, "*MATERIAL_COLOR");
    if (a) { ase3d_parse_rgb(a, &mat->color); return; }
    a = ase3d_after_token(line, "*MATERIAL_SHINESTRENGTH");
    if (a) { mat->shine_strength = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*MATERIAL_SHINE");
    if (a) { mat->shine = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*MATERIAL_TRANSPARENCY");
    if (a) { mat->transparency = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*NUMSUBMTLS");
    if (a) { mat->num_submaterials = (int)ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*BITMAP");
    if (a) { ase3d_copy_quoted(mat->bitmap, ASE3D_PATH_MAX, a); mat->has_bitmap = 1; return; }
}

static int ase3d_map_kind_from_line(const char *line)
{
    if (ase3d_starts_token(line, "*MAP_AMBIENT")) return ASE3D_MAP_AMBIENT;
    if (ase3d_starts_token(line, "*MAP_DIFFUSE") || ase3d_starts_token(line, "*MATERIAL_MAP_DIFFUSE")) return ASE3D_MAP_DIFFUSE;
    if (ase3d_starts_token(line, "*MAP_SPECULAR")) return ASE3D_MAP_SPECULAR;
    if (ase3d_starts_token(line, "*MAP_SHINE")) return ASE3D_MAP_SHINE;
    if (ase3d_starts_token(line, "*MAP_OPACITY")) return ASE3D_MAP_OPACITY;
    if (ase3d_starts_token(line, "*MAP_BUMP")) return ASE3D_MAP_BUMP;
    if (ase3d_starts_token(line, "*MAP_SELFILLUM")) return ASE3D_MAP_SELFILLUM;
    if (ase3d_starts_token(line, "*MAP_REFLECT")) return ASE3D_MAP_REFLECT;
    if (ase3d_starts_token(line, "*MAP_REFRACT")) return ASE3D_MAP_REFRACT;
    if (ase3d_starts_token(line, "*MAP_FILTER")) return ASE3D_MAP_FILTER;
    return ASE3D_MAP_UNKNOWN;
}

static void ase3d_parse_map_line(ase3d_parser *p, const char *line)
{
    ase3d_texture_map *map;
    ase3d_material *mat;
    ase3d_model *m;
    const char *a;
    if (!p || !p->model || p->current_map < 0) return;
    m = p->model;
    if ((unsigned int)p->current_map >= m->map_count) return;
    map = &m->maps[p->current_map];
    a = ase3d_after_token(line, "*MAP_NAME");
    if (a) { ase3d_copy_quoted(map->name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*MAP_CLASS");
    if (a) { ase3d_copy_quoted(map->class_name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*MAP_SUBNO");
    if (a) { (void)ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*MAP_AMOUNT");
    if (a) { map->amount = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*MAP_TEXTURE");
    if (a) { ase3d_copy_quoted(map->texture, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*BITMAP");
    if (a) {
        ase3d_copy_quoted(map->bitmap, ASE3D_PATH_MAX, a);
        map->has_bitmap = 1;
        if (p->current_material >= 0 && (unsigned int)p->current_material < m->material_count) {
            mat = &m->materials[p->current_material];
            if (!mat->has_bitmap && map->kind == ASE3D_MAP_DIFFUSE) {
                ase3d_copy_quoted(mat->bitmap, ASE3D_PATH_MAX, a);
                mat->has_bitmap = 1;
            }
        }
        return;
    }
    a = ase3d_after_token(line, "*UVW_U_OFFSET");
    if (a) { map->u_offset = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*UVW_V_OFFSET");
    if (a) { map->v_offset = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*UVW_U_TILING");
    if (a) { map->u_tiling = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*UVW_V_TILING");
    if (a) { map->v_tiling = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*UVW_ANGLE");
    if (a) { map->angle = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*MAP_BLUR");
    if (a) { map->blur = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*UVW_BLUR");
    if (a) { map->blur = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*MAP_INVERT");
    if (a) { map->invert = (int)ase3d_parse_int_ptr(&a); return; }
}

static void ase3d_parse_light_line(ase3d_parser *p, const char *line)
{
    ase3d_light *lt;
    const char *a;
    if (!p || !p->model || p->current_light < 0) return;
    if ((unsigned int)p->current_light >= p->model->light_count) return;
    lt = &p->model->lights[p->current_light];
    a = ase3d_after_token(line, "*TIMEVALUE");
    if (a) { lt->timevalue = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_TYPE");
    if (a) { ase3d_copy_quoted(lt->type_name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*LIGHT_COLOR");
    if (a) { ase3d_parse_rgb(a, &lt->color); lt->has_color = 1; return; }
    a = ase3d_after_token(line, "*LIGHT_INTENS");
    if (a) { lt->intensity = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_CONTRAST");
    if (a) { lt->contrast = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_SOFTEN_DIFF_EDGE");
    if (a) { lt->soften_diff_edge = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_HOTSPOT");
    if (a) { lt->hotspot = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_FALLOFF");
    if (a) { lt->falloff = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_ASPECT");
    if (a) { lt->aspect = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_ATTNSTART");
    if (a) { lt->attenuation_start = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_ATTNEND");
    if (a) { lt->attenuation_end = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_MAP_BIAS");
    if (a) { lt->map_bias = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_MAP_SIZE");
    if (a) { lt->map_size = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_MAP_RANGE");
    if (a) { lt->map_range = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*LIGHT_SHADOWS");
    if (a) { lt->casts_shadows = (int)ase3d_parse_int_ptr(&a); return; }
}

static void ase3d_parse_camera_line(ase3d_parser *p, const char *line)
{
    ase3d_camera *cam;
    const char *a;
    if (!p || !p->model || p->current_camera < 0) return;
    if ((unsigned int)p->current_camera >= p->model->camera_count) return;
    cam = &p->model->cameras[p->current_camera];
    a = ase3d_after_token(line, "*TIMEVALUE");
    if (a) { cam->timevalue = ase3d_parse_int_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_TARGET");
    if (a) { ase3d_copy_quoted(cam->target_name, ASE3D_NAME_MAX, a); return; }
    a = ase3d_after_token(line, "*CAMERA_FOV");
    if (a) { cam->fov = ase3d_parse_fixed_ptr(&a); cam->has_fov = 1; return; }
    a = ase3d_after_token(line, "*CAMERA_NEAR");
    if (a) { cam->near_range = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_FAR");
    if (a) { cam->far_range = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_NEAR_RANGE");
    if (a) { cam->near_range = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_FAR_RANGE");
    if (a) { cam->far_range = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_TDIST");
    if (a) { cam->target_distance = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_HITHER");
    if (a) { cam->hither = ase3d_parse_fixed_ptr(&a); return; }
    a = ase3d_after_token(line, "*CAMERA_YON");
    if (a) { cam->yon = ase3d_parse_fixed_ptr(&a); return; }
}

static void ase3d_parse_tm_line(ase3d_parser *p, const char *line)
{
    ase3d_model *m;
    ase3d_object *obj;
    const char *a;
    if (!p || !p->model || p->current_object < 0) return;
    m = p->model;
    if ((unsigned int)p->current_object >= m->object_count) return;
    obj = &m->objects[p->current_object];
    a = ase3d_after_token(line, "*TM_ROW0");
    if (a) { ase3d_parse_vec3(a, &obj->tm.row0); obj->tm.has_row0 = 1; return; }
    a = ase3d_after_token(line, "*TM_ROW1");
    if (a) { ase3d_parse_vec3(a, &obj->tm.row1); obj->tm.has_row1 = 1; return; }
    a = ase3d_after_token(line, "*TM_ROW2");
    if (a) { ase3d_parse_vec3(a, &obj->tm.row2); obj->tm.has_row2 = 1; return; }
    a = ase3d_after_token(line, "*TM_ROW3");
    if (a) { ase3d_parse_vec3(a, &obj->tm.row3); obj->tm.has_row3 = 1; return; }
    a = ase3d_after_token(line, "*TM_POS");
    if (a) { ase3d_parse_vec3(a, &obj->tm.pos); obj->tm.has_pos = 1; return; }
    a = ase3d_after_token(line, "*TM_ROTAXIS");
    if (a) { ase3d_parse_vec3(a, &obj->tm.rot_axis); obj->tm.has_rot = 1; return; }
    a = ase3d_after_token(line, "*TM_ROTANGLE");
    if (a) { obj->tm.rot_angle = ase3d_parse_fixed_ptr(&a); obj->tm.has_rot = 1; return; }
    a = ase3d_after_token(line, "*TM_SCALEAXISANG");
    if (a) { obj->tm.scale_axis_angle = ase3d_parse_fixed_ptr(&a); obj->tm.has_scale_axis = 1; return; }
    a = ase3d_after_token(line, "*TM_SCALEAXIS");
    if (a) { ase3d_parse_vec3(a, &obj->tm.scale_axis); obj->tm.has_scale_axis = 1; return; }
    a = ase3d_after_token(line, "*TM_SCALE");
    if (a) { ase3d_parse_vec3(a, &obj->tm.scale); obj->tm.has_scale = 1; return; }
}

static void ase3d_store_vertex(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    long idx;
    ase3d_vec3 v;
    m = p->model;
    idx = ase3d_parse_int_ptr(&a);
    v.x = ase3d_parse_fixed_ptr(&a);
    v.y = ase3d_parse_fixed_ptr(&a);
    v.z = ase3d_parse_fixed_ptr(&a);
    if (!m->vertices || m->vertex_count >= m->vertex_capacity) {
        m->warnings |= ASE3D_WARN_VERTEX_OVERFLOW;
        return;
    }
    m->vertices[m->vertex_count++] = v;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
    (void)idx;
}

static void ase3d_store_tvert(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_vec3 tv;
    long idx;
    m = p->model;
    idx = ase3d_parse_int_ptr(&a);
    tv.x = ase3d_parse_fixed_ptr(&a);
    tv.y = ase3d_parse_fixed_ptr(&a);
    tv.z = ase3d_parse_fixed_ptr(&a);
    if (!m->tverts || m->tvert_count >= m->tvert_capacity) {
        m->warnings |= ASE3D_WARN_TVERT_OVERFLOW;
        return;
    }
    m->tverts[m->tvert_count++] = tv;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
    (void)idx;
}

static void ase3d_store_face(ase3d_parser *p, const char *a, const char *line)
{
    ase3d_model *m;
    ase3d_face f;
    long idx;
    int base;
    m = p->model;
    idx = ase3d_parse_int_ptr(&a);
    if (*a == ':') a++;
    base = ase3d_current_vertex_base(p);
    f.index = (int)idx;
    f.local_a = ase3d_find_label_int(line, "A:", 0);
    f.local_b = ase3d_find_label_int(line, "B:", 0);
    f.local_c = ase3d_find_label_int(line, "C:", 0);
    f.a = base + f.local_a;
    f.b = base + f.local_b;
    f.c = base + f.local_c;
    f.edge_ab = ase3d_find_label_int(line, "AB:", 1);
    f.edge_bc = ase3d_find_label_int(line, "BC:", 1);
    f.edge_ca = ase3d_find_label_int(line, "CA:", 1);
    f.smoothing_mask = ase3d_parse_smoothing(line);
    f.material_id = ase3d_find_token_int(line, "*MESH_MTLID", -1);
    f.object_id = p->current_object;
    f.mesh_span_id = p->current_mesh_span;
    if (!m->faces || m->face_count >= m->face_capacity) {
        m->warnings |= ASE3D_WARN_FACE_OVERFLOW;
        return;
    }
    m->faces[m->face_count++] = f;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
}

static void ase3d_store_tface(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_tface f;
    int base;
    m = p->model;
    base = ase3d_current_tvert_base(p);
    f.index = (int)ase3d_parse_int_ptr(&a);
    f.local_a = (int)ase3d_parse_int_ptr(&a);
    f.local_b = (int)ase3d_parse_int_ptr(&a);
    f.local_c = (int)ase3d_parse_int_ptr(&a);
    f.a = base + f.local_a;
    f.b = base + f.local_b;
    f.c = base + f.local_c;
    f.channel = p->current_mapping_channel;
    f.object_id = p->current_object;
    f.mesh_span_id = p->current_mesh_span;
    if (!m->tfaces || m->tface_count >= m->tface_capacity) {
        m->warnings |= ASE3D_WARN_TFACE_OVERFLOW;
        return;
    }
    m->tfaces[m->tface_count++] = f;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
}

static void ase3d_store_cvert(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_rgb c;
    long idx;
    m = p->model;
    idx = ase3d_parse_int_ptr(&a);
    c.r = ase3d_parse_fixed_ptr(&a);
    c.g = ase3d_parse_fixed_ptr(&a);
    c.b = ase3d_parse_fixed_ptr(&a);
    if (!m->cverts || m->cvert_count >= m->cvert_capacity) {
        m->warnings |= ASE3D_WARN_CVERT_OVERFLOW;
        return;
    }
    m->cverts[m->cvert_count++] = c;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
    (void)idx;
}

static void ase3d_store_cface(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_cface f;
    int base;
    m = p->model;
    base = ase3d_current_cvert_base(p);
    f.index = (int)ase3d_parse_int_ptr(&a);
    f.local_a = (int)ase3d_parse_int_ptr(&a);
    f.local_b = (int)ase3d_parse_int_ptr(&a);
    f.local_c = (int)ase3d_parse_int_ptr(&a);
    f.a = base + f.local_a;
    f.b = base + f.local_b;
    f.c = base + f.local_c;
    f.object_id = p->current_object;
    f.mesh_span_id = p->current_mesh_span;
    if (!m->cfaces || m->cface_count >= m->cface_capacity) {
        m->warnings |= ASE3D_WARN_CFACE_OVERFLOW;
        return;
    }
    m->cfaces[m->cface_count++] = f;
    ase3d_refresh_object_counts(m, p->current_object);
    ase3d_refresh_mesh_span_counts(m, p->current_mesh_span);
}

static void ase3d_store_face_normal(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_vec3 n;
    int local_face_idx;
    int face_base;
    m = p->model;
    local_face_idx = (int)ase3d_parse_int_ptr(&a);
    ase3d_parse_vec3(a, &n);
    face_base = ase3d_current_face_base(p);
    p->current_normal_local_face = local_face_idx;
    p->current_normal_face = face_base + local_face_idx;
    p->current_normal_corner = 0;
    if (!m->face_normals || m->face_normal_count >= m->face_normal_capacity) {
        m->warnings |= ASE3D_WARN_NORMAL_OVERFLOW;
        return;
    }
    m->face_normals[m->face_normal_count++] = n;
}

static void ase3d_store_corner_normal(ase3d_parser *p, const char *a)
{
    ase3d_model *m;
    ase3d_corner_normal cn;
    int vbase;
    int lvi;
    m = p->model;
    vbase = ase3d_current_vertex_base(p);
    lvi = (int)ase3d_parse_int_ptr(&a);
    cn.face_index = p->current_normal_face;
    cn.local_face_index = p->current_normal_local_face;
    cn.vertex_index = vbase + lvi;
    cn.local_vertex_index = lvi;
    cn.corner = p->current_normal_corner;
    ase3d_parse_vec3(a, &cn.n);
    cn.object_id = p->current_object;
    cn.mesh_span_id = p->current_mesh_span;
    p->current_normal_corner++;
    if (!m->corner_normals || m->corner_normal_count >= m->corner_normal_capacity) {
        m->warnings |= ASE3D_WARN_CORNERN_OVERFLOW;
        return;
    }
    m->corner_normals[m->corner_normal_count++] = cn;
}

static void ase3d_store_pos_key(ase3d_parser *p, const char *a, int kind)
{
    ase3d_model *m;
    ase3d_pos_key k;
    m = p->model;
    k.object_id = p->current_object;
    k.tick = ase3d_parse_int_ptr(&a);
    k.pos.x = ase3d_parse_fixed_ptr(&a);
    k.pos.y = ase3d_parse_fixed_ptr(&a);
    k.pos.z = ase3d_parse_fixed_ptr(&a);
    ase3d_vec3_zero(&k.in_tangent);
    ase3d_vec3_zero(&k.out_tangent);
    k.tension = 0L;
    k.continuity = 0L;
    k.bias = 0L;
    k.ease_to = 0L;
    k.ease_from = 0L;
    k.kind = kind;
    k.has_in_tangent = 0;
    k.has_out_tangent = 0;
    k.has_tcb = 0;
    if (kind == ASE3D_KEY_TCB) {
        if (ase3d_has_more_value(a)) { k.tension = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.continuity = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.bias = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) k.ease_to = ase3d_parse_fixed_ptr(&a);
        if (ase3d_has_more_value(a)) k.ease_from = ase3d_parse_fixed_ptr(&a);
    } else if (kind == ASE3D_KEY_BEZIER) {
        if (ase3d_has_more_value(a)) {
            k.in_tangent.x = ase3d_parse_fixed_ptr(&a);
            k.in_tangent.y = ase3d_parse_fixed_ptr(&a);
            k.in_tangent.z = ase3d_parse_fixed_ptr(&a);
            k.has_in_tangent = 1;
        }
        if (ase3d_has_more_value(a)) {
            k.out_tangent.x = ase3d_parse_fixed_ptr(&a);
            k.out_tangent.y = ase3d_parse_fixed_ptr(&a);
            k.out_tangent.z = ase3d_parse_fixed_ptr(&a);
            k.has_out_tangent = 1;
        }
    }
    if (!m->pos_keys || m->pos_key_count >= m->pos_key_capacity) {
        m->warnings |= ASE3D_WARN_POSKEY_OVERFLOW;
        return;
    }
    m->pos_keys[m->pos_key_count++] = k;
    ase3d_refresh_object_counts(m, p->current_object);
}

static void ase3d_store_rot_key(ase3d_parser *p, const char *a, int kind)
{
    ase3d_model *m;
    ase3d_rot_key k;
    m = p->model;
    k.object_id = p->current_object;
    k.tick = ase3d_parse_int_ptr(&a);
    k.axis.x = ase3d_parse_fixed_ptr(&a);
    k.axis.y = ase3d_parse_fixed_ptr(&a);
    k.axis.z = ase3d_parse_fixed_ptr(&a);
    k.angle = ase3d_parse_fixed_ptr(&a);
    k.quat.x = 0L; k.quat.y = 0L; k.quat.z = 0L; k.quat.w = ASE3D_FIXED_ONE;
    k.tension = 0L;
    k.continuity = 0L;
    k.bias = 0L;
    k.ease_to = 0L;
    k.ease_from = 0L;
    k.kind = kind;
    k.has_quat = 0;
    k.has_tcb = 0;
    if (kind == ASE3D_KEY_TCB) {
        if (ase3d_has_more_value(a)) { k.tension = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.continuity = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.bias = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) k.ease_to = ase3d_parse_fixed_ptr(&a);
        if (ase3d_has_more_value(a)) k.ease_from = ase3d_parse_fixed_ptr(&a);
    }
    if (!m->rot_keys || m->rot_key_count >= m->rot_key_capacity) {
        m->warnings |= ASE3D_WARN_ROTKEY_OVERFLOW;
        return;
    }
    m->rot_keys[m->rot_key_count++] = k;
    ase3d_refresh_object_counts(m, p->current_object);
}

static void ase3d_store_scale_key(ase3d_parser *p, const char *a, int kind)
{
    ase3d_model *m;
    ase3d_scale_key k;
    m = p->model;
    k.object_id = p->current_object;
    k.tick = ase3d_parse_int_ptr(&a);
    k.scale.x = ase3d_parse_fixed_ptr(&a);
    k.scale.y = ase3d_parse_fixed_ptr(&a);
    k.scale.z = ase3d_parse_fixed_ptr(&a);
    k.axis.x = ase3d_parse_fixed_ptr(&a);
    k.axis.y = ase3d_parse_fixed_ptr(&a);
    k.axis.z = ase3d_parse_fixed_ptr(&a);
    k.angle = ase3d_parse_fixed_ptr(&a);
    ase3d_vec3_zero(&k.in_tangent);
    ase3d_vec3_zero(&k.out_tangent);
    k.tension = 0L;
    k.continuity = 0L;
    k.bias = 0L;
    k.ease_to = 0L;
    k.ease_from = 0L;
    k.kind = kind;
    k.has_in_tangent = 0;
    k.has_out_tangent = 0;
    k.has_tcb = 0;
    if (kind == ASE3D_KEY_TCB) {
        if (ase3d_has_more_value(a)) { k.tension = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.continuity = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) { k.bias = ase3d_parse_fixed_ptr(&a); k.has_tcb = 1; }
        if (ase3d_has_more_value(a)) k.ease_to = ase3d_parse_fixed_ptr(&a);
        if (ase3d_has_more_value(a)) k.ease_from = ase3d_parse_fixed_ptr(&a);
    } else if (kind == ASE3D_KEY_BEZIER) {
        if (ase3d_has_more_value(a)) {
            k.in_tangent.x = ase3d_parse_fixed_ptr(&a);
            k.in_tangent.y = ase3d_parse_fixed_ptr(&a);
            k.in_tangent.z = ase3d_parse_fixed_ptr(&a);
            k.has_in_tangent = 1;
        }
        if (ase3d_has_more_value(a)) {
            k.out_tangent.x = ase3d_parse_fixed_ptr(&a);
            k.out_tangent.y = ase3d_parse_fixed_ptr(&a);
            k.out_tangent.z = ase3d_parse_fixed_ptr(&a);
            k.has_out_tangent = 1;
        }
    }
    if (!m->scale_keys || m->scale_key_count >= m->scale_key_capacity) {
        m->warnings |= ASE3D_WARN_SCALEKEY_OVERFLOW;
        return;
    }
    m->scale_keys[m->scale_key_count++] = k;
    ase3d_refresh_object_counts(m, p->current_object);
}

static int ase3d_is_map_block_start(const char *line)
{
    if (ase3d_starts_token(line, "*MAP_DIFFUSE")) return 1;
    if (ase3d_starts_token(line, "*MAP_AMBIENT")) return 1;
    if (ase3d_starts_token(line, "*MAP_SPECULAR")) return 1;
    if (ase3d_starts_token(line, "*MAP_OPACITY")) return 1;
    if (ase3d_starts_token(line, "*MAP_BUMP")) return 1;
    if (ase3d_starts_token(line, "*MAP_SHINE")) return 1;
    if (ase3d_starts_token(line, "*MAP_SELFILLUM")) return 1;
    if (ase3d_starts_token(line, "*MAP_REFLECT")) return 1;
    if (ase3d_starts_token(line, "*MAP_REFRACT")) return 1;
    if (ase3d_starts_token(line, "*MAP_FILTER")) return 1;
    if (ase3d_starts_token(line, "*MATERIAL_MAP_DIFFUSE")) return 1;
    return 0;
}

static int ase3d_process_block_start(ase3d_parser *p, const char *line)
{
    ase3d_model *m;
    const char *a;
    int idx;
    int obj_type;
    m = p->model;
    if (!ase3d_has_char(line, '{')) return 0;

    obj_type = 0;
    if (ase3d_starts_token(line, "*GEOMOBJECT")) obj_type = ASE3D_OBJECT_GEOM;
    else if (ase3d_starts_token(line, "*HELPEROBJECT")) obj_type = ASE3D_OBJECT_HELPER;
    else if (ase3d_starts_token(line, "*LIGHTOBJECT")) obj_type = ASE3D_OBJECT_LIGHT;
    else if (ase3d_starts_token(line, "*CAMERAOBJECT")) obj_type = ASE3D_OBJECT_CAMERA;
    else if (ase3d_starts_token(line, "*SHAPEOBJECT")) obj_type = ASE3D_OBJECT_SHAPE;
    if (obj_type != 0) {
        if (ase3d_push_block(p, ASE3D_BLOCK_GEOM) != ASE3D_OK) return 1;
        p->current_object = ase3d_add_object(m, obj_type);
        p->current_light = -1;
        p->current_camera = -1;
        if (obj_type == ASE3D_OBJECT_LIGHT && p->current_object >= 0) p->current_light = ase3d_add_light(m, p->current_object);
        if (obj_type == ASE3D_OBJECT_CAMERA && p->current_object >= 0) p->current_camera = ase3d_add_camera(m, p->current_object);
        return 1;
    }
    if (ase3d_starts_token(line, "*NODE_TM")) {
        ase3d_push_block(p, ASE3D_BLOCK_NODETM);
        return 1;
    }
    if (ase3d_starts_token(line, "*TM_ANIMATION")) {
        ase3d_push_block(p, ASE3D_BLOCK_TMANIM);
        p->current_anim_track = ASE3D_TRACK_NONE;
        p->current_key_kind = ASE3D_KEY_LINEAR;
        return 1;
    }
    if (ase3d_starts_token(line, "*LIGHT_SETTINGS")) {
        ase3d_push_block(p, ASE3D_BLOCK_LIGHTSET);
        return 1;
    }
    if (ase3d_starts_token(line, "*CAMERA_SETTINGS")) {
        ase3d_push_block(p, ASE3D_BLOCK_CAMERASET);
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_POS_TRACK")) {
        ase3d_push_block(p, ASE3D_BLOCK_POS_TRACK);
        p->current_anim_track = ASE3D_TRACK_POS;
        p->current_key_kind = ASE3D_KEY_LINEAR;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_POS_BEZIER")) {
        ase3d_push_block(p, ASE3D_BLOCK_POS_TRACK);
        p->current_anim_track = ASE3D_TRACK_POS;
        p->current_key_kind = ASE3D_KEY_BEZIER;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_POS_TCB")) {
        ase3d_push_block(p, ASE3D_BLOCK_POS_TRACK);
        p->current_anim_track = ASE3D_TRACK_POS;
        p->current_key_kind = ASE3D_KEY_TCB;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_ROT_TRACK")) {
        ase3d_push_block(p, ASE3D_BLOCK_ROT_TRACK);
        p->current_anim_track = ASE3D_TRACK_ROT;
        p->current_key_kind = ASE3D_KEY_LINEAR;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_ROT_TCB")) {
        ase3d_push_block(p, ASE3D_BLOCK_ROT_TRACK);
        p->current_anim_track = ASE3D_TRACK_ROT;
        p->current_key_kind = ASE3D_KEY_TCB;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_ROT_BEZIER")) {
        ase3d_push_block(p, ASE3D_BLOCK_ROT_TRACK);
        p->current_anim_track = ASE3D_TRACK_ROT;
        p->current_key_kind = ASE3D_KEY_BEZIER;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_SCALE_TRACK")) {
        ase3d_push_block(p, ASE3D_BLOCK_SCALE_TRACK);
        p->current_anim_track = ASE3D_TRACK_SCALE;
        p->current_key_kind = ASE3D_KEY_LINEAR;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_SCALE_TCB")) {
        ase3d_push_block(p, ASE3D_BLOCK_SCALE_TRACK);
        p->current_anim_track = ASE3D_TRACK_SCALE;
        p->current_key_kind = ASE3D_KEY_TCB;
        return 1;
    }
    if (ase3d_starts_token(line, "*CONTROL_SCALE_BEZIER")) {
        ase3d_push_block(p, ASE3D_BLOCK_SCALE_TRACK);
        p->current_anim_track = ASE3D_TRACK_SCALE;
        p->current_key_kind = ASE3D_KEY_BEZIER;
        return 1;
    }
    if (ase3d_starts_token(line, "*MESH_MAPPINGCHANNEL")) {
        a = ase3d_after_token(line, "*MESH_MAPPINGCHANNEL");
        if (ase3d_push_block(p, ASE3D_BLOCK_MAPPING) == ASE3D_OK) {
            p->current_mapping_channel = (int)ase3d_parse_int_ptr(&a);
        }
        return 1;
    }
    if (ase3d_starts_token(line, "*MESH_NORMALS")) {
        ase3d_push_block(p, ASE3D_BLOCK_NORMALS);
        return 1;
    }
    if (ase3d_starts_token(line, "*MESH")) {
        if (!ase3d_starts_token(line, "*MESH_VERTEX_LIST") &&
            !ase3d_starts_token(line, "*MESH_FACE_LIST") &&
            !ase3d_starts_token(line, "*MESH_TVERTLIST") &&
            !ase3d_starts_token(line, "*MESH_TFACELIST") &&
            !ase3d_starts_token(line, "*MESH_CVERTLIST") &&
            !ase3d_starts_token(line, "*MESH_CFACELIST")) {
            ase3d_push_block(p, ASE3D_BLOCK_MESH);
            p->current_mesh_span = ase3d_add_mesh_span(p);
            return 1;
        }
    }
    if (ase3d_is_map_block_start(line)) {
        if (ase3d_push_block(p, ASE3D_BLOCK_MAP) == ASE3D_OK) {
            p->current_map_block = 1;
            p->current_map = ase3d_add_map(p, ase3d_map_kind_from_line(line));
            if (p->current_material >= 0 && (unsigned int)p->current_material < m->material_count) {
                a = ase3d_skip_ws(line);
                ase3d_copy_quoted(m->materials[p->current_material].map_type, ASE3D_NAME_MAX, a);
            }
        }
        return 1;
    }
    if (ase3d_starts_token(line, "*MATERIAL")) {
        if (!ase3d_starts_token(line, "*MATERIAL_LIST") &&
            !ase3d_starts_token(line, "*MATERIAL_MAP_DIFFUSE") &&
            !ase3d_starts_token(line, "*MATERIAL_NAME") &&
            !ase3d_starts_token(line, "*MATERIAL_CLASS")) {
            a = ase3d_after_token(line, "*MATERIAL");
            idx = (int)ase3d_parse_int_ptr(&a);
            if (ase3d_push_block(p, ASE3D_BLOCK_MATERIAL) == ASE3D_OK) {
                p->current_material = ase3d_add_material(p, idx, -1);
                p->parent_material = p->current_material;
            }
            return 1;
        }
    }
    if (ase3d_starts_token(line, "*SUBMATERIAL")) {
        a = ase3d_after_token(line, "*SUBMATERIAL");
        idx = (int)ase3d_parse_int_ptr(&a);
        if (ase3d_push_block(p, ASE3D_BLOCK_MATERIAL) == ASE3D_OK) {
            p->current_material = ase3d_add_material(p, idx, p->parent_material);
        }
        return 1;
    }

    ase3d_push_block(p, ASE3D_BLOCK_UNKNOWN);
    return 1;
}

int ase3d_parse_line(ase3d_parser *p, const char *line)
{
    const char *s;
    const char *a;
    int closing;
    int obj_idx;

    if (!p || !p->model || !line) return ASE3D_ERR_BAD_ARG;
    p->line++;
    p->model->parsed_lines = p->line;

    s = ase3d_skip_ws(line);

    closing = 0;
    while (*s == '}') {
        ase3d_pop_block(p);
        s++;
        s = ase3d_skip_ws(s);
        closing = 1;
    }
    if (closing && *s == 0) return ASE3D_OK;
    if (*s == 0) return ASE3D_OK;

    ase3d_process_block_start(p, s);
    ase3d_parse_scene_line(p, s);

    if (ase3d_top_block(p) == ASE3D_BLOCK_MAP && p->current_map >= 0) {
        if (ase3d_starts_token(s, "*MAP_NAME") || ase3d_starts_token(s, "*MAP_CLASS") ||
            ase3d_starts_token(s, "*MAP_SUBNO") || ase3d_starts_token(s, "*MAP_AMOUNT") ||
            ase3d_starts_token(s, "*MAP_TEXTURE") || ase3d_starts_token(s, "*BITMAP") ||
            ase3d_starts_token(s, "*UVW_U_OFFSET") || ase3d_starts_token(s, "*UVW_V_OFFSET") ||
            ase3d_starts_token(s, "*UVW_U_TILING") || ase3d_starts_token(s, "*UVW_V_TILING") ||
            ase3d_starts_token(s, "*UVW_ANGLE") || ase3d_starts_token(s, "*MAP_BLUR") ||
            ase3d_starts_token(s, "*UVW_BLUR") || ase3d_starts_token(s, "*MAP_INVERT")) {
            ase3d_parse_map_line(p, s);
            return ASE3D_OK;
        }
    }

    if (p->current_light >= 0) {
        if (ase3d_starts_token(s, "*LIGHT_TYPE") || ase3d_starts_token(s, "*LIGHT_COLOR") ||
            ase3d_starts_token(s, "*LIGHT_INTENS") || ase3d_starts_token(s, "*LIGHT_CONTRAST") ||
            ase3d_starts_token(s, "*LIGHT_SOFTEN_DIFF_EDGE") || ase3d_starts_token(s, "*LIGHT_HOTSPOT") ||
            ase3d_starts_token(s, "*LIGHT_FALLOFF") || ase3d_starts_token(s, "*LIGHT_ASPECT") ||
            ase3d_starts_token(s, "*LIGHT_ATTNSTART") || ase3d_starts_token(s, "*LIGHT_ATTNEND") ||
            ase3d_starts_token(s, "*LIGHT_MAP_BIAS") || ase3d_starts_token(s, "*LIGHT_MAP_SIZE") ||
            ase3d_starts_token(s, "*LIGHT_MAP_RANGE") || ase3d_starts_token(s, "*LIGHT_SHADOWS") ||
            ase3d_starts_token(s, "*TIMEVALUE")) {
            ase3d_parse_light_line(p, s);
            return ASE3D_OK;
        }
    }

    if (p->current_camera >= 0) {
        if (ase3d_starts_token(s, "*CAMERA_TARGET") || ase3d_starts_token(s, "*CAMERA_FOV") ||
            ase3d_starts_token(s, "*CAMERA_NEAR") || ase3d_starts_token(s, "*CAMERA_FAR") ||
            ase3d_starts_token(s, "*CAMERA_NEAR_RANGE") || ase3d_starts_token(s, "*CAMERA_FAR_RANGE") ||
            ase3d_starts_token(s, "*CAMERA_TDIST") || ase3d_starts_token(s, "*CAMERA_HITHER") ||
            ase3d_starts_token(s, "*CAMERA_YON") || ase3d_starts_token(s, "*TIMEVALUE")) {
            ase3d_parse_camera_line(p, s);
            return ASE3D_OK;
        }
    }

    a = ase3d_after_token(s, "*NODE_NAME");
    if (a) {
        if (ase3d_top_block(p) == ASE3D_BLOCK_TMANIM) {
            obj_idx = ase3d_find_object_by_name(p->model, a);
            if (obj_idx >= 0) p->current_object = obj_idx;
        } else if (p->current_object >= 0 && (unsigned int)p->current_object < p->model->object_count) {
            ase3d_copy_quoted(p->model->objects[p->current_object].name, ASE3D_NAME_MAX, a);
        }
        return ASE3D_OK;
    }
    a = ase3d_after_token(s, "*NODE_PARENT");
    if (a && p->current_object >= 0 && (unsigned int)p->current_object < p->model->object_count) {
        ase3d_copy_quoted(p->model->objects[p->current_object].parent_name, ASE3D_NAME_MAX, a);
        return ASE3D_OK;
    }
    a = ase3d_after_token(s, "*MATERIAL_REF");
    if (a && p->current_object >= 0 && (unsigned int)p->current_object < p->model->object_count) {
        p->model->objects[p->current_object].material_ref = (int)ase3d_parse_int_ptr(&a);
        return ASE3D_OK;
    }
    a = ase3d_after_token(s, "*TIMEVALUE");
    if (a && p->current_mesh_span >= 0 && (unsigned int)p->current_mesh_span < p->model->mesh_span_count) {
        p->model->mesh_spans[p->current_mesh_span].timevalue = ase3d_parse_int_ptr(&a);
        return ASE3D_OK;
    }

    if (ase3d_starts_token(s, "*TM_ROW0") || ase3d_starts_token(s, "*TM_ROW1") ||
        ase3d_starts_token(s, "*TM_ROW2") || ase3d_starts_token(s, "*TM_ROW3") ||
        ase3d_starts_token(s, "*TM_POS") || ase3d_starts_token(s, "*TM_ROTAXIS") ||
        ase3d_starts_token(s, "*TM_ROTANGLE") || ase3d_starts_token(s, "*TM_SCALE")) {
        ase3d_parse_tm_line(p, s);
        return ASE3D_OK;
    }

    if (p->current_material >= 0) {
        if (ase3d_starts_token(s, "*MATERIAL_NAME") || ase3d_starts_token(s, "*MATERIAL_CLASS") ||
            ase3d_starts_token(s, "*MATERIAL_AMBIENT") || ase3d_starts_token(s, "*MATERIAL_DIFFUSE") ||
            ase3d_starts_token(s, "*MATERIAL_SPECULAR") || ase3d_starts_token(s, "*MATERIAL_COLOR") ||
            ase3d_starts_token(s, "*MATERIAL_SHINE") || ase3d_starts_token(s, "*MATERIAL_SHINESTRENGTH") ||
            ase3d_starts_token(s, "*MATERIAL_TRANSPARENCY") || ase3d_starts_token(s, "*NUMSUBMTLS") ||
            ase3d_starts_token(s, "*BITMAP")) {
            ase3d_parse_material_line(p, s);
            return ASE3D_OK;
        }
    }

    a = ase3d_after_token(s, "*CONTROL_POS_SAMPLE");
    if (a) { ase3d_store_pos_key(p, a, ASE3D_KEY_LINEAR); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_BEZIER_POS_KEY");
    if (a) { ase3d_store_pos_key(p, a, ASE3D_KEY_BEZIER); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_TCB_POS_KEY");
    if (a) { ase3d_store_pos_key(p, a, ASE3D_KEY_TCB); return ASE3D_OK; }

    a = ase3d_after_token(s, "*CONTROL_ROT_SAMPLE");
    if (a) { ase3d_store_rot_key(p, a, ASE3D_KEY_LINEAR); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_TCB_ROT_KEY");
    if (a) { ase3d_store_rot_key(p, a, ASE3D_KEY_TCB); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_BEZIER_ROT_KEY");
    if (a) { ase3d_store_rot_key(p, a, ASE3D_KEY_BEZIER); return ASE3D_OK; }

    a = ase3d_after_token(s, "*CONTROL_SCALE_SAMPLE");
    if (a) { ase3d_store_scale_key(p, a, ASE3D_KEY_LINEAR); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_TCB_SCALE_KEY");
    if (a) { ase3d_store_scale_key(p, a, ASE3D_KEY_TCB); return ASE3D_OK; }
    a = ase3d_after_token(s, "*CONTROL_BEZIER_SCALE_KEY");
    if (a) { ase3d_store_scale_key(p, a, ASE3D_KEY_BEZIER); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_VERTEX");
    if (a) { ase3d_store_vertex(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_FACE");
    if (a) { ase3d_store_face(p, a, s); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_TVERT");
    if (a) { ase3d_store_tvert(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_TFACE");
    if (a) { ase3d_store_tface(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_VERTCOL");
    if (a) { ase3d_store_cvert(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_CFACE");
    if (a) { ase3d_store_cface(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_FACENORMAL");
    if (a) { ase3d_store_face_normal(p, a); return ASE3D_OK; }

    a = ase3d_after_token(s, "*MESH_VERTEXNORMAL");
    if (a) { ase3d_store_corner_normal(p, a); return ASE3D_OK; }

    return ASE3D_OK;
}

int ase3d_parse_text(ase3d_parser *p, const char *text, unsigned long text_len)
{
    char line[1024];
    unsigned int n;
    unsigned long i;
    char c;
    int r;
    int truncated;
    if (!p || !p->model || !text) return ASE3D_ERR_BAD_ARG;
    n = 0U;
    truncated = 0;
    for (i = 0UL; i < text_len; ++i) {
        c = text[i];
        if (c == '\n') {
            line[n] = 0;
            if (!truncated) {
                r = ase3d_parse_line(p, line);
                if (r != ASE3D_OK && r != ASE3D_ERR_STACK_UNDER) return r;
            } else {
                p->model->warnings |= ASE3D_WARN_LINE_TRUNCATED;
                truncated = 0;
            }
            n = 0U;
        } else {
            if (n + 1U < sizeof(line)) line[n++] = c;
            else truncated = 1;
        }
    }
    if (n > 0U && !truncated) {
        line[n] = 0;
        r = ase3d_parse_line(p, line);
        if (r != ASE3D_OK && r != ASE3D_ERR_STACK_UNDER) return r;
    }
    return ASE3D_OK;
}

ase3d_fixed ase3d_fixed_from_int(long v)
{
    return (ase3d_fixed)(v << ASE3D_FIXED_SHIFT);
}

long ase3d_fixed_to_int(ase3d_fixed v)
{
    return (long)(v >> ASE3D_FIXED_SHIFT);
}

ase3d_fixed ase3d_fixed_mul(ase3d_fixed a, ase3d_fixed b)
{
    long ah;
    long al;
    long bh;
    long bl;
    long sign;
    long r;
    sign = 1L;
    if (a < 0L) { a = -a; sign = -sign; }
    if (b < 0L) { b = -b; sign = -sign; }
    ah = a >> 8;
    al = a & 255L;
    bh = b >> 8;
    bl = b & 255L;
    r = ((ah * bh) >> 0);
    r += ((ah * bl) >> 8);
    r += ((al * bh) >> 8);
    r += ((al * bl) >> 16);
    if (sign < 0L) r = -r;
    return r;
}

ase3d_fixed ase3d_fixed_div(ase3d_fixed a, ase3d_fixed b)
{
    long q;
    long r;
    long sign;
    int i;
    if (b == 0L) return 0L;
    sign = 1L;
    if (a < 0L) { a = -a; sign = -sign; }
    if (b < 0L) { b = -b; sign = -sign; }
    q = a / b;
    r = a % b;
    q <<= ASE3D_FIXED_SHIFT;
    for (i = 0; i < ASE3D_FIXED_SHIFT; ++i) {
        r <<= 1;
        if (r >= b) {
            r -= b;
            q += (1L << (ASE3D_FIXED_SHIFT - 1 - i));
        }
    }
    if (sign < 0L) q = -q;
    return q;
}

ase3d_fixed ase3d_fixed_lerp(ase3d_fixed a, ase3d_fixed b, ase3d_fixed t)
{
    return a + ase3d_fixed_mul((b - a), t);
}

static long ase3d_abs_long(long v)
{
    return v < 0L ? -v : v;
}

static unsigned long ase3d_isqrt(unsigned long x)
{
    unsigned long op;
    unsigned long res;
    unsigned long one;
    op = x;
    res = 0UL;
    one = 1UL << 30;
    while (one > op) one >>= 2;
    while (one != 0UL) {
        if (op >= res + one) {
            op -= res + one;
            res += one << 1;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}


static ase3d_fixed ase3d_fixed_clamp(ase3d_fixed v, ase3d_fixed lo, ase3d_fixed hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static ase3d_fixed ase3d_fixed_sqrt(ase3d_fixed v)
{
    unsigned long root;
    if (v <= 0L) return 0L;
    root = ase3d_isqrt((unsigned long)v);
    return (ase3d_fixed)(root << 8);
}

ase3d_fixed ase3d_fixed_sqrt_public(ase3d_fixed v)
{
    return ase3d_fixed_sqrt(v);
}

ase3d_fixed ase3d_fixed_sin_rad(ase3d_fixed radians)
{
    ase3d_fixed x;
    ase3d_fixed pix;
    ase3d_fixed num;
    ase3d_fixed den;
    int neg;
    x = radians;
    while (x > ASE3D_PI_FIXED) x -= ASE3D_TWO_PI_FIXED;
    while (x < -ASE3D_PI_FIXED) x += ASE3D_TWO_PI_FIXED;
    neg = 0;
    if (x < 0L) { neg = 1; x = -x; }
    if (x > ASE3D_PI_FIXED) x = ASE3D_TWO_PI_FIXED - x;
    if (x < 0L) x = -x;
    pix = ase3d_fixed_mul(x, ASE3D_PI_FIXED - x);
    num = ase3d_fixed_mul(16L << ASE3D_FIXED_SHIFT, pix);
    den = ase3d_fixed_mul(5L << ASE3D_FIXED_SHIFT, ase3d_fixed_mul(ASE3D_PI_FIXED, ASE3D_PI_FIXED)) -
          ase3d_fixed_mul(4L << ASE3D_FIXED_SHIFT, pix);
    if (den == 0L) return 0L;
    x = ase3d_fixed_div(num, den);
    if (x > ASE3D_FIXED_ONE) x = ASE3D_FIXED_ONE;
    if (neg) x = -x;
    return x;
}

ase3d_fixed ase3d_fixed_cos_rad(ase3d_fixed radians)
{
    return ase3d_fixed_sin_rad(radians + ASE3D_HALF_PI_FIXED);
}

ase3d_fixed ase3d_fixed_acos(ase3d_fixed x)
{
    ase3d_fixed lo;
    ase3d_fixed hi;
    ase3d_fixed mid;
    ase3d_fixed c;
    int i;
    x = ase3d_fixed_clamp(x, -ASE3D_FIXED_ONE, ASE3D_FIXED_ONE);
    lo = 0L;
    hi = ASE3D_PI_FIXED;
    for (i = 0; i < 18; ++i) {
        mid = (lo + hi) >> 1;
        c = ase3d_fixed_cos_rad(mid);
        if (c > x) lo = mid;
        else hi = mid;
    }
    return (lo + hi) >> 1;
}

static ase3d_vec3 ase3d_vec3_add(ase3d_vec3 a, ase3d_vec3 b)
{
    ase3d_vec3 r;
    r.x = a.x + b.x; r.y = a.y + b.y; r.z = a.z + b.z;
    return r;
}

static ase3d_vec3 ase3d_vec3_sub(ase3d_vec3 a, ase3d_vec3 b)
{
    ase3d_vec3 r;
    r.x = a.x - b.x; r.y = a.y - b.y; r.z = a.z - b.z;
    return r;
}

static ase3d_vec3 ase3d_vec3_scale(ase3d_vec3 a, ase3d_fixed s)
{
    ase3d_vec3 r;
    r.x = ase3d_fixed_mul(a.x, s);
    r.y = ase3d_fixed_mul(a.y, s);
    r.z = ase3d_fixed_mul(a.z, s);
    return r;
}

static ase3d_vec3 ase3d_vec3_lerp(ase3d_vec3 a, ase3d_vec3 b, ase3d_fixed t)
{
    ase3d_vec3 r;
    r.x = ase3d_fixed_lerp(a.x, b.x, t);
    r.y = ase3d_fixed_lerp(a.y, b.y, t);
    r.z = ase3d_fixed_lerp(a.z, b.z, t);
    return r;
}

static ase3d_vec3 ase3d_vec3_normalize(ase3d_vec3 v)
{
    ase3d_fixed d;
    ase3d_fixed len;
    d = ase3d_fixed_mul(v.x, v.x) + ase3d_fixed_mul(v.y, v.y) + ase3d_fixed_mul(v.z, v.z);
    len = ase3d_fixed_sqrt(d);
    if (len == 0L) {
        v.x = ASE3D_FIXED_ONE; v.y = 0L; v.z = 0L;
        return v;
    }
    v.x = ase3d_fixed_div(v.x, len);
    v.y = ase3d_fixed_div(v.y, len);
    v.z = ase3d_fixed_div(v.z, len);
    return v;
}

ase3d_quat ase3d_quat_identity(void)
{
    ase3d_quat q;
    q.x = 0L; q.y = 0L; q.z = 0L; q.w = ASE3D_FIXED_ONE;
    return q;
}

ase3d_quat ase3d_quat_normalize(ase3d_quat q)
{
    ase3d_fixed d;
    ase3d_fixed len;
    d = ase3d_fixed_mul(q.x, q.x) + ase3d_fixed_mul(q.y, q.y) + ase3d_fixed_mul(q.z, q.z) + ase3d_fixed_mul(q.w, q.w);
    len = ase3d_fixed_sqrt(d);
    if (len == 0L) return ase3d_quat_identity();
    q.x = ase3d_fixed_div(q.x, len);
    q.y = ase3d_fixed_div(q.y, len);
    q.z = ase3d_fixed_div(q.z, len);
    q.w = ase3d_fixed_div(q.w, len);
    return q;
}

ase3d_quat ase3d_quat_mul(ase3d_quat a, ase3d_quat b)
{
    ase3d_quat r;
    r.w = ase3d_fixed_mul(a.w, b.w) - ase3d_fixed_mul(a.x, b.x) - ase3d_fixed_mul(a.y, b.y) - ase3d_fixed_mul(a.z, b.z);
    r.x = ase3d_fixed_mul(a.w, b.x) + ase3d_fixed_mul(a.x, b.w) + ase3d_fixed_mul(a.y, b.z) - ase3d_fixed_mul(a.z, b.y);
    r.y = ase3d_fixed_mul(a.w, b.y) - ase3d_fixed_mul(a.x, b.z) + ase3d_fixed_mul(a.y, b.w) + ase3d_fixed_mul(a.z, b.x);
    r.z = ase3d_fixed_mul(a.w, b.z) + ase3d_fixed_mul(a.x, b.y) - ase3d_fixed_mul(a.y, b.x) + ase3d_fixed_mul(a.z, b.w);
    return ase3d_quat_normalize(r);
}

static ase3d_fixed ase3d_quat_dot(ase3d_quat a, ase3d_quat b)
{
    return ase3d_fixed_mul(a.x, b.x) + ase3d_fixed_mul(a.y, b.y) + ase3d_fixed_mul(a.z, b.z) + ase3d_fixed_mul(a.w, b.w);
}

ase3d_quat ase3d_quat_from_axis_angle(ase3d_vec3 axis, ase3d_fixed angle)
{
    ase3d_quat q;
    ase3d_fixed half;
    ase3d_fixed s;
    axis = ase3d_vec3_normalize(axis);
    half = angle >> 1;
    s = ase3d_fixed_sin_rad(half);
    q.x = ase3d_fixed_mul(axis.x, s);
    q.y = ase3d_fixed_mul(axis.y, s);
    q.z = ase3d_fixed_mul(axis.z, s);
    q.w = ase3d_fixed_cos_rad(half);
    return ase3d_quat_normalize(q);
}

ase3d_quat ase3d_quat_slerp(ase3d_quat a, ase3d_quat b, ase3d_fixed t)
{
    ase3d_quat r;
    ase3d_fixed dot;
    ase3d_fixed omega;
    ase3d_fixed sinom;
    ase3d_fixed s0;
    ase3d_fixed s1;
    a = ase3d_quat_normalize(a);
    b = ase3d_quat_normalize(b);
    dot = ase3d_quat_dot(a, b);
    if (dot < 0L) {
        b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
        dot = -dot;
    }
    if (dot > ASE3D_SLERP_CLOSE_DOT) {
        r.x = ase3d_fixed_lerp(a.x, b.x, t);
        r.y = ase3d_fixed_lerp(a.y, b.y, t);
        r.z = ase3d_fixed_lerp(a.z, b.z, t);
        r.w = ase3d_fixed_lerp(a.w, b.w, t);
        return ase3d_quat_normalize(r);
    }
    omega = ase3d_fixed_acos(dot);
    sinom = ase3d_fixed_sin_rad(omega);
    if (sinom == 0L) return a;
    s0 = ase3d_fixed_div(ase3d_fixed_sin_rad(ase3d_fixed_mul(ASE3D_FIXED_ONE - t, omega)), sinom);
    s1 = ase3d_fixed_div(ase3d_fixed_sin_rad(ase3d_fixed_mul(t, omega)), sinom);
    r.x = ase3d_fixed_mul(a.x, s0) + ase3d_fixed_mul(b.x, s1);
    r.y = ase3d_fixed_mul(a.y, s0) + ase3d_fixed_mul(b.y, s1);
    r.z = ase3d_fixed_mul(a.z, s0) + ase3d_fixed_mul(b.z, s1);
    r.w = ase3d_fixed_mul(a.w, s0) + ase3d_fixed_mul(b.w, s1);
    return ase3d_quat_normalize(r);
}

ase3d_mat43 ase3d_quat_to_mat43(ase3d_quat q)
{
    ase3d_mat43 m;
    ase3d_fixed xx, yy, zz, xy, xz, yz, wx, wy, wz;
    ase3d_fixed two;
    q = ase3d_quat_normalize(q);
    two = 2L << ASE3D_FIXED_SHIFT;
    xx = ase3d_fixed_mul(q.x, q.x);
    yy = ase3d_fixed_mul(q.y, q.y);
    zz = ase3d_fixed_mul(q.z, q.z);
    xy = ase3d_fixed_mul(q.x, q.y);
    xz = ase3d_fixed_mul(q.x, q.z);
    yz = ase3d_fixed_mul(q.y, q.z);
    wx = ase3d_fixed_mul(q.w, q.x);
    wy = ase3d_fixed_mul(q.w, q.y);
    wz = ase3d_fixed_mul(q.w, q.z);
    m.row0.x = ASE3D_FIXED_ONE - ase3d_fixed_mul(two, yy + zz);
    m.row0.y = ase3d_fixed_mul(two, xy + wz);
    m.row0.z = ase3d_fixed_mul(two, xz - wy);
    m.row1.x = ase3d_fixed_mul(two, xy - wz);
    m.row1.y = ASE3D_FIXED_ONE - ase3d_fixed_mul(two, xx + zz);
    m.row1.z = ase3d_fixed_mul(two, yz + wx);
    m.row2.x = ase3d_fixed_mul(two, xz + wy);
    m.row2.y = ase3d_fixed_mul(two, yz - wx);
    m.row2.z = ASE3D_FIXED_ONE - ase3d_fixed_mul(two, xx + yy);
    m.row3.x = 0L; m.row3.y = 0L; m.row3.z = 0L;
    return m;
}

static ase3d_vec3 ase3d_make_normal(ase3d_vec3 a, ase3d_vec3 b, ase3d_vec3 c)
{
    long ax;
    long ay;
    long az;
    long bx;
    long by;
    long bz;
    long nx;
    long ny;
    long nz;
    long maxa;
    long scale;
    unsigned long sum;
    unsigned long len;
    ase3d_vec3 out;

    ax = (b.x - a.x) >> 8;
    ay = (b.y - a.y) >> 8;
    az = (b.z - a.z) >> 8;
    bx = (c.x - a.x) >> 8;
    by = (c.y - a.y) >> 8;
    bz = (c.z - a.z) >> 8;

    nx = ay * bz - az * by;
    ny = az * bx - ax * bz;
    nz = ax * by - ay * bx;

    maxa = ase3d_abs_long(nx);
    if (ase3d_abs_long(ny) > maxa) maxa = ase3d_abs_long(ny);
    if (ase3d_abs_long(nz) > maxa) maxa = ase3d_abs_long(nz);
    if (maxa == 0L) {
        out.x = 0L; out.y = 0L; out.z = 0L;
        return out;
    }
    if (maxa > 16000L) {
        scale = maxa / 16000L + 1L;
        nx /= scale; ny /= scale; nz /= scale;
    }
    sum = (unsigned long)(nx * nx) + (unsigned long)(ny * ny) + (unsigned long)(nz * nz);
    len = ase3d_isqrt(sum);
    if (len == 0UL) {
        out.x = 0L; out.y = 0L; out.z = 0L;
        return out;
    }
    out.x = (ase3d_fixed)((nx * ASE3D_FIXED_ONE) / (long)len);
    out.y = (ase3d_fixed)((ny * ASE3D_FIXED_ONE) / (long)len);
    out.z = (ase3d_fixed)((nz * ASE3D_FIXED_ONE) / (long)len);
    return out;
}

void ase3d_build_flat_normals(ase3d_model *m)
{
    unsigned int i;
    ase3d_face *f;
    ase3d_vec3 n;
    if (!m || !m->face_normals) return;
    m->face_normal_count = 0U;
    for (i = 0U; i < m->face_count; ++i) {
        f = &m->faces[i];
        if (f->a < 0 || f->b < 0 || f->c < 0 ||
            (unsigned int)f->a >= m->vertex_count ||
            (unsigned int)f->b >= m->vertex_count ||
            (unsigned int)f->c >= m->vertex_count) {
            m->warnings |= ASE3D_WARN_BAD_INDEX;
            continue;
        }
        n = ase3d_make_normal(m->vertices[f->a], m->vertices[f->b], m->vertices[f->c]);
        if (m->face_normal_count < m->face_normal_capacity) {
            m->face_normals[m->face_normal_count++] = n;
        } else {
            m->warnings |= ASE3D_WARN_NORMAL_OVERFLOW;
            return;
        }
    }
}

static int ase3d_faces_smooth_compatible(const ase3d_face *a, const ase3d_face *b)
{
    if (!a || !b) return 0;
    if (a->object_id != b->object_id) return 0;
    if (a->mesh_span_id != b->mesh_span_id) return 0;
    if (a->smoothing_mask == 0UL || b->smoothing_mask == 0UL) return 0;
    return (a->smoothing_mask & b->smoothing_mask) != 0UL;
}

static int ase3d_face_uses_vertex(const ase3d_face *f, int vi)
{
    return f && (f->a == vi || f->b == vi || f->c == vi);
}

static ase3d_vec3 ase3d_face_normal_by_index(const ase3d_model *m, unsigned int i)
{
    ase3d_vec3 z;
    const ase3d_face *f;
    z.x = 0L; z.y = 0L; z.z = 0L;
    if (!m || !m->faces || !m->vertices || i >= m->face_count) return z;
    f = &m->faces[i];
    if (f->a < 0 || f->b < 0 || f->c < 0 ||
        (unsigned int)f->a >= m->vertex_count ||
        (unsigned int)f->b >= m->vertex_count ||
        (unsigned int)f->c >= m->vertex_count) return z;
    return ase3d_make_normal(m->vertices[f->a], m->vertices[f->b], m->vertices[f->c]);
}

void ase3d_build_smooth_normals(ase3d_model *m)
{
    unsigned int i;
    unsigned int j;
    int c;
    int vi;
    ase3d_vec3 sum;
    ase3d_vec3 n;
    ase3d_corner_normal cn;
    const ase3d_face *f;
    if (!m || !m->corner_normals || !m->faces) return;
    m->corner_normal_count = 0U;
    for (i = 0U; i < m->face_count; ++i) {
        f = &m->faces[i];
        for (c = 0; c < 3; ++c) {
            vi = (c == 0) ? f->a : (c == 1 ? f->b : f->c);
            sum.x = 0L; sum.y = 0L; sum.z = 0L;
            for (j = 0U; j < m->face_count; ++j) {
                if (!ase3d_face_uses_vertex(&m->faces[j], vi)) continue;
                if (j != i && !ase3d_faces_smooth_compatible(f, &m->faces[j])) continue;
                n = ase3d_face_normal_by_index(m, j);
                sum.x += (n.x >> 4);
                sum.y += (n.y >> 4);
                sum.z += (n.z >> 4);
            }
            n = ase3d_vec3_normalize(sum);
            cn.face_index = (int)i;
            cn.local_face_index = f->index;
            cn.vertex_index = vi;
            cn.local_vertex_index = (c == 0) ? f->local_a : (c == 1 ? f->local_b : f->local_c);
            cn.corner = c;
            cn.n = n;
            cn.object_id = f->object_id;
            cn.mesh_span_id = f->mesh_span_id;
            if (m->corner_normal_count < m->corner_normal_capacity) {
                m->corner_normals[m->corner_normal_count++] = cn;
            } else {
                m->warnings |= ASE3D_WARN_CORNERN_OVERFLOW;
                return;
            }
        }
    }
}

void ase3d_apply_object_transforms(ase3d_model *m)
{
    unsigned int oi;
    int vi;
    int end;
    ase3d_object *obj;
    ase3d_vec3 v;
    ase3d_vec3 r;
    if (!m || !m->objects || !m->vertices) return;
    for (oi = 0U; oi < m->object_count; ++oi) {
        obj = &m->objects[oi];
        if (obj->first_vertex < 0 || obj->vertex_count <= 0) continue;
        if (!obj->tm.has_row0 && !obj->tm.has_row1 && !obj->tm.has_row2 && !obj->tm.has_row3) continue;
        end = obj->first_vertex + obj->vertex_count;
        for (vi = obj->first_vertex; vi < end; ++vi) {
            if (vi < 0 || (unsigned int)vi >= m->vertex_count) continue;
            v = m->vertices[vi];
            r.x = ase3d_fixed_mul(v.x, obj->tm.row0.x) + ase3d_fixed_mul(v.y, obj->tm.row1.x) + ase3d_fixed_mul(v.z, obj->tm.row2.x) + obj->tm.row3.x;
            r.y = ase3d_fixed_mul(v.x, obj->tm.row0.y) + ase3d_fixed_mul(v.y, obj->tm.row1.y) + ase3d_fixed_mul(v.z, obj->tm.row2.y) + obj->tm.row3.y;
            r.z = ase3d_fixed_mul(v.x, obj->tm.row0.z) + ase3d_fixed_mul(v.y, obj->tm.row1.z) + ase3d_fixed_mul(v.z, obj->tm.row2.z) + obj->tm.row3.z;
            m->vertices[vi] = r;
        }
    }
}

static ase3d_fixed ase3d_fraction_between(long a, long b, long x)
{
    long num;
    long den;
    if (b == a) return 0L;
    num = x - a;
    den = b - a;
    return ase3d_fixed_div((ase3d_fixed)num << ASE3D_FIXED_SHIFT, (ase3d_fixed)den << ASE3D_FIXED_SHIFT);
}


static int ase3d_name_equal(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

int ase3d_find_object_index(const ase3d_model *m, const char *name)
{
    unsigned int i;
    if (!m || !name || !m->objects) return -1;
    for (i = 0U; i < m->object_count; ++i) {
        if (ase3d_name_equal(m->objects[i].name, name)) return (int)i;
    }
    return -1;
}

int ase3d_find_parent_index(const ase3d_model *m, int object_id)
{
    if (!m || !m->objects || object_id < 0 || (unsigned int)object_id >= m->object_count) return -1;
    if (!m->objects[object_id].parent_name[0]) return -1;
    return ase3d_find_object_index(m, m->objects[object_id].parent_name);
}

static ase3d_vec3 ase3d_vec3_hermite(ase3d_vec3 p0, ase3d_vec3 p1, ase3d_vec3 m0, ase3d_vec3 m1, ase3d_fixed t)
{
    ase3d_fixed t2;
    ase3d_fixed t3;
    ase3d_fixed h00;
    ase3d_fixed h10;
    ase3d_fixed h01;
    ase3d_fixed h11;
    ase3d_vec3 r;
    t2 = ase3d_fixed_mul(t, t);
    t3 = ase3d_fixed_mul(t2, t);
    h00 = ase3d_fixed_mul(2L << ASE3D_FIXED_SHIFT, t3) - ase3d_fixed_mul(3L << ASE3D_FIXED_SHIFT, t2) + ASE3D_FIXED_ONE;
    h10 = t3 - ase3d_fixed_mul(2L << ASE3D_FIXED_SHIFT, t2) + t;
    h01 = -ase3d_fixed_mul(2L << ASE3D_FIXED_SHIFT, t3) + ase3d_fixed_mul(3L << ASE3D_FIXED_SHIFT, t2);
    h11 = t3 - t2;
    r.x = ase3d_fixed_mul(h00, p0.x) + ase3d_fixed_mul(h10, m0.x) + ase3d_fixed_mul(h01, p1.x) + ase3d_fixed_mul(h11, m1.x);
    r.y = ase3d_fixed_mul(h00, p0.y) + ase3d_fixed_mul(h10, m0.y) + ase3d_fixed_mul(h01, p1.y) + ase3d_fixed_mul(h11, m1.y);
    r.z = ase3d_fixed_mul(h00, p0.z) + ase3d_fixed_mul(h10, m0.z) + ase3d_fixed_mul(h01, p1.z) + ase3d_fixed_mul(h11, m1.z);
    return r;
}

static ase3d_fixed ase3d_tcb_coeff(ase3d_fixed tension, ase3d_fixed continuity, ase3d_fixed bias, int outgoing, int forward)
{
    ase3d_fixed one_minus_t;
    ase3d_fixed a;
    ase3d_fixed b;
    one_minus_t = ASE3D_FIXED_ONE - tension;
    if (outgoing) {
        if (forward) {
            a = ASE3D_FIXED_ONE - continuity;
            b = ASE3D_FIXED_ONE - bias;
        } else {
            a = ASE3D_FIXED_ONE + continuity;
            b = ASE3D_FIXED_ONE + bias;
        }
    } else {
        if (forward) {
            a = ASE3D_FIXED_ONE + continuity;
            b = ASE3D_FIXED_ONE - bias;
        } else {
            a = ASE3D_FIXED_ONE - continuity;
            b = ASE3D_FIXED_ONE + bias;
        }
    }
    return ase3d_fixed_mul(ase3d_fixed_mul(one_minus_t, a), b) >> 1;
}

static ase3d_vec3 ase3d_tcb_tangent_vec(ase3d_vec3 prev, ase3d_vec3 cur, ase3d_vec3 next, ase3d_fixed tension, ase3d_fixed continuity, ase3d_fixed bias, int outgoing)
{
    ase3d_vec3 fwd;
    ase3d_vec3 back;
    ase3d_vec3 r;
    ase3d_fixed cf;
    ase3d_fixed cb;
    fwd = ase3d_vec3_sub(next, cur);
    back = ase3d_vec3_sub(cur, prev);
    cf = ase3d_tcb_coeff(tension, continuity, bias, outgoing, 1);
    cb = ase3d_tcb_coeff(tension, continuity, bias, outgoing, 0);
    r = ase3d_vec3_add(ase3d_vec3_scale(fwd, cf), ase3d_vec3_scale(back, cb));
    return r;
}

static int ase3d_find_pos_prev_next(const ase3d_model *m, int object_id, long tick, int *prev_i, int *next_i)
{
    unsigned int i;
    int prev;
    int next;
    prev = -1;
    next = -1;
    if (!m || !m->pos_keys) return 0;
    for (i = 0U; i < m->pos_key_count; ++i) {
        if (m->pos_keys[i].object_id != object_id) continue;
        if (m->pos_keys[i].tick <= tick) {
            if (prev < 0 || m->pos_keys[i].tick > m->pos_keys[prev].tick) prev = (int)i;
        }
        if (m->pos_keys[i].tick >= tick) {
            if (next < 0 || m->pos_keys[i].tick < m->pos_keys[next].tick) next = (int)i;
        }
    }
    if (prev < 0 && next < 0) return 0;
    if (prev < 0) prev = next;
    if (next < 0) next = prev;
    *prev_i = prev;
    *next_i = next;
    return 1;
}

static int ase3d_find_pos_before(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->pos_key_count; ++i) {
        if (m->pos_keys[i].object_id != object_id) continue;
        if (m->pos_keys[i].tick < tick) {
            if (best < 0 || m->pos_keys[i].tick > m->pos_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

static int ase3d_find_pos_after(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->pos_key_count; ++i) {
        if (m->pos_keys[i].object_id != object_id) continue;
        if (m->pos_keys[i].tick > tick) {
            if (best < 0 || m->pos_keys[i].tick < m->pos_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

int ase3d_sample_position(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *out)
{
    int pi;
    int ni;
    int pprev;
    int nnext;
    ase3d_fixed t;
    ase3d_vec3 m0;
    ase3d_vec3 m1;
    const ase3d_pos_key *pk;
    const ase3d_pos_key *nk;
    if (!m || !out || !m->pos_keys) return 0;
    if (!ase3d_find_pos_prev_next(m, object_id, tick, &pi, &ni)) return 0;
    pk = &m->pos_keys[pi];
    nk = &m->pos_keys[ni];
    if (pi == ni || pk->tick == nk->tick) { *out = pk->pos; return 1; }
    t = ase3d_fraction_between(pk->tick, nk->tick, tick);
    if (pk->kind == ASE3D_KEY_BEZIER || nk->kind == ASE3D_KEY_BEZIER) {
        if (pk->has_out_tangent && nk->has_in_tangent) {
            *out = ase3d_vec3_hermite(pk->pos, nk->pos, pk->out_tangent, nk->in_tangent, t);
            return 1;
        }
    }
    if (pk->kind == ASE3D_KEY_TCB || nk->kind == ASE3D_KEY_TCB) {
        pprev = ase3d_find_pos_before(m, object_id, pk->tick);
        nnext = ase3d_find_pos_after(m, object_id, nk->tick);
        m0 = ase3d_tcb_tangent_vec(pprev >= 0 ? m->pos_keys[pprev].pos : pk->pos,
                                  pk->pos,
                                  nk->pos,
                                  pk->tension, pk->continuity, pk->bias, 1);
        m1 = ase3d_tcb_tangent_vec(pk->pos,
                                  nk->pos,
                                  nnext >= 0 ? m->pos_keys[nnext].pos : nk->pos,
                                  nk->tension, nk->continuity, nk->bias, 0);
        *out = ase3d_vec3_hermite(pk->pos, nk->pos, m0, m1, t);
        return 1;
    }
    *out = ase3d_vec3_lerp(pk->pos, nk->pos, t);
    return 1;
}

static int ase3d_find_scale_prev_next(const ase3d_model *m, int object_id, long tick, int *prev_i, int *next_i)
{
    unsigned int i;
    int prev;
    int next;
    prev = -1;
    next = -1;
    if (!m || !m->scale_keys) return 0;
    for (i = 0U; i < m->scale_key_count; ++i) {
        if (m->scale_keys[i].object_id != object_id) continue;
        if (m->scale_keys[i].tick <= tick) {
            if (prev < 0 || m->scale_keys[i].tick > m->scale_keys[prev].tick) prev = (int)i;
        }
        if (m->scale_keys[i].tick >= tick) {
            if (next < 0 || m->scale_keys[i].tick < m->scale_keys[next].tick) next = (int)i;
        }
    }
    if (prev < 0 && next < 0) return 0;
    if (prev < 0) prev = next;
    if (next < 0) next = prev;
    *prev_i = prev;
    *next_i = next;
    return 1;
}

static int ase3d_find_scale_before(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->scale_key_count; ++i) {
        if (m->scale_keys[i].object_id != object_id) continue;
        if (m->scale_keys[i].tick < tick) {
            if (best < 0 || m->scale_keys[i].tick > m->scale_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

static int ase3d_find_scale_after(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->scale_key_count; ++i) {
        if (m->scale_keys[i].object_id != object_id) continue;
        if (m->scale_keys[i].tick > tick) {
            if (best < 0 || m->scale_keys[i].tick < m->scale_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

int ase3d_sample_scale(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *out)
{
    int pi;
    int ni;
    int pprev;
    int nnext;
    ase3d_fixed t;
    ase3d_vec3 m0;
    ase3d_vec3 m1;
    const ase3d_scale_key *pk;
    const ase3d_scale_key *nk;
    if (!m || !out || !m->scale_keys) return 0;
    if (!ase3d_find_scale_prev_next(m, object_id, tick, &pi, &ni)) return 0;
    pk = &m->scale_keys[pi];
    nk = &m->scale_keys[ni];
    if (pi == ni || pk->tick == nk->tick) { *out = pk->scale; return 1; }
    t = ase3d_fraction_between(pk->tick, nk->tick, tick);
    if (pk->kind == ASE3D_KEY_BEZIER || nk->kind == ASE3D_KEY_BEZIER) {
        if (pk->has_out_tangent && nk->has_in_tangent) {
            *out = ase3d_vec3_hermite(pk->scale, nk->scale, pk->out_tangent, nk->in_tangent, t);
            return 1;
        }
    }
    if (pk->kind == ASE3D_KEY_TCB || nk->kind == ASE3D_KEY_TCB) {
        pprev = ase3d_find_scale_before(m, object_id, pk->tick);
        nnext = ase3d_find_scale_after(m, object_id, nk->tick);
        m0 = ase3d_tcb_tangent_vec(pprev >= 0 ? m->scale_keys[pprev].scale : pk->scale,
                                  pk->scale,
                                  nk->scale,
                                  pk->tension, pk->continuity, pk->bias, 1);
        m1 = ase3d_tcb_tangent_vec(pk->scale,
                                  nk->scale,
                                  nnext >= 0 ? m->scale_keys[nnext].scale : nk->scale,
                                  nk->tension, nk->continuity, nk->bias, 0);
        *out = ase3d_vec3_hermite(pk->scale, nk->scale, m0, m1, t);
        return 1;
    }
    *out = ase3d_vec3_lerp(pk->scale, nk->scale, t);
    return 1;
}

static int ase3d_find_rot_prev_next(const ase3d_model *m, int object_id, long tick, int *prev_i, int *next_i)
{
    unsigned int i;
    int prev;
    int next;
    prev = -1;
    next = -1;
    if (!m || !m->rot_keys) return 0;
    for (i = 0U; i < m->rot_key_count; ++i) {
        if (m->rot_keys[i].object_id != object_id) continue;
        if (m->rot_keys[i].tick <= tick) {
            if (prev < 0 || m->rot_keys[i].tick > m->rot_keys[prev].tick) prev = (int)i;
        }
        if (m->rot_keys[i].tick >= tick) {
            if (next < 0 || m->rot_keys[i].tick < m->rot_keys[next].tick) next = (int)i;
        }
    }
    if (prev < 0 && next < 0) return 0;
    if (prev < 0) prev = next;
    if (next < 0) next = prev;
    *prev_i = prev;
    *next_i = next;
    return 1;
}

void ase3d_finalize_animation(ase3d_model *m)
{
    unsigned int oi;
    unsigned int ki;
    ase3d_quat accum;
    ase3d_quat delta;
    if (!m || !m->objects || !m->rot_keys) return;
    for (oi = 0U; oi < m->object_count; ++oi) {
        accum = ase3d_quat_identity();
        for (ki = 0U; ki < m->rot_key_count; ++ki) {
            if (m->rot_keys[ki].object_id != (int)oi) continue;
            delta = ase3d_quat_from_axis_angle(m->rot_keys[ki].axis, m->rot_keys[ki].angle);
            accum = ase3d_quat_mul(accum, delta);
            m->rot_keys[ki].quat = accum;
            m->rot_keys[ki].has_quat = 1;
        }
    }
}

int ase3d_sample_rotation_quat(const ase3d_model *m, int object_id, long tick, ase3d_quat *out)
{
    int pi;
    int ni;
    ase3d_fixed t;
    ase3d_quat qa;
    ase3d_quat qb;
    if (!m || !out || !m->rot_keys) return 0;
    if (!ase3d_find_rot_prev_next(m, object_id, tick, &pi, &ni)) return 0;
    qa = m->rot_keys[pi].has_quat ? m->rot_keys[pi].quat : ase3d_quat_from_axis_angle(m->rot_keys[pi].axis, m->rot_keys[pi].angle);
    qb = m->rot_keys[ni].has_quat ? m->rot_keys[ni].quat : ase3d_quat_from_axis_angle(m->rot_keys[ni].axis, m->rot_keys[ni].angle);
    if (pi == ni || m->rot_keys[pi].tick == m->rot_keys[ni].tick) {
        *out = qa;
        return 1;
    }
    t = ase3d_fraction_between(m->rot_keys[pi].tick, m->rot_keys[ni].tick, tick);
    *out = ase3d_quat_slerp(qa, qb, t);
    return 1;
}

int ase3d_sample_rotation_axis_angle(const ase3d_model *m, int object_id, long tick, ase3d_vec3 *axis, ase3d_fixed *angle)
{
    int pi;
    int ni;
    if (!m || !axis || !angle || !m->rot_keys) return 0;
    if (!ase3d_find_rot_prev_next(m, object_id, tick, &pi, &ni)) return 0;
    if (pi == ni || m->rot_keys[pi].tick == m->rot_keys[ni].tick) {
        *axis = m->rot_keys[pi].axis;
        *angle = m->rot_keys[pi].angle;
        return 1;
    }
    *axis = m->rot_keys[pi].axis;
    *angle = ase3d_fixed_lerp(m->rot_keys[pi].angle, m->rot_keys[ni].angle,
                              ase3d_fraction_between(m->rot_keys[pi].tick, m->rot_keys[ni].tick, tick));
    return 1;
}

static ase3d_mat43 ase3d_mat43_identity(void)
{
    ase3d_mat43 m;
    m.row0.x = ASE3D_FIXED_ONE; m.row0.y = 0L; m.row0.z = 0L;
    m.row1.x = 0L; m.row1.y = ASE3D_FIXED_ONE; m.row1.z = 0L;
    m.row2.x = 0L; m.row2.y = 0L; m.row2.z = ASE3D_FIXED_ONE;
    m.row3.x = 0L; m.row3.y = 0L; m.row3.z = 0L;
    return m;
}

static ase3d_mat43 ase3d_mat43_mul(ase3d_mat43 a, ase3d_mat43 b)
{
    ase3d_mat43 r;
    r.row0.x = ase3d_fixed_mul(a.row0.x, b.row0.x) + ase3d_fixed_mul(a.row0.y, b.row1.x) + ase3d_fixed_mul(a.row0.z, b.row2.x);
    r.row0.y = ase3d_fixed_mul(a.row0.x, b.row0.y) + ase3d_fixed_mul(a.row0.y, b.row1.y) + ase3d_fixed_mul(a.row0.z, b.row2.y);
    r.row0.z = ase3d_fixed_mul(a.row0.x, b.row0.z) + ase3d_fixed_mul(a.row0.y, b.row1.z) + ase3d_fixed_mul(a.row0.z, b.row2.z);
    r.row1.x = ase3d_fixed_mul(a.row1.x, b.row0.x) + ase3d_fixed_mul(a.row1.y, b.row1.x) + ase3d_fixed_mul(a.row1.z, b.row2.x);
    r.row1.y = ase3d_fixed_mul(a.row1.x, b.row0.y) + ase3d_fixed_mul(a.row1.y, b.row1.y) + ase3d_fixed_mul(a.row1.z, b.row2.y);
    r.row1.z = ase3d_fixed_mul(a.row1.x, b.row0.z) + ase3d_fixed_mul(a.row1.y, b.row1.z) + ase3d_fixed_mul(a.row1.z, b.row2.z);
    r.row2.x = ase3d_fixed_mul(a.row2.x, b.row0.x) + ase3d_fixed_mul(a.row2.y, b.row1.x) + ase3d_fixed_mul(a.row2.z, b.row2.x);
    r.row2.y = ase3d_fixed_mul(a.row2.x, b.row0.y) + ase3d_fixed_mul(a.row2.y, b.row1.y) + ase3d_fixed_mul(a.row2.z, b.row2.y);
    r.row2.z = ase3d_fixed_mul(a.row2.x, b.row0.z) + ase3d_fixed_mul(a.row2.y, b.row1.z) + ase3d_fixed_mul(a.row2.z, b.row2.z);
    r.row3.x = ase3d_fixed_mul(a.row3.x, b.row0.x) + ase3d_fixed_mul(a.row3.y, b.row1.x) + ase3d_fixed_mul(a.row3.z, b.row2.x) + b.row3.x;
    r.row3.y = ase3d_fixed_mul(a.row3.x, b.row0.y) + ase3d_fixed_mul(a.row3.y, b.row1.y) + ase3d_fixed_mul(a.row3.z, b.row2.y) + b.row3.y;
    r.row3.z = ase3d_fixed_mul(a.row3.x, b.row0.z) + ase3d_fixed_mul(a.row3.y, b.row1.z) + ase3d_fixed_mul(a.row3.z, b.row2.z) + b.row3.z;
    return r;
}

ase3d_vec3 ase3d_transform_point(ase3d_mat43 m, ase3d_vec3 v)
{
    ase3d_vec3 r;
    r.x = ase3d_fixed_mul(v.x, m.row0.x) + ase3d_fixed_mul(v.y, m.row1.x) + ase3d_fixed_mul(v.z, m.row2.x) + m.row3.x;
    r.y = ase3d_fixed_mul(v.x, m.row0.y) + ase3d_fixed_mul(v.y, m.row1.y) + ase3d_fixed_mul(v.z, m.row2.y) + m.row3.y;
    r.z = ase3d_fixed_mul(v.x, m.row0.z) + ase3d_fixed_mul(v.y, m.row1.z) + ase3d_fixed_mul(v.z, m.row2.z) + m.row3.z;
    return r;
}

int ase3d_sample_local_matrix(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out)
{
    ase3d_vec3 pos;
    ase3d_vec3 scl;
    ase3d_quat q;
    ase3d_mat43 mat;
    const ase3d_object *obj;
    if (!m || !out || !m->objects || object_id < 0 || (unsigned int)object_id >= m->object_count) return 0;
    obj = &m->objects[object_id];
    pos = obj->tm.has_pos ? obj->tm.pos : obj->tm.row3;
    scl = obj->tm.scale;
    if (!ase3d_sample_position(m, object_id, tick, &pos)) {
        if (obj->tm.has_row3) pos = obj->tm.row3;
    }
    if (!ase3d_sample_scale(m, object_id, tick, &scl)) {
        if (!obj->tm.has_scale) { scl.x = ASE3D_FIXED_ONE; scl.y = ASE3D_FIXED_ONE; scl.z = ASE3D_FIXED_ONE; }
    }
    if (!ase3d_sample_rotation_quat(m, object_id, tick, &q)) {
        q = obj->tm.has_rot ? ase3d_quat_from_axis_angle(obj->tm.rot_axis, obj->tm.rot_angle) : ase3d_quat_identity();
    }
    mat = ase3d_quat_to_mat43(q);
    mat.row0 = ase3d_vec3_scale(mat.row0, scl.x);
    mat.row1 = ase3d_vec3_scale(mat.row1, scl.y);
    mat.row2 = ase3d_vec3_scale(mat.row2, scl.z);
    mat.row3 = pos;
    *out = mat;
    return 1;
}

int ase3d_sample_world_matrix(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out)
{
    int chain[ASE3D_STACK_MAX];
    int count;
    int cur;
    int i;
    ase3d_mat43 local;
    ase3d_mat43 acc;
    if (!m || !out || object_id < 0) return 0;
    count = 0;
    cur = object_id;
    while (cur >= 0 && count < ASE3D_STACK_MAX) {
        chain[count++] = cur;
        cur = ase3d_find_parent_index(m, cur);
    }
    if (count <= 0) return 0;
    acc = ase3d_mat43_identity();
    for (i = 0; i < count; ++i) {
        if (!ase3d_sample_local_matrix(m, chain[i], tick, &local)) local = ase3d_mat43_identity();
        acc = ase3d_mat43_mul(acc, local);
    }
    *out = acc;
    return 1;
}

int ase3d_find_mesh_span_for_tick(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best_prev;
    int best_next;
    best_prev = -1;
    best_next = -1;
    if (!m || !m->mesh_spans) return -1;
    for (i = 0U; i < m->mesh_span_count; ++i) {
        if (m->mesh_spans[i].object_id != object_id) continue;
        if (m->mesh_spans[i].timevalue == tick) return (int)i;
        if (m->mesh_spans[i].timevalue <= tick) {
            if (best_prev < 0 || m->mesh_spans[i].timevalue > m->mesh_spans[best_prev].timevalue) best_prev = (int)i;
        }
        if (m->mesh_spans[i].timevalue >= tick) {
            if (best_next < 0 || m->mesh_spans[i].timevalue < m->mesh_spans[best_next].timevalue) best_next = (int)i;
        }
    }
    if (best_prev >= 0) return best_prev;
    return best_next;
}

static void ase3d_bounds_include(ase3d_bounds *b, ase3d_vec3 v)
{
    if (!b) return;
    if (!b->valid) {
        b->min = v;
        b->max = v;
        b->valid = 1;
        return;
    }
    if (v.x < b->min.x) b->min.x = v.x;
    if (v.y < b->min.y) b->min.y = v.y;
    if (v.z < b->min.z) b->min.z = v.z;
    if (v.x > b->max.x) b->max.x = v.x;
    if (v.y > b->max.y) b->max.y = v.y;
    if (v.z > b->max.z) b->max.z = v.z;
}

void ase3d_compute_object_bounds(ase3d_model *m)
{
    unsigned int oi;
    int vi;
    int end;
    long tick;
    ase3d_mat43 wm;
    ase3d_vec3 v;
    if (!m || !m->objects || !m->vertices) return;
    tick = m->scene.first_frame * m->scene.ticks_per_frame;
    for (oi = 0U; oi < m->object_count; ++oi) {
        ase3d_bounds_zero(&m->objects[oi].local_bounds);
        ase3d_bounds_zero(&m->objects[oi].world_bounds);
        if (m->objects[oi].first_vertex < 0 || m->objects[oi].vertex_count <= 0) continue;
        end = m->objects[oi].first_vertex + m->objects[oi].vertex_count;
        if (end > (int)m->vertex_count) end = (int)m->vertex_count;
        if (!ase3d_sample_world_matrix(m, (int)oi, tick, &wm)) wm = ase3d_mat43_identity();
        for (vi = m->objects[oi].first_vertex; vi < end; ++vi) {
            v = m->vertices[vi];
            ase3d_bounds_include(&m->objects[oi].local_bounds, v);
            ase3d_bounds_include(&m->objects[oi].world_bounds, ase3d_transform_point(wm, v));
        }
    }
}

int ase3d_fixed_to_string(ase3d_fixed v, char *out, unsigned int out_cap, int decimals)
{
    char tmp[32];
    unsigned int n;
    unsigned int ti;
    long ip;
    long frac;
    int neg;
    int i;
    if (!out || out_cap == 0U) return 0;
    out[0] = 0;
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    neg = 0;
    if (v < 0L) { neg = 1; v = -v; }
    ip = v >> ASE3D_FIXED_SHIFT;
    frac = v & (ASE3D_FIXED_ONE - 1L);
    n = 0U;
    if (neg && n + 1U < out_cap) out[n++] = '-';
    ti = 0U;
    if (ip == 0L) tmp[ti++] = '0';
    while (ip > 0L && ti < sizeof(tmp)) {
        tmp[ti++] = (char)('0' + (ip % 10L));
        ip /= 10L;
    }
    while (ti > 0U && n + 1U < out_cap) out[n++] = tmp[--ti];
    if (decimals > 0 && n + 1U < out_cap) out[n++] = '.';
    for (i = 0; i < decimals; ++i) {
        frac *= 10L;
        if (n + 1U < out_cap) out[n++] = (char)('0' + (frac >> ASE3D_FIXED_SHIFT));
        frac = frac & (ASE3D_FIXED_ONE - 1L);
    }
    out[n] = 0;
    return (int)n;
}
