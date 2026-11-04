/* Vendored Source format submodule; included in legacy core to share C89 private parsers.
   No duplicated decoder definitions. See docs/MODULAR_V1_0_4.md. */
static void hl2m_vmt_skip_comments(const hl2m_u8 *data, unsigned int size, unsigned int *pos)
{
    int again;
    again = 1;
    while (again) {
        again = 0;
        while (*pos < size && hl2m_is_space((char)data[*pos])) ++(*pos);
        if (*pos + 1u < size && data[*pos] == '/' && data[*pos + 1u] == '/') {
            *pos += 2u;
            while (*pos < size && data[*pos] != '\n') ++(*pos);
            again = 1;
        } else if (*pos + 1u < size && data[*pos] == '/' && data[*pos + 1u] == '*') {
            *pos += 2u;
            while (*pos + 1u < size && !(data[*pos] == '*' && data[*pos + 1u] == '/')) ++(*pos);
            if (*pos + 1u < size) *pos += 2u;
            again = 1;
        }
    }
}

static int hl2m_vmt_token(const hl2m_u8 *data, unsigned int size, unsigned int *pos, char *out_token, unsigned int out_size, int *out_brace)
{
    unsigned int i;
    char quote;
    hl2m_vmt_skip_comments(data, size, pos);
    if (*pos >= size) return 0;
    *out_brace = 0;
    if (data[*pos] == '{') {
        ++(*pos);
        *out_brace = 1;
        if (out_size != 0u) { out_token[0] = '{'; if (out_size > 1u) out_token[1] = 0; }
        return 1;
    }
    if (data[*pos] == '}') {
        ++(*pos);
        *out_brace = -1;
        if (out_size != 0u) { out_token[0] = '}'; if (out_size > 1u) out_token[1] = 0; }
        return 1;
    }
    i = 0u;
    if (data[*pos] == '"') {
        quote = '"';
        ++(*pos);
        while (*pos < size && data[*pos] != (hl2m_u8)quote) {
            if (i + 1u < out_size) out_token[i++] = (char)data[*pos];
            ++(*pos);
        }
        if (*pos < size && data[*pos] == (hl2m_u8)quote) ++(*pos);
    } else {
        while (*pos < size && !hl2m_is_space((char)data[*pos]) && data[*pos] != '{' && data[*pos] != '}') {
            if (i + 1u < out_size) out_token[i++] = (char)data[*pos];
            ++(*pos);
        }
    }
    if (out_size != 0u) out_token[i] = 0;
    return 1;
}

int hl2m_decode_vmt(const hl2m_u8 *data, unsigned int size, hl2m_vmt *out_vmt)
{
    unsigned int pos;
    char tok[HL2M_TOKEN_MAX];
    char val[HL2M_TOKEN_MAX];
    int brace;
    int brace2;
    int depth;
    int have_shader;
    int r;

    if (data == 0 || out_vmt == 0) return HL2M_ERR_NULL;
    hl2m_clear_vmt(out_vmt);
    pos = 0u;
    depth = 0;
    have_shader = 0;
    while (hl2m_vmt_token(data, size, &pos, tok, HL2M_TOKEN_MAX, &brace)) {
        if (brace == 1) { ++depth; continue; }
        if (brace == -1) { if (depth > 0) --depth; continue; }
        if (!have_shader) {
            hl2m_copy_token(out_vmt->shader, HL2M_NAME_MAX, tok);
            have_shader = 1;
            continue;
        }
        if (out_vmt->param_count >= HL2M_MAX_VMT_PARAMS) return HL2M_ERR_LIMIT;
        r = hl2m_vmt_token(data, size, &pos, val, HL2M_TOKEN_MAX, &brace2);
        /* Ignore corpus trailing orphan garbage after a closed KeyValues block.
           Inside an open block, a missing value remains a real parse error. */
        if (!r) {
            if (depth == 0) break;
            return HL2M_ERR_PARSE;
        }
        out_vmt->params[out_vmt->param_count].depth = depth;
        hl2m_copy_token(out_vmt->params[out_vmt->param_count].key, HL2M_NAME_MAX, tok);
        if (brace2 == 1) {
            hl2m_copy_token(out_vmt->params[out_vmt->param_count].value, HL2M_VALUE_MAX, "{");
            ++depth;
        } else if (brace2 == -1) {
            hl2m_copy_token(out_vmt->params[out_vmt->param_count].value, HL2M_VALUE_MAX, "}");
            if (depth > 0) --depth;
        } else {
            hl2m_copy_token(out_vmt->params[out_vmt->param_count].value, HL2M_VALUE_MAX, val);
        }
        ++out_vmt->param_count;
    }
    return have_shader ? HL2M_OK : HL2M_ERR_PARSE;
}


static int hl2m_text_eq_ci(const char *a, const char *b);

static int hl2m_key_eq_ci(const char *a, const char *b)
{
    if (a != 0 && *a == '$') ++a;
    if (b != 0 && *b == '$') ++b;
    return hl2m_text_eq_ci(a, b);
}

static int hl2m_value_truthy(const char *value)
{
    if (value == 0) return 0;
    if (hl2m_text_eq_ci(value, "1")) return 1;
    if (hl2m_text_eq_ci(value, "true")) return 1;
    if (hl2m_text_eq_ci(value, "yes")) return 1;
    return 0;
}

int hl2m_vmt_find_param(const hl2m_vmt *vmt, const char *key, char *out_value, unsigned int out_size)
{
    int i;
    if (vmt == 0 || key == 0) return HL2M_ERR_NULL;
    if (out_value != 0 && out_size != 0u) out_value[0] = 0;
    for (i = vmt->param_count - 1; i >= 0; --i) {
        if (hl2m_key_eq_ci(vmt->params[i].key, key)) {
            if (out_value != 0) hl2m_copy_token(out_value, out_size, vmt->params[i].value);
            return HL2M_OK;
        }
    }
    return HL2M_ERR_PARSE;
}

int hl2m_vmt_to_material_desc(const hl2m_vmt *vmt, hl2m_material_desc *out_desc)
{
    char tmp[HL2M_VALUE_MAX];
    if (vmt == 0 || out_desc == 0) return HL2M_ERR_NULL;
    hl2m_clear_material_desc(out_desc);
    hl2m_copy_token(out_desc->shader, HL2M_NAME_MAX, vmt->shader);
    if (hl2m_text_eq_ci(vmt->shader, "VertexLitGeneric")) out_desc->flags |= HL2M_VMT_FLAG_VERTEXLIT;
    if (hl2m_text_eq_ci(vmt->shader, "UnlitGeneric")) out_desc->flags |= HL2M_VMT_FLAG_UNLIT;
    if (hl2m_text_eq_ci(vmt->shader, "LightmappedGeneric")) out_desc->flags |= HL2M_VMT_FLAG_LIGHTMAPPED;
    if (hl2m_vmt_find_param(vmt, "$basetexture", out_desc->base_texture, HL2M_PATH_MAX) != HL2M_OK) {
        (void)hl2m_vmt_find_param(vmt, "basetexture", out_desc->base_texture, HL2M_PATH_MAX);
    }
    (void)hl2m_vmt_find_param(vmt, "$bumpmap", out_desc->bumpmap, HL2M_PATH_MAX);
    (void)hl2m_vmt_find_param(vmt, "$surfaceprop", out_desc->surfaceprop, HL2M_NAME_MAX);
    (void)hl2m_vmt_find_param(vmt, "$detail", out_desc->detail, HL2M_PATH_MAX);
    (void)hl2m_vmt_find_param(vmt, "$envmap", out_desc->envmap, HL2M_PATH_MAX);
    if (hl2m_vmt_find_param(vmt, "$translucent", tmp, (unsigned int)sizeof(tmp)) == HL2M_OK && hl2m_value_truthy(tmp)) out_desc->flags |= HL2M_VMT_FLAG_TRANSLUCENT;
    if (hl2m_vmt_find_param(vmt, "$alphatest", tmp, (unsigned int)sizeof(tmp)) == HL2M_OK && hl2m_value_truthy(tmp)) out_desc->flags |= HL2M_VMT_FLAG_ALPHATEST;
    if (hl2m_vmt_find_param(vmt, "$nocull", tmp, (unsigned int)sizeof(tmp)) == HL2M_OK && hl2m_value_truthy(tmp)) out_desc->flags |= HL2M_VMT_FLAG_NOCULL;
    if (hl2m_vmt_find_param(vmt, "$selfillum", tmp, (unsigned int)sizeof(tmp)) == HL2M_OK && hl2m_value_truthy(tmp)) out_desc->flags |= HL2M_VMT_FLAG_SELFILLUM;
    if (hl2m_vmt_find_param(vmt, "$additive", tmp, (unsigned int)sizeof(tmp)) == HL2M_OK && hl2m_value_truthy(tmp)) out_desc->flags |= HL2M_VMT_FLAG_ADDITIVE;
    return HL2M_OK;
}


