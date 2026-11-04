/* xp_templates.c - known template names (arena/static storage)
   MIT License

   Notes:
   - The X file spec treats identifiers case-insensitively.
   - This module provides lightweight helpers for matching commonly used
     Direct3D templates by name.
*/
#include "xp_internal.h"

static int xp_tolower_int(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int xp_streq_icase(const char* a, const char* b) {
    int ca, cb;
    if (!a || !b) return 0;
    while (*a && *b) {
        ca = xp_tolower_int((unsigned char)*a++);
        cb = xp_tolower_int((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return (*a == '\0' && *b == '\0');
}

int xp_is_mesh_type(const char* s) { return xp_streq_icase(s, "Mesh"); }
int xp_is_mesh_normals_type(const char* s) { return xp_streq_icase(s, "MeshNormals"); }
int xp_is_mesh_texcoords_type(const char* s) { return xp_streq_icase(s, "MeshTextureCoords"); }
int xp_is_mesh_colors_type(const char* s) { return xp_streq_icase(s, "MeshVertexColors"); }
int xp_is_mesh_material_list_type(const char* s) { return xp_streq_icase(s, "MeshMaterialList"); }
int xp_is_material_type(const char* s) { return xp_streq_icase(s, "Material"); }
int xp_is_texture_filename_type(const char* s) { return xp_streq_icase(s, "TextureFilename"); }
int xp_is_material_wrap_type(const char* s) { return xp_streq_icase(s, "MaterialWrap"); }
int xp_is_mesh_face_wraps_type(const char* s) { return xp_streq_icase(s, "MeshFaceWraps"); }

int xp_is_frame_type(const char* s) { return xp_streq_icase(s, "Frame"); }
int xp_is_frame_transform_matrix_type(const char* s) { return xp_streq_icase(s, "FrameTransformMatrix"); }

int xp_is_skin_weights_type(const char* s) { return xp_streq_icase(s, "SkinWeights"); }
int xp_is_skin_mesh_header_type(const char* s) { return xp_streq_icase(s, "XSkinMeshHeader"); }

int xp_is_animation_set_type(const char* s) { return xp_streq_icase(s, "AnimationSet"); }
int xp_is_animation_type(const char* s) { return xp_streq_icase(s, "Animation"); }
int xp_is_animation_key_type(const char* s) { return xp_streq_icase(s, "AnimationKey"); }
int xp_is_anim_ticks_per_second_type(const char* s) { return xp_streq_icase(s, "AnimTicksPerSecond"); }
int xp_is_animation_options_type(const char* s) { return xp_streq_icase(s, "AnimationOptions"); }

int xp_is_vertex_duplication_indices_type(const char* s) { return xp_streq_icase(s, "VertexDuplicationIndices"); }
int xp_is_decl_data_type(const char* s) { return xp_streq_icase(s, "DeclData"); }
int xp_is_fvf_data_type(const char* s) { return xp_streq_icase(s, "FVFData"); }
