#ifndef HL2MODELIB_H
#define HL2MODELIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define HL2M_VERSION_MAJOR 1
#define HL2M_VERSION_MINOR 0
#define HL2M_VERSION_PATCH 24

#define HL2M_OK 0
#define HL2M_ERR_NULL -1
#define HL2M_ERR_RANGE -2
#define HL2M_ERR_MAGIC -3
#define HL2M_ERR_VERSION -4
#define HL2M_ERR_LIMIT -5
#define HL2M_ERR_PARSE -6
#define HL2M_ERR_UNSUPPORTED -7

#define HL2M_FIXED_ONE 65536
#define HL2M_NAME_MAX 64
#define HL2M_PATH_MAX 128
#define HL2M_VALUE_MAX 128
#define HL2M_MAX_LODS 8
#define HL2M_MAX_BONES 256
#define HL2M_MAX_TEXTURES 256
#define HL2M_MAX_CD_TEXTURES 128
#define HL2M_MAX_BODYPARTS 64
#define HL2M_MAX_MODELS 256
#define HL2M_MAX_MESHES 1024
#define HL2M_MAX_HITBOXSETS 32
#define HL2M_MAX_HITBOXES 512
#define HL2M_MAX_VVD_VERTS 131072
#define HL2M_MAX_VVD_FIXUPS 2048
#define HL2M_MAX_VTX_BODYPARTS 64
#define HL2M_MAX_VTX_MODELS 256
#define HL2M_MAX_VTX_LODS 512
#define HL2M_MAX_VTX_MESHES 2048
#define HL2M_MAX_VTX_STRIPGROUPS 4096
#define HL2M_MAX_VTX_STRIPS 8192
#define HL2M_MAX_VTX_VERTICES 262144
#define HL2M_MAX_VTX_INDICES 1048576
#define HL2M_MAX_SMD_NODES 256
#define HL2M_MAX_SMD_POSES 8192
#define HL2M_MAX_SMD_MATERIALS 256
#define HL2M_MAX_SMD_VERTICES 32768
#define HL2M_MAX_SMD_TRIANGLES 10922
#define HL2M_MAX_VMT_PARAMS 256
#define HL2M_MAX_MDL_ANIMS 512
#define HL2M_MAX_MDL_SEQUENCES 512
#define HL2M_MAX_MDL_ANIMBLOCKS 256
#define HL2M_MAX_ANIM_CHANNELS 4096
#define HL2M_MAX_VTX_MATERIAL_REPLACEMENTS 1024
#define HL2M_MAX_PHY_SOLIDS 256
#define HL2M_MAX_QC_MATERIAL_REPLACES 256
#define HL2M_MAX_QC_COLLISIONS 64
#define HL2M_MAX_VTA_FLEXES 128
#define HL2M_MAX_VTA_VERTICES 32768
#define HL2M_MAX_QC_FLEXCONTROLLERS 128
#define HL2M_MAX_QC_FLEXRULES 128
#define HL2M_MAX_QC_SEQUENCE_EVENTS 512
#define HL2M_MAX_QC_IKCHAINS 64
#define HL2M_MAX_QC_IKRULES 256
#define HL2M_MAX_CONVEX_HULLS 128
#define HL2M_MAX_CONVEX_VERTICES 1024
#define HL2M_MAX_MDL_EVENTS 512
#define HL2M_MAX_MDL_IKRULES 256
#define HL2M_MAX_COMPILE_LODS 8
#define HL2M_COMPILE_WARN_NO_ANIM 0x0001
#define HL2M_COMPILE_WARN_NO_VTA 0x0002
#define HL2M_COMPILE_WARN_PHY_BOX_ONLY 0x0004
#define HL2M_COMPILE_WARN_LOD_DECIMATED 0x0008
#define HL2M_COMPILE_WARN_LIMIT_TRUNCATED 0x0010
#define HL2M_COMPILE_WARN_BAD_BONE_REF 0x0020
#define HL2M_COMPILE_WARN_WEIGHT_REPAIRED 0x0040
#define HL2M_COMPILE_WARN_VTA_RANGE 0x0080
#define HL2M_COMPILE_WARN_ANIM_NODE_MISMATCH 0x0100
#define HL2M_COMPILE_WARN_QC_INCOMPLETE 0x0200
#define HL2M_COMPILE_WARN_BUNDLE_MISMATCH 0x0400
#define HL2M_COMPILE_WARN_CONVEX_APPROX 0x0800
#define HL2M_COMPILE_WARN_IK_METADATA 0x1000
#define HL2M_COMPILE_WARN_FLEX_RULE_SIMPLE 0x2000
#define HL2M_COMPILE_WARN_SEQUENCE_EVENTS 0x4000
#define HL2M_COMPILE_WARN_HITBOX_AUTOGEN 0x8000

#define HL2M_COMPILE_WARN_FLEX_EVAL_APPROX 0x00010000
#define HL2M_COMPILE_WARN_IK_SOLVE_APPROX 0x00020000
#define HL2M_COMPILE_WARN_CONVEX_SPLIT 0x00040000
#define HL2M_COMPILE_WARN_EVENT_WINDOW 0x00080000
#define HL2M_COMPILE_WARN_STUDIOMDL_GAP 0x00100000

#define HL2M_PARITY_QC_STRUCTURED 0x0001
#define HL2M_PARITY_BINARY_WRITERS 0x0002
#define HL2M_PARITY_VTA_FLEX 0x0004
#define HL2M_PARITY_FLEX_RULE_EVAL 0x0008
#define HL2M_PARITY_IK_METADATA 0x0010
#define HL2M_PARITY_IK_RUNTIME 0x0020
#define HL2M_PARITY_SEQUENCE_EVENTS 0x0040
#define HL2M_PARITY_HITBOXES 0x0080
#define HL2M_PARITY_PHY_CONVEX 0x0100
#define HL2M_PARITY_LOD_WRITER 0x0200
#define HL2M_PARITY_ANIM_BLOCKS 0x0400
#define HL2M_PARITY_MATERIAL_REPLACEMENTS 0x0800
#define HL2M_PARITY_ANIM_SAMPLER 0x1000
#define HL2M_PARITY_CPU_SKINNING 0x2000
#define HL2M_PARITY_EVENT_CURSOR 0x4000
#define HL2M_PARITY_LOD_RUNTIME 0x8000
#define HL2M_PARITY_POSE_BLEND 0x00010000
#define HL2M_PARITY_ATTACHMENT_RUNTIME 0x00020000
#define HL2M_PARITY_HITBOX_RUNTIME 0x00040000
#define HL2M_PARITY_DRAW_FILTER 0x00080000
#define HL2M_PARITY_SEQUENCE_EVAL 0x00100000

#define HL2M_FLEX_EVAL_STACK_MAX 32
#define HL2M_EVENT_WINDOW_MAX 128
#define HL2M_MAX_POSE_MATRICES HL2M_MAX_BONES
#define HL2M_POSE_INCLUDE_ROTATION 1
#define HL2M_POSE_TRANSLATION_ONLY 0

#define HL2M_COLLISION_AABB 0
#define HL2M_COLLISION_PER_BONE_AABB 1
#define HL2M_COLLISION_PER_MATERIAL_AABB 2
#define HL2M_COLLISION_BONE_MATERIAL_AABB 3

#define HL2M_BUNDLE_WARN_MDL_BAD_MAGIC 0x0001
#define HL2M_BUNDLE_WARN_VVD_BAD_MAGIC 0x0002
#define HL2M_BUNDLE_WARN_VTX_BAD_VERSION 0x0004
#define HL2M_BUNDLE_WARN_CHECKSUM_MISMATCH 0x0008
#define HL2M_BUNDLE_WARN_MDL_LENGTH_MISMATCH 0x0010
#define HL2M_BUNDLE_WARN_PHY_CHECKSUM_MISMATCH 0x0020
#define HL2M_BUNDLE_WARN_ANI_CHECKSUM_MISMATCH 0x0040
#define HL2M_BUNDLE_WARN_MISSING_OPTIONAL 0x0080

#define HL2M_SKIN_WARN_NO_WEIGHTS 0x0001
#define HL2M_SKIN_WARN_BAD_BONE 0x0002
#define HL2M_SKIN_WARN_PARTIAL_WEIGHT 0x0004
#define HL2M_SKIN_WARN_APPROX_INVERSE 0x0008

#define HL2M_ANIM_WARN_MISSING_FRAME 0x0001
#define HL2M_ANIM_WARN_CLAMPED_TIME 0x0002
#define HL2M_ANIM_WARN_NODE_FALLBACK 0x0004

#define HL2M_ATTACHMENT_WARN_MISSING_BONE 0x0001
#define HL2M_HITBOX_WARN_MISSING_BONE 0x0001
#define HL2M_DRAW_FILTER_WARN_FALLBACK_LOD 0x0001
#define HL2M_RUNTIME_WARN_SAMPLE 0x0001
#define HL2M_RUNTIME_WARN_IK 0x0002
#define HL2M_RUNTIME_WARN_SKIN 0x0004
#define HL2M_RUNTIME_WARN_FLEX 0x0008
#define HL2M_RUNTIME_WARN_EVENTS 0x0010
#define HL2M_RUNTIME_WARN_LOD 0x0020


#define HL2M_MAX_QC_COMMANDS 512
#define HL2M_MAX_QC_ARGS 24
#define HL2M_MAX_QC_BODIES 128
#define HL2M_MAX_QC_SEQUENCES 128
#define HL2M_MAX_QC_MATERIAL_DIRS 64
#define HL2M_MAX_QC_LODS 64
#define HL2M_MAX_QC_ATTACHMENTS 128
#define HL2M_MAX_QC_HITBOXES 512
#define HL2M_MAX_QC_DEFINEBONES 256
#define HL2M_MAX_QC_POSEPARAMS 128
#define HL2M_MAX_ENGINE_VERTICES 1048576
#define HL2M_MAX_ENGINE_INDICES 1048576
#define HL2M_MAX_ENGINE_DRAWS 4096

#define HL2M_QC_UNKNOWN 0
#define HL2M_QC_MODELNAME 1
#define HL2M_QC_BODY 2
#define HL2M_QC_MODEL 3
#define HL2M_QC_BODYGROUP 4
#define HL2M_QC_STUDIO 5
#define HL2M_QC_BLANK 6
#define HL2M_QC_CDMATERIALS 7
#define HL2M_QC_SEQUENCE 8
#define HL2M_QC_ANIMATION 9
#define HL2M_QC_COLLISIONMODEL 10
#define HL2M_QC_COLLISIONJOINTS 11
#define HL2M_QC_STATICPROP 12
#define HL2M_QC_SURFACEPROP 13
#define HL2M_QC_CONTENTS 14
#define HL2M_QC_SCALE 15
#define HL2M_QC_ORIGIN 16
#define HL2M_QC_ROTATE 17
#define HL2M_QC_TEXTUREGROUP 18
#define HL2M_QC_KEYVALUES 19
#define HL2M_QC_INCLUDEMODEL 20
#define HL2M_QC_ATTACHMENT 21
#define HL2M_QC_BONEMERGE 22
#define HL2M_QC_HITBOXSET 23
#define HL2M_QC_HBOX 24
#define HL2M_QC_DEFINEBONE 25
#define HL2M_QC_LOD 26
#define HL2M_QC_RENAMEMATERIAL 27
#define HL2M_QC_EYEPOSITION 28
#define HL2M_QC_ILLUMPOSITION 29
#define HL2M_QC_POSEPARAMETER 30
#define HL2M_QC_WEIGHTLIST 31
#define HL2M_QC_IKCHAIN 32
#define HL2M_QC_IKRULE 33
#define HL2M_QC_BBOX 34
#define HL2M_QC_CBOX 35
#define HL2M_QC_MOSTLYOPAQUE 36
#define HL2M_QC_OPAQUE 37
#define HL2M_QC_TRANSLUCENT 38
#define HL2M_QC_AMBIENTBOOST 39
#define HL2M_QC_MAXVERTS 40
#define HL2M_QC_MAXBONES 41
#define HL2M_QC_JIGGLEBONE 42
#define HL2M_QC_PROCEDURALBONES 43
#define HL2M_QC_ALWAYS_COLLAPSE 44
#define HL2M_QC_COLLAPSEBONES 45
#define HL2M_QC_KEEPBONE 46
#define HL2M_QC_SKIPBONEINBBOX 47
#define HL2M_QC_TEXRENDERMODE 48
#define HL2M_QC_JOINTMERGE 49
#define HL2M_QC_REPLACEMODEL 50
#define HL2M_QC_REPLACEMATERIAL 51
#define HL2M_QC_SHADOWLOD 52
#define HL2M_QC_EVENT 53
#define HL2M_QC_FPS 54
#define HL2M_QC_FLEXFILE 55
#define HL2M_QC_FLEXCONTROLLER 56
#define HL2M_QC_FLEXRULE 57
#define HL2M_QC_FLEXCONTROLLERUI 58
#define HL2M_QC_FLEX 59
#define HL2M_QC_FLEXPAIR 60

#define HL2M_PRIM_TRIANGLES 0
#define HL2M_PRIM_TRIANGLE_STRIP 1
#define HL2M_PRIM_RAW_INDEX_STREAM 2

#define HL2M_FORMAT_UNKNOWN 0
#define HL2M_FORMAT_MDL 1
#define HL2M_FORMAT_VVD 2
#define HL2M_FORMAT_VTX 3
#define HL2M_FORMAT_SMD 4
#define HL2M_FORMAT_VMT 5
#define HL2M_FORMAT_QC 6
#define HL2M_FORMAT_PHY 7
#define HL2M_FORMAT_ANI 8
#define HL2M_FORMAT_VTA 9
#define HL2M_FORMAT_VTF 10

#define HL2M_VMT_FLAG_TRANSLUCENT 0x0001
#define HL2M_VMT_FLAG_ALPHATEST 0x0002
#define HL2M_VMT_FLAG_NOCULL 0x0004
#define HL2M_VMT_FLAG_SELFILLUM 0x0008
#define HL2M_VMT_FLAG_ADDITIVE 0x0010
#define HL2M_VMT_FLAG_VERTEXLIT 0x0020
#define HL2M_VMT_FLAG_UNLIT 0x0040
#define HL2M_VMT_FLAG_LIGHTMAPPED 0x0080

#define HL2M_ENGINE_WARN_BODYPART_MISMATCH 0x0001
#define HL2M_ENGINE_WARN_EMPTY_DRAWS 0x0002
#define HL2M_ENGINE_WARN_CHECKSUM_MISMATCH 0x0004
#define HL2M_ENGINE_WARN_MATERIAL_REPLACED 0x0008

#define HL2M_PHY_MAGIC_VPHY 0x59485056u
#define HL2M_PHY_MAGIC_IVPS 0x53505649u

#define HL2M_MDL_MAGIC 0x54534449u
#define HL2M_VVD_MAGIC 0x56534449u
#define HL2M_VVD_LEGACY_MAGIC 0x49445653u
#define HL2M_MDL_STUDIO_VERSION 48
#define HL2M_VVD_VERSION 4
#define HL2M_VTX_VERSION 7
#define HL2M_MAX_BONES_PER_VERT 3

/* 16.16 signed fixed point. */
typedef int hl2m_fixed;
typedef unsigned char hl2m_u8;
typedef signed char hl2m_i8;
typedef unsigned short hl2m_u16;
typedef short hl2m_i16;
typedef unsigned int hl2m_u32;
typedef int hl2m_i32;

typedef struct hl2m_vec2_s {
    hl2m_fixed x;
    hl2m_fixed y;
} hl2m_vec2;

typedef struct hl2m_vec3_s {
    hl2m_fixed x;
    hl2m_fixed y;
    hl2m_fixed z;
} hl2m_vec3;

typedef struct hl2m_vec4_s {
    hl2m_fixed x;
    hl2m_fixed y;
    hl2m_fixed z;
    hl2m_fixed w;
} hl2m_vec4;

typedef struct hl2m_mdl_bone_s {
    int parent;
    int bonecontroller[6];
    char name[HL2M_NAME_MAX];
    hl2m_vec3 pos;
    hl2m_vec4 quat;
    hl2m_vec3 rot;
    hl2m_vec3 posscale;
    hl2m_vec3 rotscale;
    /* Exact Source IEEE-754 animation scales, preserved without FP arithmetic.
       16.16 quantization of tiny values would erase face channels. */
    hl2m_u32 posscale_ieee32[3];
    hl2m_u32 rotscale_ieee32[3];
    int flags;
    int proctype;
    int procindex;
    int physicsbone;
    int surfacepropidx;
    int contents;
} hl2m_mdl_bone;

typedef struct hl2m_mdl_texture_s {
    char name[HL2M_PATH_MAX];
    int flags;
    int used;
} hl2m_mdl_texture;

typedef struct hl2m_mdl_mesh_s {
    int material;
    int model_index;
    int numvertices;
    int vertexoffset;
    int numflexes;
    int materialtype;
    int materialparam;
    int meshid;
    hl2m_vec3 center;
} hl2m_mdl_mesh;

typedef struct hl2m_mdl_model_s {
    char name[HL2M_NAME_MAX];
    int type;
    hl2m_fixed boundingradius;
    int nummeshes;
    int first_mesh;
    int numvertices;
    int vertexindex;
    int tangentsindex;
} hl2m_mdl_model;

typedef struct hl2m_mdl_bodypart_s {
    char name[HL2M_NAME_MAX];
    int nummodels;
    int base;
    int first_model;
} hl2m_mdl_bodypart;

typedef struct hl2m_mdl_hitboxset_s {
    char name[HL2M_NAME_MAX];
    int first_hitbox;
    int numhitboxes;
} hl2m_mdl_hitboxset;

typedef struct hl2m_mdl_hitbox_s {
    char name[HL2M_NAME_MAX];
    int bone;
    int group;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
} hl2m_mdl_hitbox;

typedef struct hl2m_mdl_animblock_s {
    int datastart;
    int dataend;
    int data_size;
} hl2m_mdl_animblock;

typedef struct hl2m_mdl_animdesc_s {
    char name[HL2M_NAME_MAX];
    hl2m_fixed fps;
    int flags;
    int numframes;
    int nummovements;
    int movementindex;
    int animblock;
    int animindex;
    int numikrules;
    int ikruleindex;
    int animblockikruleindex;
    int sectionindex;
    int sectionframes;
    int zeroframespan;
    int zeroframecount;
    int zeroframeindex;
    int first_anim_offset;
    int anim_channel_count;
    int anim_flags_or;
} hl2m_mdl_animdesc;

typedef struct hl2m_mdl_seqdesc_s {
    char label[HL2M_NAME_MAX];
    char activity_name[HL2M_NAME_MAX];
    int flags;
    int activity;
    int actweight;
    int numevents;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
    int numblends;
    int groupsize[2];
    int paramindex[2];
    hl2m_fixed paramstart[2];
    hl2m_fixed paramend[2];
    hl2m_fixed fadeintime;
    hl2m_fixed fadeouttime;
    int animindexindex;
    int keyvalueindex;
    int keyvaluesize;
} hl2m_mdl_seqdesc;

typedef struct hl2m_mdl_event_s {
    int sequence;
    hl2m_fixed cycle;
    int event_id;
    int type;
    char options[64];
    char name[HL2M_NAME_MAX];
} hl2m_mdl_event;

typedef struct hl2m_mdl_ikrule_s {
    int anim;
    int type;
    int chain;
    int bone;
    int slot;
    hl2m_fixed start;
    hl2m_fixed peak;
    hl2m_fixed tail;
    hl2m_fixed end;
    hl2m_fixed contact;
    hl2m_fixed drop;
    hl2m_fixed top;
    char attachment[HL2M_NAME_MAX];
} hl2m_mdl_ikrule;

typedef struct hl2m_anim_channel_s {
    int anim;
    int bone;
    int flags;
    int data_offset;
    int next_offset;
} hl2m_anim_channel;

typedef struct hl2m_mdl_s {
    int id;
    int version;
    int checksum;
    char name[HL2M_NAME_MAX];
    int length;
    hl2m_vec3 eyeposition;
    hl2m_vec3 illumposition;
    hl2m_vec3 hull_min;
    hl2m_vec3 hull_max;
    hl2m_vec3 view_bbmin;
    hl2m_vec3 view_bbmax;
    int flags;
    int source_numbones;
    int source_numtextures;
    int source_numbodyparts;
    int source_numhitboxsets;
    int bone_count;
    int texture_count;
    int cdtexture_count;
    int bodypart_count;
    int model_count;
    int mesh_count;
    int hitboxset_count;
    int hitbox_count;
    int source_numlocalanim;
    int source_numlocalseq;
    int source_numanimblocks;
    int anim_count;
    int sequence_count;
    int animblock_count;
    int anim_channel_count;
    int event_count;
    int ikrule_count;
    int flexdesc_count;
    int flexcontroller_count;
    int flexrule_count;
    int ikchain_count;
    hl2m_fixed mass;
    int contents;
    char surfaceprop[HL2M_NAME_MAX];
    char animblock_name[HL2M_PATH_MAX];
    int keyvalueindex;
    int keyvaluesize;
    hl2m_mdl_bone bones[HL2M_MAX_BONES];
    hl2m_mdl_texture textures[HL2M_MAX_TEXTURES];
    char cdtextures[HL2M_MAX_CD_TEXTURES][HL2M_PATH_MAX];
    hl2m_mdl_bodypart bodyparts[HL2M_MAX_BODYPARTS];
    hl2m_mdl_model models[HL2M_MAX_MODELS];
    hl2m_mdl_mesh meshes[HL2M_MAX_MESHES];
    hl2m_mdl_hitboxset hitboxsets[HL2M_MAX_HITBOXSETS];
    hl2m_mdl_hitbox hitboxes[HL2M_MAX_HITBOXES];
    hl2m_mdl_animblock animblocks[HL2M_MAX_MDL_ANIMBLOCKS];
    hl2m_mdl_animdesc anims[HL2M_MAX_MDL_ANIMS];
    hl2m_mdl_seqdesc sequences[HL2M_MAX_MDL_SEQUENCES];
    hl2m_anim_channel anim_channels[HL2M_MAX_ANIM_CHANNELS];
    hl2m_mdl_event events[HL2M_MAX_MDL_EVENTS];
    hl2m_mdl_ikrule ikrules[HL2M_MAX_MDL_IKRULES];
} hl2m_mdl;

typedef struct hl2m_vvd_fixup_s {
    int lod;
    int source_vertex_id;
    int num_vertices;
} hl2m_vvd_fixup;

typedef struct hl2m_vvd_vertex_s {
    hl2m_fixed weight[HL2M_MAX_BONES_PER_VERT];
    int bone[HL2M_MAX_BONES_PER_VERT];
    int numbones;
    hl2m_vec3 position;
    hl2m_vec3 normal;
    hl2m_vec2 texcoord;
} hl2m_vvd_vertex;

typedef struct hl2m_vvd_s {
    int id;
    int version;
    int checksum;
    int num_lods;
    int num_lod_vertices[HL2M_MAX_LODS];
    int source_num_fixups;
    int source_vertex_count;
    int fixup_count;
    int vertex_count;
    int tangent_count;
    hl2m_vvd_fixup fixups[HL2M_MAX_VVD_FIXUPS];
    hl2m_vvd_vertex vertices[HL2M_MAX_VVD_VERTS];
    hl2m_vec4 tangents[HL2M_MAX_VVD_VERTS];
} hl2m_vvd;

typedef struct hl2m_vtx_bodypart_s {
    int first_model;
    int num_models;
} hl2m_vtx_bodypart;

typedef struct hl2m_vtx_model_s {
    int first_lod;
    int num_lods;
} hl2m_vtx_model;

typedef struct hl2m_vtx_lod_s {
    int first_mesh;
    int num_meshes;
    hl2m_fixed switch_point;
} hl2m_vtx_lod;

typedef struct hl2m_vtx_mesh_s {
    int first_stripgroup;
    int num_stripgroups;
    int flags;
} hl2m_vtx_mesh;

typedef struct hl2m_vtx_stripgroup_s {
    int first_vertex;
    int num_vertices;
    int first_index;
    int num_indices;
    int first_strip;
    int num_strips;
    int flags;
} hl2m_vtx_stripgroup;

typedef struct hl2m_vtx_strip_s {
    int first_index;
    int num_indices;
    int first_vertex;
    int num_vertices;
    int num_bones;
    int flags;
    int num_bone_state_changes;
} hl2m_vtx_strip;

typedef struct hl2m_vtx_vertex_s {
    int bone_weight_index[HL2M_MAX_BONES_PER_VERT];
    int numbones;
    int orig_mesh_vert_id;
    int bone_id[HL2M_MAX_BONES_PER_VERT];
} hl2m_vtx_vertex;

typedef struct hl2m_vtx_material_replacement_s {
    int lod;
    int material_id;
    char replacement[HL2M_PATH_MAX];
} hl2m_vtx_material_replacement;

typedef struct hl2m_vtx_s {
    int version;
    int vert_cache_size;
    int max_bones_per_strip;
    int max_bones_per_tri;
    int max_bones_per_vert;
    int checksum;
    int num_lods;
    int source_num_bodyparts;
    int material_replacement_list_offset;
    int material_replacement_count;
    int source_material_replacement_count;
    int bodypart_count;
    int model_count;
    int lod_count;
    int mesh_count;
    int stripgroup_count;
    int strip_count;
    int vertex_count;
    int index_count;
    hl2m_vtx_bodypart bodyparts[HL2M_MAX_VTX_BODYPARTS];
    hl2m_vtx_model models[HL2M_MAX_VTX_MODELS];
    hl2m_vtx_lod lods[HL2M_MAX_VTX_LODS];
    hl2m_vtx_mesh meshes[HL2M_MAX_VTX_MESHES];
    hl2m_vtx_stripgroup stripgroups[HL2M_MAX_VTX_STRIPGROUPS];
    hl2m_vtx_strip strips[HL2M_MAX_VTX_STRIPS];
    hl2m_vtx_vertex vertices[HL2M_MAX_VTX_VERTICES];
    int indices[HL2M_MAX_VTX_INDICES];
    hl2m_vtx_material_replacement material_replacements[HL2M_MAX_VTX_MATERIAL_REPLACEMENTS];
} hl2m_vtx;

typedef struct hl2m_smd_node_s {
    int id;
    int parent;
    char name[HL2M_NAME_MAX];
} hl2m_smd_node;

typedef struct hl2m_smd_pose_s {
    int time;
    int node;
    hl2m_vec3 pos;
    hl2m_vec3 rot;
} hl2m_smd_pose;

typedef struct hl2m_smd_weight_s {
    int bone;
    hl2m_fixed weight;
} hl2m_smd_weight;

typedef struct hl2m_smd_vertex_s {
    int material;
    int parent_bone;
    hl2m_vec3 position;
    hl2m_vec3 normal;
    hl2m_vec2 texcoord;
    int weight_count;
    hl2m_smd_weight weights[HL2M_MAX_BONES_PER_VERT];
} hl2m_smd_vertex;

typedef struct hl2m_smd_triangle_s {
    int material;
    int first_vertex;
} hl2m_smd_triangle;

typedef struct hl2m_smd_s {
    int version;
    int node_count;
    int pose_count;
    int material_count;
    int vertex_count;
    int triangle_count;
    hl2m_smd_node nodes[HL2M_MAX_SMD_NODES];
    hl2m_smd_pose poses[HL2M_MAX_SMD_POSES];
    char materials[HL2M_MAX_SMD_MATERIALS][HL2M_PATH_MAX];
    hl2m_smd_vertex vertices[HL2M_MAX_SMD_VERTICES];
    hl2m_smd_triangle triangles[HL2M_MAX_SMD_TRIANGLES];
} hl2m_smd;

typedef struct hl2m_vta_flex_s {
    int time;
    char name[HL2M_NAME_MAX];
    int first_vertex;
    int num_vertices;
} hl2m_vta_flex;

typedef struct hl2m_vta_vertex_s {
    int flex;
    int source_vertex;
    hl2m_vec3 position;
    hl2m_vec3 normal;
} hl2m_vta_vertex;

typedef struct hl2m_vta_s {
    int version;
    int flex_count;
    int vertex_count;
    hl2m_vta_flex flexes[HL2M_MAX_VTA_FLEXES];
    hl2m_vta_vertex vertices[HL2M_MAX_VTA_VERTICES];
} hl2m_vta;

typedef struct hl2m_vmt_param_s {
    int depth;
    char key[HL2M_NAME_MAX];
    char value[HL2M_VALUE_MAX];
} hl2m_vmt_param;

typedef struct hl2m_vmt_s {
    char shader[HL2M_NAME_MAX];
    int param_count;
    hl2m_vmt_param params[HL2M_MAX_VMT_PARAMS];
} hl2m_vmt;

typedef struct hl2m_phy_solid_s {
    int offset;
    int size;
    int vphysics_id;
    int version;
    int model_type;
    int surface_size;
    hl2m_vec3 drag_axis_areas;
    int axis_map_size;
    int legacy;
    hl2m_vec3 mass_center;
    hl2m_vec3 rotation_inertia;
    hl2m_fixed upper_limit_radius;
    int byte_size;
    int ledge_tree_root_offset;
} hl2m_phy_solid;

typedef struct hl2m_phy_s {
    int header_size;
    int id;
    int source_solid_count;
    int solid_count;
    int checksum;
    int data_offset;
    int keyvalue_offset;
    int keyvalue_size;
    hl2m_phy_solid solids[HL2M_MAX_PHY_SOLIDS];
} hl2m_phy;

typedef struct hl2m_ani_s {
    int id;
    int version;
    int checksum;
    char name[HL2M_NAME_MAX];
    int length;
    int animblock_count;
    int referenced_anim_count;
    int scanned_channel_count;
    int missing_block_count;
    hl2m_mdl_animblock animblocks[HL2M_MAX_MDL_ANIMBLOCKS];
    hl2m_anim_channel anim_channels[HL2M_MAX_ANIM_CHANNELS];
} hl2m_ani;

typedef struct hl2m_qc_command_s {
    int kind;
    int line;
    int depth;
    int argc;
    char name[HL2M_NAME_MAX];
    char args[HL2M_MAX_QC_ARGS][HL2M_VALUE_MAX];
} hl2m_qc_command;

typedef struct hl2m_qc_body_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    char source[HL2M_PATH_MAX];
} hl2m_qc_body;

typedef struct hl2m_qc_sequence_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    char source[HL2M_PATH_MAX];
} hl2m_qc_sequence;

typedef struct hl2m_qc_lod_s {
    int command_index;
    hl2m_fixed switch_point;
    char source[HL2M_PATH_MAX];
} hl2m_qc_lod;

typedef struct hl2m_qc_attachment_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    char bone[HL2M_NAME_MAX];
    hl2m_vec3 pos;
    hl2m_vec3 rot;
    int has_rotate;
} hl2m_qc_attachment;

typedef struct hl2m_qc_hitbox_s {
    int command_index;
    int group;
    char bone[HL2M_NAME_MAX];
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
    char name[HL2M_NAME_MAX];
} hl2m_qc_hitbox;

typedef struct hl2m_qc_definebone_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    char parent[HL2M_NAME_MAX];
    hl2m_vec3 pos;
    hl2m_vec3 rot;
    hl2m_vec3 fixup_pos;
    hl2m_vec3 fixup_rot;
} hl2m_qc_definebone;

typedef struct hl2m_qc_poseparameter_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    hl2m_fixed min_value;
    hl2m_fixed max_value;
    int loop_value;
} hl2m_qc_poseparameter;

typedef struct hl2m_qc_material_replace_s {
    int command_index;
    int lod_depth;
    char from_material[HL2M_PATH_MAX];
    char to_material[HL2M_PATH_MAX];
} hl2m_qc_material_replace;

typedef struct hl2m_qc_collision_s {
    int command_index;
    int joints;
    char source[HL2M_PATH_MAX];
} hl2m_qc_collision;

typedef struct hl2m_qc_flexcontroller_s {
    int command_index;
    char group[HL2M_NAME_MAX];
    char name[HL2M_NAME_MAX];
    hl2m_fixed min_value;
    hl2m_fixed max_value;
} hl2m_qc_flexcontroller;

typedef struct hl2m_qc_flexrule_s {
    int command_index;
    char flex_name[HL2M_NAME_MAX];
    char expression[HL2M_VALUE_MAX];
    int controller_index;
} hl2m_qc_flexrule;

typedef struct hl2m_qc_sequence_event_s {
    int command_index;
    int sequence_index;
    char sequence_name[HL2M_NAME_MAX];
    int event_id;
    int frame;
    hl2m_fixed cycle;
    int type;
    char options[64];
    char event_name[HL2M_NAME_MAX];
} hl2m_qc_sequence_event;

typedef struct hl2m_qc_ikchain_s {
    int command_index;
    char name[HL2M_NAME_MAX];
    char bone[HL2M_NAME_MAX];
    hl2m_vec3 knee;
} hl2m_qc_ikchain;

typedef struct hl2m_qc_ikrule_s {
    int command_index;
    int sequence_index;
    char name[HL2M_NAME_MAX];
    char chain[HL2M_NAME_MAX];
    char bone[HL2M_NAME_MAX];
    char attachment[HL2M_NAME_MAX];
    int type;
    int frame;
    hl2m_fixed start;
    hl2m_fixed peak;
    hl2m_fixed tail;
    hl2m_fixed end;
    hl2m_fixed contact;
    hl2m_fixed drop;
    hl2m_fixed top;
} hl2m_qc_ikrule;


typedef struct hl2m_qc_s {
    int command_count;
    int body_count;
    int sequence_count;
    int material_dir_count;
    int lod_count;
    int attachment_count;
    int qc_hitbox_count;
    int definebone_count;
    int poseparameter_count;
    int material_replace_count;
    int collision_count;
    int flexcontroller_count;
    int flexrule_count;
    int sequence_event_count;
    int ikchain_count;
    int ikrule_count;
    char modelname[HL2M_PATH_MAX];
    char material_dirs[HL2M_MAX_QC_MATERIAL_DIRS][HL2M_PATH_MAX];
    hl2m_qc_body bodies[HL2M_MAX_QC_BODIES];
    hl2m_qc_sequence sequences[HL2M_MAX_QC_SEQUENCES];
    hl2m_qc_lod lods[HL2M_MAX_QC_LODS];
    hl2m_qc_attachment attachments[HL2M_MAX_QC_ATTACHMENTS];
    hl2m_qc_hitbox qc_hitboxes[HL2M_MAX_QC_HITBOXES];
    hl2m_qc_definebone definebones[HL2M_MAX_QC_DEFINEBONES];
    hl2m_qc_poseparameter poseparameters[HL2M_MAX_QC_POSEPARAMS];
    hl2m_qc_material_replace material_replaces[HL2M_MAX_QC_MATERIAL_REPLACES];
    hl2m_qc_collision collisions[HL2M_MAX_QC_COLLISIONS];
    hl2m_qc_flexcontroller flexcontrollers[HL2M_MAX_QC_FLEXCONTROLLERS];
    hl2m_qc_flexrule flexrules[HL2M_MAX_QC_FLEXRULES];
    hl2m_qc_sequence_event sequence_events[HL2M_MAX_QC_SEQUENCE_EVENTS];
    hl2m_qc_ikchain ikchains[HL2M_MAX_QC_IKCHAINS];
    hl2m_qc_ikrule ikrules[HL2M_MAX_QC_IKRULES];
    hl2m_qc_command commands[HL2M_MAX_QC_COMMANDS];
} hl2m_qc;

typedef struct hl2m_material_desc_s {
    char shader[HL2M_NAME_MAX];
    char base_texture[HL2M_PATH_MAX];
    char bumpmap[HL2M_PATH_MAX];
    char surfaceprop[HL2M_NAME_MAX];
    char detail[HL2M_PATH_MAX];
    char envmap[HL2M_PATH_MAX];
    int flags;
} hl2m_material_desc;

typedef struct hl2m_flex_controller_state_s {
    int controller_count;
    char names[HL2M_MAX_QC_FLEXCONTROLLERS][HL2M_NAME_MAX];
    hl2m_fixed values[HL2M_MAX_QC_FLEXCONTROLLERS];
} hl2m_flex_controller_state;


typedef struct hl2m_flex_eval_report_s {
    int flex_count;
    int active_flex_count;
    int rules_evaluated;
    int simple_expression_count;
    int unsupported_expression_count;
    int missing_controller_count;
    int clamped_count;
    int applied_vertex_count;
} hl2m_flex_eval_report;

typedef struct hl2m_sequence_event_out_s {
    int sequence_index;
    char sequence_name[HL2M_NAME_MAX];
    int event_id;
    int frame;
    hl2m_fixed cycle;
    int type;
    char options[64];
    char event_name[HL2M_NAME_MAX];
} hl2m_sequence_event_out;

typedef struct hl2m_ik_pose_s {
    int bone_count;
    char bone_names[HL2M_MAX_BONES][HL2M_NAME_MAX];
    int parent[HL2M_MAX_BONES];
    hl2m_vec3 pos[HL2M_MAX_BONES];
    hl2m_vec3 rot[HL2M_MAX_BONES];
} hl2m_ik_pose;

typedef struct hl2m_ik_report_s {
    int chain_count;
    int rule_count;
    int solved_count;
    int missing_chain_count;
    int missing_bone_count;
    int clamped_count;
    int frame_filtered_count;
} hl2m_ik_report;

typedef struct hl2m_anim_sample_report_s {
    int frame_count;
    int first_frame;
    int last_frame;
    int frame_a;
    int frame_b;
    hl2m_fixed blend;
    int pose_count;
    int interpolated_count;
    int fallback_count;
    int warning_flags;
} hl2m_anim_sample_report;

typedef struct hl2m_mat3x4_s {
    hl2m_fixed m[3][4];
} hl2m_mat3x4;

typedef struct hl2m_pose_matrices_s {
    int bone_count;
    char bone_names[HL2M_MAX_POSE_MATRICES][HL2M_NAME_MAX];
    int parent[HL2M_MAX_POSE_MATRICES];
    hl2m_mat3x4 local[HL2M_MAX_POSE_MATRICES];
    hl2m_mat3x4 world[HL2M_MAX_POSE_MATRICES];
} hl2m_pose_matrices;

typedef struct hl2m_skin_report_s {
    int vertex_count;
    int skinned_count;
    int unweighted_count;
    int missing_bone_count;
    int partial_weight_count;
    int warning_flags;
} hl2m_skin_report;

typedef struct hl2m_event_cursor_s {
    char sequence_name[HL2M_NAME_MAX];
    int previous_frame;
    int current_frame;
    int last_frame;
    int loop_count;
} hl2m_event_cursor;

typedef struct hl2m_lod_select_report_s {
    int selected_lod;
    int replacement_count;
    hl2m_fixed switch_point;
    hl2m_fixed distance;
} hl2m_lod_select_report;

typedef struct hl2m_hitbox_build_report_s {
    int hitbox_count;
    int autogenerated_count;
    int missing_bone_count;
    int clamped_count;
} hl2m_hitbox_build_report;

typedef struct hl2m_attachment_pose_s {
    char name[HL2M_NAME_MAX];
    char bone_name[HL2M_NAME_MAX];
    int bone;
    hl2m_vec3 local_pos;
    hl2m_vec3 world_pos;
    hl2m_mat3x4 world;
} hl2m_attachment_pose;

typedef struct hl2m_attachment_report_s {
    int attachment_count;
    int resolved_count;
    int missing_bone_count;
    int warning_flags;
} hl2m_attachment_report;

typedef struct hl2m_world_hitbox_s {
    char name[HL2M_NAME_MAX];
    int bone;
    int group;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
} hl2m_world_hitbox;

typedef struct hl2m_hitbox_transform_report_s {
    int hitbox_count;
    int transformed_count;
    int missing_bone_count;
    int warning_flags;
} hl2m_hitbox_transform_report;

typedef struct hl2m_draw_filter_report_s {
    int requested_lod;
    int used_lod;
    int kept_draw_count;
    int skipped_draw_count;
    int kept_index_count;
    int warning_flags;
} hl2m_draw_filter_report;

typedef struct hl2m_sequence_runtime_report_s {
    int warning_flags;
    int sampled_bones;
    int skinned_vertices;
    int active_flex_count;
    int event_count;
    int selected_lod;
    int ik_solved_count;
    int attachment_count;
    int hitbox_count;
} hl2m_sequence_runtime_report;

typedef struct hl2m_studiomdl_parity_report_s {
    int feature_flags;
    int missing_flags;
    int warning_flags;
    int qc_command_count;
    int qc_structured_items;
    int binary_table_count;
    int runtime_bridge_count;
    int mdl_hitboxes;
    int mdl_events;
    int mdl_ikrules;
    int vtx_lods;
    int phy_solids;
    int ani_blocks;
} hl2m_studiomdl_parity_report;

typedef struct hl2m_convex_hull_s {
    int bone;
    int material;
    int first_vertex;
    int vertex_count;
    int triangle_count;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
} hl2m_convex_hull;

typedef struct hl2m_convex_decomp_s {
    int hull_count;
    int vertex_count;
    hl2m_convex_hull hulls[HL2M_MAX_CONVEX_HULLS];
    hl2m_vec3 vertices[HL2M_MAX_CONVEX_VERTICES];
} hl2m_convex_decomp;


typedef struct hl2m_engine_vertex_s {
    hl2m_vec3 position;
    hl2m_vec3 normal;
    hl2m_vec2 texcoord;
    hl2m_vec4 tangent;
    int material;
    int bone[HL2M_MAX_BONES_PER_VERT];
    hl2m_fixed weight[HL2M_MAX_BONES_PER_VERT];
    int bone_count;
    int source_vvd_vertex;
    int source_vtx_vertex;
} hl2m_engine_vertex;

typedef struct hl2m_engine_draw_s {
    int material;
    int resolved_material;
    char material_name[HL2M_PATH_MAX];
    char replacement_material[HL2M_PATH_MAX];
    int primitive;
    int first_index;
    int num_indices;
    int first_vertex;
    int num_vertices;
    int bodypart;
    int model;
    int mesh;
    int lod;
} hl2m_engine_draw;

typedef struct hl2m_engine_model_s {
    int vertex_count;
    int index_count;
    int draw_count;
    int skipped_triangle_count;
    int warning_flags;
    hl2m_engine_vertex vertices[HL2M_MAX_ENGINE_VERTICES];
    int indices[HL2M_MAX_ENGINE_INDICES];
    hl2m_engine_draw draws[HL2M_MAX_ENGINE_DRAWS];
} hl2m_engine_model;

typedef struct hl2m_compile_lod_s {
    hl2m_fixed switch_point;
    int triangle_step;
    char replace_from[HL2M_PATH_MAX];
    char replace_to[HL2M_PATH_MAX];
} hl2m_compile_lod;

typedef struct hl2m_compile_options_s {
    int checksum;
    int emit_phy;
    int emit_ani;
    int emit_vta_flexes;
    int normalize_weights;
    int emit_material_replacements;
    int emit_flex_rules;
    int emit_sequence_events;
    int emit_ik_rules;
    int collision_mode;
    int convex_max_hulls;
    int lod_count;
    char model_name[HL2M_PATH_MAX];
    char body_name[HL2M_NAME_MAX];
    char surfaceprop[HL2M_NAME_MAX];
    hl2m_compile_lod lods[HL2M_MAX_COMPILE_LODS];
} hl2m_compile_options;

typedef struct hl2m_compile_result_s {
    int warning_flags;
    int bone_count;
    int material_count;
    int vertex_count;
    int triangle_count;
    int lod_count;
    int flex_count;
    int anim_frame_count;
    int collision_solid_count;
    int hitbox_count;
    int convex_hull_count;
    int flex_controller_count;
    int flex_rule_count;
    int sequence_event_count;
    int ik_chain_count;
    int ik_rule_count;
    int bad_bone_ref_count;
    int weight_repair_count;
    int vta_range_count;
    int qc_body_count;
    int bundle_warning_flags;
    unsigned int mdl_size;
    unsigned int vvd_size;
    unsigned int vtx_size;
    unsigned int phy_size;
    unsigned int ani_size;
} hl2m_compile_result;

typedef struct hl2m_bundle_report_s {
    int warning_flags;
    int mdl_checksum;
    int vvd_checksum;
    int vtx_checksum;
    int phy_checksum;
    int ani_checksum;
    int mdl_version;
    int vvd_version;
    int vtx_version;
    int mdl_declared_length;
    unsigned int mdl_size;
    unsigned int vvd_size;
    unsigned int vtx_size;
    unsigned int phy_size;
    unsigned int ani_size;
} hl2m_bundle_report;


void hl2m_clear_mdl(hl2m_mdl *out_model);
void hl2m_clear_vvd(hl2m_vvd *out_vvd);
void hl2m_clear_vtx(hl2m_vtx *out_vtx);
void hl2m_clear_smd(hl2m_smd *out_smd);
void hl2m_clear_vmt(hl2m_vmt *out_vmt);
void hl2m_clear_phy(hl2m_phy *out_phy);
void hl2m_clear_ani(hl2m_ani *out_ani);
void hl2m_clear_material_desc(hl2m_material_desc *out_desc);
void hl2m_clear_vta(hl2m_vta *out_vta);
void hl2m_clear_flex_controller_state(hl2m_flex_controller_state *out_state);
void hl2m_clear_flex_eval_report(hl2m_flex_eval_report *out_report);
void hl2m_clear_ik_pose(hl2m_ik_pose *out_pose);
void hl2m_clear_ik_report(hl2m_ik_report *out_report);
void hl2m_clear_anim_sample_report(hl2m_anim_sample_report *out_report);
void hl2m_clear_pose_matrices(hl2m_pose_matrices *out_pose);
void hl2m_clear_skin_report(hl2m_skin_report *out_report);
void hl2m_clear_event_cursor(hl2m_event_cursor *out_cursor);
void hl2m_clear_lod_select_report(hl2m_lod_select_report *out_report);
void hl2m_clear_hitbox_build_report(hl2m_hitbox_build_report *out_report);
void hl2m_clear_attachment_report(hl2m_attachment_report *out_report);
void hl2m_clear_hitbox_transform_report(hl2m_hitbox_transform_report *out_report);
void hl2m_clear_draw_filter_report(hl2m_draw_filter_report *out_report);
void hl2m_clear_sequence_runtime_report(hl2m_sequence_runtime_report *out_report);
void hl2m_clear_studiomdl_parity_report(hl2m_studiomdl_parity_report *out_report);
void hl2m_clear_convex_decomp(hl2m_convex_decomp *out_decomp);

void hl2m_clear_qc(hl2m_qc *out_qc);
void hl2m_clear_engine_model(hl2m_engine_model *out_model);

/* Some community assets have lost exactly their first byte (I, 7, or V).
   Repairs only recognized signatures; caller supplies one spare buffer byte. */
int hl2m_repair_missing_first_byte(hl2m_u8 *data, unsigned int *io_size,
                                  unsigned int capacity, int format, int *out_repaired);
int hl2m_decode_mdl(const hl2m_u8 *data, unsigned int size, hl2m_mdl *out_model);
int hl2m_decode_vvd(const hl2m_u8 *data, unsigned int size, int root_lod, hl2m_vvd *out_vvd);
int hl2m_decode_vtx(const hl2m_u8 *data, unsigned int size, hl2m_vtx *out_vtx);
int hl2m_decode_smd(const hl2m_u8 *data, unsigned int size, hl2m_smd *out_smd);
int hl2m_decode_vmt(const hl2m_u8 *data, unsigned int size, hl2m_vmt *out_vmt);
int hl2m_decode_phy(const hl2m_u8 *data, unsigned int size, hl2m_phy *out_phy);
int hl2m_decode_ani(const hl2m_u8 *data, unsigned int size, const hl2m_mdl *mdl, hl2m_ani *out_ani);
int hl2m_decode_vta(const hl2m_u8 *data, unsigned int size, hl2m_vta *out_vta);
int hl2m_vmt_find_param(const hl2m_vmt *vmt, const char *key, char *out_value, unsigned int out_size);
int hl2m_vmt_to_material_desc(const hl2m_vmt *vmt, hl2m_material_desc *out_desc);

/* v1.0.18: independent fixed-point shortest-arc quaternion blending.
   Matrix input must represent a rigid local Source bone transform.
   Interpolates translations linearly; no heap or floating point. */
int hl2m_quaternion_blend_matrix89(const hl2m_mat3x4 *a,
    const hl2m_mat3x4 *b,hl2m_fixed t,hl2m_mat3x4 *out);

/* v1.0.16: Source-inspired sequence runtime (normalized Q16.16 cycle/axes).
   Linear matrix interpolation is orthogonalized, not exact Source slerp;
   pose-parameter normalization/posekeys and virtualmodel overrides are
   caller-owned. These functions never silently apply IK/guard corrections. */
int hl2m_sequence_blend_local89(const hl2m_pose_matrices *a,
    const hl2m_pose_matrices *b,hl2m_fixed t,hl2m_pose_matrices *out);
int hl2m_sequence_sample_cycle89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int animation,hl2m_fixed cycle,int looping,hl2m_pose_matrices *out);
int hl2m_sequence_sample_blend89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed param_x,hl2m_fixed param_y,
    hl2m_pose_matrices *out,int sampled_animations[4]);
int hl2m_sequence_add_delta89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *delta,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,hl2m_pose_matrices *out);

/* Native MDL animation API (mstudioanim_t, integer-only Source RLE). */
typedef struct hl2m_native_anim_report89_s {
    int animation_index;
    int frame;
    int frame_count;
    hl2m_fixed fps;
    int channels;
    int rotation_channels;
    int position_channels;
    int raw_quaternion_channels;
} hl2m_native_anim_report89;
/* Optional software shading parameters for VertexLitGeneric/UnlitGeneric.
   RGB output, alpha-test, detail mix, bump lighting, phong, rim, env mix.
   Inputs are material-space fragment samples supplied by the render backend. */
typedef struct hl2m_material_fx89_s {
    hl2m_material_desc material;
    char phong_exponent_texture[HL2M_PATH_MAX];
    char selfillum_tint[HL2M_VALUE_MAX];
    hl2m_fixed phong_boost, phong_exponent, rim_boost, rim_exponent;
    hl2m_fixed detail_scale, detail_blend, alpha_test_ref, alpha;
    hl2m_fixed env_strength, env_contrast, env_saturation;
    int phong,rimlight,normal_map,normal_dxt5nm,detail_map,environment,detail_mode;
    int half_lambert; /* $halflambert, disabled unless authored */
} hl2m_material_fx89;
typedef struct hl2m_fragment89_s {
    hl2m_vec3 normal,tangent,bitangent,light_dir,view_dir,half_dir;
    hl2m_fixed ambient,light_strength;
    hl2m_u8 base_rgba[4],normal_tex[4],detail_rgba[4],env_rgba[4];
    hl2m_u8 light_rgb[3];
    int have_normal_tex,have_detail_tex,have_env_tex;
    int have_shadow; /* presenter supplies visibility from its shadow/depth pass */
    hl2m_fixed shadow_factor; /* 0 occluded, 65536 fully illuminated */
} hl2m_fragment89;
/* v1.0.9 VTF RGBA + VVD tangent/UV material bridge, caller-owned buffers. */
typedef struct hl2m_texture_view89_s {
    const hl2m_u8 *pixels;
    unsigned int width,height;
    int repeat_u,repeat_v;
} hl2m_texture_view89;
int hl2m_material_vtf_path89(const char *material_name,char *out_path,unsigned int capacity);
int hl2m_fragment_tangent_basis89(hl2m_fragment89 *fragment,hl2m_vec3 normal,hl2m_vec4 tangent);
int hl2m_texture_sample89(const hl2m_texture_view89 *texture,hl2m_fixed u,hl2m_fixed v,hl2m_u8 out_rgba[4]);
/* v1.0.11 optional bilinear filter; original point sampler remains unchanged. */
int hl2m_texture_sample_linear89(const hl2m_texture_view89 *texture,hl2m_fixed u,hl2m_fixed v,hl2m_u8 out_rgba[4]);
int hl2m_fragment_sample_maps_linear89(hl2m_fragment89 *fragment,const hl2m_material_fx89 *material,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail,hl2m_fixed u,hl2m_fixed v);
int hl2m_fragment_sample_maps89(hl2m_fragment89 *fragment,const hl2m_material_fx89 *material,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail,hl2m_fixed u,hl2m_fixed v);
int hl2m_vmt_to_fx89(const hl2m_vmt *vmt,hl2m_material_fx89 *out_fx);
int hl2m_shade_fragment89(const hl2m_material_fx89 *fx,
    const hl2m_fragment89 *fragment,hl2m_u8 out_rgba[4]);

/* Original IEEE-754 scale bits x signed RLE sample -> signed Q16.16. */
hl2m_fixed hl2m_native_scale_rle89(hl2m_u32 ieee32_scale,int sample);
int hl2m_native_bind_pose89(const hl2m_mdl *mdl, hl2m_ik_pose *out_pose);
int hl2m_native_sample_anim_frame89(const hl2m_u8 *mdl_data, unsigned int mdl_size,
    const hl2m_mdl *mdl, int animation_index, int frame,
    hl2m_ik_pose *out_pose, hl2m_native_anim_report89 *out_report);
int hl2m_native_bind_matrices89(const hl2m_mdl *mdl,hl2m_pose_matrices *out_bind);
int hl2m_native_inverse_bind89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_mdl *mdl,hl2m_pose_matrices *out_inverse_bind);
int hl2m_native_sample_anim_matrices89(const hl2m_u8 *mdl_data, unsigned int mdl_size,
    const hl2m_mdl *mdl, int animation_index, int frame,
    hl2m_pose_matrices *out_world, hl2m_native_anim_report89 *out_report);
/* Embedded MDL sections and optional external .ani blocks, caller-owned. */
/* Optional diagnostic fallback, explicitly NOT exact Source playback.
   outlier criterion: abs(local_position - reference_local_position) > max_delta.
   Only rejected translation axes revert to the base/DELTA reference pose.
   Caller reads replaced_axes and replaced_bones before trusting the preview. */
typedef struct hl2m_anim_safety_report89_s {
    int replaced_axes;
    int replaced_bones;
    int first_bone;
} hl2m_anim_safety_report89;
int hl2m_native_sample_anim_matrices_guard89(const hl2m_u8 *mdl_data,
    unsigned int mdl_size,const hl2m_u8 *ani_bytes,unsigned int ani_size,
    const hl2m_mdl *mdl,int anim,int frame,hl2m_fixed max_delta,
    hl2m_pose_matrices *pose,hl2m_native_anim_report89 *report,
    hl2m_anim_safety_report89 *safety);
int hl2m_native_sample_anim_matrices_external89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_u8 *ani_data,unsigned int ani_size,const hl2m_mdl *mdl,
    int anim,int frame,hl2m_pose_matrices *out_world,hl2m_native_anim_report89 *report);
int hl2m_native_sequence_animation89(const hl2m_u8 *mdl_data, unsigned int size,
    const hl2m_mdl *mdl, int sequence_index, int blend_x, int blend_y, int *out_animation_index);

int hl2m_check_mdl_vvd_vtx_sync(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx);

int hl2m_decode_qc(const hl2m_u8 *data, unsigned int size, hl2m_qc *out_qc);
int hl2m_smd_write(const hl2m_smd *smd, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_make_basic_qc(const hl2m_smd *smd, const char *modelname, const char *body_name, const char *smd_path, const char *cdmaterials, int staticprop, hl2m_qc *out_qc);
int hl2m_qc_write(const hl2m_qc *qc, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_decompile_mdl_to_qc(const hl2m_mdl *mdl, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_build_engine_model(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int lod, hl2m_engine_model *out_model);
int hl2m_vtx_find_material_replacement(const hl2m_vtx *vtx, int lod, int material_id, char *out_name, unsigned int out_size);
int hl2m_engine_assign_material_names(const hl2m_mdl *mdl, const hl2m_vtx *vtx, int lod, hl2m_engine_model *model);
int hl2m_build_engine_model_from_smd(const hl2m_smd *smd, hl2m_engine_model *out_model);
int hl2m_engine_model_bounds(const hl2m_engine_model *model, hl2m_vec3 *out_min, hl2m_vec3 *out_max);
int hl2m_engine_model_validate(const hl2m_engine_model *model);
int hl2m_engine_model_write_obj(const hl2m_engine_model *model, const char *object_name, char *out_text, unsigned int out_size, unsigned int *out_used);

void hl2m_clear_compile_options(hl2m_compile_options *out_options);
void hl2m_clear_compile_result(hl2m_compile_result *out_result);
void hl2m_clear_bundle_report(hl2m_bundle_report *out_report);
int hl2m_make_compile_options_from_qc(const hl2m_qc *qc, hl2m_compile_options *out_options);
int hl2m_validate_compile_inputs(const hl2m_qc *qc, const hl2m_smd *reference_smd, const hl2m_smd *animation_smd, const hl2m_vta *vta, hl2m_compile_result *out_result);
int hl2m_validate_binary_bundle(const hl2m_u8 *mdl_data, unsigned int mdl_size, const hl2m_u8 *vvd_data, unsigned int vvd_size, const hl2m_u8 *vtx_data, unsigned int vtx_size, const hl2m_u8 *phy_data, unsigned int phy_size, const hl2m_u8 *ani_data, unsigned int ani_size, hl2m_bundle_report *out_report);
int hl2m_vta_write(const hl2m_vta *vta, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_engine_apply_vta_flex(const hl2m_engine_model *base_model, const hl2m_vta *vta, int flex_index, hl2m_fixed weight, hl2m_engine_model *out_model);
int hl2m_flex_state_from_qc(const hl2m_qc *qc, hl2m_flex_controller_state *out_state);
int hl2m_flex_state_set(hl2m_flex_controller_state *state, const char *name, hl2m_fixed value);
int hl2m_flex_state_clamp(const hl2m_qc *qc, hl2m_flex_controller_state *state, hl2m_flex_eval_report *out_report);
int hl2m_flex_eval_weights(const hl2m_qc *qc, const hl2m_vta *vta, const hl2m_flex_controller_state *state, hl2m_fixed out_weights[HL2M_MAX_VTA_FLEXES], hl2m_flex_eval_report *out_report);
int hl2m_engine_apply_flex_weights(const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_fixed weights[HL2M_MAX_VTA_FLEXES], hl2m_engine_model *out_model, hl2m_flex_eval_report *out_report);
int hl2m_engine_apply_flex_controllers(const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_qc *qc, const hl2m_flex_controller_state *state, hl2m_engine_model *out_model);
int hl2m_smd_frame_range(const hl2m_smd *smd, int *out_first_frame, int *out_last_frame, int *out_frame_count);
int hl2m_sample_smd_pose_lerp(const hl2m_smd *smd, int frame_a, int frame_b, hl2m_fixed blend, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report);
int hl2m_sample_smd_pose_time(const hl2m_smd *smd, hl2m_fixed frame_time, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report);
int hl2m_pose_matrices_from_pose(const hl2m_ik_pose *pose, int include_rotation, hl2m_pose_matrices *out_matrices);
int hl2m_pose_inverse_bind_from_pose(const hl2m_ik_pose *bind_pose, int include_rotation, hl2m_pose_matrices *out_inverse_bind);
int hl2m_blend_poses(const hl2m_ik_pose *pose_a, const hl2m_ik_pose *pose_b, hl2m_fixed blend, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report);
int hl2m_apply_additive_pose(const hl2m_ik_pose *base_pose, const hl2m_ik_pose *reference_pose, const hl2m_ik_pose *add_pose, hl2m_fixed weight, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report);
int hl2m_engine_cpu_skin(const hl2m_engine_model *base_model, const hl2m_pose_matrices *inverse_bind, const hl2m_pose_matrices *pose_world, hl2m_engine_model *out_model, hl2m_skin_report *out_report);
int hl2m_event_cursor_init(const char *sequence_name, int start_frame, int last_frame, hl2m_event_cursor *out_cursor);
int hl2m_event_cursor_advance(const hl2m_qc *qc, hl2m_event_cursor *cursor, int next_frame, hl2m_sequence_event_out *out_events, int out_cap, int *out_count);
int hl2m_select_lod_from_distance(const hl2m_compile_options *opt, hl2m_fixed distance, int *out_lod, hl2m_lod_select_report *out_report);
int hl2m_engine_filter_draws_for_lod(const hl2m_engine_model *model, int lod, hl2m_engine_model *out_model, hl2m_draw_filter_report *out_report);
int hl2m_build_attachment_poses_from_qc(const hl2m_qc *qc, const hl2m_pose_matrices *pose_world, hl2m_attachment_pose *out_attachments, int out_cap, int *out_count, hl2m_attachment_report *out_report);
int hl2m_transform_hitboxes_to_world(const hl2m_mdl_hitbox *hitboxes, int hitbox_count, const hl2m_pose_matrices *pose_world, hl2m_world_hitbox *out_hitboxes, int out_cap, int *out_count, hl2m_hitbox_transform_report *out_report);
int hl2m_runtime_eval_sequence(const hl2m_qc *qc, const hl2m_smd *bind_smd, const hl2m_smd *anim_smd, const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_flex_controller_state *flex_state, const hl2m_compile_options *opt, const char *sequence_name, hl2m_fixed frame_time, hl2m_fixed distance, int include_rotation, hl2m_engine_model *out_model, hl2m_sequence_event_out *out_events, int event_cap, int *out_event_count, hl2m_sequence_runtime_report *out_report);
int hl2m_build_hitboxes_from_qc_smd(const hl2m_qc *qc, const hl2m_smd *smd, hl2m_mdl_hitbox *out_hitboxes, int out_cap, int *out_count, hl2m_hitbox_build_report *out_report);
int hl2m_collect_qc_sequence_events(const hl2m_qc *qc, const char *sequence_name, int first_frame, int last_frame, hl2m_sequence_event_out *out_events, int out_cap, int *out_count);
int hl2m_ik_pose_from_smd_frame(const hl2m_smd *smd, int frame, hl2m_ik_pose *out_pose);
int hl2m_solve_ik_rules(const hl2m_qc *qc, const hl2m_ik_pose *in_pose, int frame, hl2m_ik_pose *out_pose, hl2m_ik_report *out_report);
int hl2m_build_convex_decomp_from_smd(const hl2m_smd *smd, int mode, hl2m_convex_decomp *out_decomp);
int hl2m_convex_decomp_split_long_axes(const hl2m_convex_decomp *in_decomp, int max_depth, hl2m_convex_decomp *out_decomp);
int hl2m_build_studiomdl_parity_report(const hl2m_qc *qc, const hl2m_smd *smd, const hl2m_vta *vta, const hl2m_mdl *mdl, const hl2m_vtx *vtx, const hl2m_phy *phy, const hl2m_ani *ani, hl2m_studiomdl_parity_report *out_report);
int hl2m_write_phy_from_convex_decomp(const hl2m_convex_decomp *decomp, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_write_vvd_from_smd(const hl2m_smd *smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_write_vtx_from_smd(const hl2m_smd *smd, const hl2m_qc *qc, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_write_mdl_from_qc_smd(const hl2m_qc *qc, const hl2m_smd *smd, const hl2m_smd *anim_smd, const hl2m_vta *vta, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_write_phy_from_smd(const hl2m_smd *smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_write_ani_from_smd(const hl2m_smd *anim_smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size);
int hl2m_compile_qc_smd_to_binary(const hl2m_qc *qc, const hl2m_smd *reference_smd, const hl2m_smd *animation_smd, const hl2m_vta *vta, const hl2m_compile_options *opt, hl2m_u8 *mdl_data, unsigned int mdl_cap, unsigned int *mdl_size, hl2m_u8 *vvd_data, unsigned int vvd_cap, unsigned int *vvd_size, hl2m_u8 *vtx_data, unsigned int vtx_cap, unsigned int *vtx_size, hl2m_u8 *phy_data, unsigned int phy_cap, unsigned int *phy_size, hl2m_u8 *ani_data, unsigned int ani_cap, unsigned int *ani_size, hl2m_compile_result *out_result);
int hl2m_guess_format(const hl2m_u8 *data, unsigned int size, const char *path);
const char *hl2m_error_name(int code);

/* Source VTF 7.x textures, native BC1/DXT1 and BC3/DXT5 (caller-owned RGBA). */
#define HL2M_VTF_ABGR8888 1
#define HL2M_VTF_BGR888 3
#define HL2M_VTF_BGRA8888 12
#define HL2M_VTF_DXT1 13
#define HL2M_VTF_DXT5 15
typedef struct hl2m_vtf_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int mip_count;
    unsigned int image_format;
    unsigned int data_offset; /* Highest-resolution image, first face/frame. */
    unsigned int data_size;
    unsigned int major_version;
    unsigned int minor_version;
} hl2m_vtf_info;
int hl2m_vtf_read_info(const hl2m_u8 *data, unsigned int size, hl2m_vtf_info *out_info);
int hl2m_vtf_decode_rgba(const hl2m_u8 *data, unsigned int size,
                         const hl2m_vtf_info *info, hl2m_u8 *out_rgba,
                         unsigned int out_capacity);

hl2m_fixed hl2m_fixed_from_int(int value);
int hl2m_fixed_to_int(hl2m_fixed value);
void hl2m_fixed_to_string(hl2m_fixed value, char *out_text, unsigned int out_size, int frac_digits);

/* Addition in v1.0.4: sample per-animation entity motion from MDL.
   For animation rendering, translate the entity by out_position and rotate
   it about Source yaw axis by out_yaw. This is NOT an IK implementation. */
int hl2m_native_root_motion89(const hl2m_u8 *data,unsigned int size,
                              const hl2m_mdl *mdl,int anim,int frame,
                              hl2m_vec3 *out_position,hl2m_fixed *out_yaw);


/* v1.0.21: Source SDK Studio normalized pose controllers. MDL-local only;
   external virtual-model pose remapping needs a separate resolver. */
#define HL2M_POSEPARAM_CAP121 128

typedef struct hl2m_poseparam121_s {
    char name[HL2M_NAME_MAX];
    int flags;
    hl2m_fixed start,end,loop;
} hl2m_poseparam121;
int hl2m_poseparams_read121(const hl2m_u8 *mdl_bytes,unsigned int size,
    hl2m_poseparam121 *out,unsigned int capacity,unsigned int *count);
int hl2m_pose_control_from_physical121(const hl2m_poseparam121 *desc,
    hl2m_fixed physical,hl2m_fixed *normalized);
int hl2m_pose_physical_from_control121(const hl2m_poseparam121 *desc,
    hl2m_fixed normalized,hl2m_fixed *physical);
int hl2m_pose_sequence_axes121(const hl2m_mdl *mdl,int sequence,
    const hl2m_poseparam121 *descs,unsigned int desc_count,
    const hl2m_fixed *normalized,unsigned int normalized_count,
    hl2m_fixed *physical_x,hl2m_fixed *physical_y);

/* v1.0.22: Source virtualmodel_t catalog for ONE included model group.
   The caller owns both MDL buffers and explicitly supplies include path.
   This does not load files or implement Source's complete virtual runtime. */
#define HL2M_VIRTUAL_INCLUDE_CAP122 64
#define HL2M_VIRTUAL_SEQ_CAP122 (HL2M_MAX_MDL_SEQUENCES*2)
#define HL2M_VIRTUAL_ANIM_CAP122 (HL2M_MAX_MDL_ANIMS*2)
#define HL2M_VIRTUAL_POSE_CAP122 (HL2M_POSEPARAM_CAP121*2)
typedef struct hl2m_virtual_ref122_s { int group,local; } hl2m_virtual_ref122;
typedef struct hl2m_virtual_link122_s {
    int base_bones,child_bones,mapped_bones,unmapped_child_bones,parent_conflicts;
    int child_to_base_bone[HL2M_MAX_BONES],base_to_child_bone[HL2M_MAX_BONES];
    int seq_count,anim_count,pose_count,base_pose_count,child_pose_count;
    hl2m_virtual_ref122 sequences[HL2M_VIRTUAL_SEQ_CAP122];
    hl2m_virtual_ref122 animations[HL2M_VIRTUAL_ANIM_CAP122];
    hl2m_virtual_ref122 poses[HL2M_VIRTUAL_POSE_CAP122];
    int child_seq_to_virtual[HL2M_MAX_MDL_SEQUENCES];
    int child_anim_to_virtual[HL2M_MAX_MDL_ANIMS];
    int child_pose_to_virtual[HL2M_POSEPARAM_CAP121];
    char pose_names[HL2M_VIRTUAL_POSE_CAP122][HL2M_NAME_MAX];
    hl2m_fixed pose_start[HL2M_VIRTUAL_POSE_CAP122];
    hl2m_fixed pose_end[HL2M_VIRTUAL_POSE_CAP122];
    hl2m_fixed pose_loop[HL2M_VIRTUAL_POSE_CAP122];
} hl2m_virtual_link122;
int hl2m_virtual_includes122(const hl2m_u8 *data,unsigned int size,
    char names[][HL2M_PATH_MAX],unsigned int cap,unsigned int *count);
int hl2m_virtual_build122(const hl2m_u8 *base_bytes,unsigned int base_size,
    const hl2m_mdl *base,const char *include_path,
    const hl2m_u8 *child_bytes,unsigned int child_size,
    const hl2m_mdl *child,hl2m_virtual_link122 *out);
int hl2m_virtual_base_controls122(const hl2m_virtual_link122 *link,
    const hl2m_u8 *base_bytes,unsigned int base_size,
    const hl2m_fixed *virtual_normalized,unsigned int count,
    hl2m_fixed *local_normalized,unsigned int capacity);
int hl2m_virtual_child_controls122(const hl2m_virtual_link122 *link,
    const hl2m_u8 *child_bytes,unsigned int child_size,
    const hl2m_fixed *virtual_normalized,unsigned int count,
    hl2m_fixed *local_normalized,unsigned int capacity);
int hl2m_virtual_remap_pose122(const hl2m_mdl *base,
    const hl2m_virtual_link122 *link,const hl2m_pose_matrices *child_pose,
    hl2m_pose_matrices *out);


/* v1.0.23: Multi-group, caller-owned Source virtual-model resolver.
   Max 4 total MDL groups: base + up to 3 declared $includemodel entries.
   Input buffers/models/ANI MUST outlive the resolved catalog. No filesystem,
   heap allocation, float/double or 64-bit integers. No cross-group autolayers
   or Source IK/SRC-bone-transform runtime yet. */
#define HL2M_VIRTUAL_GROUP_CAP123 4
#define HL2M_VIRTUAL_SEQ_CAP123 (HL2M_MAX_MDL_SEQUENCES*HL2M_VIRTUAL_GROUP_CAP123)
#define HL2M_VIRTUAL_ANIM_CAP123 (HL2M_MAX_MDL_ANIMS*HL2M_VIRTUAL_GROUP_CAP123)
#define HL2M_VIRTUAL_POSE_CAP123 (HL2M_POSEPARAM_CAP121*HL2M_VIRTUAL_GROUP_CAP123)
typedef struct hl2m_virtual_input123_s {
    const char *include_path; /* NULL for base group 0; exact declared path otherwise */
    const hl2m_u8 *mdl_bytes;
    unsigned int mdl_size;
    const hl2m_u8 *ani_bytes; /* optional external ANI */
    unsigned int ani_size;
    const hl2m_mdl *mdl;
} hl2m_virtual_input123;
typedef struct hl2m_virtual_group123_s {
    hl2m_virtual_input123 source;
    int mapped_bones,unmapped_bones,parent_conflicts;
    int bone_to_base[HL2M_MAX_BONES];
    int base_to_bone[HL2M_MAX_BONES];
    int seq_to_virtual[HL2M_MAX_MDL_SEQUENCES];
    int anim_to_virtual[HL2M_MAX_MDL_ANIMS];
    int pose_to_virtual[HL2M_POSEPARAM_CAP121];
    int pose_count;
} hl2m_virtual_group123;
typedef struct hl2m_virtual_multi123_s {
    int group_count,seq_count,anim_count,pose_count;
    hl2m_virtual_group123 groups[HL2M_VIRTUAL_GROUP_CAP123];
    hl2m_virtual_ref122 sequences[HL2M_VIRTUAL_SEQ_CAP123];
    hl2m_virtual_ref122 animations[HL2M_VIRTUAL_ANIM_CAP123];
    char pose_names[HL2M_VIRTUAL_POSE_CAP123][HL2M_NAME_MAX];
    hl2m_fixed pose_start[HL2M_VIRTUAL_POSE_CAP123];
    hl2m_fixed pose_end[HL2M_VIRTUAL_POSE_CAP123];
    hl2m_fixed pose_loop[HL2M_VIRTUAL_POSE_CAP123];
} hl2m_virtual_multi123;
int hl2m_virtual_build_multi123(const hl2m_virtual_input123 *inputs,
    unsigned int group_count,hl2m_virtual_multi123 *out);
/* v1.0.24 Source recursive include resolver. All MDL/ANI bytes must be
   provided by the caller. The base is sources[0], other entries are named
   by include_path and may be in arbitrary input order. DFS yields the group
   ordering used for Source-style virtual sequence indexes. Missing, unused,
   cyclic, duplicate or overflowing graphs fail transactionally.
   Supports up to 4 unique MDLs including the root, like the v123 catalog. */
typedef struct hl2m_virtual_graph124_s {
    int group_count;
    int group_to_input[HL2M_VIRTUAL_GROUP_CAP123];
    int input_to_group[HL2M_VIRTUAL_GROUP_CAP123];
    int parent_group[HL2M_VIRTUAL_GROUP_CAP123];
} hl2m_virtual_graph124;
int hl2m_virtual_build_recursive124(const hl2m_virtual_input123 *sources,
    unsigned int count,hl2m_virtual_multi123 *out,
    hl2m_virtual_graph124 *graph);
/* Resolve Source iRelativeSeq: autolayer target from parent group LOCAL
   sequence index to virtual sequence index, including STUDIO_OVERRIDE. */
int hl2m_virtual_layer_target124(const hl2m_virtual_multi123 *catalog,
    int parent_virtual_seq,int layer_local_seq,int *target_virtual_seq);

int hl2m_virtual_controls_multi123(const hl2m_virtual_multi123 *catalog,
    int group,const hl2m_fixed *global_controls,unsigned int global_count,
    hl2m_fixed *local_controls,unsigned int local_cap);
int hl2m_virtual_remap_multi123(const hl2m_virtual_multi123 *catalog,
    int group,const hl2m_pose_matrices *child_pose,hl2m_pose_matrices *out);




/* Source 2013 Studio sequence metadata, decoded without floating point.
   Native MDL seqdesc offsets, checked for each read. */
#define HL2M_SEQ_MAX_LAYERS89 32
#define HL2M_SEQ_DELTA89 0x0004
#define HL2M_SEQ_POST89 0x0010
#define HL2M_SEQ_ALLZEROS89 0x0020
#define HL2M_SEQ_LAYER_SPLINE89 0x0040
#define HL2M_SEQ_LAYER_XFADE89 0x0080
#define HL2M_SEQ_LAYER_NOBLEND89 0x0200
#define HL2M_SEQ_LAYER_LOCAL89 0x1000
#define HL2M_SEQ_LAYER_POSE89 0x4000
typedef struct hl2m_seq_layer89_s {
    int sequence;
    int pose_index;
    int flags;
    hl2m_fixed start,peak,tail,end;
} hl2m_seq_layer89;
typedef struct hl2m_seq_metadata89_s {
    int sequence;
    int flags;
    int bone_count;
    int layer_count;
    int has_weightlist;
    int has_posekeys;
    hl2m_fixed bone_weights[HL2M_MAX_BONES];
    hl2m_seq_layer89 layers[HL2M_SEQ_MAX_LAYERS89];
    hl2m_fixed posekeys[2][64];
} hl2m_seq_metadata89;
int hl2m_sequence_metadata89(const hl2m_u8 *mdl_bytes,unsigned int size,
    const hl2m_mdl *mdl,int sequence,hl2m_seq_metadata89 *out);
/* Physical Source pose parameter -> normalized sequence grid value.
   If the sequence has pose keys, compares against those physical keys. */
int hl2m_sequence_pose_axis89(const hl2m_mdl_seqdesc *seq,
    const hl2m_seq_metadata89 *meta,int axis,hl2m_fixed physical,
    hl2m_fixed *normalized);
/* Return layer cycle and envelope weight, as specified by Source's
   AddSequenceLayers() for a cycle-driven layer. No automatic scene state. */
int hl2m_sequence_layer_envelope89(const hl2m_seq_layer89 *layer,
    hl2m_fixed base_cycle,hl2m_fixed base_weight,
    hl2m_fixed *layer_cycle,hl2m_fixed *layer_weight);
/* SDK parity extension: use an explicit physical pose value for
   STUDIO_AL_POSE. For those layers the child animation keeps base_cycle. */
int hl2m_sequence_layer_envelope_control120(const hl2m_seq_layer89 *layer,
    hl2m_fixed base_cycle,hl2m_fixed base_weight,hl2m_fixed pose_physical,
    hl2m_fixed *layer_cycle,hl2m_fixed *layer_weight);
/* Bone masks and STUDIO_DELTA/STUDIO_POST semantics; caller must supply
   identity-relative matrices for delta sequences, NOT bind-space matrices. */
int hl2m_sequence_blend_mask89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *overlay,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,int flags,hl2m_pose_matrices *out);

#ifdef __cplusplus
}
#endif

#endif
