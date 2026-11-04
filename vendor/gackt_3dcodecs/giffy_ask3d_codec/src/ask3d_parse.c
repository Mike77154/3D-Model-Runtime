#include <string.h>
#include "ask3d_parse.h"

static void ask3d_warn(Ask3dScene *scene, const char *text)
{
    ask3d_scene_add_warning(scene, text);
}

void ask3d_work_clear(Ask3dWork *work)
{
    if (work == 0) {
        return;
    }
    memset(work, 0, sizeof(Ask3dWork));
}

static int ask3d_next(Ask3dWork *work, Ask3dToken *tok)
{
    return ask3d_lex_next(&work->lex, tok);
}

static void ask3d_push(Ask3dWork *work, const Ask3dToken *tok)
{
    ask3d_lex_push(&work->lex, tok);
}

static int ask3d_read_i32(Ask3dWork *work, long *out)
{
    Ask3dToken tok;
    int ok;
    if (ask3d_next(work, &tok) != ASK3D_TOK_WORD) {
        return 0;
    }
    *out = ask3d_tok_to_i32(&tok, &ok);
    return ok;
}

static int ask3d_try_read_fixed(Ask3dWork *work, ask3d_fixed *out)
{
    Ask3dToken tok;
    int ok;
    if (ask3d_next(work, &tok) != ASK3D_TOK_WORD) {
        ask3d_push(work, &tok);
        return 0;
    }
    *out = ask3d_tok_to_fixed(&tok, &ok);
    if (ok == 0) {
        ask3d_push(work, &tok);
        return 0;
    }
    return 1;
}

static int ask3d_read_fixed(Ask3dWork *work, ask3d_fixed *out)
{
    Ask3dToken tok;
    int ok;
    if (ask3d_next(work, &tok) != ASK3D_TOK_WORD) {
        return 0;
    }
    *out = ask3d_tok_to_fixed(&tok, &ok);
    return ok;
}

static int ask3d_read_vec3(Ask3dWork *work, Ask3dVec3 *out)
{
    if (!ask3d_read_fixed(work, &out->x)) {
        return 0;
    }
    if (!ask3d_read_fixed(work, &out->y)) {
        return 0;
    }
    if (!ask3d_read_fixed(work, &out->z)) {
        return 0;
    }
    return 1;
}

static int ask3d_try_block(Ask3dWork *work)
{
    Ask3dToken tok;
    if (ask3d_next(work, &tok) == ASK3D_TOK_LBRACE) {
        return 1;
    }
    ask3d_push(work, &tok);
    return 0;
}

static void ask3d_skip_block_open(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    int depth;
    depth = 1;
    scene->skipped_blocks++;
    while (depth > 0) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF) {
            return;
        }
        if (tok.type == ASK3D_TOK_LBRACE) {
            depth++;
        } else if (tok.type == ASK3D_TOK_RBRACE) {
            depth--;
        }
    }
}

static void ask3d_skip_optional_block(Ask3dWork *work, Ask3dScene *scene)
{
    if (ask3d_try_block(work)) {
        ask3d_skip_block_open(work, scene);
    }
}

static void ask3d_discard_same_line(Ask3dWork *work, unsigned long line)
{
    Ask3dToken tok;
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE || tok.type == ASK3D_TOK_LBRACE) {
            ask3d_push(work, &tok);
            break;
        }
        if (tok.line != line || ask3d_tok_starts(&tok, "*")) {
            ask3d_push(work, &tok);
            break;
        }
    }
}

static void ask3d_reset_local(Ask3dWork *work)
{
    memset(work->local_vset, 0, sizeof(work->local_vset));
    memset(work->local_tvset, 0, sizeof(work->local_tvset));
    memset(work->local_cvset, 0, sizeof(work->local_cvset));
    memset(work->local_fset, 0, sizeof(work->local_fset));
    memset(work->local_vmap, 0, sizeof(work->local_vmap));
    memset(work->local_tvmap, 0, sizeof(work->local_tvmap));
    memset(work->channel_tvset, 0, sizeof(work->channel_tvset));
    memset(work->channel_tvmap, 0, sizeof(work->channel_tvmap));
    memset(work->local_cvmap, 0, sizeof(work->local_cvmap));
    memset(work->local_fmap, 0, sizeof(work->local_fmap));
    work->local_base_face = 0U;
    work->local_face_count = 0U;
    work->current_uv_channel_index = -1;
    work->current_uv_channel_id = 0;
}

static void ask3d_parse_scene_block(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long v;
    Ask3dVec3 vec;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*SCENE_FILENAME")) {
            Ask3dToken val;
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(scene->info.filename, ASK3D_MAX_PATH, val.text);
                scene->info.has_filename = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_FIRSTFRAME")) {
            if (ask3d_read_i32(work, &v)) {
                scene->info.first_frame = (int)v;
                scene->info.has_frame_range = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_LASTFRAME")) {
            if (ask3d_read_i32(work, &v)) {
                scene->info.last_frame = (int)v;
                scene->info.has_frame_range = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_FRAMESPEED")) {
            if (ask3d_read_i32(work, &v)) {
                scene->info.frame_speed = (int)v;
                scene->info.has_frame_speed = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_TICKSPERFRAME")) {
            if (ask3d_read_i32(work, &v)) {
                scene->info.ticks_per_frame = (int)v;
                scene->info.has_ticks_per_frame = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_BACKGROUND_STATIC")) {
            if (ask3d_read_vec3(work, &vec)) {
                scene->info.background_static = vec;
                scene->info.has_background = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*SCENE_AMBIENT_STATIC")) {
            if (ask3d_read_vec3(work, &vec)) {
                scene->info.ambient_static = vec;
                scene->info.has_ambient = 1U;
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}


static Ask3dTextureMap *ask3d_material_map(Ask3dMaterial *mat, int kind)
{
    if (mat == 0) {
        return 0;
    }
    if (kind < 0 || kind >= ASK3D_MAX_TEXTURE_MAPS) {
        kind = ASK3D_MAP_UNKNOWN;
    }
    mat->maps[kind].used = 1;
    mat->maps[kind].kind = kind;
    return &mat->maps[kind];
}

static void ask3d_parse_map_block(Ask3dWork *work, Ask3dScene *scene, Ask3dTextureMap *map, Ask3dMaterial *mat)
{
    Ask3dToken tok;
    Ask3dToken val;
    ask3d_fixed f;
    long iv;

    if (!ask3d_try_block(work)) {
        return;
    }
    if (map != 0) {
        map->used = 1;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MAP_NAME")) {
            if (map != 0 && ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(map->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MAP_CLASS")) {
            if (map != 0 && ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(map->map_class, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MAP_TYPE")) {
            if (map != 0 && ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(map->map_type, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*BITMAP_FILTER")) {
            if (map != 0 && ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(map->bitmap_filter, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*BITMAP")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                if (map != 0) {
                    ask3d_copy_text(map->bitmap, ASK3D_MAX_PATH, val.text);
                    map->has_bitmap = 1U;
                }
                if (mat != 0 && mat->has_bitmap == 0U) {
                    ask3d_copy_text(mat->bitmap, ASK3D_MAX_PATH, val.text);
                    mat->has_bitmap = 1U;
                }
            }
        } else if (ask3d_tok_is(&tok, "*MAP_SUBNO")) {
            if (map != 0 && ask3d_read_i32(work, &iv)) {
                map->sub_no = (int)iv;
            }
        } else if (ask3d_tok_is(&tok, "*MAP_AMOUNT")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->amount = f;
                map->has_amount = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_U_OFFSET")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->u_offset = f;
                map->has_u_offset = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_V_OFFSET")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->v_offset = f;
                map->has_v_offset = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_U_TILING")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->u_tiling = f;
                map->has_u_tiling = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_V_TILING")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->v_tiling = f;
                map->has_v_tiling = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_ANGLE")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->angle = f;
                map->has_angle = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_BLUR")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->blur = f;
                map->has_blur = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_BLUR_OFFSET")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->blur_offset = f;
                map->has_blur_offset = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_NOUSE_AMT") || ask3d_tok_is(&tok, "*UVW_NOISE_AMT")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->noise_amt = f;
                map->has_noise_amt = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_NOISE_SIZE")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->noise_size = f;
                map->has_noise_size = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_NOISE_PHASE")) {
            if (map != 0 && ask3d_read_fixed(work, &f)) {
                map->noise_phase = f;
                map->has_noise_phase = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*UVW_NOISE_LEVEL")) {
            if (map != 0 && ask3d_read_i32(work, &iv)) {
                map->noise_level = (int)iv;
                map->has_noise_level = 1U;
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_material(Ask3dWork *work, Ask3dScene *scene, int material_index)
{
    Ask3dToken tok;
    Ask3dToken val;
    Ask3dVec3 vec;
    ask3d_fixed f;
    long idx;
    int sid;
    Ask3dMaterial *mat;

    if (material_index < 0 || (unsigned int)material_index >= ASK3D_MAX_MATERIALS) {
        ask3d_skip_optional_block(work, scene);
        return;
    }
    mat = &scene->materials[material_index];
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MATERIAL_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(mat->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_CLASS")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(mat->material_class, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_SHADING")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(mat->shading, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_FALLOFF")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(mat->falloff, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_XP_TYPE")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(mat->xp_type, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_AMBIENT")) {
            if (ask3d_read_vec3(work, &vec)) {
                mat->ambient = vec;
                mat->has_ambient = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_DIFFUSE")) {
            if (ask3d_read_vec3(work, &vec)) {
                mat->diffuse = vec;
                mat->has_diffuse = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_SPECULAR")) {
            if (ask3d_read_vec3(work, &vec)) {
                mat->specular = vec;
                mat->has_specular = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_SHINE")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->shine = f;
                mat->has_shine = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_SHINESTRENGTH")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->shine_strength = f;
                mat->has_shine_strength = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_TRANSPARENCY")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->transparency = f;
                mat->has_transparency = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_SELFILLUM")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->self_illum = f;
                mat->has_self_illum = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_WIRESIZE")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->wire_size = f;
                mat->has_wire_size = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_XP_FALLOFF")) {
            if (ask3d_read_fixed(work, &f)) {
                mat->xp_falloff = f;
                mat->has_xp_falloff = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_TWOSIDED")) {
            mat->two_sided = 1U;
            ask3d_discard_same_line(work, tok.line);
        } else if (ask3d_tok_is(&tok, "*MAP_DIFFUSE")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_DIFFUSE), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_OPACITY")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_OPACITY), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_AMBIENT")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_AMBIENT), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_SPECULAR")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_SPECULAR), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_BUMP")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_BUMP), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_REFLECT")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_REFLECT), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_SELFILLUM")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_SELFILLUM), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_SHINESTRENGTH")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_SHINESTRENGTH), mat);
        } else if (ask3d_tok_is(&tok, "*MAP_FILTER")) {
            ask3d_parse_map_block(work, scene, ask3d_material_map(mat, ASK3D_MAP_FILTER), mat);
        } else if (ask3d_tok_is(&tok, "*NUMSUBMTLS")) {
            ask3d_read_i32(work, &idx);
        } else if (ask3d_tok_is(&tok, "*SUBMATERIAL")) {
            if (ask3d_read_i32(work, &idx)) {
                sid = ask3d_scene_add_submaterial(scene, (unsigned int)material_index, (unsigned int)idx);
                if (sid >= 0) {
                    ask3d_parse_material(work, scene, sid);
                } else {
                    ask3d_skip_optional_block(work, scene);
                }
            } else {
                ask3d_skip_optional_block(work, scene);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_material_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long idx;
    int mid;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MATERIAL_COUNT")) {
            if (ask3d_read_i32(work, &idx) && idx >= 0L) {
                work->material_count_hint = (unsigned int)idx;
            }
        } else if (ask3d_tok_is(&tok, "*MATERIAL")) {
            if (ask3d_read_i32(work, &idx)) {
                mid = ask3d_scene_add_material(scene, (unsigned int)idx);
                if (mid >= 0) {
                    ask3d_parse_material(work, scene, mid);
                } else {
                    ask3d_skip_optional_block(work, scene);
                }
            } else {
                ask3d_skip_optional_block(work, scene);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_tm_rows(Ask3dWork *work, Ask3dScene *scene, Ask3dVec3 *row0, Ask3dVec3 *row1, Ask3dVec3 *row2, Ask3dVec3 *row3, unsigned int *has_transform, Ask3dVec3 *rot_axis, ask3d_fixed *rot_angle, Ask3dVec3 *scale, unsigned int *has_rot, unsigned int *has_scale)
{
    Ask3dToken tok;
    Ask3dVec3 vec;
    ask3d_fixed f;

    if (!ask3d_try_block(work)) {
        return;
    }
    if (has_transform != 0) {
        *has_transform = 1U;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*TM_ROW0")) {
            if (ask3d_read_vec3(work, &vec) && row0 != 0) {
                *row0 = vec;
            }
        } else if (ask3d_tok_is(&tok, "*TM_ROW1")) {
            if (ask3d_read_vec3(work, &vec) && row1 != 0) {
                *row1 = vec;
            }
        } else if (ask3d_tok_is(&tok, "*TM_ROW2")) {
            if (ask3d_read_vec3(work, &vec) && row2 != 0) {
                *row2 = vec;
            }
        } else if (ask3d_tok_is(&tok, "*TM_ROW3") || ask3d_tok_is(&tok, "*TM_POS")) {
            if (ask3d_read_vec3(work, &vec) && row3 != 0) {
                *row3 = vec;
            }
        } else if (ask3d_tok_is(&tok, "*TM_ROTAXIS")) {
            if (ask3d_read_vec3(work, &vec) && rot_axis != 0) {
                *rot_axis = vec;
                if (has_rot != 0) {
                    *has_rot = 1U;
                }
            }
        } else if (ask3d_tok_is(&tok, "*TM_ROTANGLE")) {
            if (ask3d_read_fixed(work, &f) && rot_angle != 0) {
                *rot_angle = f;
                if (has_rot != 0) {
                    *has_rot = 1U;
                }
            }
        } else if (ask3d_tok_is(&tok, "*TM_SCALE")) {
            if (ask3d_read_vec3(work, &vec) && scale != 0) {
                *scale = vec;
                if (has_scale != 0) {
                    *has_scale = 1U;
                }
            }
        } else if (ask3d_tok_is(&tok, "*TM_SCALEAXIS")) {
            ask3d_read_vec3(work, &vec);
        } else if (ask3d_tok_is(&tok, "*TM_SCALEAXISANG")) {
            ask3d_read_fixed(work, &f);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static unsigned int ask3d_map_vertex(Ask3dWork *work, Ask3dScene *scene, unsigned int local_index)
{
    if (local_index >= ASK3D_MAX_LOCAL_VERTICES) {
        ask3d_warn(scene, "local vertex index outside workspace");
        return 0U;
    }
    if (work->local_vset[local_index] == 0U) {
        ask3d_warn(scene, "face references missing vertex");
        return 0U;
    }
    return work->local_vmap[local_index];
}

static unsigned int ask3d_map_tvertex(Ask3dWork *work, Ask3dScene *scene, unsigned int local_index, unsigned int *ok)
{
    if (local_index >= ASK3D_MAX_LOCAL_TVERTICES) {
        ask3d_warn(scene, "local texture vertex index outside workspace");
        *ok = 0U;
        return 0U;
    }
    if (work->local_tvset[local_index] == 0U) {
        ask3d_warn(scene, "face references missing texture vertex");
        *ok = 0U;
        return 0U;
    }
    *ok = 1U;
    return work->local_tvmap[local_index];
}

static unsigned int ask3d_map_cvertex(Ask3dWork *work, Ask3dScene *scene, unsigned int local_index, unsigned int *ok)
{
    if (local_index >= ASK3D_MAX_LOCAL_CVERTICES) {
        ask3d_warn(scene, "local color vertex index outside workspace");
        *ok = 0U;
        return 0U;
    }
    if (work->local_cvset[local_index] == 0U) {
        ask3d_warn(scene, "face references missing color vertex");
        *ok = 0U;
        return 0U;
    }
    *ok = 1U;
    return work->local_cvmap[local_index];
}

static unsigned int ask3d_map_face(Ask3dWork *work, Ask3dScene *scene, unsigned int local_index, unsigned int *ok)
{
    if (local_index < ASK3D_MAX_LOCAL_FACES && work->local_fset[local_index] != 0U) {
        *ok = 1U;
        return work->local_fmap[local_index];
    }
    if (local_index < work->local_face_count) {
        *ok = 1U;
        return work->local_base_face + local_index;
    }
    ask3d_warn(scene, "face side data references missing face");
    *ok = 0U;
    return 0U;
}

static void ask3d_parse_vertex_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long idx;
    Ask3dVec3 vec;
    unsigned int gidx;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_VERTEX")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_vec3(work, &vec)) {
                if (idx >= 0L && (unsigned long)idx < ASK3D_MAX_LOCAL_VERTICES) {
                    if (ask3d_scene_add_vertex(scene, &vec, &gidx)) {
                        work->local_vmap[(unsigned int)idx] = gidx;
                        work->local_vset[(unsigned int)idx] = 1U;
                    }
                } else {
                    ask3d_warn(scene, "vertex index outside workspace");
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_tvertex_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long idx;
    ask3d_fixed ignored;
    Ask3dVec2 uv;
    unsigned int gidx;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_TVERT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_fixed(work, &uv.u) && ask3d_read_fixed(work, &uv.v)) {
                ask3d_try_read_fixed(work, &ignored);
                if (idx >= 0L && (unsigned long)idx < ASK3D_MAX_LOCAL_TVERTICES) {
                    if (ask3d_scene_add_tvertex(scene, &uv, &gidx)) {
                        work->local_tvmap[(unsigned int)idx] = gidx;
                        work->local_tvset[(unsigned int)idx] = 1U;
                    }
                } else {
                    ask3d_warn(scene, "texture vertex index outside workspace");
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_cvertex_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long idx;
    Ask3dVec3 col;
    unsigned int gidx;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_VERTCOL") || ask3d_tok_is(&tok, "*MESH_CVERT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_vec3(work, &col)) {
                if (idx >= 0L && (unsigned long)idx < ASK3D_MAX_LOCAL_CVERTICES) {
                    if (ask3d_scene_add_cvertex(scene, &col, &gidx)) {
                        work->local_cvmap[(unsigned int)idx] = gidx;
                        work->local_cvset[(unsigned int)idx] = 1U;
                    }
                } else {
                    ask3d_warn(scene, "color vertex index outside workspace");
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_mesh_face(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long face_idx;
    long tmp;
    unsigned int la;
    unsigned int lb;
    unsigned int lc;
    Ask3dFace face;
    unsigned int gface;

    face_idx = 0L;
    la = 0U;
    lb = 0U;
    lc = 0U;
    memset(&face, 0, sizeof(Ask3dFace));
    face.material_id = -1;
    face.edge_visible[0] = 1U;
    face.edge_visible[1] = 1U;
    face.edge_visible[2] = 1U;

    ask3d_read_i32(work, &face_idx);

    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            ask3d_push(work, &tok);
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_FACE")) {
            ask3d_push(work, &tok);
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_SMOOTHING")) {
            if (ask3d_read_i32(work, &tmp)) {
                face.smoothing = (unsigned int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "*MESH_MTLID")) {
            if (ask3d_read_i32(work, &tmp)) {
                face.material_id = (int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "A:") || ask3d_tok_is(&tok, "A")) {
            if (ask3d_read_i32(work, &tmp)) {
                la = (unsigned int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "B:") || ask3d_tok_is(&tok, "B")) {
            if (ask3d_read_i32(work, &tmp)) {
                lb = (unsigned int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "C:") || ask3d_tok_is(&tok, "C")) {
            if (ask3d_read_i32(work, &tmp)) {
                lc = (unsigned int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "AB:") || ask3d_tok_is(&tok, "AB")) {
            if (ask3d_read_i32(work, &tmp)) {
                face.edge_visible[0] = (unsigned char)(tmp != 0L);
            }
        } else if (ask3d_tok_is(&tok, "BC:") || ask3d_tok_is(&tok, "BC")) {
            if (ask3d_read_i32(work, &tmp)) {
                face.edge_visible[1] = (unsigned char)(tmp != 0L);
            }
        } else if (ask3d_tok_is(&tok, "CA:") || ask3d_tok_is(&tok, "CA")) {
            if (ask3d_read_i32(work, &tmp)) {
                face.edge_visible[2] = (unsigned char)(tmp != 0L);
            }
        } else if (ask3d_tok_starts(&tok, "*")) {
            ask3d_push(work, &tok);
            break;
        } else {
        }
    }

    face.v[0] = ask3d_map_vertex(work, scene, la);
    face.v[1] = ask3d_map_vertex(work, scene, lb);
    face.v[2] = ask3d_map_vertex(work, scene, lc);

    if (ask3d_scene_add_face(scene, &face, &gface)) {
        if (face_idx >= 0L && (unsigned long)face_idx < ASK3D_MAX_LOCAL_FACES) {
            work->local_fmap[(unsigned int)face_idx] = gface;
            work->local_fset[(unsigned int)face_idx] = 1U;
        }
        work->local_face_count++;
    }
}

static void ask3d_parse_face_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_FACE")) {
            ask3d_parse_mesh_face(work, scene);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_tface_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long face_idx;
    long a;
    long b;
    long c;
    unsigned int ok0;
    unsigned int ok1;
    unsigned int ok2;
    unsigned int okf;
    unsigned int gface;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_TFACE")) {
            if (ask3d_read_i32(work, &face_idx) && ask3d_read_i32(work, &a) && ask3d_read_i32(work, &b) && ask3d_read_i32(work, &c)) {
                if (face_idx >= 0L) {
                    gface = ask3d_map_face(work, scene, (unsigned int)face_idx, &okf);
                    if (okf != 0U && gface < scene->face_count) {
                        scene->faces[gface].tv[0] = ask3d_map_tvertex(work, scene, (unsigned int)a, &ok0);
                        scene->faces[gface].tv[1] = ask3d_map_tvertex(work, scene, (unsigned int)b, &ok1);
                        scene->faces[gface].tv[2] = ask3d_map_tvertex(work, scene, (unsigned int)c, &ok2);
                        if (ok0 != 0U && ok1 != 0U && ok2 != 0U) {
                            scene->faces[gface].has_tv = 1U;
                        }
                    }
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}


static void ask3d_reset_channel_uv(Ask3dWork *work)
{
    memset(work->channel_tvset, 0, sizeof(work->channel_tvset));
    memset(work->channel_tvmap, 0, sizeof(work->channel_tvmap));
}

static unsigned int ask3d_map_channel_tvertex(Ask3dWork *work, Ask3dScene *scene, unsigned int local_index, unsigned int *ok)
{
    if (local_index >= ASK3D_MAX_LOCAL_TVERTICES) {
        ask3d_warn(scene, "channel texture vertex index outside workspace");
        *ok = 0U;
        return 0U;
    }
    if (work->channel_tvset[local_index] == 0U) {
        ask3d_warn(scene, "channel side data references missing texture vertex");
        *ok = 0U;
        return 0U;
    }
    *ok = 1U;
    return work->channel_tvmap[local_index];
}

static void ask3d_parse_channel_tvertex_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long idx;
    ask3d_fixed ignored;
    Ask3dVec2 uv;
    unsigned int gidx;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_TVERT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_fixed(work, &uv.u) && ask3d_read_fixed(work, &uv.v)) {
                ask3d_try_read_fixed(work, &ignored);
                if (idx >= 0L && (unsigned long)idx < ASK3D_MAX_LOCAL_TVERTICES) {
                    if (ask3d_scene_add_tvertex(scene, &uv, &gidx)) {
                        work->channel_tvmap[(unsigned int)idx] = gidx;
                        work->channel_tvset[(unsigned int)idx] = 1U;
                    }
                } else {
                    ask3d_warn(scene, "channel texture vertex index outside workspace");
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_channel_tface_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long face_idx;
    long a;
    long b;
    long c;
    unsigned int ok0;
    unsigned int ok1;
    unsigned int ok2;
    unsigned int okf;
    unsigned int gface;
    Ask3dUvFace uvf;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_TFACE")) {
            if (ask3d_read_i32(work, &face_idx) && ask3d_read_i32(work, &a) && ask3d_read_i32(work, &b) && ask3d_read_i32(work, &c)) {
                if (face_idx >= 0L) {
                    gface = ask3d_map_face(work, scene, (unsigned int)face_idx, &okf);
                    if (okf != 0U && gface < scene->face_count && work->current_uv_channel_index >= 0) {
                        memset(&uvf, 0, sizeof(Ask3dUvFace));
                        uvf.used = 1;
                        uvf.uv_channel_index = work->current_uv_channel_index;
                        uvf.face_index = gface;
                        uvf.tv[0] = ask3d_map_channel_tvertex(work, scene, (unsigned int)a, &ok0);
                        uvf.tv[1] = ask3d_map_channel_tvertex(work, scene, (unsigned int)b, &ok1);
                        uvf.tv[2] = ask3d_map_channel_tvertex(work, scene, (unsigned int)c, &ok2);
                        if (ok0 != 0U && ok1 != 0U && ok2 != 0U) {
                            ask3d_scene_add_uv_face(scene, &uvf, 0);
                        }
                    }
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_cface_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long face_idx;
    long a;
    long b;
    long c;
    unsigned int ok0;
    unsigned int ok1;
    unsigned int ok2;
    unsigned int okf;
    unsigned int gface;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_CFACE")) {
            if (ask3d_read_i32(work, &face_idx) && ask3d_read_i32(work, &a) && ask3d_read_i32(work, &b) && ask3d_read_i32(work, &c)) {
                if (face_idx >= 0L) {
                    gface = ask3d_map_face(work, scene, (unsigned int)face_idx, &okf);
                    if (okf != 0U && gface < scene->face_count) {
                        scene->faces[gface].cv[0] = ask3d_map_cvertex(work, scene, (unsigned int)a, &ok0);
                        scene->faces[gface].cv[1] = ask3d_map_cvertex(work, scene, (unsigned int)b, &ok1);
                        scene->faces[gface].cv[2] = ask3d_map_cvertex(work, scene, (unsigned int)c, &ok2);
                        if (ok0 != 0U && ok1 != 0U && ok2 != 0U) {
                            scene->faces[gface].has_cv = 1U;
                        }
                    }
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_set_vertex_normal(Ask3dScene *scene, unsigned int gface, unsigned int gv, const Ask3dVec3 *normal)
{
    unsigned int i;
    Ask3dFace *face;
    if (scene == 0 || normal == 0 || gface >= scene->face_count) {
        return;
    }
    face = &scene->faces[gface];
    i = 0U;
    while (i < 3U) {
        if (face->v[i] == gv && face->has_vertex_normal[i] == 0U) {
            face->vertex_normal[i] = *normal;
            face->has_vertex_normal[i] = 1U;
            return;
        }
        i++;
    }
}

static void ask3d_parse_normal_list(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    long face_idx;
    long local_vertex;
    Ask3dVec3 vec;
    unsigned int okf;
    unsigned int current_face;
    unsigned int global_vertex;

    current_face = 0U;
    okf = 0U;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_FACENORMAL")) {
            if (ask3d_read_i32(work, &face_idx) && ask3d_read_vec3(work, &vec) && face_idx >= 0L) {
                current_face = ask3d_map_face(work, scene, (unsigned int)face_idx, &okf);
                if (okf != 0U && current_face < scene->face_count) {
                    scene->faces[current_face].normal = vec;
                    scene->faces[current_face].has_normal = 1U;
                }
            }
        } else if (ask3d_tok_is(&tok, "*MESH_VERTEXNORMAL")) {
            if (ask3d_read_i32(work, &local_vertex) && ask3d_read_vec3(work, &vec)) {
                if (okf != 0U && local_vertex >= 0L) {
                    global_vertex = ask3d_map_vertex(work, scene, (unsigned int)local_vertex);
                    ask3d_set_vertex_normal(scene, current_face, global_vertex, &vec);
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}


static void ask3d_parse_mapping_channel(Ask3dWork *work, Ask3dScene *scene, Ask3dObject *obj, long channel)
{
    Ask3dToken tok;
    long ignored;
    Ask3dUvChannel uvch;
    unsigned int channel_index;
    unsigned int first_tvertex;
    unsigned int first_uv_face;
    unsigned int start_tvertex;
    unsigned int start_uv_face;

    if (!ask3d_try_block(work)) {
        return;
    }
    if (channel == 1L) {
        while (1) {
            ask3d_next(work, &tok);
            if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
                break;
            }
            if (ask3d_tok_is(&tok, "*MESH_NUMTVERTEX")) {
                ask3d_read_i32(work, &ignored);
            } else if (ask3d_tok_is(&tok, "*MESH_TVERTLIST")) {
                ask3d_parse_tvertex_list(work, scene);
            } else if (ask3d_tok_is(&tok, "*MESH_NUMTVFACES")) {
                ask3d_read_i32(work, &ignored);
            } else if (ask3d_tok_is(&tok, "*MESH_TFACELIST")) {
                ask3d_parse_tface_list(work, scene);
            } else {
                ask3d_skip_optional_block(work, scene);
            }
        }
        return;
    }

    memset(&uvch, 0, sizeof(Ask3dUvChannel));
    uvch.used = 1;
    uvch.object_index = work->current_object_index;
    uvch.channel = (int)channel;
    uvch.first_tvertex = scene->tvertex_count;
    uvch.first_uv_face = scene->uv_face_count;
    start_tvertex = scene->tvertex_count;
    start_uv_face = scene->uv_face_count;
    channel_index = 0U;
    if (!ask3d_scene_add_uv_channel(scene, &uvch, &channel_index)) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    if (obj != 0) {
        obj->extra_uv_channel_count++;
    }
    work->current_uv_channel_index = (int)channel_index;
    work->current_uv_channel_id = (int)channel;
    ask3d_reset_channel_uv(work);

    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_NUMTVERTEX")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_TVERTLIST")) {
            ask3d_parse_channel_tvertex_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMTVFACES")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_TFACELIST")) {
            ask3d_parse_channel_tface_list(work, scene);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
    first_tvertex = scene->uv_channels[channel_index].first_tvertex;
    first_uv_face = scene->uv_channels[channel_index].first_uv_face;
    if (first_tvertex == start_tvertex) {
        scene->uv_channels[channel_index].tvertex_count = scene->tvertex_count - start_tvertex;
    }
    if (first_uv_face == start_uv_face) {
        scene->uv_channels[channel_index].uv_face_count = scene->uv_face_count - start_uv_face;
    }
    work->current_uv_channel_index = -1;
    work->current_uv_channel_id = 0;
}

static void ask3d_parse_mesh(Ask3dWork *work, Ask3dScene *scene, Ask3dObject *obj, int object_index)
{
    Ask3dToken tok;
    long ignored;
    long channel;

    if (!ask3d_try_block(work)) {
        return;
    }
    ask3d_reset_local(work);
    work->current_object_index = object_index;
    work->local_base_face = scene->face_count;

    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*TIMEVALUE")) {
            if (ask3d_read_i32(work, &ignored)) {
                obj->first_timevalue = (int)ignored;
            }
        } else if (ask3d_tok_is(&tok, "*MESH_NUMVERTEX")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMFACES")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_VERTEX_LIST")) {
            ask3d_parse_vertex_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_FACE_LIST")) {
            ask3d_parse_face_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMTVERTEX")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_TVERTLIST")) {
            ask3d_parse_tvertex_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMTVFACES")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_TFACELIST")) {
            ask3d_parse_tface_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMCVERTEX")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_CVERTLIST")) {
            ask3d_parse_cvertex_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_NUMCVFACES")) {
            ask3d_read_i32(work, &ignored);
        } else if (ask3d_tok_is(&tok, "*MESH_CFACELIST")) {
            ask3d_parse_cface_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*MESH_MAPPINGCHANNEL")) {
            if (ask3d_read_i32(work, &channel)) {
                ask3d_parse_mapping_channel(work, scene, obj, channel);
            } else {
                ask3d_skip_optional_block(work, scene);
            }
        } else if (ask3d_tok_is(&tok, "*MESH_NORMALS")) {
            ask3d_parse_normal_list(work, scene);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_snapshot_add_bounds(Ask3dMeshSnapshot *snap, const Ask3dVec3 *v)
{
    if (snap == 0 || v == 0) {
        return;
    }
    if (snap->has_bounds == 0U) {
        snap->bounds_min = *v;
        snap->bounds_max = *v;
        snap->has_bounds = 1U;
        return;
    }
    if (v->x < snap->bounds_min.x) snap->bounds_min.x = v->x;
    if (v->y < snap->bounds_min.y) snap->bounds_min.y = v->y;
    if (v->z < snap->bounds_min.z) snap->bounds_min.z = v->z;
    if (v->x > snap->bounds_max.x) snap->bounds_max.x = v->x;
    if (v->y > snap->bounds_max.y) snap->bounds_max.y = v->y;
    if (v->z > snap->bounds_max.z) snap->bounds_max.z = v->z;
}

static void ask3d_parse_snapshot_vertex_list(Ask3dWork *work, Ask3dScene *scene, Ask3dMeshSnapshot *snap)
{
    Ask3dToken tok;
    long idx;
    Ask3dVec3 vec;
    (void)scene;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_VERTEX")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_vec3(work, &vec)) {
                snap->vertex_count++;
                ask3d_snapshot_add_bounds(snap, &vec);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_snapshot_tvertex_list(Ask3dWork *work, Ask3dScene *scene, Ask3dMeshSnapshot *snap)
{
    Ask3dToken tok;
    long idx;
    ask3d_fixed a;
    ask3d_fixed b;
    ask3d_fixed c;
    (void)scene;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_TVERT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_fixed(work, &a) && ask3d_read_fixed(work, &b)) {
                ask3d_try_read_fixed(work, &c);
                snap->tvertex_count++;
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_snapshot_face_list(Ask3dWork *work, Ask3dScene *scene, Ask3dMeshSnapshot *snap)
{
    Ask3dToken tok;
    unsigned long line;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH_FACE")) {
            snap->face_count++;
            line = tok.line;
            ask3d_discard_same_line(work, line);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_mesh_snapshot(Ask3dWork *work, Ask3dScene *scene, int object_index)
{
    Ask3dToken tok;
    long ignored;
    long channel;
    Ask3dMeshSnapshot snap;

    memset(&snap, 0, sizeof(Ask3dMeshSnapshot));
    snap.used = 1;
    snap.object_index = object_index;
    snap.timevalue = -2147483647;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*TIMEVALUE")) {
            if (ask3d_read_i32(work, &ignored)) {
                snap.timevalue = (int)ignored;
            }
        } else if (ask3d_tok_is(&tok, "*MESH_VERTEX_LIST")) {
            ask3d_parse_snapshot_vertex_list(work, scene, &snap);
        } else if (ask3d_tok_is(&tok, "*MESH_FACE_LIST")) {
            ask3d_parse_snapshot_face_list(work, scene, &snap);
        } else if (ask3d_tok_is(&tok, "*MESH_TVERTLIST")) {
            ask3d_parse_snapshot_tvertex_list(work, scene, &snap);
        } else if (ask3d_tok_is(&tok, "*MESH_MAPPINGCHANNEL")) {
            if (ask3d_read_i32(work, &channel)) {
                ask3d_skip_optional_block(work, scene);
            } else {
                ask3d_skip_optional_block(work, scene);
            }
        } else if (ask3d_tok_is(&tok, "*MESH_NUMVERTEX") || ask3d_tok_is(&tok, "*MESH_NUMFACES") || ask3d_tok_is(&tok, "*MESH_NUMTVERTEX") || ask3d_tok_is(&tok, "*MESH_NUMTVFACES")) {
            ask3d_read_i32(work, &ignored);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
    ask3d_scene_add_mesh_snapshot(scene, &snap, 0);
}

static void ask3d_parse_mesh_animation(Ask3dWork *work, Ask3dScene *scene, int object_index)
{
    Ask3dToken tok;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*MESH")) {
            ask3d_parse_mesh_snapshot(work, scene, object_index);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_anim_track(Ask3dWork *work, Ask3dScene *scene, int object_index, int track_type)
{
    Ask3dToken tok;
    long timevalue;
    Ask3dAnimSample sample;
    unsigned long line;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if ((track_type == ASK3D_ANIM_POS && ask3d_tok_is(&tok, "*CONTROL_POS_SAMPLE")) ||
            (track_type == ASK3D_ANIM_ROT && ask3d_tok_is(&tok, "*CONTROL_ROT_SAMPLE")) ||
            (track_type == ASK3D_ANIM_SCALE && ask3d_tok_is(&tok, "*CONTROL_SCALE_SAMPLE"))) {
            memset(&sample, 0, sizeof(Ask3dAnimSample));
            sample.used = 1;
            sample.object_index = object_index;
            sample.track_type = track_type;
            line = tok.line;
            if (ask3d_read_i32(work, &timevalue) && ask3d_read_vec3(work, &sample.value)) {
                sample.timevalue = timevalue;
                if (track_type == ASK3D_ANIM_ROT) {
                    if (ask3d_try_read_fixed(work, &sample.aux)) {
                        sample.has_aux = 1U;
                    }
                }
                ask3d_scene_add_anim_sample(scene, &sample, 0);
                ask3d_discard_same_line(work, line);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_tm_animation(Ask3dWork *work, Ask3dScene *scene, int object_index)
{
    Ask3dToken tok;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*CONTROL_POS_TRACK")) {
            ask3d_parse_anim_track(work, scene, object_index, ASK3D_ANIM_POS);
        } else if (ask3d_tok_is(&tok, "*CONTROL_ROT_TRACK")) {
            ask3d_parse_anim_track(work, scene, object_index, ASK3D_ANIM_ROT);
        } else if (ask3d_tok_is(&tok, "*CONTROL_SCALE_TRACK")) {
            ask3d_parse_anim_track(work, scene, object_index, ASK3D_ANIM_SCALE);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_add_base_snapshot(Ask3dScene *scene, int object_index, Ask3dObject *obj)
{
    Ask3dMeshSnapshot snap;
    if (scene == 0 || obj == 0 || obj->vertex_count == 0U) {
        return;
    }
    memset(&snap, 0, sizeof(Ask3dMeshSnapshot));
    snap.used = 1;
    snap.object_index = object_index;
    snap.timevalue = obj->first_timevalue;
    snap.vertex_count = obj->vertex_count;
    snap.tvertex_count = obj->tvertex_count;
    snap.face_count = obj->face_count;
    snap.has_bounds = obj->has_bounds;
    snap.bounds_min = obj->bounds_min;
    snap.bounds_max = obj->bounds_max;
    ask3d_scene_add_mesh_snapshot(scene, &snap, 0);
}

static void ask3d_parse_geomobject(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    Ask3dToken val;
    long tmp;
    int oid;
    Ask3dObject *obj;

    oid = ask3d_scene_begin_object(scene);
    if (!ask3d_try_block(work)) {
        return;
    }
    if (oid < 0) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    obj = &scene->objects[oid];

    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*NODE_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(obj->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_PARENT")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(obj->parent_name, ASK3D_MAX_NAME, val.text);
                obj->has_parent = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*WIREFRAME_COLOR")) {
            if (ask3d_read_vec3(work, &obj->wire_color)) {
                obj->has_wire_color = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*PROP_MOTIONBLUR")) {
            if (ask3d_read_i32(work, &tmp)) { obj->prop_motionblur = tmp != 0L ? 1U : 0U; }
        } else if (ask3d_tok_is(&tok, "*PROP_CASTSHADOW")) {
            if (ask3d_read_i32(work, &tmp)) { obj->prop_castshadow = tmp != 0L ? 1U : 0U; }
        } else if (ask3d_tok_is(&tok, "*PROP_RECVSHADOW")) {
            if (ask3d_read_i32(work, &tmp)) { obj->prop_recvshadow = tmp != 0L ? 1U : 0U; }
        } else if (ask3d_tok_is(&tok, "*MATERIAL_REF")) {
            if (ask3d_read_i32(work, &tmp)) {
                obj->material_ref = (int)tmp;
            }
        } else if (ask3d_tok_is(&tok, "*NODE_TM")) {
            ask3d_parse_tm_rows(work, scene, &obj->tm_row0, &obj->tm_row1, &obj->tm_row2, &obj->tm_row3, &obj->has_transform, &obj->tm_rot_axis, &obj->tm_rot_angle, &obj->tm_scale, &obj->has_tm_rot, &obj->has_tm_scale);
        } else if (ask3d_tok_is(&tok, "*MESH")) {
            obj->mesh_count++;
            if (obj->mesh_count == 1U) {
                ask3d_parse_mesh(work, scene, obj, oid);
            } else {
                ask3d_parse_mesh_snapshot(work, scene, oid);
            }
        } else if (ask3d_tok_is(&tok, "*MESH_ANIMATION")) {
            ask3d_parse_mesh_animation(work, scene, oid);
        } else if (ask3d_tok_is(&tok, "*TM_ANIMATION")) {
            ask3d_parse_tm_animation(work, scene, oid);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }

    obj->vertex_count = scene->vertex_count - obj->first_vertex;
    obj->face_count = scene->face_count - obj->first_face;
    obj->tvertex_count = scene->tvertex_count - obj->first_tvertex;
    obj->cvertex_count = scene->cvertex_count - obj->first_cvertex;
    ask3d_scene_compute_object_bounds(scene, obj);
    ask3d_add_base_snapshot(scene, oid, obj);
}


static void ask3d_add_scalar_setting(Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, ask3d_fixed value)
{
    Ask3dSettingSample sample;
    memset(&sample, 0, sizeof(Ask3dSettingSample));
    sample.used = 1;
    sample.node_type = node_type;
    sample.node_index = node_index;
    sample.setting_type = setting_type;
    sample.timevalue = timevalue;
    sample.scalar = value;
    sample.has_scalar = 1U;
    ask3d_scene_add_setting_sample(scene, &sample, 0);
}

static void ask3d_add_vec_setting(Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, const Ask3dVec3 *value)
{
    Ask3dSettingSample sample;
    if (value == 0) {
        return;
    }
    memset(&sample, 0, sizeof(Ask3dSettingSample));
    sample.used = 1;
    sample.node_type = node_type;
    sample.node_index = node_index;
    sample.setting_type = setting_type;
    sample.timevalue = timevalue;
    sample.vec = *value;
    sample.has_vec = 1U;
    ask3d_scene_add_setting_sample(scene, &sample, 0);
}

static void ask3d_parse_camera_settings(Ask3dWork *work, Ask3dScene *scene, Ask3dCamera *cam, int camera_index)
{
    Ask3dToken tok;
    long timevalue;
    ask3d_fixed f;
    timevalue = 0L;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*TIMEVALUE")) {
            ask3d_read_i32(work, &timevalue);
        } else if (ask3d_tok_is(&tok, "*CAMERA_FOV")) {
            if (ask3d_read_fixed(work, &f)) {
                cam->fov = f;
                cam->has_fov = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_CAMERA, camera_index, ASK3D_SETTING_CAMERA_FOV, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*CAMERA_NEAR")) {
            if (ask3d_read_fixed(work, &f)) {
                cam->near_clip = f;
                cam->has_near_clip = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_CAMERA, camera_index, ASK3D_SETTING_CAMERA_NEAR, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*CAMERA_FAR")) {
            if (ask3d_read_fixed(work, &f)) {
                cam->far_clip = f;
                cam->has_far_clip = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_CAMERA, camera_index, ASK3D_SETTING_CAMERA_FAR, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*CAMERA_TDIST")) {
            if (ask3d_read_fixed(work, &f)) {
                cam->target_distance = f;
                cam->has_target_distance = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_CAMERA, camera_index, ASK3D_SETTING_CAMERA_TDIST, timevalue, f);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_cameraobject(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    Ask3dToken val;
    int cid;
    Ask3dCamera *cam;

    cid = ask3d_scene_begin_camera(scene);
    if (!ask3d_try_block(work)) {
        return;
    }
    if (cid < 0) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    cam = &scene->cameras[cid];
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*NODE_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(cam->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_PARENT")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(cam->parent_name, ASK3D_MAX_NAME, val.text);
                cam->has_parent = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*CAMERA_TYPE")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(cam->camera_type, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_TM")) {
            ask3d_parse_tm_rows(work, scene, &cam->tm_row0, &cam->tm_row1, &cam->tm_row2, &cam->tm_row3, &cam->has_transform, 0, 0, 0, 0, 0);
        } else if (ask3d_tok_is(&tok, "*CAMERA_SETTINGS")) {
            ask3d_parse_camera_settings(work, scene, cam, cid);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_light_settings(Ask3dWork *work, Ask3dScene *scene, Ask3dLight *light, int light_index)
{
    Ask3dToken tok;
    Ask3dToken val;
    long timevalue;
    Ask3dVec3 vec;
    ask3d_fixed f;
    timevalue = 0L;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*TIMEVALUE")) {
            ask3d_read_i32(work, &timevalue);
        } else if (ask3d_tok_is(&tok, "*LIGHT_TYPE")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(light->light_type, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*LIGHT_COLOR")) {
            if (ask3d_read_vec3(work, &vec)) {
                light->color = vec;
                light->has_color = 1U;
                ask3d_add_vec_setting(scene, ASK3D_NODE_LIGHT, light_index, ASK3D_SETTING_LIGHT_COLOR, timevalue, &vec);
            }
        } else if (ask3d_tok_is(&tok, "*LIGHT_INTENS")) {
            if (ask3d_read_fixed(work, &f)) {
                light->intensity = f;
                light->has_intensity = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_LIGHT, light_index, ASK3D_SETTING_LIGHT_INTENS, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*LIGHT_HOTSPOT")) {
            if (ask3d_read_fixed(work, &f)) {
                light->hotspot = f;
                light->has_hotspot = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_LIGHT, light_index, ASK3D_SETTING_LIGHT_HOTSPOT, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*LIGHT_FALLOFF")) {
            if (ask3d_read_fixed(work, &f)) {
                light->falloff = f;
                light->has_falloff = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_LIGHT, light_index, ASK3D_SETTING_LIGHT_FALLOFF, timevalue, f);
            }
        } else if (ask3d_tok_is(&tok, "*LIGHT_DECAY")) {
            if (ask3d_read_fixed(work, &f)) {
                light->decay = f;
                light->has_decay = 1U;
                ask3d_add_scalar_setting(scene, ASK3D_NODE_LIGHT, light_index, ASK3D_SETTING_LIGHT_DECAY, timevalue, f);
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_lightobject(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    Ask3dToken val;
    int lid;
    Ask3dLight *light;

    lid = ask3d_scene_begin_light(scene);
    if (!ask3d_try_block(work)) {
        return;
    }
    if (lid < 0) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    light = &scene->lights[lid];
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*NODE_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(light->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_PARENT")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(light->parent_name, ASK3D_MAX_NAME, val.text);
                light->has_parent = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*NODE_TM")) {
            ask3d_parse_tm_rows(work, scene, &light->tm_row0, &light->tm_row1, &light->tm_row2, &light->tm_row3, &light->has_transform, 0, 0, 0, 0, 0);
        } else if (ask3d_tok_is(&tok, "*LIGHT_SETTINGS")) {
            ask3d_parse_light_settings(work, scene, light, lid);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}


static void ask3d_parse_helperobject(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    Ask3dToken val;
    int hid;
    Ask3dHelper *helper;

    hid = ask3d_scene_begin_helper(scene);
    if (!ask3d_try_block(work)) {
        return;
    }
    if (hid < 0) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    helper = &scene->helpers[hid];
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*NODE_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(helper->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_PARENT")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(helper->parent_name, ASK3D_MAX_NAME, val.text);
                helper->has_parent = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*NODE_TM")) {
            ask3d_parse_tm_rows(work, scene, &helper->tm_row0, &helper->tm_row1, &helper->tm_row2, &helper->tm_row3, &helper->has_transform, 0, 0, 0, 0, 0);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_add_shape_point3(Ask3dScene *scene, Ask3dShape *shape, int shape_index, int spline_index, int point_index, const Ask3dVec3 *p)
{
    Ask3dShapePoint point;
    if (scene == 0 || shape == 0 || p == 0) {
        return;
    }
    memset(&point, 0, sizeof(Ask3dShapePoint));
    point.used = 1;
    point.shape_index = shape_index;
    point.spline_index = spline_index;
    point.point_index = point_index;
    point.p = *p;
    if (ask3d_scene_add_shape_point(scene, &point, 0)) {
        shape->point_count++;
    }
}

static void ask3d_add_shape_knot(Ask3dScene *scene, Ask3dShape *shape, int shape_index, int spline_index, int point_index, const Ask3dVec3 *p, const Ask3dVec3 *in_vec, const Ask3dVec3 *out_vec)
{
    Ask3dShapePoint point;
    if (scene == 0 || shape == 0 || p == 0) {
        return;
    }
    memset(&point, 0, sizeof(Ask3dShapePoint));
    point.used = 1;
    point.shape_index = shape_index;
    point.spline_index = spline_index;
    point.point_index = point_index;
    point.p = *p;
    if (in_vec != 0 && out_vec != 0) {
        point.in_vec = *in_vec;
        point.out_vec = *out_vec;
        point.has_in_out = 1U;
    }
    if (ask3d_scene_add_shape_point(scene, &point, 0)) {
        shape->point_count++;
    }
}

static void ask3d_parse_shape_line(Ask3dWork *work, Ask3dScene *scene, Ask3dShape *shape, int shape_index, int spline_index)
{
    Ask3dToken tok;
    long idx;
    Ask3dVec3 p;
    Ask3dVec3 a;
    Ask3dVec3 b;
    long tmp;

    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*SHAPE_CLOSED")) {
            if (ask3d_read_i32(work, &tmp)) {
                shape->closed = tmp != 0L ? 1U : 0U;
            }
        } else if (ask3d_tok_is(&tok, "*SHAPE_VERTEX") || ask3d_tok_is(&tok, "*SHAPE_POINT") || ask3d_tok_is(&tok, "*SHAPE_INTERP_POINT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_vec3(work, &p)) {
                ask3d_add_shape_point3(scene, shape, shape_index, spline_index, (int)idx, &p);
            }
        } else if (ask3d_tok_is(&tok, "*SHAPE_KNOT")) {
            if (ask3d_read_i32(work, &idx) && ask3d_read_vec3(work, &p)) {
                if (ask3d_read_vec3(work, &a) && ask3d_read_vec3(work, &b)) {
                    ask3d_add_shape_knot(scene, shape, shape_index, spline_index, (int)idx, &p, &a, &b);
                } else {
                    ask3d_add_shape_point3(scene, shape, shape_index, spline_index, (int)idx, &p);
                }
            }
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

static void ask3d_parse_shapeobject(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    Ask3dToken val;
    int sid;
    int spline_index;
    Ask3dShape *shape;

    sid = ask3d_scene_begin_shape(scene);
    if (!ask3d_try_block(work)) {
        return;
    }
    if (sid < 0) {
        ask3d_skip_block_open(work, scene);
        return;
    }
    spline_index = 0;
    shape = &scene->shapes[sid];
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*NODE_NAME")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(shape->name, ASK3D_MAX_NAME, val.text);
            }
        } else if (ask3d_tok_is(&tok, "*NODE_PARENT")) {
            if (ask3d_next(work, &val) == ASK3D_TOK_WORD) {
                ask3d_copy_text(shape->parent_name, ASK3D_MAX_NAME, val.text);
                shape->has_parent = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*NODE_TM")) {
            ask3d_parse_tm_rows(work, scene, &shape->tm_row0, &shape->tm_row1, &shape->tm_row2, &shape->tm_row3, &shape->has_transform, 0, 0, 0, 0, 0);
        } else if (ask3d_tok_is(&tok, "*SHAPE_LINE") || ask3d_tok_is(&tok, "*SHAPE_SPLINE")) {
            ask3d_parse_shape_line(work, scene, shape, sid, spline_index);
            spline_index++;
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
    ask3d_scene_compute_shape_bounds(scene, shape);
}

static void ask3d_parse_group(Ask3dWork *work, Ask3dScene *scene)
{
    Ask3dToken tok;
    if (!ask3d_try_block(work)) {
        return;
    }
    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF || tok.type == ASK3D_TOK_RBRACE) {
            break;
        }
        if (ask3d_tok_is(&tok, "*GEOMOBJECT")) {
            ask3d_parse_geomobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*CAMERAOBJECT")) {
            ask3d_parse_cameraobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*LIGHTOBJECT")) {
            ask3d_parse_lightobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*HELPEROBJECT")) {
            ask3d_parse_helperobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*SHAPEOBJECT")) {
            ask3d_parse_shapeobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*GROUP")) {
            ask3d_parse_group(work, scene);
        } else if (ask3d_tok_is(&tok, "*COMMENT")) {
            ask3d_next(work, &tok);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }
}

int ask3d_parse_memory(Ask3dScene *scene, Ask3dWork *work, const char *data, unsigned long size)
{
    Ask3dToken tok;
    long ignored;

    if (scene == 0 || work == 0 || data == 0) {
        return ASK3D_ERR_BAD_ARG;
    }

    ask3d_scene_clear(scene);
    ask3d_work_clear(work);
    ask3d_lex_init(&work->lex, data, size);

    while (1) {
        ask3d_next(work, &tok);
        if (tok.type == ASK3D_TOK_EOF) {
            break;
        }
        if (tok.type == ASK3D_TOK_RBRACE) {
            continue;
        }
        if (ask3d_tok_is(&tok, "*MATERIAL_LIST")) {
            ask3d_parse_material_list(work, scene);
        } else if (ask3d_tok_is(&tok, "*GEOMOBJECT")) {
            ask3d_parse_geomobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*CAMERAOBJECT")) {
            ask3d_parse_cameraobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*LIGHTOBJECT")) {
            ask3d_parse_lightobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*HELPEROBJECT")) {
            ask3d_parse_helperobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*SHAPEOBJECT")) {
            ask3d_parse_shapeobject(work, scene);
        } else if (ask3d_tok_is(&tok, "*GROUP")) {
            ask3d_parse_group(work, scene);
        } else if (ask3d_tok_is(&tok, "*SCENE")) {
            ask3d_parse_scene_block(work, scene);
        } else if (ask3d_tok_is(&tok, "*3DSMAX_ASCIIEXPORT")) {
            if (ask3d_read_i32(work, &ignored)) {
                scene->info.export_version = (int)ignored;
                scene->info.has_export_version = 1U;
            }
        } else if (ask3d_tok_is(&tok, "*COMMENT")) {
            ask3d_next(work, &tok);
        } else {
            ask3d_skip_optional_block(work, scene);
        }
    }

    if (scene->overflowed != 0U) {
        return ASK3D_ERR_OVERFLOW;
    }
    return ASK3D_OK;
}
