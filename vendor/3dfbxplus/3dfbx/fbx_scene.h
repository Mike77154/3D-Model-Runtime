#ifndef FBX_SCENE_H
#define FBX_SCENE_H

#include "fbx_importer.h" /* FbxNode + integer types */
#include "fbx_fixed.h"
#include "fbx_anim.h"     /* FbxSkeleton / FbxSkin */
#include "fbx_material.h" /* FbxMaterial */

#ifdef __cplusplus
extern "C" {
#endif

/* Tipos de nodo lógico que nos interesan */
typedef enum {
    FBX_MODEL_TYPE_UNKNOWN = 0,
    FBX_MODEL_TYPE_MESH,
    FBX_MODEL_TYPE_CAMERA,
    FBX_MODEL_TYPE_LIGHT,
    FBX_MODEL_TYPE_LINE,
    FBX_MODEL_TYPE_NURBS_CURVE,
    FBX_MODEL_TYPE_NURBS_SURFACE,
    FBX_MODEL_TYPE_BONE
} FbxModelType;

#ifndef FBX_SUBMESH_DEFINED
#define FBX_SUBMESH_DEFINED
typedef struct FbxSubMeshTag {
    fbx_u32 first_index;
    fbx_u32 index_count;
    int     material_index;
} FbxSubMesh;
#endif


typedef enum FbxCurveFormTag {
    FBX_CURVE_FORM_UNKNOWN = 0,
    FBX_CURVE_FORM_OPEN,
    FBX_CURVE_FORM_CLOSED,
    FBX_CURVE_FORM_PERIODIC
} FbxCurveForm;

typedef struct FbxCurveGeometryTag {
    int valid;
    fbx_i64 geometry_id;
    char *name;
    int kind; /* FBX_MODEL_TYPE_LINE or FBX_MODEL_TYPE_NURBS_CURVE */
    int order;
    int dimension;
    int form;
    fbx_u32 point_count;
    fbx_real *points; /* xyz for Line, xyzw for NurbsCurve when present */
    fbx_u32 index_count;
    fbx_i32 *indices; /* Line::PointsIndex, may contain negative polygon terminators */
    fbx_u32 knot_count;
    fbx_real *knots;
} FbxCurveGeometry;

typedef struct FbxSurfaceGeometryTag {
    int valid;
    fbx_i64 geometry_id;
    char *name;
    int order_u;
    int order_v;
    int dimension_u;
    int dimension_v;
    int step_u;
    int step_v;
    int form_u;
    int form_v;
    fbx_u32 point_count;
    fbx_real *points; /* xyzw control points */
    fbx_u32 knot_u_count;
    fbx_real *knots_u;
    fbx_u32 knot_v_count;
    fbx_real *knots_v;
} FbxSurfaceGeometry;

typedef struct FbxCameraInfoTag {
    int      valid;
    fbx_real field_of_view;
    fbx_real near_plane;
    fbx_real far_plane;
    fbx_real aspect_width;
    fbx_real aspect_height;
    int      projection_type;
} FbxCameraInfo;

typedef struct FbxLightInfoTag {
    int      valid;
    int      light_type;
    fbx_real color[3];
    fbx_real intensity;
    fbx_real cone_angle;
    fbx_real decay_start;
} FbxLightInfo;


typedef struct FbxMorphTargetTag {
    fbx_i64 id;
    char   *name;

    /* Raw FBX Shape payload. Indexes point at original control points.
       Positions/normals are kept in FBX fixed units and are not expanded to
       final duplicated render vertices yet. */
    fbx_u32 index_count;
    fbx_u32 *indices;
    fbx_real *positions; /* fbx_real[3 * index_count] when present */
    fbx_real *normals;   /* fbx_real[3 * index_count] when present */
    fbx_real weight;

    /* Expanded to mesh->vertex_count after final vertex duplication.
       baked_positions are absolute target positions aligned to FbxMesh.positions.
       baked_normals are absolute target normals aligned to FbxMesh.normals. */
    fbx_u32 baked_vertex_count;
    fbx_real *baked_positions;
    fbx_real *baked_normals;
} FbxMorphTarget;

typedef struct FbxMesh {
    /* Vertex buffer FINAL (ya resuelto a mapping correcto de UV/normals/tangents).
       Esto puede implicar duplicación de control points (seams / hard edges).

       - vertex_count: número de vértices finales
       - positions: xyzxyz...
       - normals:   opcional (xyzxyz...) (puede ser NULL)

       UVs:
       - uv_set_count: cantidad de UV sets detectados
       - uv_sets[i]: fbx_real[2 * vertex_count]
       - uvs: alias conveniente a uv_sets[0] (si existe)

       Tangentes:
       - tangents:   opcional (xyzxyz...)
       - bitangents: opcional (xyzxyz...)

       Colores (si existen):
       - colors: rgba por vértice (fbx_real[4 * vertex_count])

       - indices: index buffer triangulado (3 * triangle_count)

       - vertex_to_control_point: índice del control point original para cada
         vértice final (útil para skinning u otros datos por control point).

       Skinning:
       - skin: pesos por vértice (referencian huesos en scene->skeleton)
    */
    fbx_i64 geometry_id; /* unique FBX Geometry object ID; model ID for legacy embedded geometry */

    fbx_u32 vertex_count;
    fbx_real  *positions;
    fbx_real  *normals;

    fbx_u32 uv_set_count;
    fbx_real  **uv_sets;
    char   **uv_set_names; /* puede ser NULL */
    fbx_real  *uvs;           /* alias a uv_sets[0] */

    fbx_real  *tangents;
    fbx_real  *bitangents;

    fbx_real  *colors;

    fbx_u32 index_count;
    fbx_u32 *indices;

    fbx_u32 *vertex_to_control_point;

    /* number of original FBX control points referenced by this mesh
       (i.e. count of the Geometry::Vertices array / 3). */
    fbx_u32  control_point_count;

    FbxSkin *skin;

    /* Material binding resolved at scene-build time. material_ids are FBX object IDs
       connected to the owning model; submeshes are contiguous index ranges grouped
       by LayerElementMaterial when present. */
    int      material_slot_count;
    fbx_i64 *material_ids;
    fbx_u32  submesh_count;
    FbxSubMesh *submeshes;

    int morph_target_count;
    FbxMorphTarget *morph_targets;
} FbxMesh;

typedef struct FbxModel {
    /* ID interno de FBX (para conexiones) */
    fbx_i64      id;

    FbxModelType type;
    char        *name;

    /* Transform local básico (FBX lo complica, pero aquí lo simplificamos)
       Rotación en Euler XYZ (grados) */
    fbx_real translation[3];
    fbx_real rotation_euler[3];
    fbx_real scale[3];

    /* FBX Transform Stack (mínimo para compatibilidad real) */
    fbx_real pre_rotation[3];
    fbx_real post_rotation[3];
    int   rotation_order;     /* 0..5 (XYZ, XZY, YZX, YXZ, ZXY, ZYX) */

    fbx_real rotation_offset[3];
    fbx_real rotation_pivot[3];
    fbx_real scaling_offset[3];
    fbx_real scaling_pivot[3];

    /* Geometric transform: aplica a la geometría (mesh), no al node transform */
    fbx_real geometric_translation[3];
    fbx_real geometric_rotation[3];
    fbx_real geometric_scaling[3];
    fbx_real geom_matrix[16];

    /* Matrices 4x4 row-major (convención interna; row-vectors, traslación en fila 3) */
    fbx_real local_matrix[16];
    fbx_real global_matrix[16];

    /* Jerarquía */
    struct FbxModel  *parent;
    struct FbxModel **children;
    int              child_count;

    /* Índice en scene->skeleton si este Model es un hueso; -1 si no aplica */
    int bone_index;

    /* Geometry payloads; only one is normally set depending on type. */
    FbxMesh *mesh;
    FbxCurveGeometry *curve;
    FbxSurfaceGeometry *surface;

    FbxCameraInfo camera;
    FbxLightInfo  light;

    /* Lista enlazada simple (para iterar todos) */
    struct FbxModel *next;
} FbxModel;

typedef struct FbxScene {
    FbxModel    *models;         /* lista enlazada de modelos */
    int          model_count;

    /* Skeleton global (si hay clusters/skin). Puede ser NULL si no hay skinning. */
    FbxSkeleton *skeleton;

    /* Materials parsed from Objects/Connections. */
    int           material_count;
    FbxMaterial **materials;

    /* Global unique texture/video graph nodes. Material slots still preserve
       per-material usage; this list is for ufbx-style unique texture counting,
       embedded payload ownership, and graph diagnostics. */
    int            texture_count;
    FbxTextureSlot *textures;
    int            video_count;
    FbxTextureSlot *videos;

    /* Animations: 1 clip por AnimationStack (si existen) */
    int          anim_clip_count;
    FbxAnimClip **anim_clips;    /* array de punteros (size = anim_clip_count) */

    /* ufbx-style asset counters gathered from the raw Objects table.
       mesh_geometry_count counts object-level Geometry::Mesh entries,
       including orphan, empty, duplicate-ID, and negative-ID objects.
       anim_layer_object_count counts AnimationLayer objects even when they
       are empty/orphan placeholders. */
    int          mesh_geometry_count;
    int          anim_layer_object_count;

    int          line_count;
    int          curve_count;
    int          surface_count;
    int          camera_count;
    int          light_count;
} FbxScene;

/* Construir escena a partir del root node completo del FBX */
FbxScene *fbx_scene_build(FbxNode *root);

/* Liberar todo lo asociado a la escena */
void fbx_scene_destroy(FbxScene *scene);

/* Recalcular transforms globales (por si cambias T/R/S locales a mano) */
void fbx_scene_compute_global_transforms(FbxScene *scene);

/* Evaluate FBX transform stack for a model at a given time.
   - clip: NULL para pose base (sin animación)
   - out16: matriz 4x4 row-major, row-vectors (traslación en fila 3)
*/
void fbx_model_eval_local_matrix(const FbxModel *model,
                                const FbxAnimClip *clip,
                                fbx_real time,
                                fbx_real out16[16]);

void fbx_model_eval_global_matrix(const FbxModel *model,
                                 const FbxAnimClip *clip,
                                 fbx_real time,
                                 fbx_real out16[16]);

/* Bake/compile a clip to fixed-FPS samples into clip->baked_* buffers.
   Returns 1 on success, 0 on error.
*/
int fbx_scene_bake_anim_clip(const FbxScene *scene,
                            FbxAnimClip *clip,
                            fbx_real fps);

/* Expand Shape morph payloads from original control-point mapping to the final
   render vertex buffer. Safe to call more than once. */
int fbx_mesh_bake_morph_targets(FbxMesh *mesh);

/* Apply baked morph targets to caller-provided final vertex buffers. weights is
   size mesh->morph_target_count; FBX_REAL_ONE means 100 percent. */
int fbx_mesh_apply_morph(const FbxMesh *mesh,
                         const fbx_real *weights,
                         fbx_real *out_positions,
                         fbx_real *out_normals);

/* ufbx-style unique mesh count: distinct Geometry IDs, with Model IDs used for
   legacy files that embed geometry directly under Model. This keeps node
   instances separate from mesh asset payloads. */
int fbx_scene_count_unique_meshes(const FbxScene *scene);

#ifdef __cplusplus
}
#endif

#endif /* FBX_SCENE_H */
