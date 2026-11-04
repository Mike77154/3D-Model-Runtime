#ifndef MF3D_EXT_MATERIALS_H
#define MF3D_EXT_MATERIALS_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_BASEGROUPS
#define MF3D_EXT_MAX_BASEGROUPS 64
#endif
#ifndef MF3D_EXT_MAX_BASES
#define MF3D_EXT_MAX_BASES 512
#endif
#ifndef MF3D_EXT_MAX_COLORGROUPS
#define MF3D_EXT_MAX_COLORGROUPS 64
#endif
#ifndef MF3D_EXT_MAX_COLORS
#define MF3D_EXT_MAX_COLORS 2048
#endif
#ifndef MF3D_EXT_MAX_TEXTURES
#define MF3D_EXT_MAX_TEXTURES 128
#endif
#ifndef MF3D_EXT_MAX_TEXGROUPS
#define MF3D_EXT_MAX_TEXGROUPS 128
#endif
#ifndef MF3D_EXT_MAX_TEXCOORDS
#define MF3D_EXT_MAX_TEXCOORDS 4096
#endif
#ifndef MF3D_EXT_MAX_MULTIPROPS
#define MF3D_EXT_MAX_MULTIPROPS 128
#endif
#ifndef MF3D_EXT_MAX_MULTIPROP_LAYERS
#define MF3D_EXT_MAX_MULTIPROP_LAYERS 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_ext_rgba_s { mf3d_u8 r, g, b, a; } mf3d_ext_rgba;

typedef struct mf3d_base_material_s {
    char name[64];
    mf3d_ext_rgba color;
} mf3d_base_material;

typedef struct mf3d_base_group_s {
    mf3d_u32 id;
    mf3d_u32 displaypropertiesid;
    mf3d_u32 first;
    mf3d_u32 count;
} mf3d_base_group;

typedef struct mf3d_color_group_s {
    mf3d_u32 id;
    mf3d_u32 displaypropertiesid;
    mf3d_u32 first;
    mf3d_u32 count;
} mf3d_color_group;

typedef struct mf3d_texture2d_s {
    mf3d_u32 id;
    char path[160];
    char contenttype[40];
    char tilestyleu[24];
    char tilestylev[24];
    char filter[24];
} mf3d_texture2d;

typedef struct mf3d_texcoord_s {
    mf3d_fix u, v;
} mf3d_texcoord;

typedef struct mf3d_texture2d_group_s {
    mf3d_u32 id;
    mf3d_u32 texid;
    mf3d_u32 displaypropertiesid;
    mf3d_u32 first;
    mf3d_u32 count;
} mf3d_texture2d_group;

typedef struct mf3d_multiprop_s {
    mf3d_u32 id;
    mf3d_u32 layer_count;
    mf3d_u32 pids[MF3D_EXT_MAX_MULTIPROP_LAYERS];
    char blendmethods[64];
} mf3d_multiprop;

typedef struct mf3d_materials_s {
    mf3d_base_group base_groups[MF3D_EXT_MAX_BASEGROUPS];
    mf3d_base_material bases[MF3D_EXT_MAX_BASES];
    mf3d_color_group color_groups[MF3D_EXT_MAX_COLORGROUPS];
    mf3d_ext_rgba colors[MF3D_EXT_MAX_COLORS];
    mf3d_texture2d textures[MF3D_EXT_MAX_TEXTURES];
    mf3d_texture2d_group tex_groups[MF3D_EXT_MAX_TEXGROUPS];
    mf3d_texcoord texcoords[MF3D_EXT_MAX_TEXCOORDS];
    mf3d_multiprop multiprops[MF3D_EXT_MAX_MULTIPROPS];
    mf3d_u32 base_group_count, base_count;
    mf3d_u32 color_group_count, color_count;
    mf3d_u32 texture_count, tex_group_count, texcoord_count;
    mf3d_u32 multiprop_count;
} mf3d_materials;

void mf3d_materials_init(mf3d_materials *m);
int mf3d_materials_parse_xml(char *xml, mf3d_materials *out_mats);
const mf3d_base_group *mf3d_materials_find_base_group(const mf3d_materials *m, mf3d_u32 id);
const mf3d_color_group *mf3d_materials_find_color_group(const mf3d_materials *m, mf3d_u32 id);
const mf3d_texture2d_group *mf3d_materials_find_tex_group(const mf3d_materials *m, mf3d_u32 id);
const mf3d_texture2d *mf3d_materials_find_texture(const mf3d_materials *m, mf3d_u32 id);
int mf3d_materials_triangle_color(const mf3d_materials *m, const mf3d_tri *tri, mf3d_ext_rgba *rgba);

#ifdef __cplusplus
}
#endif
#endif
