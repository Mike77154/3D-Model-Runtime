/* QC decoder, command IR, writer, basic recipe; core text helpers. */
static int hl2m_qc_kind_from_name(const char *name)
{
    const char *p;
    p = name;
    if (*p == '$') ++p;
    if (hl2m_text_eq_ci(p, "modelname")) return HL2M_QC_MODELNAME;
    if (hl2m_text_eq_ci(p, "body")) return HL2M_QC_BODY;
    if (hl2m_text_eq_ci(p, "model")) return HL2M_QC_MODEL;
    if (hl2m_text_eq_ci(p, "bodygroup")) return HL2M_QC_BODYGROUP;
    if (hl2m_text_eq_ci(p, "studio")) return HL2M_QC_STUDIO;
    if (hl2m_text_eq_ci(p, "blank")) return HL2M_QC_BLANK;
    if (hl2m_text_eq_ci(p, "cdmaterials")) return HL2M_QC_CDMATERIALS;
    if (hl2m_text_eq_ci(p, "sequence")) return HL2M_QC_SEQUENCE;
    if (hl2m_text_eq_ci(p, "animation")) return HL2M_QC_ANIMATION;
    if (hl2m_text_eq_ci(p, "collisionmodel")) return HL2M_QC_COLLISIONMODEL;
    if (hl2m_text_eq_ci(p, "collisionjoints")) return HL2M_QC_COLLISIONJOINTS;
    if (hl2m_text_eq_ci(p, "staticprop")) return HL2M_QC_STATICPROP;
    if (hl2m_text_eq_ci(p, "surfaceprop")) return HL2M_QC_SURFACEPROP;
    if (hl2m_text_eq_ci(p, "contents")) return HL2M_QC_CONTENTS;
    if (hl2m_text_eq_ci(p, "scale")) return HL2M_QC_SCALE;
    if (hl2m_text_eq_ci(p, "origin")) return HL2M_QC_ORIGIN;
    if (hl2m_text_eq_ci(p, "rotate")) return HL2M_QC_ROTATE;
    if (hl2m_text_eq_ci(p, "texturegroup")) return HL2M_QC_TEXTUREGROUP;
    if (hl2m_text_eq_ci(p, "keyvalues")) return HL2M_QC_KEYVALUES;
    if (hl2m_text_eq_ci(p, "includemodel")) return HL2M_QC_INCLUDEMODEL;
    if (hl2m_text_eq_ci(p, "attachment")) return HL2M_QC_ATTACHMENT;
    if (hl2m_text_eq_ci(p, "bonemerge")) return HL2M_QC_BONEMERGE;
    if (hl2m_text_eq_ci(p, "hitboxset")) return HL2M_QC_HITBOXSET;
    if (hl2m_text_eq_ci(p, "hbox")) return HL2M_QC_HBOX;
    if (hl2m_text_eq_ci(p, "definebone")) return HL2M_QC_DEFINEBONE;
    if (hl2m_text_eq_ci(p, "lod")) return HL2M_QC_LOD;
    if (hl2m_text_eq_ci(p, "renamematerial")) return HL2M_QC_RENAMEMATERIAL;
    if (hl2m_text_eq_ci(p, "eyeposition")) return HL2M_QC_EYEPOSITION;
    if (hl2m_text_eq_ci(p, "illumposition")) return HL2M_QC_ILLUMPOSITION;
    if (hl2m_text_eq_ci(p, "poseparameter")) return HL2M_QC_POSEPARAMETER;
    if (hl2m_text_eq_ci(p, "weightlist")) return HL2M_QC_WEIGHTLIST;
    if (hl2m_text_eq_ci(p, "ikchain")) return HL2M_QC_IKCHAIN;
    if (hl2m_text_eq_ci(p, "ikrule")) return HL2M_QC_IKRULE;
    if (hl2m_text_eq_ci(p, "bbox")) return HL2M_QC_BBOX;
    if (hl2m_text_eq_ci(p, "cbox")) return HL2M_QC_CBOX;
    if (hl2m_text_eq_ci(p, "mostlyopaque")) return HL2M_QC_MOSTLYOPAQUE;
    if (hl2m_text_eq_ci(p, "opaque")) return HL2M_QC_OPAQUE;
    if (hl2m_text_eq_ci(p, "translucent")) return HL2M_QC_TRANSLUCENT;
    if (hl2m_text_eq_ci(p, "ambientboost")) return HL2M_QC_AMBIENTBOOST;
    if (hl2m_text_eq_ci(p, "maxverts")) return HL2M_QC_MAXVERTS;
    if (hl2m_text_eq_ci(p, "maxbones")) return HL2M_QC_MAXBONES;
    if (hl2m_text_eq_ci(p, "jigglebone")) return HL2M_QC_JIGGLEBONE;
    if (hl2m_text_eq_ci(p, "proceduralbones")) return HL2M_QC_PROCEDURALBONES;
    if (hl2m_text_eq_ci(p, "alwayscollapse")) return HL2M_QC_ALWAYS_COLLAPSE;
    if (hl2m_text_eq_ci(p, "collapsebones")) return HL2M_QC_COLLAPSEBONES;
    if (hl2m_text_eq_ci(p, "keepbone")) return HL2M_QC_KEEPBONE;
    if (hl2m_text_eq_ci(p, "skipboneinbbox")) return HL2M_QC_SKIPBONEINBBOX;
    if (hl2m_text_eq_ci(p, "texrendermode")) return HL2M_QC_TEXRENDERMODE;
    if (hl2m_text_eq_ci(p, "jointmerge")) return HL2M_QC_JOINTMERGE;
    if (hl2m_text_eq_ci(p, "replacemodel")) return HL2M_QC_REPLACEMODEL;
    if (hl2m_text_eq_ci(p, "replacematerial")) return HL2M_QC_REPLACEMATERIAL;
    if (hl2m_text_eq_ci(p, "shadowlod")) return HL2M_QC_SHADOWLOD;
    if (hl2m_text_eq_ci(p, "event")) return HL2M_QC_EVENT;
    if (hl2m_text_eq_ci(p, "fps")) return HL2M_QC_FPS;
    if (hl2m_text_eq_ci(p, "flexfile")) return HL2M_QC_FLEXFILE;
    if (hl2m_text_eq_ci(p, "flexcontroller")) return HL2M_QC_FLEXCONTROLLER;
    if (hl2m_text_eq_ci(p, "flexrule")) return HL2M_QC_FLEXRULE;
    if (hl2m_text_eq_ci(p, "flexcontrollerui")) return HL2M_QC_FLEXCONTROLLERUI;
    if (hl2m_text_eq_ci(p, "flex")) return HL2M_QC_FLEX;
    if (hl2m_text_eq_ci(p, "flexpair")) return HL2M_QC_FLEXPAIR;
    return HL2M_QC_UNKNOWN;
}


static void hl2m_qc_strip_comments(char *line, int *block_comment)
{
    unsigned int r;
    unsigned int w;
    char quote;
    r = 0u;
    w = 0u;
    quote = 0;
    while (line[r] != 0) {
        if (*block_comment) {
            if (line[r] == '*' && line[r + 1u] == '/') {
                *block_comment = 0;
                r += 2u;
            } else {
                ++r;
            }
            continue;
        }
        if (quote != 0) {
            line[w++] = line[r];
            if (line[r] == quote) quote = 0;
            ++r;
            continue;
        }
        if (line[r] == '"') {
            quote = '"';
            line[w++] = line[r++];
            continue;
        }
        if (line[r] == '/' && line[r + 1u] == '/') {
            break;
        }
        if (line[r] == '/' && line[r + 1u] == '*') {
            *block_comment = 1;
            r += 2u;
            continue;
        }
        line[w++] = line[r++];
    }
    line[w] = 0;
}

static int hl2m_qc_next_token(const char **pp, char *out_text, unsigned int out_size, int *out_brace)
{
    const char *p;
    unsigned int i;
    char quote;
    p = hl2m_skip_spaces(*pp);
    *out_brace = 0;
    if (*p == 0) {
        if (out_size != 0u) out_text[0] = 0;
        *pp = p;
        return 0;
    }
    if (*p == '{') {
        if (out_size != 0u) {
            out_text[0] = '{';
            if (out_size > 1u) out_text[1] = 0;
        }
        *out_brace = 1;
        *pp = p + 1;
        return 1;
    }
    if (*p == '}') {
        if (out_size != 0u) {
            out_text[0] = '}';
            if (out_size > 1u) out_text[1] = 0;
        }
        *out_brace = -1;
        *pp = p + 1;
        return 1;
    }
    i = 0u;
    if (*p == '"') {
        quote = '"';
        ++p;
        while (*p != 0 && *p != quote) {
            if (i + 1u < out_size) out_text[i++] = *p;
            ++p;
        }
        if (*p == quote) ++p;
    } else {
        while (*p != 0 && !hl2m_is_space(*p) && *p != '{' && *p != '}') {
            if (i + 1u < out_size) out_text[i++] = *p;
            ++p;
        }
    }
    if (out_size != 0u) out_text[i] = 0;
    *pp = p;
    return 1;
}

static void hl2m_qc_trim_prefix_dollar(char *dst, unsigned int dst_size, const char *src)
{
    if (src != 0 && src[0] == '$') ++src;
    hl2m_copy_token(dst, dst_size, src);
}

static int hl2m_qc_add_body(hl2m_qc *out_qc, int command_index, const char *name, const char *source)
{
    int i;
    if (out_qc->body_count >= HL2M_MAX_QC_BODIES) return HL2M_ERR_LIMIT;
    i = out_qc->body_count;
    out_qc->bodies[i].command_index = command_index;
    hl2m_copy_token(out_qc->bodies[i].name, HL2M_NAME_MAX, name);
    hl2m_copy_token(out_qc->bodies[i].source, HL2M_PATH_MAX, source);
    ++out_qc->body_count;
    return HL2M_OK;
}

static int hl2m_qc_add_sequence(hl2m_qc *out_qc, int command_index, const char *name, const char *source)
{
    int i;
    if (out_qc->sequence_count >= HL2M_MAX_QC_SEQUENCES) return HL2M_ERR_LIMIT;
    i = out_qc->sequence_count;
    out_qc->sequences[i].command_index = command_index;
    hl2m_copy_token(out_qc->sequences[i].name, HL2M_NAME_MAX, name);
    hl2m_copy_token(out_qc->sequences[i].source, HL2M_PATH_MAX, source);
    ++out_qc->sequence_count;
    return HL2M_OK;
}


static int hl2m_parse_fixed_text(const char *text, hl2m_fixed *out_v)
{
    const char *p;
    int r;
    p = text;
    r = hl2m_parse_fixed_ptr(&p, out_v);
    if (r != HL2M_OK) return r;
    p = hl2m_skip_spaces(p);
    return (*p == 0) ? HL2M_OK : HL2M_ERR_PARSE;
}

static int hl2m_parse_int_text(const char *text, int *out_v)
{
    const char *p;
    int r;
    p = text;
    r = hl2m_parse_int_ptr(&p, out_v);
    if (r != HL2M_OK) return r;
    p = hl2m_skip_spaces(p);
    return (*p == 0) ? HL2M_OK : HL2M_ERR_PARSE;
}

static int hl2m_qc_add_lod(hl2m_qc *out_qc, int command_index, const char *switch_point, const char *source)
{
    int i;
    if (out_qc->lod_count >= HL2M_MAX_QC_LODS) return HL2M_ERR_LIMIT;
    i = out_qc->lod_count;
    out_qc->lods[i].command_index = command_index;
    if (hl2m_parse_fixed_text(switch_point, &out_qc->lods[i].switch_point) != HL2M_OK) out_qc->lods[i].switch_point = 0;
    if (source != 0) hl2m_copy_token(out_qc->lods[i].source, HL2M_PATH_MAX, source);
    ++out_qc->lod_count;
    return HL2M_OK;
}

static int hl2m_qc_add_attachment(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    int a;
    if (cmd->argc < 5) return HL2M_OK;
    if (out_qc->attachment_count >= HL2M_MAX_QC_ATTACHMENTS) return HL2M_ERR_LIMIT;
    i = out_qc->attachment_count;
    out_qc->attachments[i].command_index = command_index;
    hl2m_copy_token(out_qc->attachments[i].name, HL2M_NAME_MAX, cmd->args[0]);
    hl2m_copy_token(out_qc->attachments[i].bone, HL2M_NAME_MAX, cmd->args[1]);
    (void)hl2m_parse_fixed_text(cmd->args[2], &out_qc->attachments[i].pos.x);
    (void)hl2m_parse_fixed_text(cmd->args[3], &out_qc->attachments[i].pos.y);
    (void)hl2m_parse_fixed_text(cmd->args[4], &out_qc->attachments[i].pos.z);
    for (a = 5; a + 3 < cmd->argc; ++a) {
        if (hl2m_text_eq_ci(cmd->args[a], "rotate")) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->attachments[i].rot.x);
            (void)hl2m_parse_fixed_text(cmd->args[a + 2], &out_qc->attachments[i].rot.y);
            (void)hl2m_parse_fixed_text(cmd->args[a + 3], &out_qc->attachments[i].rot.z);
            out_qc->attachments[i].has_rotate = 1;
        }
    }
    ++out_qc->attachment_count;
    return HL2M_OK;
}

static int hl2m_qc_add_hitbox(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    if (cmd->argc < 8) return HL2M_OK;
    if (out_qc->qc_hitbox_count >= HL2M_MAX_QC_HITBOXES) return HL2M_ERR_LIMIT;
    i = out_qc->qc_hitbox_count;
    out_qc->qc_hitboxes[i].command_index = command_index;
    (void)hl2m_parse_int_text(cmd->args[0], &out_qc->qc_hitboxes[i].group);
    hl2m_copy_token(out_qc->qc_hitboxes[i].bone, HL2M_NAME_MAX, cmd->args[1]);
    (void)hl2m_parse_fixed_text(cmd->args[2], &out_qc->qc_hitboxes[i].bbmin.x);
    (void)hl2m_parse_fixed_text(cmd->args[3], &out_qc->qc_hitboxes[i].bbmin.y);
    (void)hl2m_parse_fixed_text(cmd->args[4], &out_qc->qc_hitboxes[i].bbmin.z);
    (void)hl2m_parse_fixed_text(cmd->args[5], &out_qc->qc_hitboxes[i].bbmax.x);
    (void)hl2m_parse_fixed_text(cmd->args[6], &out_qc->qc_hitboxes[i].bbmax.y);
    (void)hl2m_parse_fixed_text(cmd->args[7], &out_qc->qc_hitboxes[i].bbmax.z);
    if (cmd->argc > 8) hl2m_copy_token(out_qc->qc_hitboxes[i].name, HL2M_NAME_MAX, cmd->args[8]);
    ++out_qc->qc_hitbox_count;
    return HL2M_OK;
}

static int hl2m_qc_add_definebone(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    if (cmd->argc < 8) return HL2M_OK;
    if (out_qc->definebone_count >= HL2M_MAX_QC_DEFINEBONES) return HL2M_ERR_LIMIT;
    i = out_qc->definebone_count;
    out_qc->definebones[i].command_index = command_index;
    hl2m_copy_token(out_qc->definebones[i].name, HL2M_NAME_MAX, cmd->args[0]);
    hl2m_copy_token(out_qc->definebones[i].parent, HL2M_NAME_MAX, cmd->args[1]);
    (void)hl2m_parse_fixed_text(cmd->args[2], &out_qc->definebones[i].pos.x);
    (void)hl2m_parse_fixed_text(cmd->args[3], &out_qc->definebones[i].pos.y);
    (void)hl2m_parse_fixed_text(cmd->args[4], &out_qc->definebones[i].pos.z);
    (void)hl2m_parse_fixed_text(cmd->args[5], &out_qc->definebones[i].rot.x);
    (void)hl2m_parse_fixed_text(cmd->args[6], &out_qc->definebones[i].rot.y);
    (void)hl2m_parse_fixed_text(cmd->args[7], &out_qc->definebones[i].rot.z);
    if (cmd->argc >= 14) {
        (void)hl2m_parse_fixed_text(cmd->args[8], &out_qc->definebones[i].fixup_pos.x);
        (void)hl2m_parse_fixed_text(cmd->args[9], &out_qc->definebones[i].fixup_pos.y);
        (void)hl2m_parse_fixed_text(cmd->args[10], &out_qc->definebones[i].fixup_pos.z);
        (void)hl2m_parse_fixed_text(cmd->args[11], &out_qc->definebones[i].fixup_rot.x);
        (void)hl2m_parse_fixed_text(cmd->args[12], &out_qc->definebones[i].fixup_rot.y);
        (void)hl2m_parse_fixed_text(cmd->args[13], &out_qc->definebones[i].fixup_rot.z);
    }
    ++out_qc->definebone_count;
    return HL2M_OK;
}

static int hl2m_qc_add_poseparameter(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    int a;
    if (cmd->argc < 3) return HL2M_OK;
    if (out_qc->poseparameter_count >= HL2M_MAX_QC_POSEPARAMS) return HL2M_ERR_LIMIT;
    i = out_qc->poseparameter_count;
    out_qc->poseparameters[i].command_index = command_index;
    hl2m_copy_token(out_qc->poseparameters[i].name, HL2M_NAME_MAX, cmd->args[0]);
    (void)hl2m_parse_fixed_text(cmd->args[1], &out_qc->poseparameters[i].min_value);
    (void)hl2m_parse_fixed_text(cmd->args[2], &out_qc->poseparameters[i].max_value);
    for (a = 3; a + 1 < cmd->argc; ++a) {
        if (hl2m_text_eq_ci(cmd->args[a], "loop")) {
            (void)hl2m_parse_int_text(cmd->args[a + 1], &out_qc->poseparameters[i].loop_value);
        }
    }
    ++out_qc->poseparameter_count;
    return HL2M_OK;
}


static int hl2m_qc_add_material_replace(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    if (cmd->argc < 2) return HL2M_OK;
    if (out_qc->material_replace_count >= HL2M_MAX_QC_MATERIAL_REPLACES) return HL2M_ERR_LIMIT;
    i = out_qc->material_replace_count;
    out_qc->material_replaces[i].command_index = command_index;
    out_qc->material_replaces[i].lod_depth = cmd->depth;
    hl2m_copy_token(out_qc->material_replaces[i].from_material, HL2M_PATH_MAX, cmd->args[0]);
    hl2m_copy_token(out_qc->material_replaces[i].to_material, HL2M_PATH_MAX, cmd->args[1]);
    ++out_qc->material_replace_count;
    return HL2M_OK;
}

static int hl2m_qc_add_collision(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd, int joints)
{
    int i;
    if (cmd->argc < 1) return HL2M_OK;
    if (out_qc->collision_count >= HL2M_MAX_QC_COLLISIONS) return HL2M_ERR_LIMIT;
    i = out_qc->collision_count;
    out_qc->collisions[i].command_index = command_index;
    out_qc->collisions[i].joints = joints;
    hl2m_copy_token(out_qc->collisions[i].source, HL2M_PATH_MAX, cmd->args[0]);
    ++out_qc->collision_count;
    return HL2M_OK;
}


static int hl2m_qc_find_flexcontroller(const hl2m_qc *qc, const char *name)
{
    int i;
    if (qc == 0 || name == 0) return -1;
    for (i = 0; i < qc->flexcontroller_count; ++i) {
        if (hl2m_text_eq_ci(qc->flexcontrollers[i].name, name)) return i;
    }
    return -1;
}

static int hl2m_qc_find_ikchain(const hl2m_qc *qc, const char *name)
{
    int i;
    if (qc == 0 || name == 0) return -1;
    for (i = 0; i < qc->ikchain_count; ++i) {
        if (hl2m_text_eq_ci(qc->ikchains[i].name, name)) return i;
    }
    return -1;
}

static int hl2m_qc_add_flexcontroller(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    int a;
    if (cmd->argc < 1) return HL2M_OK;
    if (out_qc->flexcontroller_count >= HL2M_MAX_QC_FLEXCONTROLLERS) return HL2M_ERR_LIMIT;
    i = out_qc->flexcontroller_count;
    out_qc->flexcontrollers[i].command_index = command_index;
    if (cmd->argc >= 1) hl2m_copy_token(out_qc->flexcontrollers[i].group, HL2M_NAME_MAX, cmd->args[0]);
    if (cmd->argc >= 5 && hl2m_text_eq_ci(cmd->args[1], "range")) {
        (void)hl2m_parse_fixed_text(cmd->args[2], &out_qc->flexcontrollers[i].min_value);
        (void)hl2m_parse_fixed_text(cmd->args[3], &out_qc->flexcontrollers[i].max_value);
        hl2m_copy_token(out_qc->flexcontrollers[i].name, HL2M_NAME_MAX, cmd->args[4]);
    } else {
        out_qc->flexcontrollers[i].min_value = 0;
        out_qc->flexcontrollers[i].max_value = HL2M_FIXED_ONE;
        hl2m_copy_token(out_qc->flexcontrollers[i].name, HL2M_NAME_MAX, cmd->args[cmd->argc - 1]);
        for (a = 1; a + 2 < cmd->argc; ++a) {
            if (hl2m_text_eq_ci(cmd->args[a], "range")) {
                (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->flexcontrollers[i].min_value);
                (void)hl2m_parse_fixed_text(cmd->args[a + 2], &out_qc->flexcontrollers[i].max_value);
            }
        }
    }
    ++out_qc->flexcontroller_count;
    return HL2M_OK;
}

static int hl2m_qc_add_flexrule(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    int a;
    unsigned int w;
    if (cmd->argc < 1) return HL2M_OK;
    if (out_qc->flexrule_count >= HL2M_MAX_QC_FLEXRULES) return HL2M_ERR_LIMIT;
    i = out_qc->flexrule_count;
    out_qc->flexrules[i].command_index = command_index;
    hl2m_copy_token(out_qc->flexrules[i].flex_name, HL2M_NAME_MAX, cmd->args[0]);
    out_qc->flexrules[i].controller_index = hl2m_qc_find_flexcontroller(out_qc, cmd->args[0]);
    w = 0u;
    for (a = 1; a < cmd->argc; ++a) {
        const char *q;
        if (a > 1 && w + 1u < HL2M_VALUE_MAX) out_qc->flexrules[i].expression[w++] = ' ';
        q = cmd->args[a];
        while (*q != 0 && w + 1u < HL2M_VALUE_MAX) out_qc->flexrules[i].expression[w++] = *q++;
        if (out_qc->flexrules[i].controller_index < 0) {
            int ci;
            ci = hl2m_qc_find_flexcontroller(out_qc, cmd->args[a]);
            if (ci >= 0) out_qc->flexrules[i].controller_index = ci;
        }
    }
    out_qc->flexrules[i].expression[w] = 0;
    ++out_qc->flexrule_count;
    return HL2M_OK;
}

static int hl2m_qc_add_sequence_event(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd, int current_sequence)
{
    int i;
    if (cmd->argc < 2) return HL2M_OK;
    if (out_qc->sequence_event_count >= HL2M_MAX_QC_SEQUENCE_EVENTS) return HL2M_ERR_LIMIT;
    i = out_qc->sequence_event_count;
    out_qc->sequence_events[i].command_index = command_index;
    out_qc->sequence_events[i].sequence_index = current_sequence;
    if (current_sequence >= 0 && current_sequence < out_qc->sequence_count) {
        hl2m_copy_token(out_qc->sequence_events[i].sequence_name, HL2M_NAME_MAX, out_qc->sequences[current_sequence].name);
    }
    (void)hl2m_parse_int_text(cmd->args[0], &out_qc->sequence_events[i].event_id);
    (void)hl2m_parse_int_text(cmd->args[1], &out_qc->sequence_events[i].frame);
    out_qc->sequence_events[i].cycle = out_qc->sequence_events[i].frame * HL2M_FIXED_ONE;
    if (cmd->argc >= 3) hl2m_copy_token(out_qc->sequence_events[i].options, (unsigned int)sizeof(out_qc->sequence_events[i].options), cmd->args[2]);
    if (cmd->argc >= 4) hl2m_copy_token(out_qc->sequence_events[i].event_name, HL2M_NAME_MAX, cmd->args[3]);
    ++out_qc->sequence_event_count;
    return HL2M_OK;
}

static int hl2m_qc_add_ikchain(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd)
{
    int i;
    int a;
    if (cmd->argc < 2) return HL2M_OK;
    if (out_qc->ikchain_count >= HL2M_MAX_QC_IKCHAINS) return HL2M_ERR_LIMIT;
    i = out_qc->ikchain_count;
    out_qc->ikchains[i].command_index = command_index;
    hl2m_copy_token(out_qc->ikchains[i].name, HL2M_NAME_MAX, cmd->args[0]);
    hl2m_copy_token(out_qc->ikchains[i].bone, HL2M_NAME_MAX, cmd->args[1]);
    for (a = 2; a + 3 < cmd->argc; ++a) {
        if (hl2m_text_eq_ci(cmd->args[a], "knee")) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikchains[i].knee.x);
            (void)hl2m_parse_fixed_text(cmd->args[a + 2], &out_qc->ikchains[i].knee.y);
            (void)hl2m_parse_fixed_text(cmd->args[a + 3], &out_qc->ikchains[i].knee.z);
        }
    }
    ++out_qc->ikchain_count;
    return HL2M_OK;
}

static int hl2m_qc_ik_type_from_text(const char *text)
{
    if (text == 0) return 0;
    if (hl2m_text_eq_ci(text, "self")) return 1;
    if (hl2m_text_eq_ci(text, "world")) return 2;
    if (hl2m_text_eq_ci(text, "ground")) return 3;
    if (hl2m_text_eq_ci(text, "footstep")) return 3;
    if (hl2m_text_eq_ci(text, "release")) return 4;
    if (hl2m_text_eq_ci(text, "attachment")) return 5;
    if (hl2m_text_eq_ci(text, "unlatch")) return 6;
    return 0;
}

static int hl2m_qc_add_ikrule(hl2m_qc *out_qc, int command_index, const hl2m_qc_command *cmd, int current_sequence)
{
    int i;
    int a;
    if (cmd->argc < 2) return HL2M_OK;
    if (out_qc->ikrule_count >= HL2M_MAX_QC_IKRULES) return HL2M_ERR_LIMIT;
    i = out_qc->ikrule_count;
    out_qc->ikrules[i].command_index = command_index;
    out_qc->ikrules[i].sequence_index = current_sequence;
    hl2m_copy_token(out_qc->ikrules[i].name, HL2M_NAME_MAX, cmd->args[0]);
    out_qc->ikrules[i].type = hl2m_qc_ik_type_from_text(cmd->args[1]);
    out_qc->ikrules[i].start = 0;
    out_qc->ikrules[i].peak = 0;
    out_qc->ikrules[i].tail = HL2M_FIXED_ONE;
    out_qc->ikrules[i].end = HL2M_FIXED_ONE;
    for (a = 2; a < cmd->argc; ++a) {
        if (hl2m_text_eq_ci(cmd->args[a], "chain") && a + 1 < cmd->argc) {
            hl2m_copy_token(out_qc->ikrules[i].chain, HL2M_NAME_MAX, cmd->args[a + 1]);
        } else if (hl2m_text_eq_ci(cmd->args[a], "bone") && a + 1 < cmd->argc) {
            hl2m_copy_token(out_qc->ikrules[i].bone, HL2M_NAME_MAX, cmd->args[a + 1]);
        } else if (hl2m_text_eq_ci(cmd->args[a], "attachment") && a + 1 < cmd->argc) {
            hl2m_copy_token(out_qc->ikrules[i].attachment, HL2M_NAME_MAX, cmd->args[a + 1]);
        } else if (hl2m_text_eq_ci(cmd->args[a], "frame") && a + 1 < cmd->argc) {
            (void)hl2m_parse_int_text(cmd->args[a + 1], &out_qc->ikrules[i].frame);
        } else if (hl2m_text_eq_ci(cmd->args[a], "contact") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].contact);
        } else if (hl2m_text_eq_ci(cmd->args[a], "start") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].start);
        } else if (hl2m_text_eq_ci(cmd->args[a], "peak") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].peak);
        } else if (hl2m_text_eq_ci(cmd->args[a], "tail") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].tail);
        } else if (hl2m_text_eq_ci(cmd->args[a], "end") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].end);
        } else if (hl2m_text_eq_ci(cmd->args[a], "drop") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].drop);
        } else if (hl2m_text_eq_ci(cmd->args[a], "top") && a + 1 < cmd->argc) {
            (void)hl2m_parse_fixed_text(cmd->args[a + 1], &out_qc->ikrules[i].top);
        }
    }
    if (out_qc->ikrules[i].chain[0] == 0) hl2m_copy_token(out_qc->ikrules[i].chain, HL2M_NAME_MAX, out_qc->ikrules[i].name);
    ++out_qc->ikrule_count;
    return HL2M_OK;
}

static int hl2m_qc_add_material_dir(hl2m_qc *out_qc, const char *path)
{
    int i;
    for (i = 0; i < out_qc->material_dir_count; ++i) {
        if (hl2m_text_eq_ci(out_qc->material_dirs[i], path)) return HL2M_OK;
    }
    if (out_qc->material_dir_count >= HL2M_MAX_QC_MATERIAL_DIRS) return HL2M_ERR_LIMIT;
    hl2m_copy_token(out_qc->material_dirs[out_qc->material_dir_count], HL2M_PATH_MAX, path);
    ++out_qc->material_dir_count;
    return HL2M_OK;
}

int hl2m_decode_qc(const hl2m_u8 *data, unsigned int size, hl2m_qc *out_qc)
{
    hl2m_stream s;
    char line[HL2M_LINE_MAX];
    char tok[HL2M_TOKEN_MAX];
    char current_bodygroup[HL2M_NAME_MAX];
    const char *p;
    int line_no;
    int block_comment;
    int depth;
    int brace;
    int cmd_index;
    int kind;
    int current_sequence;
    int r;

    if (data == 0 || out_qc == 0) return HL2M_ERR_NULL;
    hl2m_clear_qc(out_qc);
    s.data = data;
    s.size = size;
    s.pos = 0u;
    line_no = 0;
    block_comment = 0;
    depth = 0;
    current_sequence = -1;
    current_bodygroup[0] = 0;
    while (hl2m_stream_line(&s, line, (unsigned int)sizeof(line))) {
        ++line_no;
        hl2m_qc_strip_comments(line, &block_comment);
        p = hl2m_skip_spaces(line);
        if (*p == 0) continue;
        while (*p == '{' || *p == '}') {
            if (*p == '{') ++depth;
            else if (depth > 0) { --depth; if (depth == 0) current_sequence = -1; }
            ++p;
            p = hl2m_skip_spaces(p);
        }
        if (*p == 0) continue;
        if (out_qc->command_count >= HL2M_MAX_QC_COMMANDS) return HL2M_ERR_LIMIT;
        if (!hl2m_qc_next_token(&p, tok, HL2M_TOKEN_MAX, &brace)) continue;
        if (brace == 1) { ++depth; continue; }
        if (brace == -1) { if (depth > 0) { --depth; if (depth == 0) current_sequence = -1; } continue; }
        kind = hl2m_qc_kind_from_name(tok);
        if (tok[0] != '$' && kind == HL2M_QC_UNKNOWN) continue;
        cmd_index = out_qc->command_count;
        out_qc->commands[cmd_index].kind = kind;
        out_qc->commands[cmd_index].line = line_no;
        out_qc->commands[cmd_index].depth = depth;
        hl2m_qc_trim_prefix_dollar(out_qc->commands[cmd_index].name, HL2M_NAME_MAX, tok);
        while (hl2m_qc_next_token(&p, tok, HL2M_TOKEN_MAX, &brace)) {
            if (brace == 1) { ++depth; continue; }
            if (brace == -1) { if (depth > 0) { --depth; if (depth == 0) current_sequence = -1; } continue; }
            if (out_qc->commands[cmd_index].argc < HL2M_MAX_QC_ARGS) {
                hl2m_copy_token(out_qc->commands[cmd_index].args[out_qc->commands[cmd_index].argc], HL2M_VALUE_MAX, tok);
                ++out_qc->commands[cmd_index].argc;
            } else {
                return HL2M_ERR_LIMIT;
            }
        }
        ++out_qc->command_count;

        if (kind == HL2M_QC_BODYGROUP && out_qc->commands[cmd_index].argc >= 1) {
            hl2m_copy_token(current_bodygroup, HL2M_NAME_MAX, out_qc->commands[cmd_index].args[0]);
        } else if (kind == HL2M_QC_MODELNAME && out_qc->commands[cmd_index].argc >= 1) {
            hl2m_copy_token(out_qc->modelname, HL2M_PATH_MAX, out_qc->commands[cmd_index].args[0]);
        } else if (kind == HL2M_QC_CDMATERIALS && out_qc->commands[cmd_index].argc >= 1) {
            r = hl2m_qc_add_material_dir(out_qc, out_qc->commands[cmd_index].args[0]);
            if (r != HL2M_OK) return r;
        } else if ((kind == HL2M_QC_BODY || kind == HL2M_QC_MODEL) && out_qc->commands[cmd_index].argc >= 2) {
            r = hl2m_qc_add_body(out_qc, cmd_index, out_qc->commands[cmd_index].args[0], out_qc->commands[cmd_index].args[1]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_STUDIO && out_qc->commands[cmd_index].argc >= 1) {
            r = hl2m_qc_add_body(out_qc, cmd_index, current_bodygroup, out_qc->commands[cmd_index].args[0]);
            if (r != HL2M_OK) return r;
        } else if ((kind == HL2M_QC_SEQUENCE || kind == HL2M_QC_ANIMATION) && out_qc->commands[cmd_index].argc >= 1) {
            if (out_qc->commands[cmd_index].argc >= 2) {
                r = hl2m_qc_add_sequence(out_qc, cmd_index, out_qc->commands[cmd_index].args[0], out_qc->commands[cmd_index].args[1]);
            } else {
                r = hl2m_qc_add_sequence(out_qc, cmd_index, out_qc->commands[cmd_index].args[0], out_qc->commands[cmd_index].args[0]);
            }
            if (r != HL2M_OK) return r;
            current_sequence = out_qc->sequence_count - 1;
        } else if (kind == HL2M_QC_LOD && out_qc->commands[cmd_index].argc >= 1) {
            if (out_qc->commands[cmd_index].argc >= 2) r = hl2m_qc_add_lod(out_qc, cmd_index, out_qc->commands[cmd_index].args[0], out_qc->commands[cmd_index].args[1]);
            else r = hl2m_qc_add_lod(out_qc, cmd_index, out_qc->commands[cmd_index].args[0], 0);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_REPLACEMODEL && out_qc->commands[cmd_index].argc >= 2) {
            if (out_qc->lod_count > 0) hl2m_copy_token(out_qc->lods[out_qc->lod_count - 1].source, HL2M_PATH_MAX, out_qc->commands[cmd_index].args[1]);
        } else if (kind == HL2M_QC_ATTACHMENT) {
            r = hl2m_qc_add_attachment(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_HBOX) {
            r = hl2m_qc_add_hitbox(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_DEFINEBONE) {
            r = hl2m_qc_add_definebone(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_POSEPARAMETER) {
            r = hl2m_qc_add_poseparameter(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_RENAMEMATERIAL || kind == HL2M_QC_REPLACEMATERIAL) {
            r = hl2m_qc_add_material_replace(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_COLLISIONMODEL) {
            r = hl2m_qc_add_collision(out_qc, cmd_index, &out_qc->commands[cmd_index], 0);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_COLLISIONJOINTS) {
            r = hl2m_qc_add_collision(out_qc, cmd_index, &out_qc->commands[cmd_index], 1);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_FLEXCONTROLLER) {
            r = hl2m_qc_add_flexcontroller(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_FLEXRULE || kind == HL2M_QC_FLEX) {
            r = hl2m_qc_add_flexrule(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_EVENT) {
            r = hl2m_qc_add_sequence_event(out_qc, cmd_index, &out_qc->commands[cmd_index], current_sequence);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_IKCHAIN) {
            r = hl2m_qc_add_ikchain(out_qc, cmd_index, &out_qc->commands[cmd_index]);
            if (r != HL2M_OK) return r;
        } else if (kind == HL2M_QC_IKRULE) {
            r = hl2m_qc_add_ikrule(out_qc, cmd_index, &out_qc->commands[cmd_index], current_sequence);
            if (r != HL2M_OK) return r;
        }
    }
    return HL2M_OK;
}


static int hl2m_qc_arg_needs_quote(const char *text)
{
    if (*text == 0) return 1;
    while (*text != 0) {
        if (hl2m_is_space(*text) || *text == '{' || *text == '}' || *text == '"') return 1;
        ++text;
    }
    return 0;
}

static int hl2m_out_append_qc_arg(char *out_text, unsigned int out_size, unsigned int *used, const char *text)
{
    int ret;
    int quote;
    ret = HL2M_OK;
    quote = hl2m_qc_arg_needs_quote(text);
    if (quote) {
        if (hl2m_out_append_char(out_text, out_size, used, '"') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    while (*text != 0) {
        if (*text == '"') {
            if (hl2m_out_append_char(out_text, out_size, used, '_') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        } else {
            if (hl2m_out_append_char(out_text, out_size, used, *text) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
        ++text;
    }
    if (quote) {
        if (hl2m_out_append_char(out_text, out_size, used, '"') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    return ret;
}


static int hl2m_qc_kind_uses_dollar(int kind)
{
    if (kind == HL2M_QC_STUDIO) return 0;
    if (kind == HL2M_QC_BLANK) return 0;
    if (kind == HL2M_QC_REPLACEMODEL) return 0;
    if (kind == HL2M_QC_REPLACEMATERIAL) return 0;
    if (kind == HL2M_QC_SHADOWLOD) return 0;
    if (kind == HL2M_QC_EVENT) return 0;
    if (kind == HL2M_QC_FPS) return 0;
    return 1;
}

int hl2m_qc_write(const hl2m_qc *qc, char *out_text, unsigned int out_size, unsigned int *out_used)
{
    unsigned int used;
    int ret;
    int i;
    int a;
    int d;
    if (qc == 0) return HL2M_ERR_NULL;
    used = 0u;
    ret = HL2M_OK;
    if (out_text != 0 && out_size != 0u) out_text[0] = 0;
    for (i = 0; i < qc->command_count; ++i) {
        for (d = 0; d < qc->commands[i].depth; ++d) {
            if (hl2m_out_append_text(out_text, out_size, &used, "    ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
        if (hl2m_qc_kind_uses_dollar(qc->commands[i].kind)) {
            if (hl2m_out_append_char(out_text, out_size, &used, '$') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
        if (hl2m_out_append_text(out_text, out_size, &used, qc->commands[i].name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        for (a = 0; a < qc->commands[i].argc; ++a) {
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_qc_arg(out_text, out_size, &used, qc->commands[i].args[a]) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (out_used != 0) *out_used = used;
    return ret;
}


static int hl2m_qc_push_command(hl2m_qc *qc, int kind, const char *name, int argc, const char **argv)
{
    int c;
    int a;
    if (qc->command_count >= HL2M_MAX_QC_COMMANDS) return HL2M_ERR_LIMIT;
    c = qc->command_count;
    qc->commands[c].kind = kind;
    qc->commands[c].line = c + 1;
    qc->commands[c].depth = 0;
    qc->commands[c].argc = argc;
    hl2m_copy_token(qc->commands[c].name, HL2M_NAME_MAX, name);
    for (a = 0; a < argc && a < HL2M_MAX_QC_ARGS; ++a) hl2m_copy_token(qc->commands[c].args[a], HL2M_VALUE_MAX, argv[a]);
    ++qc->command_count;
    return HL2M_OK;
}

int hl2m_make_basic_qc(const hl2m_smd *smd, const char *modelname, const char *body_name, const char *smd_path, const char *cdmaterials, int staticprop, hl2m_qc *out_qc)
{
    const char *args[4];
    int r;
    if (smd == 0 || out_qc == 0) return HL2M_ERR_NULL;
    hl2m_clear_qc(out_qc);
    if (modelname == 0 || modelname[0] == 0) modelname = "models/hl2modelib/generated.mdl";
    if (body_name == 0 || body_name[0] == 0) body_name = "body";
    if (smd_path == 0 || smd_path[0] == 0) smd_path = "generated_ref.smd";
    if (cdmaterials == 0 || cdmaterials[0] == 0) cdmaterials = "models/hl2modelib";
    args[0] = modelname;
    r = hl2m_qc_push_command(out_qc, HL2M_QC_MODELNAME, "modelname", 1, args);
    if (r != HL2M_OK) return r;
    hl2m_copy_token(out_qc->modelname, HL2M_PATH_MAX, modelname);
    args[0] = cdmaterials;
    r = hl2m_qc_push_command(out_qc, HL2M_QC_CDMATERIALS, "cdmaterials", 1, args);
    if (r != HL2M_OK) return r;
    r = hl2m_qc_add_material_dir(out_qc, cdmaterials);
    if (r != HL2M_OK) return r;
    args[0] = body_name;
    args[1] = smd_path;
    r = hl2m_qc_push_command(out_qc, HL2M_QC_BODY, "body", 2, args);
    if (r != HL2M_OK) return r;
    r = hl2m_qc_add_body(out_qc, out_qc->command_count - 1, body_name, smd_path);
    if (r != HL2M_OK) return r;
    args[0] = "idle";
    args[1] = smd_path;
    args[2] = "fps";
    args[3] = "30";
    r = hl2m_qc_push_command(out_qc, HL2M_QC_SEQUENCE, "sequence", 4, args);
    if (r != HL2M_OK) return r;
    r = hl2m_qc_add_sequence(out_qc, out_qc->command_count - 1, "idle", smd_path);
    if (r != HL2M_OK) return r;
    if (staticprop) {
        r = hl2m_qc_push_command(out_qc, HL2M_QC_STATICPROP, "staticprop", 0, args);
        if (r != HL2M_OK) return r;
    }
    if (smd->material_count > 0) {
        args[0] = "default";
        r = hl2m_qc_push_command(out_qc, HL2M_QC_SURFACEPROP, "surfaceprop", 1, args);
        if (r != HL2M_OK) return r;
    }
    return HL2M_OK;
}

