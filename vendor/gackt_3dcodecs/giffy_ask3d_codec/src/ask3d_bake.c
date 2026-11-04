#include <string.h>
#include "ask3d_bake.h"
#include "ask3d_resolve.h"

void ask3d_baked_mesh_clear(Ask3dBakedMesh *mesh)
{
    if (mesh == 0) {
        return;
    }
    memset(mesh, 0, sizeof(Ask3dBakedMesh));
}


static ask3d_fixed ask3d_bake_abs(ask3d_fixed v)
{
    return v < 0 ? -v : v;
}

static void ask3d_bake_sort3_desc(ask3d_fixed *a, ask3d_fixed *b, ask3d_fixed *c)
{
    ask3d_fixed t;
    if (*a < *b) { t = *a; *a = *b; *b = t; }
    if (*b < *c) { t = *b; *b = *c; *c = t; }
    if (*a < *b) { t = *a; *a = *b; *b = t; }
}

static int ask3d_bake_normalize_vec3(Ask3dVec3 *v)
{
    ask3d_fixed a;
    ask3d_fixed b;
    ask3d_fixed c;
    ask3d_fixed len;
    if (v == 0) {
        return 0;
    }
    a = ask3d_bake_abs(v->x);
    b = ask3d_bake_abs(v->y);
    c = ask3d_bake_abs(v->z);
    ask3d_bake_sort3_desc(&a, &b, &c);
    len = a + (b >> 1) + (c >> 2);
    if (len == 0) {
        return 0;
    }
    v->x = ask3d_fixed_div(v->x, len);
    v->y = ask3d_fixed_div(v->y, len);
    v->z = ask3d_fixed_div(v->z, len);
    return 1;
}

static int ask3d_bake_make_tangent(const Ask3dScene *scene, const Ask3dFace *face, Ask3dVec3 *out)
{
    Ask3dVec3 p0;
    Ask3dVec3 p1;
    Ask3dVec3 p2;
    Ask3dVec2 uv0;
    Ask3dVec2 uv1;
    Ask3dVec2 uv2;
    Ask3dVec3 e1;
    Ask3dVec3 e2;
    ask3d_fixed du1;
    ask3d_fixed dv1;
    ask3d_fixed du2;
    ask3d_fixed dv2;
    ask3d_fixed det;
    Ask3dVec3 t;
    if (scene == 0 || face == 0 || out == 0 || face->has_tv == 0U) {
        return 0;
    }
    if (face->v[0] >= scene->vertex_count || face->v[1] >= scene->vertex_count || face->v[2] >= scene->vertex_count) {
        return 0;
    }
    if (face->tv[0] >= scene->tvertex_count || face->tv[1] >= scene->tvertex_count || face->tv[2] >= scene->tvertex_count) {
        return 0;
    }
    p0 = scene->vertices[face->v[0]];
    p1 = scene->vertices[face->v[1]];
    p2 = scene->vertices[face->v[2]];
    uv0 = scene->tvertices[face->tv[0]];
    uv1 = scene->tvertices[face->tv[1]];
    uv2 = scene->tvertices[face->tv[2]];
    e1.x = p1.x - p0.x; e1.y = p1.y - p0.y; e1.z = p1.z - p0.z;
    e2.x = p2.x - p0.x; e2.y = p2.y - p0.y; e2.z = p2.z - p0.z;
    du1 = uv1.u - uv0.u;
    dv1 = uv1.v - uv0.v;
    du2 = uv2.u - uv0.u;
    dv2 = uv2.v - uv0.v;
    det = ask3d_fixed_mul(du1, dv2) - ask3d_fixed_mul(du2, dv1);
    if (det == 0) {
        return 0;
    }
    t.x = ask3d_fixed_div(ask3d_fixed_mul(e1.x, dv2) - ask3d_fixed_mul(e2.x, dv1), det);
    t.y = ask3d_fixed_div(ask3d_fixed_mul(e1.y, dv2) - ask3d_fixed_mul(e2.y, dv1), det);
    t.z = ask3d_fixed_div(ask3d_fixed_mul(e1.z, dv2) - ask3d_fixed_mul(e2.z, dv1), det);
    if (!ask3d_bake_normalize_vec3(&t)) {
        return 0;
    }
    *out = t;
    return 1;
}

static void ask3d_bake_note_material(Ask3dBakedMesh *mesh, int mat)
{
    if (mesh == 0 || mat < 0 || (unsigned int)mat >= ASK3D_MAX_MATERIALS) {
        return;
    }
    if (mesh->material_use[mat] == 0U) {
        mesh->material_count++;
    }
    mesh->material_use[mat]++;
}

static int ask3d_bake_one_corner(const Ask3dScene *scene, const Ask3dObject *obj, unsigned int object_index, const Ask3dFace *face, unsigned int face_index, unsigned int corner, int mat, const Ask3dMatrix34 *world, const Ask3dVec3 *face_tangent, Ask3dBakedMesh *mesh)
{
    Ask3dBakedVertex *bv;
    unsigned int vi;
    unsigned int ti;
    unsigned int ci;

    if (scene == 0 || obj == 0 || face == 0 || mesh == 0 || corner >= 3U) {
        return 0;
    }
    if (mesh->vertex_count >= ASK3D_MAX_BAKED_VERTICES) {
        mesh->overflowed = 1U;
        return 0;
    }
    bv = &mesh->vertices[mesh->vertex_count];
    memset(bv, 0, sizeof(Ask3dBakedVertex));
    vi = face->v[corner];
    if (vi < scene->vertex_count) {
        if (world != 0 && world->valid != 0U) {
            ask3d_matrix_transform_point(world, &scene->vertices[vi], &bv->position);
        } else {
            bv->position = scene->vertices[vi];
        }
    }
    if (face->has_vertex_normal[corner] != 0U) {
        if (world != 0 && world->valid != 0U) {
            ask3d_matrix_transform_vector(world, &face->vertex_normal[corner], &bv->normal);
        } else {
            bv->normal = face->vertex_normal[corner];
        }
        bv->has_normal = 1U;
    } else if (face->has_normal != 0U) {
        if (world != 0 && world->valid != 0U) {
            ask3d_matrix_transform_vector(world, &face->normal, &bv->normal);
        } else {
            bv->normal = face->normal;
        }
        bv->has_normal = 1U;
    }
    if (face_tangent != 0) {
        if (world != 0 && world->valid != 0U) {
            ask3d_matrix_transform_vector(world, face_tangent, &bv->tangent);
            ask3d_bake_normalize_vec3(&bv->tangent);
        } else {
            bv->tangent = *face_tangent;
        }
        bv->has_tangent = 1U;
    }
    if (face->has_tv != 0U) {
        ti = face->tv[corner];
        if (ti < scene->tvertex_count) {
            bv->uv = scene->tvertices[ti];
            bv->has_uv = 1U;
        }
    }
    if (face->has_cv != 0U) {
        ci = face->cv[corner];
        if (ci < scene->cvertex_count) {
            bv->color = scene->cvertices[ci];
            bv->has_color = 1U;
        }
    }
    bv->material_index = mat;
    bv->source_object = object_index;
    bv->source_face = face_index;
    bv->source_corner = corner;
    mesh->vertex_count++;
    return 1;
}

static int ask3d_bake_object_core(const Ask3dScene *scene, unsigned int object_index, Ask3dBakedMesh *mesh, unsigned int use_world)
{
    const Ask3dObject *obj;
    const Ask3dFace *face;
    Ask3dMatrix34 world;
    Ask3dMatrix34 *world_ptr;
    unsigned int f;
    unsigned int base;
    int mat;
    unsigned int ok;
    Ask3dVec3 tangent;
    Ask3dVec3 *tangent_ptr;

    if (scene == 0 || mesh == 0 || object_index >= scene->object_count) {
        return 0;
    }
    obj = &scene->objects[object_index];
    world_ptr = 0;
    if (use_world != 0U) {
        if (ask3d_get_world_matrix(scene, ASK3D_NODE_GEOM, (int)object_index, &world)) {
            world_ptr = &world;
        }
    }
    ok = 1U;
    base = obj->first_face;
    for (f = 0U; f < obj->face_count; f++) {
        if (base + f >= scene->face_count) {
            ok = 0U;
            break;
        }
        face = &scene->faces[base + f];
        mat = ask3d_resolve_face_material_index(scene, obj, face);
        tangent_ptr = 0;
        if (ask3d_bake_make_tangent(scene, face, &tangent)) {
            tangent_ptr = &tangent;
        }
        if (!ask3d_bake_one_corner(scene, obj, object_index, face, base + f, 0U, mat, world_ptr, tangent_ptr, mesh)) ok = 0U;
        if (!ask3d_bake_one_corner(scene, obj, object_index, face, base + f, 1U, mat, world_ptr, tangent_ptr, mesh)) ok = 0U;
        if (!ask3d_bake_one_corner(scene, obj, object_index, face, base + f, 2U, mat, world_ptr, tangent_ptr, mesh)) ok = 0U;
        mesh->triangle_count++;
        ask3d_bake_note_material(mesh, mat);
    }
    return ok != 0U ? 1 : 0;
}

int ask3d_bake_object_local(const Ask3dScene *scene, unsigned int object_index, Ask3dBakedMesh *mesh)
{
    if (mesh == 0) {
        return 0;
    }
    ask3d_baked_mesh_clear(mesh);
    return ask3d_bake_object_core(scene, object_index, mesh, 0U);
}

int ask3d_bake_object_world(const Ask3dScene *scene, unsigned int object_index, Ask3dBakedMesh *mesh)
{
    if (mesh == 0) {
        return 0;
    }
    ask3d_baked_mesh_clear(mesh);
    return ask3d_bake_object_core(scene, object_index, mesh, 1U);
}

int ask3d_bake_scene_local(const Ask3dScene *scene, Ask3dBakedMesh *mesh)
{
    unsigned int i;
    unsigned int ok;
    if (mesh == 0) {
        return 0;
    }
    ask3d_baked_mesh_clear(mesh);
    if (scene == 0) {
        return 0;
    }
    ok = 1U;
    for (i = 0U; i < scene->object_count; i++) {
        if (!ask3d_bake_object_core(scene, i, mesh, 0U)) {
            ok = 0U;
        }
    }
    return ok != 0U ? 1 : 0;
}

int ask3d_bake_scene_world(const Ask3dScene *scene, Ask3dBakedMesh *mesh)
{
    unsigned int i;
    unsigned int ok;
    if (mesh == 0) {
        return 0;
    }
    ask3d_baked_mesh_clear(mesh);
    if (scene == 0) {
        return 0;
    }
    ok = 1U;
    for (i = 0U; i < scene->object_count; i++) {
        if (!ask3d_bake_object_core(scene, i, mesh, 1U)) {
            ok = 0U;
        }
    }
    return ok != 0U ? 1 : 0;
}
