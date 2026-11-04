#include "g3ds_fixed.h"

#define G3DS_CHUNK_MAIN3DS       0x4D4Du
#define G3DS_CHUNK_M3D_VERSION   0x0002u
#define G3DS_CHUNK_M3D_KFVERSION 0x0005u
#define G3DS_CHUNK_MDATA         0x3D3Du
#define G3DS_CHUNK_MESH_VERSION  0x3D3Eu
#define G3DS_CHUNK_MASTER_SCALE  0x0100u
#define G3DS_CHUNK_LO_SHADOW_BIAS 0x1400u
#define G3DS_CHUNK_HI_SHADOW_BIAS 0x1410u
#define G3DS_CHUNK_SHADOW_MAP_SIZE 0x1420u
#define G3DS_CHUNK_SHADOW_SAMPLES 0x1430u
#define G3DS_CHUNK_SHADOW_RANGE 0x1440u
#define G3DS_CHUNK_SHADOW_FILTER 0x1450u
#define G3DS_CHUNK_RAY_BIAS     0x1460u
#define G3DS_CHUNK_O_CONSTS     0x1500u
#define G3DS_CHUNK_BIT_MAP       0x1100u
#define G3DS_CHUNK_USE_BIT_MAP   0x1101u
#define G3DS_CHUNK_SOLID_BGND    0x1200u
#define G3DS_CHUNK_USE_SOLID_BGND 0x1201u
#define G3DS_CHUNK_V_GRADIENT    0x1300u
#define G3DS_CHUNK_USE_V_GRADIENT 0x1301u
#define G3DS_CHUNK_AMBIENT_LIGHT 0x2100u
#define G3DS_CHUNK_FOG           0x2200u
#define G3DS_CHUNK_USE_FOG       0x2201u
#define G3DS_CHUNK_FOG_BGND      0x2210u
#define G3DS_CHUNK_DISTANCE_CUE  0x2300u
#define G3DS_CHUNK_USE_DISTANCE_CUE 0x2301u
#define G3DS_CHUNK_LAYER_FOG     0x2302u
#define G3DS_CHUNK_USE_LAYER_FOG 0x2303u
#define G3DS_CHUNK_DCUE_BGND     0x2310u
#define G3DS_CHUNK_DEFAULT_VIEW  0x3000u
#define G3DS_CHUNK_VIEW_TOP      0x3010u
#define G3DS_CHUNK_VIEW_BOTTOM   0x3020u
#define G3DS_CHUNK_VIEW_LEFT     0x3030u
#define G3DS_CHUNK_VIEW_RIGHT    0x3040u
#define G3DS_CHUNK_VIEW_FRONT    0x3050u
#define G3DS_CHUNK_VIEW_BACK     0x3060u
#define G3DS_CHUNK_VIEW_USER     0x3070u
#define G3DS_CHUNK_VIEW_CAMERA   0x3080u

#define G3DS_CHUNK_PERCENT_I     0x0030u
#define G3DS_CHUNK_PERCENT_R     0x0031u
#define G3DS_CHUNK_COLOR_F       0x0010u
#define G3DS_CHUNK_COLOR_24      0x0011u
#define G3DS_CHUNK_LIN_COLOR_24  0x0012u
#define G3DS_CHUNK_LIN_COLOR_F   0x0013u

#define G3DS_CHUNK_NAMED_OBJECT  0x4000u
#define G3DS_CHUNK_OBJ_HIDDEN    0x4010u
#define G3DS_CHUNK_OBJ_VIS_LOFTER 0x4011u
#define G3DS_CHUNK_OBJ_DOESNT_CAST 0x4012u
#define G3DS_CHUNK_OBJ_MATTE     0x4013u
#define G3DS_CHUNK_OBJ_FAST      0x4014u
#define G3DS_CHUNK_OBJ_PROCEDURAL 0x4015u
#define G3DS_CHUNK_OBJ_FROZEN    0x4016u
#define G3DS_CHUNK_OBJ_DONT_RCVSHADOW 0x4017u

#define G3DS_CHUNK_N_TRI_OBJECT  0x4100u
#define G3DS_CHUNK_POINT_ARRAY   0x4110u
#define G3DS_CHUNK_POINT_FLAGS   0x4111u
#define G3DS_CHUNK_FACE_ARRAY    0x4120u
#define G3DS_CHUNK_MSH_MAT_GROUP 0x4130u
#define G3DS_CHUNK_TEX_VERTS     0x4140u
#define G3DS_CHUNK_SMOOTH_GROUP  0x4150u
#define G3DS_CHUNK_MESH_MATRIX   0x4160u
#define G3DS_CHUNK_MESH_COLOR    0x4165u
#define G3DS_CHUNK_MESH_TEX_INFO 0x4170u
#define G3DS_CHUNK_PROC_NAME     0x4181u
#define G3DS_CHUNK_PROC_DATA     0x4182u
#define G3DS_CHUNK_MSH_BOXMAP    0x4190u

#define G3DS_CHUNK_N_DIRECT_LIGHT 0x4600u
#define G3DS_CHUNK_DL_SPOTLIGHT  0x4610u
#define G3DS_CHUNK_DL_OFF        0x4620u
#define G3DS_CHUNK_DL_ATTENUATE  0x4625u
#define G3DS_CHUNK_DL_RAYSHAD    0x4627u
#define G3DS_CHUNK_DL_SHADOWED   0x4630u
#define G3DS_CHUNK_DL_LOCAL_SHADOW2 0x4641u
#define G3DS_CHUNK_DL_SEE_CONE   0x4650u
#define G3DS_CHUNK_DL_SPOT_RECT  0x4651u
#define G3DS_CHUNK_DL_SPOT_OVERSHOOT 0x4652u
#define G3DS_CHUNK_DL_SPOT_PROJECTOR 0x4653u
#define G3DS_CHUNK_DL_SPOT_ROLL  0x4656u
#define G3DS_CHUNK_DL_SPOT_ASPECT 0x4657u
#define G3DS_CHUNK_DL_RAY_BIAS   0x4658u
#define G3DS_CHUNK_DL_INNER_RANGE 0x4659u
#define G3DS_CHUNK_DL_OUTER_RANGE 0x465Au
#define G3DS_CHUNK_DL_MULTIPLIER 0x465Bu

#define G3DS_CHUNK_N_CAMERA      0x4700u
#define G3DS_CHUNK_CAM_SEE_CONE  0x4710u
#define G3DS_CHUNK_CAM_RANGES    0x4720u

#define G3DS_CHUNK_MAT_ENTRY     0xAFFFu
#define G3DS_CHUNK_MAT_NAME      0xA000u
#define G3DS_CHUNK_MAT_AMBIENT   0xA010u
#define G3DS_CHUNK_MAT_DIFFUSE   0xA020u
#define G3DS_CHUNK_MAT_SPECULAR  0xA030u
#define G3DS_CHUNK_MAT_SHININESS 0xA040u
#define G3DS_CHUNK_MAT_SHIN2PCT  0xA041u
#define G3DS_CHUNK_MAT_TRANSPARENCY 0xA050u
#define G3DS_CHUNK_MAT_XPFALL    0xA052u
#define G3DS_CHUNK_MAT_REFBLUR   0xA053u
#define G3DS_CHUNK_MAT_SELF_ILLUM 0xA080u
#define G3DS_CHUNK_MAT_SELF_ILPCT 0xA084u
#define G3DS_CHUNK_MAT_TWO_SIDE  0xA081u
#define G3DS_CHUNK_MAT_DECAL     0xA082u
#define G3DS_CHUNK_MAT_ADDITIVE  0xA083u
#define G3DS_CHUNK_MAT_WIRE      0xA085u
#define G3DS_CHUNK_MAT_WIRESIZE  0xA087u
#define G3DS_CHUNK_MAT_FACEMAP   0xA088u
#define G3DS_CHUNK_MAT_PHONGSOFT 0xA08Cu
#define G3DS_CHUNK_MAT_WIREABS   0xA08Eu
#define G3DS_CHUNK_MAT_SHADING   0xA100u
#define G3DS_CHUNK_MAT_TEXMAP    0xA200u
#define G3DS_CHUNK_MAT_SPECMAP   0xA204u
#define G3DS_CHUNK_MAT_OPACMAP   0xA210u
#define G3DS_CHUNK_MAT_REFLMAP   0xA220u
#define G3DS_CHUNK_MAT_BUMPMAP   0xA230u
#define G3DS_CHUNK_MAT_ACUBIC    0xA310u
#define G3DS_CHUNK_MAT_USE_XPFALL 0xA240u
#define G3DS_CHUNK_MAT_USE_REFBLUR 0xA250u
#define G3DS_CHUNK_MAT_MAPNAME   0xA300u
#define G3DS_CHUNK_MAT_SXP_TEXT_DATA 0xA320u
#define G3DS_CHUNK_MAT_SXP_TEXT2_DATA 0xA321u
#define G3DS_CHUNK_MAT_SXP_OPAC_DATA 0xA322u
#define G3DS_CHUNK_MAT_SXP_BUMP_DATA 0xA324u
#define G3DS_CHUNK_MAT_SXP_SPEC_DATA 0xA325u
#define G3DS_CHUNK_MAT_SXP_SHIN_DATA 0xA326u
#define G3DS_CHUNK_MAT_SXP_SELFI_DATA 0xA328u
#define G3DS_CHUNK_MAT_SXP_REFL_DATA 0xA32Au
#define G3DS_CHUNK_MAT_SXP_TEXT_MASKDATA 0xA32Bu
#define G3DS_CHUNK_MAT_SXP_TEXT2_MASKDATA 0xA32Cu
#define G3DS_CHUNK_MAT_SXP_OPAC_MASKDATA 0xA32Eu
#define G3DS_CHUNK_MAT_SXP_BUMP_MASKDATA 0xA330u
#define G3DS_CHUNK_MAT_SXP_SPEC_MASKDATA 0xA332u
#define G3DS_CHUNK_MAT_SXP_SHIN_MASKDATA 0xA334u
#define G3DS_CHUNK_MAT_SXP_SELFI_MASKDATA 0xA336u
#define G3DS_CHUNK_MAT_SXP_REFL_MASKDATA 0xA338u
#define G3DS_CHUNK_MAT_TEXMASK   0xA33Eu
#define G3DS_CHUNK_MAT_TEX2MAP   0xA33Au
#define G3DS_CHUNK_MAT_SHINMAP   0xA33Cu
#define G3DS_CHUNK_MAT_SELFIMAP  0xA33Du
#define G3DS_CHUNK_MAT_TEX2MASK  0xA340u
#define G3DS_CHUNK_MAT_OPACMASK  0xA342u
#define G3DS_CHUNK_MAT_BUMPMASK  0xA344u
#define G3DS_CHUNK_MAT_SHINMASK  0xA346u
#define G3DS_CHUNK_MAT_SPECMASK  0xA348u
#define G3DS_CHUNK_MAT_SELFIMASK 0xA34Au
#define G3DS_CHUNK_MAT_REFLMASK  0xA34Cu
#define G3DS_CHUNK_MAT_MAP_TILING 0xA351u
#define G3DS_CHUNK_MAT_MAP_BLUR  0xA353u
#define G3DS_CHUNK_MAT_MAP_VSCALE 0xA354u
#define G3DS_CHUNK_MAT_MAP_USCALE 0xA356u
#define G3DS_CHUNK_MAT_MAP_UOFFSET 0xA358u
#define G3DS_CHUNK_MAT_MAP_VOFFSET 0xA35Au
#define G3DS_CHUNK_MAT_MAP_ANG   0xA35Cu
#define G3DS_CHUNK_MAT_MAP_COL1  0xA360u
#define G3DS_CHUNK_MAT_MAP_COL2  0xA362u
#define G3DS_CHUNK_MAT_MAP_RCOL  0xA364u
#define G3DS_CHUNK_MAT_MAP_GCOL  0xA366u
#define G3DS_CHUNK_MAT_MAP_BCOL  0xA368u

#define G3DS_CHUNK_KFDATA        0xB000u
#define G3DS_CHUNK_AMBIENT_NODE  0xB001u
#define G3DS_CHUNK_OBJECT_NODE   0xB002u
#define G3DS_CHUNK_CAMERA_NODE   0xB003u
#define G3DS_CHUNK_TARGET_NODE   0xB004u
#define G3DS_CHUNK_LIGHT_NODE    0xB005u
#define G3DS_CHUNK_L_TARGET_NODE 0xB006u
#define G3DS_CHUNK_SPOT_NODE     0xB007u
#define G3DS_CHUNK_KFSEG         0xB008u
#define G3DS_CHUNK_KFCURTIME     0xB009u
#define G3DS_CHUNK_KFHDR         0xB00Au
#define G3DS_CHUNK_NODE_HDR      0xB010u
#define G3DS_CHUNK_INSTANCE_NAME 0xB011u
#define G3DS_CHUNK_PIVOT         0xB013u
#define G3DS_CHUNK_BOUNDBOX      0xB014u
#define G3DS_CHUNK_MORPH_SMOOTH  0xB015u
#define G3DS_CHUNK_POS_TRACK     0xB020u
#define G3DS_CHUNK_ROT_TRACK     0xB021u
#define G3DS_CHUNK_SCL_TRACK     0xB022u
#define G3DS_CHUNK_FOV_TRACK     0xB023u
#define G3DS_CHUNK_ROLL_TRACK    0xB024u
#define G3DS_CHUNK_COLOR_TRACK   0xB025u
#define G3DS_CHUNK_MORPH_TRACK   0xB026u
#define G3DS_CHUNK_HOT_TRACK     0xB027u
#define G3DS_CHUNK_FALL_TRACK    0xB028u
#define G3DS_CHUNK_HIDE_TRACK    0xB029u
#define G3DS_CHUNK_NODE_ID       0xB030u

#define G3DS_Q16_MAX 2147483647L
#define G3DS_Q16_MIN (-2147483647L - 1L)

#define G3DS_COLOR_NONE   0u
#define G3DS_COLOR_MAT_AMB 1u
#define G3DS_COLOR_MAT_DIF 2u
#define G3DS_COLOR_MAT_SPC 3u
#define G3DS_COLOR_LIGHT   4u
#define G3DS_COLOR_AMBIENT 5u
#define G3DS_COLOR_BG_SOLID 6u
#define G3DS_COLOR_BG_GRAD_TOP 7u
#define G3DS_COLOR_BG_GRAD_MID 8u
#define G3DS_COLOR_BG_GRAD_BOTTOM 9u
#define G3DS_COLOR_FOG 10u
#define G3DS_COLOR_LAYER_FOG 11u
#define G3DS_COLOR_MAP_TINT1 12u
#define G3DS_COLOR_MAP_TINT2 13u
#define G3DS_COLOR_MAP_RCOL 14u
#define G3DS_COLOR_MAP_GCOL 15u
#define G3DS_COLOR_MAP_BCOL 16u


static void g3ds_audit_chunk(G3DS_Model *m, G3DS_U16 id, G3DS_U32 len)
{
    G3DS_U16 i;
    G3DS_U16 slot;
    if (m == (G3DS_Model *)0) {
        return;
    }
    for (i = 0u; i < m->chunk_audit_count; ++i) {
        if (m->chunk_audit[i].id == id) {
            m->chunk_audit[i].count = m->chunk_audit[i].count + 1u;
            m->chunk_audit[i].bytes = m->chunk_audit[i].bytes + len;
            return;
        }
    }
    if (m->chunk_audit_count < (G3DS_U16)G3DS_MAX_CHUNK_AUDIT) {
        slot = m->chunk_audit_count;
        m->chunk_audit[slot].id = id;
        m->chunk_audit[slot].count = 1u;
        m->chunk_audit[slot].bytes = len;
        m->chunk_audit_count = (G3DS_U16)(m->chunk_audit_count + 1u);
    }
}

typedef struct G3DS_Reader_TAG {
    const G3DS_U8 *data;
    G3DS_U32 size;
    G3DS_U32 pos;
    int err;
    G3DS_Model *model;
    G3DS_S32 current_object;
    G3DS_S32 current_material;
    G3DS_S32 current_light;
    G3DS_S32 current_camera;
    G3DS_S32 current_anim_node;
    G3DS_S32 current_map_slot;
    G3DS_U16 face_base;
    G3DS_U16 face_count;
    char named_name[G3DS_NAME_MAX];
    G3DS_S32 named_mesh_index;
    G3DS_S32 named_light_index;
    G3DS_S32 named_camera_index;
    G3DS_U16 named_flags;
    G3DS_U16 color_target;
} G3DS_Reader;

static void g3ds_zero(void *ptr, G3DS_U32 count)
{
    G3DS_U8 *p;
    G3DS_U32 i;
    p = (G3DS_U8 *)ptr;
    for (i = 0u; i < count; ++i) {
        p[i] = (G3DS_U8)0u;
    }
}

static void g3ds_copy_cstr(char *dst, G3DS_U32 dst_size, const char *src)
{
    G3DS_U32 i;
    if (dst_size == 0u) {
        return;
    }
    i = 0u;
    while (i + 1u < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void g3ds_map_init(G3DS_Map *map)
{
    map->filename[0] = '\0';
    map->strength = 0L;
    map->tiling = 0u;
    map->blur = 0L;
    map->u_scale = G3DS_Q16_ONE;
    map->v_scale = G3DS_Q16_ONE;
    map->u_offset = 0L;
    map->v_offset = 0L;
    map->angle_degrees = 0L;
    map->tint1.r = map->tint1.g = map->tint1.b = 255u;
    map->tint2.r = map->tint2.g = map->tint2.b = 255u;
    map->tint_r.r = 255u; map->tint_r.g = 0u; map->tint_r.b = 0u;
    map->tint_g.r = 0u; map->tint_g.g = 255u; map->tint_g.b = 0u;
    map->tint_b.r = 0u; map->tint_b.g = 0u; map->tint_b.b = 255u;
    map->tint_flags = 0u;
    map->sxp_data_bytes = 0u;
    map->sxp_mask_bytes = 0u;
    map->has_sxp_data = 0u;
    map->has_sxp_mask = 0u;
    map->has_map = 0u;
}

void g3ds_model_init(G3DS_Model *model)
{
    G3DS_U32 i;
    G3DS_U32 j;
    if (model == (G3DS_Model *)0) {
        return;
    }
    g3ds_zero(model, (G3DS_U32)sizeof(G3DS_Model));
    model->master_scale = G3DS_Q16_ONE;
    for (i = 0u; i < G3DS_MAX_FACES; ++i) {
        model->faces[i].material = (G3DS_U16)G3DS_NO_MATERIAL;
    }
    for (i = 0u; i < G3DS_MAX_MATERIALS; ++i) {
        model->materials[i].ambient.r = 0u;
        model->materials[i].ambient.g = 0u;
        model->materials[i].ambient.b = 0u;
        model->materials[i].diffuse.r = 255u;
        model->materials[i].diffuse.g = 255u;
        model->materials[i].diffuse.b = 255u;
        model->materials[i].specular.r = 255u;
        model->materials[i].specular.g = 255u;
        model->materials[i].specular.b = 255u;
        model->materials[i].shading = 3u;
        for (j = 0u; j < G3DS_MAP_COUNT; ++j) {
            g3ds_map_init(&model->materials[i].maps[j]);
        }
    }
    for (i = 0u; i < G3DS_MAX_LIGHTS; ++i) {
        model->lights[i].color.r = 255u;
        model->lights[i].color.g = 255u;
        model->lights[i].color.b = 255u;
        model->lights[i].multiplier = G3DS_Q16_ONE;
    }
    model->ambient_light.r = 0u;
    model->ambient_light.g = 0u;
    model->ambient_light.b = 0u;
    model->background.solid.r = 0u;
    model->background.solid.g = 0u;
    model->background.solid.b = 0u;
    model->background.gradient_top.r = 0u;
    model->background.gradient_top.g = 0u;
    model->background.gradient_top.b = 0u;
    model->background.gradient_mid.r = 0u;
    model->background.gradient_mid.g = 0u;
    model->background.gradient_mid.b = 0u;
    model->background.gradient_bottom.r = 0u;
    model->background.gradient_bottom.g = 0u;
    model->background.gradient_bottom.b = 0u;
    model->background.gradient_midpoint = 0L;
    model->atmosphere.fog_color.r = 255u;
    model->atmosphere.fog_color.g = 255u;
    model->atmosphere.fog_color.b = 255u;
    model->atmosphere.layer_color.r = 255u;
    model->atmosphere.layer_color.g = 255u;
    model->atmosphere.layer_color.b = 255u;
    for (i = 0u; i < G3DS_MAX_ANIM_NODES; ++i) {
        model->anim_nodes[i].node_id = (G3DS_U16)G3DS_NO_PARENT;
        model->anim_nodes[i].parent_id = (G3DS_U16)G3DS_NO_PARENT;
    }
}

G3DS_Q16 g3ds_q16_from_i32(G3DS_S32 v)
{
    if (v > (G3DS_Q16_MAX >> G3DS_Q16_SHIFT)) {
        return G3DS_Q16_MAX;
    }
    if (v < (G3DS_Q16_MIN >> G3DS_Q16_SHIFT)) {
        return G3DS_Q16_MIN;
    }
    return (G3DS_Q16)(v << G3DS_Q16_SHIFT);
}

G3DS_Q16 g3ds_q16_from_ieee32(G3DS_U32 bits)
{
    G3DS_U32 sign;
    G3DS_U32 exp;
    G3DS_U32 mant;
    G3DS_U32 mag;
    int shift;
    G3DS_Q16 out;

    bits &= 0xFFFFFFFFUL;
    sign = bits & 0x80000000UL;
    exp = (bits >> 23) & 0xFFUL;
    mant = bits & 0x7FFFFFUL;

    if (exp == 0u) {
        return 0L;
    }
    if (exp == 0xFFu) {
        return sign ? G3DS_Q16_MIN : G3DS_Q16_MAX;
    }

    mant |= 0x800000UL;
    shift = (int)exp - 134;

    if (shift >= 8) {
        return sign ? G3DS_Q16_MIN : G3DS_Q16_MAX;
    }
    if (shift >= 0) {
        mag = (mant << shift) & 0xFFFFFFFFUL;
    } else {
        if (shift <= -31) {
            mag = 0u;
        } else {
            mag = mant >> (-shift);
        }
    }
    if (mag > 0x7FFFFFFFUL) {
        return sign ? G3DS_Q16_MIN : G3DS_Q16_MAX;
    }
    out = (G3DS_Q16)mag;
    if (sign) {
        out = (G3DS_Q16)(-out);
    }
    return out;
}

G3DS_U32 g3ds_q16_to_decimal4(G3DS_Q16 v, G3DS_U32 *whole, G3DS_U32 *frac4)
{
    G3DS_U32 mag;
    G3DS_U32 w;
    G3DS_U32 f;
    G3DS_U32 neg;

    neg = 0u;
    if (v < 0L) {
        neg = 1u;
        mag = (G3DS_U32)(-v);
    } else {
        mag = (G3DS_U32)v;
    }
    w = mag >> G3DS_Q16_SHIFT;
    f = (((mag & 0xFFFFUL) * 10000UL) + 32768UL) >> G3DS_Q16_SHIFT;
    if (f >= 10000UL) {
        w += 1UL;
        f -= 10000UL;
    }
    if (whole != (G3DS_U32 *)0) {
        *whole = w;
    }
    if (frac4 != (G3DS_U32 *)0) {
        *frac4 = f;
    }
    return neg;
}

static int g3ds_has(G3DS_Reader *r, G3DS_U32 n)
{
    if (r->pos > r->size || n > (r->size - r->pos)) {
        r->err = G3DS_ERR_SHORT;
        r->model->error_offset = r->pos;
        return 0;
    }
    return 1;
}

static G3DS_U8 g3ds_read_u8(G3DS_Reader *r)
{
    G3DS_U8 v;
    if (!g3ds_has(r, 1u)) {
        return 0u;
    }
    v = r->data[r->pos];
    r->pos += 1u;
    return v;
}

static G3DS_U16 g3ds_read_u16(G3DS_Reader *r)
{
    G3DS_U16 v;
    if (!g3ds_has(r, 2u)) {
        return 0u;
    }
    v = (G3DS_U16)r->data[r->pos];
    v = (G3DS_U16)(v | ((G3DS_U16)r->data[r->pos + 1u] << 8));
    r->pos += 2u;
    return v;
}

static G3DS_U32 g3ds_read_u32(G3DS_Reader *r)
{
    G3DS_U32 v;
    if (!g3ds_has(r, 4u)) {
        return 0u;
    }
    v = (G3DS_U32)r->data[r->pos];
    v |= ((G3DS_U32)r->data[r->pos + 1u] << 8);
    v |= ((G3DS_U32)r->data[r->pos + 2u] << 16);
    v |= ((G3DS_U32)r->data[r->pos + 3u] << 24);
    r->pos += 4u;
    return (v & 0xFFFFFFFFUL);
}

static G3DS_Q16 g3ds_read_real32_as_q16(G3DS_Reader *r)
{
    return g3ds_q16_from_ieee32(g3ds_read_u32(r));
}

static void g3ds_skip_to(G3DS_Reader *r, G3DS_U32 end)
{
    if (end > r->size) {
        r->err = G3DS_ERR_SHORT;
        r->model->error_offset = r->pos;
        r->pos = r->size;
        return;
    }
    r->pos = end;
}

static int g3ds_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static void g3ds_read_cstr(G3DS_Reader *r, G3DS_U32 end, char *dst, G3DS_U32 dst_size)
{
    G3DS_U32 i;
    G3DS_U8 c;
    int done;
    if (dst_size > 0u) {
        dst[0] = '\0';
    }
    i = 0u;
    done = 0;
    while (r->pos < end && r->err == G3DS_OK) {
        c = g3ds_read_u8(r);
        if (c == 0u) {
            done = 1;
            break;
        }
        if (i + 1u < dst_size) {
            dst[i] = (char)c;
            ++i;
            dst[i] = '\0';
        }
    }
    if (!done && r->err == G3DS_OK) {
        r->err = G3DS_ERR_TRUNCATED_STRING;
        r->model->error_offset = r->pos;
    }
}

static void g3ds_parse_children(G3DS_Reader *r, G3DS_U32 end);

static G3DS_U16 g3ds_find_material_index(G3DS_Model *m, const char *name)
{
    G3DS_U16 i;
    for (i = 0u; i < m->material_count; ++i) {
        if (g3ds_streq(m->materials[i].name, name)) {
            return i;
        }
    }
    return (G3DS_U16)G3DS_NO_MATERIAL;
}

static G3DS_Color *g3ds_current_color(G3DS_Reader *r)
{
    if (r->current_material >= 0) {
        if (r->color_target == G3DS_COLOR_MAT_AMB) {
            return &r->model->materials[(G3DS_U16)r->current_material].ambient;
        }
        if (r->color_target == G3DS_COLOR_MAT_DIF) {
            return &r->model->materials[(G3DS_U16)r->current_material].diffuse;
        }
        if (r->color_target == G3DS_COLOR_MAT_SPC) {
            return &r->model->materials[(G3DS_U16)r->current_material].specular;
        }
    }
    if (r->current_light >= 0 && r->color_target == G3DS_COLOR_LIGHT) {
        return &r->model->lights[(G3DS_U16)r->current_light].color;
    }
    if (r->color_target == G3DS_COLOR_AMBIENT) { return &r->model->ambient_light; }
    if (r->color_target == G3DS_COLOR_BG_SOLID) { return &r->model->background.solid; }
    if (r->color_target == G3DS_COLOR_BG_GRAD_TOP) { return &r->model->background.gradient_top; }
    if (r->color_target == G3DS_COLOR_BG_GRAD_MID) { return &r->model->background.gradient_mid; }
    if (r->color_target == G3DS_COLOR_BG_GRAD_BOTTOM) { return &r->model->background.gradient_bottom; }
    if (r->color_target == G3DS_COLOR_FOG) { return &r->model->atmosphere.fog_color; }
    if (r->color_target == G3DS_COLOR_LAYER_FOG) { return &r->model->atmosphere.layer_color; }
    if (r->current_material >= 0 && r->current_map_slot >= 0 && (G3DS_U32)r->current_map_slot < G3DS_MAP_COUNT) {
        G3DS_Map *map;
        map = &r->model->materials[(G3DS_U16)r->current_material].maps[(G3DS_U16)r->current_map_slot];
        if (r->color_target == G3DS_COLOR_MAP_TINT1) { map->tint_flags = (G3DS_U16)(map->tint_flags | 0x0001u); return &map->tint1; }
        if (r->color_target == G3DS_COLOR_MAP_TINT2) { map->tint_flags = (G3DS_U16)(map->tint_flags | 0x0002u); return &map->tint2; }
        if (r->color_target == G3DS_COLOR_MAP_RCOL) { map->tint_flags = (G3DS_U16)(map->tint_flags | 0x0004u); return &map->tint_r; }
        if (r->color_target == G3DS_COLOR_MAP_GCOL) { map->tint_flags = (G3DS_U16)(map->tint_flags | 0x0008u); return &map->tint_g; }
        if (r->color_target == G3DS_COLOR_MAP_BCOL) { map->tint_flags = (G3DS_U16)(map->tint_flags | 0x0010u); return &map->tint_b; }
    }
    return (G3DS_Color *)0;
}

static G3DS_U8 g3ds_q16_to_u8_clamped(G3DS_Q16 v)
{
    G3DS_U32 u;
    if (v <= 0L) {
        return 0u;
    }
    if (v >= G3DS_Q16_ONE) {
        return 255u;
    }
    u = ((G3DS_U32)v * 255u) >> G3DS_Q16_SHIFT;
    if (u > 255u) {
        u = 255u;
    }
    return (G3DS_U8)u;
}

static void g3ds_parse_color_chunk(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 id)
{
    G3DS_Color *c;
    c = g3ds_current_color(r);
    if (c == (G3DS_Color *)0) {
        g3ds_skip_to(r, end);
        return;
    }
    if (id == G3DS_CHUNK_COLOR_24 || id == G3DS_CHUNK_LIN_COLOR_24) {
        c->r = g3ds_read_u8(r);
        c->g = g3ds_read_u8(r);
        c->b = g3ds_read_u8(r);
    } else {
        c->r = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
        c->g = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
        c->b = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_raw_color_to_target(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 target)
{
    G3DS_U16 prev;
    G3DS_Color *c;
    prev = r->color_target;
    r->color_target = target;
    c = g3ds_current_color(r);
    if (c != (G3DS_Color *)0 && r->pos + 3u <= end) {
        c->r = g3ds_read_u8(r);
        c->g = g3ds_read_u8(r);
        c->b = g3ds_read_u8(r);
    }
    r->color_target = prev;
    g3ds_skip_to(r, end);
}

static G3DS_Q16 g3ds_read_percent(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 id)
{
    G3DS_Q16 out;
    if (id == G3DS_CHUNK_PERCENT_I) {
        out = g3ds_q16_from_i32((G3DS_S32)g3ds_read_u16(r));
    } else {
        out = g3ds_read_real32_as_q16(r);
    }
    g3ds_skip_to(r, end);
    return out;
}

static void g3ds_parse_color_group(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 target)
{
    G3DS_U16 prev;
    prev = r->color_target;
    r->color_target = target;
    g3ds_parse_children(r, end);
    r->color_target = prev;
}

static void g3ds_parse_gradient(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_U16 prev;
    G3DS_U16 step;
    G3DS_U32 start;
    G3DS_U32 len;
    G3DS_U32 cend;
    G3DS_U16 id;

    r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_HAS_GRADIENT);
    if (r->pos + 4u <= end) {
        r->model->background.gradient_midpoint = g3ds_read_real32_as_q16(r);
    }
    prev = r->color_target;
    step = 0u;
    while (r->err == G3DS_OK && r->pos + 6u <= end && step < 3u) {
        start = r->pos;
        id = g3ds_read_u16(r);
        len = g3ds_read_u32(r);
        r->model->last_chunk_id = id;
        if (len < 6u || start > end || len > (end - start)) {
            r->err = G3DS_ERR_BAD_CHUNK;
            r->model->error_offset = start;
            break;
        }
        cend = start + len;
        if (id == G3DS_CHUNK_COLOR_F || id == G3DS_CHUNK_COLOR_24 || id == G3DS_CHUNK_LIN_COLOR_24 || id == G3DS_CHUNK_LIN_COLOR_F) {
            if (step == 0u) { r->color_target = G3DS_COLOR_BG_GRAD_TOP; }
            else if (step == 1u) { r->color_target = G3DS_COLOR_BG_GRAD_MID; }
            else { r->color_target = G3DS_COLOR_BG_GRAD_BOTTOM; }
            g3ds_parse_color_chunk(r, cend, id);
            step = (G3DS_U16)(step + 1u);
        } else {
            g3ds_skip_to(r, cend);
        }
    }
    r->color_target = prev;
    if (r->err == G3DS_OK) { g3ds_skip_to(r, end); }
}

static void g3ds_parse_fog(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_U16 prev;
    r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_HAS_FOG);
    if (r->pos + 16u <= end) {
        r->model->atmosphere.fog_near_plane = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.fog_near_density = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.fog_far_plane = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.fog_far_density = g3ds_read_real32_as_q16(r);
    }
    prev = r->color_target;
    r->color_target = G3DS_COLOR_FOG;
    g3ds_parse_children(r, end);
    r->color_target = prev;
}

static void g3ds_parse_distance_cue(G3DS_Reader *r, G3DS_U32 end)
{
    r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_HAS_DISTANCE);
    if (r->pos + 16u <= end) {
        r->model->atmosphere.distance_near_plane = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.distance_near_dim = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.distance_far_plane = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.distance_far_dim = g3ds_read_real32_as_q16(r);
    }
    g3ds_parse_children(r, end);
}

static void g3ds_parse_layer_fog(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_U16 prev;
    r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_HAS_LAYER);
    if (r->pos + 16u <= end) {
        r->model->atmosphere.layer_z_min = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.layer_z_max = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.layer_density = g3ds_read_real32_as_q16(r);
        r->model->atmosphere.layer_flags = g3ds_read_u32(r);
    }
    prev = r->color_target;
    r->color_target = G3DS_COLOR_LAYER_FOG;
    g3ds_parse_children(r, end);
    r->color_target = prev;
}

static void g3ds_parse_acubic(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Material *mat;
    if (r->current_material < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    mat = &r->model->materials[(G3DS_U16)r->current_material];
    mat->has_acubic = 1u;
    if (r->pos + 12u <= end) {
        mat->acubic_shade_level = g3ds_read_u8(r);
        mat->acubic_aa_level = g3ds_read_u8(r);
        mat->acubic_flags = g3ds_read_u16(r);
        mat->acubic_size = g3ds_read_u32(r);
        mat->acubic_frame_interval = g3ds_read_u32(r);
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_percent_to_material(G3DS_Reader *r, G3DS_U32 end, G3DS_Q16 *dst)
{
    G3DS_U32 start;
    G3DS_U32 len;
    G3DS_U32 cend;
    G3DS_U16 id;

    while (r->err == G3DS_OK && r->pos + 6u <= end) {
        start = r->pos;
        id = g3ds_read_u16(r);
        len = g3ds_read_u32(r);
        r->model->last_chunk_id = id;
        if (len < 6u || start > end || len > (end - start)) {
            r->err = G3DS_ERR_BAD_CHUNK;
            r->model->error_offset = start;
            return;
        }
        cend = start + len;
        if (id == G3DS_CHUNK_PERCENT_I || id == G3DS_CHUNK_PERCENT_R) {
            *dst = g3ds_read_percent(r, cend, id);
        } else {
            g3ds_skip_to(r, cend);
        }
    }
    if (r->err == G3DS_OK) {
        g3ds_skip_to(r, end);
    }
}

static void g3ds_parse_material_entry(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_S32 prev_material;
    G3DS_U16 mi;
    if (r->model->material_count >= (G3DS_U16)G3DS_MAX_MATERIALS) {
        r->err = G3DS_ERR_CAPACITY_MATERIALS;
        r->model->error_offset = r->pos;
        return;
    }
    mi = r->model->material_count;
    r->model->material_count = (G3DS_U16)(r->model->material_count + 1u);
    r->model->materials[mi].name[0] = '\0';
    prev_material = r->current_material;
    r->current_material = (G3DS_S32)mi;
    g3ds_parse_children(r, end);
    r->current_material = prev_material;
}

static G3DS_Map *g3ds_current_map(G3DS_Reader *r)
{
    if (r->current_material < 0 || r->current_map_slot < 0) {
        return (G3DS_Map *)0;
    }
    if ((G3DS_U32)r->current_map_slot >= G3DS_MAP_COUNT) {
        return (G3DS_Map *)0;
    }
    return &r->model->materials[(G3DS_U16)r->current_material].maps[(G3DS_U16)r->current_map_slot];
}

static void g3ds_parse_map_block(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 slot)
{
    G3DS_S32 prev;
    G3DS_Map *map;
    prev = r->current_map_slot;
    r->current_map_slot = (G3DS_S32)slot;
    map = g3ds_current_map(r);
    if (map != (G3DS_Map *)0) {
        map->has_map = 1u;
    }
    g3ds_parse_children(r, end);
    r->current_map_slot = prev;
}

static void g3ds_apply_named_flag(G3DS_Reader *r, G3DS_U16 flag)
{
    r->named_flags = (G3DS_U16)(r->named_flags | flag);
    if (r->named_mesh_index >= 0) {
        r->model->objects[(G3DS_U16)r->named_mesh_index].flags = r->named_flags;
    }
}

static void g3ds_parse_point_array(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 count;
    G3DS_U16 i;
    G3DS_U32 room;
    G3DS_U16 base;
    G3DS_Vertex *v;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    count = g3ds_read_u16(r);
    if (r->err != G3DS_OK) {
        return;
    }
    room = (G3DS_U32)G3DS_MAX_VERTICES - (G3DS_U32)r->model->vertex_count;
    if ((G3DS_U32)count > room) {
        r->err = G3DS_ERR_CAPACITY_VERTICES;
        r->model->error_offset = r->pos;
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    base = r->model->vertex_count;
    obj->first_vertex = base;
    obj->vertex_count = count;
    r->model->vertex_count = (G3DS_U16)(r->model->vertex_count + count);
    for (i = 0u; i < count; ++i) {
        v = &r->model->vertices[(G3DS_U16)(base + i)];
        v->x = g3ds_read_real32_as_q16(r);
        v->y = g3ds_read_real32_as_q16(r);
        v->z = g3ds_read_real32_as_q16(r);
        v->u = 0L;
        v->v = 0L;
        v->has_uv = 0u;
        v->point_flags = 0u;
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_point_flags(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 count;
    G3DS_U16 i;
    G3DS_U16 idx;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    count = g3ds_read_u16(r);
    for (i = 0u; i < count; ++i) {
        idx = (G3DS_U16)(obj->first_vertex + i);
        if (i < obj->vertex_count && idx < r->model->vertex_count) {
            r->model->vertices[idx].point_flags = g3ds_read_u16(r);
        } else {
            (void)g3ds_read_u16(r);
        }
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_tex_verts(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 count;
    G3DS_U16 i;
    G3DS_U16 idx;
    G3DS_Vertex *vtx;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    count = g3ds_read_u16(r);
    for (i = 0u; i < count; ++i) {
        if (i < obj->vertex_count) {
            idx = (G3DS_U16)(obj->first_vertex + i);
            if (idx >= r->model->vertex_count) {
                r->err = G3DS_ERR_BAD_INDEX;
                r->model->error_offset = r->pos;
                return;
            }
            vtx = &r->model->vertices[idx];
            vtx->u = g3ds_read_real32_as_q16(r);
            vtx->v = g3ds_read_real32_as_q16(r);
            vtx->has_uv = 1u;
        } else {
            (void)g3ds_read_real32_as_q16(r);
            (void)g3ds_read_real32_as_q16(r);
        }
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_mesh_matrix(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 i;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    obj->has_matrix = 1u;
    for (i = 0u; i < 12u; ++i) {
        obj->matrix[i] = g3ds_read_real32_as_q16(r);
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_mesh_tex_info(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 i;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    obj->has_tex_icon = 1u;
    for (i = 0u; i < 21u && r->pos + 4u <= end; ++i) {
        obj->tex_icon[i] = g3ds_read_real32_as_q16(r);
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_boxmap(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 i;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    obj->has_boxmap = 1u;
    for (i = 0u; i < G3DS_BOXMAP_SIDES && r->pos < end; ++i) {
        g3ds_read_cstr(r, end, obj->boxmap[i], (G3DS_U32)G3DS_NAME_MAX);
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_mat_group(G3DS_Reader *r, G3DS_U32 end)
{
    char name[G3DS_NAME_MAX];
    G3DS_U16 mat;
    G3DS_U16 count;
    G3DS_U16 i;
    G3DS_U16 fidx;
    G3DS_U16 absolute;
    g3ds_read_cstr(r, end, name, (G3DS_U32)sizeof(name));
    if (r->err != G3DS_OK) {
        return;
    }
    count = g3ds_read_u16(r);
    mat = g3ds_find_material_index(r->model, name);
    for (i = 0u; i < count; ++i) {
        fidx = g3ds_read_u16(r);
        if (r->err != G3DS_OK) {
            return;
        }
        if (fidx < r->face_count) {
            absolute = (G3DS_U16)(r->face_base + fidx);
            if (absolute < r->model->face_count) {
                r->model->faces[absolute].material = mat;
            }
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_smooth_group(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_U16 i;
    G3DS_U16 absolute;
    for (i = 0u; i < r->face_count && r->pos + 4u <= end; ++i) {
        absolute = (G3DS_U16)(r->face_base + i);
        if (absolute >= r->model->face_count) {
            r->err = G3DS_ERR_BAD_INDEX;
            r->model->error_offset = r->pos;
            return;
        }
        r->model->faces[absolute].smoothing = g3ds_read_u32(r);
        if (r->err != G3DS_OK) {
            return;
        }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_face_array(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    G3DS_U16 count;
    G3DS_U16 i;
    G3DS_U32 room;
    G3DS_U16 base;
    G3DS_U16 prev_base;
    G3DS_U16 prev_count;
    G3DS_Face *f;
    G3DS_U16 ia;
    G3DS_U16 ib;
    G3DS_U16 ic;
    G3DS_U16 flags;
    if (r->current_object < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    count = g3ds_read_u16(r);
    if (r->err != G3DS_OK) {
        return;
    }
    room = (G3DS_U32)G3DS_MAX_FACES - (G3DS_U32)r->model->face_count;
    if ((G3DS_U32)count > room) {
        r->err = G3DS_ERR_CAPACITY_FACES;
        r->model->error_offset = r->pos;
        return;
    }
    obj = &r->model->objects[(G3DS_U16)r->current_object];
    base = r->model->face_count;
    obj->first_face = base;
    obj->face_count = count;
    r->model->face_count = (G3DS_U16)(r->model->face_count + count);
    for (i = 0u; i < count; ++i) {
        ia = g3ds_read_u16(r);
        ib = g3ds_read_u16(r);
        ic = g3ds_read_u16(r);
        flags = g3ds_read_u16(r);
        if (r->err != G3DS_OK) {
            return;
        }
        if (ia >= obj->vertex_count || ib >= obj->vertex_count || ic >= obj->vertex_count) {
            r->err = G3DS_ERR_BAD_INDEX;
            r->model->error_offset = r->pos;
            return;
        }
        f = &r->model->faces[(G3DS_U16)(base + i)];
        f->a = (G3DS_U16)(obj->first_vertex + ia);
        f->b = (G3DS_U16)(obj->first_vertex + ib);
        f->c = (G3DS_U16)(obj->first_vertex + ic);
        f->flags = flags;
        f->material = (G3DS_U16)G3DS_NO_MATERIAL;
        f->smoothing = 0u;
    }
    prev_base = r->face_base;
    prev_count = r->face_count;
    r->face_base = base;
    r->face_count = count;
    g3ds_parse_children(r, end);
    r->face_base = prev_base;
    r->face_count = prev_count;
}

static void g3ds_parse_n_tri_object(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_S32 prev_object;
    G3DS_U16 oi;
    G3DS_Object *obj;
    if (r->model->object_count >= (G3DS_U16)G3DS_MAX_OBJECTS) {
        r->err = G3DS_ERR_CAPACITY_OBJECTS;
        r->model->error_offset = r->pos;
        return;
    }
    oi = r->model->object_count;
    r->model->object_count = (G3DS_U16)(r->model->object_count + 1u);
    obj = &r->model->objects[oi];
    g3ds_copy_cstr(obj->name, (G3DS_U32)G3DS_NAME_MAX, r->named_name);
    obj->first_vertex = r->model->vertex_count;
    obj->first_face = r->model->face_count;
    obj->vertex_count = 0u;
    obj->face_count = 0u;
    obj->flags = r->named_flags;
    obj->mesh_color = 0u;
    obj->has_matrix = 0u;
    obj->has_tex_icon = 0u;
    obj->has_boxmap = 0u;
    r->named_mesh_index = (G3DS_S32)oi;
    prev_object = r->current_object;
    r->current_object = (G3DS_S32)oi;
    g3ds_parse_children(r, end);
    r->current_object = prev_object;
}

static void g3ds_parse_direct_light(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Light *l;
    G3DS_S32 prev_light;
    G3DS_U16 prev_color;
    G3DS_U16 li;
    if (r->model->light_count >= (G3DS_U16)G3DS_MAX_LIGHTS) {
        r->err = G3DS_ERR_CAPACITY_LIGHTS;
        r->model->error_offset = r->pos;
        return;
    }
    li = r->model->light_count;
    r->model->light_count = (G3DS_U16)(r->model->light_count + 1u);
    l = &r->model->lights[li];
    g3ds_copy_cstr(l->name, (G3DS_U32)G3DS_NAME_MAX, r->named_name);
    l->x = g3ds_read_real32_as_q16(r);
    l->y = g3ds_read_real32_as_q16(r);
    l->z = g3ds_read_real32_as_q16(r);
    if (r->err != G3DS_OK) {
        return;
    }
    r->named_light_index = (G3DS_S32)li;
    prev_light = r->current_light;
    prev_color = r->color_target;
    r->current_light = (G3DS_S32)li;
    r->color_target = G3DS_COLOR_LIGHT;
    g3ds_parse_children(r, end);
    r->current_light = prev_light;
    r->color_target = prev_color;
}

static void g3ds_parse_spotlight(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Light *l;
    if (r->current_light < 0) {
        g3ds_skip_to(r, end);
        return;
    }
    l = &r->model->lights[(G3DS_U16)r->current_light];
    l->flags = (G3DS_U16)(l->flags | G3DS_LIGHT_FLAG_SPOT);
    l->spot_tx = g3ds_read_real32_as_q16(r);
    l->spot_ty = g3ds_read_real32_as_q16(r);
    l->spot_tz = g3ds_read_real32_as_q16(r);
    l->spot_hotspot = g3ds_read_real32_as_q16(r);
    l->spot_falloff = g3ds_read_real32_as_q16(r);
    if (r->err != G3DS_OK) {
        return;
    }
    g3ds_parse_children(r, end);
}

static void g3ds_parse_camera(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Camera *c;
    G3DS_S32 prev_cam;
    G3DS_U16 ci;
    if (r->model->camera_count >= (G3DS_U16)G3DS_MAX_CAMERAS) {
        r->err = G3DS_ERR_CAPACITY_CAMERAS;
        r->model->error_offset = r->pos;
        return;
    }
    ci = r->model->camera_count;
    r->model->camera_count = (G3DS_U16)(r->model->camera_count + 1u);
    c = &r->model->cameras[ci];
    g3ds_copy_cstr(c->name, (G3DS_U32)G3DS_NAME_MAX, r->named_name);
    c->x = g3ds_read_real32_as_q16(r);
    c->y = g3ds_read_real32_as_q16(r);
    c->z = g3ds_read_real32_as_q16(r);
    c->tx = g3ds_read_real32_as_q16(r);
    c->ty = g3ds_read_real32_as_q16(r);
    c->tz = g3ds_read_real32_as_q16(r);
    c->bank_degrees = g3ds_read_real32_as_q16(r);
    c->lens_mm = g3ds_read_real32_as_q16(r);
    if (r->err != G3DS_OK) {
        return;
    }
    r->named_camera_index = (G3DS_S32)ci;
    prev_cam = r->current_camera;
    r->current_camera = (G3DS_S32)ci;
    g3ds_parse_children(r, end);
    r->current_camera = prev_cam;
}

static void g3ds_parse_named_object(G3DS_Reader *r, G3DS_U32 end)
{
    char prev_name[G3DS_NAME_MAX];
    G3DS_S32 prev_named_mesh;
    G3DS_S32 prev_named_light;
    G3DS_S32 prev_named_camera;
    G3DS_U16 prev_flags;
    g3ds_copy_cstr(prev_name, (G3DS_U32)G3DS_NAME_MAX, r->named_name);
    prev_named_mesh = r->named_mesh_index;
    prev_named_light = r->named_light_index;
    prev_named_camera = r->named_camera_index;
    prev_flags = r->named_flags;
    r->named_mesh_index = -1L;
    r->named_light_index = -1L;
    r->named_camera_index = -1L;
    r->named_flags = 0u;
    g3ds_read_cstr(r, end, r->named_name, (G3DS_U32)G3DS_NAME_MAX);
    if (r->err == G3DS_OK) {
        g3ds_parse_children(r, end);
    }
    g3ds_copy_cstr(r->named_name, (G3DS_U32)G3DS_NAME_MAX, prev_name);
    r->named_mesh_index = prev_named_mesh;
    r->named_light_index = prev_named_light;
    r->named_camera_index = prev_named_camera;
    r->named_flags = prev_flags;
}

static void g3ds_skip_key_spline(G3DS_Reader *r, G3DS_U16 flags)
{
    if (flags & 0x0001u) { (void)g3ds_read_real32_as_q16(r); }
    if (flags & 0x0002u) { (void)g3ds_read_real32_as_q16(r); }
    if (flags & 0x0004u) { (void)g3ds_read_real32_as_q16(r); }
    if (flags & 0x0008u) { (void)g3ds_read_real32_as_q16(r); }
    if (flags & 0x0010u) { (void)g3ds_read_real32_as_q16(r); }
}

static void g3ds_read_key_header(G3DS_Reader *r, G3DS_U32 *frame, G3DS_U16 *spline_flags)
{
    *frame = g3ds_read_u32(r);
    *spline_flags = g3ds_read_u16(r);
    g3ds_skip_key_spline(r, *spline_flags);
}

static G3DS_U32 g3ds_read_track_header(G3DS_Reader *r, G3DS_U16 *flags)
{
    G3DS_U32 keys;
    *flags = g3ds_read_u16(r);
    (void)g3ds_read_u32(r);
    (void)g3ds_read_u32(r);
    keys = g3ds_read_u32(r);
    return keys;
}

static void g3ds_parse_pos_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_PosKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_POS_KEYS - (G3DS_U32)r->model->pos_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->pos_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_pos_key = first;
    n->pos_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->pos_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline;
        k->x = g3ds_read_real32_as_q16(r);
        k->y = g3ds_read_real32_as_q16(r);
        k->z = g3ds_read_real32_as_q16(r);
        if (r->err != G3DS_OK) { return; }
    }
    r->model->pos_key_count = (G3DS_U16)(r->model->pos_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_rot_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_RotKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_ROT_KEYS - (G3DS_U32)r->model->rot_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->rot_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_rot_key = first;
    n->rot_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->rot_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline;
        k->angle_degrees = g3ds_read_real32_as_q16(r);
        k->axis_x = g3ds_read_real32_as_q16(r);
        k->axis_y = g3ds_read_real32_as_q16(r);
        k->axis_z = g3ds_read_real32_as_q16(r);
        if (r->err != G3DS_OK) { return; }
    }
    r->model->rot_key_count = (G3DS_U16)(r->model->rot_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_scl_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_SclKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_SCL_KEYS - (G3DS_U32)r->model->scl_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->scl_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_scl_key = first;
    n->scl_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->scl_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline;
        k->x = g3ds_read_real32_as_q16(r);
        k->y = g3ds_read_real32_as_q16(r);
        k->z = g3ds_read_real32_as_q16(r);
        if (r->err != G3DS_OK) { return; }
    }
    r->model->scl_key_count = (G3DS_U16)(r->model->scl_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_color_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_ColorKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_COLOR_KEYS - (G3DS_U32)r->model->color_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->color_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_color_key = first;
    n->color_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->color_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline;
        k->color.r = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
        k->color.g = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
        k->color.b = g3ds_q16_to_u8_clamped(g3ds_read_real32_as_q16(r));
        if (r->err != G3DS_OK) { return; }
    }
    r->model->color_key_count = (G3DS_U16)(r->model->color_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_scalar_track(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 kind)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_ScalarKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_SCALAR_KEYS - (G3DS_U32)r->model->scalar_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->scalar_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    if (n->scalar_key_count == 0u) {
        n->first_scalar_key = first;
    }
    for (i = 0u; i < count; ++i) {
        k = &r->model->scalar_keys[(G3DS_U16)(r->model->scalar_key_count + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline; k->kind = kind;
        k->value = g3ds_read_real32_as_q16(r);
        if (r->err != G3DS_OK) { return; }
    }
    r->model->scalar_key_count = (G3DS_U16)(r->model->scalar_key_count + (G3DS_U16)count);
    n->scalar_key_count = (G3DS_U16)(n->scalar_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_hide_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_HideKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_HIDE_KEYS - (G3DS_U32)r->model->hide_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->hide_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_hide_key = first;
    n->hide_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->hide_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame; k->spline_flags = spline;
        if (r->err != G3DS_OK) { return; }
    }
    r->model->hide_key_count = (G3DS_U16)(r->model->hide_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_anim_node(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 kind)
{
    G3DS_S32 prev;
    G3DS_U16 ni;
    G3DS_AnimNode *n;
    if (r->model->anim_node_count >= (G3DS_U16)G3DS_MAX_ANIM_NODES) {
        r->err = G3DS_ERR_CAPACITY_ANIM_NODES;
        r->model->error_offset = r->pos;
        return;
    }
    ni = r->model->anim_node_count;
    r->model->anim_node_count = (G3DS_U16)(r->model->anim_node_count + 1u);
    n = &r->model->anim_nodes[ni];
    n->kind = kind;
    n->node_id = (G3DS_U16)G3DS_NO_PARENT;
    n->parent_id = (G3DS_U16)G3DS_NO_PARENT;
    prev = r->current_anim_node;
    r->current_anim_node = (G3DS_S32)ni;
    g3ds_parse_children(r, end);
    r->current_anim_node = prev;
}

static void g3ds_parse_node_hdr(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    g3ds_read_cstr(r, end, n->name, (G3DS_U32)G3DS_NAME_MAX);
    if (r->err != G3DS_OK) { return; }
    n->flags1 = g3ds_read_u16(r);
    n->flags2 = g3ds_read_u16(r);
    n->parent_id = g3ds_read_u16(r);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_pivot(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->pivot[0] = g3ds_read_real32_as_q16(r);
    n->pivot[1] = g3ds_read_real32_as_q16(r);
    n->pivot[2] = g3ds_read_real32_as_q16(r);
    n->has_pivot = 1u;
    g3ds_skip_to(r, end);
}

static void g3ds_parse_bbox(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->bbox_min[0] = g3ds_read_real32_as_q16(r);
    n->bbox_min[1] = g3ds_read_real32_as_q16(r);
    n->bbox_min[2] = g3ds_read_real32_as_q16(r);
    n->bbox_max[0] = g3ds_read_real32_as_q16(r);
    n->bbox_max[1] = g3ds_read_real32_as_q16(r);
    n->bbox_max[2] = g3ds_read_real32_as_q16(r);
    n->has_bbox = 1u;
    g3ds_skip_to(r, end);
}

static void g3ds_parse_kfhdr(G3DS_Reader *r, G3DS_U32 end)
{
    r->model->keyframer_revision = g3ds_read_u16(r);
    g3ds_read_cstr(r, end, r->model->keyframer_name, (G3DS_U32)G3DS_NAME_MAX);
    if (r->err != G3DS_OK) { return; }
    r->model->keyframer_length = g3ds_read_u32(r);
    g3ds_skip_to(r, end);
}


static void g3ds_parse_render_q16(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 id)
{
    G3DS_RenderSettings *rs;
    rs = &r->model->render;
    if (id == G3DS_CHUNK_LO_SHADOW_BIAS) { rs->low_shadow_bias = g3ds_read_real32_as_q16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_LOW_SHADOW_BIAS); }
    else if (id == G3DS_CHUNK_HI_SHADOW_BIAS) { rs->high_shadow_bias = g3ds_read_real32_as_q16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_HIGH_SHADOW_BIAS); }
    else if (id == G3DS_CHUNK_SHADOW_FILTER) { rs->shadow_filter = g3ds_read_real32_as_q16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_SHADOW_FILTER); }
    else if (id == G3DS_CHUNK_RAY_BIAS) { rs->ray_bias = g3ds_read_real32_as_q16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_RAY_BIAS); }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_render_u16(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 id)
{
    G3DS_RenderSettings *rs;
    rs = &r->model->render;
    if (id == G3DS_CHUNK_SHADOW_MAP_SIZE) { rs->shadow_map_size = g3ds_read_u16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_SHADOW_MAP_SIZE); }
    else if (id == G3DS_CHUNK_SHADOW_SAMPLES) { rs->shadow_samples = g3ds_read_u16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_SHADOW_SAMPLES); }
    else if (id == G3DS_CHUNK_SHADOW_RANGE) { rs->shadow_range = g3ds_read_u16(r); rs->flags = (G3DS_U16)(rs->flags | G3DS_RENDER_HAS_SHADOW_RANGE); }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_construction_plane(G3DS_Reader *r, G3DS_U32 end)
{
    r->model->render.construction_plane[0] = g3ds_read_real32_as_q16(r);
    r->model->render.construction_plane[1] = g3ds_read_real32_as_q16(r);
    r->model->render.construction_plane[2] = g3ds_read_real32_as_q16(r);
    r->model->render.flags = (G3DS_U16)(r->model->render.flags | G3DS_RENDER_HAS_CONSTRUCTION_PLANE);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_default_view_leaf(G3DS_Reader *r, G3DS_U32 end, G3DS_U16 id)
{
    G3DS_DefaultView *v;
    v = &r->model->default_view;
    v->has_view = 1u;
    if (id == G3DS_CHUNK_VIEW_TOP) { v->kind = G3DS_VIEW_TOP; }
    else if (id == G3DS_CHUNK_VIEW_BOTTOM) { v->kind = G3DS_VIEW_BOTTOM; }
    else if (id == G3DS_CHUNK_VIEW_LEFT) { v->kind = G3DS_VIEW_LEFT; }
    else if (id == G3DS_CHUNK_VIEW_RIGHT) { v->kind = G3DS_VIEW_RIGHT; }
    else if (id == G3DS_CHUNK_VIEW_FRONT) { v->kind = G3DS_VIEW_FRONT; }
    else if (id == G3DS_CHUNK_VIEW_BACK) { v->kind = G3DS_VIEW_BACK; }
    else if (id == G3DS_CHUNK_VIEW_USER) { v->kind = G3DS_VIEW_USER; }
    else if (id == G3DS_CHUNK_VIEW_CAMERA) {
        v->kind = G3DS_VIEW_CAMERA;
        g3ds_read_cstr(r, end, v->camera_name, (G3DS_U32)G3DS_NAME_MAX);
        return;
    }
    v->target[0] = g3ds_read_real32_as_q16(r);
    v->target[1] = g3ds_read_real32_as_q16(r);
    v->target[2] = g3ds_read_real32_as_q16(r);
    v->width = g3ds_read_real32_as_q16(r);
    if (id == G3DS_CHUNK_VIEW_USER) {
        v->horizontal_angle = g3ds_read_real32_as_q16(r);
        v->vertical_angle = g3ds_read_real32_as_q16(r);
        v->bank_angle = g3ds_read_real32_as_q16(r);
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_proc_name(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    if (r->current_object >= 0) {
        obj = &r->model->objects[(G3DS_U16)r->current_object];
        g3ds_read_cstr(r, end, obj->proc_name, (G3DS_U32)G3DS_NAME_MAX);
        obj->has_proc_name = 1u;
    } else {
        g3ds_skip_to(r, end);
    }
}

static void g3ds_parse_proc_data(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_Object *obj;
    if (r->current_object >= 0) {
        obj = &r->model->objects[(G3DS_U16)r->current_object];
        if (end >= r->pos) { obj->proc_data_bytes = end - r->pos; }
        obj->has_proc_data = 1u;
    }
    g3ds_skip_to(r, end);
}

static void g3ds_note_sxp_payload(G3DS_Reader *r, G3DS_U32 end, int is_mask)
{
    G3DS_Map *map;
    G3DS_U32 bytes;
    bytes = 0u;
    if (end >= r->pos) { bytes = end - r->pos; }
    map = g3ds_current_map(r);
    if (map != (G3DS_Map *)0) {
        if (is_mask) { map->sxp_mask_bytes = bytes; map->has_sxp_mask = 1u; }
        else { map->sxp_data_bytes = bytes; map->has_sxp_data = 1u; }
    }
    g3ds_skip_to(r, end);
}

static void g3ds_parse_morph_track(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    G3DS_U16 flags;
    G3DS_U32 count;
    G3DS_U32 i;
    G3DS_U16 first;
    G3DS_U32 frame;
    G3DS_U16 spline;
    G3DS_MorphKey *k;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    count = g3ds_read_track_header(r, &flags);
    (void)flags;
    if ((G3DS_U32)G3DS_MAX_MORPH_KEYS - (G3DS_U32)r->model->morph_key_count < count) {
        r->err = G3DS_ERR_CAPACITY_KEYS; r->model->error_offset = r->pos; return;
    }
    first = r->model->morph_key_count;
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->first_morph_key = first;
    n->morph_key_count = (G3DS_U16)count;
    for (i = 0u; i < count; ++i) {
        k = &r->model->morph_keys[(G3DS_U16)(first + (G3DS_U16)i)];
        g3ds_read_key_header(r, &frame, &spline);
        k->frame = frame;
        k->spline_flags = spline;
        g3ds_read_cstr(r, end, k->target, (G3DS_U32)G3DS_NAME_MAX);
        if (r->err != G3DS_OK) { return; }
    }
    r->model->morph_key_count = (G3DS_U16)(r->model->morph_key_count + (G3DS_U16)count);
    g3ds_skip_to(r, end);
}

static void g3ds_parse_morph_smooth(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_AnimNode *n;
    if (r->current_anim_node < 0) { g3ds_skip_to(r, end); return; }
    n = &r->model->anim_nodes[(G3DS_U16)r->current_anim_node];
    n->morph_smooth_degrees = g3ds_read_real32_as_q16(r);
    n->has_morph_smooth = 1u;
    g3ds_skip_to(r, end);
}

static void g3ds_parse_children(G3DS_Reader *r, G3DS_U32 end)
{
    G3DS_U32 start;
    G3DS_U32 len;
    G3DS_U32 cend;
    G3DS_U16 id;
    G3DS_Map *map;
    G3DS_Material *mat;
    G3DS_Light *l;
    G3DS_Camera *cam;

    while (r->err == G3DS_OK && r->pos + 6u <= end) {
        start = r->pos;
        id = g3ds_read_u16(r);
        len = g3ds_read_u32(r);
        r->model->last_chunk_id = id;
        if (r->err != G3DS_OK) { return; }
        if (len < 6u || start > end || len > (end - start)) {
            r->err = G3DS_ERR_BAD_CHUNK;
            r->model->error_offset = start;
            return;
        }
        cend = start + len;
        g3ds_audit_chunk(r->model, id, len);
        switch (id) {
        case G3DS_CHUNK_MAIN3DS:
        case G3DS_CHUNK_MDATA:
        case G3DS_CHUNK_N_TRI_OBJECT:
        case G3DS_CHUNK_KFDATA:
            if (id == G3DS_CHUNK_N_TRI_OBJECT) { g3ds_parse_n_tri_object(r, cend); }
            else { g3ds_parse_children(r, cend); }
            break;
        case G3DS_CHUNK_M3D_VERSION:
            r->model->file_version = g3ds_read_u32(r); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_M3D_KFVERSION:
            r->model->keyframer_version = g3ds_read_u16(r); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MESH_VERSION:
            r->model->mesh_version = g3ds_read_u32(r); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MASTER_SCALE:
            r->model->master_scale = g3ds_read_real32_as_q16(r); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_LO_SHADOW_BIAS:
        case G3DS_CHUNK_HI_SHADOW_BIAS:
        case G3DS_CHUNK_SHADOW_FILTER:
        case G3DS_CHUNK_RAY_BIAS:
            g3ds_parse_render_q16(r, cend, id); break;
        case G3DS_CHUNK_SHADOW_MAP_SIZE:
        case G3DS_CHUNK_SHADOW_SAMPLES:
        case G3DS_CHUNK_SHADOW_RANGE:
            g3ds_parse_render_u16(r, cend, id); break;
        case G3DS_CHUNK_O_CONSTS:
            g3ds_parse_construction_plane(r, cend); break;
        case G3DS_CHUNK_DEFAULT_VIEW:
            g3ds_parse_children(r, cend); break;
        case G3DS_CHUNK_VIEW_TOP:
        case G3DS_CHUNK_VIEW_BOTTOM:
        case G3DS_CHUNK_VIEW_LEFT:
        case G3DS_CHUNK_VIEW_RIGHT:
        case G3DS_CHUNK_VIEW_FRONT:
        case G3DS_CHUNK_VIEW_BACK:
        case G3DS_CHUNK_VIEW_USER:
        case G3DS_CHUNK_VIEW_CAMERA:
            g3ds_parse_default_view_leaf(r, cend, id); break;
        case G3DS_CHUNK_AMBIENT_LIGHT:
            r->model->has_ambient_light = 1u; g3ds_parse_color_group(r, cend, G3DS_COLOR_AMBIENT); break;
        case G3DS_CHUNK_BIT_MAP:
            r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_HAS_BITMAP);
            g3ds_read_cstr(r, cend, r->model->background.bitmap, (G3DS_U32)G3DS_MAPNAME_MAX); break;
        case G3DS_CHUNK_USE_BIT_MAP:
            r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_USE_BITMAP); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_SOLID_BGND:
            r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_HAS_SOLID); g3ds_parse_color_group(r, cend, G3DS_COLOR_BG_SOLID); break;
        case G3DS_CHUNK_USE_SOLID_BGND:
            r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_USE_SOLID); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_V_GRADIENT:
            g3ds_parse_gradient(r, cend); break;
        case G3DS_CHUNK_USE_V_GRADIENT:
            r->model->background.flags = (G3DS_U16)(r->model->background.flags | G3DS_BG_USE_GRADIENT); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_FOG:
            g3ds_parse_fog(r, cend); break;
        case G3DS_CHUNK_USE_FOG:
            r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_USE_FOG); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_FOG_BGND:
            r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_FOG_BGND); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DISTANCE_CUE:
            g3ds_parse_distance_cue(r, cend); break;
        case G3DS_CHUNK_USE_DISTANCE_CUE:
            r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_USE_DISTANCE); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DCUE_BGND:
            r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_DCUE_BGND); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_LAYER_FOG:
            g3ds_parse_layer_fog(r, cend); break;
        case G3DS_CHUNK_USE_LAYER_FOG:
            r->model->atmosphere.flags = (G3DS_U16)(r->model->atmosphere.flags | G3DS_ATMOS_USE_LAYER); g3ds_skip_to(r, cend); break;

        case G3DS_CHUNK_MAT_ENTRY: g3ds_parse_material_entry(r, cend); break;
        case G3DS_CHUNK_MAT_NAME:
            if (r->current_material >= 0) {
                g3ds_read_cstr(r, cend, r->model->materials[(G3DS_U16)r->current_material].name, (G3DS_U32)G3DS_NAME_MAX);
            } else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_AMBIENT: g3ds_parse_color_group(r, cend, G3DS_COLOR_MAT_AMB); break;
        case G3DS_CHUNK_MAT_DIFFUSE: g3ds_parse_color_group(r, cend, G3DS_COLOR_MAT_DIF); break;
        case G3DS_CHUNK_MAT_SPECULAR: g3ds_parse_color_group(r, cend, G3DS_COLOR_MAT_SPC); break;
        case G3DS_CHUNK_COLOR_F:
        case G3DS_CHUNK_COLOR_24:
        case G3DS_CHUNK_LIN_COLOR_24:
        case G3DS_CHUNK_LIN_COLOR_F:
            g3ds_parse_color_chunk(r, cend, id); break;
        case G3DS_CHUNK_MAT_SHININESS:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].shininess); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_SHIN2PCT:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].shininess_strength); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_TRANSPARENCY:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].transparency); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_XPFALL:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].transparency_falloff); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_REFBLUR:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].reflection_blur); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_SELF_ILPCT:
            if (r->current_material >= 0) { g3ds_parse_percent_to_material(r, cend, &r->model->materials[(G3DS_U16)r->current_material].self_illum_pct); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_SHADING:
            if (r->current_material >= 0) { r->model->materials[(G3DS_U16)r->current_material].shading = g3ds_read_u16(r); }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MAT_WIRESIZE:
            if (r->current_material >= 0) { r->model->materials[(G3DS_U16)r->current_material].wire_size = g3ds_read_real32_as_q16(r); }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MAT_SELF_ILLUM:
        case G3DS_CHUNK_MAT_TWO_SIDE:
        case G3DS_CHUNK_MAT_DECAL:
        case G3DS_CHUNK_MAT_ADDITIVE:
        case G3DS_CHUNK_MAT_WIRE:
        case G3DS_CHUNK_MAT_FACEMAP:
        case G3DS_CHUNK_MAT_PHONGSOFT:
        case G3DS_CHUNK_MAT_WIREABS:
        case G3DS_CHUNK_MAT_USE_XPFALL:
        case G3DS_CHUNK_MAT_USE_REFBLUR:
            if (r->current_material >= 0) {
                mat = &r->model->materials[(G3DS_U16)r->current_material];
                if (id == G3DS_CHUNK_MAT_SELF_ILLUM) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_SELF_ILLUM);
                else if (id == G3DS_CHUNK_MAT_TWO_SIDE) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_TWO_SIDE);
                else if (id == G3DS_CHUNK_MAT_DECAL) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_DECAL);
                else if (id == G3DS_CHUNK_MAT_ADDITIVE) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_ADDITIVE);
                else if (id == G3DS_CHUNK_MAT_WIRE) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_WIRE);
                else if (id == G3DS_CHUNK_MAT_FACEMAP) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_FACEMAP);
                else if (id == G3DS_CHUNK_MAT_PHONGSOFT) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_PHONGSOFT);
                else if (id == G3DS_CHUNK_MAT_WIREABS) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_WIREABS);
                else if (id == G3DS_CHUNK_MAT_USE_XPFALL) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_USE_XPFALL);
                else if (id == G3DS_CHUNK_MAT_USE_REFBLUR) mat->flags = (G3DS_U16)(mat->flags | G3DS_MAT_FLAG_USE_REFBLUR);
            }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MAT_ACUBIC: g3ds_parse_acubic(r, cend); break;
        case G3DS_CHUNK_MAT_TEXMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_DIFFUSE); break;
        case G3DS_CHUNK_MAT_TEX2MAP: g3ds_parse_map_block(r, cend, G3DS_MAP_DIFFUSE2); break;
        case G3DS_CHUNK_MAT_OPACMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_OPACITY); break;
        case G3DS_CHUNK_MAT_BUMPMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_BUMP); break;
        case G3DS_CHUNK_MAT_SPECMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_SPECULAR); break;
        case G3DS_CHUNK_MAT_SHINMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_SHININESS); break;
        case G3DS_CHUNK_MAT_SELFIMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_SELFILLUM); break;
        case G3DS_CHUNK_MAT_REFLMAP: g3ds_parse_map_block(r, cend, G3DS_MAP_REFLECTION); break;
        case G3DS_CHUNK_MAT_TEXMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_DIFFUSE_MASK); break;
        case G3DS_CHUNK_MAT_TEX2MASK: g3ds_parse_map_block(r, cend, G3DS_MAP_DIFFUSE2_MASK); break;
        case G3DS_CHUNK_MAT_OPACMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_OPACITY_MASK); break;
        case G3DS_CHUNK_MAT_BUMPMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_BUMP_MASK); break;
        case G3DS_CHUNK_MAT_SPECMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_SPECULAR_MASK); break;
        case G3DS_CHUNK_MAT_SHINMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_SHININESS_MASK); break;
        case G3DS_CHUNK_MAT_SELFIMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_SELFILLUM_MASK); break;
        case G3DS_CHUNK_MAT_REFLMASK: g3ds_parse_map_block(r, cend, G3DS_MAP_REFLECTION_MASK); break;
        case G3DS_CHUNK_MAT_SXP_TEXT_DATA:
        case G3DS_CHUNK_MAT_SXP_TEXT2_DATA:
        case G3DS_CHUNK_MAT_SXP_OPAC_DATA:
        case G3DS_CHUNK_MAT_SXP_BUMP_DATA:
        case G3DS_CHUNK_MAT_SXP_SPEC_DATA:
        case G3DS_CHUNK_MAT_SXP_SHIN_DATA:
        case G3DS_CHUNK_MAT_SXP_SELFI_DATA:
        case G3DS_CHUNK_MAT_SXP_REFL_DATA:
            g3ds_note_sxp_payload(r, cend, 0); break;
        case G3DS_CHUNK_MAT_SXP_TEXT_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_TEXT2_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_OPAC_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_BUMP_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_SPEC_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_SHIN_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_SELFI_MASKDATA:
        case G3DS_CHUNK_MAT_SXP_REFL_MASKDATA:
            g3ds_note_sxp_payload(r, cend, 1); break;
        case G3DS_CHUNK_MAT_MAP_COL1: g3ds_parse_raw_color_to_target(r, cend, G3DS_COLOR_MAP_TINT1); break;
        case G3DS_CHUNK_MAT_MAP_COL2: g3ds_parse_raw_color_to_target(r, cend, G3DS_COLOR_MAP_TINT2); break;
        case G3DS_CHUNK_MAT_MAP_RCOL: g3ds_parse_raw_color_to_target(r, cend, G3DS_COLOR_MAP_RCOL); break;
        case G3DS_CHUNK_MAT_MAP_GCOL: g3ds_parse_raw_color_to_target(r, cend, G3DS_COLOR_MAP_GCOL); break;
        case G3DS_CHUNK_MAT_MAP_BCOL: g3ds_parse_raw_color_to_target(r, cend, G3DS_COLOR_MAP_BCOL); break;
        case G3DS_CHUNK_PERCENT_I:
        case G3DS_CHUNK_PERCENT_R:
            map = g3ds_current_map(r);
            if (map != (G3DS_Map *)0) { map->strength = g3ds_read_percent(r, cend, id); }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_MAPNAME:
            map = g3ds_current_map(r);
            if (map != (G3DS_Map *)0) { g3ds_read_cstr(r, cend, map->filename, (G3DS_U32)G3DS_MAPNAME_MAX); map->has_map = 1u; }
            else { g3ds_skip_to(r, cend); }
            break;
        case G3DS_CHUNK_MAT_MAP_TILING:
            map = g3ds_current_map(r);
            if (map != (G3DS_Map *)0) { map->tiling = g3ds_read_u16(r); }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MAT_MAP_BLUR:
        case G3DS_CHUNK_MAT_MAP_USCALE:
        case G3DS_CHUNK_MAT_MAP_VSCALE:
        case G3DS_CHUNK_MAT_MAP_UOFFSET:
        case G3DS_CHUNK_MAT_MAP_VOFFSET:
        case G3DS_CHUNK_MAT_MAP_ANG:
            map = g3ds_current_map(r);
            if (map != (G3DS_Map *)0) {
                if (id == G3DS_CHUNK_MAT_MAP_BLUR) map->blur = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_MAT_MAP_USCALE) map->u_scale = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_MAT_MAP_VSCALE) map->v_scale = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_MAT_MAP_UOFFSET) map->u_offset = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_MAT_MAP_VOFFSET) map->v_offset = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_MAT_MAP_ANG) map->angle_degrees = g3ds_read_real32_as_q16(r);
            }
            g3ds_skip_to(r, cend); break;

        case G3DS_CHUNK_NAMED_OBJECT: g3ds_parse_named_object(r, cend); break;
        case G3DS_CHUNK_OBJ_HIDDEN: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_HIDDEN); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_VIS_LOFTER: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_VIS_LOFTER); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_DOESNT_CAST: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_DOESNT_CAST); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_MATTE: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_MATTE); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_FAST: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_FAST); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_PROCEDURAL: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_PROCEDURAL); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_FROZEN: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_FROZEN); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_OBJ_DONT_RCVSHADOW: g3ds_apply_named_flag(r, G3DS_OBJ_FLAG_DONT_RECEIVE); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_POINT_ARRAY: g3ds_parse_point_array(r, cend); break;
        case G3DS_CHUNK_POINT_FLAGS: g3ds_parse_point_flags(r, cend); break;
        case G3DS_CHUNK_FACE_ARRAY: g3ds_parse_face_array(r, cend); break;
        case G3DS_CHUNK_MSH_MAT_GROUP: g3ds_parse_mat_group(r, cend); break;
        case G3DS_CHUNK_SMOOTH_GROUP: g3ds_parse_smooth_group(r, cend); break;
        case G3DS_CHUNK_TEX_VERTS: g3ds_parse_tex_verts(r, cend); break;
        case G3DS_CHUNK_MESH_MATRIX: g3ds_parse_mesh_matrix(r, cend); break;
        case G3DS_CHUNK_MESH_COLOR:
            if (r->current_object >= 0) { r->model->objects[(G3DS_U16)r->current_object].mesh_color = g3ds_read_u8(r); }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_MESH_TEX_INFO: g3ds_parse_mesh_tex_info(r, cend); break;
        case G3DS_CHUNK_MSH_BOXMAP: g3ds_parse_boxmap(r, cend); break;
        case G3DS_CHUNK_PROC_NAME: g3ds_parse_proc_name(r, cend); break;
        case G3DS_CHUNK_PROC_DATA: g3ds_parse_proc_data(r, cend); break;

        case G3DS_CHUNK_N_DIRECT_LIGHT: g3ds_parse_direct_light(r, cend); break;
        case G3DS_CHUNK_DL_SPOTLIGHT: g3ds_parse_spotlight(r, cend); break;
        case G3DS_CHUNK_DL_OFF:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_OFF);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_ATTENUATE:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_ATTENUATE);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_RAYSHAD:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_RAYSHAD);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_SHADOWED:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_SHADOWED);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_SEE_CONE:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_SEE_CONE);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_SPOT_RECT:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_RECTANGULAR);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_SPOT_OVERSHOOT:
            if (r->current_light >= 0) r->model->lights[(G3DS_U16)r->current_light].flags = (G3DS_U16)(r->model->lights[(G3DS_U16)r->current_light].flags | G3DS_LIGHT_FLAG_OVERSHOOT);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_INNER_RANGE:
        case G3DS_CHUNK_DL_OUTER_RANGE:
        case G3DS_CHUNK_DL_MULTIPLIER:
        case G3DS_CHUNK_DL_SPOT_ROLL:
        case G3DS_CHUNK_DL_SPOT_ASPECT:
        case G3DS_CHUNK_DL_RAY_BIAS:
            if (r->current_light >= 0) {
                l = &r->model->lights[(G3DS_U16)r->current_light];
                if (id == G3DS_CHUNK_DL_INNER_RANGE) l->inner_range = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_DL_OUTER_RANGE) l->outer_range = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_DL_MULTIPLIER) l->multiplier = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_DL_SPOT_ROLL) l->spot_roll = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_DL_SPOT_ASPECT) l->spot_aspect = g3ds_read_real32_as_q16(r);
                else if (id == G3DS_CHUNK_DL_RAY_BIAS) l->ray_bias = g3ds_read_real32_as_q16(r);
            }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_LOCAL_SHADOW2:
            if (r->current_light >= 0) {
                l = &r->model->lights[(G3DS_U16)r->current_light];
                l->local_shadow_bias = g3ds_read_real32_as_q16(r);
                l->local_shadow_filter = g3ds_read_real32_as_q16(r);
                l->local_shadow_map_size = g3ds_read_u16(r);
            }
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_DL_SPOT_PROJECTOR:
            if (r->current_light >= 0) { g3ds_read_cstr(r, cend, r->model->lights[(G3DS_U16)r->current_light].projector, (G3DS_U32)G3DS_MAPNAME_MAX); }
            else { g3ds_skip_to(r, cend); }
            break;

        case G3DS_CHUNK_N_CAMERA: g3ds_parse_camera(r, cend); break;
        case G3DS_CHUNK_CAM_SEE_CONE:
            if (r->current_camera >= 0) r->model->cameras[(G3DS_U16)r->current_camera].see_cone = 1u;
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_CAM_RANGES:
            if (r->current_camera >= 0) {
                cam = &r->model->cameras[(G3DS_U16)r->current_camera];
                cam->near_range = g3ds_read_real32_as_q16(r);
                cam->far_range = g3ds_read_real32_as_q16(r);
                cam->has_ranges = 1u;
            }
            g3ds_skip_to(r, cend); break;

        case G3DS_CHUNK_KFHDR: g3ds_parse_kfhdr(r, cend); break;
        case G3DS_CHUNK_KFSEG:
            r->model->keyframer_segment_start = g3ds_read_u32(r);
            r->model->keyframer_segment_end = g3ds_read_u32(r);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_KFCURTIME:
            r->model->keyframer_current_frame = g3ds_read_u32(r); g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_AMBIENT_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_AMBIENT); break;
        case G3DS_CHUNK_OBJECT_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_OBJECT); break;
        case G3DS_CHUNK_CAMERA_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_CAMERA); break;
        case G3DS_CHUNK_TARGET_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_CAMERA_TARGET); break;
        case G3DS_CHUNK_LIGHT_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_LIGHT); break;
        case G3DS_CHUNK_L_TARGET_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_SPOT_TARGET); break;
        case G3DS_CHUNK_SPOT_NODE: g3ds_parse_anim_node(r, cend, G3DS_NODE_SPOT_LIGHT); break;
        case G3DS_CHUNK_NODE_ID:
            if (r->current_anim_node >= 0) r->model->anim_nodes[(G3DS_U16)r->current_anim_node].node_id = g3ds_read_u16(r);
            g3ds_skip_to(r, cend); break;
        case G3DS_CHUNK_NODE_HDR: g3ds_parse_node_hdr(r, cend); break;
        case G3DS_CHUNK_INSTANCE_NAME:
            if (r->current_anim_node >= 0) g3ds_read_cstr(r, cend, r->model->anim_nodes[(G3DS_U16)r->current_anim_node].instance_name, (G3DS_U32)G3DS_NAME_MAX);
            else g3ds_skip_to(r, cend);
            break;
        case G3DS_CHUNK_PIVOT: g3ds_parse_pivot(r, cend); break;
        case G3DS_CHUNK_BOUNDBOX: g3ds_parse_bbox(r, cend); break;
        case G3DS_CHUNK_MORPH_SMOOTH: g3ds_parse_morph_smooth(r, cend); break;
        case G3DS_CHUNK_POS_TRACK: g3ds_parse_pos_track(r, cend); break;
        case G3DS_CHUNK_ROT_TRACK: g3ds_parse_rot_track(r, cend); break;
        case G3DS_CHUNK_SCL_TRACK: g3ds_parse_scl_track(r, cend); break;
        case G3DS_CHUNK_COLOR_TRACK: g3ds_parse_color_track(r, cend); break;
        case G3DS_CHUNK_MORPH_TRACK: g3ds_parse_morph_track(r, cend); break;
        case G3DS_CHUNK_FOV_TRACK: g3ds_parse_scalar_track(r, cend, G3DS_SCALAR_FOV); break;
        case G3DS_CHUNK_ROLL_TRACK: g3ds_parse_scalar_track(r, cend, G3DS_SCALAR_ROLL); break;
        case G3DS_CHUNK_HOT_TRACK: g3ds_parse_scalar_track(r, cend, G3DS_SCALAR_HOTSPOT); break;
        case G3DS_CHUNK_FALL_TRACK: g3ds_parse_scalar_track(r, cend, G3DS_SCALAR_FALLOFF); break;
        case G3DS_CHUNK_HIDE_TRACK: g3ds_parse_hide_track(r, cend); break;

        default:
            r->model->ignored_chunk_count = r->model->ignored_chunk_count + 1u;
            g3ds_skip_to(r, cend);
            break;
        }
        if (r->err != G3DS_OK) { return; }
        if (r->pos != cend) { g3ds_skip_to(r, cend); }
    }
    if (r->err == G3DS_OK && r->pos != end) {
        if (r->pos < end) { r->model->ignored_chunk_count = r->model->ignored_chunk_count + 1u; }
        r->pos = end;
    }
}

int g3ds_load_memory(const G3DS_U8 *data, G3DS_U32 size, G3DS_Model *model)
{
    G3DS_Reader r;
    G3DS_U16 root_id;
    G3DS_U32 root_len;
    if (data == (const G3DS_U8 *)0 || model == (G3DS_Model *)0) {
        return G3DS_ERR_NULL;
    }
    g3ds_model_init(model);
    r.data = data;
    r.size = size;
    r.pos = 0u;
    r.err = G3DS_OK;
    r.model = model;
    r.current_object = -1L;
    r.current_material = -1L;
    r.current_light = -1L;
    r.current_camera = -1L;
    r.current_anim_node = -1L;
    r.current_map_slot = -1L;
    r.face_base = 0u;
    r.face_count = 0u;
    r.named_name[0] = '\0';
    r.named_mesh_index = -1L;
    r.named_light_index = -1L;
    r.named_camera_index = -1L;
    r.named_flags = 0u;
    r.color_target = G3DS_COLOR_NONE;
    if (size < 6u) {
        model->error_offset = 0u;
        return G3DS_ERR_SHORT;
    }
    root_id = g3ds_read_u16(&r);
    root_len = g3ds_read_u32(&r);
    model->last_chunk_id = root_id;
    if (r.err != G3DS_OK) { return r.err; }
    if (root_id != (G3DS_U16)G3DS_CHUNK_MAIN3DS) {
        model->error_offset = 0u;
        return G3DS_ERR_BAD_MAGIC;
    }
    if (root_len < 6u || root_len > size) {
        model->error_offset = 0u;
        return G3DS_ERR_BAD_CHUNK;
    }
    g3ds_parse_children(&r, root_len);
    if (r.err != G3DS_OK) { return r.err; }
    return G3DS_OK;
}


static G3DS_S32 g3ds_abs_s32(G3DS_S32 v)
{
    if (v < 0L) {
        if (v == G3DS_Q16_MIN) { return G3DS_Q16_MAX; }
        return (G3DS_S32)(-v);
    }
    return v;
}

static G3DS_S32 g3ds_add_sat(G3DS_S32 a, G3DS_S32 b)
{
    if (b > 0L && a > (G3DS_Q16_MAX - b)) { return G3DS_Q16_MAX; }
    if (b < 0L && a < (G3DS_Q16_MIN - b)) { return G3DS_Q16_MIN; }
    return (G3DS_S32)(a + b);
}

static G3DS_S32 g3ds_clamp_s16(G3DS_S32 v)
{
    if (v > 32767L) { return 32767L; }
    if (v < -32768L) { return -32768L; }
    return v;
}

static G3DS_Q16 g3ds_ratio_to_q16(G3DS_S32 num, G3DS_S32 den)
{
    G3DS_U32 n;
    G3DS_U32 d;
    G3DS_U32 rem;
    G3DS_U32 q;
    G3DS_U16 i;
    int neg;
    if (den <= 0L || num == 0L) { return 0L; }
    neg = 0;
    if (num < 0L) { neg = 1; n = (G3DS_U32)g3ds_abs_s32(num); }
    else { n = (G3DS_U32)num; }
    d = (G3DS_U32)den;
    if (n >= d) { return neg ? (G3DS_Q16)(-G3DS_Q16_ONE) : (G3DS_Q16)G3DS_Q16_ONE; }
    rem = n;
    q = 0u;
    for (i = 0u; i < 16u; ++i) {
        rem = (rem << 1) & 0xFFFFFFFFUL;
        q = (q << 1) & 0xFFFFFFFFUL;
        if (rem >= d) {
            rem -= d;
            q |= 1u;
        }
    }
    return neg ? (G3DS_Q16)(-(G3DS_S32)q) : (G3DS_Q16)q;
}

static void g3ds_normalize_max3(G3DS_S32 x, G3DS_S32 y, G3DS_S32 z, G3DS_Q16 *ox, G3DS_Q16 *oy, G3DS_Q16 *oz)
{
    G3DS_S32 ax;
    G3DS_S32 ay;
    G3DS_S32 az;
    G3DS_S32 maxv;
    ax = g3ds_abs_s32(x);
    ay = g3ds_abs_s32(y);
    az = g3ds_abs_s32(z);
    maxv = ax;
    if (ay > maxv) { maxv = ay; }
    if (az > maxv) { maxv = az; }
    if (maxv <= 0L) {
        *ox = 0L; *oy = 0L; *oz = G3DS_Q16_ONE;
        return;
    }
    *ox = g3ds_ratio_to_q16(x, maxv);
    *oy = g3ds_ratio_to_q16(y, maxv);
    *oz = g3ds_ratio_to_q16(z, maxv);
}

static void g3ds_compute_face_normal(const G3DS_Model *model, G3DS_U16 face_index, G3DS_Q16 *nx, G3DS_Q16 *ny, G3DS_Q16 *nz)
{
    const G3DS_Face *f;
    const G3DS_Vertex *a;
    const G3DS_Vertex *b;
    const G3DS_Vertex *c;
    G3DS_S32 ax;
    G3DS_S32 ay;
    G3DS_S32 az;
    G3DS_S32 bx;
    G3DS_S32 by;
    G3DS_S32 bz;
    G3DS_S32 cx;
    G3DS_S32 cy;
    G3DS_S32 cz;
    G3DS_S32 ux;
    G3DS_S32 uy;
    G3DS_S32 uz;
    G3DS_S32 vx;
    G3DS_S32 vy;
    G3DS_S32 vz;
    G3DS_S32 rx;
    G3DS_S32 ry;
    G3DS_S32 rz;
    f = &model->faces[face_index];
    a = &model->vertices[f->a];
    b = &model->vertices[f->b];
    c = &model->vertices[f->c];
    ax = a->x >> 8; ay = a->y >> 8; az = a->z >> 8;
    bx = b->x >> 8; by = b->y >> 8; bz = b->z >> 8;
    cx = c->x >> 8; cy = c->y >> 8; cz = c->z >> 8;
    ux = g3ds_clamp_s16((G3DS_S32)(bx - ax));
    uy = g3ds_clamp_s16((G3DS_S32)(by - ay));
    uz = g3ds_clamp_s16((G3DS_S32)(bz - az));
    vx = g3ds_clamp_s16((G3DS_S32)(cx - ax));
    vy = g3ds_clamp_s16((G3DS_S32)(cy - ay));
    vz = g3ds_clamp_s16((G3DS_S32)(cz - az));
    rx = (G3DS_S32)(uy * vz - uz * vy);
    ry = (G3DS_S32)(uz * vx - ux * vz);
    rz = (G3DS_S32)(ux * vy - uy * vx);
    g3ds_normalize_max3(rx, ry, rz, nx, ny, nz);
}

static int g3ds_face_has_vertex(const G3DS_Face *f, G3DS_U16 v)
{
    return (f->a == v || f->b == v || f->c == v) ? 1 : 0;
}

static int g3ds_smoothing_compatible(const G3DS_Face *a, const G3DS_Face *b, G3DS_U16 same_face)
{
    if (same_face) { return 1; }
    if (a->smoothing != 0u && b->smoothing != 0u && ((a->smoothing & b->smoothing) != 0u)) { return 1; }
    return 0;
}

static void g3ds_build_corner_normal(const G3DS_Model *model, const G3DS_NormalSet *face_normals, G3DS_U16 object_index, G3DS_U16 face_index, G3DS_U16 vertex_index, G3DS_Q16 *nx, G3DS_Q16 *ny, G3DS_Q16 *nz)
{
    const G3DS_Object *obj;
    const G3DS_Face *base_face;
    const G3DS_Face *test_face;
    G3DS_U16 i;
    G3DS_U16 fi;
    G3DS_S32 sx;
    G3DS_S32 sy;
    G3DS_S32 sz;
    obj = &model->objects[object_index];
    base_face = &model->faces[face_index];
    sx = 0L; sy = 0L; sz = 0L;
    for (i = 0u; i < obj->face_count; ++i) {
        fi = (G3DS_U16)(obj->first_face + i);
        test_face = &model->faces[fi];
        if (g3ds_face_has_vertex(test_face, vertex_index) && g3ds_smoothing_compatible(base_face, test_face, (G3DS_U16)(fi == face_index))) {
            sx = g3ds_add_sat(sx, face_normals->face_x[fi]);
            sy = g3ds_add_sat(sy, face_normals->face_y[fi]);
            sz = g3ds_add_sat(sz, face_normals->face_z[fi]);
        }
    }
    g3ds_normalize_max3(sx, sy, sz, nx, ny, nz);
}

int g3ds_compute_object_bounds(const G3DS_Model *model, G3DS_U16 object_index, G3DS_Bounds *bounds)
{
    const G3DS_Object *obj;
    const G3DS_Vertex *v;
    G3DS_U16 i;
    G3DS_U16 vi;
    if (model == (const G3DS_Model *)0 || bounds == (G3DS_Bounds *)0) { return G3DS_ERR_NULL; }
    if (object_index >= model->object_count) { return G3DS_ERR_BAD_INDEX; }
    obj = &model->objects[object_index];
    if (obj->vertex_count == 0u) { return G3DS_ERR_BAD_INDEX; }
    vi = obj->first_vertex;
    v = &model->vertices[vi];
    bounds->min_x = bounds->max_x = v->x;
    bounds->min_y = bounds->max_y = v->y;
    bounds->min_z = bounds->max_z = v->z;
    for (i = 1u; i < obj->vertex_count; ++i) {
        vi = (G3DS_U16)(obj->first_vertex + i);
        v = &model->vertices[vi];
        if (v->x < bounds->min_x) { bounds->min_x = v->x; }
        if (v->y < bounds->min_y) { bounds->min_y = v->y; }
        if (v->z < bounds->min_z) { bounds->min_z = v->z; }
        if (v->x > bounds->max_x) { bounds->max_x = v->x; }
        if (v->y > bounds->max_y) { bounds->max_y = v->y; }
        if (v->z > bounds->max_z) { bounds->max_z = v->z; }
    }
    return G3DS_OK;
}

static G3DS_U16 g3ds_last_pos_key(const G3DS_Model *model, const G3DS_AnimNode *node, G3DS_U32 frame, int *found)
{
    G3DS_U16 i;
    G3DS_U16 idx;
    G3DS_U16 best;
    *found = 0;
    best = node->first_pos_key;
    for (i = 0u; i < node->pos_key_count; ++i) {
        idx = (G3DS_U16)(node->first_pos_key + i);
        if (model->pos_keys[idx].frame <= frame) { best = idx; *found = 1; }
    }
    if (!*found && node->pos_key_count > 0u) { best = node->first_pos_key; *found = 1; }
    return best;
}

static G3DS_U16 g3ds_last_rot_key(const G3DS_Model *model, const G3DS_AnimNode *node, G3DS_U32 frame, int *found)
{
    G3DS_U16 i;
    G3DS_U16 idx;
    G3DS_U16 best;
    *found = 0;
    best = node->first_rot_key;
    for (i = 0u; i < node->rot_key_count; ++i) {
        idx = (G3DS_U16)(node->first_rot_key + i);
        if (model->rot_keys[idx].frame <= frame) { best = idx; *found = 1; }
    }
    if (!*found && node->rot_key_count > 0u) { best = node->first_rot_key; *found = 1; }
    return best;
}

static G3DS_U16 g3ds_last_scl_key(const G3DS_Model *model, const G3DS_AnimNode *node, G3DS_U32 frame, int *found)
{
    G3DS_U16 i;
    G3DS_U16 idx;
    G3DS_U16 best;
    *found = 0;
    best = node->first_scl_key;
    for (i = 0u; i < node->scl_key_count; ++i) {
        idx = (G3DS_U16)(node->first_scl_key + i);
        if (model->scl_keys[idx].frame <= frame) { best = idx; *found = 1; }
    }
    if (!*found && node->scl_key_count > 0u) { best = node->first_scl_key; *found = 1; }
    return best;
}

int g3ds_sample_node_pose_step(const G3DS_Model *model, G3DS_U16 node_index, G3DS_U32 frame, G3DS_NodePose *pose)
{
    const G3DS_AnimNode *node;
    G3DS_U16 idx;
    G3DS_U16 i;
    int found;
    if (model == (const G3DS_Model *)0 || pose == (G3DS_NodePose *)0) { return G3DS_ERR_NULL; }
    if (node_index >= model->anim_node_count) { return G3DS_ERR_BAD_INDEX; }
    g3ds_zero(pose, (G3DS_U32)sizeof(G3DS_NodePose));
    pose->scl[0] = G3DS_Q16_ONE; pose->scl[1] = G3DS_Q16_ONE; pose->scl[2] = G3DS_Q16_ONE;
    pose->rot_axis[0] = 0L; pose->rot_axis[1] = 0L; pose->rot_axis[2] = G3DS_Q16_ONE;
    pose->color.r = 255u; pose->color.g = 255u; pose->color.b = 255u;
    node = &model->anim_nodes[node_index];
    idx = g3ds_last_pos_key(model, node, frame, &found);
    if (found) {
        pose->pos[0] = model->pos_keys[idx].x; pose->pos[1] = model->pos_keys[idx].y; pose->pos[2] = model->pos_keys[idx].z; pose->has_pos = 1u;
    }
    idx = g3ds_last_rot_key(model, node, frame, &found);
    if (found) {
        pose->rot_angle_degrees = model->rot_keys[idx].angle_degrees;
        pose->rot_axis[0] = model->rot_keys[idx].axis_x; pose->rot_axis[1] = model->rot_keys[idx].axis_y; pose->rot_axis[2] = model->rot_keys[idx].axis_z; pose->has_rot = 1u;
    }
    idx = g3ds_last_scl_key(model, node, frame, &found);
    if (found) {
        pose->scl[0] = model->scl_keys[idx].x; pose->scl[1] = model->scl_keys[idx].y; pose->scl[2] = model->scl_keys[idx].z; pose->has_scl = 1u;
    }
    for (i = 0u; i < node->color_key_count; ++i) {
        idx = (G3DS_U16)(node->first_color_key + i);
        if (model->color_keys[idx].frame <= frame || !pose->has_color) { pose->color = model->color_keys[idx].color; pose->has_color = 1u; }
    }
    for (i = 0u; i < node->scalar_key_count; ++i) {
        idx = (G3DS_U16)(node->first_scalar_key + i);
        if (model->scalar_keys[idx].frame <= frame) {
            if (model->scalar_keys[idx].kind == G3DS_SCALAR_FOV) { pose->fov = model->scalar_keys[idx].value; pose->has_fov = 1u; }
            else if (model->scalar_keys[idx].kind == G3DS_SCALAR_ROLL) { pose->roll = model->scalar_keys[idx].value; pose->has_roll = 1u; }
            else if (model->scalar_keys[idx].kind == G3DS_SCALAR_HOTSPOT) { pose->hotspot = model->scalar_keys[idx].value; pose->has_hotspot = 1u; }
            else if (model->scalar_keys[idx].kind == G3DS_SCALAR_FALLOFF) { pose->falloff = model->scalar_keys[idx].value; pose->has_falloff = 1u; }
        }
    }
    for (i = 0u; i < node->hide_key_count; ++i) {
        idx = (G3DS_U16)(node->first_hide_key + i);
        if (model->hide_keys[idx].frame <= frame) { pose->hidden = 1u; }
    }
    return G3DS_OK;
}

int g3ds_build_normals(const G3DS_Model *model, G3DS_NormalSet *normals)
{
    G3DS_U16 i;
    G3DS_U16 j;
    G3DS_U16 fi;
    const G3DS_Object *obj;
    const G3DS_Face *f;
    if (model == (const G3DS_Model *)0 || normals == (G3DS_NormalSet *)0) { return G3DS_ERR_NULL; }
    for (i = 0u; i < model->face_count; ++i) {
        g3ds_compute_face_normal(model, i, &normals->face_x[i], &normals->face_y[i], &normals->face_z[i]);
        normals->corner_ax[i] = normals->face_x[i]; normals->corner_ay[i] = normals->face_y[i]; normals->corner_az[i] = normals->face_z[i];
        normals->corner_bx[i] = normals->face_x[i]; normals->corner_by[i] = normals->face_y[i]; normals->corner_bz[i] = normals->face_z[i];
        normals->corner_cx[i] = normals->face_x[i]; normals->corner_cy[i] = normals->face_y[i]; normals->corner_cz[i] = normals->face_z[i];
    }
    for (i = 0u; i < model->object_count; ++i) {
        obj = &model->objects[i];
        for (j = 0u; j < obj->face_count; ++j) {
            fi = (G3DS_U16)(obj->first_face + j);
            f = &model->faces[fi];
            g3ds_build_corner_normal(model, normals, i, fi, f->a, &normals->corner_ax[fi], &normals->corner_ay[fi], &normals->corner_az[fi]);
            g3ds_build_corner_normal(model, normals, i, fi, f->b, &normals->corner_bx[fi], &normals->corner_by[fi], &normals->corner_bz[fi]);
            g3ds_build_corner_normal(model, normals, i, fi, f->c, &normals->corner_cx[fi], &normals->corner_cy[fi], &normals->corner_cz[fi]);
        }
    }
    return G3DS_OK;
}

int g3ds_find_object(const G3DS_Model *model, const char *name)
{
    G3DS_U16 i;
    if (model == (const G3DS_Model *)0 || name == (const char *)0) { return G3DS_ERR_NULL; }
    for (i = 0u; i < model->object_count; ++i) {
        if (g3ds_streq(model->objects[i].name, name)) { return (int)i; }
    }
    return G3DS_ERR_BAD_INDEX;
}

int g3ds_find_material(const G3DS_Model *model, const char *name)
{
    G3DS_U16 i;
    if (model == (const G3DS_Model *)0 || name == (const char *)0) { return G3DS_ERR_NULL; }
    for (i = 0u; i < model->material_count; ++i) {
        if (g3ds_streq(model->materials[i].name, name)) { return (int)i; }
    }
    return G3DS_ERR_BAD_INDEX;
}

int g3ds_find_anim_node_by_id(const G3DS_Model *model, G3DS_U16 node_id)
{
    G3DS_U16 i;
    if (model == (const G3DS_Model *)0) { return G3DS_ERR_NULL; }
    for (i = 0u; i < model->anim_node_count; ++i) {
        if (model->anim_nodes[i].node_id == node_id) { return (int)i; }
    }
    return G3DS_ERR_BAD_INDEX;
}

int g3ds_object_local_matrix(const G3DS_Model *model, G3DS_U16 object_index, G3DS_Q16 out12[12])
{
    G3DS_U16 i;
    const G3DS_Object *obj;
    if (model == (const G3DS_Model *)0 || out12 == (G3DS_Q16 *)0) { return G3DS_ERR_NULL; }
    if (object_index >= model->object_count) { return G3DS_ERR_BAD_INDEX; }
    obj = &model->objects[object_index];
    if (obj->has_matrix) {
        for (i = 0u; i < 12u; ++i) { out12[i] = obj->matrix[i]; }
    } else {
        for (i = 0u; i < 12u; ++i) { out12[i] = 0L; }
        out12[0] = G3DS_Q16_ONE;
        out12[4] = G3DS_Q16_ONE;
        out12[8] = G3DS_Q16_ONE;
    }
    return G3DS_OK;
}

int g3ds_get_chunk_audit(const G3DS_Model *model, G3DS_U16 index, G3DS_ChunkAudit *out_audit)
{
    if (model == (const G3DS_Model *)0 || out_audit == (G3DS_ChunkAudit *)0) { return G3DS_ERR_NULL; }
    if (index >= model->chunk_audit_count) { return G3DS_ERR_BAD_INDEX; }
    *out_audit = model->chunk_audit[index];
    return G3DS_OK;
}

const char *g3ds_result_string(int code)
{
    switch (code) {
    case G3DS_OK: return "ok";
    case G3DS_ERR_NULL: return "null argument";
    case G3DS_ERR_SHORT: return "input too short";
    case G3DS_ERR_BAD_CHUNK: return "bad chunk header or length";
    case G3DS_ERR_BAD_MAGIC: return "not a 3ds main chunk";
    case G3DS_ERR_CAPACITY_OBJECTS: return "object capacity exceeded";
    case G3DS_ERR_CAPACITY_VERTICES: return "vertex capacity exceeded";
    case G3DS_ERR_CAPACITY_FACES: return "face capacity exceeded";
    case G3DS_ERR_CAPACITY_MATERIALS: return "material capacity exceeded";
    case G3DS_ERR_BAD_INDEX: return "bad mesh index";
    case G3DS_ERR_TRUNCATED_STRING: return "truncated string";
    case G3DS_ERR_CAPACITY_LIGHTS: return "light capacity exceeded";
    case G3DS_ERR_CAPACITY_CAMERAS: return "camera capacity exceeded";
    case G3DS_ERR_CAPACITY_ANIM_NODES: return "animation node capacity exceeded";
    case G3DS_ERR_CAPACITY_KEYS: return "animation key capacity exceeded";
    case G3DS_ERR_RANGE: return "numeric range error";
    default: return "unknown error";
    }
}
