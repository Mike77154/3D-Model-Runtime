#include "fbx_semantic_writer.h"
#include "fbx_alloc.h"

#include <string.h>

static char *fbxs_dup_or_empty(const char *s)
{
    if (!s) s = "";
    return fbx_strdup(s);
}

static FbxNode *fbxs_node(const char *name, int prop_count)
{
    FbxNode *n;
    n = (FbxNode*)fbx_alloc_zero_n(1u, sizeof(FbxNode));
    if (!n) return NULL;
    n->name = fbxs_dup_or_empty(name);
    if (!n->name) return NULL;
    if (prop_count > 0) {
        n->properties = (FbxProperty*)fbx_alloc_zero_n((size_t)prop_count, sizeof(FbxProperty));
        if (!n->properties) return NULL;
        n->property_count = prop_count;
    }
    return n;
}

static int fbxs_add_child(FbxNode *parent, FbxNode *child)
{
    FbxNode **next;
    if (!parent || !child) return 0;
    next = (FbxNode**)fbx_resize(parent->children, sizeof(FbxNode*) * (size_t)(parent->child_count + 1));
    if (!next) return 0;
    parent->children = next;
    parent->children[parent->child_count] = child;
    parent->child_count += 1;
    return 1;
}

static int fbxs_prop_i64(FbxProperty *p, fbx_i64 v)
{
    if (!p) return 0;
    p->type_code = 'L';
    p->type = FBX_PROP_INT;
    p->count = 1;
    p->byte_length = (fbx_i32)sizeof(fbx_i64);
    p->data = fbx_alloc(sizeof(fbx_i64));
    if (!p->data) return 0;
    memcpy(p->data, &v, sizeof(v));
    return 1;
}

static int fbxs_prop_str(FbxProperty *p, const char *s)
{
    size_t len;
    char *dst;
    if (!p) return 0;
    if (!s) s = "";
    len = strlen(s);
    dst = (char*)fbx_alloc(len + 1u);
    if (!dst) return 0;
    memcpy(dst, s, len + 1u);
    p->type_code = 'S';
    p->type = FBX_PROP_STRING;
    p->count = 1;
    p->byte_length = (fbx_i32)len;
    p->data = dst;
    return 1;
}

static int fbxs_prop_real(FbxProperty *p, fbx_real v)
{
    if (!p) return 0;
    p->type_code = 'D';
    p->type = FBX_PROP_REAL;
    p->count = 1;
    p->byte_length = (fbx_i32)sizeof(fbx_real);
    p->data = fbx_alloc(sizeof(fbx_real));
    if (!p->data) return 0;
    memcpy(p->data, &v, sizeof(v));
    return 1;
}

static int fbxs_prop_arr_real(FbxProperty *p, const fbx_real *arr, fbx_u32 count)
{
    if (!p) return 0;
    p->type_code = 'r';
    p->type = FBX_PROP_ARRAY_REAL;
    p->encoding = 0;
    p->count = (fbx_i32)count;
    p->byte_length = (fbx_i32)(sizeof(fbx_real) * (size_t)count);
    if (count > 0u) {
        p->data = fbx_alloc(sizeof(fbx_real) * (size_t)count);
        if (!p->data) return 0;
        if (arr) memcpy(p->data, arr, sizeof(fbx_real) * (size_t)count);
        else memset(p->data, 0, sizeof(fbx_real) * (size_t)count);
    }
    return 1;
}

static int fbxs_prop_arr_i32(FbxProperty *p, const fbx_i32 *arr, fbx_u32 count)
{
    if (!p) return 0;
    p->type_code = 'i';
    p->type = FBX_PROP_ARRAY_INT;
    p->encoding = 0;
    p->count = (fbx_i32)count;
    p->byte_length = (fbx_i32)(sizeof(fbx_i32) * (size_t)count);
    if (count > 0u) {
        p->data = fbx_alloc(sizeof(fbx_i32) * (size_t)count);
        if (!p->data) return 0;
        if (arr) memcpy(p->data, arr, sizeof(fbx_i32) * (size_t)count);
        else memset(p->data, 0, sizeof(fbx_i32) * (size_t)count);
    }
    return 1;
}

static int fbxs_prop_arr_i64(FbxProperty *p, const fbx_i64 *arr, fbx_u32 count)
{
    if (!p) return 0;
    p->type_code = 'l';
    p->type = FBX_PROP_ARRAY_INT;
    p->encoding = 0;
    p->count = (fbx_i32)count;
    p->byte_length = (fbx_i32)(sizeof(fbx_i64) * (size_t)count);
    if (count > 0u) {
        p->data = fbx_alloc(sizeof(fbx_i64) * (size_t)count);
        if (!p->data) return 0;
        if (arr) memcpy(p->data, arr, sizeof(fbx_i64) * (size_t)count);
        else memset(p->data, 0, sizeof(fbx_i64) * (size_t)count);
    }
    return 1;
}

static FbxNode *fbxs_scalar_i64_node(const char *name, fbx_i64 v)
{
    FbxNode *n;
    n = fbxs_node(name, 1);
    if (!n) return NULL;
    if (!fbxs_prop_i64(&n->properties[0], v)) return NULL;
    return n;
}

static FbxNode *fbxs_array_real_node(const char *name, const fbx_real *arr, fbx_u32 count)
{
    FbxNode *n;
    n = fbxs_node(name, 1);
    if (!n) return NULL;
    if (!fbxs_prop_arr_real(&n->properties[0], arr, count)) return NULL;
    return n;
}

static FbxNode *fbxs_array_i32_node(const char *name, const fbx_i32 *arr, fbx_u32 count)
{
    FbxNode *n;
    n = fbxs_node(name, 1);
    if (!n) return NULL;
    if (!fbxs_prop_arr_i32(&n->properties[0], arr, count)) return NULL;
    return n;
}

static FbxNode *fbxs_array_i64_node(const char *name, const fbx_i64 *arr, fbx_u32 count)
{
    FbxNode *n;
    n = fbxs_node(name, 1);
    if (!n) return NULL;
    if (!fbxs_prop_arr_i64(&n->properties[0], arr, count)) return NULL;
    return n;
}

static FbxNode *fbxs_p_real1(const char *name, const char *type, fbx_real x)
{
    FbxNode *p;
    p = fbxs_node("P", 5);
    if (!p) return NULL;
    if (!fbxs_prop_str(&p->properties[0], name)) return NULL;
    if (!fbxs_prop_str(&p->properties[1], type)) return NULL;
    if (!fbxs_prop_str(&p->properties[2], "")) return NULL;
    if (!fbxs_prop_str(&p->properties[3], "A")) return NULL;
    if (!fbxs_prop_real(&p->properties[4], x)) return NULL;
    return p;
}

static FbxNode *fbxs_p_i64(const char *name, const char *type, fbx_i64 x)
{
    FbxNode *p;
    p = fbxs_node("P", 5);
    if (!p) return NULL;
    if (!fbxs_prop_str(&p->properties[0], name)) return NULL;
    if (!fbxs_prop_str(&p->properties[1], type)) return NULL;
    if (!fbxs_prop_str(&p->properties[2], "")) return NULL;
    if (!fbxs_prop_str(&p->properties[3], "")) return NULL;
    if (!fbxs_prop_i64(&p->properties[4], x)) return NULL;
    return p;
}

static void fbxs_trs_defaults(fbx_real t[3], fbx_real r[3], fbx_real sc[3])
{
    t[0] = t[1] = t[2] = FBX_REAL_ZERO;
    r[0] = r[1] = r[2] = FBX_REAL_ZERO;
    sc[0] = sc[1] = sc[2] = FBX_REAL_ONE;
}

static FbxNode *fbxs_string_node(const char *name, const char *value)
{
    FbxNode *n;
    n = fbxs_node(name, 1);
    if (!n) return NULL;
    if (!fbxs_prop_str(&n->properties[0], value)) return NULL;
    return n;
}

static FbxNode *fbxs_p_real3(const char *name, const char *type, fbx_real x, fbx_real y, fbx_real z)
{
    FbxNode *p;
    p = fbxs_node("P", 7);
    if (!p) return NULL;
    if (!fbxs_prop_str(&p->properties[0], name)) return NULL;
    if (!fbxs_prop_str(&p->properties[1], type)) return NULL;
    if (!fbxs_prop_str(&p->properties[2], "")) return NULL;
    if (!fbxs_prop_str(&p->properties[3], "A")) return NULL;
    if (!fbxs_prop_real(&p->properties[4], x)) return NULL;
    if (!fbxs_prop_real(&p->properties[5], y)) return NULL;
    if (!fbxs_prop_real(&p->properties[6], z)) return NULL;
    return p;
}

static fbx_i64 fbxs_geom_id(int mesh_index)
{
    return (fbx_i64)1000000 + (fbx_i64)mesh_index * (fbx_i64)10 + (fbx_i64)1;
}

static fbx_i64 fbxs_model_id(int mesh_index)
{
    return (fbx_i64)1000000 + (fbx_i64)mesh_index * (fbx_i64)10 + (fbx_i64)2;
}

static fbx_i64 fbxs_mat_id(int mesh_index)
{
    return (fbx_i64)1000000 + (fbx_i64)mesh_index * (fbx_i64)10 + (fbx_i64)3;
}

static fbx_i64 fbxs_tex_id(int mesh_index)
{
    return (fbx_i64)1000000 + (fbx_i64)mesh_index * (fbx_i64)10 + (fbx_i64)4;
}

static fbx_i64 fbxs_camera_id(int index)
{
    return (fbx_i64)2000000 + (fbx_i64)index * (fbx_i64)10 + (fbx_i64)2;
}

static fbx_i64 fbxs_light_id(int index)
{
    return (fbx_i64)2100000 + (fbx_i64)index * (fbx_i64)10 + (fbx_i64)2;
}

static fbx_i64 fbxs_bone_id(int index)
{
    return (fbx_i64)3000000 + (fbx_i64)index * (fbx_i64)10 + (fbx_i64)2;
}

static fbx_i64 fbxs_skin_id(int mesh_index)
{
    return (fbx_i64)4000000 + (fbx_i64)mesh_index * (fbx_i64)100 + (fbx_i64)1;
}

static fbx_i64 fbxs_cluster_id(int cluster_index)
{
    return (fbx_i64)4100000 + (fbx_i64)cluster_index * (fbx_i64)10 + (fbx_i64)1;
}

static fbx_i64 fbxs_anim_stack_id(int stack_index)
{
    return (fbx_i64)5000000 + (fbx_i64)stack_index * (fbx_i64)1000 + (fbx_i64)1;
}

static fbx_i64 fbxs_anim_layer_id(int stack_index)
{
    return (fbx_i64)5000000 + (fbx_i64)stack_index * (fbx_i64)1000 + (fbx_i64)2;
}

static fbx_i64 fbxs_anim_cnode_id(int stack_index, int curve_index)
{
    return (fbx_i64)5000000 + (fbx_i64)stack_index * (fbx_i64)1000 + (fbx_i64)10 + (fbx_i64)curve_index * (fbx_i64)2;
}

static fbx_i64 fbxs_anim_curve_id(int stack_index, int curve_index)
{
    return fbxs_anim_cnode_id(stack_index, curve_index) + (fbx_i64)1;
}

static FbxNode *fbxs_layer_element_named(const char *node_name,
                                         const char *mapping,
                                         const char *reference,
                                         const char *data_name,
                                         const fbx_real *data,
                                         fbx_u32 data_count)
{
    FbxNode *le;
    le = fbxs_node(node_name, 1);
    if (!le) return NULL;
    if (!fbxs_prop_i64(&le->properties[0], 0)) return NULL;
    if (!fbxs_add_child(le, fbxs_string_node("MappingInformationType", mapping))) return NULL;
    if (!fbxs_add_child(le, fbxs_string_node("ReferenceInformationType", reference))) return NULL;
    if (!fbxs_add_child(le, fbxs_array_real_node(data_name, data, data_count))) return NULL;
    return le;
}

static FbxNode *fbxs_geometry_from_mesh(const FbxSemanticMesh *m, int mesh_index, const FbxSemanticWriteOptions *opt)
{
    FbxNode *g;
    fbx_i32 *pvi;
    fbx_u32 i;
    fbx_u32 tri;
    const char *name;

    if (!m || !m->positions || m->vertex_count == 0u || !m->indices || m->index_count == 0u) return NULL;
    if ((m->index_count % 3u) != 0u) return NULL;

    name = m->name ? m->name : "Mesh";
    g = fbxs_node("Geometry", 3);
    if (!g) return NULL;
    if (!fbxs_prop_i64(&g->properties[0], fbxs_geom_id(mesh_index))) return NULL;
    if (!fbxs_prop_str(&g->properties[1], name)) return NULL;
    if (!fbxs_prop_str(&g->properties[2], "Mesh")) return NULL;

    if (!fbxs_add_child(g, fbxs_array_real_node("Vertices", m->positions, m->vertex_count * 3u))) return NULL;

    pvi = (fbx_i32*)fbx_alloc(sizeof(fbx_i32) * (size_t)m->index_count);
    if (!pvi) return NULL;
    for (tri = 0u; tri < m->index_count / 3u; ++tri) {
        for (i = 0u; i < 3u; ++i) {
            fbx_u32 idx;
            idx = m->indices[tri * 3u + i];
            if (idx >= m->vertex_count) idx = 0u;
            if (i == 2u) pvi[tri * 3u + i] = -((fbx_i32)idx + 1);
            else pvi[tri * 3u + i] = (fbx_i32)idx;
        }
    }
    if (!fbxs_add_child(g, fbxs_array_i32_node("PolygonVertexIndex", pvi, m->index_count))) return NULL;

    if (opt->include_normals && m->normals) {
        if (!fbxs_add_child(g, fbxs_layer_element_named("LayerElementNormal", "ByVertice", "Direct", "Normals", m->normals, m->vertex_count * 3u))) return NULL;
    }
    if (opt->include_uvs && m->uvs) {
        FbxNode *uv;
        uv = fbxs_layer_element_named("LayerElementUV", "ByVertice", "Direct", "UV", m->uvs, m->vertex_count * 2u);
        if (!uv) return NULL;
        if (!fbxs_add_child(uv, fbxs_string_node("Name", "UVMap"))) return NULL;
        if (!fbxs_add_child(g, uv)) return NULL;
    }
    if (opt->include_colors && m->colors) {
        if (!fbxs_add_child(g, fbxs_layer_element_named("LayerElementColor", "ByVertice", "Direct", "Colors", m->colors, m->vertex_count * 4u))) return NULL;
    }

    return g;
}

static FbxNode *fbxs_model_from_mesh(const FbxSemanticMesh *m, int mesh_index)
{
    FbxNode *model;
    FbxNode *props;
    const char *name;

    name = m && m->name ? m->name : "Mesh";
    model = fbxs_node("Model", 3);
    if (!model) return NULL;
    if (!fbxs_prop_i64(&model->properties[0], fbxs_model_id(mesh_index))) return NULL;
    if (!fbxs_prop_str(&model->properties[1], name)) return NULL;
    if (!fbxs_prop_str(&model->properties[2], "Mesh")) return NULL;

    props = fbxs_node("Properties70", 0);
    if (!props) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Translation", "Lcl Translation", FBX_REAL_ZERO, FBX_REAL_ZERO, FBX_REAL_ZERO))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Rotation", "Lcl Rotation", FBX_REAL_ZERO, FBX_REAL_ZERO, FBX_REAL_ZERO))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Scaling", "Lcl Scaling", FBX_REAL_ONE, FBX_REAL_ONE, FBX_REAL_ONE))) return NULL;
    if (!fbxs_add_child(model, props)) return NULL;
    return model;
}

static FbxNode *fbxs_material_from_mesh(const FbxSemanticMesh *m, int mesh_index)
{
    FbxNode *mat;
    FbxNode *props;
    const char *name;
    if (!m || !m->material_name) return NULL;
    name = m->material_name;
    mat = fbxs_node("Material", 3);
    if (!mat) return NULL;
    if (!fbxs_prop_i64(&mat->properties[0], fbxs_mat_id(mesh_index))) return NULL;
    if (!fbxs_prop_str(&mat->properties[1], name)) return NULL;
    if (!fbxs_prop_str(&mat->properties[2], "")) return NULL;
    if (!fbxs_add_child(mat, fbxs_string_node("ShadingModel", "phong"))) return NULL;
    props = fbxs_node("Properties70", 0);
    if (!props) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("DiffuseColor", "Color", FBX_REAL_ONE, FBX_REAL_ONE, FBX_REAL_ONE))) return NULL;
    if (!fbxs_add_child(mat, props)) return NULL;
    return mat;
}

static FbxNode *fbxs_texture_from_mesh(const FbxSemanticMesh *m, int mesh_index)
{
    FbxNode *tex;
    const char *path;
    if (!m || !m->diffuse_texture) return NULL;
    path = m->diffuse_texture;
    tex = fbxs_node("Texture", 3);
    if (!tex) return NULL;
    if (!fbxs_prop_i64(&tex->properties[0], fbxs_tex_id(mesh_index))) return NULL;
    if (!fbxs_prop_str(&tex->properties[1], "Texture::Diffuse")) return NULL;
    if (!fbxs_prop_str(&tex->properties[2], "")) return NULL;
    if (!fbxs_add_child(tex, fbxs_string_node("Type", "TextureVideoClip"))) return NULL;
    if (!fbxs_add_child(tex, fbxs_string_node("FileName", path))) return NULL;
    if (!fbxs_add_child(tex, fbxs_string_node("RelativeFilename", path))) return NULL;
    return tex;
}


static FbxNode *fbxs_model_with_transform(fbx_i64 id,
                                          const char *name,
                                          const char *type,
                                          const fbx_real t[3],
                                          const fbx_real r[3],
                                          const fbx_real sc[3])
{
    FbxNode *model;
    FbxNode *props;
    fbx_real dt[3];
    fbx_real dr[3];
    fbx_real ds[3];
    if (!name) name = "Node";
    if (!type) type = "Null";
    fbxs_trs_defaults(dt, dr, ds);
    if (t) memcpy(dt, t, sizeof(dt));
    if (r) memcpy(dr, r, sizeof(dr));
    if (sc) memcpy(ds, sc, sizeof(ds));

    model = fbxs_node("Model", 3);
    if (!model) return NULL;
    if (!fbxs_prop_i64(&model->properties[0], id)) return NULL;
    if (!fbxs_prop_str(&model->properties[1], name)) return NULL;
    if (!fbxs_prop_str(&model->properties[2], type)) return NULL;
    props = fbxs_node("Properties70", 0);
    if (!props) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Translation", "Lcl Translation", dt[0], dt[1], dt[2]))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Rotation", "Lcl Rotation", dr[0], dr[1], dr[2]))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Lcl Scaling", "Lcl Scaling", ds[0], ds[1], ds[2]))) return NULL;
    if (!fbxs_add_child(model, props)) return NULL;
    return model;
}

static FbxNode *fbxs_camera_from_semantic(const FbxSemanticCamera *camera, int index)
{
    FbxNode *model;
    FbxNode *props;
    const char *name;
    name = camera && camera->name ? camera->name : "Camera";
    model = fbxs_model_with_transform(fbxs_camera_id(index), name, "Camera",
                                      camera ? camera->translation : NULL,
                                      camera ? camera->rotation_euler : NULL,
                                      NULL);
    if (!model) return NULL;
    props = fbxs_node("Properties70", 0);
    if (!props) return NULL;
    if (!fbxs_add_child(props, fbxs_p_i64("ProjectionType", "enum", camera ? (fbx_i64)camera->projection_type : 0))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real1("FieldOfView", "Number", camera ? camera->field_of_view : fbx_real_from_int(60)))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real1("NearPlane", "Number", camera ? camera->near_plane : fbx_real_from_ratio(1,10)))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real1("FarPlane", "Number", camera ? camera->far_plane : fbx_real_from_int(1000)))) return NULL;
    if (!fbxs_add_child(model, props)) return NULL;
    return model;
}

static FbxNode *fbxs_light_from_semantic(const FbxSemanticLight *light, int index)
{
    FbxNode *model;
    FbxNode *props;
    const char *name;
    fbx_real color[3];
    name = light && light->name ? light->name : "Light";
    color[0] = color[1] = color[2] = FBX_REAL_ONE;
    if (light) memcpy(color, light->color, sizeof(color));
    model = fbxs_model_with_transform(fbxs_light_id(index), name, "Light",
                                      light ? light->translation : NULL,
                                      light ? light->rotation_euler : NULL,
                                      NULL);
    if (!model) return NULL;
    props = fbxs_node("Properties70", 0);
    if (!props) return NULL;
    if (!fbxs_add_child(props, fbxs_p_i64("LightType", "enum", light ? (fbx_i64)light->light_type : 0))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real3("Color", "Color", color[0], color[1], color[2]))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real1("Intensity", "Number", light ? light->intensity : fbx_real_from_int(100)))) return NULL;
    if (!fbxs_add_child(props, fbxs_p_real1("OuterAngle", "Number", light ? light->cone_angle : fbx_real_from_int(45)))) return NULL;
    if (!fbxs_add_child(model, props)) return NULL;
    return model;
}

static FbxNode *fbxs_bone_from_semantic(const FbxSemanticBone *bone, int index)
{
    const char *name;
    fbx_real sc[3];
    name = bone && bone->name ? bone->name : "Bone";
    sc[0] = sc[1] = sc[2] = FBX_REAL_ONE;
    if (bone) memcpy(sc, bone->scale, sizeof(sc));
    return fbxs_model_with_transform(fbxs_bone_id(index), name, "LimbNode",
                                     bone ? bone->translation : NULL,
                                     bone ? bone->rotation_euler : NULL,
                                     sc);
}

static void fbxs_identity_matrix(fbx_real m[16])
{
    int i;
    for (i = 0; i < 16; ++i) m[i] = FBX_REAL_ZERO;
    m[0] = m[5] = m[10] = m[15] = FBX_REAL_ONE;
}

static FbxNode *fbxs_skin_from_mesh(int mesh_index)
{
    FbxNode *skin;
    skin = fbxs_node("Deformer", 3);
    if (!skin) return NULL;
    if (!fbxs_prop_i64(&skin->properties[0], fbxs_skin_id(mesh_index))) return NULL;
    if (!fbxs_prop_str(&skin->properties[1], "Skin")) return NULL;
    if (!fbxs_prop_str(&skin->properties[2], "Skin")) return NULL;
    if (!fbxs_add_child(skin, fbxs_scalar_i64_node("Version", 101))) return NULL;
    return skin;
}

static FbxNode *fbxs_cluster_from_semantic(const FbxSemanticSkinCluster *cluster, int cluster_index)
{
    FbxNode *cl;
    fbx_i32 *indices;
    fbx_real m[16];
    fbx_u32 i;
    if (!cluster || cluster->bone_index < 0) return NULL;
    cl = fbxs_node("Deformer", 3);
    if (!cl) return NULL;
    if (!fbxs_prop_i64(&cl->properties[0], fbxs_cluster_id(cluster_index))) return NULL;
    if (!fbxs_prop_str(&cl->properties[1], "Cluster")) return NULL;
    if (!fbxs_prop_str(&cl->properties[2], "Cluster")) return NULL;
    if (!fbxs_add_child(cl, fbxs_scalar_i64_node("Version", 100))) return NULL;
    indices = NULL;
    if (cluster->count > 0u) {
        indices = (fbx_i32*)fbx_alloc(sizeof(fbx_i32) * (size_t)cluster->count);
        if (!indices) return NULL;
        for (i = 0u; i < cluster->count; ++i) indices[i] = (fbx_i32)cluster->indices[i];
    }
    if (!fbxs_add_child(cl, fbxs_array_i32_node("Indexes", indices, cluster->count))) return NULL;
    if (!fbxs_add_child(cl, fbxs_array_real_node("Weights", cluster->weights, cluster->count))) return NULL;
    fbxs_identity_matrix(m);
    if (!fbxs_add_child(cl, fbxs_array_real_node("Transform", m, 16u))) return NULL;
    if (!fbxs_add_child(cl, fbxs_array_real_node("TransformLink", m, 16u))) return NULL;
    return cl;
}

static const char *fbxs_anim_channel_prop(int channel)
{
    if (channel >= FBX_ANIM_CHANNEL_T_X && channel <= FBX_ANIM_CHANNEL_T_Z) return "Lcl Translation";
    if (channel >= FBX_ANIM_CHANNEL_R_X && channel <= FBX_ANIM_CHANNEL_R_Z) return "Lcl Rotation";
    if (channel >= FBX_ANIM_CHANNEL_S_X && channel <= FBX_ANIM_CHANNEL_S_Z) return "Lcl Scaling";
    return "DeformPercent";
}

static const char *fbxs_anim_channel_axis(int channel)
{
    int axis;
    axis = channel % 3;
    if (axis == 0) return "d|X";
    if (axis == 1) return "d|Y";
    return "d|Z";
}

static FbxNode *fbxs_anim_stack_from_semantic(const FbxSemanticAnimStack *stack, int stack_index)
{
    FbxNode *n;
    n = fbxs_node("AnimationStack", 2);
    if (!n) return NULL;
    if (!fbxs_prop_i64(&n->properties[0], fbxs_anim_stack_id(stack_index))) return NULL;
    if (!fbxs_prop_str(&n->properties[1], stack && stack->name ? stack->name : "Take 001")) return NULL;
    return n;
}

static FbxNode *fbxs_anim_layer_from_semantic(const FbxSemanticAnimStack *stack, int stack_index)
{
    FbxNode *n;
    (void)stack;
    n = fbxs_node("AnimationLayer", 2);
    if (!n) return NULL;
    if (!fbxs_prop_i64(&n->properties[0], fbxs_anim_layer_id(stack_index))) return NULL;
    if (!fbxs_prop_str(&n->properties[1], "BaseLayer")) return NULL;
    return n;
}

static FbxNode *fbxs_anim_curve_node_from_semantic(const FbxSemanticAnimCurve *curve, int stack_index, int curve_index)
{
    FbxNode *n;
    n = fbxs_node("AnimationCurveNode", 2);
    if (!n) return NULL;
    if (!fbxs_prop_i64(&n->properties[0], fbxs_anim_cnode_id(stack_index, curve_index))) return NULL;
    if (!fbxs_prop_str(&n->properties[1], curve && curve->property_name ? curve->property_name : fbxs_anim_channel_prop(curve ? curve->channel : 0))) return NULL;
    return n;
}

static FbxNode *fbxs_anim_curve_from_semantic(const FbxSemanticAnimCurve *curve, int stack_index, int curve_index)
{
    FbxNode *n;
    fbx_i64 *times;
    fbx_real *values;
    fbx_i32 *flags;
    fbx_real *attrs;
    fbx_i32 ref_count;
    int i;
    if (!curve || curve->key_count <= 0 || !curve->keys) return NULL;
    n = fbxs_node("AnimationCurve", 3);
    if (!n) return NULL;
    if (!fbxs_prop_i64(&n->properties[0], fbxs_anim_curve_id(stack_index, curve_index))) return NULL;
    if (!fbxs_prop_str(&n->properties[1], "")) return NULL;
    if (!fbxs_prop_str(&n->properties[2], "")) return NULL;
    times = (fbx_i64*)fbx_alloc(sizeof(fbx_i64) * (size_t)curve->key_count);
    values = (fbx_real*)fbx_alloc(sizeof(fbx_real) * (size_t)curve->key_count);
    flags = (fbx_i32*)fbx_alloc_zero_n((size_t)curve->key_count, sizeof(fbx_i32));
    attrs = (fbx_real*)fbx_alloc_zero_n((size_t)curve->key_count * 4u, sizeof(fbx_real));
    if (!times || !values || !flags || !attrs) return NULL;
    for (i = 0; i < curve->key_count; ++i) {
        times[i] = curve->keys[i].time_ticks;
        values[i] = curve->keys[i].value;
        if (curve->keys[i].interpolation == (unsigned char)FBX_ANIM_INTERP_CONSTANT) flags[i] = 2;
        else if (curve->keys[i].interpolation == (unsigned char)FBX_ANIM_INTERP_CUBIC) flags[i] = 8;
        else flags[i] = 4;
    }
    ref_count = curve->key_count;
    if (!fbxs_add_child(n, fbxs_array_i64_node("KeyTime", times, (fbx_u32)curve->key_count))) return NULL;
    if (!fbxs_add_child(n, fbxs_array_real_node("KeyValueFloat", values, (fbx_u32)curve->key_count))) return NULL;
    if (!fbxs_add_child(n, fbxs_array_i32_node("KeyAttrFlags", flags, (fbx_u32)curve->key_count))) return NULL;
    if (!fbxs_add_child(n, fbxs_array_real_node("KeyAttrDataFloat", attrs, (fbx_u32)curve->key_count * 4u))) return NULL;
    if (!fbxs_add_child(n, fbxs_array_i32_node("KeyAttrRefCount", &ref_count, 1u))) return NULL;
    return n;
}

static fbx_i64 fbxs_anim_target_id(const FbxSemanticScene *scene, const FbxSemanticAnimCurve *curve)
{
    if (!scene || !curve) return 0;
    if (curve->target_kind == 0) {
        if (curve->target_index >= 0 && curve->target_index < scene->mesh_count) return fbxs_model_id(curve->target_index);
        if (curve->target_index >= 1000 && curve->target_index - 1000 < scene->bone_count) return fbxs_bone_id(curve->target_index - 1000);
    }
    return 0;
}

static FbxNode *fbxs_connection_oo(fbx_i64 from, fbx_i64 to)
{
    FbxNode *c;
    c = fbxs_node("C", 3);
    if (!c) return NULL;
    if (!fbxs_prop_str(&c->properties[0], "OO")) return NULL;
    if (!fbxs_prop_i64(&c->properties[1], from)) return NULL;
    if (!fbxs_prop_i64(&c->properties[2], to)) return NULL;
    return c;
}

static FbxNode *fbxs_connection_op(fbx_i64 from, fbx_i64 to, const char *prop)
{
    FbxNode *c;
    c = fbxs_node("C", 4);
    if (!c) return NULL;
    if (!fbxs_prop_str(&c->properties[0], "OP")) return NULL;
    if (!fbxs_prop_i64(&c->properties[1], from)) return NULL;
    if (!fbxs_prop_i64(&c->properties[2], to)) return NULL;
    if (!fbxs_prop_str(&c->properties[3], prop)) return NULL;
    return c;
}

void fbx_semantic_write_options_default(FbxSemanticWriteOptions *opt)
{
    if (!opt) return;
    opt->version = 7400;
    opt->include_normals = 1;
    opt->include_uvs = 1;
    opt->include_colors = 1;
    opt->include_materials = 1;
    opt->binary_compress_arrays = 0;
}

FbxNode *fbx_semantic_build_tree(const FbxSemanticScene *scene,
                                 const FbxSemanticWriteOptions *options)
{
    FbxSemanticWriteOptions def_opt;
    const FbxSemanticWriteOptions *opt;
    FbxNode *root;
    FbxNode *hdr;
    FbxNode *objects;
    FbxNode *conns;
    int i;
    int j;
    int *skin_emitted;

    if (!scene || scene->mesh_count < 0) return NULL;
    fbx_semantic_write_options_default(&def_opt);
    opt = options ? options : &def_opt;
    skin_emitted = NULL;

    root = fbxs_node("Root", 0);
    hdr = fbxs_node("FBXHeaderExtension", 0);
    objects = fbxs_node("Objects", 0);
    conns = fbxs_node("Connections", 0);
    if (!root || !hdr || !objects || !conns) return NULL;

    if (!fbxs_add_child(hdr, fbxs_scalar_i64_node("FBXHeaderVersion", (fbx_i64)1003))) return NULL;
    if (!fbxs_add_child(hdr, fbxs_scalar_i64_node("FBXVersion", (fbx_i64)opt->version))) return NULL;
    if (!fbxs_add_child(hdr, fbxs_string_node("Creator", scene->creator ? scene->creator : "3dfbx semantic writer"))) return NULL;
    if (!fbxs_add_child(root, hdr)) return NULL;
    if (!fbxs_add_child(root, objects)) return NULL;
    if (!fbxs_add_child(root, conns)) return NULL;

    for (i = 0; i < scene->mesh_count; ++i) {
        const FbxSemanticMesh *m;
        FbxNode *g;
        FbxNode *model;
        m = &scene->meshes[i];
        g = fbxs_geometry_from_mesh(m, i, opt);
        model = fbxs_model_from_mesh(m, i);
        if (!g || !model) return NULL;
        if (!fbxs_add_child(objects, g)) return NULL;
        if (!fbxs_add_child(objects, model)) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_geom_id(i), fbxs_model_id(i)))) return NULL;

        if (opt->include_materials && m->material_name) {
            FbxNode *mat;
            mat = fbxs_material_from_mesh(m, i);
            if (!mat) return NULL;
            if (!fbxs_add_child(objects, mat)) return NULL;
            if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_mat_id(i), fbxs_model_id(i)))) return NULL;
            if (m->diffuse_texture) {
                FbxNode *tex;
                tex = fbxs_texture_from_mesh(m, i);
                if (!tex) return NULL;
                if (!fbxs_add_child(objects, tex)) return NULL;
                if (!fbxs_add_child(conns, fbxs_connection_op(fbxs_tex_id(i), fbxs_mat_id(i), "DiffuseColor"))) return NULL;
            }
        }
    }

    for (i = 0; i < scene->camera_count; ++i) {
        FbxNode *camera;
        camera = fbxs_camera_from_semantic(&scene->cameras[i], i);
        if (!camera) return NULL;
        if (!fbxs_add_child(objects, camera)) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_camera_id(i), (fbx_i64)0))) return NULL;
    }

    for (i = 0; i < scene->light_count; ++i) {
        FbxNode *light;
        light = fbxs_light_from_semantic(&scene->lights[i], i);
        if (!light) return NULL;
        if (!fbxs_add_child(objects, light)) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_light_id(i), (fbx_i64)0))) return NULL;
    }

    for (i = 0; i < scene->bone_count; ++i) {
        FbxNode *bone;
        fbx_i64 parent_id;
        bone = fbxs_bone_from_semantic(&scene->bones[i], i);
        if (!bone) return NULL;
        if (!fbxs_add_child(objects, bone)) return NULL;
        parent_id = (fbx_i64)0;
        if (scene->bones[i].parent_index >= 0 && scene->bones[i].parent_index < scene->bone_count) {
            parent_id = fbxs_bone_id(scene->bones[i].parent_index);
        }
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_bone_id(i), parent_id))) return NULL;
    }

    if (scene->mesh_count > 0 && scene->skin_cluster_count > 0) {
        skin_emitted = (int*)fbx_alloc_zero_n((size_t)scene->mesh_count, sizeof(int));
        if (!skin_emitted) return NULL;
    }
    for (i = 0; i < scene->skin_cluster_count; ++i) {
        const FbxSemanticSkinCluster *cluster;
        FbxNode *cluster_node;
        int mesh_index;
        int bone_index;
        cluster = &scene->skin_clusters[i];
        mesh_index = cluster->mesh_index;
        bone_index = cluster->bone_index;
        if (mesh_index < 0 || mesh_index >= scene->mesh_count) continue;
        if (bone_index < 0 || bone_index >= scene->bone_count) continue;
        if (skin_emitted && !skin_emitted[mesh_index]) {
            FbxNode *skin;
            skin = fbxs_skin_from_mesh(mesh_index);
            if (!skin) return NULL;
            if (!fbxs_add_child(objects, skin)) return NULL;
            if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_skin_id(mesh_index), fbxs_geom_id(mesh_index)))) return NULL;
            skin_emitted[mesh_index] = 1;
        }
        cluster_node = fbxs_cluster_from_semantic(cluster, i);
        if (!cluster_node) return NULL;
        if (!fbxs_add_child(objects, cluster_node)) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_cluster_id(i), fbxs_skin_id(mesh_index)))) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_bone_id(bone_index), fbxs_cluster_id(i)))) return NULL;
    }

    for (i = 0; i < scene->anim_stack_count; ++i) {
        const FbxSemanticAnimStack *stack;
        FbxNode *stack_node;
        FbxNode *layer_node;
        stack = &scene->anim_stacks[i];
        stack_node = fbxs_anim_stack_from_semantic(stack, i);
        layer_node = fbxs_anim_layer_from_semantic(stack, i);
        if (!stack_node || !layer_node) return NULL;
        if (!fbxs_add_child(objects, stack_node)) return NULL;
        if (!fbxs_add_child(objects, layer_node)) return NULL;
        if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_anim_layer_id(i), fbxs_anim_stack_id(i)))) return NULL;
        for (j = 0; j < stack->curve_count; ++j) {
            const FbxSemanticAnimCurve *curve;
            FbxNode *cnode;
            FbxNode *acurve;
            fbx_i64 target_id;
            const char *target_prop;
            const char *axis_prop;
            curve = &stack->curves[j];
            target_id = fbxs_anim_target_id(scene, curve);
            if (target_id == (fbx_i64)0) continue;
            cnode = fbxs_anim_curve_node_from_semantic(curve, i, j);
            acurve = fbxs_anim_curve_from_semantic(curve, i, j);
            if (!cnode || !acurve) return NULL;
            if (!fbxs_add_child(objects, cnode)) return NULL;
            if (!fbxs_add_child(objects, acurve)) return NULL;
            if (!fbxs_add_child(conns, fbxs_connection_oo(fbxs_anim_cnode_id(i, j), fbxs_anim_layer_id(i)))) return NULL;
            target_prop = curve->property_name ? curve->property_name : fbxs_anim_channel_prop(curve->channel);
            if (!fbxs_add_child(conns, fbxs_connection_op(fbxs_anim_cnode_id(i, j), target_id, target_prop))) return NULL;
            if (curve->target_kind == 1) axis_prop = "d|DeformPercent";
            else axis_prop = fbxs_anim_channel_axis(curve->channel);
            if (!fbxs_add_child(conns, fbxs_connection_op(fbxs_anim_curve_id(i, j), fbxs_anim_cnode_id(i, j), axis_prop))) return NULL;
        }
    }

    return root;
}

int fbx_semantic_write_ascii(const FbxSemanticScene *scene,
                             const FbxCodecWriteStream *stream,
                             const FbxSemanticWriteOptions *options)
{
    FbxNode *root;
    FbxCodecWriteOptions wopt;
    FbxSemanticWriteOptions def_opt;
    const FbxSemanticWriteOptions *opt;

    if (!scene || !stream) return 0;
    fbx_semantic_write_options_default(&def_opt);
    opt = options ? options : &def_opt;
    root = fbx_semantic_build_tree(scene, opt);
    if (!root) return 0;
    fbx_codec_write_options_default(&wopt);
    wopt.version = opt->version;
    return fbx_codec_write_ascii(root, stream, &wopt);
}

FBX_Result fbx_semantic_write_binary(const FbxSemanticScene *scene,
                                     FbxWriteBuffer *buffer,
                                     const FbxSemanticWriteOptions *options)
{
    FbxNode *root;
    FbxBinaryWriteOptions bopt;
    FbxSemanticWriteOptions def_opt;
    const FbxSemanticWriteOptions *opt;

    if (!scene || !buffer) return FBX_ERR_INVALID_ARGUMENT;
    fbx_semantic_write_options_default(&def_opt);
    opt = options ? options : &def_opt;
    root = fbx_semantic_build_tree(scene, opt);
    if (!root) return FBX_ERR_OUT_OF_MEMORY;
    fbx_binary_write_options_default(&bopt);
    bopt.version = opt->version;
    bopt.compress_arrays = opt->binary_compress_arrays;
    return fbx_binary_write_tree(buffer, root, &bopt);
}
