#include "gobj_mtl.h"

static int gm_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int gm_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static const char *gm_skip_space(const char *p, const char *end)
{
    while (p < end && gm_is_space(*p)) {
        ++p;
    }
    return p;
}

static int gm_streq_range(const char *a, const char *b, const char *s)
{
    const char *p;

    p = a;
    while (p < b && *s != '\0') {
        if (*p != *s) {
            return 0;
        }
        ++p;
        ++s;
    }
    return p == b && *s == '\0';
}

static int gm_name_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void gm_copy_range(char *dst, unsigned int dst_cap, const char *a, const char *b)
{
    unsigned int i;

    if (dst_cap == 0u) {
        return;
    }
    i = 0u;
    while (a < b && *a != '#' && !gm_is_space(*a) && i + 1u < dst_cap) {
        dst[i] = *a;
        ++i;
        ++a;
    }
    dst[i] = '\0';
}

static void gm_copy_next_token(char *dst, unsigned int dst_cap, const char *a, const char *b)
{
    const char *p;
    const char *q;

    p = gm_skip_space(a, b);
    q = p;
    while (q < b && *q != '#' && !gm_is_space(*q)) {
        ++q;
    }
    gm_copy_range(dst, dst_cap, p, q);
}

static void gm_copy_last_token(char *dst, unsigned int dst_cap, const char *a, const char *b)
{
    const char *p;
    const char *q;
    const char *last_a;
    const char *last_b;

    p = gm_skip_space(a, b);
    last_a = p;
    last_b = p;
    while (p < b && *p != '#') {
        p = gm_skip_space(p, b);
        q = p;
        while (q < b && *q != '#' && !gm_is_space(*q)) {
            ++q;
        }
        if (q > p) {
            last_a = p;
            last_b = q;
        }
        p = q;
    }
    gm_copy_range(dst, dst_cap, last_a, last_b);
}

static int gm_parse_signed_int(const char **pp, const char *end, int *out)
{
    const char *p;
    int sign;
    int any;
    long v;

    p = *pp;
    p = gm_skip_space(p, end);
    sign = 1;
    any = 0;
    v = 0L;
    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }
    while (p < end && gm_is_digit(*p)) {
        any = 1;
        if (v < 214748364L) {
            v = (v * 10L) + (long)(*p - '0');
        }
        ++p;
    }
    if (!any) {
        return 0;
    }
    if (sign < 0) {
        v = -v;
    }
    *out = (int)v;
    *pp = p;
    return 1;
}

static int gm_parse_fixed(const char **pp, const char *end, GOBJ_Fixed *out)
{
    const char *p;
    int sign;
    int any;
    long ip;
    long frac;
    long scale;
    long val;
    int exp_sign;
    int exp_any;
    long exp_val;
    long i;

    p = *pp;
    p = gm_skip_space(p, end);
    sign = 1;
    any = 0;
    ip = 0L;
    frac = 0L;
    scale = 1L;
    exp_sign = 1;
    exp_any = 0;
    exp_val = 0L;
    if (p < end && *p == '-') {
        sign = -1;
        ++p;
    } else if (p < end && *p == '+') {
        ++p;
    }
    while (p < end && gm_is_digit(*p)) {
        any = 1;
        if (ip < 32767L) {
            ip = (ip * 10L) + (long)(*p - '0');
        }
        ++p;
    }
    if (p < end && *p == '.') {
        ++p;
        while (p < end && gm_is_digit(*p)) {
            any = 1;
            if (scale < 1000000L) {
                frac = (frac * 10L) + (long)(*p - '0');
                scale = scale * 10L;
            }
            ++p;
        }
    }
    if (!any) {
        return 0;
    }
    val = (ip << GOBJ_FIXED_SHIFT) + ((frac * GOBJ_FIXED_ONE) / scale);
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        if (p < end && *p == '-') {
            exp_sign = -1;
            ++p;
        } else if (p < end && *p == '+') {
            ++p;
        }
        while (p < end && gm_is_digit(*p)) {
            exp_any = 1;
            if (exp_val < 9L) {
                exp_val = (exp_val * 10L) + (long)(*p - '0');
            }
            ++p;
        }
        if (!exp_any) {
            return 0;
        }
        i = 0L;
        while (i < exp_val) {
            if (exp_sign > 0) {
                val = val * 10L;
            } else {
                val = val / 10L;
            }
            ++i;
        }
    }
    if (sign < 0) {
        val = -val;
    }
    *out = (GOBJ_Fixed)val;
    *pp = p;
    return 1;
}

static int gm_parse_vec3(const char *p, const char *end, GOBJ_Vec3 *out)
{
    if (!gm_parse_fixed(&p, end, &out->x)) return 0;
    if (!gm_parse_fixed(&p, end, &out->y)) return 0;
    if (!gm_parse_fixed(&p, end, &out->z)) return 0;
    return 1;
}

static void gm_material_clear(GOBJ_MtlMaterial *m)
{
    m->name[0] = '\0';
    m->ka.x = 0L; m->ka.y = 0L; m->ka.z = 0L;
    m->kd.x = 0L; m->kd.y = 0L; m->kd.z = 0L;
    m->ks.x = 0L; m->ks.y = 0L; m->ks.z = 0L;
    m->ke.x = 0L; m->ke.y = 0L; m->ke.z = 0L;
    m->tf.x = 0L; m->tf.y = 0L; m->tf.z = 0L;
    m->ns = 0L;
    m->ni = 0L;
    m->d = GOBJ_FIXED_ONE;
    m->tr = 0L;
    m->illum = 0;
    m->flags = 0u;
    m->map_ka[0] = '\0';
    m->map_kd[0] = '\0';
    m->map_ks[0] = '\0';
    m->map_ke[0] = '\0';
    m->map_d[0] = '\0';
    m->map_bump[0] = '\0';
}

static int gm_process_line(GOBJ_MtlLibrary *lib,
                           GOBJ_MtlMaterial **current,
                           const char *line,
                           const char *end,
                           GOBJ_MtlReport *report)
{
    const char *p;
    const char *k0;
    const char *k1;
    GOBJ_MtlMaterial *m;
    int iv;

    p = gm_skip_space(line, end);
    if (p >= end) {
        if (report != (GOBJ_MtlReport *)0) report->blank_lines = report->blank_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (*p == '#') {
        if (report != (GOBJ_MtlReport *)0) report->comment_lines = report->comment_lines + 1u;
        return GOBJ_MTL_OK;
    }
    k0 = p;
    while (p < end && !gm_is_space(*p) && *p != '#') {
        ++p;
    }
    k1 = p;
    p = gm_skip_space(p, end);

    if (gm_streq_range(k0, k1, "newmtl")) {
        if (lib->materials == (GOBJ_MtlMaterial *)0 || lib->material_count >= lib->material_cap) {
            return GOBJ_MTL_ERR_MATERIAL_CAP;
        }
        m = &lib->materials[lib->material_count];
        gm_material_clear(m);
        gm_copy_next_token(m->name, (unsigned int)GOBJ_NAME_MAX, p, end);
        lib->material_count = lib->material_count + 1u;
        *current = m;
        if (report != (GOBJ_MtlReport *)0) report->material_lines = report->material_lines + 1u;
        return GOBJ_MTL_OK;
    }

    m = *current;
    if (m == (GOBJ_MtlMaterial *)0) {
        if (report != (GOBJ_MtlReport *)0) report->ignored_lines = report->ignored_lines + 1u;
        return GOBJ_MTL_OK;
    }

    if (gm_streq_range(k0, k1, "Ka")) {
        if (!gm_parse_vec3(p, end, &m->ka)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_KA;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Kd")) {
        if (!gm_parse_vec3(p, end, &m->kd)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_KD;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Ks")) {
        if (!gm_parse_vec3(p, end, &m->ks)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_KS;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Ke")) {
        if (!gm_parse_vec3(p, end, &m->ke)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_KE;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Tf")) {
        if (!gm_parse_vec3(p, end, &m->tf)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_TF;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Ns")) {
        if (!gm_parse_fixed(&p, end, &m->ns)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_NS;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Ni")) {
        if (!gm_parse_fixed(&p, end, &m->ni)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_NI;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "d")) {
        if (!gm_parse_fixed(&p, end, &m->d)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_D;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "Tr")) {
        if (!gm_parse_fixed(&p, end, &m->tr)) return GOBJ_MTL_ERR_PARSE_NUMBER;
        m->flags = m->flags | GOBJ_MTL_HAS_TR;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "illum")) {
        if (!gm_parse_signed_int(&p, end, &iv)) return GOBJ_MTL_ERR_PARSE_INT;
        m->illum = iv;
        m->flags = m->flags | GOBJ_MTL_HAS_ILLUM;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_Ka")) {
        gm_copy_last_token(m->map_ka, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_KA;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_Kd")) {
        gm_copy_last_token(m->map_kd, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_KD;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_Ks")) {
        gm_copy_last_token(m->map_ks, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_KS;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_Ke")) {
        gm_copy_last_token(m->map_ke, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_KE;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_d")) {
        gm_copy_last_token(m->map_d, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_D;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }
    if (gm_streq_range(k0, k1, "map_bump") || gm_streq_range(k0, k1, "bump")) {
        gm_copy_last_token(m->map_bump, (unsigned int)GOBJ_PATH_MAX, p, end);
        m->flags = m->flags | GOBJ_MTL_HAS_MAP_BUMP;
        if (report != (GOBJ_MtlReport *)0) report->map_lines = report->map_lines + 1u;
        return GOBJ_MTL_OK;
    }

    if (report != (GOBJ_MtlReport *)0) report->ignored_lines = report->ignored_lines + 1u;
    return GOBJ_MTL_OK;
}

void gobj_mtl_report_clear(GOBJ_MtlReport *report)
{
    if (report == (GOBJ_MtlReport *)0) return;
    report->line = 0u;
    report->blank_lines = 0u;
    report->comment_lines = 0u;
    report->ignored_lines = 0u;
    report->material_lines = 0u;
    report->map_lines = 0u;
    report->last_error = GOBJ_MTL_OK;
}

void gobj_mtl_bind(GOBJ_MtlLibrary *lib,
                   GOBJ_MtlMaterial *materials,
                   unsigned int material_cap)
{
    if (lib == (GOBJ_MtlLibrary *)0) return;
    lib->materials = materials;
    lib->material_cap = material_cap;
    gobj_mtl_reset(lib);
}

void gobj_mtl_reset(GOBJ_MtlLibrary *lib)
{
    if (lib == (GOBJ_MtlLibrary *)0) return;
    lib->material_count = 0u;
}

int gobj_import_mtl_memory(GOBJ_MtlLibrary *lib,
                           const char *data,
                           unsigned long data_size,
                           GOBJ_MtlReport *report)
{
    const char *p;
    const char *end;
    const char *line;
    const char *line_end;
    GOBJ_MtlMaterial *current;
    int r;

    if (lib == (GOBJ_MtlLibrary *)0 || data == (const char *)0) {
        if (report != (GOBJ_MtlReport *)0) report->last_error = GOBJ_MTL_ERR_NULL;
        return GOBJ_MTL_ERR_NULL;
    }
    gobj_mtl_report_clear(report);
    gobj_mtl_reset(lib);
    current = (GOBJ_MtlMaterial *)0;
    p = data;
    end = data + data_size;
    while (p < end) {
        line = p;
        while (p < end && *p != '\n') {
            ++p;
        }
        line_end = p;
        if (line_end > line && *(line_end - 1) == '\r') {
            --line_end;
        }
        if (report != (GOBJ_MtlReport *)0) report->line = report->line + 1u;
        r = gm_process_line(lib, &current, line, line_end, report);
        if (r != GOBJ_MTL_OK) {
            if (report != (GOBJ_MtlReport *)0) report->last_error = r;
            return r;
        }
        if (p < end && *p == '\n') {
            ++p;
        }
    }
    if (report != (GOBJ_MtlReport *)0) report->last_error = GOBJ_MTL_OK;
    return GOBJ_MTL_OK;
}

const GOBJ_MtlMaterial *gobj_mtl_find(const GOBJ_MtlLibrary *lib,
                                      const char *name)
{
    unsigned int i;

    if (lib == (const GOBJ_MtlLibrary *)0 || name == (const char *)0) return (const GOBJ_MtlMaterial *)0;
    i = 0u;
    while (i < lib->material_count) {
        if (gm_name_equal(lib->materials[i].name, name)) {
            return &lib->materials[i];
        }
        ++i;
    }
    return (const GOBJ_MtlMaterial *)0;
}

const char *gobj_mtl_result_string(int code)
{
    switch (code) {
    case GOBJ_MTL_OK: return "ok";
    case GOBJ_MTL_ERR_NULL: return "null argument";
    case GOBJ_MTL_ERR_PARSE_NUMBER: return "numeric parse error";
    case GOBJ_MTL_ERR_PARSE_INT: return "integer parse error";
    case GOBJ_MTL_ERR_MATERIAL_CAP: return "material capacity exceeded";
    default: return "unknown error";
    }
}
