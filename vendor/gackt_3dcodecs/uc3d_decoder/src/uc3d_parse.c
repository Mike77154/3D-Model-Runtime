#include "uc3d.h"

static int uc3d_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int uc3d_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int uc3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char uc3d_upper_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static void uc3d_zero_bytes(void *ptr, unsigned long size)
{
    unsigned char *p;
    unsigned long i;
    p = (unsigned char *)ptr;
    for (i = 0UL; i < size; ++i) {
        p[i] = 0U;
    }
}

static unsigned long uc3d_str_len(const char *s)
{
    unsigned long n;
    if (s == (const char *)0) {
        return 0UL;
    }
    n = 0UL;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static void uc3d_copy_n(char *dst, unsigned long dst_size, const char *src, unsigned long src_len)
{
    unsigned long i;
    unsigned long max;
    if (dst == (char *)0 || dst_size == 0UL) {
        return;
    }
    max = dst_size - 1UL;
    if (src_len < max) {
        max = src_len;
    }
    for (i = 0UL; i < max; ++i) {
        dst[i] = src[i];
    }
    dst[max] = '\0';
}

static void uc3d_copy_cstr(char *dst, unsigned long dst_size, const char *src)
{
    uc3d_copy_n(dst, dst_size, src, uc3d_str_len(src));
}

static int uc3d_eq_ci_n(const char *a, const char *b, unsigned long n)
{
    unsigned long i;
    for (i = 0UL; i < n; ++i) {
        if (a[i] == '\0' || b[i] == '\0') {
            return a[i] == b[i];
        }
        if (uc3d_upper_char(a[i]) != uc3d_upper_char(b[i])) {
            return 0;
        }
    }
    return 1;
}

static int uc3d_eq_ci(const char *a, const char *b)
{
    unsigned long i;
    i = 0UL;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (uc3d_upper_char(a[i]) != uc3d_upper_char(b[i])) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static int uc3d_starts_ci(const char *s, const char *prefix)
{
    unsigned long n;
    n = uc3d_str_len(prefix);
    return uc3d_eq_ci_n(s, prefix, n);
}

static long uc3d_parse_long_local(const char *text)
{
    long value;
    int sign;
    unsigned long i;
    if (text == (const char *)0) {
        return 0L;
    }
    i = 0UL;
    while (uc3d_is_space(text[i])) {
        ++i;
    }
    sign = 1;
    if (text[i] == '-') {
        sign = -1;
        ++i;
    } else if (text[i] == '+') {
        ++i;
    }
    value = 0L;
    while (uc3d_is_digit(text[i])) {
        if (value < 214748364L) {
            value = (value * 10L) + (long)(text[i] - '0');
        }
        ++i;
    }
    return sign < 0 ? -value : value;
}

uc3d_fixed uc3d_fixed_from_parts(long whole, long frac, long frac_scale, int sign)
{
    uc3d_fixed value;
    uc3d_fixed frac_part;
    if (frac_scale <= 0L) {
        frac_scale = 1L;
    }
    if (whole < 0L) {
        whole = -whole;
    }
    if (frac < 0L) {
        frac = -frac;
    }
    value = (uc3d_fixed)(whole << UC3D_FIXED_SHIFT);
    frac_part = (uc3d_fixed)(((long)frac * UC3D_FIXED_ONE) / frac_scale);
    value += frac_part;
    if (sign < 0) {
        value = -value;
    }
    return value;
}

uc3d_fixed uc3d_fixed_parse(const char *text)
{
    unsigned long i;
    int sign;
    long whole;
    long frac;
    long frac_scale;

    if (text == (const char *)0) {
        return 0L;
    }

    i = 0UL;
    while (uc3d_is_space(text[i])) {
        ++i;
    }
    sign = 1;
    if (text[i] == '-') {
        sign = -1;
        ++i;
    } else if (text[i] == '+') {
        ++i;
    }

    whole = 0L;
    while (uc3d_is_digit(text[i])) {
        if (whole < 32767L) {
            whole = whole * 10L + (long)(text[i] - '0');
        }
        ++i;
    }

    frac = 0L;
    frac_scale = 1L;
    if (text[i] == '.') {
        ++i;
        while (uc3d_is_digit(text[i]) && frac_scale < 1000000L) {
            frac = frac * 10L + (long)(text[i] - '0');
            frac_scale *= 10L;
            ++i;
        }
    }

    return uc3d_fixed_from_parts(whole, frac, frac_scale, sign);
}

void uc3d_fixed_to_string(uc3d_fixed value, char *out, unsigned long out_size, int decimals)
{
    char tmp[32];
    unsigned long pos;
    unsigned long i;
    unsigned long n;
    long whole;
    long frac;
    long scale;
    int neg;

    if (out == (char *)0 || out_size == 0UL) {
        return;
    }
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 6) {
        decimals = 6;
    }

    neg = 0;
    if (value < 0L) {
        neg = 1;
        value = -value;
    }
    whole = (long)(value >> UC3D_FIXED_SHIFT);
    frac = (long)(value & (UC3D_FIXED_ONE - 1L));
    scale = 1L;
    for (i = 0UL; i < (unsigned long)decimals; ++i) {
        scale *= 10L;
    }
    frac = (long)(((long)frac * scale + (UC3D_FIXED_ONE / 2L)) >> UC3D_FIXED_SHIFT);
    if (frac >= scale && decimals > 0) {
        frac -= scale;
        ++whole;
    }

    pos = 0UL;
    if (neg && pos + 1UL < out_size) {
        out[pos++] = '-';
    }

    n = 0UL;
    if (whole == 0L) {
        tmp[n++] = '0';
    } else {
        while (whole > 0L && n < sizeof(tmp)) {
            tmp[n++] = (char)('0' + (whole % 10L));
            whole /= 10L;
        }
    }
    while (n > 0UL && pos + 1UL < out_size) {
        out[pos++] = tmp[--n];
    }

    if (decimals > 0 && pos + 1UL < out_size) {
        out[pos++] = '.';
        scale /= 10L;
        while (scale > 0L && pos + 1UL < out_size) {
            out[pos++] = (char)('0' + (frac / scale));
            frac %= scale;
            scale /= 10L;
        }
    }
    out[pos] = '\0';
}

static const char *uc3d_skip_space(const char *p)
{
    while (*p != '\0' && uc3d_is_space(*p)) {
        ++p;
    }
    return p;
}

static const char *uc3d_read_word(const char *p, char *out, unsigned long out_size)
{
    const char *start;
    p = uc3d_skip_space(p);
    start = p;
    while (*p != '\0' && (uc3d_is_alpha(*p) || uc3d_is_digit(*p) || *p == '#')) {
        ++p;
    }
    uc3d_copy_n(out, out_size, start, (unsigned long)(p - start));
    return p;
}

static const char *uc3d_read_token_value(const char *p, char *out, unsigned long out_size)
{
    const char *start;
    unsigned long n;
    char quote;

    p = uc3d_skip_space(p);
    if (*p == '"' || *p == '\'') {
        quote = *p;
        ++p;
        start = p;
        while (*p != '\0' && *p != quote) {
            ++p;
        }
        uc3d_copy_n(out, out_size, start, (unsigned long)(p - start));
        if (*p == quote) {
            ++p;
        }
        return p;
    }

    start = p;
    while (*p != '\0' && !uc3d_is_space(*p)) {
        ++p;
    }
    n = (unsigned long)(p - start);
    while (n > 0UL && (start[n - 1UL] == ';' || start[n - 1UL] == ',')) {
        --n;
    }
    uc3d_copy_n(out, out_size, start, n);
    return p;
}

static int uc3d_get_key_value(const char *line, const char *key, char *out, unsigned long out_size)
{
    const char *p;
    const char *start;
    char found_key[UC3D_MAX_KEY];
    char val[UC3D_MAX_VALUE];
    unsigned long n;

    if (out_size > 0UL) {
        out[0] = '\0';
    }
    p = line;
    while (*p != '\0') {
        p = uc3d_skip_space(p);
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != '=' && !uc3d_is_space(*p)) {
            ++p;
        }
        n = (unsigned long)(p - start);
        uc3d_copy_n(found_key, sizeof(found_key), start, n);
        if (*p == '=') {
            ++p;
            p = uc3d_read_token_value(p, val, sizeof(val));
            if (uc3d_eq_ci(found_key, key)) {
                uc3d_copy_cstr(out, out_size, val);
                return 1;
            }
        } else {
            while (*p != '\0' && uc3d_is_space(*p)) {
                ++p;
            }
        }
    }
    return 0;
}

static int uc3d_add_exec(uc3d_scene *scene, const char *category, const char *command, const char *raw)
{
    uc3d_exec *e;
    if (scene->exec_count >= UC3D_MAX_EXECS) {
        scene->overflow_count++;
        return UC3D_ERR_FULL;
    }
    e = &scene->execs[scene->exec_count++];
    uc3d_copy_cstr(e->category, sizeof(e->category), category);
    uc3d_copy_cstr(e->command, sizeof(e->command), command);
    uc3d_copy_cstr(e->raw, sizeof(e->raw), raw);
    return UC3D_OK;
}

int uc3d_find_mesh(const uc3d_scene *scene, const char *name)
{
    unsigned long i;
    if (scene == (const uc3d_scene *)0 || name == (const char *)0) {
        return UC3D_ERR_NULL;
    }
    for (i = 0UL; i < scene->mesh_count; ++i) {
        if (uc3d_eq_ci(scene->meshes[i].mesh, name) || uc3d_eq_ci(scene->meshes[i].meshmap, name)) {
            return (int)i;
        }
    }
    return UC3D_ERR_NOT_FOUND;
}

int uc3d_find_texture(const uc3d_scene *scene, const char *name)
{
    unsigned long i;
    if (scene == (const uc3d_scene *)0 || name == (const char *)0) {
        return UC3D_ERR_NULL;
    }
    for (i = 0UL; i < scene->texture_count; ++i) {
        if (uc3d_eq_ci(scene->textures[i].name, name)) {
            return (int)i;
        }
    }
    return UC3D_ERR_NOT_FOUND;
}

static uc3d_mesh_ref *uc3d_get_or_add_mesh(uc3d_scene *scene, const char *name)
{
    int idx;
    uc3d_mesh_ref *m;
    if (name == (const char *)0 || name[0] == '\0') {
        return (uc3d_mesh_ref *)0;
    }
    idx = uc3d_find_mesh(scene, name);
    if (idx >= 0) {
        return &scene->meshes[idx];
    }
    if (scene->mesh_count >= UC3D_MAX_MESHES) {
        scene->overflow_count++;
        return (uc3d_mesh_ref *)0;
    }
    m = &scene->meshes[scene->mesh_count++];
    uc3d_zero_bytes(m, sizeof(*m));
    uc3d_copy_cstr(m->mesh, sizeof(m->mesh), name);
    m->scale_x = UC3D_FIXED_ONE;
    m->scale_y = UC3D_FIXED_ONE;
    m->scale_z = UC3D_FIXED_ONE;
    return m;
}

static void uc3d_parse_class_line(uc3d_scene *scene, const char *line)
{
    char word[UC3D_MAX_NAME];
    const char *p;
    p = line;
    p = uc3d_read_word(p, word, sizeof(word));
    if (!uc3d_eq_ci(word, "class")) {
        return;
    }
    p = uc3d_read_word(p, scene->class_name, sizeof(scene->class_name));
    scene->has_class = 1;
    while (*p != '\0') {
        p = uc3d_read_word(p, word, sizeof(word));
        if (word[0] == '\0') {
            break;
        }
        if (uc3d_eq_ci(word, "extends") || uc3d_eq_ci(word, "expands")) {
            p = uc3d_read_word(p, scene->parent_name, sizeof(scene->parent_name));
            break;
        }
    }
}

static void uc3d_parse_mesh_import(uc3d_scene *scene, const char *line)
{
    char val[UC3D_MAX_VALUE];
    char mesh_name[UC3D_MAX_NAME];
    uc3d_mesh_ref *m;

    mesh_name[0] = '\0';
    if (!uc3d_get_key_value(line, "MESH", mesh_name, sizeof(mesh_name))) {
        return;
    }
    m = uc3d_get_or_add_mesh(scene, mesh_name);
    if (m == (uc3d_mesh_ref *)0) {
        return;
    }
    if (uc3d_get_key_value(line, "ANIVFILE", val, sizeof(val))) {
        uc3d_copy_cstr(m->anivfile, sizeof(m->anivfile), val);
    }
    if (uc3d_get_key_value(line, "DATAFILE", val, sizeof(val))) {
        uc3d_copy_cstr(m->datafile, sizeof(m->datafile), val);
    }
    if (uc3d_get_key_value(line, "X", val, sizeof(val))) {
        m->import_x = uc3d_fixed_parse(val);
        m->has_import = 1;
    }
    if (uc3d_get_key_value(line, "Y", val, sizeof(val))) {
        m->import_y = uc3d_fixed_parse(val);
        m->has_import = 1;
    }
    if (uc3d_get_key_value(line, "Z", val, sizeof(val))) {
        m->import_z = uc3d_fixed_parse(val);
        m->has_import = 1;
    }
}

static void uc3d_parse_mesh_origin(uc3d_scene *scene, const char *line)
{
    char val[UC3D_MAX_VALUE];
    char mesh_name[UC3D_MAX_NAME];
    uc3d_mesh_ref *m;

    if (!uc3d_get_key_value(line, "MESH", mesh_name, sizeof(mesh_name))) {
        return;
    }
    m = uc3d_get_or_add_mesh(scene, mesh_name);
    if (m == (uc3d_mesh_ref *)0) {
        return;
    }
    if (uc3d_get_key_value(line, "X", val, sizeof(val))) {
        m->origin_x = uc3d_fixed_parse(val);
        m->has_origin = 1;
    }
    if (uc3d_get_key_value(line, "Y", val, sizeof(val))) {
        m->origin_y = uc3d_fixed_parse(val);
        m->has_origin = 1;
    }
    if (uc3d_get_key_value(line, "Z", val, sizeof(val))) {
        m->origin_z = uc3d_fixed_parse(val);
        m->has_origin = 1;
    }
    if (uc3d_get_key_value(line, "PITCH", val, sizeof(val))) {
        m->pitch = uc3d_parse_long_local(val);
    }
    if (uc3d_get_key_value(line, "YAW", val, sizeof(val))) {
        m->yaw = uc3d_parse_long_local(val);
    }
    if (uc3d_get_key_value(line, "ROLL", val, sizeof(val))) {
        m->roll = uc3d_parse_long_local(val);
    }
}

static void uc3d_parse_mesh_sequence(uc3d_scene *scene, const char *line)
{
    uc3d_sequence *s;
    char val[UC3D_MAX_VALUE];
    if (scene->sequence_count >= UC3D_MAX_SEQUENCES) {
        scene->overflow_count++;
        return;
    }
    s = &scene->sequences[scene->sequence_count];
    uc3d_zero_bytes(s, sizeof(*s));
    if (!uc3d_get_key_value(line, "MESH", s->mesh, sizeof(s->mesh))) {
        return;
    }
    if (!uc3d_get_key_value(line, "SEQ", s->name, sizeof(s->name))) {
        uc3d_copy_cstr(s->name, sizeof(s->name), "All");
    }
    if (uc3d_get_key_value(line, "STARTFRAME", val, sizeof(val))) {
        s->start_frame = uc3d_parse_long_local(val);
    }
    if (uc3d_get_key_value(line, "NUMFRAMES", val, sizeof(val))) {
        s->num_frames = uc3d_parse_long_local(val);
    }
    if (uc3d_get_key_value(line, "RATE", val, sizeof(val))) {
        s->rate = uc3d_fixed_parse(val);
    }
    scene->sequence_count++;
}

static void uc3d_parse_meshmap_new(uc3d_scene *scene, const char *line)
{
    char meshmap[UC3D_MAX_NAME];
    char mesh[UC3D_MAX_NAME];
    uc3d_mesh_ref *m;
    meshmap[0] = '\0';
    mesh[0] = '\0';
    uc3d_get_key_value(line, "MESHMAP", meshmap, sizeof(meshmap));
    uc3d_get_key_value(line, "MESH", mesh, sizeof(mesh));
    if (mesh[0] != '\0') {
        m = uc3d_get_or_add_mesh(scene, mesh);
    } else {
        m = uc3d_get_or_add_mesh(scene, meshmap);
    }
    if (m != (uc3d_mesh_ref *)0 && meshmap[0] != '\0') {
        uc3d_copy_cstr(m->meshmap, sizeof(m->meshmap), meshmap);
    }
}

static void uc3d_parse_meshmap_scale(uc3d_scene *scene, const char *line)
{
    char meshmap[UC3D_MAX_NAME];
    char val[UC3D_MAX_VALUE];
    uc3d_mesh_ref *m;
    meshmap[0] = '\0';
    if (!uc3d_get_key_value(line, "MESHMAP", meshmap, sizeof(meshmap))) {
        if (!uc3d_get_key_value(line, "MESH", meshmap, sizeof(meshmap))) {
            return;
        }
    }
    m = uc3d_get_or_add_mesh(scene, meshmap);
    if (m == (uc3d_mesh_ref *)0) {
        return;
    }
    if (m->meshmap[0] == '\0') {
        uc3d_copy_cstr(m->meshmap, sizeof(m->meshmap), meshmap);
    }
    if (uc3d_get_key_value(line, "X", val, sizeof(val))) {
        m->scale_x = uc3d_fixed_parse(val);
        m->has_scale = 1;
    }
    if (uc3d_get_key_value(line, "Y", val, sizeof(val))) {
        m->scale_y = uc3d_fixed_parse(val);
        m->has_scale = 1;
    }
    if (uc3d_get_key_value(line, "Z", val, sizeof(val))) {
        m->scale_z = uc3d_fixed_parse(val);
        m->has_scale = 1;
    }
}

static void uc3d_parse_meshmap_settexture(uc3d_scene *scene, const char *line)
{
    uc3d_material_binding *b;
    char val[UC3D_MAX_VALUE];
    if (scene->material_count >= UC3D_MAX_MATERIALS) {
        scene->overflow_count++;
        return;
    }
    b = &scene->materials[scene->material_count];
    uc3d_zero_bytes(b, sizeof(*b));
    if (!uc3d_get_key_value(line, "MESHMAP", b->meshmap, sizeof(b->meshmap))) {
        return;
    }
    if (uc3d_get_key_value(line, "NUM", val, sizeof(val))) {
        b->slot = uc3d_parse_long_local(val);
    }
    uc3d_get_key_value(line, "TEXTURE", b->texture, sizeof(b->texture));
    scene->material_count++;
}

static void uc3d_parse_texture_import(uc3d_scene *scene, const char *line)
{
    uc3d_texture *t;
    char val[UC3D_MAX_VALUE];
    char name[UC3D_MAX_NAME];
    int idx;

    name[0] = '\0';
    uc3d_get_key_value(line, "NAME", name, sizeof(name));
    idx = uc3d_find_texture(scene, name);
    if (idx >= 0) {
        t = &scene->textures[idx];
    } else {
        if (scene->texture_count >= UC3D_MAX_TEXTURES) {
            scene->overflow_count++;
            return;
        }
        t = &scene->textures[scene->texture_count++];
        uc3d_zero_bytes(t, sizeof(*t));
        uc3d_copy_cstr(t->name, sizeof(t->name), name);
    }
    uc3d_get_key_value(line, "FILE", t->file, sizeof(t->file));
    uc3d_get_key_value(line, "GROUP", t->group, sizeof(t->group));
    if (uc3d_get_key_value(line, "FLAGS", val, sizeof(val))) {
        t->flags = uc3d_parse_long_local(val);
        t->has_flags = 1;
    }
    if (uc3d_get_key_value(line, "LODSET", val, sizeof(val))) {
        t->lodset = uc3d_parse_long_local(val);
        t->has_lodset = 1;
    }
    if (uc3d_get_key_value(line, "MIPS", val, sizeof(val))) {
        t->mips_on = uc3d_eq_ci(val, "ON") || uc3d_eq_ci(val, "TRUE") || uc3d_eq_ci(val, "1");
    }
}

static void uc3d_parse_exec_line(uc3d_scene *scene, const char *line)
{
    const char *p;
    char hash_exec[UC3D_MAX_NAME];
    char category[UC3D_MAX_NAME];
    char command[UC3D_MAX_NAME];

    p = line;
    p = uc3d_read_word(p, hash_exec, sizeof(hash_exec));
    if (!uc3d_eq_ci(hash_exec, "#exec")) {
        return;
    }
    p = uc3d_read_word(p, category, sizeof(category));
    p = uc3d_read_word(p, command, sizeof(command));
    uc3d_add_exec(scene, category, command, line);

    if (uc3d_eq_ci(category, "MESH")) {
        if (uc3d_eq_ci(command, "IMPORT")) {
            uc3d_parse_mesh_import(scene, line);
        } else if (uc3d_eq_ci(command, "ORIGIN")) {
            uc3d_parse_mesh_origin(scene, line);
        } else if (uc3d_eq_ci(command, "SEQUENCE")) {
            uc3d_parse_mesh_sequence(scene, line);
        } else if (uc3d_eq_ci(command, "FLIPFACES")) {
            scene->warning_count++;
        }
    } else if (uc3d_eq_ci(category, "MESHMAP")) {
        if (uc3d_eq_ci(command, "NEW")) {
            uc3d_parse_meshmap_new(scene, line);
        } else if (uc3d_eq_ci(command, "SCALE")) {
            uc3d_parse_meshmap_scale(scene, line);
        } else if (uc3d_eq_ci(command, "SETTEXTURE")) {
            uc3d_parse_meshmap_settexture(scene, line);
        }
    } else if (uc3d_eq_ci(category, "TEXTURE") && uc3d_eq_ci(command, "IMPORT")) {
        uc3d_parse_texture_import(scene, line);
    }
}

static void uc3d_parse_default_property(uc3d_scene *scene, const char *line)
{
    const char *p;
    const char *start;
    unsigned long key_len;
    uc3d_default_prop *prop;

    p = uc3d_skip_space(line);
    if (*p == '\0' || *p == '{' || *p == '}') {
        return;
    }
    start = p;
    while (*p != '\0' && *p != '=' && !uc3d_is_space(*p)) {
        ++p;
    }
    key_len = (unsigned long)(p - start);
    p = uc3d_skip_space(p);
    if (*p != '=') {
        return;
    }
    ++p;
    if (scene->default_prop_count >= UC3D_MAX_DEFAULT_PROPS) {
        scene->overflow_count++;
        return;
    }
    prop = &scene->default_props[scene->default_prop_count++];
    uc3d_zero_bytes(prop, sizeof(*prop));
    uc3d_copy_n(prop->key, sizeof(prop->key), start, key_len);
    p = uc3d_read_token_value(p, prop->value, sizeof(prop->value));
    (void)p;
}

static void uc3d_strip_comments_from_line(const char *src, unsigned long len, char *dst, unsigned long dst_size, int *block_comment)
{
    unsigned long i;
    unsigned long out;
    char quote;
    int in_quote;

    out = 0UL;
    in_quote = 0;
    quote = '\0';
    i = 0UL;
    while (i < len && out + 1UL < dst_size) {
        if (*block_comment) {
            if (i + 1UL < len && src[i] == '*' && src[i + 1UL] == '/') {
                *block_comment = 0;
                i += 2UL;
            } else {
                ++i;
            }
            continue;
        }
        if (!in_quote && i + 1UL < len && src[i] == '/' && src[i + 1UL] == '*') {
            *block_comment = 1;
            i += 2UL;
            continue;
        }
        if (!in_quote && i + 1UL < len && src[i] == '/' && src[i + 1UL] == '/') {
            break;
        }
        if (src[i] == '"' || src[i] == '\'') {
            if (!in_quote) {
                in_quote = 1;
                quote = src[i];
            } else if (quote == src[i]) {
                in_quote = 0;
                quote = '\0';
            }
        }
        dst[out++] = src[i++];
    }
    dst[out] = '\0';
}

void uc3d_scene_init(uc3d_scene *scene)
{
    if (scene == (uc3d_scene *)0) {
        return;
    }
    uc3d_zero_bytes(scene, sizeof(*scene));
}

int uc3d_parse_uc_buffer(const char *text, unsigned long text_size, uc3d_scene *scene)
{
    unsigned long pos;
    unsigned long start;
    unsigned long len;
    char line[512];
    const char *p;
    int in_default;
    int block_comment;

    if (text == (const char *)0 || scene == (uc3d_scene *)0) {
        return UC3D_ERR_NULL;
    }

    uc3d_scene_init(scene);
    pos = 0UL;
    in_default = 0;
    block_comment = 0;

    while (pos < text_size) {
        start = pos;
        while (pos < text_size && text[pos] != '\n' && text[pos] != '\r') {
            ++pos;
        }
        len = pos - start;
        uc3d_strip_comments_from_line(text + start, len, line, sizeof(line), &block_comment);
        while (pos < text_size && (text[pos] == '\n' || text[pos] == '\r')) {
            ++pos;
        }

        p = uc3d_skip_space(line);
        if (*p == '\0') {
            continue;
        }
        if (in_default) {
            if (*p == '}') {
                in_default = 0;
                continue;
            }
            uc3d_parse_default_property(scene, p);
            continue;
        }
        if (uc3d_starts_ci(p, "defaultproperties")) {
            in_default = 1;
            if (p[17] != '\0') {
                p += 17;
                p = uc3d_skip_space(p);
                if (*p != '{' && *p != '\0') {
                    uc3d_parse_default_property(scene, p);
                }
            }
            continue;
        }
        if (uc3d_starts_ci(p, "class")) {
            uc3d_parse_class_line(scene, p);
        } else if (uc3d_starts_ci(p, "#exec")) {
            uc3d_parse_exec_line(scene, p);
        }
    }

    return scene->overflow_count > 0UL ? UC3D_ERR_FULL : UC3D_OK;
}
