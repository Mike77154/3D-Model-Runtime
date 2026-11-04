#ifndef MODELRT_INTERNAL_H
#define MODELRT_INTERNAL_H

#include "../include/model_runtime89.h"

#define MODELRT_PROBE_NO 0
#define MODELRT_PROBE_MAYBE 25
#define MODELRT_PROBE_YES 100

typedef struct modelrt_provider_s {
    modelrt_u32 codec_id;
    const char *name;
    int (*probe)(const char *path, const modelrt_u8 *head, size_t head_size);
    int (*open)(modelrt_runtime *rt, modelrt_handle *handle, const char *path);
    void (*close)(modelrt_handle *handle);
    int (*visit)(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                 const modelrt_visit_sink *sink, void *user);
    int (*animation_count)(const modelrt_handle *handle);
    int (*animation_info)(modelrt_handle *handle, int animation_index,
                          modelrt_animation_info *out_info);
    int (*texture_count)(const modelrt_handle *handle);
    int (*texture_info)(modelrt_handle *handle, int texture_index,
                        modelrt_texture_info *out_info);
    int (*texture_decode_rgba)(modelrt_handle *handle, int texture_index,
                               modelrt_u8 *dst, size_t dst_capacity,
                               modelrt_u32 *out_width, modelrt_u32 *out_height,
                               size_t *out_bytes);
    int (*texture_encoded_view)(modelrt_handle *handle, int texture_index,
                                modelrt_texture_blob_view *out_view);
    int (*animation_bind)(modelrt_handle *target, modelrt_handle *source,
                          modelrt_animation_binding *binding);
    int (*visit_bound_animation)(modelrt_animation_binding *binding,
                                 int animation_index, modelrt_q16 time,
                                 const modelrt_visit_sink *sink, void *user);
    /* Optional exact dense-frame visitor. Providers that recover authored
       frames can expose animation independently of a seconds/tick rate. */
    int (*visit_animation_frame)(modelrt_handle *handle, int animation_index,
                                 modelrt_u32 frame_index,
                                 const modelrt_visit_sink *sink, void *user);
} modelrt_provider;

void *modelrt_arena_alloc(modelrt_handle *handle, size_t bytes, size_t alignment);
void *modelrt_binding_alloc(modelrt_animation_binding *binding, size_t bytes, size_t alignment);
void modelrt_zero(void *p, size_t bytes);
void modelrt_copy_string(char *dst, size_t cap, const char *src);
int modelrt_ext_eq(const char *path, const char *ext);
int modelrt_path_has_suffix(const char *path, const char *suffix);
int modelrt_read_file_arena(modelrt_handle *handle, const char *path,
                            modelrt_u8 **out_data, size_t *out_size);
int modelrt_read_file_head(const char *path, modelrt_u8 *dst, size_t cap, size_t *out_size);
void modelrt_set_error(modelrt_runtime *rt, const char *provider_name, int code, const char *detail);

extern const modelrt_provider modelrt_provider_fbx89;
extern const modelrt_provider modelrt_provider_xps89;
extern const modelrt_provider modelrt_provider_hl1modelib89;
extern const modelrt_provider modelrt_provider_mmd89;
extern const modelrt_provider modelrt_provider_g3d89;
extern const modelrt_provider modelrt_provider_re89;
extern const modelrt_provider modelrt_provider_re3_89;
extern const modelrt_provider modelrt_provider_gunsurvivor89;
extern const modelrt_provider modelrt_provider_hod2_89;
extern const modelrt_provider modelrt_provider_re4classic89;
extern const modelrt_provider modelrt_provider_rtcw89;
extern const modelrt_provider modelrt_provider_3dgs89;
extern const modelrt_provider modelrt_provider_frag_arg89;
extern const modelrt_provider modelrt_provider_frag_ugh89;
extern const modelrt_provider modelrt_provider_blend89;
extern const modelrt_provider modelrt_provider_blade_xmdl89;
extern const modelrt_provider modelrt_provider_blade_xir89;
extern const modelrt_provider modelrt_provider_x89;
extern const modelrt_provider modelrt_provider_serioussam89;
extern const modelrt_provider modelrt_provider_pskpsa89;
extern const modelrt_provider modelrt_provider_hl2modelib89;
extern const modelrt_provider modelrt_provider_ms3d89;
extern const modelrt_provider modelrt_provider_c4d89;
extern const modelrt_provider modelrt_provider_dts89;

extern const modelrt_provider modelrt_provider_gackt_off89;
extern const modelrt_provider modelrt_provider_gackt_obj89;
extern const modelrt_provider modelrt_provider_gackt_ply89;
extern const modelrt_provider modelrt_provider_gackt_irr89;
extern const modelrt_provider modelrt_provider_gackt_stl89;
extern const modelrt_provider modelrt_provider_gackt_md289;

extern const modelrt_provider modelrt_provider_gackt_ac3d89;
extern const modelrt_provider modelrt_provider_gackt_3ds89;
extern const modelrt_provider modelrt_provider_gackt_lwo89;
extern const modelrt_provider modelrt_provider_gackt_ogex89;
extern const modelrt_provider modelrt_provider_gackt_raw89;
extern const modelrt_provider modelrt_provider_gackt_nff89;
extern const modelrt_provider modelrt_provider_gackt_b3d89;
extern const modelrt_provider modelrt_provider_gackt_q3d89;
extern const modelrt_provider modelrt_provider_gackt_hmb89;
extern const modelrt_provider modelrt_provider_native_ase89;
extern const modelrt_provider modelrt_provider_native_md389;
extern const modelrt_provider modelrt_provider_native_vrml1_89;
extern const modelrt_provider modelrt_provider_native_vrml97_89;
extern const modelrt_provider modelrt_provider_native_ogre89;
extern const modelrt_provider modelrt_provider_native_gltf89;
extern const modelrt_provider modelrt_provider_native_acc89;
extern const modelrt_provider modelrt_provider_native_sib89;
extern const modelrt_provider modelrt_provider_native_cob89;
extern const modelrt_provider modelrt_provider_native_md5_89;
extern const modelrt_provider modelrt_provider_native_amf89;
extern const modelrt_provider modelrt_provider_native_ter89;
extern const modelrt_provider modelrt_provider_native_ndo89;
extern const modelrt_provider modelrt_provider_native_ask89;
extern const modelrt_provider modelrt_provider_native_dbo89;
extern const modelrt_provider modelrt_provider_native_amj89;
extern const modelrt_provider modelrt_provider_native_quake89;
extern const modelrt_provider modelrt_provider_native_mdc89;
extern const modelrt_provider modelrt_provider_native_x3d89;
extern const modelrt_provider modelrt_provider_native_lxo89;
extern const modelrt_provider modelrt_provider_native_xgl89;
extern const modelrt_provider modelrt_provider_native_dxf89;
extern const modelrt_provider modelrt_provider_native_xsi89;
extern const modelrt_provider modelrt_provider_native_ifc89;
extern const modelrt_provider modelrt_provider_native_bvh_anima89;
extern const modelrt_provider modelrt_provider_native_bvh_bravure89;
extern const modelrt_provider modelrt_provider_native_dae89;
extern const modelrt_provider modelrt_provider_native_tmd89;
extern const modelrt_provider modelrt_provider_native_vta89;
extern const modelrt_provider modelrt_provider_native_md4_89;
extern const modelrt_provider modelrt_provider_native_md7_89;
extern const modelrt_provider modelrt_provider_native_uc89;
extern const modelrt_provider modelrt_provider_native_ue1_89;
extern const modelrt_provider modelrt_provider_native_panda89;
extern const modelrt_provider modelrt_provider_native_3mf89;
extern const modelrt_provider modelrt_provider_native_mpkf89;
extern const modelrt_provider modelrt_provider_native_m3d89;
extern const modelrt_provider modelrt_provider_native_csm89;
extern const modelrt_provider modelrt_provider_native_lws89;
extern const modelrt_provider modelrt_provider_native_poser89;
extern const modelrt_provider modelrt_provider_native_dff89;
extern const modelrt_provider modelrt_provider_native_mot89;
extern const modelrt_provider modelrt_provider_native_step89;
extern const modelrt_provider modelrt_provider_native_scn89;
extern const modelrt_provider modelrt_provider_native_usd89;
extern const modelrt_provider modelrt_provider_native_iqm89;
#endif
