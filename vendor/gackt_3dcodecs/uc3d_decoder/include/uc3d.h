#ifndef UC3D_H
#define UC3D_H

#ifdef __cplusplus
extern "C" {
#endif

/*
   UC3D - fixed point, no-heap UnrealScript .uc / UE1 vertex mesh helper.

   Design contract for embedded/retro-friendly users:
   - C89 core.
   - No malloc, free, realloc, heap containers.
   - No float or double in the public API or implementation.
   - Caller owns every input buffer and output array.
*/

#define UC3D_VERSION_MAJOR 0
#define UC3D_VERSION_MINOR 1
#define UC3D_VERSION_PATCH 0

#ifndef UC3D_MAX_CLASS_NAME
#define UC3D_MAX_CLASS_NAME 64
#endif

#ifndef UC3D_MAX_NAME
#define UC3D_MAX_NAME 64
#endif

#ifndef UC3D_MAX_PATH
#define UC3D_MAX_PATH 160
#endif

#ifndef UC3D_MAX_MESHES
#define UC3D_MAX_MESHES 32
#endif

#ifndef UC3D_MAX_TEXTURES
#define UC3D_MAX_TEXTURES 64
#endif

#ifndef UC3D_MAX_SEQUENCES
#define UC3D_MAX_SEQUENCES 128
#endif

#ifndef UC3D_MAX_MATERIALS
#define UC3D_MAX_MATERIALS 128
#endif

#ifndef UC3D_MAX_DEFAULT_PROPS
#define UC3D_MAX_DEFAULT_PROPS 192
#endif

#ifndef UC3D_MAX_EXECS
#define UC3D_MAX_EXECS 192
#endif

#ifndef UC3D_MAX_KEY
#define UC3D_MAX_KEY 48
#endif

#ifndef UC3D_MAX_VALUE
#define UC3D_MAX_VALUE 128
#endif

#define UC3D_FIXED_SHIFT 16
#define UC3D_FIXED_ONE   ((uc3d_fixed)65536L)
#define UC3D_FIXED_HALF  ((uc3d_fixed)32768L)

#define UC3D_OK 0
#define UC3D_ERR_NULL -1
#define UC3D_ERR_FULL -2
#define UC3D_ERR_BAD_FORMAT -3
#define UC3D_ERR_TRUNCATED -4
#define UC3D_ERR_RANGE -5
#define UC3D_ERR_NOT_FOUND -6

typedef long uc3d_fixed;

typedef struct uc3d_key_value_s {
    char key[UC3D_MAX_KEY];
    char value[UC3D_MAX_VALUE];
} uc3d_key_value;

typedef struct uc3d_exec_s {
    char category[UC3D_MAX_NAME];
    char command[UC3D_MAX_NAME];
    char raw[UC3D_MAX_VALUE];
} uc3d_exec;

typedef struct uc3d_sequence_s {
    char mesh[UC3D_MAX_NAME];
    char name[UC3D_MAX_NAME];
    long start_frame;
    long num_frames;
    uc3d_fixed rate;
} uc3d_sequence;

typedef struct uc3d_texture_s {
    char name[UC3D_MAX_NAME];
    char file[UC3D_MAX_PATH];
    char group[UC3D_MAX_NAME];
    long flags;
    long lodset;
    int mips_on;
    int has_flags;
    int has_lodset;
} uc3d_texture;

typedef struct uc3d_material_binding_s {
    char meshmap[UC3D_MAX_NAME];
    long slot;
    char texture[UC3D_MAX_NAME];
} uc3d_material_binding;

typedef struct uc3d_mesh_ref_s {
    char mesh[UC3D_MAX_NAME];
    char meshmap[UC3D_MAX_NAME];
    char anivfile[UC3D_MAX_PATH];
    char datafile[UC3D_MAX_PATH];
    uc3d_fixed import_x;
    uc3d_fixed import_y;
    uc3d_fixed import_z;
    uc3d_fixed origin_x;
    uc3d_fixed origin_y;
    uc3d_fixed origin_z;
    uc3d_fixed scale_x;
    uc3d_fixed scale_y;
    uc3d_fixed scale_z;
    long pitch;
    long yaw;
    long roll;
    int has_import;
    int has_origin;
    int has_scale;
} uc3d_mesh_ref;

typedef struct uc3d_default_prop_s {
    char key[UC3D_MAX_KEY];
    char value[UC3D_MAX_VALUE];
} uc3d_default_prop;

typedef struct uc3d_scene_s {
    char class_name[UC3D_MAX_CLASS_NAME];
    char parent_name[UC3D_MAX_CLASS_NAME];
    int has_class;

    uc3d_exec execs[UC3D_MAX_EXECS];
    unsigned long exec_count;

    uc3d_mesh_ref meshes[UC3D_MAX_MESHES];
    unsigned long mesh_count;

    uc3d_texture textures[UC3D_MAX_TEXTURES];
    unsigned long texture_count;

    uc3d_sequence sequences[UC3D_MAX_SEQUENCES];
    unsigned long sequence_count;

    uc3d_material_binding materials[UC3D_MAX_MATERIALS];
    unsigned long material_count;

    uc3d_default_prop default_props[UC3D_MAX_DEFAULT_PROPS];
    unsigned long default_prop_count;

    unsigned long warning_count;
    unsigned long overflow_count;
} uc3d_scene;

typedef struct uc3d_vertex_s {
    uc3d_fixed x;
    uc3d_fixed y;
    uc3d_fixed z;
} uc3d_vertex;

typedef struct uc3d_face_s {
    unsigned short v[3];
    unsigned char uv[3][2];
    unsigned char poly_flags;
    unsigned char mesh_color;
    unsigned char tex_num;
    unsigned char aux_flags;
} uc3d_face;

typedef struct uc3d_ue1_mesh_out_s {
    uc3d_vertex *vertices;
    unsigned long vertex_capacity;
    unsigned long vertex_count;

    uc3d_face *faces;
    unsigned long face_capacity;
    unsigned long face_count;

    unsigned long frame_count;
    unsigned long frame_size;
    unsigned long warning_count;
} uc3d_ue1_mesh_out;

#define UC3D_UE1_FORMAT_UNREAL 0
#define UC3D_UE1_FORMAT_ION    1

void uc3d_scene_init(uc3d_scene *scene);
int uc3d_parse_uc_buffer(const char *text, unsigned long text_size, uc3d_scene *scene);
int uc3d_find_mesh(const uc3d_scene *scene, const char *name);
int uc3d_find_texture(const uc3d_scene *scene, const char *name);
uc3d_fixed uc3d_fixed_from_parts(long whole, long frac, long frac_scale, int sign);
uc3d_fixed uc3d_fixed_parse(const char *text);
void uc3d_fixed_to_string(uc3d_fixed value, char *out, unsigned long out_size, int decimals);

int uc3d_ue1_decode_frame(const unsigned char *data_file,
                          unsigned long data_size,
                          const unsigned char *anim_file,
                          unsigned long anim_size,
                          unsigned long frame_index,
                          int format,
                          uc3d_ue1_mesh_out *out_mesh);


#ifndef UC3D_MAX_PKG_NAMES
#define UC3D_MAX_PKG_NAMES 512
#endif

#ifndef UC3D_MAX_PKG_IMPORTS
#define UC3D_MAX_PKG_IMPORTS 512
#endif

#ifndef UC3D_MAX_PKG_EXPORTS
#define UC3D_MAX_PKG_EXPORTS 512
#endif

#ifndef UC3D_MAX_REF_PATH
#define UC3D_MAX_REF_PATH 192
#endif

#define UC3D_UPKG_MAGIC 0x9E2A83C1UL

typedef struct uc3d_upkg_name_s {
    char name[UC3D_MAX_NAME];
    unsigned long flags;
    unsigned long file_offset;
} uc3d_upkg_name;

typedef struct uc3d_upkg_import_s {
    long class_package;
    long class_name;
    long package_index;
    long object_name;
    char class_package_name[UC3D_MAX_NAME];
    char class_name_text[UC3D_MAX_NAME];
    char object_name_text[UC3D_MAX_NAME];
    char package_path[UC3D_MAX_REF_PATH];
    char full_path[UC3D_MAX_REF_PATH];
    unsigned long file_offset;
} uc3d_upkg_import;

typedef struct uc3d_upkg_export_s {
    long class_index;
    long super_index;
    long package_index;
    long object_name;
    unsigned long object_flags;
    long serial_size;
    long serial_offset;
    unsigned long serial_crc32;
    char object_name_text[UC3D_MAX_NAME];
    char class_path[UC3D_MAX_REF_PATH];
    char super_path[UC3D_MAX_REF_PATH];
    char package_path[UC3D_MAX_REF_PATH];
    char full_path[UC3D_MAX_REF_PATH];
    unsigned long file_offset;
    int payload_in_range;
} uc3d_upkg_export;

typedef struct uc3d_upkg_s {
    unsigned long magic;
    unsigned long version;
    unsigned long package_flags;
    unsigned long guid[4];
    unsigned long generation_count;
    unsigned long header_size_guess;

    unsigned long declared_name_count;
    unsigned long declared_import_count;
    unsigned long declared_export_count;
    unsigned long name_offset;
    unsigned long import_offset;
    unsigned long export_offset;

    uc3d_upkg_name names[UC3D_MAX_PKG_NAMES];
    unsigned long name_count;

    uc3d_upkg_import imports[UC3D_MAX_PKG_IMPORTS];
    unsigned long import_count;

    uc3d_upkg_export exports[UC3D_MAX_PKG_EXPORTS];
    unsigned long export_count;

    unsigned long warning_count;
    unsigned long overflow_count;
} uc3d_upkg;

void uc3d_upkg_init(uc3d_upkg *pkg);
int uc3d_parse_upkg_buffer(const unsigned char *data,
                           unsigned long data_size,
                           uc3d_upkg *pkg);
int uc3d_upkg_read_compact_index(const unsigned char *data,
                                 unsigned long data_size,
                                 unsigned long *offset,
                                 long *out_value);
int uc3d_upkg_resolve_object_ref(const uc3d_upkg *pkg,
                                 long object_ref,
                                 char *out,
                                 unsigned long out_size);
int uc3d_upkg_find_export_by_name(const uc3d_upkg *pkg, const char *name);
int uc3d_upkg_find_export_by_class(const uc3d_upkg *pkg, const char *class_substr);
int uc3d_upkg_get_export_payload(const unsigned char *data,
                                 unsigned long data_size,
                                 const uc3d_upkg_export *exp,
                                 const unsigned char **payload,
                                 unsigned long *payload_size);

void uc3d_apply_mesh_ref_transform(const uc3d_mesh_ref *ref,
                                   uc3d_vertex *vertices,
                                   unsigned long vertex_count);

#ifdef __cplusplus
}
#endif

#endif
