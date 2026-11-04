#include <string.h>
#include "ask3d_export_obj.h"

static int ask3d_w(Ask3dWriteFn fn, void *user, const char *text)
{
    if (fn == 0 || text == 0) {
        return 0;
    }
    return fn(user, text);
}

static int ask3d_w_name(Ask3dWriteFn fn, void *user, const char *name)
{
    char buf[ASK3D_MAX_NAME];
    unsigned int i;
    char c;

    i = 0U;
    if (name == 0 || name[0] == '\0') {
        return ask3d_w(fn, user, "unnamed");
    }
    while (name[i] != '\0' && i + 1U < ASK3D_MAX_NAME) {
        c = name[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '\\' || c == ':') {
            buf[i] = '_';
        } else {
            buf[i] = c;
        }
        i++;
    }
    buf[i] = '\0';
    return ask3d_w(fn, user, buf);
}

static int ask3d_w_fixed(Ask3dWriteFn fn, void *user, ask3d_fixed v)
{
    char buf[48];
    ask3d_fixed_to_ascii(v, buf, (unsigned int)sizeof(buf), 5U);
    return ask3d_w(fn, user, buf);
}

static int ask3d_w_int(Ask3dWriteFn fn, void *user, long v)
{
    char buf[32];
    ask3d_i32_to_ascii(v, buf, (unsigned int)sizeof(buf));
    return ask3d_w(fn, user, buf);
}

static int ask3d_face_has_any_normal(const Ask3dFace *face)
{
    if (face == 0) {
        return 0;
    }
    if (face->has_normal != 0U) {
        return 1;
    }
    if (face->has_vertex_normal[0] != 0U || face->has_vertex_normal[1] != 0U || face->has_vertex_normal[2] != 0U) {
        return 1;
    }
    return 0;
}

static Ask3dVec3 ask3d_face_normal_for_corner(const Ask3dFace *face, unsigned int corner)
{
    Ask3dVec3 zero;
    zero.x = 0;
    zero.y = 0;
    zero.z = ASK3D_FIXED_ONE;
    if (face == 0 || corner >= 3U) {
        return zero;
    }
    if (face->has_vertex_normal[corner] != 0U) {
        return face->vertex_normal[corner];
    }
    if (face->has_normal != 0U) {
        return face->normal;
    }
    return zero;
}

static int ask3d_resolve_material_index(const Ask3dScene *scene, const Ask3dObject *obj, const Ask3dFace *face)
{
    int parent;
    int sub;
    int direct;

    if (scene == 0 || obj == 0 || face == 0) {
        return -1;
    }
    parent = obj->material_ref;
    if (parent >= 0 && (unsigned int)parent < ASK3D_MAX_MATERIALS && face->material_id >= 0) {
        sub = ask3d_scene_get_submaterial(scene, (unsigned int)parent, (unsigned int)face->material_id);
        if (sub >= 0 && (unsigned int)sub < scene->material_count && scene->materials[sub].used != 0) {
            return sub;
        }
    }
    if (parent >= 0 && (unsigned int)parent < scene->material_count && scene->materials[parent].used != 0) {
        return parent;
    }
    direct = face->material_id;
    if (direct >= 0 && (unsigned int)direct < scene->material_count && scene->materials[direct].used != 0) {
        return direct;
    }
    return -1;
}

static int ask3d_w_material_label(const Ask3dScene *scene, int material_index, Ask3dWriteFn fn, void *user)
{
    if (scene != 0 && material_index >= 0 && (unsigned int)material_index < scene->material_count && scene->materials[material_index].used != 0) {
        return ask3d_w_name(fn, user, scene->materials[material_index].name);
    }
    return ask3d_w(fn, user, "default_material");
}

static void ask3d_w_obj_face_corner(Ask3dWriteFn fn, void *user, const Ask3dFace *face, unsigned int corner, unsigned int normal_index)
{
    ask3d_w_int(fn, user, (long)face->v[corner] + 1L);
    if (face->has_tv != 0U || normal_index != 0U) {
        ask3d_w(fn, user, "/");
        if (face->has_tv != 0U) {
            ask3d_w_int(fn, user, (long)face->tv[corner] + 1L);
        }
        if (normal_index != 0U) {
            ask3d_w(fn, user, "/");
            ask3d_w_int(fn, user, (long)normal_index);
        }
    }
}

int ask3d_export_obj(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user, const char *mtllib_name)
{
    unsigned int oi;
    unsigned int i;
    unsigned int fi;
    unsigned int ci;
    unsigned int idx;
    unsigned int normal_base;
    unsigned int normal_cursor;
    unsigned int normal_emit_count;
    unsigned int face_normal_index;
    const Ask3dObject *obj;
    const Ask3dFace *face;
    Ask3dVec3 n;
    int last_material;
    int material_index;

    if (scene == 0 || write_fn == 0) {
        return 0;
    }

    normal_base = 1U;
    ask3d_w(write_fn, user, "# giffy_ask3d OBJ export\n");
    ask3d_w(write_fn, user, "# vertex colors are retained in the parsed scene; OBJ export keeps geometry, uvs, normals and materials.\n");
    if (mtllib_name != 0 && mtllib_name[0] != '\0') {
        ask3d_w(write_fn, user, "mtllib ");
        ask3d_w(write_fn, user, mtllib_name);
        ask3d_w(write_fn, user, "\n");
    }

    for (oi = 0U; oi < scene->object_count; oi++) {
        obj = &scene->objects[oi];
        if (obj->used == 0) {
            continue;
        }
        ask3d_w(write_fn, user, "\no ");
        ask3d_w_name(write_fn, user, obj->name);
        ask3d_w(write_fn, user, "\n");

        for (i = 0U; i < obj->vertex_count; i++) {
            idx = obj->first_vertex + i;
            ask3d_w(write_fn, user, "v ");
            ask3d_w_fixed(write_fn, user, scene->vertices[idx].x);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, scene->vertices[idx].y);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, scene->vertices[idx].z);
            ask3d_w(write_fn, user, "\n");
        }

        for (i = 0U; i < obj->tvertex_count; i++) {
            idx = obj->first_tvertex + i;
            ask3d_w(write_fn, user, "vt ");
            ask3d_w_fixed(write_fn, user, scene->tvertices[idx].u);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, scene->tvertices[idx].v);
            ask3d_w(write_fn, user, "\n");
        }

        normal_emit_count = 0U;
        for (fi = 0U; fi < obj->face_count; fi++) {
            face = &scene->faces[obj->first_face + fi];
            if (ask3d_face_has_any_normal(face)) {
                for (ci = 0U; ci < 3U; ci++) {
                    n = ask3d_face_normal_for_corner(face, ci);
                    ask3d_w(write_fn, user, "vn ");
                    ask3d_w_fixed(write_fn, user, n.x);
                    ask3d_w(write_fn, user, " ");
                    ask3d_w_fixed(write_fn, user, n.y);
                    ask3d_w(write_fn, user, " ");
                    ask3d_w_fixed(write_fn, user, n.z);
                    ask3d_w(write_fn, user, "\n");
                }
                normal_emit_count += 3U;
            }
        }

        last_material = -999999;
        normal_cursor = normal_base;
        for (fi = 0U; fi < obj->face_count; fi++) {
            face = &scene->faces[obj->first_face + fi];
            material_index = ask3d_resolve_material_index(scene, obj, face);
            if (material_index != last_material) {
                ask3d_w(write_fn, user, "usemtl ");
                ask3d_w_material_label(scene, material_index, write_fn, user);
                ask3d_w(write_fn, user, "\n");
                last_material = material_index;
            }
            face_normal_index = 0U;
            if (ask3d_face_has_any_normal(face)) {
                face_normal_index = normal_cursor;
                normal_cursor += 3U;
            }
            ask3d_w(write_fn, user, "f ");
            ask3d_w_obj_face_corner(write_fn, user, face, 0U, face_normal_index == 0U ? 0U : face_normal_index);
            ask3d_w(write_fn, user, " ");
            ask3d_w_obj_face_corner(write_fn, user, face, 1U, face_normal_index == 0U ? 0U : face_normal_index + 1U);
            ask3d_w(write_fn, user, " ");
            ask3d_w_obj_face_corner(write_fn, user, face, 2U, face_normal_index == 0U ? 0U : face_normal_index + 2U);
            ask3d_w(write_fn, user, "\n");
        }
        normal_base += normal_emit_count;
    }
    return 1;
}

int ask3d_export_mtl(const Ask3dScene *scene, Ask3dWriteFn write_fn, void *user)
{
    unsigned int i;
    const Ask3dMaterial *mat;
    ask3d_fixed d;
    ask3d_fixed ns;

    if (scene == 0 || write_fn == 0) {
        return 0;
    }

    ask3d_w(write_fn, user, "# giffy_ask3d MTL export\n");
    ask3d_w(write_fn, user, "newmtl default_material\nKd 0.80000 0.80000 0.80000\nKa 0.20000 0.20000 0.20000\n\n");

    for (i = 0U; i < scene->material_count; i++) {
        mat = &scene->materials[i];
        if (mat->used == 0) {
            continue;
        }
        ask3d_w(write_fn, user, "newmtl ");
        ask3d_w_name(write_fn, user, mat->name);
        ask3d_w(write_fn, user, "\n");
        if (mat->has_diffuse != 0U) {
            ask3d_w(write_fn, user, "Kd ");
            ask3d_w_fixed(write_fn, user, mat->diffuse.x);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->diffuse.y);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->diffuse.z);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->has_ambient != 0U) {
            ask3d_w(write_fn, user, "Ka ");
            ask3d_w_fixed(write_fn, user, mat->ambient.x);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->ambient.y);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->ambient.z);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->has_specular != 0U) {
            ask3d_w(write_fn, user, "Ks ");
            ask3d_w_fixed(write_fn, user, mat->specular.x);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->specular.y);
            ask3d_w(write_fn, user, " ");
            ask3d_w_fixed(write_fn, user, mat->specular.z);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->has_shine != 0U) {
            ns = ask3d_fixed_mul(mat->shine, ask3d_fixed_from_int(1000L));
            ask3d_w(write_fn, user, "Ns ");
            ask3d_w_fixed(write_fn, user, ns);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->has_transparency != 0U) {
            d = ASK3D_FIXED_ONE - mat->transparency;
            if (d < 0) d = 0;
            if (d > ASK3D_FIXED_ONE) d = ASK3D_FIXED_ONE;
            ask3d_w(write_fn, user, "d ");
            ask3d_w_fixed(write_fn, user, d);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_DIFFUSE].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_Kd ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_DIFFUSE].bitmap);
            ask3d_w(write_fn, user, "\n");
        } else if (mat->has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_Kd ");
            ask3d_w(write_fn, user, mat->bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_SPECULAR].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_Ks ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_SPECULAR].bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_OPACITY].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_d ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_OPACITY].bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_BUMP].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_Bump ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_BUMP].bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_SELFILLUM].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "map_Ke ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_SELFILLUM].bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        if (mat->maps[ASK3D_MAP_REFLECT].has_bitmap != 0U) {
            ask3d_w(write_fn, user, "refl ");
            ask3d_w(write_fn, user, mat->maps[ASK3D_MAP_REFLECT].bitmap);
            ask3d_w(write_fn, user, "\n");
        }
        ask3d_w(write_fn, user, "\n");
    }
    return 1;
}
