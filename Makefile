CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c89 -pedantic-errors -Wall -Wextra -O2

# Optional built-in image decoders. All are ON by default.
IMAGE_PNG ?= 1
IMAGE_BMP ?= 1
IMAGE_DDS ?= 1
IMAGE_TGA ?= 1
IMAGE_JPG ?= 1
IMAGE_TIM ?= 1
IMAGE_TIFF ?= 1
IMAGE_PCX ?= 1
CPPFLAGS = -Ivendor/gackt_3dcodecs/mf3d_decoder_c89/src -Ivendor/gackt_3dcodecs/mp_kf_decoder_c89/include -Ivendor/gackt_3dcodecs/uc3d_decoder/include -Ivendor/gackt_3dcodecs/ue1_3d_codec_c89/include -Ivendor/gackt_3dcodecs/p3codec_c89/include -Ivendor/gackt_3dcodecs/md4_codec_c89/include -Ivendor/gackt_3dcodecs/md7_codec_c89/include -Ivendor/gackt_3dcodecs/c3dae_collada_codec_v9_deep_modules/include -Ivendor/gackt_3dcodecs/psx_tmd_codec3d_c89/include -Ivendor/gackt_3dcodecs/vta_decoder3d_c89/include -Iinclude -Isrc \
 -Ivendor/3dfbxplus/3dfbx -Ivendor/3dfbxplus/fbx_codec \
 -Ivendor/xps_uvfix_work/include -Ivendor/hl1modelib -Ivendor/hl2modelib/include \
 -Ivendor/mmd_codec_c89_fixed_v3/include -Ivendor/genesis3d_c89_codec \
 -Ivendor/re1codec_v0_3_mvref/include -Ivendor/re3_codec_v0_28/include \
 -Ivendor/xparsermodget98_v0_5/include -Ivendor/xparsermodget98_v0_5/src \
 -Ivendor/zlib89/include -Ivendor/png89/include -Ivendor/bmp89/include \
 -Ivendor/dds89/include -Ivendor/tga89/include -Ivendor/jpg89/include \
 -Ivendor/tim89/include -Ivendor/tiff89/include -Ivendor/pcx89/include \
 -Ivendor/SeriousSamClassicCodec89_v0_9/include \
 -Ivendor/pskpsa3d_c89_actorx_rootfix_v3_portable/include \
 -Ivendor/giffany_ms3d_codec_c89/include \
 -Ivendor/Cinema4DCodec89_v0_14/include -Ivendor/Cinema4DCodec89_v0_14/src \
 -Ivendor/dts89/include -Ivendor/re4_classic_v5_2/include
CPPFLAGS += -DMODELRT_IMAGE_ENABLE_PNG=$(IMAGE_PNG) \
 -DMODELRT_IMAGE_ENABLE_BMP=$(IMAGE_BMP) -DMODELRT_IMAGE_ENABLE_DDS=$(IMAGE_DDS) \
 -DMODELRT_IMAGE_ENABLE_TGA=$(IMAGE_TGA) -DMODELRT_IMAGE_ENABLE_JPG=$(IMAGE_JPG) \
 -DMODELRT_IMAGE_ENABLE_TIM=$(IMAGE_TIM) -DMODELRT_IMAGE_ENABLE_TIFF=$(IMAGE_TIFF) -DMODELRT_IMAGE_ENABLE_PCX=$(IMAGE_PCX)

CORE = src/providers/provider_native_iqm89.c src/modelrt_prj89.c src/providers/provider_native_usd89.c src/providers/provider_native_final89.c src/providers/provider_native_dff89.c src/providers/provider_native_poser89.c src/providers/provider_native_batch8_89.c src/providers/provider_native_repair89.c src/providers/provider_native_batch5b_89.c src/providers/provider_native_batch5a_89.c src/providers/provider_native_gltf89.c src/providers/provider_native_ogre89.c src/providers/provider_native_vrml97_89.c src/providers/provider_native_ase89.c src/providers/provider_native_md389.c src/providers/provider_native_vrml1_89.c src/providers/provider_gackt_wave3_89.c src/providers/provider_gackt_wave2_89.c src/providers/provider_gackt_geometry89.c src/providers/provider_gackt_irrlicht89.c src/providers/provider_gackt_stl89.c src/providers/provider_gackt_md289.c \
 src/model_runtime89.c src/image/model_image89.c \
 src/providers/provider_fbx89.c src/providers/provider_xps89.c \
 src/providers/provider_hl1modelib89.c src/providers/provider_hl2modelib89.c src/providers/provider_mmd89.c \
 src/providers/provider_g3d89.c src/providers/provider_re89.c src/providers/provider_re3_89.c src/providers/provider_gunsurvivor89.c src/providers/provider_hod2_89.c src/providers/provider_re4classic89.c src/providers/provider_rtcw89.c src/providers/provider_3dgs89.c \
 src/providers/provider_frag_r16_89.c src/providers/provider_blend89.c src/providers/provider_blade3d89.c src/providers/provider_x89.c \
 src/providers/provider_serioussam89.c src/providers/provider_pskpsa89.c \
 src/providers/provider_ms3d89.c src/providers/provider_c4d89.c src/providers/provider_dts89.c

FBX = vendor/3dfbxplus/3dfbx/fbx_alloc.c \
 vendor/3dfbxplus/3dfbx/fbx_fixed.c \
 vendor/3dfbxplus/3dfbx/fbx_gackt_export.c \
 vendor/3dfbxplus/3dfbx/fbx_gackt_runtime.c \
 vendor/3dfbxplus/3dfbx/special_adapters/fbx_legacy6000_adapter.c \
 vendor/3dfbxplus/3dfbx/fbx_anim.c vendor/3dfbxplus/3dfbx/fbx_arrays.c \
 vendor/3dfbxplus/3dfbx/fbx_connections.c vendor/3dfbxplus/3dfbx/fbx_importer.c \
 vendor/3dfbxplus/3dfbx/fbx_importer_ascii.c vendor/3dfbxplus/3dfbx/fbx_importer_bin.c \
 vendor/3dfbxplus/3dfbx/fbx_material.c vendor/3dfbxplus/3dfbx/fbx_mesh_ext.c \
 vendor/3dfbxplus/3dfbx/fbx_nurbs.c vendor/3dfbxplus/3dfbx/fbx_scene.c \
 vendor/3dfbxplus/3dfbx/fbx_scene_transverse.c vendor/3dfbxplus/3dfbx/fbx_version.c \
 vendor/3dfbxplus/fbx_codec/fbx_mini_zlib.c vendor/3dfbxplus/fbx_codec/fbx_stream.c \
 vendor/3dfbxplus/fbx_codec/fbx_semantic_writer.c vendor/3dfbxplus/fbx_codec/fbx_codec_decode.c \
 vendor/3dfbxplus/fbx_codec/fbx_codec_writer_ascii.c vendor/3dfbxplus/fbx_codec/fbx_binary_writer.c

XPS = vendor/xps_uvfix_work/src/xps_api.c vendor/xps_uvfix_work/src/xps_detect.c \
 vendor/xps_uvfix_work/src/xps_util.c vendor/xps_uvfix_work/src/xps_fx.c \
 vendor/xps_uvfix_work/src/xps_io.c vendor/xps_uvfix_work/src/xps_header.c \
 vendor/xps_uvfix_work/src/xps_ascii.c vendor/xps_uvfix_work/src/xps_bin.c \
 vendor/xps_uvfix_work/src/xps_pose.c vendor/xps_uvfix_work/src/xps_bonedict.c \
 vendor/xps_uvfix_work/src/xps_scene.c

HLFP = vendor/hl1modelib/hlfp_common.c vendor/hl1modelib/hlfp_smd.c \
 vendor/hl1modelib/hlfp_mdl.c vendor/hl1modelib/hlfp_math.c \
 vendor/hl1modelib/hlfp_blend.c vendor/hl1modelib/hlfp_ctrlquat.c \
 vendor/hl1modelib/hlfp_goldsrc.c

MMD = vendor/mmd_codec_c89_fixed_v3/src/mmd_codec.c
G3D = vendor/genesis3d_c89_codec/g3d_codec.c vendor/genesis3d_c89_codec/g3d_kib.c
FRAGR16 = vendor/fragformats_r16/vendor_fragmotion/arg_vendor.c \
 vendor/fragformats_r16/vendor_common/fixed_matrix.c \
 vendor/fragformats_r16/vendor_directx_x/x_vendor.c \
 vendor/fragformats_r16/vendor_dbo/dbo_vendor.c
CPPFLAGS += -Ivendor/fragformats_r16/vendor_fragmotion -Ivendor/fragformats_r16/vendor_common

# Phase18 SDNA core native adapter (strict C89, uncompressed only).
# CLI main and zlib/zstd static binary vendors intentionally NOT linked.
CPPFLAGS += -Ivendor/blend_c89_phase18/include
BLEND18 = $(filter-out %/main.c %/bc_wrapper.c,$(wildcard vendor/blend_c89_phase18/src/*.c))

RE1C = vendor/re1codec_v0_3_mvref/src/re1codec.c
RE3C = vendor/re3_codec_v0_28/src/re3_codec.c vendor/re3_codec_v0_28/src/re3_packed.c
RTCWC = vendor/rtcw_natsuki_v19/rtcw_codec.c vendor/rtcw_natsuki_v19/rtcw_mds_pose.c
GS3DC = $(addprefix vendor/3dgs_phase15/,$(shell cat vendor/3dgs_phase15/SOURCES_C89.txt))
CPPFLAGS += -Ivendor/3dgs_phase15/include
HOD2C = vendor/hod2_phase12/native_c89/hod2_pol.c \
 vendor/hod2_phase12/native_c89/hod2_tables.c \
 vendor/hod2_phase12/native_c89/hod2_tablepack.c \
 vendor/hod2_phase12/inherited/c89/hod2_extra_parts.c \
 vendor/hod2_phase12/inherited/c89/hod2_resource_guard.c \
 src/providers/hod2_binary_runtime.c
RE4C = $(wildcard vendor/re4_classic_v5_2/src/core/*.c) \
 $(wildcard vendor/re4_classic_v5_2/archive/*.c) \
 $(wildcard vendor/re4_classic_v5_2/texture/*.c) \
 $(wildcard vendor/re4_classic_v5_2/model/*.c) \
 $(wildcard vendor/re4_classic_v5_2/animation/*.c) \
 $(wildcard vendor/re4_classic_v5_2/event/*.c)
X89 = vendor/xparsermodget98_v0_5/src/xp_mszip_stream.c \
 vendor/xparsermodget98_v0_5/src/third_party/puff.c \
 vendor/xparsermodget98_v0_5/src/xp_reader.c vendor/xparsermodget98_v0_5/src/xp_lexer.c \
 vendor/xparsermodget98_v0_5/src/xp_blexer.c vendor/xparsermodget98_v0_5/src/xp_parser.c \
 vendor/xparsermodget98_v0_5/src/xp_templates.c vendor/xparsermodget98_v0_5/src/xp_semantics.c vendor/xparsermodget98_v0_5/src/xp_anim_pose.c vendor/xparsermodget98_v0_5/src/xp_mszip.c

# Native experimental codec vendors; legacy GACKT router is NOT linked.
NATIVE_WAVE4 = $(wildcard vendor/gackt_3dcodecs/gltf3d_c89_fixed/src/*.c) \
 vendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/src/omc_core.c \
 vendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/src/omc_fixed.c \
 vendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/src/omc_xml.c \
 vendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/src/omc_binary.c \
 vendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/src/omc_write.c \
 vendor/gackt_3dcodecs/vrml97_wrl_fixed_codec_v4/src/vrml97_codec.c \
 vendor/gackt_3dcodecs/ase_decoder3d_c89_fixed/src/ase3d.c \
 $(wildcard vendor/gackt_3dcodecs/md3_fixed_codec_q3heur/src/*.c) \
 vendor/gackt_3dcodecs/vrml1_wrl_c89_codec/src/vr1_fixed.c \
 vendor/gackt_3dcodecs/vrml1_wrl_c89_codec/src/vr1_scene.c \
 vendor/gackt_3dcodecs/vrml1_wrl_c89_codec/src/vr1_lexer.c \
 vendor/gackt_3dcodecs/vrml1_wrl_c89_codec/src/vr1_parser.c
CPPFLAGS += -Ivendor/gackt_3dcodecs/gltf3d_c89_fixed/include \
 -Ivendor/gackt_3dcodecs/ogre_mesh_codec_c89_v12/include \
 -Ivendor/gackt_3dcodecs/vrml97_wrl_fixed_codec_v4/include \
 -Ivendor/gackt_3dcodecs/ase_decoder3d_c89_fixed/include \
 -Ivendor/gackt_3dcodecs/md3_fixed_codec_q3heur/include \
 -Ivendor/gackt_3dcodecs/vrml1_wrl_c89_codec/include

GACKT_WAVE3 = vendor/gackt_3dcodecs/b3d_c89_codec/src/b3d_c89.c \
 vendor/gackt_3dcodecs/q3d_codec_c89/src/q3d_codec.c \
 vendor/gackt_3dcodecs/hmb3d_codec_c89/src/hmb3d.c
CPPFLAGS += -Ivendor/gackt_3dcodecs/b3d_c89_codec/src \
 -Ivendor/gackt_3dcodecs/q3d_codec_c89/include \
 -Ivendor/gackt_3dcodecs/hmb3d_codec_c89/src

GACKT_WAVE2 = vendor/gackt_3dcodecs/ac3d_decoder3d_c89_fixed/src/ac3d_decoder.c \
 vendor/gackt_3dcodecs/g3ds_fixed_codec_c89_v4/src/g3ds_fixed.c \
 vendor/gackt_3dcodecs/lwo3d_c89_push/src/lwo3d.c \
 vendor/gackt_3dcodecs/giffany_ogex_decoder3d_c89_v4/src/gif_ogex_fixed.c \
 vendor/gackt_3dcodecs/rawtri89/src/rawtri89.c \
 vendor/gackt_3dcodecs/nff89_enff_lib/src/nff89.c \
 vendor/gackt_3dcodecs/nff89_enff_lib/src/nff89_cad.c
CPPFLAGS += -Ivendor/gackt_3dcodecs/ac3d_decoder3d_c89_fixed/src \
 -Ivendor/gackt_3dcodecs/g3ds_fixed_codec_c89_v4/include \
 -Ivendor/gackt_3dcodecs/lwo3d_c89_push/include \
 -Ivendor/gackt_3dcodecs/giffany_ogex_decoder3d_c89_v4/include \
 -Ivendor/gackt_3dcodecs/rawtri89/include \
 -Ivendor/gackt_3dcodecs/nff89_enff_lib/src

GACKT_NATIVE = $(wildcard vendor/gackt_3dcodecs/md2fx_c89/src/*.c) \
 vendor/gackt_3dcodecs/stl3d_decoder_c89/src/stl3d_decoder.c \
 vendor/gackt_3dcodecs/stl3d_decoder_c89/src/stl3d_fixed.c \
 vendor/gackt_3dcodecs/off3d_c89/src/off3d.c \
 vendor/gackt_3dcodecs/giffy_obj_codec_c89_fixed_v2/src/gobj_obj.c \
 vendor/gackt_3dcodecs/giffy_obj_codec_c89_fixed_v2/src/gobj_mtl.c \
 vendor/gackt_3dcodecs/gif_ply_codec_c89/src/gif_ply_codec.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_attr.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_common.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_fx.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_mesh.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_mesh_stream.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_raw.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_scene.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_scene_stream.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_validate.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_writer.c \
 vendor/gackt_3dcodecs/irrfxlib_v3/src/ir_xml.c

CPPFLAGS += -Ivendor/gackt_3dcodecs/md2fx_c89/include \
 -Ivendor/gackt_3dcodecs/stl3d_decoder_c89/include \
 -Ivendor/gackt_3dcodecs/off3d_c89/include \
 -Ivendor/gackt_3dcodecs/giffy_obj_codec_c89_fixed_v2/include \
 -Ivendor/gackt_3dcodecs/gif_ply_codec_c89/include \
 -Ivendor/gackt_3dcodecs/irrfxlib_v3/include

IMAGE89 =
ifeq ($(IMAGE_PCX),1)
IMAGE89 += vendor/pcx89/src/pcx89.c
endif
ifneq ($(filter 1,$(IMAGE_PNG) $(IMAGE_TIFF)),)
IMAGE89 += vendor/zlib89/src/zlib89.c
endif
ifeq ($(IMAGE_PNG),1)
IMAGE89 += vendor/png89/src/png89.c
endif
ifeq ($(IMAGE_BMP),1)
IMAGE89 += vendor/bmp89/src/bmp89.c
endif
ifeq ($(IMAGE_DDS),1)
IMAGE89 += vendor/dds89/src/dds89.c
endif
ifeq ($(IMAGE_TGA),1)
IMAGE89 += vendor/tga89/src/tga89.c
endif
ifeq ($(IMAGE_JPG),1)
IMAGE89 += vendor/jpg89/src/jpg89.c
endif
ifeq ($(IMAGE_TIM),1)
IMAGE89 += vendor/tim89/src/tim89.c
endif
ifeq ($(IMAGE_TIFF),1)
IMAGE89 += vendor/tiff89/src/tiff89.c
endif

SS89 = vendor/SeriousSamClassicCodec89_v0_9/src/ss89.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89normals.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89fx.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89fxpasses.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89env.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89settings.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89resources.c \
 vendor/SeriousSamClassicCodec89_v0_9/src/ss89skin.c
ACTORX = vendor/pskpsa3d_c89_actorx_rootfix_v3_portable/src/pskpsa3d.c

HL2M = vendor/hl2modelib/src/hl2modelib.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_native_anim89.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_poseparams121.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_quaternion89.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_sequence_eval89.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_sequence_layers89.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_virtual_layers124.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_virtualmodel122.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_virtualmodel123.c \
 vendor/hl2modelib/vendors/source/animator/hl2m_virtualmodel124.c \
 vendor/hl2modelib/vendors/source/formats/ani_vendor89.c \
 vendor/hl2modelib/vendors/source/formats/qc_vendor89.c \
 vendor/hl2modelib/vendors/source/formats/smd_vendor89.c \
 vendor/hl2modelib/vendors/source/formats/vta_vendor89.c \
 vendor/hl2modelib/vendors/source/ik/hl2m_ik89.c \
 vendor/hl2modelib/vendors/source/ik/hl2m_ik_2bone89.c \
 vendor/hl2modelib/vendors/source/master/hl2m_master89.c \
 vendor/hl2modelib/vendors/source/mesher/hl2m_mesher89.c \
 vendor/hl2modelib/vendors/source/physics/hl2m_character89.c \
 vendor/hl2modelib/vendors/source/physics/hl2m_physics89.c \
 vendor/hl2modelib/vendors/source/shaderer/hl2m_shaderer89.c \
 vendor/hl2modelib/vendors/source/shaderer/hl2m_smooth89.c \
 vendor/hl2modelib/vendors/source/texture/hl2m_vtf89.c \
 vendor/hl2modelib/vendors/source/texturizer/hl2m_material_sampler89.c \
 vendor/hl2modelib/vendors/source/texturizer/hl2m_shader89.c \
 vendor/hl2modelib/vendors/source/texturizer/hl2m_uv_filter89.c

MS3DC = vendor/giffany_ms3d_codec_c89/src/gif_ms3d_codec.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_normals.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_skin.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_quat.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_uv.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_mtl.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_player.c \
 vendor/giffany_ms3d_codec_c89/src/gif_ms3d_render.c

DTSC = vendor/dts89/src/dts89_anim.c \
 vendor/dts89/src/dts89_animer.c \
 vendor/dts89/src/dts89_core.c \
 vendor/dts89/src/dts89_decode.c \
 vendor/dts89/src/dts89_detach.c \
 vendor/dts89/src/dts89_dsq.c \
 vendor/dts89/src/dts89_encode.c \
 vendor/dts89/src/dts89_lighting40.c \
 vendor/dts89/src/dts89_lod41.c \
 vendor/dts89/src/dts89_mesher.c \
 vendor/dts89/src/dts89_posefeamw.c \
 vendor/dts89/src/dts89_probe.c \
 vendor/dts89/src/dts89_runtime.c \
 vendor/dts89/src/dts89_shaderer.c \
 vendor/dts89/src/dts89_texturer_opener.c \
 vendor/dts89/src/dts89_tscript.c \
 vendor/dts89/src/dts89_uv_texturer.c \
 vendor/dts89/src/dts89_validate.c

C4DC = vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_anim.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_anim_angle.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_anim_frame.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_anim_sample.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_animer_module.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_joint_records.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_rig_evidence.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_skin_core.c \
 vendor/Cinema4DCodec89_v0_14/modules/animer/c4d89_weight_inventory.c \
 vendor/Cinema4DCodec89_v0_14/modules/mesher/c4d89_mesh_obj.c \
 vendor/Cinema4DCodec89_v0_14/modules/mesher/c4d89_mesh_scan.c \
 vendor/Cinema4DCodec89_v0_14/modules/mesher/c4d89_mesh_stream.c \
 vendor/Cinema4DCodec89_v0_14/modules/mesher/c4d89_mesher_module.c \
 vendor/Cinema4DCodec89_v0_14/modules/shaderer/c4d89_shaderer_module.c \
 vendor/Cinema4DCodec89_v0_14/modules/shaderer/c4d89_smooth.c \
 vendor/Cinema4DCodec89_v0_14/modules/shaderer/c4d89_smooth_obj.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_asset_resolve.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_bitmap_catalog.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_bmp.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_dispatch.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_internal.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_jpeg.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_png.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_probe.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_resource.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_img_tga.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_texture_hints.c \
 vendor/Cinema4DCodec89_v0_14/modules/texturer_opener/c4d89_texturer_opener_module.c \
 vendor/Cinema4DCodec89_v0_14/modules/uv_texturer/c4d89_poly_regions.c \
 vendor/Cinema4DCodec89_v0_14/modules/uv_texturer/c4d89_texturetag_links.c \
 vendor/Cinema4DCodec89_v0_14/modules/uv_texturer/c4d89_uv_texturer_module.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_binary.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_extract.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_floatq.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_jpeg.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_magic.c \
 vendor/Cinema4DCodec89_v0_14/src/c4d89_names.c

# Batch 5B — five additional native decoders.
BATCH5B = $(wildcard vendor/gackt_3dcodecs/ter3d_codec_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/ndo3d_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/giffy_ask3d_codec/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/dbo_fixed_c89_toolkit/src/*.c) \
 vendor/gackt_3dcodecs/amj3d_decoder_c89/src/amj3d.c
CPPFLAGS += -Ivendor/gackt_3dcodecs/ter3d_codec_c89/include \
 -Ivendor/gackt_3dcodecs/ndo3d_c89/include \
 -Ivendor/gackt_3dcodecs/giffy_ask3d_codec/include \
 -Ivendor/gackt_3dcodecs/dbo_fixed_c89_toolkit/include \
 -Ivendor/gackt_3dcodecs/amj3d_decoder_c89/include
# Batch 5: real native decoders. No GACKT-LANG.
CPPFLAGS += -DAMF_STRICT_C89_NO_LONGLONG=1
BATCH5A = $(wildcard vendor/gackt_3dcodecs/acc3d_codec_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/sib3d_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/giffany_cob_codec_c89_omega/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/md5fx_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/amf_chiquita3d/src/*.c)
CPPFLAGS += -Ivendor/gackt_3dcodecs/acc3d_codec_c89/include \
 -Ivendor/gackt_3dcodecs/sib3d_c89/include \
 -Ivendor/gackt_3dcodecs/giffany_cob_codec_c89_omega/include \
 -Ivendor/gackt_3dcodecs/md5fx_c89/include \
 -Ivendor/gackt_3dcodecs/amf_chiquita3d/include



# Native Wave 6: 10 direct decoders; no GACKT-LANG bridge.
CPPFLAGS += -DANIMA_BVH_MAX_FRAMES=5000 -DBRAVURE_BVH_MAX_FRAMES=5000
BATCH6 = src/providers/provider_native_batch6a_89.c src/providers/provider_native_batch6b_89.c \
 $(wildcard vendor/gackt_3dcodecs/quake_mdl_c89_fixed_v9_loader_hardened/quake_mdl_c89_fixed_v9_loader_hardened/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/mdc3d_codec_c89_v04/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/x3d_giffany_c89_v8/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/lxo3d_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/xgl_zgl_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/gfdxf_c89_fixed_codec_v13/gfdxf_c89_v13/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/gxsi_decoder3d_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/ifc3d_c89/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/anima_biovisionbvh/src/*.c) \
 $(wildcard vendor/gackt_3dcodecs/bravure_bvh/src/*.c)
CPPFLAGS += -Ivendor/gackt_3dcodecs/quake_mdl_c89_fixed_v9_loader_hardened/quake_mdl_c89_fixed_v9_loader_hardened/include \
 -Ivendor/gackt_3dcodecs/mdc3d_codec_c89_v04/include \
 -Ivendor/gackt_3dcodecs/x3d_giffany_c89_v8/include \
 -Ivendor/gackt_3dcodecs/lxo3d_c89/include \
 -Ivendor/gackt_3dcodecs/xgl_zgl_c89/src \
 -Ivendor/gackt_3dcodecs/gfdxf_c89_fixed_codec_v13/gfdxf_c89_v13/include \
 -Ivendor/gackt_3dcodecs/gxsi_decoder3d_c89/include \
 -Ivendor/gackt_3dcodecs/ifc3d_c89/include \
 -Ivendor/gackt_3dcodecs/anima_biovisionbvh/include \
 -Ivendor/gackt_3dcodecs/bravure_bvh/include

# Batch 7 native repair: direct codec APIs, no GACKT-language.
NATIVE_REPAIR = $(wildcard vendor/gackt_3dcodecs/c3dae_collada_codec_v9_deep_modules/src/*.c) \
 vendor/gackt_3dcodecs/psx_tmd_codec3d_c89/src/tmd_codec3d.c \
 vendor/gackt_3dcodecs/vta_decoder3d_c89/src/vta_decoder3d.c

NATIVE_MD = $(wildcard vendor/gackt_3dcodecs/md4_codec_c89/src/*.c) vendor/gackt_3dcodecs/md7_codec_c89/src/md7_codec.c
NATIVE_SCENES = $(wildcard vendor/gackt_3dcodecs/uc3d_decoder/src/*.c) $(wildcard vendor/gackt_3dcodecs/ue1_3d_codec_c89/src/*.c) $(wildcard vendor/gackt_3dcodecs/p3codec_c89/src/common/*.c) $(wildcard vendor/gackt_3dcodecs/p3codec_c89/src/egg/*.c)
NATIVE_MF_MPKF = $(wildcard vendor/gackt_3dcodecs/mf3d_decoder_c89/src/*.c) $(wildcard vendor/gackt_3dcodecs/mp_kf_decoder_c89/src/*.c)
NATIVE_BATCH8 = $(filter-out %/dff3d_dump.c %/dff3d_obj.c %/dff3d_mtl.c,$(wildcard vendor/gackt_3dcodecs/dff3d_c89_fixed/dff3d_c89_fixed/src/*.c)) vendor/gackt_3dcodecs/codec3d_poser_c89_v03/src/codec3d_poser.c vendor/gackt_3dcodecs/3d89_codec/src/m3d89.c vendor/gackt_3dcodecs/lws3d_c89/src/lws3d.c $(wildcard vendor/gackt_3dcodecs/CSM_chiquisumother3d/src/*.c)
CPPFLAGS += -Ivendor/gackt_3dcodecs/dff3d_c89_fixed/dff3d_c89_fixed/include -Ivendor/gackt_3dcodecs/codec3d_poser_c89_v03/include -Ivendor/gackt_3dcodecs/3d89_codec/include -Ivendor/gackt_3dcodecs/CSM_chiquisumother3d/include -Ivendor/gackt_3dcodecs/lws3d_c89/include
NATIVE_FINAL = vendor/gackt_3dcodecs/prj_c89_lib/src/prj_c89.c $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/src/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/zlib/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/crate_values/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/livrps_flatten/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/variant_prune/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_cpu/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/material_eval/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/volume_decode/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/crate_tables/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/value_resolution/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/variant_stack/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_final/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/material_graph/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/volume_metadata/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_deser/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/prim_index/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/variant_policy/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_runtime/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/shader_compiler/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_tree/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_abstract/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/composition_runtime/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_bake/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/shader_vm/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_sample/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_schema/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/composition_cache/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/variant_resolver/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/skel_mapper/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/shader_bytecode/*.c) $(wildcard vendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_bricks/*.c) vendor/gackt_3dcodecs/mot89_chiquis_fixed/src/mot89.c vendor/gackt_3dcodecs/step89/src/step89.c vendor/gackt_3dcodecs/step89/src/step89_geom.c vendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/src/scn3d_io.c vendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/src/scn3d_fixed.c vendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/src/scn3d_core.c vendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/src/scn3d_truespace.c vendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/src/scn3d_volume.c
CPPFLAGS += -Ivendor/gackt_3dcodecs/prj_c89_lib/include
CPPFLAGS += -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/include -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/zlib -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/crate_values -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/livrps_flatten -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/variant_prune -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_cpu -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/material_eval -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/volume_decode -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/crate_tables -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/value_resolution -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/variant_stack -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_final -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/material_graph -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/volume_metadata -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_deser -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/prim_index -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/variant_policy -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_runtime -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/shader_compiler -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_tree -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_abstract -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/composition_runtime -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/skinning_bake -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/shader_vm -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_sample -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/usdc_schema -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/composition_cache -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/variant_resolver -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/skel_mapper -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/shader_bytecode -Ivendor/gackt_3dcodecs/usd3d_decoder_c89/voxel_bricks
CPPFLAGS += -Ivendor/gackt_3dcodecs/mot89_chiquis_fixed/include -Ivendor/gackt_3dcodecs/step89/include -Ivendor/gackt_3dcodecs/giffany_scn3d_decoder_c89/include
SRC = $(NATIVE_FINAL) $(NATIVE_BATCH8) $(NATIVE_MF_MPKF) $(NATIVE_SCENES) $(NATIVE_MD) $(NATIVE_REPAIR) $(NATIVE_WAVE4) $(BATCH6) $(BATCH5A) $(BATCH5B) $(GACKT_WAVE3) $(GACKT_WAVE2) $(GACKT_NATIVE) $(MS3DC) $(C4DC) $(DTSC) $(CORE) $(SS89) $(ACTORX) $(HL2M) $(FBX) $(XPS) $(HLFP) $(MMD) $(G3D) $(RE1C) $(RE3C) $(HOD2C) $(RE4C) $(RTCWC) $(GS3DC) $(FRAGR16) $(BLEND18) $(X89) $(IMAGE89)
OBJ = $(SRC:.c=.o)

all: libmodel_runtime89.a

libmodel_runtime89.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	find src vendor -name '*.o' -delete
	rm -f libmodel_runtime89.a tests/smoke tests/fbx_regression tests/anim_attachment_regression tests/goldsrc_regression tests/goldsrc_axis_regression tests/goldsrc_transparency_regression tests/xps_regression tests/xps_uv_regression tests/re1codec_regression tests/re1codec_tmd_regression tests/re1codec_emw_regression tests/re3_runtime_regression tests/re3_vendor_test tests/re3_fixture.emd tests/gen_re3_fixture tests/directx_x_regression tests/directx_x_v05_animation_regression tests/directx_x_v05_frame_visit_regression tests/image_codec_regression tests/image_codec_advanced_regression tests/directx_x_open_matrix tests/directx_x_v04_semantics_regression tests/provider_fallback_regression

smoke: libmodel_runtime89.a tests/smoke.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/smoke.c libmodel_runtime89.a -o tests/smoke

fbx-regression: libmodel_runtime89.a tests/fbx_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/fbx_regression.c libmodel_runtime89.a -o tests/fbx_regression
	./tests/fbx_regression tests/rigged_test.fbx
	./tests/fbx_regression tests/bones_only_test.fbx

.PHONY: all clean smoke fbx-regression anim-attachment-regression goldsrc-regression goldsrc-axis-regression goldsrc-transparency-regression xps-regression xps-uv-regression re1codec-regression re1codec-tmd-regression re1codec-emw-regression re3-regression directx-x-regression directx-x-v05-animation directx-x-v05-frame-visit image-codec-regression image-codec-advanced-regression

anim-attachment-regression: libmodel_runtime89.a tests/anim_attachment_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/anim_attachment_regression.c libmodel_runtime89.a -o tests/anim_attachment_regression
	./tests/anim_attachment_regression tests/rigged_test.fbx tests/external_anim.fbx
	./tests/anim_attachment_regression tests/g3d_fixture/sample_body.bdy tests/g3d_fixture/sample_walk.mot



goldsrc-regression: libmodel_runtime89.a tests/goldsrc_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/goldsrc_regression.c libmodel_runtime89.a -o tests/goldsrc_regression
	./tests/goldsrc_regression tests/goldsrc_motion_regression.mdl


goldsrc-axis-regression: libmodel_runtime89.a tests/goldsrc_axis_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/goldsrc_axis_regression.c libmodel_runtime89.a -o tests/goldsrc_axis_regression
	./tests/goldsrc_axis_regression tests/goldsrc_axis_test.smd


goldsrc-transparency-regression: libmodel_runtime89.a tests/goldsrc_transparency_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/goldsrc_transparency_regression.c libmodel_runtime89.a -o tests/goldsrc_transparency_regression
	./tests/goldsrc_transparency_regression tests/goldsrc_masked_texture.mdl


xps-regression: libmodel_runtime89.a tests/xps_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/xps_regression.c libmodel_runtime89.a -o tests/xps_regression
	./tests/xps_regression tests/xps_regression_v3.xps tests/xps_regression_legacy.mesh tests/xps_regression_ascii.mesh.ascii tests/xps_regression_boned.mesh.ascii


xps-uv-regression: libmodel_runtime89.a tests/xps_uv_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/xps_uv_regression.c libmodel_runtime89.a -o tests/xps_uv_regression
	./tests/xps_uv_regression tests/xps_uv_layer_regression.mesh.ascii


re1codec-regression: libmodel_runtime89.a tests/re1codec_regression.c tests/re1codec_fixture.emd
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/re1codec_regression.c libmodel_runtime89.a -o tests/re1codec_regression
	./tests/re1codec_regression tests/re1codec_fixture.emd

re1codec-tmd-regression: libmodel_runtime89.a tests/re1codec_tmd_regression.c tests/re1codec_fixture.tmd
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/re1codec_tmd_regression.c libmodel_runtime89.a -o tests/re1codec_tmd_regression
	./tests/re1codec_tmd_regression tests/re1codec_fixture.tmd

re1codec-emw-regression: libmodel_runtime89.a tests/re1codec_emw_regression.c tests/re1codec_fixture.emw
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/re1codec_emw_regression.c libmodel_runtime89.a -o tests/re1codec_emw_regression
	./tests/re1codec_emw_regression tests/re1codec_fixture.emw


tests/gen_re3_fixture: tests/gen_re3_fixture.c
	$(CC) $(CFLAGS) tests/gen_re3_fixture.c -o tests/gen_re3_fixture

tests/re3_fixture.emd: tests/gen_re3_fixture
	./tests/gen_re3_fixture

re3-regression: libmodel_runtime89.a tests/re3_runtime_regression.c tests/re3_fixture.emd
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/re3_runtime_regression.c libmodel_runtime89.a -o tests/re3_runtime_regression
	./tests/re3_runtime_regression tests/re3_fixture.emd
	$(MAKE) -C vendor/re3_codec_v0_28 test


directx-x-regression: libmodel_runtime89.a tests/directx_x_regression.c tests/directx_x_runtime_test.x
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/directx_x_regression.c libmodel_runtime89.a -o tests/directx_x_regression
	./tests/directx_x_regression tests/directx_x_runtime_test.x


directx-x-open-matrix: libmodel_runtime89.a tests/directx_x_open_matrix.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/directx_x_open_matrix.c libmodel_runtime89.a -o tests/directx_x_open_matrix
	./tests/directx_x_open_matrix vendor/xparsermodget98_v0_5/tests/fixtures/*.x


directx-x-v04-semantics: libmodel_runtime89.a tests/directx_x_v04_semantics_regression.c tests/directx_x_v04_rig_uv_anim.x
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/directx_x_v04_semantics_regression.c libmodel_runtime89.a -o tests/directx_x_v04_semantics_regression
	./tests/directx_x_v04_semantics_regression tests/directx_x_v04_rig_uv_anim.x


directx-x-v05-animation: libmodel_runtime89.a tests/directx_x_v05_animation_regression.c tests/directx_x_v05_multiclip_poses.x tests/directx_x_v05_timed_skin.x
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/directx_x_v05_animation_regression.c libmodel_runtime89.a -o tests/directx_x_v05_animation_regression
	./tests/directx_x_v05_animation_regression tests/directx_x_v05_multiclip_poses.x tests/directx_x_v05_timed_skin.x


directx-x-v05-frame-visit: libmodel_runtime89.a tests/directx_x_v05_frame_visit_regression.c tests/directx_x_v05_unknown_rate_skin.x
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/directx_x_v05_frame_visit_regression.c libmodel_runtime89.a -o tests/directx_x_v05_frame_visit_regression
	./tests/directx_x_v05_frame_visit_regression tests/directx_x_v05_unknown_rate_skin.x


image-codec-regression: libmodel_runtime89.a tests/image_codec_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/image_codec_regression.c libmodel_runtime89.a -o tests/image_codec_regression
	./tests/image_codec_regression tests/image_fixtures


image-codec-advanced-regression: libmodel_runtime89.a tests/image_codec_advanced_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/image_codec_advanced_regression.c libmodel_runtime89.a -o tests/image_codec_advanced_regression
	./tests/image_codec_advanced_regression tests/image_fixtures_adv

provider-fallback-regression: libmodel_runtime89.a tests/provider_fallback_regression.c tests/re1codec_fixture.emd
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/provider_fallback_regression.c libmodel_runtime89.a -o tests/provider_fallback_regression
	./tests/provider_fallback_regression tests/re1codec_fixture.emd

# New Source/HL2 regression: MDL+VVD+VTX, VMT->VTF, SMD; includes real vendor fixture.
.PHONY: hl2modelib-regression
hl2modelib-regression: libmodel_runtime89.a tests/hl2modelib_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/hl2modelib_regression.c libmodel_runtime89.a -o tests/hl2modelib_regression
	./tests/hl2modelib_regression tests/source_fixture/source_model.mdl tests/source_fixture/source_model.source.smd

# v19 integration smoke (generated native DTS fixture + original MS3D fixture)
v19-regression: libmodel_runtime89.a tests/v19_allcodecs_regression.c tests/v19_generate_dts.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/v19_generate_dts.c libmodel_runtime89.a -o tests/v19_generate_dts
	./tests/v19_generate_dts tests/v19_synthetic.dts
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/v19_allcodecs_regression.c libmodel_runtime89.a -o tests/v19_allcodecs_regression
	./tests/v19_allcodecs_regression vendor/giffany_ms3d_codec_c89/tests/tiny.ms3d tests/v19_synthetic.dts

.PHONY: v19-regression

# Native experimental provider certification (requires external corpus paths)
.PHONY: native-wave4-test
native-wave4-test: libmodel_runtime89.a tests/native_wave4_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/native_wave4_regression.c libmodel_runtime89.a -o tests/native_wave4_regression


# Compile the independent native C89 batch-5 corpus harness.
.PHONY: native-batch5-build
native-batch5-build: libmodel_runtime89.a
	$(CC) $(CFLAGS) -Iinclude tests/native_batch5_regression.c libmodel_runtime89.a -o tests/native_batch5_regression

.PHONY: native-batch6-build
native-batch6-build: libmodel_runtime89.a



# Verified native Batch 7 harness, no GACKT interchange
native-batch7-regression: libmodel_runtime89.a tests/native_repair_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/native_repair_regression.c libmodel_runtime89.a -o tests/native_repair_regression
	./tests/native_repair_regression

# Native IQM v2 dense-frame pose regression; supply your own Assimp corpus.
.PHONY: native-iqm-frame-build
native-iqm-frame-build: libmodel_runtime89.a tests/native_iqm_frames89_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/native_iqm_frames89_regression.c libmodel_runtime89.a -lm -o tests/native_iqm_frames89_regression

# IQM native skinning/rotation regression. Synthetic asset generated by C89 source;
# neither the fixture nor the compiled tools ship in the source-only ZIP.
.PHONY: native-iqm-skinning-build native-iqm-skinning-smoke
native-iqm-skinning-build: libmodel_runtime89.a tests/native_iqm_skinning_fixture89.c tests/native_iqm_rotation89_regression.c tests/native_iqm_skinning89_regression.c
	$(CC) $(CFLAGS) tests/native_iqm_skinning_fixture89.c -o tests/native_iqm_skinning_fixture89
	$(CC) $(CFLAGS) -Iinclude tests/native_iqm_rotation89_regression.c libmodel_runtime89.a -lm -o tests/native_iqm_rotation89_regression
	$(CC) $(CFLAGS) -Iinclude tests/native_iqm_skinning89_regression.c libmodel_runtime89.a -lm -o tests/native_iqm_skinning89_regression

native-iqm-skinning-smoke: native-iqm-skinning-build
	./tests/native_iqm_skinning_fixture89 tests/iqm_generated_rotation.iqm
	./tests/native_iqm_rotation89_regression tests/iqm_generated_rotation.iqm
	rm -f tests/iqm_generated_rotation.iqm

.PHONY: native-iqm-interpolation-build native-iqm-interpolation-smoke
native-iqm-interpolation-build: libmodel_runtime89.a tests/native_iqm_interpolation89_regression.c tests/native_iqm_skinning_fixture89.c
	$(CC) $(CFLAGS) tests/native_iqm_skinning_fixture89.c -o tests/native_iqm_skinning_fixture89
	$(CC) $(CFLAGS) -Iinclude tests/native_iqm_interpolation89_regression.c libmodel_runtime89.a -lm -o tests/native_iqm_interpolation89_regression

native-iqm-interpolation-smoke: native-iqm-interpolation-build
	./tests/native_iqm_skinning_fixture89 tests/iqm_rot_synthetic.iqm
	./tests/native_iqm_skinning_fixture89 tests/iqm_scale_synthetic.iqm nonuniform
	./tests/native_iqm_interpolation89_regression tests/iqm_rot_synthetic.iqm tests/iqm_scale_synthetic.iqm
	rm -f tests/iqm_rot_synthetic.iqm tests/iqm_scale_synthetic.iqm

.PHONY: native-iqm-interpolation-corpus-build
native-iqm-interpolation-corpus-build: libmodel_runtime89.a tests/native_iqm_interpolation_corpus89.c
	$(CC) $(CFLAGS) -Iinclude tests/native_iqm_interpolation_corpus89.c libmodel_runtime89.a -lm -o tests/native_iqm_interpolation_corpus89

# Gun Survivor v19 integration (no game assets required for synthetic smoke).
.PHONY: gunsurvivor-regression-build gunsurvivor-synthetic

gunsurvivor-regression-build: libmodel_runtime89.a tests/gunsurvivor_runtime_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/gunsurvivor_runtime_regression.c libmodel_runtime89.a -o tests/gunsurvivor_runtime_regression

gunsurvivor-synthetic: gunsurvivor-regression-build tests/gen_gunsurvivor_fixture.c
	$(CC) $(CFLAGS) tests/gen_gunsurvivor_fixture.c -o tests/gen_gunsurvivor_fixture
	./tests/gen_gunsurvivor_fixture tests/gs_synthetic
	./tests/gunsurvivor_runtime_regression tests/gs_synthetic.NAB 1 1 1
	./tests/gunsurvivor_runtime_regression tests/gs_synthetic.NAC 0 0 1
	rm -f tests/gs_synthetic.NAB tests/gs_synthetic.NAC

.PHONY: hod2-regression-build hod2-synthetic hod2-tool-check
hod2-regression-build: libmodel_runtime89.a tests/hod2_runtime_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/hod2_runtime_regression.c libmodel_runtime89.a -o tests/hod2_runtime_regression

hod2-synthetic: hod2-regression-build tests/gen_hod2_fixture.c
	$(CC) $(CFLAGS) tests/gen_hod2_fixture.c -o tests/gen_hod2_fixture
	mkdir -p tests/pol
	./tests/gen_hod2_fixture tests/hod2_fixture.pol tests/hod2_fixture.h2mc tests/pol/hod2_fixture.bin
	./tests/hod2_runtime_regression tests/hod2_fixture.pol 1 1 0
	./tests/hod2_runtime_regression tests/pol/hod2_fixture.bin 1 1 0
	./tests/hod2_runtime_regression tests/hod2_fixture.h2mc 0 0 1

hod2-tool-check:
	$(MAKE) -C vendor/hod2_phase12/native_c89 check

# Phase12 standalone H2TB tables (no executables or models required)
.PHONY: hod2-phase12-tables
hod2-phase12-tables: hod2-regression-build tests/hod2_tables_regression
	./tests/hod2_tables_regression vendor/hod2_phase12/tables/HOD2_1999_tables.h2tb 0
	./tests/hod2_tables_regression vendor/hod2_phase12/tables/HOD2_2000_tables.h2tb 1

tests/hod2_tables_regression: tests/hod2_tables_regression.c libmodel_runtime89.a
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/hod2_tables_regression.c libmodel_runtime89.a -o tests/hod2_tables_regression

# RE4 Classic v4.1 provider: author-created fixture generator, no game assets.
.PHONY: re4classic-regression-build re4classic-synthetic
re4classic-regression-build: libmodel_runtime89.a tests/re4classic_runtime_regression.c
	$(CC) $(CFLAGS) -Iinclude tests/re4classic_runtime_regression.c libmodel_runtime89.a -o tests/re4classic_runtime_regression

re4classic-synthetic: re4classic-regression-build tests/gen_re4classic_fixture.c
	$(CC) $(CFLAGS) tests/gen_re4classic_fixture.c -o tests/gen_re4classic_fixture
	./tests/gen_re4classic_fixture tests/re4_synthetic
	./tests/re4classic_runtime_regression tests/re4_synthetic.pmd tests/re4_synthetic.tpl tests/re4_synthetic.fcv
	rm -f tests/re4_synthetic.pmd tests/re4_synthetic.tpl tests/re4_synthetic.fcv

# RE4 Classic v4.1: real runtime FCV/PMD fixed-point skeletal binding,
# rigid & two-weight skin, explicit validated extra-part retargeting.
.PHONY: re4classic-animated-synthetic
re4classic-animated-synthetic: libmodel_runtime89.a tests/gen_re4_skeletal_fixture.c tests/re4_skeletal_runtime_regression.c
	$(CC) $(CFLAGS) tests/gen_re4_skeletal_fixture.c -o tests/gen_re4_skeletal_fixture
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/re4_skeletal_runtime_regression.c libmodel_runtime89.a -o tests/re4_skeletal_runtime_regression
	./tests/gen_re4_skeletal_fixture tests/re4_skeletal_synthetic
	./tests/re4_skeletal_runtime_regression tests/re4_skeletal_synthetic.pmd tests/re4_skeletal_synthetic.fcv
	rm -f tests/re4_skeletal_synthetic.pmd tests/re4_skeletal_synthetic.fcv

# Inspect a user's native RE4 master PMD, explicitly paired FCV and accessories.
.PHONY: re4-character-inspect
re4-character-inspect: libmodel_runtime89.a examples/re4_character_inspect89.c
	$(CC) $(CFLAGS) $(CPPFLAGS) examples/re4_character_inspect89.c libmodel_runtime89.a -o examples/re4_character_inspect89

# Quake IDPO v6 corpus regression (source only; pass your corpus paths explicitly).
.PHONY: quake-corpus-regression-build
quake-corpus-regression-build: libmodel_runtime89.a tests/quake_mdl_corpus89.c
	$(CC) $(CFLAGS) -Iinclude tests/quake_mdl_corpus89.c libmodel_runtime89.a -o tests/quake_mdl_corpus89

# Quake II MD2 + PCX standalone corpus regression (source-only test tools).
.PHONY: quake2-md2-corpus-build quake2-pcx-corpus-build
quake2-md2-corpus-build: libmodel_runtime89.a tests/quake2_md2_corpus89.c
	$(CC) $(CFLAGS) -Iinclude tests/quake2_md2_corpus89.c libmodel_runtime89.a -o tests/quake2_md2_corpus89
quake2-pcx-corpus-build: libmodel_runtime89.a tests/quake2_pcx_corpus89.c
	$(CC) $(CFLAGS) -Iinclude tests/quake2_pcx_corpus89.c libmodel_runtime89.a -o tests/quake2_pcx_corpus89

# Two-codec upgrade smoke test (synthetic fixtures are generated by tests/)
rtcw-re4-v52-test: libmodel_runtime89.a tests/rtcw_re4_v52_runtime_regression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/rtcw_re4_v52_runtime_regression.c libmodel_runtime89.a -o tests/rtcw_re4_v52_runtime_regression

# 3D GameStudio Fase15: generate non-commercial test assets on demand.
.PHONY: gs3d-phase14-runtime-test
gs3d-phase14-runtime-test: libmodel_runtime89.a tests/gs3d_phase14_runtime_regression.c
	python3 vendor/3dgs_phase15/tests/make_fixtures.py
	python3 vendor/3dgs_phase15/tests/make_phase12_fixtures.py
	python3 vendor/3dgs_phase15/tests/make_phase14_texture_fixtures.py
	$(CC) $(CFLAGS) -Werror -Iinclude tests/gs3d_phase14_runtime_regression.c libmodel_runtime89.a -o tests/gs3d_phase14_runtime_regression
	@set -e; for f in a4_mdl3 a5_mdl4 a6_mdl5 a7_a8_mdl7 mdl7_weighted_valid mdl7_skeletal_sequence mdl7_all_dds_and_reference; do \
	  ./tests/gs3d_phase14_runtime_regression vendor/3dgs_phase15/tests/fixtures/$$f.mdl; \
	done

# Fase15: regression in both legacy and new file-space matrix modes.
.PHONY: gs3d-phase15-runtime-test
gs3d-phase15-runtime-test: gs3d-phase14-runtime-test
	./tests/gs3d_phase14_runtime_regression vendor/3dgs_phase15/tests/fixtures/mdl7_weighted_valid.mdl file
	./tests/gs3d_phase14_runtime_regression vendor/3dgs_phase15/tests/fixtures/mdl7_skeletal_sequence.mdl file

.PHONY: frag-r16-runtime-test
frag-r16-runtime-test: libmodel_runtime89.a tests/frag_r16_runtime_regression.c
	python3 vendor/fragformats_r16/tests/create_synthetic_arg.py
	python3 -c "from pathlib import Path; Path('vendor/fragformats_r16/tests/probe_only.ugh').write_bytes(b'ugh0'+bytes(12))"
	$(CC) $(CFLAGS) -Werror -Iinclude tests/frag_r16_runtime_regression.c libmodel_runtime89.a -o tests/frag_r16_runtime_regression
	./tests/frag_r16_runtime_regression vendor/fragformats_r16/tests/synthetic_arg_10001.arg vendor/fragformats_r16/tests/synthetic_arg_10002.arg vendor/fragformats_r16/tests/bad_index.arg vendor/fragformats_r16/tests/truncated.arg vendor/fragformats_r16/tests/probe_only.ugh


.PHONY: frag-r16-audit-test frag-r16-vendor-test
frag-r16-audit-test: libmodel_runtime89.a tests/frag_r16_audit_regression.c
	python3 vendor/fragformats_r16/tests/create_directx_samples.py
	python3 vendor/fragformats_r16/tests/create_synthetic_dbo.py
	$(CC) $(CFLAGS) -Werror -Iinclude tests/frag_r16_audit_regression.c libmodel_runtime89.a -o tests/frag_r16_audit_regression
	./tests/frag_r16_audit_regression vendor/fragformats_r16/tests/directx_txt0302.x vendor/fragformats_r16/tests/synthetic_plane.dbo

frag-r16-vendor-test:
	$(MAKE) -C vendor/fragformats_r16 test

.PHONY: frag-r16-original-providers-test
frag-r16-original-providers-test: libmodel_runtime89.a tests/frag_r16_original_providers.c
	python3 vendor/fragformats_r16/tests/create_directx_samples.py
	python3 vendor/fragformats_r16/tests/create_synthetic_dbo.py
	$(CC) $(CFLAGS) -Werror -Iinclude tests/frag_r16_original_providers.c libmodel_runtime89.a -o tests/frag_r16_original_providers
	./tests/frag_r16_original_providers vendor/fragformats_r16/tests/directx_txt0302.x vendor/fragformats_r16/tests/synthetic_plane.dbo

# Phase18 native BLEND integration and R22 UGH nested-header regression.
.PHONY: blend-phase18-runtime-test frag-r22-ugh-runtime-test
blend-phase18-runtime-test: libmodel_runtime89.a tests/blend_phase18_runtime_regression.c
	$(CC) -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tests/blend_phase18_runtime_regression.c libmodel_runtime89.a -o tests/blend_phase18_runtime_regression
	./tests/blend_phase18_runtime_regression vendor/blend_c89_phase18/examples/phase13_two_rigs_SYNTHETIC.blend vendor/blend_c89_phase18/examples/phase16_two_rigs_ik_SYNTHETIC.blend
frag-r22-ugh-runtime-test: libmodel_runtime89.a tests/frag_r22_ugh_header_regression.c
	$(MAKE) -C vendor/fragformats_r22 all
	cd vendor/fragformats_r22 && sh tests/run_tests_r22.sh
	$(CC) -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tests/frag_r22_ugh_header_regression.c libmodel_runtime89.a -o tests/frag_r22_ugh_header_regression
	./tests/frag_r22_ugh_header_regression vendor/fragformats_r22/tests/generated_r22/two_stage.ugh vendor/fragformats_r22/tests/generated_r22/bad_inner.ugh vendor/fragformats_r22/tests/generated_r22/new_version.ugh

# Research-only Blade3D diagnostics: XIR1 is NOT native .xmdl.
.PHONY: blade3d-xir-runtime-test
blade3d-xir-runtime-test: libmodel_runtime89.a tests/blade3d_xir_runtime_regression.c
	python3 tests/blade3d_create_probe_fixtures.py
	$(CC) -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude tests/blade3d_xir_runtime_regression.c libmodel_runtime89.a -o tests/blade3d_xir_runtime_regression
	./tests/blade3d_xir_runtime_regression vendor/fragformats_r22/tests/synthetic_mesh.xir vendor/fragformats_r22/tests/bad_index_mesh.xir vendor/fragformats_r22/tests/truncated_mesh.xir vendor/fragformats_r22/tests/bad_count_mesh.xir tests/blade_xmdl_header_candidate.xmdl tests/blade_xmdl_bad_header.xmdl tests/blade_xir_nan_position.xir
