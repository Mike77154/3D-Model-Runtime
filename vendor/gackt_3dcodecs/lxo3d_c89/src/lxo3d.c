#include "lxo3d.h"

#define LXO3D_FORM LXO3D_ID('F','O','R','M')
#define LXO3D_LXOB LXO3D_ID('L','X','O','B')
#define LXO3D_LXPR LXO3D_ID('L','X','P','R')
#define LXO3D_LXPM LXO3D_ID('L','X','P','M')
#define LXO3D_LXPE LXO3D_ID('L','X','P','E')
#define LXO3D_VRSN LXO3D_ID('V','R','S','N')
#define LXO3D_TAGS LXO3D_ID('T','A','G','S')
#define LXO3D_LAYR LXO3D_ID('L','A','Y','R')
#define LXO3D_PNTS LXO3D_ID('P','N','T','S')
#define LXO3D_BBOX LXO3D_ID('B','B','O','X')
#define LXO3D_POLS LXO3D_ID('P','O','L','S')
#define LXO3D_PTAG LXO3D_ID('P','T','A','G')
#define LXO3D_VMAP LXO3D_ID('V','M','A','P')
#define LXO3D_VMAD LXO3D_ID('V','M','A','D')
#define LXO3D_VMED LXO3D_ID('V','M','E','D')
#define LXO3D_CHNM LXO3D_ID('C','H','N','M')
#define LXO3D_ITEM LXO3D_ID('I','T','E','M')
#define LXO3D_ENVL LXO3D_ID('E','N','V','L')
#define LXO3D_ACTN LXO3D_ID('A','C','T','N')
#define LXO3D_DATA LXO3D_ID('D','A','T','A')

#define LXO3D_SC_LINK LXO3D_ID('L','I','N','K')
#define LXO3D_SC_CLNK LXO3D_ID('C','L','N','K')
#define LXO3D_SC_CHNL LXO3D_ID('C','H','N','L')
#define LXO3D_SC_CHNS LXO3D_ID('C','H','N','S')
#define LXO3D_SC_CHAN LXO3D_ID('C','H','A','N')
#define LXO3D_SC_GRAD LXO3D_ID('G','R','A','D')
#define LXO3D_SC_VNAM LXO3D_ID('V','N','A','M')
#define LXO3D_SC_UNIQ LXO3D_ID('U','N','I','Q')
#define LXO3D_SC_LAYR LXO3D_ID('L','A','Y','R')
#define LXO3D_SC_BCHN LXO3D_ID('B','C','H','N')
#define LXO3D_SC_KEY_ LXO3D_ID('K','E','Y',' ')
#define LXO3D_SC_TANI LXO3D_ID('T','A','N','I')
#define LXO3D_SC_TANO LXO3D_ID('T','A','N','O')
#define LXO3D_SC_PRE_ LXO3D_ID('P','R','E',' ')
#define LXO3D_SC_POST LXO3D_ID('P','O','S','T')

#define LXO3D_MASK_POLY_COUNT 0x03FFu
#define LXO3D_FIXED_MAX 2147483647L
#define LXO3D_FIXED_MIN (-2147483647L - 1L)

static unsigned long lxo3d_be_u16(const unsigned char *p)
{
    unsigned long v;
    v = ((unsigned long)p[0] << 8) | (unsigned long)p[1];
    return v;
}

static long lxo3d_be_i16(const unsigned char *p)
{
    unsigned long u;
    u = lxo3d_be_u16(p);
    if (u & 0x8000UL) {
        return (long)(u - 0x10000UL);
    }
    return (long)u;
}

static unsigned long lxo3d_be_u32(const unsigned char *p)
{
    unsigned long v;
    v = ((unsigned long)p[0] << 24) |
        ((unsigned long)p[1] << 16) |
        ((unsigned long)p[2] << 8)  |
        (unsigned long)p[3];
    return v;
}

static void lxo3d_copy_id_text(unsigned long id, char out_text[5])
{
    out_text[0] = (char)((id >> 24) & 255UL);
    out_text[1] = (char)((id >> 16) & 255UL);
    out_text[2] = (char)((id >> 8) & 255UL);
    out_text[3] = (char)(id & 255UL);
    out_text[4] = '\0';
}

void lxo3d_id_to_text(unsigned long id, char out_text[5])
{
    lxo3d_copy_id_text(id, out_text);
}

static void lxo3d_zero_chars(char *dst, unsigned long count)
{
    unsigned long i;
    for (i = 0; i < count; ++i) {
        dst[i] = (char)0;
    }
}

static int lxo3d_id_has3(unsigned long id, char a, char b, char c)
{
    if (((char)((id >> 24) & 255UL)) != a) {
        return 0;
    }
    if (((char)((id >> 16) & 255UL)) != b) {
        return 0;
    }
    if (((char)((id >> 8) & 255UL)) != c) {
        return 0;
    }
    return 1;
}

static char lxo3d_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static int lxo3d_streq(const char *a, const char *b)
{
    unsigned long i;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    i = 0UL;
    while (a[i] != (char)0 && b[i] != (char)0) {
        if (a[i] != b[i]) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static int lxo3d_streqi(const char *a, const char *b)
{
    unsigned long i;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    i = 0UL;
    while (a[i] != (char)0 && b[i] != (char)0) {
        if (lxo3d_to_lower(a[i]) != lxo3d_to_lower(b[i])) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static int lxo3d_containsi(const char *hay, const char *needle)
{
    unsigned long i;
    unsigned long j;
    if (hay == (const char *)0 || needle == (const char *)0 || needle[0] == (char)0) {
        return 0;
    }
    i = 0UL;
    while (hay[i] != (char)0) {
        j = 0UL;
        while (hay[i + j] != (char)0 && needle[j] != (char)0 &&
               lxo3d_to_lower(hay[i + j]) == lxo3d_to_lower(needle[j])) {
            ++j;
        }
        if (needle[j] == (char)0) {
            return 1;
        }
        ++i;
    }
    return 0;
}

static void lxo3d_copy_cstr(char *dst, unsigned long dst_size, const char *src)
{
    unsigned long i;
    if (dst_size == 0UL) {
        return;
    }
    i = 0UL;
    if (src != (const char *)0) {
        while (src[i] != (char)0 && i + 1UL < dst_size) {
            dst[i] = src[i];
            ++i;
        }
    }
    dst[i] = (char)0;
}

static unsigned long lxo3d_item_kind_from_type(const char *type)
{
    if (lxo3d_streq(type, "mesh") || lxo3d_streqi(type, "mesh")) {
        return LXO3D_ITEM_KIND_MESH;
    }
    if (lxo3d_streqi(type, "camera")) {
        return LXO3D_ITEM_KIND_CAMERA;
    }
    if (lxo3d_containsi(type, "light")) {
        return LXO3D_ITEM_KIND_LIGHT;
    }
    if (lxo3d_containsi(type, "material")) {
        return LXO3D_ITEM_KIND_MATERIAL;
    }
    if (lxo3d_containsi(type, "shader") || lxo3d_containsi(type, "mask") ||
        lxo3d_containsi(type, "image") || lxo3d_containsi(type, "texture")) {
        return LXO3D_ITEM_KIND_SHADER;
    }
    if (lxo3d_containsi(type, "locator")) {
        return LXO3D_ITEM_KIND_LOCATOR;
    }
    if (lxo3d_containsi(type, "xfrm") || lxo3d_containsi(type, "transform")) {
        return LXO3D_ITEM_KIND_TRANSFORM;
    }
    if (lxo3d_containsi(type, "meshop") || lxo3d_containsi(type, "meshfusion") ||
        lxo3d_containsi(type, "fusion")) {
        return LXO3D_ITEM_KIND_MESHOP;
    }
    return LXO3D_ITEM_KIND_OTHER;
}

void lxo3d_clear(Lxo3D_Model *model)
{
    unsigned long i;
    unsigned long j;

    if (model == (Lxo3D_Model *)0) {
        return;
    }

    model->form_type = 0UL;
    model->version_major = 0UL;
    model->version_minor = 0UL;
    lxo3d_zero_chars(model->app_name, LXO3D_MAX_STRING);
    model->flags = 0UL;
    model->chunk_count = 0UL;
    model->unknown_chunk_count = 0UL;

    model->layer_count = 0UL;
    for (i = 0; i < LXO3D_MAX_LAYERS; ++i) {
        model->layers[i].legacy_index = 0UL;
        model->layers[i].flags = 0UL;
        model->layers[i].pivot.x = 0L;
        model->layers[i].pivot.y = 0L;
        model->layers[i].pivot.z = 0L;
        model->layers[i].scale_pivot.x = 0L;
        model->layers[i].scale_pivot.y = 0L;
        model->layers[i].scale_pivot.z = 0L;
        lxo3d_zero_chars(model->layers[i].name, LXO3D_MAX_STRING);
        model->layers[i].parent_index = 0L;
        model->layers[i].item_ref = 0UL;
        model->layers[i].first_point = 0UL;
        model->layers[i].point_count = 0UL;
        model->layers[i].first_poly = 0UL;
        model->layers[i].poly_count = 0UL;
    }

    model->point_count = 0UL;
    for (i = 0; i < LXO3D_MAX_POINTS; ++i) {
        model->points[i].x = 0L;
        model->points[i].y = 0L;
        model->points[i].z = 0L;
    }

    model->poly_count = 0UL;
    for (i = 0; i < LXO3D_MAX_POLYS; ++i) {
        model->polys[i].type_id = 0UL;
        model->polys[i].flags = 0UL;
        model->polys[i].layer_index = 0UL;
        model->polys[i].first_index = 0UL;
        model->polys[i].index_count = 0UL;
        model->polys[i].material_tag = 0xFFFFFFFFUL;
        model->polys[i].part_tag = 0xFFFFFFFFUL;
        model->polys[i].smoothing_tag = 0xFFFFFFFFUL;
    }
    model->poly_index_count = 0UL;
    for (i = 0; i < LXO3D_MAX_POLY_INDEXES; ++i) {
        model->poly_indices[i] = 0UL;
    }

    model->tag_count = 0UL;
    for (i = 0; i < LXO3D_MAX_TAGS; ++i) {
        for (j = 0; j < LXO3D_MAX_STRING; ++j) {
            model->tags[i][j] = (char)0;
        }
    }

    model->map_count = 0UL;
    for (i = 0; i < LXO3D_MAX_MAPS; ++i) {
        model->maps[i].kind = 0UL;
        model->maps[i].type_id = 0UL;
        model->maps[i].dimension = 0UL;
        lxo3d_zero_chars(model->maps[i].name, LXO3D_MAX_STRING);
        model->maps[i].first_value = 0UL;
        model->maps[i].value_count = 0UL;
    }

    model->vmap_value_count = 0UL;
    for (i = 0; i < LXO3D_MAX_VMAP_VALUES; ++i) {
        model->vmap_values[i].map_index = 0UL;
        model->vmap_values[i].point_index = 0UL;
        for (j = 0; j < 6UL; ++j) {
            model->vmap_values[i].value[j] = 0L;
        }
    }

    model->vmad_value_count = 0UL;
    for (i = 0; i < LXO3D_MAX_VMAD_VALUES; ++i) {
        model->vmad_values[i].map_index = 0UL;
        model->vmad_values[i].point_index = 0UL;
        model->vmad_values[i].poly_index = 0UL;
        for (j = 0; j < 6UL; ++j) {
            model->vmad_values[i].value[j] = 0L;
        }
    }

    model->channel_name_count = 0UL;
    for (i = 0; i < LXO3D_MAX_CHANNEL_NAMES; ++i) {
        lxo3d_zero_chars(model->channel_names[i], LXO3D_MAX_STRING);
    }

    model->item_count = 0UL;
    for (i = 0; i < LXO3D_MAX_ITEMS; ++i) {
        model->items[i].kind = 0UL;
        model->items[i].ref_id = 0UL;
        lxo3d_zero_chars(model->items[i].type, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->items[i].name, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->items[i].visible_name, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->items[i].unique_name, LXO3D_MAX_STRING);
        model->items[i].layer_flags = 0UL;
        model->items[i].rgba[0] = 0U;
        model->items[i].rgba[1] = 0U;
        model->items[i].rgba[2] = 0U;
        model->items[i].rgba[3] = 0U;
        model->items[i].first_link = 0UL;
        model->items[i].link_count = 0UL;
        model->items[i].first_channel = 0UL;
        model->items[i].channel_count = 0UL;
        model->items[i].first_bchn = 0UL;
        model->items[i].bchn_count = 0UL;
        model->items[i].parent_ref = 0UL;
        model->items[i].parent_link_index = 0L;
    }

    model->link_count = 0UL;
    for (i = 0; i < LXO3D_MAX_LINKS; ++i) {
        model->links[i].item_index = 0UL;
        lxo3d_zero_chars(model->links[i].graph, LXO3D_MAX_STRING);
        model->links[i].target_ref = 0UL;
        model->links[i].link_index = 0L;
    }

    model->channel_link_count = 0UL;
    for (i = 0; i < LXO3D_MAX_CHANNEL_LINKS; ++i) {
        model->channel_links[i].item_index = 0UL;
        lxo3d_zero_chars(model->channel_links[i].graph, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->channel_links[i].from_channel, LXO3D_MAX_STRING);
        model->channel_links[i].from_ref = 0UL;
        lxo3d_zero_chars(model->channel_links[i].to_channel, LXO3D_MAX_STRING);
        model->channel_links[i].from_link_index = 0L;
        model->channel_links[i].to_link_index = 0L;
    }

    model->channel_count = 0UL;
    for (i = 0; i < LXO3D_MAX_CHANNELS; ++i) {
        model->channels[i].item_index = 0xFFFFFFFFUL;
        model->channels[i].action_index = 0xFFFFFFFFUL;
        model->channels[i].channel_name_index = 0xFFFFFFFFUL;
        model->channels[i].type = 0UL;
        model->channels[i].envelope_index = 0xFFFFFFFFUL;
        lxo3d_zero_chars(model->channels[i].name, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->channels[i].string_value, LXO3D_MAX_STRING);
        model->channels[i].int_value = 0L;
        model->channels[i].fixed_value = 0L;
    }

    model->envelope_count = 0UL;
    for (i = 0; i < LXO3D_MAX_ENVELOPES; ++i) {
        model->envelopes[i].index = 0UL;
        model->envelopes[i].type = 0UL;
        model->envelopes[i].pre_behavior = 0UL;
        model->envelopes[i].post_behavior = 0UL;
        model->envelopes[i].first_key = 0UL;
        model->envelopes[i].key_count = 0UL;
    }
    model->env_key_count = 0UL;
    for (i = 0; i < LXO3D_MAX_ENV_KEYS; ++i) {
        model->env_keys[i].envelope_array_index = 0UL;
        model->env_keys[i].input = 0L;
        model->env_keys[i].value = 0L;
        model->env_keys[i].flags = 0UL;
        model->env_keys[i].tani_type = 0UL;
        model->env_keys[i].tano_type = 0UL;
        model->env_keys[i].tani_weight = 0L;
        model->env_keys[i].tani_value = 0L;
        model->env_keys[i].tano_weight = 0L;
        model->env_keys[i].tano_value = 0L;
    }

    model->action_count = 0UL;
    for (i = 0; i < LXO3D_MAX_ACTIONS; ++i) {
        lxo3d_zero_chars(model->actions[i].name, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->actions[i].type, LXO3D_MAX_STRING);
        model->actions[i].first_channel = 0UL;
        model->actions[i].channel_count = 0UL;
    }

    model->bchn_count = 0UL;
    for (i = 0; i < LXO3D_MAX_BCHN; ++i) {
        model->bchannels[i].item_index = 0UL;
        lxo3d_zero_chars(model->bchannels[i].operation_type, LXO3D_MAX_STRING);
        model->bchannels[i].data_index = 0UL;
    }

    model->data_count = 0UL;
    for (i = 0; i < LXO3D_MAX_DATA; ++i) {
        model->datas[i].index = 0UL;
        model->datas[i].unknown = 0UL;
        lxo3d_zero_chars(model->datas[i].exotype, LXO3D_MAX_STRING);
        model->datas[i].first_pair = 0UL;
        model->datas[i].pair_count = 0UL;
    }
    model->data_pair_count = 0UL;
    for (i = 0; i < LXO3D_MAX_DATA_PAIRS; ++i) {
        model->data_pairs[i].data_array_index = 0UL;
        lxo3d_zero_chars(model->data_pairs[i].name, LXO3D_MAX_STRING);
        lxo3d_zero_chars(model->data_pairs[i].value, LXO3D_MAX_STRING);
    }
}

static unsigned long lxo3d_min_ul(unsigned long a, unsigned long b)
{
    if (a < b) {
        return a;
    }
    return b;
}

static int lxo3d_range_ok(unsigned long pos, unsigned long need, unsigned long end)
{
    if (pos > end) {
        return 0;
    }
    if (need > end - pos) {
        return 0;
    }
    return 1;
}

static unsigned long lxo3d_next_even(unsigned long n)
{
    if ((n & 1UL) != 0UL) {
        return n + 1UL;
    }
    return n;
}

static Lxo3D_Fixed lxo3d_f4_to_fixed(unsigned long bits, unsigned long *sat_flag)
{
    unsigned long sign;
    unsigned long exp;
    unsigned long frac;
    unsigned long mant;
    unsigned long mag;
    int shift;

    sign = (bits >> 31) & 1UL;
    exp = (bits >> 23) & 255UL;
    frac = bits & 0x7FFFFFUL;

    if (exp == 0UL) {
        return 0L;
    }

    if (exp == 255UL) {
        if (sat_flag != (unsigned long *)0) {
            *sat_flag = 1UL;
        }
        if (sign != 0UL) {
            return LXO3D_FIXED_MIN;
        }
        return LXO3D_FIXED_MAX;
    }

    mant = 0x800000UL | frac;
    shift = (int)exp - 134;

    if (shift >= 0) {
        if (shift > 7) {
            if (sat_flag != (unsigned long *)0) {
                *sat_flag = 1UL;
            }
            if (sign != 0UL) {
                return LXO3D_FIXED_MIN;
            }
            return LXO3D_FIXED_MAX;
        }
        mag = mant << shift;
        if (mag > 2147483647UL) {
            if (sat_flag != (unsigned long *)0) {
                *sat_flag = 1UL;
            }
            if (sign != 0UL) {
                return LXO3D_FIXED_MIN;
            }
            return LXO3D_FIXED_MAX;
        }
    } else {
        shift = -shift;
        if (shift >= 31) {
            mag = 0UL;
        } else {
            mag = mant >> shift;
        }
    }

    if (sign != 0UL) {
        return -(long)mag;
    }
    return (long)mag;
}

static unsigned long lxo3d_read_vx(const unsigned char *data, unsigned long end, unsigned long *pos, int *ok)
{
    unsigned long first;
    unsigned long v;

    if (*ok == 0) {
        return 0UL;
    }
    if (!lxo3d_range_ok(*pos, 2UL, end)) {
        *ok = 0;
        return 0UL;
    }
    first = lxo3d_be_u16(data + *pos);
    if (first < 0xFF00UL) {
        *pos += 2UL;
        return first;
    }
    if (!lxo3d_range_ok(*pos, 4UL, end)) {
        *ok = 0;
        return 0UL;
    }
    v = lxo3d_be_u32(data + *pos) & 0x00FFFFFFUL;
    *pos += 4UL;
    return v;
}

static void lxo3d_copy_s0(char *dst,
                          unsigned long dst_size,
                          const unsigned char *data,
                          unsigned long start,
                          unsigned long end,
                          unsigned long *next_pos)
{
    unsigned long p;
    unsigned long n;

    if (dst_size == 0UL) {
        return;
    }
    dst[0] = (char)0;
    p = start;
    n = 0UL;
    while (p < end && data[p] != (unsigned char)0) {
        if (n + 1UL < dst_size) {
            dst[n] = (char)data[p];
            ++n;
        }
        ++p;
    }
    if (n < dst_size) {
        dst[n] = (char)0;
    } else {
        dst[dst_size - 1UL] = (char)0;
    }
    if (p < end && data[p] == (unsigned char)0) {
        ++p;
    }
    p = lxo3d_next_even(p - start) + start;
    if (p > end) {
        p = end;
    }
    if (next_pos != (unsigned long *)0) {
        *next_pos = p;
    }
}

static int lxo3d_parse_vrsn(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;

    if (!lxo3d_range_ok(start, 8UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    model->version_major = lxo3d_be_u32(data + start);
    model->version_minor = lxo3d_be_u32(data + start + 4UL);
    pos = start + 8UL;
    if (pos < end) {
        lxo3d_copy_s0(model->app_name, LXO3D_MAX_STRING, data, pos, end, &pos);
    }
    return LXO3D_OK;
}

static int lxo3d_parse_tags(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long next;

    pos = start;
    while (pos < end) {
        if (model->tag_count >= LXO3D_MAX_TAGS) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            return LXO3D_OK;
        }
        lxo3d_copy_s0(model->tags[model->tag_count], LXO3D_MAX_STRING, data, pos, end, &next);
        if (next <= pos) {
            return LXO3D_E_BAD_CHUNK;
        }
        ++model->tag_count;
        pos = next;
    }
    return LXO3D_OK;
}

static int lxo3d_parse_layr(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    Lxo3D_Layer *ly;
    unsigned long pos;
    unsigned long sat;

    if (model->layer_count >= LXO3D_MAX_LAYERS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    if (!lxo3d_range_ok(start, 16UL, end)) {
        return LXO3D_E_TRUNCATED;
    }

    ly = &model->layers[model->layer_count];
    ly->legacy_index = lxo3d_be_u16(data + start);
    ly->flags = lxo3d_be_u16(data + start + 2UL);
    sat = 0UL;
    ly->pivot.x = lxo3d_f4_to_fixed(lxo3d_be_u32(data + start + 4UL), &sat);
    ly->pivot.y = lxo3d_f4_to_fixed(lxo3d_be_u32(data + start + 8UL), &sat);
    ly->pivot.z = lxo3d_f4_to_fixed(lxo3d_be_u32(data + start + 12UL), &sat);
    if (sat != 0UL) {
        model->flags |= LXO3D_FLAG_FIXED_SATURATED;
    }

    pos = start + 16UL;
    lxo3d_copy_s0(ly->name, LXO3D_MAX_STRING, data, pos, end, &pos);

    if (lxo3d_range_ok(pos, 2UL, end)) {
        ly->parent_index = lxo3d_be_i16(data + pos);
        pos += 2UL;
    }
    if (lxo3d_range_ok(pos, 4UL, end)) {
        pos += 4UL;
    }
    if (lxo3d_range_ok(pos, 4UL, end)) {
        pos += 4UL;
    }
    if (lxo3d_range_ok(pos, 12UL, end)) {
        sat = 0UL;
        ly->scale_pivot.x = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
        ly->scale_pivot.y = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos + 4UL), &sat);
        ly->scale_pivot.z = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos + 8UL), &sat);
        if (sat != 0UL) {
            model->flags |= LXO3D_FLAG_FIXED_SATURATED;
        }
        pos += 12UL;
    }
    if (lxo3d_range_ok(pos, 24UL, end)) {
        pos += 24UL;
    }
    if (lxo3d_range_ok(pos, 4UL, end)) {
        ly->item_ref = lxo3d_be_u32(data + pos);
    }

    ly->first_point = model->point_count;
    ly->first_poly = model->poly_count;
    ++model->layer_count;
    return LXO3D_OK;
}

static int lxo3d_current_layer_index(const Lxo3D_Model *model, unsigned long *out_index)
{
    if (model->layer_count == 0UL) {
        *out_index = 0UL;
        return 0;
    }
    *out_index = model->layer_count - 1UL;
    return 1;
}

static int lxo3d_parse_pnts(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long count;
    unsigned long i;
    unsigned long layer_index;
    unsigned long sat;

    if (((end - start) % 12UL) != 0UL) {
        return LXO3D_E_BAD_CHUNK;
    }
    count = (end - start) / 12UL;
    if (!lxo3d_current_layer_index(model, &layer_index)) {
        model->flags |= LXO3D_FLAG_SCENE_ONLY;
    } else {
        model->layers[layer_index].first_point = model->point_count;
    }

    pos = start;
    i = 0UL;
    while (i < count) {
        if (model->point_count >= LXO3D_MAX_POINTS) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            return LXO3D_OK;
        }
        sat = 0UL;
        model->points[model->point_count].x = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
        model->points[model->point_count].y = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos + 4UL), &sat);
        model->points[model->point_count].z = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos + 8UL), &sat);
        if (sat != 0UL) {
            model->flags |= LXO3D_FLAG_FIXED_SATURATED;
        }
        ++model->point_count;
        pos += 12UL;
        ++i;
    }

    if (model->layer_count != 0UL) {
        model->layers[layer_index].point_count = model->point_count - model->layers[layer_index].first_point;
    }
    return LXO3D_OK;
}

static int lxo3d_store_poly_index(Lxo3D_Model *model, unsigned long v)
{
    if (model->poly_index_count >= LXO3D_MAX_POLY_INDEXES) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return 0;
    }
    if (v >= model->point_count) {
        model->flags |= LXO3D_FLAG_BAD_INDEX;
    }
    model->poly_indices[model->poly_index_count] = v;
    ++model->poly_index_count;
    return 1;
}

static int lxo3d_parse_pols(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long type_id;
    unsigned long raw_count;
    unsigned long count;
    unsigned long flags;
    unsigned long i;
    unsigned long poly_index;
    unsigned long layer_index;
    int ok;

    if (!lxo3d_range_ok(start, 4UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    type_id = lxo3d_be_u32(data + start);
    pos = start + 4UL;
    ok = 1;
    if (!lxo3d_current_layer_index(model, &layer_index)) {
        layer_index = 0UL;
        model->flags |= LXO3D_FLAG_SCENE_ONLY;
    } else {
        model->layers[layer_index].first_poly = model->poly_count;
    }

    while (pos < end) {
        if (!lxo3d_range_ok(pos, 2UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        raw_count = lxo3d_be_u16(data + pos);
        pos += 2UL;
        count = raw_count & LXO3D_MASK_POLY_COUNT;
        flags = raw_count & (~(unsigned long)LXO3D_MASK_POLY_COUNT);

        if (model->poly_count >= LXO3D_MAX_POLYS) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            return LXO3D_OK;
        }

        poly_index = model->poly_count;
        model->polys[poly_index].type_id = type_id;
        model->polys[poly_index].flags = flags;
        model->polys[poly_index].layer_index = layer_index;
        model->polys[poly_index].first_index = model->poly_index_count;
        model->polys[poly_index].index_count = count;
        model->polys[poly_index].material_tag = 0xFFFFFFFFUL;
        model->polys[poly_index].part_tag = 0xFFFFFFFFUL;
        model->polys[poly_index].smoothing_tag = 0xFFFFFFFFUL;

        i = 0UL;
        while (i < count) {
            lxo3d_store_poly_index(model, lxo3d_read_vx(data, end, &pos, &ok));
            if (!ok) {
                return LXO3D_E_TRUNCATED;
            }
            ++i;
        }
        ++model->poly_count;
    }

    if (model->layer_count != 0UL) {
        model->layers[layer_index].poly_count = model->poly_count - model->layers[layer_index].first_poly;
    }
    return LXO3D_OK;
}

static int lxo3d_parse_ptag(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long tag_type;
    unsigned long poly_index;
    unsigned long tag_index;
    int ok;

    if (!lxo3d_range_ok(start, 4UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    tag_type = lxo3d_be_u32(data + start);
    pos = start + 4UL;
    ok = 1;

    while (pos < end) {
        poly_index = lxo3d_read_vx(data, end, &pos, &ok);
        if (!ok) {
            return LXO3D_E_TRUNCATED;
        }
        if (!lxo3d_range_ok(pos, 2UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        tag_index = lxo3d_be_u16(data + pos);
        pos += 2UL;
        if (poly_index >= model->poly_count) {
            model->flags |= LXO3D_FLAG_BAD_INDEX;
        } else {
            if (tag_type == LXO3D_ID_MATR) {
                model->polys[poly_index].material_tag = tag_index;
            } else if (tag_type == LXO3D_ID_PART) {
                model->polys[poly_index].part_tag = tag_index;
            } else if (tag_type == LXO3D_ID_SMGP) {
                model->polys[poly_index].smoothing_tag = tag_index;
            }
        }
    }
    return LXO3D_OK;
}

static int lxo3d_parse_vmap(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long next;
    unsigned long map_index;
    unsigned long dim;
    unsigned long dim_file;
    unsigned long i;
    unsigned long sat;
    int ok;

    if (!lxo3d_range_ok(start, 6UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    if (model->map_count >= LXO3D_MAX_MAPS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }

    map_index = model->map_count;
    model->maps[map_index].kind = LXO3D_MAP_KIND_VMAP;
    model->maps[map_index].type_id = lxo3d_be_u32(data + start);
    dim_file = lxo3d_be_u16(data + start + 4UL);
    dim = dim_file;
    if (dim > 6UL) {
        dim = 6UL;
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
    }
    model->maps[map_index].dimension = dim;
    pos = start + 6UL;
    lxo3d_copy_s0(model->maps[map_index].name, LXO3D_MAX_STRING, data, pos, end, &next);
    pos = next;
    model->maps[map_index].first_value = model->vmap_value_count;
    ok = 1;

    while (pos < end) {
        if (model->vmap_value_count >= LXO3D_MAX_VMAP_VALUES) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            break;
        }
        model->vmap_values[model->vmap_value_count].map_index = map_index;
        model->vmap_values[model->vmap_value_count].point_index = lxo3d_read_vx(data, end, &pos, &ok);
        if (!ok) {
            return LXO3D_E_TRUNCATED;
        }
        if (model->vmap_values[model->vmap_value_count].point_index >= model->point_count) {
            model->flags |= LXO3D_FLAG_BAD_INDEX;
        }
        i = 0UL;
        while (i < dim_file) {
            if (!lxo3d_range_ok(pos, 4UL, end)) {
                return LXO3D_E_TRUNCATED;
            }
            sat = 0UL;
            if (i < 6UL) {
                model->vmap_values[model->vmap_value_count].value[i] = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
            } else {
                (void)lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
            }
            if (sat != 0UL) {
                model->flags |= LXO3D_FLAG_FIXED_SATURATED;
            }
            pos += 4UL;
            ++i;
        }
        while (i < 6UL) {
            model->vmap_values[model->vmap_value_count].value[i] = 0L;
            ++i;
        }
        ++model->vmap_value_count;
    }

    model->maps[map_index].value_count = model->vmap_value_count - model->maps[map_index].first_value;
    ++model->map_count;
    return LXO3D_OK;
}

static int lxo3d_parse_vmad(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long next;
    unsigned long map_index;
    unsigned long dim;
    unsigned long dim_file;
    unsigned long i;
    unsigned long sat;
    int ok;

    if (!lxo3d_range_ok(start, 6UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    if (model->map_count >= LXO3D_MAX_MAPS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }

    map_index = model->map_count;
    model->maps[map_index].kind = LXO3D_MAP_KIND_VMAD;
    model->maps[map_index].type_id = lxo3d_be_u32(data + start);
    dim_file = lxo3d_be_u16(data + start + 4UL);
    dim = dim_file;
    if (dim > 6UL) {
        dim = 6UL;
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
    }
    model->maps[map_index].dimension = dim;
    pos = start + 6UL;
    lxo3d_copy_s0(model->maps[map_index].name, LXO3D_MAX_STRING, data, pos, end, &next);
    pos = next;
    model->maps[map_index].first_value = model->vmad_value_count;
    ok = 1;

    while (pos < end) {
        if (model->vmad_value_count >= LXO3D_MAX_VMAD_VALUES) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            break;
        }
        model->vmad_values[model->vmad_value_count].map_index = map_index;
        model->vmad_values[model->vmad_value_count].point_index = lxo3d_read_vx(data, end, &pos, &ok);
        model->vmad_values[model->vmad_value_count].poly_index = lxo3d_read_vx(data, end, &pos, &ok);
        if (!ok) {
            return LXO3D_E_TRUNCATED;
        }
        if (model->vmad_values[model->vmad_value_count].point_index >= model->point_count ||
            model->vmad_values[model->vmad_value_count].poly_index >= model->poly_count) {
            model->flags |= LXO3D_FLAG_BAD_INDEX;
        }
        i = 0UL;
        while (i < dim_file) {
            if (!lxo3d_range_ok(pos, 4UL, end)) {
                return LXO3D_E_TRUNCATED;
            }
            sat = 0UL;
            if (i < 6UL) {
                model->vmad_values[model->vmad_value_count].value[i] = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
            } else {
                (void)lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
            }
            if (sat != 0UL) {
                model->flags |= LXO3D_FLAG_FIXED_SATURATED;
            }
            pos += 4UL;
            ++i;
        }
        while (i < 6UL) {
            model->vmad_values[model->vmad_value_count].value[i] = 0L;
            ++i;
        }
        ++model->vmad_value_count;
    }

    model->maps[map_index].value_count = model->vmad_value_count - model->maps[map_index].first_value;
    ++model->map_count;
    return LXO3D_OK;
}

static int lxo3d_parse_chnm(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long next;

    pos = start;
    while (pos < end) {
        if (model->channel_name_count >= LXO3D_MAX_CHANNEL_NAMES) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            return LXO3D_OK;
        }
        lxo3d_copy_s0(model->channel_names[model->channel_name_count], LXO3D_MAX_STRING, data, pos, end, &next);
        if (next <= pos) {
            return LXO3D_E_BAD_CHUNK;
        }
        ++model->channel_name_count;
        pos = next;
    }
    model->flags |= LXO3D_FLAG_SCENE_META;
    return LXO3D_OK;
}

int lxo3d_find_item_by_ref(const Lxo3D_Model *model, unsigned long ref_id, unsigned long *out_index)
{
    unsigned long i;
    if (model == (const Lxo3D_Model *)0) {
        return 0;
    }
    for (i = 0UL; i < model->item_count; ++i) {
        if (model->items[i].ref_id == ref_id) {
            if (out_index != (unsigned long *)0) {
                *out_index = i;
            }
            return 1;
        }
    }
    return 0;
}

static Lxo3D_Channel *lxo3d_new_channel(Lxo3D_Model *model,
                                        unsigned long item_index,
                                        unsigned long action_index)
{
    Lxo3D_Channel *ch;
    if (model->channel_count >= LXO3D_MAX_CHANNELS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return (Lxo3D_Channel *)0;
    }
    ch = &model->channels[model->channel_count];
    ch->item_index = item_index;
    ch->action_index = action_index;
    ch->channel_name_index = 0xFFFFFFFFUL;
    ch->type = 0UL;
    ch->envelope_index = 0xFFFFFFFFUL;
    lxo3d_zero_chars(ch->name, LXO3D_MAX_STRING);
    lxo3d_zero_chars(ch->string_value, LXO3D_MAX_STRING);
    ch->int_value = 0L;
    ch->fixed_value = 0L;
    ++model->channel_count;
    return ch;
}

static int lxo3d_parse_channel_value(const unsigned char *data,
                                      unsigned long start,
                                      unsigned long end,
                                      unsigned long type,
                                      Lxo3D_Channel *ch,
                                      unsigned long *next_pos,
                                      Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long base_type;
    unsigned long sat;

    pos = start;
    base_type = type & 0x0FUL;
    ch->type = type;
    if ((type & LXO3D_CHAN_ENVELOPE) != 0UL) {
        model->flags |= LXO3D_FLAG_ANIM_META;
    }

    if (base_type == LXO3D_CHAN_INT) {
        if (!lxo3d_range_ok(pos, 4UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        ch->int_value = (long)lxo3d_be_u32(data + pos);
        pos += 4UL;
    } else if (base_type == LXO3D_CHAN_FLOAT) {
        if (!lxo3d_range_ok(pos, 4UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        sat = 0UL;
        ch->fixed_value = lxo3d_f4_to_fixed(lxo3d_be_u32(data + pos), &sat);
        if (sat != 0UL) {
            model->flags |= LXO3D_FLAG_FIXED_SATURATED;
        }
        pos += 4UL;
    } else if (base_type == LXO3D_CHAN_STRING) {
        lxo3d_copy_s0(ch->string_value, LXO3D_MAX_STRING, data, pos, end, &pos);
    } else {
        pos = end;
    }

    if (next_pos != (unsigned long *)0) {
        *next_pos = pos;
    }
    return LXO3D_OK;
}

static int lxo3d_store_link(Lxo3D_Model *model,
                            unsigned long item_index,
                            const char *graph,
                            unsigned long target_ref,
                            long link_index)
{
    Lxo3D_Link *ln;
    Lxo3D_Item *it;
    if (model->link_count >= LXO3D_MAX_LINKS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    ln = &model->links[model->link_count];
    ln->item_index = item_index;
    lxo3d_copy_cstr(ln->graph, LXO3D_MAX_STRING, graph);
    ln->target_ref = target_ref;
    ln->link_index = link_index;
    if (item_index < model->item_count) {
        it = &model->items[item_index];
        if (lxo3d_streqi(graph, "parent")) {
            it->parent_ref = target_ref;
            it->parent_link_index = link_index;
        }
    }
    ++model->link_count;
    return LXO3D_OK;
}

static int lxo3d_store_channel_link(Lxo3D_Model *model,
                                       unsigned long item_index,
                                       const char *graph,
                                       const char *from_channel,
                                       unsigned long from_ref,
                                       const char *to_channel,
                                       long from_link_index,
                                       long to_link_index)
{
    Lxo3D_ChannelLink *ln;
    if (model->channel_link_count >= LXO3D_MAX_CHANNEL_LINKS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    ln = &model->channel_links[model->channel_link_count];
    ln->item_index = item_index;
    lxo3d_copy_cstr(ln->graph, LXO3D_MAX_STRING, graph);
    lxo3d_copy_cstr(ln->from_channel, LXO3D_MAX_STRING, from_channel);
    ln->from_ref = from_ref;
    lxo3d_copy_cstr(ln->to_channel, LXO3D_MAX_STRING, to_channel);
    ln->from_link_index = from_link_index;
    ln->to_link_index = to_link_index;
    ++model->channel_link_count;
    model->flags |= LXO3D_FLAG_CONSTRAINT_META;
    return LXO3D_OK;
}

static int lxo3d_parse_item_chan_by_name(const unsigned char *data,
                                          unsigned long start,
                                          unsigned long end,
                                          Lxo3D_Model *model,
                                          unsigned long item_index,
                                          unsigned long action_index,
                                          int is_string_only)
{
    unsigned long pos;
    unsigned long type;
    Lxo3D_Channel *ch;

    ch = lxo3d_new_channel(model, item_index, action_index);
    if (ch == (Lxo3D_Channel *)0) {
        return LXO3D_OK;
    }
    pos = start;
    lxo3d_copy_s0(ch->name, LXO3D_MAX_STRING, data, pos, end, &pos);
    if (is_string_only != 0) {
        ch->type = LXO3D_CHAN_STRING;
        lxo3d_copy_s0(ch->string_value, LXO3D_MAX_STRING, data, pos, end, &pos);
        return LXO3D_OK;
    }
    if (!lxo3d_range_ok(pos, 2UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    type = lxo3d_be_u16(data + pos);
    pos += 2UL;
    return lxo3d_parse_channel_value(data, pos, end, type, ch, &pos, model);
}

static int lxo3d_parse_item_chan_by_index(const unsigned char *data,
                                           unsigned long start,
                                           unsigned long end,
                                           Lxo3D_Model *model,
                                           unsigned long item_index,
                                           unsigned long action_index,
                                           int has_env_index)
{
    unsigned long pos;
    unsigned long ch_index;
    unsigned long type;
    int ok;
    Lxo3D_Channel *ch;

    ch = lxo3d_new_channel(model, item_index, action_index);
    if (ch == (Lxo3D_Channel *)0) {
        return LXO3D_OK;
    }
    pos = start;
    ok = 1;
    ch_index = lxo3d_read_vx(data, end, &pos, &ok);
    if (!ok) {
        return LXO3D_E_TRUNCATED;
    }
    ch->channel_name_index = ch_index;
    if (ch_index < model->channel_name_count) {
        lxo3d_copy_cstr(ch->name, LXO3D_MAX_STRING, model->channel_names[ch_index]);
    }
    if (!lxo3d_range_ok(pos, 2UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    type = lxo3d_be_u16(data + pos);
    pos += 2UL;
    if (has_env_index != 0) {
        ch->envelope_index = lxo3d_read_vx(data, end, &pos, &ok);
        if (!ok) {
            return LXO3D_E_TRUNCATED;
        }
        model->flags |= LXO3D_FLAG_ANIM_META;
    }
    return lxo3d_parse_channel_value(data, pos, end, type, ch, &pos, model);
}

static int lxo3d_parse_grad_by_name(const unsigned char *data,
                                     unsigned long start,
                                     unsigned long end,
                                     Lxo3D_Model *model,
                                     unsigned long item_index,
                                     unsigned long action_index)
{
    unsigned long pos;
    int ok;
    Lxo3D_Channel *ch;

    ch = lxo3d_new_channel(model, item_index, action_index);
    if (ch == (Lxo3D_Channel *)0) {
        return LXO3D_OK;
    }
    pos = start;
    lxo3d_copy_s0(ch->name, LXO3D_MAX_STRING, data, pos, end, &pos);
    ok = 1;
    ch->type = LXO3D_CHAN_ENVELOPE | LXO3D_CHAN_FLOAT;
    ch->envelope_index = lxo3d_read_vx(data, end, &pos, &ok);
    if (!ok) {
        return LXO3D_E_TRUNCATED;
    }
    if (lxo3d_range_ok(pos, 4UL, end)) {
        ch->int_value = (long)lxo3d_be_u32(data + pos);
    }
    model->flags |= LXO3D_FLAG_ANIM_META;
    return LXO3D_OK;
}

static int lxo3d_parse_grad_by_index(const unsigned char *data,
                                      unsigned long start,
                                      unsigned long end,
                                      Lxo3D_Model *model,
                                      unsigned long item_index,
                                      unsigned long action_index)
{
    unsigned long pos;
    unsigned long ch_index;
    int ok;
    Lxo3D_Channel *ch;

    ch = lxo3d_new_channel(model, item_index, action_index);
    if (ch == (Lxo3D_Channel *)0) {
        return LXO3D_OK;
    }
    pos = start;
    ok = 1;
    ch_index = lxo3d_read_vx(data, end, &pos, &ok);
    if (!ok) {
        return LXO3D_E_TRUNCATED;
    }
    ch->channel_name_index = ch_index;
    if (ch_index < model->channel_name_count) {
        lxo3d_copy_cstr(ch->name, LXO3D_MAX_STRING, model->channel_names[ch_index]);
    }
    ch->type = LXO3D_CHAN_ENVELOPE | LXO3D_CHAN_FLOAT;
    ch->envelope_index = lxo3d_read_vx(data, end, &pos, &ok);
    if (!ok) {
        return LXO3D_E_TRUNCATED;
    }
    if (lxo3d_range_ok(pos, 4UL, end)) {
        ch->int_value = (long)lxo3d_be_u32(data + pos);
    }
    model->flags |= LXO3D_FLAG_ANIM_META;
    return LXO3D_OK;
}

static int lxo3d_parse_item(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long item_index;
    unsigned long sub_id;
    unsigned long sub_size;
    unsigned long sub_start;
    unsigned long sub_end;
    unsigned long tmp_pos;
    char graph[LXO3D_MAX_STRING];
    char op[LXO3D_MAX_STRING];
    char from_channel[LXO3D_MAX_STRING];
    char to_channel[LXO3D_MAX_STRING];
    unsigned long target_ref;
    unsigned long from_ref;
    long link_index;
    long from_link_index;
    long to_link_index;
    Lxo3D_Item *it;
    int r;

    if (model->item_count >= LXO3D_MAX_ITEMS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }

    item_index = model->item_count;
    it = &model->items[item_index];
    pos = start;
    lxo3d_copy_s0(it->type, LXO3D_MAX_STRING, data, pos, end, &pos);
    lxo3d_copy_s0(it->name, LXO3D_MAX_STRING, data, pos, end, &pos);
    if (!lxo3d_range_ok(pos, 4UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    it->ref_id = lxo3d_be_u32(data + pos);
    pos += 4UL;
    it->kind = lxo3d_item_kind_from_type(it->type);
    it->first_link = model->link_count;
    it->first_channel = model->channel_count;
    it->first_bchn = model->bchn_count;
    it->parent_ref = 0UL;
    it->parent_link_index = 0L;
    ++model->item_count;

    while (pos < end) {
        if (!lxo3d_range_ok(pos, 6UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        sub_id = lxo3d_be_u32(data + pos);
        sub_size = lxo3d_be_u16(data + pos + 4UL);
        sub_start = pos + 6UL;
        if (sub_size > end - sub_start) {
            return LXO3D_E_TRUNCATED;
        }
        sub_end = sub_start + sub_size;
        r = LXO3D_OK;

        if (sub_id == LXO3D_SC_LINK) {
            tmp_pos = sub_start;
            lxo3d_copy_s0(graph, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
            if (!lxo3d_range_ok(tmp_pos, 8UL, sub_end)) {
                return LXO3D_E_TRUNCATED;
            }
            target_ref = lxo3d_be_u32(data + tmp_pos);
            link_index = (long)lxo3d_be_u32(data + tmp_pos + 4UL);
            r = lxo3d_store_link(model, item_index, graph, target_ref, link_index);
        } else if (sub_id == LXO3D_SC_CLNK) {
            tmp_pos = sub_start;
            lxo3d_copy_s0(graph, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
            lxo3d_copy_s0(from_channel, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
            if (!lxo3d_range_ok(tmp_pos, 4UL, sub_end)) {
                return LXO3D_E_TRUNCATED;
            }
            from_ref = lxo3d_be_u32(data + tmp_pos);
            tmp_pos += 4UL;
            lxo3d_copy_s0(to_channel, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
            if (!lxo3d_range_ok(tmp_pos, 8UL, sub_end)) {
                return LXO3D_E_TRUNCATED;
            }
            from_link_index = (long)lxo3d_be_u32(data + tmp_pos);
            to_link_index = (long)lxo3d_be_u32(data + tmp_pos + 4UL);
            r = lxo3d_store_channel_link(model, item_index, graph, from_channel, from_ref, to_channel, from_link_index, to_link_index);
        } else if (sub_id == LXO3D_SC_CHNL) {
            r = lxo3d_parse_item_chan_by_name(data, sub_start, sub_end, model, item_index, 0xFFFFFFFFUL, 0);
        } else if (sub_id == LXO3D_SC_CHNS) {
            r = lxo3d_parse_item_chan_by_name(data, sub_start, sub_end, model, item_index, 0xFFFFFFFFUL, 1);
        } else if (sub_id == LXO3D_SC_CHAN) {
            r = lxo3d_parse_item_chan_by_index(data, sub_start, sub_end, model, item_index, 0xFFFFFFFFUL, 0);
        } else if (sub_id == LXO3D_SC_GRAD) {
            r = lxo3d_parse_grad_by_name(data, sub_start, sub_end, model, item_index, 0xFFFFFFFFUL);
        } else if (sub_id == LXO3D_SC_VNAM) {
            lxo3d_copy_s0(it->visible_name, LXO3D_MAX_STRING, data, sub_start, sub_end, &tmp_pos);
        } else if (sub_id == LXO3D_SC_UNIQ) {
            lxo3d_copy_s0(it->unique_name, LXO3D_MAX_STRING, data, sub_start, sub_end, &tmp_pos);
        } else if (sub_id == LXO3D_SC_LAYR) {
            if (lxo3d_range_ok(sub_start, 4UL, sub_end)) {
                it->layer_flags = lxo3d_be_u32(data + sub_start);
            }
            if (sub_end >= sub_start + 4UL) {
                it->rgba[0] = data[sub_end - 4UL];
                it->rgba[1] = data[sub_end - 3UL];
                it->rgba[2] = data[sub_end - 2UL];
                it->rgba[3] = data[sub_end - 1UL];
            }
        } else if (sub_id == LXO3D_SC_BCHN) {
            if (model->bchn_count >= LXO3D_MAX_BCHN) {
                model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            } else {
                tmp_pos = sub_start;
                lxo3d_copy_s0(op, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
                model->bchannels[model->bchn_count].item_index = item_index;
                lxo3d_copy_cstr(model->bchannels[model->bchn_count].operation_type, LXO3D_MAX_STRING, op);
                if (lxo3d_range_ok(tmp_pos, 4UL, sub_end)) {
                    model->bchannels[model->bchn_count].data_index = lxo3d_be_u32(data + tmp_pos);
                }
                ++model->bchn_count;
                model->flags |= LXO3D_FLAG_PROC_UNEVALUATED;
            }
        }

        if (r != LXO3D_OK) {
            return r;
        }
        pos = sub_end;
        if ((sub_size & 1UL) != 0UL) {
            if (pos >= end) {
                return LXO3D_E_TRUNCATED;
            }
            ++pos;
        }
    }

    it->link_count = model->link_count - it->first_link;
    it->channel_count = model->channel_count - it->first_channel;
    it->bchn_count = model->bchn_count - it->first_bchn;
    model->flags |= LXO3D_FLAG_SCENE_META;
    return LXO3D_OK;
}

static int lxo3d_parse_envl(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long env_index;
    unsigned long sub_id;
    unsigned long sub_size;
    unsigned long sub_start;
    unsigned long sub_end;
    unsigned long key_index;
    unsigned long sat;
    int ok;
    Lxo3D_Envelope *env;
    Lxo3D_EnvKey *key;

    if (model->envelope_count >= LXO3D_MAX_ENVELOPES) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    pos = start;
    ok = 1;
    env_index = model->envelope_count;
    env = &model->envelopes[env_index];
    env->index = lxo3d_read_vx(data, end, &pos, &ok);
    if (!ok) {
        return LXO3D_E_TRUNCATED;
    }
    if (!lxo3d_range_ok(pos, 4UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    env->type = lxo3d_be_u32(data + pos);
    pos += 4UL;
    env->first_key = model->env_key_count;
    ++model->envelope_count;

    while (pos < end) {
        if (!lxo3d_range_ok(pos, 6UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        sub_id = lxo3d_be_u32(data + pos);
        sub_size = lxo3d_be_u16(data + pos + 4UL);
        sub_start = pos + 6UL;
        if (sub_size > end - sub_start) {
            return LXO3D_E_TRUNCATED;
        }
        sub_end = sub_start + sub_size;

        if (sub_id == LXO3D_SC_KEY_ || lxo3d_id_has3(sub_id, 'K', 'E', 'Y')) {
            if (model->env_key_count >= LXO3D_MAX_ENV_KEYS) {
                model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            } else if (lxo3d_range_ok(sub_start, 8UL, sub_end)) {
                key_index = model->env_key_count;
                key = &model->env_keys[key_index];
                key->envelope_array_index = env_index;
                sat = 0UL;
                key->input = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start), &sat);
                key->value = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start + 4UL), &sat);
                if (sat != 0UL) {
                    model->flags |= LXO3D_FLAG_FIXED_SATURATED;
                }
                ++model->env_key_count;
            }
        } else if (sub_id == LXO3D_SC_TANI || sub_id == LXO3D_SC_TANO) {
            if (model->env_key_count > env->first_key && lxo3d_range_ok(sub_start, 10UL, sub_end)) {
                key = &model->env_keys[model->env_key_count - 1UL];
                sat = 0UL;
                if (sub_id == LXO3D_SC_TANI) {
                    key->tani_type = lxo3d_be_u16(data + sub_start);
                    key->tani_weight = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start + 2UL), &sat);
                    key->tani_value = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start + 6UL), &sat);
                } else {
                    key->tano_type = lxo3d_be_u16(data + sub_start);
                    key->tano_weight = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start + 2UL), &sat);
                    key->tano_value = lxo3d_f4_to_fixed(lxo3d_be_u32(data + sub_start + 6UL), &sat);
                }
                if (sat != 0UL) {
                    model->flags |= LXO3D_FLAG_FIXED_SATURATED;
                }
            }
        } else if (sub_id == LXO3D_SC_PRE_ || lxo3d_id_has3(sub_id, 'P', 'R', 'E')) {
            if (lxo3d_range_ok(sub_start, 2UL, sub_end)) {
                env->pre_behavior = lxo3d_be_u16(data + sub_start);
            }
        } else if (sub_id == LXO3D_SC_POST) {
            if (lxo3d_range_ok(sub_start, 2UL, sub_end)) {
                env->post_behavior = lxo3d_be_u16(data + sub_start);
            }
        }
        pos = sub_end;
        if ((sub_size & 1UL) != 0UL) {
            if (pos >= end) {
                return LXO3D_E_TRUNCATED;
            }
            ++pos;
        }
    }
    env->key_count = model->env_key_count - env->first_key;
    model->flags |= LXO3D_FLAG_ANIM_META;
    return LXO3D_OK;
}

static int lxo3d_parse_actn(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long action_index;
    unsigned long sub_id;
    unsigned long sub_size;
    unsigned long sub_start;
    unsigned long sub_end;
    unsigned long current_item;
    unsigned long ref;
    unsigned long tmp_pos;
    int have_item;
    int r;
    Lxo3D_Action *act;

    if (model->action_count >= LXO3D_MAX_ACTIONS) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    action_index = model->action_count;
    act = &model->actions[action_index];
    pos = start;
    lxo3d_copy_s0(act->name, LXO3D_MAX_STRING, data, pos, end, &pos);
    lxo3d_copy_s0(act->type, LXO3D_MAX_STRING, data, pos, end, &pos);
    act->first_channel = model->channel_count;
    ++model->action_count;

    current_item = 0xFFFFFFFFUL;
    have_item = 0;
    while (pos < end) {
        if (!lxo3d_range_ok(pos, 6UL, end)) {
            return LXO3D_E_TRUNCATED;
        }
        sub_id = lxo3d_be_u32(data + pos);
        sub_size = lxo3d_be_u16(data + pos + 4UL);
        sub_start = pos + 6UL;
        if (sub_size > end - sub_start) {
            return LXO3D_E_TRUNCATED;
        }
        sub_end = sub_start + sub_size;
        r = LXO3D_OK;
        if (sub_id == LXO3D_ITEM) {
            if (!lxo3d_range_ok(sub_start, 4UL, sub_end)) {
                return LXO3D_E_TRUNCATED;
            }
            ref = lxo3d_be_u32(data + sub_start);
            have_item = lxo3d_find_item_by_ref(model, ref, &current_item);
            if (!have_item) {
                current_item = 0xFFFFFFFFUL;
            }
        } else if (sub_id == LXO3D_SC_CHAN) {
            r = lxo3d_parse_item_chan_by_index(data, sub_start, sub_end, model, current_item, action_index, 1);
        } else if (sub_id == LXO3D_SC_GRAD) {
            r = lxo3d_parse_grad_by_index(data, sub_start, sub_end, model, current_item, action_index);
        } else if (sub_id == LXO3D_SC_CHNS) {
            Lxo3D_Channel *ch;
            ch = lxo3d_new_channel(model, current_item, action_index);
            if (ch != (Lxo3D_Channel *)0) {
                tmp_pos = sub_start;
                lxo3d_copy_s0(ch->name, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
                ch->channel_name_index = lxo3d_read_vx(data, sub_end, &tmp_pos, &have_item);
                if (ch->channel_name_index < model->channel_name_count) {
                    lxo3d_copy_cstr(ch->name, LXO3D_MAX_STRING, model->channel_names[ch->channel_name_index]);
                }
                ch->type = LXO3D_CHAN_STRING;
                lxo3d_copy_s0(ch->string_value, LXO3D_MAX_STRING, data, tmp_pos, sub_end, &tmp_pos);
            }
        }
        if (r != LXO3D_OK) {
            return r;
        }
        pos = sub_end;
        if ((sub_size & 1UL) != 0UL) {
            if (pos >= end) {
                return LXO3D_E_TRUNCATED;
            }
            ++pos;
        }
    }
    act->channel_count = model->channel_count - act->first_channel;
    model->flags |= LXO3D_FLAG_ANIM_META;
    return LXO3D_OK;
}

static int lxo3d_parse_data_chunk(const unsigned char *data, unsigned long start, unsigned long end, Lxo3D_Model *model)
{
    unsigned long pos;
    unsigned long next;
    unsigned long data_index;
    Lxo3D_Data *dt;

    if (model->data_count >= LXO3D_MAX_DATA) {
        model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
        return LXO3D_OK;
    }
    if (!lxo3d_range_ok(start, 8UL, end)) {
        return LXO3D_E_TRUNCATED;
    }
    data_index = model->data_count;
    dt = &model->datas[data_index];
    dt->index = lxo3d_be_u32(data + start);
    dt->unknown = lxo3d_be_u32(data + start + 4UL);
    dt->first_pair = model->data_pair_count;
    pos = start + 8UL;
    lxo3d_copy_s0(dt->exotype, LXO3D_MAX_STRING, data, pos, end, &pos);
    while (pos < end) {
        if (model->data_pair_count >= LXO3D_MAX_DATA_PAIRS) {
            model->flags |= LXO3D_FLAG_TRUNCATED_OUTPUT;
            break;
        }
        model->data_pairs[model->data_pair_count].data_array_index = data_index;
        lxo3d_copy_s0(model->data_pairs[model->data_pair_count].name, LXO3D_MAX_STRING, data, pos, end, &next);
        if (next <= pos) {
            break;
        }
        pos = next;
        if (pos >= end) {
            break;
        }
        lxo3d_copy_s0(model->data_pairs[model->data_pair_count].value, LXO3D_MAX_STRING, data, pos, end, &next);
        if (next <= pos) {
            break;
        }
        pos = next;
        ++model->data_pair_count;
    }
    dt->pair_count = model->data_pair_count - dt->first_pair;
    ++model->data_count;
    model->flags |= LXO3D_FLAG_PROC_UNEVALUATED;
    return LXO3D_OK;
}

const Lxo3D_Channel *lxo3d_find_item_channel(const Lxo3D_Model *model,
                                             unsigned long item_index,
                                             const char *channel_name)
{
    unsigned long i;
    const Lxo3D_Channel *ch;
    if (model == (const Lxo3D_Model *)0 || channel_name == (const char *)0) {
        return (const Lxo3D_Channel *)0;
    }
    if (item_index >= model->item_count) {
        return (const Lxo3D_Channel *)0;
    }
    for (i = model->items[item_index].first_channel;
         i < model->items[item_index].first_channel + model->items[item_index].channel_count && i < model->channel_count;
         ++i) {
        ch = &model->channels[i];
        if (lxo3d_streqi(ch->name, channel_name)) {
            return ch;
        }
    }
    for (i = 0UL; i < model->channel_count; ++i) {
        ch = &model->channels[i];
        if (ch->item_index == item_index && lxo3d_streqi(ch->name, channel_name)) {
            return ch;
        }
    }
    return (const Lxo3D_Channel *)0;
}

static void lxo3d_transform_set_axis(Lxo3D_Vec3 *v, char axis, Lxo3D_Fixed value)
{
    if (axis == 'X' || axis == 'x') {
        v->x = value;
    } else if (axis == 'Y' || axis == 'y') {
        v->y = value;
    } else if (axis == 'Z' || axis == 'z') {
        v->z = value;
    }
}

static int lxo3d_try_transform_channel(Lxo3D_Transform *tr, const Lxo3D_Channel *ch)
{
    unsigned long n;
    char axis;
    if (ch == (const Lxo3D_Channel *)0) {
        return 0;
    }
    n = 0UL;
    while (ch->name[n] != (char)0) {
        ++n;
    }
    if (n < 5UL) {
        return 0;
    }
    axis = ch->name[n - 1UL];
    if (lxo3d_containsi(ch->name, "pos.") || lxo3d_containsi(ch->name, "position.")) {
        lxo3d_transform_set_axis(&tr->position, axis, ch->fixed_value);
        tr->has_position = 1UL;
        return 1;
    }
    if (lxo3d_containsi(ch->name, "rot.") || lxo3d_containsi(ch->name, "rotation.")) {
        lxo3d_transform_set_axis(&tr->rotation, axis, ch->fixed_value);
        tr->has_rotation = 1UL;
        return 1;
    }
    if (lxo3d_containsi(ch->name, "scl.") || lxo3d_containsi(ch->name, "scale.")) {
        lxo3d_transform_set_axis(&tr->scale, axis, ch->fixed_value);
        tr->has_scale = 1UL;
        return 1;
    }
    return 0;
}

int lxo3d_item_transform(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_Transform *out_transform)
{
    unsigned long i;
    const Lxo3D_Item *it;
    if (model == (const Lxo3D_Model *)0 || out_transform == (Lxo3D_Transform *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (item_index >= model->item_count) {
        return LXO3D_E_BAD_ARG;
    }
    out_transform->position.x = 0L;
    out_transform->position.y = 0L;
    out_transform->position.z = 0L;
    out_transform->rotation.x = 0L;
    out_transform->rotation.y = 0L;
    out_transform->rotation.z = 0L;
    out_transform->scale.x = LXO3D_FIXED_ONE;
    out_transform->scale.y = LXO3D_FIXED_ONE;
    out_transform->scale.z = LXO3D_FIXED_ONE;
    out_transform->has_position = 0UL;
    out_transform->has_rotation = 0UL;
    out_transform->has_scale = 0UL;

    it = &model->items[item_index];
    for (i = it->first_channel; i < it->first_channel + it->channel_count && i < model->channel_count; ++i) {
        (void)lxo3d_try_transform_channel(out_transform, &model->channels[i]);
    }
    for (i = 0UL; i < model->channel_count; ++i) {
        if (model->channels[i].item_index == item_index && model->channels[i].action_index != 0xFFFFFFFFUL) {
            (void)lxo3d_try_transform_channel(out_transform, &model->channels[i]);
        }
    }
    return LXO3D_OK;
}


static unsigned long lxo3d_ul_sat_mul(unsigned long a, unsigned long b, unsigned long *sat)
{
    if (a != 0UL && b > 2147483647UL / a) {
        *sat = 1UL;
        return 2147483647UL;
    }
    return a * b;
}

static unsigned long lxo3d_ul_sat_add(unsigned long a, unsigned long b, unsigned long *sat)
{
    if (b > 2147483647UL - a) {
        *sat = 1UL;
        return 2147483647UL;
    }
    return a + b;
}

static Lxo3D_Fixed lxo3d_fixed_mul(Lxo3D_Fixed a, Lxo3D_Fixed b)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long res;
    unsigned long cross;
    unsigned long sat;
    int neg;

    neg = 0;
    if (a < 0L) { neg = !neg; ua = (unsigned long)(-a); } else { ua = (unsigned long)a; }
    if (b < 0L) { neg = !neg; ub = (unsigned long)(-b); } else { ub = (unsigned long)b; }

    ah = ua >> 8;
    al = ua & 255UL;
    bh = ub >> 8;
    bl = ub & 255UL;
    sat = 0UL;
    res = lxo3d_ul_sat_mul(ah, bh, &sat);
    cross = lxo3d_ul_sat_add(lxo3d_ul_sat_mul(ah, bl, &sat), lxo3d_ul_sat_mul(al, bh, &sat), &sat);
    res = lxo3d_ul_sat_add(res, cross >> 8, &sat);
    res = lxo3d_ul_sat_add(res, (al * bl) >> 16, &sat);
    if (sat != 0UL || res > 2147483647UL) {
        return neg ? LXO3D_FIXED_MIN : LXO3D_FIXED_MAX;
    }
    return neg ? -(Lxo3D_Fixed)res : (Lxo3D_Fixed)res;
}

static Lxo3D_Fixed lxo3d_fixed_div(Lxo3D_Fixed a, Lxo3D_Fixed b)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long q;
    unsigned long r;
    unsigned long frac_hi;
    unsigned long rem_hi;
    unsigned long frac_lo;
    unsigned long res;
    int neg;
    if (b == 0L) {
        if (a < 0L) {
            return LXO3D_FIXED_MIN;
        }
        return LXO3D_FIXED_MAX;
    }
    neg = 0;
    if (a < 0L) { neg = !neg; ua = (unsigned long)(-a); } else { ua = (unsigned long)a; }
    if (b < 0L) { neg = !neg; ub = (unsigned long)(-b); } else { ub = (unsigned long)b; }
    q = ua / ub;
    r = ua % ub;
    if (q > 32767UL) {
        return neg ? LXO3D_FIXED_MIN : LXO3D_FIXED_MAX;
    }
    frac_hi = (r * 256UL) / ub;
    rem_hi = (r * 256UL) % ub;
    frac_lo = (rem_hi * 256UL) / ub;
    res = (q << 16) + (frac_hi << 8) + frac_lo;
    if (res > 2147483647UL) {
        return neg ? LXO3D_FIXED_MIN : LXO3D_FIXED_MAX;
    }
    return neg ? -(Lxo3D_Fixed)res : (Lxo3D_Fixed)res;
}

static Lxo3D_Fixed lxo3d_fixed_abs(Lxo3D_Fixed v)
{
    if (v < 0L) {
        return -v;
    }
    return v;
}

static Lxo3D_Fixed lxo3d_wrap_pi(Lxo3D_Fixed a)
{
    const Lxo3D_Fixed pi = 205887L;
    const Lxo3D_Fixed two_pi = 411775L;
    while (a > pi) {
        a -= two_pi;
    }
    while (a < -pi) {
        a += two_pi;
    }
    return a;
}

static Lxo3D_Fixed lxo3d_sin_approx(Lxo3D_Fixed a)
{
    const Lxo3D_Fixed b = 83443L;   /* 4/pi in 16.16 */
    const Lxo3D_Fixed c = -26561L;  /* -4/(pi*pi) in 16.16 */
    Lxo3D_Fixed aa;
    Lxo3D_Fixed y;
    a = lxo3d_wrap_pi(a);
    aa = lxo3d_fixed_abs(a);
    y = lxo3d_fixed_mul(b, a) + lxo3d_fixed_mul(c, lxo3d_fixed_mul(a, aa));
    return y;
}

static Lxo3D_Fixed lxo3d_cos_approx(Lxo3D_Fixed a)
{
    const Lxo3D_Fixed half_pi = 102944L;
    return lxo3d_sin_approx(a + half_pi);
}

static void lxo3d_mat4_identity(Lxo3D_Mat4 *m)
{
    unsigned long i;
    for (i = 0UL; i < 16UL; ++i) {
        m->m[i] = 0L;
    }
    m->m[0] = LXO3D_FIXED_ONE;
    m->m[5] = LXO3D_FIXED_ONE;
    m->m[10] = LXO3D_FIXED_ONE;
    m->m[15] = LXO3D_FIXED_ONE;
}

static void lxo3d_mat4_copy(Lxo3D_Mat4 *dst, const Lxo3D_Mat4 *src)
{
    unsigned long i;
    for (i = 0UL; i < 16UL; ++i) {
        dst->m[i] = src->m[i];
    }
}

static void lxo3d_mat4_mul(Lxo3D_Mat4 *out, const Lxo3D_Mat4 *a, const Lxo3D_Mat4 *b)
{
    Lxo3D_Mat4 r;
    unsigned long row;
    unsigned long col;
    unsigned long k;
    Lxo3D_Fixed sum;
    for (row = 0UL; row < 4UL; ++row) {
        for (col = 0UL; col < 4UL; ++col) {
            sum = 0L;
            for (k = 0UL; k < 4UL; ++k) {
                sum += lxo3d_fixed_mul(a->m[row * 4UL + k], b->m[k * 4UL + col]);
            }
            r.m[row * 4UL + col] = sum;
        }
    }
    lxo3d_mat4_copy(out, &r);
}

static void lxo3d_mat4_post_mul(Lxo3D_Mat4 *m, const Lxo3D_Mat4 *rhs)
{
    Lxo3D_Mat4 tmp;
    lxo3d_mat4_mul(&tmp, m, rhs);
    lxo3d_mat4_copy(m, &tmp);
}

static void lxo3d_mat4_translate(Lxo3D_Mat4 *m, const Lxo3D_Vec3 *v)
{
    lxo3d_mat4_identity(m);
    m->m[3] = v->x;
    m->m[7] = v->y;
    m->m[11] = v->z;
}

static void lxo3d_mat4_scale(Lxo3D_Mat4 *m, const Lxo3D_Vec3 *v)
{
    lxo3d_mat4_identity(m);
    m->m[0] = v->x;
    m->m[5] = v->y;
    m->m[10] = v->z;
}

static void lxo3d_mat4_rotx(Lxo3D_Mat4 *m, Lxo3D_Fixed a)
{
    Lxo3D_Fixed s;
    Lxo3D_Fixed c;
    s = lxo3d_sin_approx(a);
    c = lxo3d_cos_approx(a);
    lxo3d_mat4_identity(m);
    m->m[5] = c;
    m->m[6] = -s;
    m->m[9] = s;
    m->m[10] = c;
}

static void lxo3d_mat4_roty(Lxo3D_Mat4 *m, Lxo3D_Fixed a)
{
    Lxo3D_Fixed s;
    Lxo3D_Fixed c;
    s = lxo3d_sin_approx(a);
    c = lxo3d_cos_approx(a);
    lxo3d_mat4_identity(m);
    m->m[0] = c;
    m->m[2] = s;
    m->m[8] = -s;
    m->m[10] = c;
}

static void lxo3d_mat4_rotz(Lxo3D_Mat4 *m, Lxo3D_Fixed a)
{
    Lxo3D_Fixed s;
    Lxo3D_Fixed c;
    s = lxo3d_sin_approx(a);
    c = lxo3d_cos_approx(a);
    lxo3d_mat4_identity(m);
    m->m[0] = c;
    m->m[1] = -s;
    m->m[4] = s;
    m->m[5] = c;
}

static void lxo3d_vec3_neg(Lxo3D_Vec3 *out, const Lxo3D_Vec3 *in)
{
    out->x = -in->x;
    out->y = -in->y;
    out->z = -in->z;
}

static void lxo3d_mat4_transform_point(const Lxo3D_Mat4 *m, const Lxo3D_Vec3 *src, Lxo3D_Vec3 *dst)
{
    Lxo3D_Fixed x;
    Lxo3D_Fixed y;
    Lxo3D_Fixed z;
    x = lxo3d_fixed_mul(m->m[0], src->x) + lxo3d_fixed_mul(m->m[1], src->y) + lxo3d_fixed_mul(m->m[2], src->z) + m->m[3];
    y = lxo3d_fixed_mul(m->m[4], src->x) + lxo3d_fixed_mul(m->m[5], src->y) + lxo3d_fixed_mul(m->m[6], src->z) + m->m[7];
    z = lxo3d_fixed_mul(m->m[8], src->x) + lxo3d_fixed_mul(m->m[9], src->y) + lxo3d_fixed_mul(m->m[10], src->z) + m->m[11];
    dst->x = x;
    dst->y = y;
    dst->z = z;
}

static void lxo3d_transform_init(Lxo3D_Transform *tr)
{
    tr->position.x = 0L;
    tr->position.y = 0L;
    tr->position.z = 0L;
    tr->rotation.x = 0L;
    tr->rotation.y = 0L;
    tr->rotation.z = 0L;
    tr->scale.x = LXO3D_FIXED_ONE;
    tr->scale.y = LXO3D_FIXED_ONE;
    tr->scale.z = LXO3D_FIXED_ONE;
    tr->has_position = 0UL;
    tr->has_rotation = 0UL;
    tr->has_scale = 0UL;
}

static void lxo3d_apply_channel_to_vec(Lxo3D_Vec3 *v, const Lxo3D_Channel *ch)
{
    unsigned long n;
    char axis;
    n = 0UL;
    while (ch->name[n] != (char)0) {
        ++n;
    }
    if (n == 0UL) {
        return;
    }
    axis = ch->name[n - 1UL];
    if (axis == 'r') { axis = 'R'; }
    if (axis == 'g') { axis = 'G'; }
    if (axis == 'b') { axis = 'B'; }
    if (axis == 'R') { v->x = ch->fixed_value; }
    else if (axis == 'G') { v->y = ch->fixed_value; }
    else if (axis == 'B') { v->z = ch->fixed_value; }
    else { lxo3d_transform_set_axis(v, axis, ch->fixed_value); }
}

static int lxo3d_try_pivot_channel(Lxo3D_AdvancedTransform *adv, const Lxo3D_Channel *ch)
{
    if (ch == (const Lxo3D_Channel *)0) {
        return 0;
    }
    if (lxo3d_containsi(ch->name, "rpiv") || lxo3d_containsi(ch->name, "rotpivot") ||
        lxo3d_containsi(ch->name, "rotatepivot") || lxo3d_containsi(ch->name, "pivot.rot")) {
        lxo3d_apply_channel_to_vec(&adv->rotate_pivot, ch);
        adv->has_rotate_pivot = 1UL;
        return 1;
    }
    if (lxo3d_containsi(ch->name, "spiv") || lxo3d_containsi(ch->name, "sclpivot") ||
        lxo3d_containsi(ch->name, "scalepivot") || lxo3d_containsi(ch->name, "pivot.scl")) {
        lxo3d_apply_channel_to_vec(&adv->scale_pivot, ch);
        adv->has_scale_pivot = 1UL;
        return 1;
    }
    return 0;
}

static void lxo3d_build_local_matrix(const Lxo3D_AdvancedTransform *adv, Lxo3D_Mat4 *out)
{
    Lxo3D_Mat4 t;
    Lxo3D_Mat4 rp;
    Lxo3D_Mat4 nrp;
    Lxo3D_Mat4 sp;
    Lxo3D_Mat4 nsp;
    Lxo3D_Mat4 s;
    Lxo3D_Mat4 rx;
    Lxo3D_Mat4 ry;
    Lxo3D_Mat4 rz;
    Lxo3D_Vec3 neg;
    lxo3d_mat4_identity(out);
    lxo3d_mat4_translate(&t, &adv->trs.position);
    lxo3d_mat4_post_mul(out, &t);
    lxo3d_mat4_translate(&rp, &adv->rotate_pivot);
    lxo3d_vec3_neg(&neg, &adv->rotate_pivot);
    lxo3d_mat4_translate(&nrp, &neg);
    lxo3d_mat4_rotx(&rx, adv->trs.rotation.x);
    lxo3d_mat4_roty(&ry, adv->trs.rotation.y);
    lxo3d_mat4_rotz(&rz, adv->trs.rotation.z);
    lxo3d_mat4_post_mul(out, &rp);
    lxo3d_mat4_post_mul(out, &rz);
    lxo3d_mat4_post_mul(out, &ry);
    lxo3d_mat4_post_mul(out, &rx);
    lxo3d_mat4_post_mul(out, &nrp);
    lxo3d_mat4_translate(&sp, &adv->scale_pivot);
    lxo3d_vec3_neg(&neg, &adv->scale_pivot);
    lxo3d_mat4_translate(&nsp, &neg);
    lxo3d_mat4_scale(&s, &adv->trs.scale);
    lxo3d_mat4_post_mul(out, &sp);
    lxo3d_mat4_post_mul(out, &s);
    lxo3d_mat4_post_mul(out, &nsp);
}

static int lxo3d_find_layer_by_item_ref(const Lxo3D_Model *model, unsigned long ref_id, unsigned long *out_layer)
{
    unsigned long i;
    for (i = 0UL; i < model->layer_count; ++i) {
        if (model->layers[i].item_ref == ref_id) {
            if (out_layer != (unsigned long *)0) {
                *out_layer = i;
            }
            return 1;
        }
    }
    return 0;
}

int lxo3d_eval_envelope_linear(const Lxo3D_Model *model, unsigned long envelope_index, Lxo3D_Fixed input, Lxo3D_Fixed *out_value)
{
    const Lxo3D_Envelope *env;
    const Lxo3D_EnvKey *a;
    const Lxo3D_EnvKey *b;
    unsigned long i;
    Lxo3D_Fixed t;
    if (model == (const Lxo3D_Model *)0 || out_value == (Lxo3D_Fixed *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (envelope_index >= model->envelope_count) {
        return LXO3D_E_BAD_ARG;
    }
    env = &model->envelopes[envelope_index];
    if (env->key_count == 0UL) {
        *out_value = 0L;
        return LXO3D_OK;
    }
    a = &model->env_keys[env->first_key];
    if (input <= a->input) {
        *out_value = a->value;
        return LXO3D_OK;
    }
    for (i = 1UL; i < env->key_count; ++i) {
        b = &model->env_keys[env->first_key + i];
        if (input <= b->input) {
            t = lxo3d_fixed_div(input - a->input, b->input - a->input);
            *out_value = a->value + lxo3d_fixed_mul(t, b->value - a->value);
            return LXO3D_OK;
        }
        a = b;
    }
    *out_value = a->value;
    return LXO3D_OK;
}

int lxo3d_eval_channel_fixed(const Lxo3D_Model *model, unsigned long item_index, const char *channel_name, Lxo3D_Fixed time, Lxo3D_Fixed *out_value)
{
    const Lxo3D_Channel *ch;
    unsigned long i;
    if (model == (const Lxo3D_Model *)0 || out_value == (Lxo3D_Fixed *)0) {
        return LXO3D_E_BAD_ARG;
    }
    ch = lxo3d_find_item_channel(model, item_index, channel_name);
    if (ch != (const Lxo3D_Channel *)0) {
        if ((ch->type & LXO3D_CHAN_ENVELOPE) != 0UL && ch->envelope_index < model->envelope_count) {
            return lxo3d_eval_envelope_linear(model, ch->envelope_index, time, out_value);
        }
        if ((ch->type & 0x0FUL) == LXO3D_CHAN_INT) {
            *out_value = ch->int_value * LXO3D_FIXED_ONE;
        } else {
            *out_value = ch->fixed_value;
        }
        return LXO3D_OK;
    }
    for (i = 0UL; i < model->channel_link_count; ++i) {
        if (model->channel_links[i].item_index == item_index && lxo3d_streqi(model->channel_links[i].to_channel, channel_name)) {
            unsigned long from_item;
            if (lxo3d_find_item_by_ref(model, model->channel_links[i].from_ref, &from_item)) {
                return lxo3d_eval_channel_fixed(model, from_item, model->channel_links[i].from_channel, time, out_value);
            }
        }
    }
    return LXO3D_E_BAD_ARG;
}

static void lxo3d_collect_transform_channels(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_AdvancedTransform *adv)
{
    unsigned long i;
    const Lxo3D_Item *it;
    if (item_index >= model->item_count) {
        return;
    }
    it = &model->items[item_index];
    for (i = it->first_channel; i < it->first_channel + it->channel_count && i < model->channel_count; ++i) {
        (void)lxo3d_try_transform_channel(&adv->trs, &model->channels[i]);
        (void)lxo3d_try_pivot_channel(adv, &model->channels[i]);
    }
    for (i = 0UL; i < model->channel_count; ++i) {
        if (model->channels[i].item_index == item_index && model->channels[i].action_index != 0xFFFFFFFFUL) {
            (void)lxo3d_try_transform_channel(&adv->trs, &model->channels[i]);
            (void)lxo3d_try_pivot_channel(adv, &model->channels[i]);
        }
    }
}

static int lxo3d_item_local_matrix_with_xfrmcore(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_Mat4 *out)
{
    Lxo3D_AdvancedTransform adv;
    Lxo3D_Mat4 xm;
    long best;
    long pass;
    unsigned long found;
    unsigned long li;
    unsigned long xi;
    if (item_index >= model->item_count) {
        return LXO3D_E_BAD_ARG;
    }
    lxo3d_transform_init(&adv.trs);
    adv.rotate_pivot.x = 0L;
    adv.rotate_pivot.y = 0L;
    adv.rotate_pivot.z = 0L;
    adv.scale_pivot.x = 0L;
    adv.scale_pivot.y = 0L;
    adv.scale_pivot.z = 0L;
    adv.has_rotate_pivot = 0UL;
    adv.has_scale_pivot = 0UL;
    lxo3d_collect_transform_channels(model, item_index, &adv);
    lxo3d_build_local_matrix(&adv, out);
    best = 2147483647L;
    found = 1UL;
    while (found != 0UL) {
        found = 0UL;
        pass = -2147483647L;
        for (li = 0UL; li < model->link_count; ++li) {
            if (model->links[li].target_ref == model->items[item_index].ref_id && lxo3d_streqi(model->links[li].graph, "xfrmcore")) {
                if (model->links[li].link_index < best && model->links[li].link_index > pass) {
                    if (lxo3d_find_item_by_ref(model, model->items[model->links[li].item_index].ref_id, &xi)) {
                        pass = model->links[li].link_index;
                        found = 1UL;
                    }
                }
            }
        }
        if (found != 0UL) {
            for (li = 0UL; li < model->link_count; ++li) {
                if (model->links[li].target_ref == model->items[item_index].ref_id && lxo3d_streqi(model->links[li].graph, "xfrmcore") && model->links[li].link_index == pass) {
                    xi = model->links[li].item_index;
                    if (xi < model->item_count && lxo3d_item_advanced_transform(model, xi, &adv) == LXO3D_OK) {
                        lxo3d_mat4_copy(&xm, &adv.local_matrix);
                        lxo3d_mat4_post_mul(out, &xm);
                    }
                }
            }
            best = pass;
        }
    }
    return LXO3D_OK;
}

int lxo3d_item_advanced_transform(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_AdvancedTransform *out_transform)
{
    unsigned long layer_index;
    unsigned long parent_index;
    const Lxo3D_Item *it;
    Lxo3D_Mat4 parent;
    Lxo3D_Mat4 local;
    if (model == (const Lxo3D_Model *)0 || out_transform == (Lxo3D_AdvancedTransform *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (item_index >= model->item_count) {
        return LXO3D_E_BAD_ARG;
    }
    it = &model->items[item_index];
    lxo3d_transform_init(&out_transform->trs);
    out_transform->rotate_pivot.x = 0L;
    out_transform->rotate_pivot.y = 0L;
    out_transform->rotate_pivot.z = 0L;
    out_transform->scale_pivot.x = 0L;
    out_transform->scale_pivot.y = 0L;
    out_transform->scale_pivot.z = 0L;
    out_transform->has_rotate_pivot = 0UL;
    out_transform->has_scale_pivot = 0UL;
    out_transform->parent_item_index = 0xFFFFFFFFUL;
    out_transform->depth = 0UL;
    out_transform->flags = LXO3D_FLAG_WORLD_XFORM_APPROX;
    lxo3d_mat4_identity(&out_transform->local_matrix);
    lxo3d_mat4_identity(&out_transform->world_matrix);
    if (lxo3d_find_layer_by_item_ref(model, it->ref_id, &layer_index)) {
        out_transform->rotate_pivot = model->layers[layer_index].pivot;
        out_transform->scale_pivot = model->layers[layer_index].scale_pivot;
        out_transform->has_rotate_pivot = 1UL;
        out_transform->has_scale_pivot = 1UL;
    }
    lxo3d_collect_transform_channels(model, item_index, out_transform);
    (void)lxo3d_item_local_matrix_with_xfrmcore(model, item_index, &out_transform->local_matrix);
    lxo3d_mat4_copy(&out_transform->world_matrix, &out_transform->local_matrix);
    parent_index = item_index;
    while (parent_index < model->item_count && model->items[parent_index].parent_ref != 0UL && out_transform->depth < 32UL) {
        if (!lxo3d_find_item_by_ref(model, model->items[parent_index].parent_ref, &parent_index)) {
            break;
        }
        if (out_transform->parent_item_index == 0xFFFFFFFFUL) {
            out_transform->parent_item_index = parent_index;
        }
        if (lxo3d_item_local_matrix_with_xfrmcore(model, parent_index, &parent) == LXO3D_OK) {
            lxo3d_mat4_copy(&local, &out_transform->world_matrix);
            lxo3d_mat4_mul(&out_transform->world_matrix, &parent, &local);
        }
        ++out_transform->depth;
    }
    return LXO3D_OK;
}

int lxo3d_transform_point_world(const Lxo3D_Model *model, unsigned long item_index, const Lxo3D_Vec3 *src, Lxo3D_Vec3 *dst)
{
    Lxo3D_AdvancedTransform adv;
    if (model == (const Lxo3D_Model *)0 || src == (const Lxo3D_Vec3 *)0 || dst == (Lxo3D_Vec3 *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (lxo3d_item_advanced_transform(model, item_index, &adv) != LXO3D_OK) {
        return LXO3D_E_BAD_ARG;
    }
    lxo3d_mat4_transform_point(&adv.world_matrix, src, dst);
    return LXO3D_OK;
}

static int lxo3d_data_pair_fixed(const Lxo3D_Model *model, unsigned long data_array_index, const char *name, Lxo3D_Fixed *out_value)
{
    unsigned long i;
    const char *s;
    long sign;
    long ip;
    long fp;
    long div;
    for (i = 0UL; i < model->data_pair_count; ++i) {
        if (model->data_pairs[i].data_array_index == data_array_index && lxo3d_streqi(model->data_pairs[i].name, name)) {
            s = model->data_pairs[i].value;
            sign = 1L;
            ip = 0L;
            fp = 0L;
            div = 1L;
            if (*s == '-') {
                sign = -1L;
                ++s;
            }
            while (*s >= '0' && *s <= '9') {
                ip = ip * 10L + (long)(*s - '0');
                ++s;
            }
            if (*s == '.') {
                ++s;
                while (*s >= '0' && *s <= '9' && div < 100000L) {
                    fp = fp * 10L + (long)(*s - '0');
                    div *= 10L;
                    ++s;
                }
            }
            *out_value = sign * ((ip * LXO3D_FIXED_ONE) + (fp * LXO3D_FIXED_ONE) / div);
            return 1;
        }
    }
    return 0;
}

static int lxo3d_find_data_by_stored_index(const Lxo3D_Model *model, unsigned long stored_index, unsigned long *out_array_index)
{
    unsigned long i;
    for (i = 0UL; i < model->data_count; ++i) {
        if (model->datas[i].index == stored_index) {
            if (out_array_index != (unsigned long *)0) {
                *out_array_index = i;
            }
            return 1;
        }
    }
    return 0;
}

static unsigned long lxo3d_proc_kind_from_text(const char *name)
{
    if (lxo3d_containsi(name, "union") || lxo3d_containsi(name, "add")) { return LXO3D_PROC_UNION; }
    if (lxo3d_containsi(name, "intersect") || lxo3d_containsi(name, "and")) { return LXO3D_PROC_INTERSECT; }
    if (lxo3d_containsi(name, "subtract") || lxo3d_containsi(name, "difference") || lxo3d_containsi(name, "cut")) { return LXO3D_PROC_SUBTRACT; }
    if (lxo3d_containsi(name, "blend") || lxo3d_containsi(name, "smooth")) { return LXO3D_PROC_BLEND; }
    return LXO3D_PROC_UNKNOWN;
}

int lxo3d_extract_proc_op(const Lxo3D_Model *model, unsigned long bchn_index, Lxo3D_ProcOpInfo *out_info)
{
    const Lxo3D_BChannel *bc;
    unsigned long data_array;
    unsigned long i;
    if (model == (const Lxo3D_Model *)0 || out_info == (Lxo3D_ProcOpInfo *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (bchn_index >= model->bchn_count) {
        return LXO3D_E_BAD_ARG;
    }
    bc = &model->bchannels[bchn_index];
    out_info->item_index = bc->item_index;
    out_info->data_index = bc->data_index;
    lxo3d_copy_cstr(out_info->op_name, LXO3D_MAX_STRING, bc->operation_type);
    lxo3d_zero_chars(out_info->exotype, LXO3D_MAX_STRING);
    out_info->op_kind = lxo3d_proc_kind_from_text(bc->operation_type);
    out_info->bbox_approximated = 0UL;
    out_info->bbox.valid = 0UL;
    if (lxo3d_find_data_by_stored_index(model, bc->data_index, &data_array)) {
        lxo3d_copy_cstr(out_info->exotype, LXO3D_MAX_STRING, model->datas[data_array].exotype);
        for (i = model->datas[data_array].first_pair; i < model->datas[data_array].first_pair + model->datas[data_array].pair_count && i < model->data_pair_count; ++i) {
            if (lxo3d_streqi(model->data_pairs[i].name, "operation")) {
                out_info->op_kind = lxo3d_proc_kind_from_text(model->data_pairs[i].value);
                lxo3d_copy_cstr(out_info->op_name, LXO3D_MAX_STRING, model->data_pairs[i].value);
            }
        }
        if (lxo3d_data_pair_fixed(model, data_array, "bboxMinX", &out_info->bbox.minv.x) &&
            lxo3d_data_pair_fixed(model, data_array, "bboxMinY", &out_info->bbox.minv.y) &&
            lxo3d_data_pair_fixed(model, data_array, "bboxMinZ", &out_info->bbox.minv.z) &&
            lxo3d_data_pair_fixed(model, data_array, "bboxMaxX", &out_info->bbox.maxv.x) &&
            lxo3d_data_pair_fixed(model, data_array, "bboxMaxY", &out_info->bbox.maxv.y) &&
            lxo3d_data_pair_fixed(model, data_array, "bboxMaxZ", &out_info->bbox.maxv.z)) {
            out_info->bbox.valid = 1UL;
            out_info->bbox_approximated = 1UL;
        }
    }
    return LXO3D_OK;
}

int lxo3d_compute_item_bbox(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_BBox *out_bbox)
{
    unsigned long layer_index;
    unsigned long i;
    const Lxo3D_Layer *ly;
    Lxo3D_Vec3 wp;
    if (model == (const Lxo3D_Model *)0 || out_bbox == (Lxo3D_BBox *)0) {
        return LXO3D_E_BAD_ARG;
    }
    out_bbox->valid = 0UL;
    if (item_index >= model->item_count) {
        return LXO3D_E_BAD_ARG;
    }
    if (!lxo3d_find_layer_by_item_ref(model, model->items[item_index].ref_id, &layer_index)) {
        return LXO3D_OK;
    }
    ly = &model->layers[layer_index];
    for (i = ly->first_point; i < ly->first_point + ly->point_count && i < model->point_count; ++i) {
        (void)lxo3d_transform_point_world(model, item_index, &model->points[i], &wp);
        if (out_bbox->valid == 0UL) {
            out_bbox->minv = wp;
            out_bbox->maxv = wp;
            out_bbox->valid = 1UL;
        } else {
            if (wp.x < out_bbox->minv.x) { out_bbox->minv.x = wp.x; }
            if (wp.y < out_bbox->minv.y) { out_bbox->minv.y = wp.y; }
            if (wp.z < out_bbox->minv.z) { out_bbox->minv.z = wp.z; }
            if (wp.x > out_bbox->maxv.x) { out_bbox->maxv.x = wp.x; }
            if (wp.y > out_bbox->maxv.y) { out_bbox->maxv.y = wp.y; }
            if (wp.z > out_bbox->maxv.z) { out_bbox->maxv.z = wp.z; }
        }
    }
    return LXO3D_OK;
}

int lxo3d_extract_constraint(const Lxo3D_Model *model, unsigned long ordinal, Lxo3D_ConstraintInfo *out_info)
{
    unsigned long i;
    unsigned long target_item;
    if (model == (const Lxo3D_Model *)0 || out_info == (Lxo3D_ConstraintInfo *)0) {
        return LXO3D_E_BAD_ARG;
    }
    if (ordinal < model->link_count) {
        i = ordinal;
        out_info->item_index = model->links[i].item_index;
        out_info->target_item_index = 0xFFFFFFFFUL;
        if (lxo3d_find_item_by_ref(model, model->links[i].target_ref, &target_item)) {
            out_info->target_item_index = target_item;
        }
        out_info->kind = LXO3D_CONSTRAINT_UNKNOWN;
        if (lxo3d_streqi(model->links[i].graph, "parent")) { out_info->kind = LXO3D_CONSTRAINT_PARENT; }
        if (lxo3d_streqi(model->links[i].graph, "xfrmcore")) { out_info->kind = LXO3D_CONSTRAINT_XFRMCORE; }
        lxo3d_copy_cstr(out_info->graph, LXO3D_MAX_STRING, model->links[i].graph);
        lxo3d_zero_chars(out_info->from_channel, LXO3D_MAX_STRING);
        lxo3d_zero_chars(out_info->to_channel, LXO3D_MAX_STRING);
        out_info->order_index = model->links[i].link_index;
        return LXO3D_OK;
    }
    ordinal -= model->link_count;
    if (ordinal < model->channel_link_count) {
        i = ordinal;
        out_info->kind = LXO3D_CONSTRAINT_CHANNEL;
        out_info->item_index = model->channel_links[i].item_index;
        out_info->target_item_index = 0xFFFFFFFFUL;
        if (lxo3d_find_item_by_ref(model, model->channel_links[i].from_ref, &target_item)) {
            out_info->target_item_index = target_item;
        }
        lxo3d_copy_cstr(out_info->graph, LXO3D_MAX_STRING, model->channel_links[i].graph);
        lxo3d_copy_cstr(out_info->from_channel, LXO3D_MAX_STRING, model->channel_links[i].from_channel);
        lxo3d_copy_cstr(out_info->to_channel, LXO3D_MAX_STRING, model->channel_links[i].to_channel);
        out_info->order_index = model->channel_links[i].to_link_index;
        return LXO3D_OK;
    }
    return LXO3D_E_BAD_ARG;
}

static int lxo3d_get_channel_value_by_names(const Lxo3D_Model *model, unsigned long item_index, const char *a, const char *b, const char *c, Lxo3D_Fixed *out_value)
{
    const Lxo3D_Channel *ch;
    ch = lxo3d_find_item_channel(model, item_index, a);
    if (ch == (const Lxo3D_Channel *)0 && b != (const char *)0) { ch = lxo3d_find_item_channel(model, item_index, b); }
    if (ch == (const Lxo3D_Channel *)0 && c != (const char *)0) { ch = lxo3d_find_item_channel(model, item_index, c); }
    if (ch != (const Lxo3D_Channel *)0) {
        if ((ch->type & 0x0FUL) == LXO3D_CHAN_INT) { *out_value = ch->int_value * LXO3D_FIXED_ONE; }
        else { *out_value = ch->fixed_value; }
        return 1;
    }
    return 0;
}

int lxo3d_extract_material(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_MaterialInfo *out_info)
{
    if (model == (const Lxo3D_Model *)0 || out_info == (Lxo3D_MaterialInfo *)0) { return LXO3D_E_BAD_ARG; }
    if (item_index >= model->item_count) { return LXO3D_E_BAD_ARG; }
    out_info->item_index = item_index;
    out_info->diffuse_r = LXO3D_FIXED_ONE;
    out_info->diffuse_g = LXO3D_FIXED_ONE;
    out_info->diffuse_b = LXO3D_FIXED_ONE;
    out_info->opacity = LXO3D_FIXED_ONE;
    out_info->roughness = 0L;
    out_info->specular = 0L;
    out_info->has_color = 0UL;
    out_info->has_opacity = 0UL;
    if (lxo3d_get_channel_value_by_names(model, item_index, "diffCol.R", "diffuse.R", "color.R", &out_info->diffuse_r)) { out_info->has_color = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "diffCol.G", "diffuse.G", "color.G", &out_info->diffuse_g)) { out_info->has_color = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "diffCol.B", "diffuse.B", "color.B", &out_info->diffuse_b)) { out_info->has_color = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "opacity", "alpha", "transparency", &out_info->opacity)) { out_info->has_opacity = 1UL; }
    (void)lxo3d_get_channel_value_by_names(model, item_index, "rough", "roughness", (const char *)0, &out_info->roughness);
    (void)lxo3d_get_channel_value_by_names(model, item_index, "spec", "specular", (const char *)0, &out_info->specular);
    return LXO3D_OK;
}

int lxo3d_extract_camera(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_CameraInfo *out_info)
{
    if (model == (const Lxo3D_Model *)0 || out_info == (Lxo3D_CameraInfo *)0) { return LXO3D_E_BAD_ARG; }
    if (item_index >= model->item_count) { return LXO3D_E_BAD_ARG; }
    out_info->item_index = item_index;
    out_info->focal_length = 0L;
    out_info->film_aperture_x = 0L;
    out_info->film_aperture_y = 0L;
    out_info->near_clip = 0L;
    out_info->far_clip = 0L;
    out_info->has_focal_length = 0UL;
    out_info->has_clip = 0UL;
    if (lxo3d_get_channel_value_by_names(model, item_index, "focalLen", "focalLength", "focal", &out_info->focal_length)) { out_info->has_focal_length = 1UL; }
    (void)lxo3d_get_channel_value_by_names(model, item_index, "filmApertureX", "apertureX", "filmFit.X", &out_info->film_aperture_x);
    (void)lxo3d_get_channel_value_by_names(model, item_index, "filmApertureY", "apertureY", "filmFit.Y", &out_info->film_aperture_y);
    if (lxo3d_get_channel_value_by_names(model, item_index, "nearClip", "clipNear", (const char *)0, &out_info->near_clip)) { out_info->has_clip = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "farClip", "clipFar", (const char *)0, &out_info->far_clip)) { out_info->has_clip = 1UL; }
    return LXO3D_OK;
}

int lxo3d_extract_light(const Lxo3D_Model *model, unsigned long item_index, Lxo3D_LightInfo *out_info)
{
    if (model == (const Lxo3D_Model *)0 || out_info == (Lxo3D_LightInfo *)0) { return LXO3D_E_BAD_ARG; }
    if (item_index >= model->item_count) { return LXO3D_E_BAD_ARG; }
    out_info->item_index = item_index;
    out_info->intensity = LXO3D_FIXED_ONE;
    out_info->color_r = LXO3D_FIXED_ONE;
    out_info->color_g = LXO3D_FIXED_ONE;
    out_info->color_b = LXO3D_FIXED_ONE;
    out_info->has_intensity = 0UL;
    out_info->has_color = 0UL;
    if (lxo3d_get_channel_value_by_names(model, item_index, "intensity", "radiance", "power", &out_info->intensity)) { out_info->has_intensity = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "color.R", "lightColor.R", "diffCol.R", &out_info->color_r)) { out_info->has_color = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "color.G", "lightColor.G", "diffCol.G", &out_info->color_g)) { out_info->has_color = 1UL; }
    if (lxo3d_get_channel_value_by_names(model, item_index, "color.B", "lightColor.B", "diffCol.B", &out_info->color_b)) { out_info->has_color = 1UL; }
    return LXO3D_OK;
}

int lxo3d_decode(const unsigned char *data, unsigned long size, Lxo3D_Model *model)
{
    unsigned long form_size;
    unsigned long form_end;
    unsigned long pos;
    unsigned long chunk_id;
    unsigned long chunk_size;
    unsigned long chunk_start;
    unsigned long chunk_end;
    int r;

    if (data == (const unsigned char *)0 || model == (Lxo3D_Model *)0) {
        return LXO3D_E_BAD_ARG;
    }
    lxo3d_clear(model);
    if (size < 12UL) {
        return LXO3D_E_TOO_SMALL;
    }
    if (lxo3d_be_u32(data) != LXO3D_FORM) {
        return LXO3D_E_NOT_FORM;
    }
    form_size = lxo3d_be_u32(data + 4UL);
    model->form_type = lxo3d_be_u32(data + 8UL);

    if (model->form_type != LXO3D_LXOB &&
        model->form_type != LXO3D_LXPR &&
        model->form_type != LXO3D_LXPM &&
        model->form_type != LXO3D_LXPE) {
        model->flags |= LXO3D_FLAG_UNSUPPORTED_FORM;
        return LXO3D_E_NOT_LXOB;
    }

    if (form_size == 0UL) {
        form_end = size;
    } else {
        if (form_size < 4UL) {
            return LXO3D_E_BAD_CHUNK;
        }
        if (form_size > size - 8UL) {
            return LXO3D_E_TRUNCATED;
        }
        form_end = 8UL + form_size;
        form_end = lxo3d_min_ul(form_end, size);
    }

    pos = 12UL;
    while (pos < form_end) {
        if (!lxo3d_range_ok(pos, 8UL, form_end)) {
            return LXO3D_E_TRUNCATED;
        }
        chunk_id = lxo3d_be_u32(data + pos);
        chunk_size = lxo3d_be_u32(data + pos + 4UL);
        chunk_start = pos + 8UL;
        if (chunk_size > form_end - chunk_start) {
            return LXO3D_E_TRUNCATED;
        }
        chunk_end = chunk_start + chunk_size;
        ++model->chunk_count;

        if (chunk_id == LXO3D_VRSN) {
            r = lxo3d_parse_vrsn(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_CHNM) {
            r = lxo3d_parse_chnm(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_ITEM) {
            r = lxo3d_parse_item(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_ENVL) {
            r = lxo3d_parse_envl(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_ACTN) {
            r = lxo3d_parse_actn(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_DATA) {
            r = lxo3d_parse_data_chunk(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_TAGS) {
            r = lxo3d_parse_tags(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_LAYR) {
            r = lxo3d_parse_layr(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_PNTS) {
            r = lxo3d_parse_pnts(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_POLS) {
            r = lxo3d_parse_pols(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_PTAG) {
            r = lxo3d_parse_ptag(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_VMAP) {
            r = lxo3d_parse_vmap(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_VMAD) {
            r = lxo3d_parse_vmad(data, chunk_start, chunk_end, model);
        } else if (chunk_id == LXO3D_BBOX || chunk_id == LXO3D_VMED) {
            r = LXO3D_OK;
        } else {
            r = LXO3D_OK;
            ++model->unknown_chunk_count;
            model->flags |= LXO3D_FLAG_UNKNOWN_CHUNKS;
        }

        if (r != LXO3D_OK) {
            return r;
        }

        pos = chunk_end;
        if ((chunk_size & 1UL) != 0UL) {
            if (pos >= form_end) {
                return LXO3D_E_TRUNCATED;
            }
            ++pos;
        }
    }

    return LXO3D_OK;
}

int lxo3d_emit_triangles(const Lxo3D_Model *model, Lxo3D_TriangleCB cb, void *user)
{
    unsigned long p;
    unsigned long j;
    unsigned long a;
    unsigned long b;
    unsigned long c;
    const Lxo3D_Poly *poly;
    int r;

    if (model == (const Lxo3D_Model *)0 || cb == (Lxo3D_TriangleCB)0) {
        return LXO3D_E_BAD_ARG;
    }
    for (p = 0UL; p < model->poly_count; ++p) {
        poly = &model->polys[p];
        if (poly->type_id == LXO3D_ID_FACE || poly->type_id == LXO3D_ID_SUBD || poly->type_id == LXO3D_ID_PSUB) {
            if (poly->index_count >= 3UL &&
                poly->first_index + poly->index_count <= model->poly_index_count) {
                a = model->poly_indices[poly->first_index];
                j = 1UL;
                while (j + 1UL < poly->index_count) {
                    b = model->poly_indices[poly->first_index + j];
                    c = model->poly_indices[poly->first_index + j + 1UL];
                    r = cb(user, p, a, b, c, poly->material_tag);
                    if (r != 0) {
                        return r;
                    }
                    ++j;
                }
            }
        }
    }
    return LXO3D_OK;
}

const char *lxo3d_result_string(int code)
{
    switch (code) {
    case LXO3D_OK:
        return "OK";
    case LXO3D_E_BAD_ARG:
        return "bad argument";
    case LXO3D_E_TOO_SMALL:
        return "input too small";
    case LXO3D_E_NOT_FORM:
        return "not an IFF FORM file";
    case LXO3D_E_NOT_LXOB:
        return "not an LXOB/LXPR/LXPM/LXPE form";
    case LXO3D_E_TRUNCATED:
        return "truncated file or chunk";
    case LXO3D_E_BAD_CHUNK:
        return "bad chunk layout";
    case LXO3D_E_OVERFLOW:
        return "output capacity overflow";
    default:
        return "unknown error";
    }
}
