/*
   emd_textured_viewer.c - RE1 EMD -> textured BMP preview
   v11: RE1MV EDD/EMR + EMR PartList hierarchy path.

   Pipeline:
     load_emd(); build_piece_transforms(); assemble_world_vertices(); decode_tim();
     project_vertices(); rasterize_textured_triangles(); save_framebuffer_to_bmp();

   Important v07 fixes:
     - reads CBA/CLUT per primitive
     - reads TSB/TPAGE per primitive
     - samples TIM indexed pixels directly instead of using one global RGBA palette
     - preserves metadata when splitting FT4/GT4 quads internally
     - differentiates RE1 TMD FT3/GT3/FT4/GT4 packet layouts where present
     - v11 defaults animation hierarchy to EMR PartList child table, matching RE1MV ANI export

   C89-ish standalone diagnostic tool using giffy_recodec public model API
   for geometry plus an internal RE1 TMD primitive pass for exact texture refs.
*/
#include "giffy_recodec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static float mf_min(float a,float b){return a<b?a:b;}
static float mf_max(float a,float b){return a>b?a:b;}

#define FB_W 320
#define FB_H 240
#define BIGF 1000000000.0f
#define MAX_RE1_SECTIONS 6
#define MAX_RE1_PIECES   128

typedef struct v3_s { float x,y,z; } v3;
typedef struct p2_s { float x,y,z,u,v; int valid; } p2;
typedef struct m4_s { float v[16]; } m4;
typedef struct fb_s { unsigned char *rgb; float *z; int w,h; } fb;
typedef struct sec_s { unsigned int off,size; } sec;

typedef struct re1_texref_s {
    unsigned char u[4];
    unsigned char v[4];
    unsigned short cba;
    unsigned short tsb;
    unsigned short primitive_code_lo;
    unsigned char sides;
    unsigned char valid;
    unsigned char gouraud;
    unsigned char textured;
} re1_texref;

typedef struct re1_tim_raw_s {
    const unsigned char *data;
    size_t size;
    unsigned int type;       /* 0=4bpp, 1=8bpp, 2=16bpp, 3=24bpp */
    unsigned int has_clut;
    const unsigned char *clut;
    unsigned int clut_x;
    unsigned int clut_y;
    unsigned int clut_w;
    unsigned int clut_h;
    const unsigned char *pix;
    unsigned int pix_x;
    unsigned int pix_y;
    unsigned int pix_w_words;
    unsigned int pix_h;
    unsigned int width_pixels;
    unsigned int height_pixels;
} re1_tim_raw;

typedef struct emd_loaded_s {
    grc_model model;
    grc_texture tex_fallback;
    sec sections[MAX_RE1_SECTIONS];
    int section_count;
    v3 local_pivot[64];
    v3 world_pivot[64];
    m4 world_mtx[64];
    v3 anim_root;
    int pose_angles[64][3];
    int parent[64];
    unsigned int emd_type;
    int has_bind;
    int use_anim;
    int used_re1mv_build_model;
    int used_emr_hierarchy;
    int has_emr_hierarchy;
    int force_legacy_build_model;
    unsigned char *file_data;
    size_t file_size;
    re1_tim_raw raw_tim;
    re1_texref *texrefs;     /* aligned to model.faces[] */
    unsigned int texref_count;
    int cli_frame;
    int cli_bank;
    int cli_clip;
    int cli_anim_requested;
    unsigned int cli_pose_index;
    unsigned int cli_clip_frames;
    unsigned int cli_frame_step;
    int cli_palette;
    int force_global_rgba;
    int fixed_bounds;
    int dump_bounds;
    v3 fixed_mn;
    v3 fixed_mx;
} emd_loaded;

static unsigned int rd32(const unsigned char *p) { return ((unsigned int)p[0]) | ((unsigned int)p[1]<<8) | ((unsigned int)p[2]<<16) | ((unsigned int)p[3]<<24); }
static unsigned int rd16(const unsigned char *p) { return ((unsigned int)p[0]) | ((unsigned int)p[1]<<8); }
static int range_ok(size_t pos, size_t len, size_t size) { return pos <= size && len <= size - pos; }

static int load_file(const char *path, unsigned char **out, size_t *out_size)
{
    FILE *f; long n; unsigned char *b;
    *out = NULL; *out_size = 0u;
    f = fopen(path, "rb"); if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f); if (n <= 0) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    b = (unsigned char*)malloc((size_t)n); if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return 0; }
    fclose(f); *out = b; *out_size = (size_t)n; return 1;
}

static int read_re1_end_sections(const unsigned char *data, size_t size, sec out_secs[MAX_RE1_SECTIONS], int *out_count)
{
    unsigned int o5[5];
    unsigned int o4[4];
    unsigned int dir_start;
    int i;
    (void)data;
    if (size >= 20u) {
        dir_start = (unsigned int)(size - 20u);
        for (i=0;i<5;i++) o5[i] = rd32(data + size - 20u + (size_t)i*4u);
        for (i=0;i<5;i++) {
            if (o5[i] >= dir_start) break;
            if (i>0 && o5[i] < o5[i-1]) break;
        }
        if (i == 5 && o5[0] > 0u) {
            out_secs[0].off = 0u;     out_secs[0].size = o5[0];
            out_secs[1].off = o5[0];  out_secs[1].size = o5[1] - o5[0];
            out_secs[2].off = o5[1];  out_secs[2].size = o5[2] - o5[1];
            out_secs[3].off = o5[2];  out_secs[3].size = o5[3] - o5[2];
            out_secs[4].off = o5[3];  out_secs[4].size = o5[4] - o5[3];
            out_secs[5].off = o5[4];  out_secs[5].size = dir_start - o5[4];
            *out_count = 6;
            return 1;
        }
    }
    if (size < 16u) return 0;
    for (i=0;i<4;i++) o4[i] = rd32(data + size - 16u + (size_t)i*4u);
    for (i=0;i<4;i++) { if ((size_t)o4[i] >= size) return 0; if (i>0 && o4[i] < o4[i-1]) return 0; }
    for (i=0;i<4;i++) { out_secs[i].off = o4[i]; out_secs[i].size = (i<3) ? (o4[i+1]-o4[i]) : ((unsigned int)size - 16u - o4[i]); }
    *out_count = 4;
    return 1;
}

static void psx1555_to_rgba(unsigned short c, unsigned char out[4])
{
    out[0] = (unsigned char)(((c & 31u) * 255u) / 31u);
    out[1] = (unsigned char)((((c >> 5) & 31u) * 255u) / 31u);
    out[2] = (unsigned char)((((c >> 10) & 31u) * 255u) / 31u);
    out[3] = (c == 0u) ? 0u : 255u;
}

static int parse_tim_raw(const unsigned char *data, size_t size, re1_tim_raw *t)
{
    unsigned int flags, sec_size;
    size_t pos;
    memset(t, 0, sizeof(*t));
    if (data == NULL || size < 20u || data[0] != 0x10u || data[1] != 0u) return 0;
    flags = rd32(data + 4u);
    t->type = flags & 7u;
    t->has_clut = (flags & 8u) ? 1u : 0u;
    pos = 8u;
    if (t->has_clut) {
        if (!range_ok(pos, 12u, size)) return 0;
        sec_size = rd32(data + pos);
        if (sec_size < 12u || !range_ok(pos, sec_size, size)) return 0;
        t->clut_x = rd16(data + pos + 4u);
        t->clut_y = rd16(data + pos + 6u);
        t->clut_w = rd16(data + pos + 8u);
        t->clut_h = rd16(data + pos + 10u);
        t->clut = data + pos + 12u;
        pos += sec_size;
    }
    if (!range_ok(pos, 12u, size)) return 0;
    sec_size = rd32(data + pos);
    if (sec_size < 12u || !range_ok(pos, sec_size, size)) return 0;
    t->pix_x = rd16(data + pos + 4u);
    t->pix_y = rd16(data + pos + 6u);
    t->pix_w_words = rd16(data + pos + 8u);
    t->pix_h = rd16(data + pos + 10u);
    t->pix = data + pos + 12u;
    t->data = data;
    t->size = size;
    t->height_pixels = t->pix_h;
    if (t->type == 0u) t->width_pixels = t->pix_w_words * 4u;
    else if (t->type == 1u) t->width_pixels = t->pix_w_words * 2u;
    else if (t->type == 2u) t->width_pixels = t->pix_w_words;
    else if (t->type == 3u) t->width_pixels = (t->pix_w_words * 2u) / 3u;
    else return 0;
    return 1;
}

static int tim_read_index(const re1_tim_raw *tim, int x, int y)
{
    const unsigned char *row;
    unsigned char packed;
    if (x < 0 || y < 0 || x >= (int)tim->width_pixels || y >= (int)tim->height_pixels) return 0;
    if (tim->type == 0u) {
        row = tim->pix + (size_t)y * (size_t)tim->pix_w_words * 2u;
        packed = row[x >> 1];
        return (x & 1) ? ((packed >> 4) & 15) : (packed & 15);
    }
    if (tim->type == 1u) {
        row = tim->pix + (size_t)y * (size_t)tim->pix_w_words * 2u;
        return row[x];
    }
    return 0;
}

static void sample_tim_raw(const re1_tim_raw *tim, const re1_texref *tr, float uf, float vf, unsigned char out[4])
{
    int u, v, page_no, page_x, page_y, sx, sy;
    int clut_x, clut_y, pal_row, pal_col, idx, colors;
    unsigned short c;
    out[0]=out[1]=out[2]=0; out[3]=0;
    if (tim == NULL || tim->pix == NULL || tr == NULL || tr->valid == 0u) return;
    u = (int)(uf + 0.5f); v = (int)(vf + 0.5f);
    if (u < 0) u = 0;
    if (u > 255) u = 255;
    if (v < 0) v = 0;
    if (v > 255) v = 255;

    /* PSX TSB texture page: low bits are texture-page X. For 8bpp one page step
       is 64 VRAM words = 128 pixels in the decoded TIM. This is the exact area
       RE1MV approximated by adding Rp=0.5 to the normalized U coordinate. */
    page_no = (int)(tr->tsb & 0x0Fu);
    if (tim->type == 0u) page_x = page_no * 256;        /* 4bpp: 64 words = 256 pixels */
    else if (tim->type == 1u) page_x = page_no * 128;   /* 8bpp: 64 words = 128 pixels */
    else page_x = page_no * 64;
    page_y = ((tr->tsb >> 4) & 1u) ? 256 : 0;

    sx = page_x + u;
    sy = page_y + v;
    if (sx < 0 || sy < 0 || sx >= (int)tim->width_pixels || sy >= (int)tim->height_pixels) {
        /* RE1 assets often use page-local UVs, so do not clamp across pages unless
           the source TIM is smaller than the theoretical page. */
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= (int)tim->width_pixels) sx = (int)tim->width_pixels - 1;
        if (sy >= (int)tim->height_pixels) sy = (int)tim->height_pixels - 1;
    }

    if (tim->type == 0u || tim->type == 1u) {
        if (tim->clut == NULL || tim->clut_w == 0u || tim->clut_h == 0u) return;
        clut_x = (int)((tr->cba & 0x3Fu) * 16u);
        clut_y = (int)((tr->cba >> 6u) & 0x1FFu);
        pal_row = clut_y - (int)tim->clut_y;
        pal_col = clut_x - (int)tim->clut_x;
        colors = (tim->type == 0u) ? 16 : 256;
        if (pal_row < 0 || pal_row >= (int)tim->clut_h) pal_row = 0;
        if (pal_col < 0 || pal_col + colors > (int)tim->clut_w) pal_col = 0;
        idx = tim_read_index(tim, sx, sy);
        if (idx < 0 || idx >= colors) idx = 0;
        c = rd16(tim->clut + ((size_t)pal_row * (size_t)tim->clut_w + (size_t)(pal_col + idx)) * 2u);
        psx1555_to_rgba(c, out);
    } else if (tim->type == 2u) {
        if (sx < 0 || sy < 0 || sx >= (int)tim->pix_w_words || sy >= (int)tim->pix_h) return;
        c = rd16(tim->pix + ((size_t)sy * (size_t)tim->pix_w_words + (size_t)sx) * 2u);
        psx1555_to_rgba(c, out);
    } else {
        out[0]=out[1]=out[2]=255; out[3]=255;
    }
}

static void set_default_human_parents(emd_loaded *e)
{
    static int p15[15] = { -1,0,1,1,3,4,1,6,7,0,9,10,0,12,13 };
    unsigned int i;
    for (i=0;i<64;i++) e->parent[i] = -1;
    for (i=0;i<e->model.piece_count && i<15u;i++) e->parent[i] = p15[i];
}

static void parse_re1_texrefs_from_tmd(emd_loaded *e, const unsigned char *sec, size_t sec_size)
{
    unsigned int obj_count, oi, face_cursor;
    if (e == NULL || sec == NULL || sec_size < 12u) return;
    obj_count = rd32(sec + 8u);
    if (obj_count == 0u || obj_count > 2048u || !range_ok(12u, (size_t)obj_count * 28u, sec_size)) return;
    e->texrefs = (re1_texref*)calloc((size_t)e->model.face_count, sizeof(re1_texref));
    if (e->texrefs == NULL) return;
    e->texref_count = e->model.face_count;
    face_cursor = 0u;
    for (oi = 0u; oi < obj_count; ++oi) {
        size_t dpos;
        unsigned int prim_offset, prim_count, pi;
        dpos = 12u + (size_t)oi * 28u;
        prim_offset = rd32(sec + dpos + 16u);
        prim_count = rd32(sec + dpos + 20u);
        for (pi = 0u; pi < prim_count && face_cursor < e->model.face_count; ++pi) {
            const unsigned char *p;
            unsigned int code;
            re1_texref *tr;
            size_t pos;
            pos = 12u + (size_t)prim_offset + (size_t)pi * 28u;
            if (!range_ok(pos, 28u, sec_size)) break;
            p = sec + pos;
            code = rd32(p);
            tr = &e->texrefs[face_cursor];
            memset(tr, 0, sizeof(*tr));
            tr->valid = 1u;
            tr->textured = 1u;
            tr->primitive_code_lo = (unsigned short)(code & 0xffffu);
            if (code == 0x34000609u || code == 0x30000609u || code == 0x24000609u) {
                /* RE1MV FT3/GT3-like packed primitive. */
                tr->sides = 3u;
                tr->u[0]=p[4];  tr->v[0]=p[5];  tr->cba=(unsigned short)rd16(p+6u);
                tr->u[1]=p[8];  tr->v[1]=p[9];  tr->tsb=(unsigned short)rd16(p+10u);
                tr->u[2]=p[12]; tr->v[2]=p[13];
                tr->gouraud = ((code & 0x10000000u) != 0u) ? 1u : 0u;
            } else if (code == 0x3C00080Cu || code == 0x3800080Cu || code == 0x2C00080Cu) {
                /* RE1MV FT4/GT4-like packed primitive. */
                tr->sides = 4u;
                tr->u[0]=p[4];  tr->v[0]=p[5];  tr->cba=(unsigned short)rd16(p+6u);
                tr->u[1]=p[8];  tr->v[1]=p[9];  tr->tsb=(unsigned short)rd16(p+10u);
                tr->u[2]=p[12]; tr->v[2]=p[13];
                tr->u[3]=p[16]; tr->v[3]=p[17];
                tr->gouraud = ((code & 0x10000000u) != 0u) ? 1u : 0u;
            } else {
                /* Fallback to the known RE1 triangle offsets. */
                tr->sides = 3u;
                tr->u[0]=p[4];  tr->v[0]=p[5];  tr->cba=(unsigned short)rd16(p+6u);
                tr->u[1]=p[8];  tr->v[1]=p[9];  tr->tsb=(unsigned short)rd16(p+10u);
                tr->u[2]=p[12]; tr->v[2]=p[13];
            }
            face_cursor++;
        }
    }
}


static void m4_identity(m4 *m)
{
    int i;
    for (i=0;i<16;i++) m->v[i] = 0.0f;
    m->v[0]=1.0f; m->v[5]=1.0f; m->v[10]=1.0f; m->v[15]=1.0f;
}

static void m4_mul(m4 *out, const m4 *a, const m4 *b)
{
    m4 r;
    int row, col;
    for (row=0; row<4; row++) {
        for (col=0; col<4; col++) {
            r.v[col*4+row] =
                a->v[0*4+row] * b->v[col*4+0] +
                a->v[1*4+row] * b->v[col*4+1] +
                a->v[2*4+row] * b->v[col*4+2] +
                a->v[3*4+row] * b->v[col*4+3];
        }
    }
    *out = r;
}

static void m4_translate(m4 *m, float x, float y, float z)
{
    m4_identity(m);
    m->v[12]=x; m->v[13]=y; m->v[14]=z;
}

static void m4_from_euler_psx(m4 *out, int ax, int ay, int az)
{
    float x, y, z, cx, sx, cy, sy, cz, sz;
    m4 rx, ry, rz, tmp;
    /* RE1MV converts (angle & 0x0FFF) / 0x0FFF * 360. */
    x = ((float)ax * 6.28318530718f) / 4095.0f;
    y = ((float)ay * 6.28318530718f) / 4095.0f;
    z = ((float)az * 6.28318530718f) / 4095.0f;
    cx = (float)cos((double)x); sx = (float)sin((double)x);
    cy = (float)cos((double)y); sy = (float)sin((double)y);
    cz = (float)cos((double)z); sz = (float)sin((double)z);
    m4_identity(&rx); m4_identity(&ry); m4_identity(&rz);
    rx.v[5]=cx;  rx.v[6]=sx;  rx.v[9]=-sx; rx.v[10]=cx;
    ry.v[0]=cy;  ry.v[2]=-sy; ry.v[8]=sy;  ry.v[10]=cy;
    rz.v[0]=cz;  rz.v[1]=sz;  rz.v[4]=-sz; rz.v[5]=cz;
    m4_mul(&tmp, &rz, &ry);
    m4_mul(out, &tmp, &rx);
}

static v3 m4_transform_point(const m4 *m, v3 p)
{
    v3 r;
    r.x = m->v[0]*p.x + m->v[4]*p.y + m->v[8]*p.z + m->v[12];
    r.y = m->v[1]*p.x + m->v[5]*p.y + m->v[9]*p.z + m->v[13];
    r.z = m->v[2]*p.x + m->v[6]*p.y + m->v[10]*p.z + m->v[14];
    return r;
}

static float re1mv_calc_pos(unsigned int w)
{
    /* Mirrors RE1MV Calculate_Pos(): PSX coords are effectively sign-inverted
       and scaled for GL preview. Keep the 65535 quirk for source parity. */
    w &= 0xffffu;
    if (w > 32767u) return (float)(65535u - w) / 100.0f;
    return -((float)w) / 100.0f;
}

static int re1mv_neg12(unsigned int w)
{
    w &= 0x0fffu;
    return -(int)w;
}


static int re1mv_check_id_from_emr(const unsigned char *emr, unsigned int emr_size, unsigned int *out_type)
{
    unsigned int part_list_off, object_count, i, result;
    if (out_type != NULL) *out_type = 0u;
    if (emr == NULL || emr_size < 8u) return 0;
    part_list_off = rd16(emr + 0u);
    object_count = rd16(emr + 4u);
    if (object_count == 0u || object_count > 64u) return 0;
    if (part_list_off >= emr_size || part_list_off + object_count * 4u > emr_size) return 0;
    result = 0u;
    for (i = 0u; i < object_count; ++i) {
        result ^= rd32(emr + part_list_off + i * 4u);
    }
    if (out_type != NULL) *out_type = result;
    return 1;
}

static int re1mv_load_bind_from_emr(emd_loaded *e, unsigned int emr_index)
{
    const unsigned char *emr;
    unsigned int emr_off, emr_size, object_count, part_list_off, i, emd_type;
    if (e == NULL || e->section_count < 1) return 0;
    if (emr_index >= (unsigned int)e->section_count) return 0;
    emr_off = e->sections[emr_index].off;
    emr_size = e->sections[emr_index].size;
    if (emr_off + emr_size > (unsigned int)e->file_size) return 0;
    emr = e->file_data + emr_off;
    if (emr_size < 8u) return 0;
    part_list_off = rd16(emr + 0u);
    object_count = rd16(emr + 4u);
    if (object_count == 0u || object_count > 64u) return 0;
    if (8u + object_count * 6u > emr_size) return 0;

    for (i = 0u; i < 64u; ++i) {
        e->local_pivot[i].x = 0.0f;
        e->local_pivot[i].y = 0.0f;
        e->local_pivot[i].z = 0.0f;
        e->parent[i] = -1;
    }

    for (i = 0u; i < object_count && i < 64u; ++i) {
        const unsigned char *s = emr + 8u + i * 6u;
        /* RE1MV Load_Ans(): file triplet is Z,Y,X, stored for glTranslatef(X,Y,Z). */
        e->local_pivot[i].z = re1mv_calc_pos(rd16(s + 0u));
        e->local_pivot[i].y = re1mv_calc_pos(rd16(s + 2u));
        e->local_pivot[i].x = re1mv_calc_pos(rd16(s + 4u));
    }

    /*
       RE1MV EMR2ANI exports the PartListOffset table as the skeleton tree:
         LS[i].Count, LS[i].Offset; children are bytes at PartListOffset+Offset.
       The older diagnostic renderer ignored this table and forced the Pascal
       Build_Model stack.  EM1111 proves why that is wrong: EDD/EMR pose values
       match RE1MV ANI exactly, but the hardcoded stack parents split torso/legs.
       Prefer this byte table; keep hardcoded Build_Model only as opt-in legacy.
    */
    if (part_list_off > 0u && part_list_off + object_count * 4u <= emr_size) {
        unsigned int valid_edges;
        valid_edges = 0u;
        for (i = 0u; i < object_count && i < 64u; ++i) {
            unsigned int count;
            unsigned int off;
            unsigned int j;
            count = rd16(emr + part_list_off + i * 4u + 0u);
            off   = rd16(emr + part_list_off + i * 4u + 2u);
            if (count > 64u) continue;
            if (part_list_off + off + count > emr_size) continue;
            for (j = 0u; j < count; ++j) {
                unsigned int child;
                child = (unsigned int)emr[part_list_off + off + j];
                if (child < object_count && child < 64u && child != i) {
                    if (e->parent[child] == -1) {
                        e->parent[child] = (int)i;
                        valid_edges++;
                    }
                }
            }
        }
        if (valid_edges > 0u) e->has_emr_hierarchy = 1;
    }

    if (re1mv_check_id_from_emr(emr, emr_size, &emd_type)) e->emd_type = emd_type;
    e->has_bind = 1;
    return 1;
}

static int apply_re1_anim_pose(emd_loaded *e)
{
    unsigned int emr_idx, edd_idx, emr_off, emr_size, edd_off, edd_size;
    const unsigned char *emr;
    const unsigned char *edd;
    const unsigned char *frame;
    unsigned int part_list_off, anim_off, object_count, frame_size;
    unsigned int first_stream, clip_count;
    unsigned int frame_count, stream_off, frame_index, frame_rec, pose_index, frame_step;
    unsigned int total_frames, i;
    if (e == NULL || e->cli_anim_requested == 0) return 0;
    if (e->section_count < 6) return 0;
    if (e->cli_bank == 0) { emr_idx = 0u; edd_idx = 1u; }
    else { emr_idx = 2u; edd_idx = 3u; }
    emr_off = e->sections[emr_idx].off; emr_size = e->sections[emr_idx].size;
    edd_off = e->sections[edd_idx].off; edd_size = e->sections[edd_idx].size;
    if (emr_off + emr_size > (unsigned int)e->file_size) return 0;
    if (edd_off + edd_size > (unsigned int)e->file_size) return 0;
    emr = e->file_data + emr_off;
    edd = e->file_data + edd_off;
    if (emr_size < 176u || edd_size < 4u) return 0;

    /* RE1MV TEMRHeader:
       +0 PartListOffset, +2 AnimationOffset, +4 ObjectCount, +6 FrameSize.
       Previous experimental code confused +0 with frame size; that made poses
       drift because EM1111 uses FrameSize=0x68, not PartListOffset=0x64. */
    part_list_off = rd16(emr + 0u);
    anim_off      = rd16(emr + 2u);
    object_count  = rd16(emr + 4u);
    frame_size    = rd16(emr + 6u);
    if (object_count == 0u || object_count > 64u) return 0;
    if (part_list_off >= emr_size || part_list_off + object_count * 4u > emr_size) return 0;
    re1mv_check_id_from_emr(emr, emr_size, &e->emd_type);
    if (anim_off >= emr_size || frame_size < 12u + object_count * 6u) return 0;
    if (frame_size > 512u) return 0;
    total_frames = (emr_size > anim_off + 4u) ? (unsigned int)(((emr_size - 4u) - anim_off) / frame_size) : 0u;
    if (total_frames == 0u) return 0;

    first_stream = rd16(edd + 2u);
    if (first_stream < 4u || first_stream > edd_size) return 0;
    clip_count = first_stream / 4u;
    if ((unsigned int)e->cli_clip >= clip_count) return 0;
    frame_count = rd16(edd + (size_t)e->cli_clip * 4u + 0u);
    stream_off = rd16(edd + (size_t)e->cli_clip * 4u + 2u);
    if (frame_count == 0u) return 0;
    if (stream_off < first_stream || stream_off + frame_count * 4u > edd_size) return 0;
    frame_index = ((unsigned int)e->cli_frame) % frame_count;
    frame_rec = stream_off + frame_index * 4u;
    pose_index = rd16(edd + frame_rec + 0u);
    frame_step = rd16(edd + frame_rec + 2u);
    if (pose_index >= total_frames) return 0;
    if (anim_off + pose_index * frame_size + frame_size > emr_size) return 0;
    frame = emr + anim_off + pose_index * frame_size;

    for (i=0;i<64u;i++) {
        e->pose_angles[i][0] = 0;
        e->pose_angles[i][1] = 0;
        e->pose_angles[i][2] = 0;
    }

    /* RE1MV root translation: Pos.X/Y/Z are rendered as glTranslatef(Z,Y,X).
       Keep it separate from Obj_Pos[0], because Call_Oblect(0) ignores
       Obj_Pos[0] and uses Pos[ii] instead. */
    e->anim_root.x = re1mv_calc_pos(rd16(frame + 4u));
    e->anim_root.y = re1mv_calc_pos(rd16(frame + 2u));
    e->anim_root.z = re1mv_calc_pos(rd16(frame + 0u));

    for (i=0u; i<object_count && i<64u; i++) {
        size_t aoff; unsigned int ax, ay, az;
        aoff = 12u + (size_t)i * 6u;
        ax = rd16(frame + aoff + 0u) & 0x0fffu;
        ay = rd16(frame + aoff + 2u) & 0x0fffu;
        az = rd16(frame + aoff + 4u) & 0x0fffu;
        /* RE1MV OpenGL order:
           glRotate AngleX around -Z, AngleY around -Y, AngleZ around -X.
           m4_from_euler_psx composes Rz(az)*Ry(ay)*Rx(ax), so remap to
           X=-AngleZ, Y=-AngleY, Z=-AngleX. */
        e->pose_angles[i][0] = re1mv_neg12(az);
        e->pose_angles[i][1] = re1mv_neg12(ay);
        e->pose_angles[i][2] = re1mv_neg12(ax);
    }

    e->use_anim = 1;
    e->cli_pose_index = pose_index;
    e->cli_clip_frames = frame_count;
    e->cli_frame_step = frame_step;
    return 1;
}

static int load_emd(const char *path, emd_loaded *e)
{
    grc_options opt; int rc; unsigned int tmd_index, tim_index;
    int saved_frame, saved_bank, saved_clip, saved_anim, saved_palette, saved_global;
    int saved_fixed_bounds, saved_dump_bounds, saved_force_legacy;
    v3 saved_mn, saved_mx;
    saved_frame = e->cli_frame; saved_bank = e->cli_bank; saved_clip = e->cli_clip;
    saved_anim = e->cli_anim_requested; saved_palette = e->cli_palette; saved_global = e->force_global_rgba;
    saved_fixed_bounds = e->fixed_bounds; saved_dump_bounds = e->dump_bounds; saved_force_legacy = e->force_legacy_build_model;
    saved_mn = e->fixed_mn; saved_mx = e->fixed_mx;
    memset(e, 0, sizeof(*e));
    e->cli_frame = saved_frame; e->cli_bank = saved_bank; e->cli_clip = saved_clip;
    e->cli_anim_requested = saved_anim; e->cli_palette = saved_palette; e->force_global_rgba = saved_global;
    e->fixed_bounds = saved_fixed_bounds; e->dump_bounds = saved_dump_bounds; e->force_legacy_build_model = saved_force_legacy;
    e->fixed_mn = saved_mn; e->fixed_mx = saved_mx;
    grc_model_init(&e->model); grc_texture_init(&e->tex_fallback);
    if (!load_file(path, &e->file_data, &e->file_size)) return 0;
    if (!read_re1_end_sections(e->file_data, e->file_size, e->sections, &e->section_count)) return 0;
    tmd_index = (e->section_count >= 6) ? 4u : ((e->section_count >= 5) ? 3u : 2u);
    tim_index = (e->section_count >= 6) ? 5u : ((e->section_count >= 5) ? 4u : 3u);
    grc_options_default(&opt); opt.native_game_hint = GRC_GAME_RE1; opt.native_format_hint = GRC_FORMAT_EMD; opt.scale = 1.0f;
    rc = grc_load_emd(path, &e->model, &opt);
    if (rc != GRC_OK) return 0;
    if (tim_index < (unsigned int)e->section_count && e->sections[tim_index].off + e->sections[tim_index].size <= (unsigned int)e->file_size) {
        const unsigned char *tim = e->file_data + e->sections[tim_index].off;
        parse_tim_raw(tim, e->sections[tim_index].size, &e->raw_tim);
        grc_tim_decode_rgba(tim, e->sections[tim_index].size, &e->tex_fallback, 0);
    }
    if (tmd_index < (unsigned int)e->section_count && e->sections[tmd_index].off + e->sections[tmd_index].size <= (unsigned int)e->file_size) {
        const unsigned char *tmd = e->file_data + e->sections[tmd_index].off;
        parse_re1_texrefs_from_tmd(e, tmd, e->sections[tmd_index].size);
    }
    /* v10 bind path: mirror RE1MV Load_Ans() from the selected EMR bank.
       Six-section RE1 EMDs carry EMR_A/EDD_A/EMR_B/EDD_B/TMD/TIM;
       Bank A uses section 0, Bank B uses section 2. */
    if (e->section_count >= 6) {
        unsigned int bind_index;
        bind_index = (e->cli_anim_requested && e->cli_bank == 0) ? 0u : 2u;
        re1mv_load_bind_from_emr(e, bind_index);
    } else if (e->section_count >= 5) {
        re1mv_load_bind_from_emr(e, 1u);
    } else if (e->section_count >= 1) {
        re1mv_load_bind_from_emr(e, 0u);
    }
    if (!e->has_emr_hierarchy) set_default_human_parents(e);
    apply_re1_anim_pose(e);
    return 1;
}


#define RE1MV_OP_PUSH 1u
#define RE1MV_OP_POP  2u
#define RE1MV_OP_CALL 3u

typedef struct re1mv_op_s {
    unsigned char op;
    unsigned char arg;
} re1mv_op;

static const re1mv_op re1mv_ops_3c0006[] = {
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_370000[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_4d0002[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_410000[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_480002[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_40000[] = {
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_4b0000[] = {
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_460000[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_b000b[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 15u },
    { RE1MV_OP_CALL, 16u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 17u },
    { RE1MV_OP_CALL, 18u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 19u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_4d0006[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_a0001[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_CALL, 15u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_190009[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_0001[] = {
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_CALL, 15u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 16u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 17u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_500000[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_CALL, 15u },
    { RE1MV_OP_CALL, 16u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_10001[] = {
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_POP, 0u }
};

static const re1mv_op re1mv_ops_80005[] = {
    { RE1MV_OP_CALL, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 1u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 2u },
    { RE1MV_OP_CALL, 3u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 4u },
    { RE1MV_OP_CALL, 5u },
    { RE1MV_OP_CALL, 6u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 7u },
    { RE1MV_OP_CALL, 8u },
    { RE1MV_OP_CALL, 9u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 10u },
    { RE1MV_OP_CALL, 11u },
    { RE1MV_OP_CALL, 12u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_PUSH, 0u },
    { RE1MV_OP_CALL, 13u },
    { RE1MV_OP_CALL, 14u },
    { RE1MV_OP_CALL, 15u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u },
    { RE1MV_OP_POP, 0u }
};


static void re1mv_stack_push(m4 stack[64], int *sp, const m4 *cur)
{
    if (*sp < 64) {
        stack[*sp] = *cur;
        *sp = *sp + 1;
    }
}

static void re1mv_stack_pop(m4 stack[64], int *sp, m4 *cur)
{
    if (*sp > 0) {
        *sp = *sp - 1;
        *cur = stack[*sp];
    }
}

static void re1mv_call_object(emd_loaded *e, unsigned int piece_index, m4 *cur)
{
    m4 t, r, local, world;
    v3 tr, origin;
    origin.x = 0.0f; origin.y = 0.0f; origin.z = 0.0f;
    if (piece_index >= 64u) return;
    if (piece_index == 0u && e->use_anim) tr = e->anim_root;
    else tr = e->local_pivot[piece_index];
    m4_translate(&t, tr.x, tr.y, tr.z);
    m4_from_euler_psx(&r, e->pose_angles[piece_index][0], e->pose_angles[piece_index][1], e->pose_angles[piece_index][2]);
    m4_mul(&local, &t, &r);
    m4_mul(&world, cur, &local);
    *cur = world;
    if (piece_index < e->model.piece_count) {
        e->world_mtx[piece_index] = world;
        e->world_pivot[piece_index] = m4_transform_point(&world, origin);
    }
}

static int re1mv_get_ops(unsigned int emd_type, const re1mv_op **ops_out, unsigned int *count_out)
{
#define RET_OPS(hexname) do { *ops_out = re1mv_ops_##hexname; *count_out = (unsigned int)(sizeof(re1mv_ops_##hexname) / sizeof(re1mv_ops_##hexname[0])); return 1; } while (0)
    switch (emd_type) {
    case 0x003C0006u: RET_OPS(3c0006);
    case 0x00370000u: RET_OPS(370000);
    case 0x004D0002u: RET_OPS(4d0002);
    case 0x00410000u: RET_OPS(410000);
    case 0x00480002u: RET_OPS(480002);
    case 0x00040000u: RET_OPS(40000);
    case 0x004B0000u: RET_OPS(4b0000);
    case 0x00460000u: RET_OPS(460000);
    case 0x000B000Bu: RET_OPS(b000b);
    case 0x004D0006u: RET_OPS(4d0006);
    case 0x000A0001u: RET_OPS(a0001);
    case 0x00190009u: RET_OPS(190009);
    case 0x00000001u: RET_OPS(0001);
    case 0x00500000u: RET_OPS(500000);
    case 0x00010001u: RET_OPS(10001);
    case 0x00080005u: RET_OPS(80005);
    default: break;
    }
#undef RET_OPS
    *ops_out = NULL;
    *count_out = 0u;
    return 0;
}


static void emr_tree_build_one(emd_loaded *e, unsigned int piece_index, const m4 *parent_world)
{
    m4 t, r, local, world;
    v3 tr, origin;
    origin.x = 0.0f; origin.y = 0.0f; origin.z = 0.0f;
    if (piece_index == 0u && e->use_anim) tr = e->anim_root;
    else tr = e->local_pivot[piece_index];
    m4_translate(&t, tr.x, tr.y, tr.z);
    m4_from_euler_psx(&r, e->pose_angles[piece_index][0], e->pose_angles[piece_index][1], e->pose_angles[piece_index][2]);
    m4_mul(&local, &t, &r);
    if (parent_world != NULL) m4_mul(&world, parent_world, &local);
    else world = local;
    e->world_mtx[piece_index] = world;
    e->world_pivot[piece_index] = m4_transform_point(&world, origin);
}

static int emr_partlist_build_model_matrices(emd_loaded *e)
{
    unsigned char built[64];
    unsigned int total, built_count, pass, i;
    m4 identity;
    if (e == NULL || e->has_bind == 0 || e->has_emr_hierarchy == 0) return 0;
    total = e->model.piece_count;
    if (total > 64u) total = 64u;
    if (total == 0u) return 0;
    for (i = 0u; i < 64u; ++i) built[i] = 0u;
    m4_identity(&identity);
    built_count = 0u;
    for (pass = 0u; pass < 64u && built_count < total; ++pass) {
        for (i = 0u; i < total; ++i) {
            int p;
            if (built[i]) continue;
            p = e->parent[i];
            if (p < 0) {
                emr_tree_build_one(e, i, NULL);
                built[i] = 1u;
                built_count++;
            } else if ((unsigned int)p < total && built[p]) {
                emr_tree_build_one(e, i, &e->world_mtx[p]);
                built[i] = 1u;
                built_count++;
            }
        }
    }
    if (built_count == 0u) return 0;
    for (i = 0u; i < total; ++i) {
        if (!built[i]) {
            emr_tree_build_one(e, i, &identity);
        }
    }
    e->used_emr_hierarchy = 1;
    return 1;
}

static int re1mv_build_model_matrices(emd_loaded *e)
{
    const re1mv_op *ops;
    unsigned int count, i;
    m4 cur, stack[64];
    int sp;
    if (e == NULL || e->has_bind == 0 || e->emd_type == 0u) return 0;
    if (!re1mv_get_ops(e->emd_type, &ops, &count)) return 0;
    m4_identity(&cur);
    sp = 0;
    for (i = 0u; i < count; ++i) {
        if (ops[i].op == RE1MV_OP_PUSH) re1mv_stack_push(stack, &sp, &cur);
        else if (ops[i].op == RE1MV_OP_POP) re1mv_stack_pop(stack, &sp, &cur);
        else if (ops[i].op == RE1MV_OP_CALL) re1mv_call_object(e, (unsigned int)ops[i].arg, &cur);
    }
    e->used_re1mv_build_model = 1;
    return 1;
}

static void build_piece_transforms(emd_loaded *e)
{
    unsigned int i;
    v3 zero;
    zero.x=zero.y=zero.z=0.0f;
    for (i=0;i<64u;i++) { e->world_pivot[i] = zero; m4_identity(&e->world_mtx[i]); }
    if (!e->force_legacy_build_model && emr_partlist_build_model_matrices(e)) return;
    if (e->force_legacy_build_model && re1mv_build_model_matrices(e)) return;
    for (i=0;i<e->model.piece_count && i<64u;i++) {
        int p;
        m4 t, r, local, world;
        v3 origin;
        p = e->parent[i];
        origin.x=origin.y=origin.z=0.0f;
        if (!e->has_bind) { m4_identity(&e->world_mtx[i]); e->world_pivot[i] = zero; continue; }
        if (i == 0u && e->use_anim) m4_translate(&t, e->anim_root.x, e->anim_root.y, e->anim_root.z);
        else m4_translate(&t, e->local_pivot[i].x, e->local_pivot[i].y, e->local_pivot[i].z);
        m4_from_euler_psx(&r, e->pose_angles[i][0], e->pose_angles[i][1], e->pose_angles[i][2]);
        m4_mul(&local, &t, &r);
        if (p >= 0 && (unsigned int)p < i) m4_mul(&world, &e->world_mtx[p], &local);
        else world = local;
        e->world_mtx[i] = world;
        e->world_pivot[i] = m4_transform_point(&world, origin);
    }
}

static v3 assemble_world_vertices(const grc_model *m, const emd_loaded *e, unsigned int piece_index, int vertex_index)
{
    v3 r; grc_vec3 v; v = m->vertices[vertex_index];
    r.x = v.x; r.y = v.y; r.z = v.z;
    if (piece_index < 64u && e->has_bind) return m4_transform_point(&e->world_mtx[piece_index], r);
    if (piece_index < 64u) { r.x += e->world_pivot[piece_index].x; r.y += e->world_pivot[piece_index].y; r.z += e->world_pivot[piece_index].z; }
    return r;
}

static int decode_tim(const emd_loaded *e) { return (e->raw_tim.pix != NULL) || (e->tex_fallback.rgba != NULL); }

static void fb_init(fb *f, int w, int h)
{
    int i; f->w=w; f->h=h; f->rgb=(unsigned char*)malloc((size_t)w*(size_t)h*3u); f->z=(float*)malloc((size_t)w*(size_t)h*sizeof(float));
    for (i=0;i<w*h;i++) { f->rgb[i*3+0]=180; f->rgb[i*3+1]=196; f->rgb[i*3+2]=194; f->z[i]=BIGF; }
}
static void fb_free(fb *f) { if (f->rgb) free(f->rgb); if (f->z) free(f->z); f->rgb=NULL; f->z=NULL; }

static void compute_bounds(const emd_loaded *e, v3 *mn, v3 *mx)
{
    const grc_model *m=&e->model; unsigned int pi, vi; int first=1;
    mn->x = mn->y = mn->z = 0.0f;
    mx->x = mx->y = mx->z = 0.0f;
    for (pi=0;pi<m->piece_count;pi++) {
        const grc_piece *pc=&m->pieces[pi];
        for (vi=0;vi<pc->vertex_count;vi++) {
            v3 wv = assemble_world_vertices(m,e,pi,(int)(pc->first_vertex+vi));
            if (first) { *mn=*mx=wv; first=0; }
            if (wv.x < mn->x) mn->x = wv.x;
            if (wv.y < mn->y) mn->y = wv.y;
            if (wv.z < mn->z) mn->z = wv.z;
            if (wv.x > mx->x) mx->x = wv.x;
            if (wv.y > mx->y) mx->y = wv.y;
            if (wv.z > mx->z) mx->z = wv.z;
        }
    }
}

static p2 project_vertices(v3 v, v3 mn, v3 mx, const char *view, int w, int h)
{
    float ax, ay, depth, sx, sy, cx, cy, spanx, spany, scale; p2 p;
    if (strcmp(view,"side")==0) { ax=v.x; ay=v.y; depth=v.z; spanx=mx.x-mn.x; spany=mx.y-mn.y; cx=(mn.x+mx.x)*0.5f; cy=(mn.y+mx.y)*0.5f; }
    else if (strcmp(view,"iso")==0) { ax=(v.z-v.x)*0.7071f; ay=v.y + (v.x+v.z)*0.18f; depth=(v.x+v.z)*0.7071f; spanx=(mx.z-mn.z)+(mx.x-mn.x); spany=(mx.y-mn.y)+(mx.x-mn.x)*0.25f; cx=0.0f; cy=(mn.y+mx.y)*0.5f; }
    else { ax=v.z; ay=v.y; depth=-v.x; spanx=mx.z-mn.z; spany=mx.y-mn.y; cx=(mn.z+mx.z)*0.5f; cy=(mn.y+mx.y)*0.5f; }
    if (spanx < 1.0f) spanx = 1.0f;
    if (spany < 1.0f) spany = 1.0f;
    sx = (float)w * 0.82f / spanx; sy = (float)h * 0.82f / spany; scale = (sx<sy)?sx:sy;
    p.x = (ax - cx) * scale + (float)w*0.5f;
    /* Bitmap scanlines grow downward; RE1MV/OpenGL world Y grows upward.
       Flip the presentation axis here so diagnostic strips are not vertically
       mirrored while keeping the actual model/pose matrices untouched. */
    p.y = (cy - ay) * scale + (float)h*0.5f;
    p.z = depth; p.u=0; p.v=0; p.valid=1; return p;
}

static float edgef(float ax,float ay,float bx,float by,float cx,float cy) { return (cx-ax)*(by-ay) - (cy-ay)*(bx-ax); }

static void fallback_sample_rgba(const grc_texture *tex, float uu, float vv, unsigned char out[4])
{
    int tx, ty; const unsigned char *s;
    out[0]=out[1]=out[2]=0; out[3]=0;
    if (tex == NULL || tex->rgba == NULL || tex->width == 0u || tex->height == 0u) return;
    tx=(int)(uu*(float)(tex->width-1u)); ty=(int)(vv*(float)(tex->height-1u));
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx >= (int)tex->width) tx = (int)tex->width - 1;
    if (ty >= (int)tex->height) ty = (int)tex->height - 1;
    s=tex->rgba+(((size_t)ty*tex->width+(size_t)tx)*4u);
    out[0]=s[0]; out[1]=s[1]; out[2]=s[2]; out[3]=s[3];
}

static void raster_tri(fb *f, const emd_loaded *e, const re1_texref *tr, p2 a, p2 b, p2 c)
{
    int minx,maxx,miny,maxy,x,y; float area;
    if (!a.valid || !b.valid || !c.valid) return;
    area=edgef(a.x,a.y,b.x,b.y,c.x,c.y); if (area > -0.01f && area < 0.01f) return;
    minx=(int)floor((double)mf_min(a.x,mf_min(b.x,c.x))); maxx=(int)ceil((double)mf_max(a.x,mf_max(b.x,c.x)));
    miny=(int)floor((double)mf_min(a.y,mf_min(b.y,c.y))); maxy=(int)ceil((double)mf_max(a.y,mf_max(b.y,c.y)));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= f->w) maxx = f->w - 1;
    if (maxy >= f->h) maxy = f->h - 1;
    for (y=miny;y<=maxy;y++) for (x=minx;x<=maxx;x++) {
        float px=(float)x+0.5f, py=(float)y+0.5f;
        float w0=edgef(b.x,b.y,c.x,c.y,px,py)/area;
        float w1=edgef(c.x,c.y,a.x,a.y,px,py)/area;
        float w2=edgef(a.x,a.y,b.x,b.y,px,py)/area;
        if (w0>=-0.0001f && w1>=-0.0001f && w2>=-0.0001f) {
            float z=w0*a.z+w1*b.z+w2*c.z; int idx=y*f->w+x;
            if (z < f->z[idx]) {
                float uu=w0*a.u+w1*b.u+w2*c.u; float vv=w0*a.v+w1*b.v+w2*c.v;
                unsigned char rgba[4];
                if (e->raw_tim.pix != NULL && tr != NULL && tr->valid) sample_tim_raw(&e->raw_tim, tr, uu, vv, rgba);
                else fallback_sample_rgba(&e->tex_fallback, uu, vv, rgba);
                if (rgba[3] < 8u) continue;
                f->z[idx]=z; f->rgb[idx*3+0]=rgba[0]; f->rgb[idx*3+1]=rgba[1]; f->rgb[idx*3+2]=rgba[2];
            }
        }
    }
}

static void rasterize_textured_triangles(emd_loaded *e, fb *f, const char *view)
{
    grc_model *m=&e->model; v3 mn,mx; unsigned int pi, fi;
    if (e->fixed_bounds) { mn = e->fixed_mn; mx = e->fixed_mx; }
    else compute_bounds(e,&mn,&mx);
    for (pi=0;pi<m->piece_count;pi++) {
        grc_piece *pc=&m->pieces[pi];
        for (fi=0;fi<pc->face_count;fi++) {
            grc_face *fa=&m->faces[pc->first_face+fi]; p2 p[4]; int k; re1_texref *tr;
            if (fa->vertex_count != 3u && fa->vertex_count != 4u) continue;
            tr = (e->texrefs != NULL && pc->first_face+fi < e->texref_count) ? &e->texrefs[pc->first_face+fi] : NULL;
            for (k=0;k<(int)fa->vertex_count;k++) {
                v3 wv = assemble_world_vertices(m,e,pi,fa->v[k]);
                p[k] = project_vertices(wv,mn,mx,view,f->w,f->h);
                if (tr != NULL && tr->valid && k < (int)tr->sides) { p[k].u=(float)tr->u[k]; p[k].v=(float)tr->v[k]; }
                else if (fa->vt[k]>=0 && (unsigned int)fa->vt[k]<m->uv_count) { p[k].u=m->uvs[fa->vt[k]].u; p[k].v=m->uvs[fa->vt[k]].v; }
            }
            if (fa->vertex_count == 3u) raster_tri(f,e,tr,p[0],p[1],p[2]);
            else {
                /* Preserve the same primitive metadata for both generated triangles. */
                raster_tri(f,e,tr,p[0],p[1],p[2]);
                raster_tri(f,e,tr,p[0],p[2],p[3]);
            }
        }
    }
}

static int save_framebuffer_to_bmp(const char *path, const fb *f)
{
    FILE *fp; unsigned int row, pad, size, off=54u; int x,y; unsigned char hdr[54]; unsigned char z[3]={0,0,0};
    row=(unsigned int)f->w*3u; pad=(4u-(row&3u))&3u; size=off+(row+pad)*(unsigned int)f->h;
    memset(hdr,0,sizeof(hdr)); hdr[0]='B'; hdr[1]='M';
    hdr[2]=(unsigned char)(size); hdr[3]=(unsigned char)(size>>8); hdr[4]=(unsigned char)(size>>16); hdr[5]=(unsigned char)(size>>24);
    hdr[10]=(unsigned char)off; hdr[14]=40; hdr[18]=(unsigned char)f->w; hdr[19]=(unsigned char)(f->w>>8); hdr[20]=(unsigned char)(f->w>>16); hdr[21]=(unsigned char)(f->w>>24);
    hdr[22]=(unsigned char)f->h; hdr[23]=(unsigned char)(f->h>>8); hdr[24]=(unsigned char)(f->h>>16); hdr[25]=(unsigned char)(f->h>>24); hdr[26]=1; hdr[28]=24;
    fp=fopen(path,"wb"); if(!fp) return 0; fwrite(hdr,1,54,fp);
    for(y=f->h-1;y>=0;y--) { for(x=0;x<f->w;x++) { const unsigned char *p=f->rgb+(y*f->w+x)*3; fputc(p[2],fp); fputc(p[1],fp); fputc(p[0],fp); } fwrite(z,1,pad,fp); }
    fclose(fp); return 1;
}

static void free_emd(emd_loaded *e)
{
    if (e->texrefs) free(e->texrefs);
    if (e->file_data) free(e->file_data);
    grc_texture_free(&e->tex_fallback);
    grc_model_free(&e->model);
}

int main(int argc, char **argv)
{
    emd_loaded e; fb f; const char *view="front"; int ai;
    if (argc < 3) { fprintf(stderr,"usage: %s input.emd output.bmp [front|side|iso] [--anim] [--bank A|B] [--clip N] [--frame N] [--pal N] [--dump-bounds] [--bounds minx miny minz maxx maxy maxz] [--legacy-buildmodel]\n", argv[0]); return 2; }
    memset(&e,0,sizeof(e));
    e.cli_bank = 1;
    e.cli_clip = 0;
    if (argc >= 4 && argv[3][0] != '-') view = argv[3];
    for (ai=4; ai<argc; ++ai) {
        if (strcmp(argv[ai],"--frame")==0 && ai+1<argc) { e.cli_frame = atoi(argv[++ai]); e.cli_anim_requested = 1; }
        else if (strcmp(argv[ai],"--clip")==0 && ai+1<argc) { e.cli_clip = atoi(argv[++ai]); e.cli_anim_requested = 1; }
        else if (strcmp(argv[ai],"--bank")==0 && ai+1<argc) { ++ai; e.cli_bank = (argv[ai][0]=='A' || argv[ai][0]=='a' || argv[ai][0]=='0') ? 0 : 1; e.cli_anim_requested = 1; }
        else if (strcmp(argv[ai],"--anim")==0) { e.cli_anim_requested = 1; }
        else if (strcmp(argv[ai],"--pal")==0 && ai+1<argc) { e.cli_palette = atoi(argv[++ai]); }
        else if (strcmp(argv[ai],"--global-rgba")==0) { e.force_global_rgba = 1; }
        else if (strcmp(argv[ai],"--legacy-buildmodel")==0 || strcmp(argv[ai],"--legacy-stack")==0) { e.force_legacy_build_model = 1; }
        else if (strcmp(argv[ai],"--dump-bounds")==0) { e.dump_bounds = 1; }
        else if (strcmp(argv[ai],"--bounds")==0 && ai+6<argc) {
            e.fixed_bounds = 1;
            e.fixed_mn.x = (float)atof(argv[++ai]);
            e.fixed_mn.y = (float)atof(argv[++ai]);
            e.fixed_mn.z = (float)atof(argv[++ai]);
            e.fixed_mx.x = (float)atof(argv[++ai]);
            e.fixed_mx.y = (float)atof(argv[++ai]);
            e.fixed_mx.z = (float)atof(argv[++ai]);
        }
    }
    (void)e.cli_palette; (void)e.force_global_rgba;
    if (!load_emd(argv[1], &e)) { fprintf(stderr,"load_emd failed\n"); free_emd(&e); return 1; }
    build_piece_transforms(&e);
    if (e.dump_bounds) {
        v3 bmn, bmx;
        compute_bounds(&e, &bmn, &bmx);
        fprintf(stderr, "bounds: %.6f %.6f %.6f %.6f %.6f %.6f\n",
                bmn.x, bmn.y, bmn.z, bmx.x, bmx.y, bmx.z);
    }
    if (!decode_tim(&e)) { fprintf(stderr,"decode_tim failed\n"); free_emd(&e); return 1; }
    fb_init(&f, FB_W, FB_H);
    rasterize_textured_triangles(&e, &f, view);
    if (!save_framebuffer_to_bmp(argv[2], &f)) fprintf(stderr,"save_framebuffer_to_bmp failed\n");
    fprintf(stderr,"v11 RE1MV-EDD/EMR tex+anim path: faces=%u texrefs=%u tim=%upx x %upx type=%u clut=%ux%u emd_type=0x%06X hierarchy=%s legacy_build_model=%d\n",
        (unsigned int)e.model.face_count, (unsigned int)e.texref_count,
        (unsigned int)e.raw_tim.width_pixels, (unsigned int)e.raw_tim.height_pixels,
        (unsigned int)e.raw_tim.type, (unsigned int)e.raw_tim.clut_w, (unsigned int)e.raw_tim.clut_h,
        (unsigned int)e.emd_type,
        e.used_emr_hierarchy ? "emr_partlist" : (e.used_re1mv_build_model ? "re1mv_stack" : "fallback"),
        e.used_re1mv_build_model);
    if (e.use_anim) {
        fprintf(stderr,"anim applied: bank=%c clip=%d frame=%d/%u pose=%u step=%u\n",
            e.cli_bank ? 'B' : 'A', e.cli_clip, e.cli_frame,
            (unsigned int)e.cli_clip_frames, (unsigned int)e.cli_pose_index,
            (unsigned int)e.cli_frame_step);
    } else if (e.cli_anim_requested) {
        fprintf(stderr,"anim requested but no valid RE1 EDD/EMR pose was applied\n");
    }
    fb_free(&f); free_emd(&e);
    return 0;
}
