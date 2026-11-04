#include "bravure_bvh_internal.h"

void bbvh_mat_identity(bbvh_mat4 *m)
{
    int i;

    if (m == (bbvh_mat4 *)0) {
        return;
    }
    for (i = 0; i < 16; ++i) {
        m->m[i] = 0L;
    }
    m->m[0] = BBVH_FIXED_ONE;
    m->m[5] = BBVH_FIXED_ONE;
    m->m[10] = BBVH_FIXED_ONE;
    m->m[15] = BBVH_FIXED_ONE;
}

void bbvh_mat_mul(bbvh_mat4 *out_m, const bbvh_mat4 *a, const bbvh_mat4 *b)
{
    bbvh_mat4 r;
    int row;
    int col;
    int k;
    bbvh_fixed sum;
    bbvh_fixed term;

    if (out_m == (bbvh_mat4 *)0 || a == (const bbvh_mat4 *)0 || b == (const bbvh_mat4 *)0) {
        return;
    }

    for (row = 0; row < 4; ++row) {
        for (col = 0; col < 4; ++col) {
            sum = 0L;
            for (k = 0; k < 4; ++k) {
                term = bbvh_fixed_mul(a->m[row * 4 + k], b->m[k * 4 + col]);
                sum = bbvh_fixed_add_sat(sum, term);
            }
            r.m[row * 4 + col] = sum;
        }
    }
    for (row = 0; row < 16; ++row) {
        out_m->m[row] = r.m[row];
    }
}

void bbvh_mat_translation(bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y, bbvh_fixed z)
{
    bbvh_mat_identity(m);
    if (m == (bbvh_mat4 *)0) {
        return;
    }
    m->m[3] = x;
    m->m[7] = y;
    m->m[11] = z;
}

void bbvh_mat_rotation_x(bbvh_mat4 *m, bbvh_fixed degrees_q16)
{
    bbvh_fixed s;
    bbvh_fixed c;

    if (m == (bbvh_mat4 *)0) {
        return;
    }
    s = bbvh_fixed_sin_deg(degrees_q16);
    c = bbvh_fixed_cos_deg(degrees_q16);
    bbvh_mat_identity(m);
    m->m[5] = c;
    m->m[6] = -s;
    m->m[9] = s;
    m->m[10] = c;
}

void bbvh_mat_rotation_y(bbvh_mat4 *m, bbvh_fixed degrees_q16)
{
    bbvh_fixed s;
    bbvh_fixed c;

    if (m == (bbvh_mat4 *)0) {
        return;
    }
    s = bbvh_fixed_sin_deg(degrees_q16);
    c = bbvh_fixed_cos_deg(degrees_q16);
    bbvh_mat_identity(m);
    m->m[0] = c;
    m->m[2] = s;
    m->m[8] = -s;
    m->m[10] = c;
}

void bbvh_mat_rotation_z(bbvh_mat4 *m, bbvh_fixed degrees_q16)
{
    bbvh_fixed s;
    bbvh_fixed c;

    if (m == (bbvh_mat4 *)0) {
        return;
    }
    s = bbvh_fixed_sin_deg(degrees_q16);
    c = bbvh_fixed_cos_deg(degrees_q16);
    bbvh_mat_identity(m);
    m->m[0] = c;
    m->m[1] = -s;
    m->m[4] = s;
    m->m[5] = c;
}

void bbvh_mat_apply_point(const bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y,
                          bbvh_fixed z, bbvh_fixed out_xyz[3])
{
    bbvh_fixed rx;
    bbvh_fixed ry;
    bbvh_fixed rz;

    if (m == (const bbvh_mat4 *)0 || out_xyz == (bbvh_fixed *)0) {
        return;
    }
    rx = bbvh_fixed_add_sat(bbvh_fixed_mul(m->m[0], x), bbvh_fixed_mul(m->m[1], y));
    rx = bbvh_fixed_add_sat(rx, bbvh_fixed_mul(m->m[2], z));
    rx = bbvh_fixed_add_sat(rx, m->m[3]);

    ry = bbvh_fixed_add_sat(bbvh_fixed_mul(m->m[4], x), bbvh_fixed_mul(m->m[5], y));
    ry = bbvh_fixed_add_sat(ry, bbvh_fixed_mul(m->m[6], z));
    ry = bbvh_fixed_add_sat(ry, m->m[7]);

    rz = bbvh_fixed_add_sat(bbvh_fixed_mul(m->m[8], x), bbvh_fixed_mul(m->m[9], y));
    rz = bbvh_fixed_add_sat(rz, bbvh_fixed_mul(m->m[10], z));
    rz = bbvh_fixed_add_sat(rz, m->m[11]);

    out_xyz[0] = rx;
    out_xyz[1] = ry;
    out_xyz[2] = rz;
}
