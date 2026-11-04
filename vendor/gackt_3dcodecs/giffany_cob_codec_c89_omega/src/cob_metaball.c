#include "cob_private.h"

/* Metaball field sampler + conservative block-surface polygonizer. */

static unsigned long mb_abs_fixed(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed mb_from_mag_sign(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed mb_mul(cob_fixed a, cob_fixed b)
{
    unsigned long am;
    unsigned long bm;
    unsigned long aw;
    unsigned long af;
    unsigned long bw;
    unsigned long bf;
    unsigned long out;
    int sign;
    sign = 1;
    if (a < 0) sign = -sign;
    if (b < 0) sign = -sign;
    am = mb_abs_fixed(a);
    bm = mb_abs_fixed(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += bw * af;
    out += (af * bf) >> 16;
    return mb_from_mag_sign(out, sign);
}

static cob_fixed mb_div(cob_fixed num, cob_fixed den)
{
    unsigned long n;
    unsigned long d;
    unsigned long whole;
    unsigned long rem;
    unsigned long out;
    int sign;
    if (den == 0) return 0;
    sign = 1;
    if (num < 0) sign = -sign;
    if (den < 0) sign = -sign;
    n = mb_abs_fixed(num);
    d = mb_abs_fixed(den);
    whole = n / d;
    rem = n % d;
    if (whole > 32767UL) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (whole << 16) + ((rem << 16) / d);
    return mb_from_mag_sign(out, sign);
}

static cob_fixed mb_div_u(cob_fixed num, unsigned long den)
{
    unsigned long n;
    unsigned long out;
    int sign;
    if (den == 0UL) return 0;
    sign = num < 0 ? -1 : 1;
    n = mb_abs_fixed(num);
    out = n / den;
    return mb_from_mag_sign(out, sign);
}

void cob_metaball_field_clear(cob_metaball_field *field)
{
    if (field == 0) return;
    memset(field, 0, sizeof(*field));
    field->threshold = COB_FIXED_ONE;
    field->minv.x = -COB_FIXED_ONE;
    field->minv.y = -COB_FIXED_ONE;
    field->minv.z = -COB_FIXED_ONE;
    field->maxv.x = COB_FIXED_ONE;
    field->maxv.y = COB_FIXED_ONE;
    field->maxv.z = COB_FIXED_ONE;
}

int cob_metaball_field_add(cob_metaball_field *field, const cob_vec3q *center, cob_fixed radius, cob_fixed strength)
{
    cob_metaball *b;
    if (field == 0 || center == 0) return COB_ERR_ARGUMENT;
    if (field->ball_count >= COB_MAX_METABALLS) return COB_ERR_CAPACITY;
    b = &field->balls[field->ball_count++];
    b->center = *center;
    b->radius = radius;
    b->strength = strength;
    return COB_OK;
}

cob_fixed cob_metaball_field_eval(const cob_metaball_field *field, const cob_vec3q *p)
{
    unsigned long i;
    cob_fixed sum;
    cob_fixed dx;
    cob_fixed dy;
    cob_fixed dz;
    cob_fixed d2;
    cob_fixed r2;
    cob_fixed denom;
    cob_fixed num;
    const cob_metaball *b;
    if (field == 0 || p == 0) return 0;
    sum = 0;
    for (i = 0; i < field->ball_count; ++i) {
        b = &field->balls[i];
        dx = p->x - b->center.x;
        dy = p->y - b->center.y;
        dz = p->z - b->center.z;
        d2 = mb_mul(dx, dx) + mb_mul(dy, dy) + mb_mul(dz, dz);
        r2 = mb_mul(b->radius, b->radius);
        denom = d2 + 1024L; /* tiny epsilon in Q16 */
        num = mb_mul(b->strength, r2);
        sum += mb_div(num, denom);
    }
    return sum;
}

static int mb_inside(const cob_metaball_field *field, int x, int y, int z, unsigned int grid, cob_fixed sx, cob_fixed sy, cob_fixed sz)
{
    cob_vec3q p;
    if (x < 0 || y < 0 || z < 0) return 0;
    if ((unsigned int)x >= grid || (unsigned int)y >= grid || (unsigned int)z >= grid) return 0;
    p.x = field->minv.x + mb_mul(sx, ((cob_fixed)x * COB_FIXED_ONE + COB_FIXED_HALF));
    p.y = field->minv.y + mb_mul(sy, ((cob_fixed)y * COB_FIXED_ONE + COB_FIXED_HALF));
    p.z = field->minv.z + mb_mul(sz, ((cob_fixed)z * COB_FIXED_ONE + COB_FIXED_HALF));
    return cob_metaball_field_eval(field, &p) >= field->threshold;
}

static int mb_add_quad(cob_scene *scene, cob_mesh *m, cob_vec3q q[4])
{
    cob_face *f;
    unsigned long base;
    int k;
    if (scene->vertex_count + 4UL > COB_MAX_VERTICES) return COB_ERR_CAPACITY;
    if (scene->uv_count + 4UL > COB_MAX_UVS) return COB_ERR_CAPACITY;
    if (scene->face_count + 1UL > COB_MAX_FACES) return COB_ERR_CAPACITY;
    if (scene->index_count + 4UL > COB_MAX_INDICES) return COB_ERR_CAPACITY;
    base = scene->vertex_count - m->first_vertex;
    for (k = 0; k < 4; ++k) {
        scene->vertices[scene->vertex_count++] = q[k];
        if (m->uv_count == 0UL) m->first_uv = scene->uv_count;
        scene->uvs[scene->uv_count].u = 0;
        scene->uvs[scene->uv_count].v = 0;
        ++scene->uv_count;
        ++m->vertex_count;
        ++m->uv_count;
    }
    f = &scene->faces[scene->face_count++];
    memset(f, 0, sizeof(*f));
    f->index_count = 4U;
    f->material = 0;
    f->first_index = scene->index_count;
    for (k = 0; k < 4; ++k) {
        scene->indices[scene->index_count].pos_index = (cob_i32)(base + (unsigned long)k);
        scene->indices[scene->index_count].uv_index = (cob_i32)(base + (unsigned long)k);
        ++scene->index_count;
    }
    ++m->face_count;
    return COB_OK;
}

static int mb_emit_side(cob_scene *scene, cob_mesh *m, cob_fixed x0, cob_fixed y0, cob_fixed z0, cob_fixed x1, cob_fixed y1, cob_fixed z1, int side)
{
    cob_vec3q q[4];
    if (side == 0) {
        q[0].x = x0; q[0].y = y0; q[0].z = z0;
        q[1].x = x0; q[1].y = y1; q[1].z = z0;
        q[2].x = x0; q[2].y = y1; q[2].z = z1;
        q[3].x = x0; q[3].y = y0; q[3].z = z1;
    } else if (side == 1) {
        q[0].x = x1; q[0].y = y0; q[0].z = z1;
        q[1].x = x1; q[1].y = y1; q[1].z = z1;
        q[2].x = x1; q[2].y = y1; q[2].z = z0;
        q[3].x = x1; q[3].y = y0; q[3].z = z0;
    } else if (side == 2) {
        q[0].x = x1; q[0].y = y0; q[0].z = z0;
        q[1].x = x0; q[1].y = y0; q[1].z = z0;
        q[2].x = x0; q[2].y = y0; q[2].z = z1;
        q[3].x = x1; q[3].y = y0; q[3].z = z1;
    } else if (side == 3) {
        q[0].x = x0; q[0].y = y1; q[0].z = z0;
        q[1].x = x1; q[1].y = y1; q[1].z = z0;
        q[2].x = x1; q[2].y = y1; q[2].z = z1;
        q[3].x = x0; q[3].y = y1; q[3].z = z1;
    } else if (side == 4) {
        q[0].x = x0; q[0].y = y0; q[0].z = z0;
        q[1].x = x1; q[1].y = y0; q[1].z = z0;
        q[2].x = x1; q[2].y = y1; q[2].z = z0;
        q[3].x = x0; q[3].y = y1; q[3].z = z0;
    } else {
        q[0].x = x0; q[0].y = y1; q[0].z = z1;
        q[1].x = x1; q[1].y = y1; q[1].z = z1;
        q[2].x = x1; q[2].y = y0; q[2].z = z1;
        q[3].x = x0; q[3].y = y0; q[3].z = z1;
    }
    return mb_add_quad(scene, m, q);
}

int cob_metaball_field_polygonize_to_scene(cob_scene *scene, const cob_metaball_field *field, const char *mesh_name, unsigned int grid)
{
    cob_mesh *m;
    unsigned int x;
    unsigned int y;
    unsigned int z;
    cob_fixed sx;
    cob_fixed sy;
    cob_fixed sz;
    cob_fixed x0;
    cob_fixed y0;
    cob_fixed z0;
    cob_fixed x1;
    cob_fixed y1;
    cob_fixed z1;
    int st;

    if (scene == 0 || field == 0) return COB_ERR_ARGUMENT;
    if (grid < 2U) return COB_ERR_ARGUMENT;
    if (grid > 32U) grid = 32U;
    if (scene->mesh_count >= COB_MAX_MESHES) return COB_ERR_CAPACITY;
    m = &scene->meshes[scene->mesh_count];
    memset(m, 0, sizeof(*m));
    cob_zero_axes(m->axes);
    cob_set_identity(m->transform);
    m->id = 2000000L + (cob_i32)scene->mesh_count;
    m->parent_id = 0;
    m->major = 0;
    m->minor = 8;
    m->unit_scale = COB_FIXED_ONE;
    cob_copy_name(m->name, sizeof(m->name), mesh_name != 0 ? mesh_name : "Metaball_field", cob_strlen_ul(mesh_name != 0 ? mesh_name : "Metaball_field"));
    m->first_vertex = scene->vertex_count;
    m->first_uv = scene->uv_count;
    m->first_face = scene->face_count;
    ++scene->mesh_count;

    sx = mb_div_u(field->maxv.x - field->minv.x, grid);
    sy = mb_div_u(field->maxv.y - field->minv.y, grid);
    sz = mb_div_u(field->maxv.z - field->minv.z, grid);
    for (z = 0U; z < grid; ++z) {
        for (y = 0U; y < grid; ++y) {
            for (x = 0U; x < grid; ++x) {
                if (!mb_inside(field, (int)x, (int)y, (int)z, grid, sx, sy, sz)) continue;
                x0 = field->minv.x + mb_mul(sx, (cob_fixed)x * COB_FIXED_ONE);
                y0 = field->minv.y + mb_mul(sy, (cob_fixed)y * COB_FIXED_ONE);
                z0 = field->minv.z + mb_mul(sz, (cob_fixed)z * COB_FIXED_ONE);
                x1 = x0 + sx;
                y1 = y0 + sy;
                z1 = z0 + sz;
                if (!mb_inside(field, (int)x - 1, (int)y, (int)z, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 0); if (st != COB_OK) return st; }
                if (!mb_inside(field, (int)x + 1, (int)y, (int)z, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 1); if (st != COB_OK) return st; }
                if (!mb_inside(field, (int)x, (int)y - 1, (int)z, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 2); if (st != COB_OK) return st; }
                if (!mb_inside(field, (int)x, (int)y + 1, (int)z, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 3); if (st != COB_OK) return st; }
                if (!mb_inside(field, (int)x, (int)y, (int)z - 1, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 4); if (st != COB_OK) return st; }
                if (!mb_inside(field, (int)x, (int)y, (int)z + 1, grid, sx, sy, sz)) { st = mb_emit_side(scene, m, x0, y0, z0, x1, y1, z1, 5); if (st != COB_OK) return st; }
            }
        }
    }
    return COB_OK;
}
