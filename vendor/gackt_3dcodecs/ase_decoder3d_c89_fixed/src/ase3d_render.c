#include "ase3d_render.h"

static void ase3d_r_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (!dst || cap == 0U) return;
    i = 0U;
    if (src) {
        while (src[i] && i + 1U < cap) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = 0;
}

static ase3d_fixed ase3d_r_clamp01(ase3d_fixed v)
{
    if (v < 0L) return 0L;
    if (v > ASE3D_FIXED_ONE) return ASE3D_FIXED_ONE;
    return v;
}

static long ase3d_r_fixed_to_byte(ase3d_fixed v)
{
    v = ase3d_r_clamp01(v);
    return (v * 255L + (ASE3D_FIXED_ONE >> 1)) >> ASE3D_FIXED_SHIFT;
}

ase3d_u32 ase3d_pack_rgba_fixed(ase3d_fixed r, ase3d_fixed g, ase3d_fixed b, ase3d_fixed a)
{
    ase3d_u32 ar;
    ase3d_u32 rr;
    ase3d_u32 gg;
    ase3d_u32 bb;
    ar = (ase3d_u32)ase3d_r_fixed_to_byte(a);
    rr = (ase3d_u32)ase3d_r_fixed_to_byte(r);
    gg = (ase3d_u32)ase3d_r_fixed_to_byte(g);
    bb = (ase3d_u32)ase3d_r_fixed_to_byte(b);
    return (ar << 24) | (rr << 16) | (gg << 8) | bb;
}

void ase3d_unpack_rgba_fixed(ase3d_u32 c, ase3d_fixed *r, ase3d_fixed *g, ase3d_fixed *b, ase3d_fixed *a)
{
    if (a) *a = (ase3d_fixed)(((c >> 24) & 255UL) * ASE3D_FIXED_ONE / 255L);
    if (r) *r = (ase3d_fixed)(((c >> 16) & 255UL) * ASE3D_FIXED_ONE / 255L);
    if (g) *g = (ase3d_fixed)(((c >> 8) & 255UL) * ASE3D_FIXED_ONE / 255L);
    if (b) *b = (ase3d_fixed)((c & 255UL) * ASE3D_FIXED_ONE / 255L);
}

static ase3d_fixed ase3d_r_blend_fixed(ase3d_fixed a, ase3d_fixed b, ase3d_fixed t)
{
    t = ase3d_r_clamp01(t);
    return ase3d_fixed_lerp(a, b, t);
}

static ase3d_u32 ase3d_r_modulate(ase3d_u32 a, ase3d_u32 b)
{
    ase3d_fixed ar, ag, ab, aa;
    ase3d_fixed br, bg, bb, ba;
    ase3d_unpack_rgba_fixed(a, &ar, &ag, &ab, &aa);
    ase3d_unpack_rgba_fixed(b, &br, &bg, &bb, &ba);
    return ase3d_pack_rgba_fixed(ase3d_fixed_mul(ar, br), ase3d_fixed_mul(ag, bg), ase3d_fixed_mul(ab, bb), ase3d_fixed_mul(aa, ba));
}

static ase3d_u32 ase3d_r_lerp_color(ase3d_u32 a, ase3d_u32 b, ase3d_fixed t)
{
    ase3d_fixed ar, ag, ab, aa;
    ase3d_fixed br, bg, bb, ba;
    ase3d_unpack_rgba_fixed(a, &ar, &ag, &ab, &aa);
    ase3d_unpack_rgba_fixed(b, &br, &bg, &bb, &ba);
    return ase3d_pack_rgba_fixed(ase3d_r_blend_fixed(ar, br, t), ase3d_r_blend_fixed(ag, bg, t), ase3d_r_blend_fixed(ab, bb, t), ase3d_r_blend_fixed(aa, ba, t));
}

void ase3d_shader_node_zero(ase3d_shader_node *node)
{
    if (!node) return;
    node->type = 0;
    node->parent = -1;
    node->material_index = -1;
    node->map_index = -1;
    node->slot = 0;
    node->color.r = 0L; node->color.g = 0L; node->color.b = 0L;
    node->value = 0L;
    node->amount = ASE3D_FIXED_ONE;
    ase3d_r_copy(node->name, ASE3D_NAME_MAX, "");
    ase3d_r_copy(node->bitmap, ASE3D_PATH_MAX, "");
}

void ase3d_shader_tree_zero(ase3d_shader_tree *tree)
{
    if (!tree) return;
    tree->nodes = (ase3d_shader_node *)0;
    tree->node_capacity = 0U;
    tree->node_count = 0U;
    tree->warnings = 0UL;
}

static int ase3d_shader_add_node(ase3d_shader_tree *tree)
{
    unsigned int i;
    if (!tree || !tree->nodes || tree->node_count >= tree->node_capacity) {
        if (tree) tree->warnings |= ASE3D_SHADER_WARN_NODE_OVERFLOW;
        return -1;
    }
    i = tree->node_count++;
    ase3d_shader_node_zero(&tree->nodes[i]);
    return (int)i;
}

static int ase3d_shader_slot_from_map_kind(int kind)
{
    if (kind == ASE3D_MAP_AMBIENT) return ASE3D_SHADER_SLOT_AMBIENT;
    if (kind == ASE3D_MAP_DIFFUSE) return ASE3D_SHADER_SLOT_DIFFUSE;
    if (kind == ASE3D_MAP_SPECULAR) return ASE3D_SHADER_SLOT_SPECULAR;
    if (kind == ASE3D_MAP_OPACITY) return ASE3D_SHADER_SLOT_OPACITY;
    if (kind == ASE3D_MAP_BUMP) return ASE3D_SHADER_SLOT_BUMP;
    if (kind == ASE3D_MAP_REFLECT) return ASE3D_SHADER_SLOT_REFLECT;
    if (kind == ASE3D_MAP_REFRACT) return ASE3D_SHADER_SLOT_REFRACT;
    if (kind == ASE3D_MAP_SELFILLUM) return ASE3D_SHADER_SLOT_SELFILLUM;
    return ASE3D_SHADER_SLOT_ROOT;
}

static int ase3d_shader_node_type_from_map_kind(int kind)
{
    if (kind == ASE3D_MAP_OPACITY) return ASE3D_SHADER_NODE_OPACITY;
    if (kind == ASE3D_MAP_BUMP) return ASE3D_SHADER_NODE_BUMP;
    if (kind == ASE3D_MAP_SPECULAR) return ASE3D_SHADER_NODE_SPECULAR;
    if (kind == ASE3D_MAP_REFLECT) return ASE3D_SHADER_NODE_REFLECTION;
    if (kind == ASE3D_MAP_REFRACT) return ASE3D_SHADER_NODE_REFRACTION;
    if (kind == ASE3D_MAP_SELFILLUM) return ASE3D_SHADER_NODE_SELFILLUM;
    return ASE3D_SHADER_NODE_MAP;
}

int ase3d_shader_tree_find_material_root(const ase3d_shader_tree *tree, int material_index)
{
    unsigned int i;
    if (!tree || !tree->nodes) return -1;
    for (i = 0U; i < tree->node_count; ++i) {
        if ((tree->nodes[i].type == ASE3D_SHADER_NODE_MATERIAL || tree->nodes[i].type == ASE3D_SHADER_NODE_SUBMATERIAL) &&
            tree->nodes[i].material_index == material_index) return (int)i;
    }
    return -1;
}

int ase3d_shader_tree_build(ase3d_shader_tree *tree, const ase3d_model *model)
{
    unsigned int i;
    int n;
    int p;
    int c;
    const ase3d_material *mat;
    const ase3d_texture_map *map;
    if (!tree || !model) return 0;
    tree->node_count = 0U;
    tree->warnings = 0UL;
    for (i = 0U; i < model->material_count; ++i) {
        mat = &model->materials[i];
        n = ase3d_shader_add_node(tree);
        if (n < 0) return 0;
        tree->nodes[n].type = mat->parent_index >= 0 ? ASE3D_SHADER_NODE_SUBMATERIAL : ASE3D_SHADER_NODE_MATERIAL;
        tree->nodes[n].parent = -1;
        tree->nodes[n].material_index = (int)i;
        tree->nodes[n].slot = ASE3D_SHADER_SLOT_ROOT;
        tree->nodes[n].color = mat->diffuse;
        tree->nodes[n].value = mat->shine;
        tree->nodes[n].amount = ASE3D_FIXED_ONE;
        ase3d_r_copy(tree->nodes[n].name, ASE3D_NAME_MAX, mat->name);
        if (mat->bitmap[0]) ase3d_r_copy(tree->nodes[n].bitmap, ASE3D_PATH_MAX, mat->bitmap);
    }
    for (i = 0U; i < model->material_count; ++i) {
        mat = &model->materials[i];
        if (mat->parent_index >= 0) {
            p = ase3d_shader_tree_find_material_root(tree, mat->parent_index);
            c = ase3d_shader_tree_find_material_root(tree, (int)i);
            if (p >= 0 && c >= 0) tree->nodes[c].parent = p;
        }
    }
    for (i = 0U; i < model->map_count; ++i) {
        map = &model->maps[i];
        p = ase3d_shader_tree_find_material_root(tree, map->material_index);
        n = ase3d_shader_add_node(tree);
        if (n < 0) return 0;
        tree->nodes[n].type = ase3d_shader_node_type_from_map_kind(map->kind);
        tree->nodes[n].parent = p;
        tree->nodes[n].material_index = map->material_index;
        tree->nodes[n].map_index = (int)i;
        tree->nodes[n].slot = ase3d_shader_slot_from_map_kind(map->kind);
        tree->nodes[n].amount = map->amount;
        tree->nodes[n].value = map->amount;
        ase3d_r_copy(tree->nodes[n].name, ASE3D_NAME_MAX, map->name[0] ? map->name : map->texture);
        if (map->bitmap[0]) ase3d_r_copy(tree->nodes[n].bitmap, ASE3D_PATH_MAX, map->bitmap);
    }
    return 1;
}

static int ase3d_find_material_by_declared_index(const ase3d_model *model, int parent, int declared_index)
{
    unsigned int i;
    if (!model || !model->materials) return -1;
    for (i = 0U; i < model->material_count; ++i) {
        if (model->materials[i].parent_index == parent && model->materials[i].index == declared_index) return (int)i;
    }
    return -1;
}

int ase3d_resolve_face_material(const ase3d_model *model, const ase3d_face *face)
{
    const ase3d_object *obj;
    int parent_declared;
    int mi;
    if (!model || !face || !model->objects || !model->materials) return -1;
    if (face->object_id < 0 || (unsigned int)face->object_id >= model->object_count) return -1;
    obj = &model->objects[face->object_id];
    parent_declared = obj->material_ref;
    if (face->material_id >= 0) {
        mi = ase3d_find_material_by_declared_index(model, parent_declared, face->material_id);
        if (mi >= 0) return mi;
    }
    mi = ase3d_find_material_by_declared_index(model, -1, parent_declared);
    if (mi >= 0) return mi;
    if (parent_declared >= 0 && (unsigned int)parent_declared < model->material_count) return parent_declared;
    if (face->material_id >= 0 && (unsigned int)face->material_id < model->material_count) return face->material_id;
    return -1;
}

static ase3d_u32 ase3d_shader_vertex_color(ase3d_rgb vc)
{
    return ase3d_pack_rgba_fixed(vc.r, vc.g, vc.b, ASE3D_FIXED_ONE);
}

ase3d_u32 ase3d_shader_eval_material(const ase3d_shader_eval *eval, int material_index, ase3d_fixed u, ase3d_fixed v, ase3d_rgb vertex_color)
{
    const ase3d_model *model;
    const ase3d_material *mat;
    unsigned int i;
    ase3d_u32 out;
    ase3d_u32 tex;
    ase3d_u32 vc;
    ase3d_fixed alpha;
    ase3d_fixed map_amount;
    const ase3d_texture_map *map;
    model = eval ? eval->model : (const ase3d_model *)0;
    if (!model || !model->materials || material_index < 0 || (unsigned int)material_index >= model->material_count) {
        out = ase3d_pack_rgba_fixed(ASE3D_FIXED_ONE, ASE3D_FIXED_ONE, ASE3D_FIXED_ONE, ASE3D_FIXED_ONE);
    } else {
        mat = &model->materials[material_index];
        alpha = ASE3D_FIXED_ONE - ase3d_r_clamp01(mat->transparency);
        out = ase3d_pack_rgba_fixed(mat->diffuse.r, mat->diffuse.g, mat->diffuse.b, alpha);
        for (i = 0U; i < model->map_count; ++i) {
            map = &model->maps[i];
            if (map->material_index != material_index) continue;
            map_amount = map->amount;
            if (map_amount == 0L) map_amount = ASE3D_FIXED_ONE;
            if ((eval->flags & ASE3D_RENDER_FLAG_TEXTURES) && eval->sample_texture && map->has_bitmap && map->bitmap[0]) {
                tex = eval->sample_texture(map->bitmap, ase3d_fixed_mul(u + map->u_offset, map->u_tiling), ase3d_fixed_mul(v + map->v_offset, map->v_tiling), eval->texture_user);
                if (map->kind == ASE3D_MAP_DIFFUSE) out = ase3d_r_lerp_color(out, tex, map_amount);
                else if (map->kind == ASE3D_MAP_OPACITY) {
                    ase3d_fixed tr, tg, tb, ta;
                    ase3d_fixed orr, og, ob, oa;
                    ase3d_unpack_rgba_fixed(tex, &tr, &tg, &tb, &ta);
                    ase3d_unpack_rgba_fixed(out, &orr, &og, &ob, &oa);
                    oa = ase3d_fixed_mul(oa, ase3d_fixed_mul(tr, map_amount));
                    out = ase3d_pack_rgba_fixed(orr, og, ob, oa);
                } else if (map->kind == ASE3D_MAP_SELFILLUM) out = ase3d_r_lerp_color(out, tex, map_amount);
            }
        }
    }
    vc = ase3d_shader_vertex_color(vertex_color);
    out = ase3d_r_modulate(out, vc);
    return out;
}

void ase3d_render_clear(ase3d_framebuffer *fb)
{
    long total;
    long i;
    if (!fb || !fb->color || fb->width <= 0 || fb->height <= 0) return;
    total = (long)fb->width * (long)fb->height;
    for (i = 0L; i < total; ++i) {
        fb->color[i] = fb->clear_color;
        if (fb->depth) fb->depth[i] = fb->clear_depth;
    }
}

static long ase3d_r_edge(long ax, long ay, long bx, long by, long px, long py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static int ase3d_r_min3(int a, int b, int c)
{
    int m;
    m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

static int ase3d_r_max3(int a, int b, int c)
{
    int m;
    m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

static ase3d_fixed ase3d_r_interp_fixed(ase3d_fixed a, ase3d_fixed b, ase3d_fixed c, long wa, long wb, long wc, long area)
{
    ase3d_fixed fa;
    ase3d_fixed fb;
    ase3d_fixed fc;
    if (area == 0L) return a;
    fa = ase3d_fixed_div((ase3d_fixed)wa, (ase3d_fixed)area);
    fb = ase3d_fixed_div((ase3d_fixed)wb, (ase3d_fixed)area);
    fc = ase3d_fixed_div((ase3d_fixed)wc, (ase3d_fixed)area);
    return ase3d_fixed_mul(a, fa) + ase3d_fixed_mul(b, fb) + ase3d_fixed_mul(c, fc);
}

static ase3d_rgb ase3d_r_white(void)
{
    ase3d_rgb c;
    c.r = ASE3D_FIXED_ONE;
    c.g = ASE3D_FIXED_ONE;
    c.b = ASE3D_FIXED_ONE;
    return c;
}

void ase3d_render_triangle(ase3d_framebuffer *fb, const ase3d_shader_eval *eval, int material_index, ase3d_screen_vertex a, ase3d_screen_vertex b, ase3d_screen_vertex c)
{
    int ax, ay, bx, by, cx, cy;
    int minx, maxx, miny, maxy;
    int x, y;
    long area;
    long area_abs;
    int area_neg;
    long w0, w1, w2;
    long idx;
    ase3d_fixed z;
    ase3d_fixed u;
    ase3d_fixed v;
    ase3d_u32 col;
    ase3d_rgb white;
    if (!fb || !fb->color || fb->width <= 0 || fb->height <= 0) return;
    ax = (int)ase3d_fixed_to_int(a.x); ay = (int)ase3d_fixed_to_int(a.y);
    bx = (int)ase3d_fixed_to_int(b.x); by = (int)ase3d_fixed_to_int(b.y);
    cx = (int)ase3d_fixed_to_int(c.x); cy = (int)ase3d_fixed_to_int(c.y);
    minx = ase3d_r_min3(ax, bx, cx); maxx = ase3d_r_max3(ax, bx, cx);
    miny = ase3d_r_min3(ay, by, cy); maxy = ase3d_r_max3(ay, by, cy);
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= fb->width) maxx = fb->width - 1;
    if (maxy >= fb->height) maxy = fb->height - 1;
    area = ase3d_r_edge(ax, ay, bx, by, cx, cy);
    if (area == 0L) return;
    area_neg = area < 0L ? 1 : 0;
    area_abs = area_neg ? -area : area;
    white = ase3d_r_white();
    for (y = miny; y <= maxy; ++y) {
        for (x = minx; x <= maxx; ++x) {
            w0 = ase3d_r_edge(bx, by, cx, cy, x, y);
            w1 = ase3d_r_edge(cx, cy, ax, ay, x, y);
            w2 = ase3d_r_edge(ax, ay, bx, by, x, y);
            if (area_neg) { w0 = -w0; w1 = -w1; w2 = -w2; }
            if (w0 < 0L || w1 < 0L || w2 < 0L) continue;
            z = ase3d_r_interp_fixed(a.z, b.z, c.z, w0, w1, w2, area_abs);
            idx = (long)y * (long)fb->width + (long)x;
            if (fb->depth && z >= fb->depth[idx]) continue;
            u = ase3d_r_interp_fixed(a.u, b.u, c.u, w0, w1, w2, area_abs);
            v = ase3d_r_interp_fixed(a.v, b.v, c.v, w0, w1, w2, area_abs);
            col = ase3d_shader_eval_material(eval, material_index, u, v, white);
            fb->color[idx] = col;
            if (fb->depth) fb->depth[idx] = z;
        }
    }
}

static int ase3d_r_face_tface(const ase3d_model *model, const ase3d_face *face)
{
    unsigned int i;
    if (!model || !model->tfaces || !face) return -1;
    for (i = 0U; i < model->tface_count; ++i) {
        if (model->tfaces[i].object_id == face->object_id && model->tfaces[i].mesh_span_id == face->mesh_span_id && model->tfaces[i].index == face->index) return (int)i;
    }
    return -1;
}

static ase3d_screen_vertex ase3d_r_make_sv(const ase3d_model *model, ase3d_vec3 p, ase3d_vec3 tv, const ase3d_ortho_camera *cam)
{
    ase3d_screen_vertex v;
    v.x = cam->center_x + ase3d_fixed_mul(p.x, cam->scale_x);
    v.y = cam->center_y - ase3d_fixed_mul(p.y, cam->scale_y);
    v.z = p.z;
    v.u = tv.x;
    v.v = tv.y;
    v.color = 0xffffffffUL;
    (void)model;
    return v;
}

int ase3d_render_model_ortho(ase3d_framebuffer *fb, const ase3d_shader_eval *eval, const ase3d_model *model, const ase3d_ortho_camera *cam)
{
    unsigned int i;
    int mat;
    int tfid;
    ase3d_face *face;
    const ase3d_tface *tf;
    ase3d_vec3 va;
    ase3d_vec3 vb;
    ase3d_vec3 vc;
    ase3d_vec3 ta;
    ase3d_vec3 tb;
    ase3d_vec3 tc;
    ase3d_mat43 wm;
    ase3d_screen_vertex sa;
    ase3d_screen_vertex sb;
    ase3d_screen_vertex sc;
    if (!fb || !model || !model->vertices || !model->faces || !cam) return 0;
    for (i = 0U; i < model->face_count; ++i) {
        face = (ase3d_face *)&model->faces[i];
        if (face->a < 0 || face->b < 0 || face->c < 0) continue;
        if ((unsigned int)face->a >= model->vertex_count || (unsigned int)face->b >= model->vertex_count || (unsigned int)face->c >= model->vertex_count) continue;
        va = model->vertices[face->a];
        vb = model->vertices[face->b];
        vc = model->vertices[face->c];
        if (cam->use_world_transform && ase3d_sample_world_matrix(model, face->object_id, cam->tick, &wm)) {
            va = ase3d_transform_point(wm, va);
            vb = ase3d_transform_point(wm, vb);
            vc = ase3d_transform_point(wm, vc);
        }
        ta.x = 0L; ta.y = 0L; ta.z = 0L;
        tb = ta;
        tc = ta;
        tfid = ase3d_r_face_tface(model, face);
        if (tfid >= 0) {
            tf = &model->tfaces[tfid];
            if (tf->a >= 0 && (unsigned int)tf->a < model->tvert_count) ta = model->tverts[tf->a];
            if (tf->b >= 0 && (unsigned int)tf->b < model->tvert_count) tb = model->tverts[tf->b];
            if (tf->c >= 0 && (unsigned int)tf->c < model->tvert_count) tc = model->tverts[tf->c];
        }
        sa = ase3d_r_make_sv(model, va, ta, cam);
        sb = ase3d_r_make_sv(model, vb, tb, cam);
        sc = ase3d_r_make_sv(model, vc, tc, cam);
        mat = ase3d_resolve_face_material(model, face);
        ase3d_render_triangle(fb, eval, mat, sa, sb, sc);
    }
    return 1;
}
