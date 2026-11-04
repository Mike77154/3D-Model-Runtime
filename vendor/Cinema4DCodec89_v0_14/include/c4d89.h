/* Cinema4DCodec89 direct-native mesh decoder v0.3, public domain (CC0). */
#ifndef C4D89_H
#define C4D89_H
#include <stdio.h>

#define C4D89_MAX_IMAGES 256u
#define C4D89_MAX_NAMES 1024u
#define C4D89_MAX_NAME 128u
#define C4D89_OK 0
#define C4D89_ERR_IO 1
#define C4D89_ERR_SIGNATURE 2
#define C4D89_ERR_LIMIT 3
#define C4D89_ERR_UNSUPPORTED 4
#define C4D89_ERR_CORRUPT 5

typedef struct C4D89_Image {
    unsigned long offset;  /* physical offset inside native .c4d */
    unsigned long length;  /* JPEG byte length, SOI through EOI */
    unsigned int width;
    unsigned int height;
    unsigned int components;  /* as declared by JPEG SOF */
} C4D89_Image;

typedef struct C4D89_Name {
    char text[C4D89_MAX_NAME];
    unsigned long offset; /* position of string marker */
    unsigned int is_texture_reference;
} C4D89_Name;

typedef struct C4D89_Report {
    unsigned int signature; /* 1=XC4DC4D6; 2=QC4DC4D6 */
    unsigned long file_size;
    unsigned int image_count;
    unsigned int name_count;
    unsigned int names_over_limit;
    unsigned int images_over_limit;
    C4D89_Image images[C4D89_MAX_IMAGES];
    C4D89_Name names[C4D89_MAX_NAMES];
} C4D89_Report;

/* Native .c4d; zero allocations, no SDK runtime, no intermediary format. */
int c4d89_probe(const char *path);  /* 1=X, 2=Q, 0=not recognized */
int c4d89_scan(const char *path, C4D89_Report *report);
int c4d89_extract_image(const char *c4d_path,
                        const C4D89_Image *image, const char *jpeg_path);
/* Stream an embedded JPEG directly into a game-engine decoder; no tempfile. */
typedef int (*C4D89_ImageBeginFn)(void *user, const C4D89_Image *image);
typedef int (*C4D89_ImageChunkFn)(void *user, const unsigned char *data, unsigned int count);
typedef int (*C4D89_ImageEndFn)(void *user);
typedef struct C4D89_ImageSink {
    C4D89_ImageBeginFn begin;
    C4D89_ImageChunkFn chunk;
    C4D89_ImageEndFn end;
} C4D89_ImageSink;
int c4d89_stream_image(const char *path,const C4D89_Image *image,
                       const C4D89_ImageSink *sink,void *user);
/* Native geometry now decoded: pass a non-NULL C4D89_MeshReceiver. */
int c4d89_load_mesh(const char *path, void *engine_scene);
const char *c4d89_error(int status);

/* Native geometry decoder, discovered and validated against XC4DC4D6 scenes. */
#define C4D89_MAX_MESHES 64u
#define C4D89_MAX_MESH_VERTICES 65535ul
#define C4D89_MAX_MESH_FACES 131072ul
#define C4D89_ERR_SINK 6

typedef struct C4D89_MeshDesc {
    unsigned long uv_offset;       /* first big-endian float32 UVW */
    unsigned long face_offset;     /* first 4-byte control field */
    unsigned long vertex_offset;   /* first big-endian float32 XYZ */
    unsigned long uv_count;        /* four corners per polygon */
    unsigned long face_count;
    unsigned long vertex_count;
    unsigned long triangle_count;
    unsigned long quad_count;
} C4D89_MeshDesc;
typedef struct C4D89_MeshTable {
    unsigned int count;
    C4D89_MeshDesc meshes[C4D89_MAX_MESHES];
} C4D89_MeshTable;
typedef struct C4D89_Face {
    unsigned int a,b,c,d;
    unsigned int corners;          /* 3 for triangle; 4 for quad */
} C4D89_Face;
typedef struct C4D89_MeshSink {
    int (*begin)(void*, unsigned int, const C4D89_MeshDesc*);
    int (*vertex)(void*, unsigned long, long, long, long); /* Q16.16 XYZ */
    int (*uv)(void*, unsigned long, long, long);          /* Q16.16 UV */
    int (*face)(void*, unsigned long, const C4D89_Face*);
    int (*end)(void*, unsigned int);
} C4D89_MeshSink;
typedef struct C4D89_MeshReceiver {
    C4D89_MeshSink sink;
    void *user;
} C4D89_MeshReceiver;
/* Not a preview scanner: these APIs read geometric data directly from .c4d. */
int c4d89_find_meshes(const char *path, C4D89_MeshTable *table);
int c4d89_stream_mesh(const char *path, unsigned int mesh_number,
    const C4D89_MeshDesc *mesh, const C4D89_MeshSink *sink, void *user);
int c4d89_stream_scene(const char *path, const C4D89_MeshSink *sink, void *user);
int c4d89_export_obj(const char *c4d_path, const char *obj_path);
int c4d89_export_obj_groups(const char *c4d_path, const char *obj_path);
/* Decode raw big-endian IEEE 754 binary32 -> Q16.16, no float/double. */
int c4d89_be_float_to_q16(const unsigned char *bytes, long *out);

/* Native resources, candidate texture associations, joint inventory,
   and independently verified scalar keyframe records, XC4DC4D6. */
#define C4D89_MAX_TEXTURES 256u
#define C4D89_MAX_JOINT_HINTS 256u
#define C4D89_MAX_TRACKS 8192u
#define C4D89_CHANNEL_NAME 24u

typedef struct C4D89_TextureHint {
    char path[C4D89_MAX_NAME];
    unsigned long string_offset;
    unsigned int jpeg_count;
    unsigned int best_jpeg_index;
    unsigned int best_width, best_height;
} C4D89_TextureHint;
typedef struct C4D89_TextureCatalog {
    unsigned int count;
    unsigned int unassigned_jpegs;
    C4D89_TextureHint textures[C4D89_MAX_TEXTURES];
} C4D89_TextureCatalog;
/* NOTE: these are texture-to-embedded-JPEG candidates, not material bindings. */
int c4d89_texture_hints(const C4D89_Report *report, C4D89_TextureCatalog *catalog);

/* Native bitmap-path strings in Mmaterial/Xbitmap parameters. Identifies
   bitmap resources in material records; DOES NOT establish TextureTag material
   links or resolve the material channel (color/normal/bump). */
#define C4D89_MAX_BITMAP_PATHS 256u
typedef struct C4D89_BitmapPath {
    char name[C4D89_MAX_NAME];
    unsigned long material_offset,filename_offset;
    unsigned int parameter_id;
    unsigned int jpeg_count,best_jpeg_index; /* proximity-based embedded JPEG candidates */
} C4D89_BitmapPath;
typedef struct C4D89_BitmapCatalog {
    unsigned int count,omitted;
    C4D89_BitmapPath paths[C4D89_MAX_BITMAP_PATHS];
} C4D89_BitmapCatalog;
int c4d89_scan_material_bitmaps(const char *path,C4D89_BitmapCatalog *out);
int c4d89_attach_material_jpegs(const C4D89_Report *report,C4D89_BitmapCatalog *catalog);

typedef struct C4D89_JointHint {
    char name[C4D89_MAX_NAME];
    unsigned long name_offset;
} C4D89_JointHint;
typedef struct C4D89_RigEvidence {
    unsigned int joint_type_strings;
    unsigned int skin_type_strings;
    unsigned int weight_type_strings;
    unsigned int candidate_joint_count;
    unsigned int omitted_joint_hints;
    C4D89_JointHint joint_hints[C4D89_MAX_JOINT_HINTS];
} C4D89_RigEvidence;
/* A name inventory. Does NOT deliver hierarchy, weights or inverse bind matrices. */
int c4d89_rig_evidence(const C4D89_Report *report, C4D89_RigEvidence *rig);

typedef struct C4D89_Key {
    unsigned long file_offset;     /* start of native key record */
    long time_q16;                 /* seconds, fixed point */
    long value_q16;                /* parameter units, fixed point */
    long time_numerator_q16;       /* original rational time */
    long time_denominator_q16;
} C4D89_Key;
typedef struct C4D89_AnimTrack {
    char channel[C4D89_CHANNEL_NAME];
    unsigned long name_offset;
    char owner_hint[C4D89_MAX_NAME]; /* adjacent Joint record, not verified parent */
    unsigned long owner_name_offset;
    unsigned long owner_joint_offset;
    unsigned int owner_confidence; /* 0 unknown; 1 name adjacent to Joint */
    unsigned long data_start;
    unsigned long data_end;
    unsigned long first_key_offset;
    unsigned long key_count;
    unsigned int key_stride;       /* observed common stride, or 0 */
    long start_time_q16, end_time_q16;
    long first_value_q16, last_value_q16;
} C4D89_AnimTrack;
typedef struct C4D89_AnimIndex {
    unsigned int count;
    unsigned int omitted_tracks;
    unsigned long decoded_keys;
    C4D89_AnimTrack tracks[C4D89_MAX_TRACKS];
} C4D89_AnimIndex;
typedef int (*C4D89_KeyFn)(void *user,unsigned long key_index,
                            const C4D89_Key *key);
/* One index per scene, no dynamic allocation. Repeated channel names are kept. */
int c4d89_scan_animation(const char *path,C4D89_AnimIndex *index);
int c4d89_stream_track(const char *path,const C4D89_AnimTrack *track,
                       C4D89_KeyFn callback,void *user);
/* Fast indexed Q16.16 linear interpolation on fixed-stride native tracks.
   Clamps endpoints. No hierarchy, tangents or skinning inference. */
int c4d89_sample_track_linear_file(FILE *file,const C4D89_AnimTrack *track,
                               long seconds_q16,long *value_q16);
int c4d89_sample_track_linear(const char *path,const C4D89_AnimTrack *track,
                               long seconds_q16,long *value_q16);

/* Native Joint records: their 4x3 matrix is OBSERVED generic object matrix.
   It is NOT proven to be skin inverse-bind or even the final rest transform. */
#define C4D89_MAX_JOINT_RECORDS 512u
#define C4D89_QONE 65536L
#define C4D89_SKIN_MAX_INFLUENCES 4u
typedef struct C4D89_JointRecord {
    char name[C4D89_MAX_NAME];
    unsigned long name_offset;
    unsigned long joint_offset;
    unsigned long matrix_offset;
    long matrix_q16[12]; /* C4D offset, X, Y, Z, each xyz (Q16.16) */
    unsigned int has_matrix;
} C4D89_JointRecord;
typedef struct C4D89_JointTable {
    unsigned int count;
    unsigned int omitted;
    unsigned int with_matrix;
    C4D89_JointRecord joints[C4D89_MAX_JOINT_RECORDS];
} C4D89_JointTable;
int c4d89_scan_joint_records(const char *path,C4D89_JointTable *table);

/* Portable fixed-point skinning kernel: takes externally VALIDATED matrices
   and weights. DOES NOT automatically infer native C4D skin data. */
typedef struct C4D89_Influence {
    unsigned int joint;
    long weight_q16;
} C4D89_Influence;
typedef struct C4D89_SkinVertex {
    unsigned int count;
    C4D89_Influence influences[C4D89_SKIN_MAX_INFLUENCES];
} C4D89_SkinVertex;
int c4d89_fixed_mul(long a,long b,long *out);
int c4d89_skin_point(const long xyz[3],const C4D89_SkinVertex *weights,
                     const long *joint_matrices,unsigned int joint_count,
                     long result_xyz[3]);

/* C4D6 Weight class inventory. Opaque records are NOT joint-point weights.
   Strong framing: class id 0x8DE5, alias2, integer N and 12*N bytes. */
#define C4D89_MAX_WEIGHT_TAGS 128u
typedef struct C4D89_WeightTagHint {
    unsigned long class_offset;
    unsigned long alias_offset;
    unsigned long blob_offset;
    unsigned long blob_bytes;
    unsigned long opaque_record_count;
    unsigned long selected_point_count;
    unsigned long max_point_index;
    unsigned long bitmap_offset;
    unsigned long bitmap_bytes;
    unsigned int has_alias;
    unsigned int layout; /* 0 unknown; 1 zero entry; 2 blob only; 3 index ranges; 4 cross-checked bitmap */
} C4D89_WeightTagHint;
typedef struct C4D89_WeightInventory {
    unsigned int count;
    unsigned int omitted;
    C4D89_WeightTagHint tags[C4D89_MAX_WEIGHT_TAGS];
} C4D89_WeightInventory;
typedef int (*C4D89_OpaqueWeightFn)(void *user,unsigned long index,
                                     const unsigned char record[12]);
int c4d89_inventory_weight_tags(const char *path,C4D89_WeightInventory *out);
typedef int (*C4D89_WeightPointFn)(void *user,unsigned long point_index,
                                     unsigned long ordered_position);
/* Native index range expansion. NOT a per-joint weight decoder. */
int c4d89_stream_weight_point_ids(const char *path,
    const C4D89_WeightTagHint *tag,C4D89_WeightPointFn fn,void *user);
/* Record bytes preserved exactly: no inference of index or weight meanings. */
int c4d89_stream_opaque_weight_records(const char *path,
    const C4D89_WeightTagHint *tag,C4D89_OpaqueWeightFn fn,void *user);


/* Native polygon selection ranges. These describe sets of FACE indices, NOT
   necessarily texture->material bindings. No spatial heuristics used. */
#define C4D89_MAX_POLY_RANGES 512u
#define C4D89_POLY_MESH_UNKNOWN 65535u
typedef struct C4D89_PolyRange {
    char selection[C4D89_MAX_NAME];
    unsigned long marker_offset; /* position of the AD BB range marker */
    unsigned long first_face;
    unsigned long last_face; /* inclusive */
    unsigned int mesh_index; /* verified with mesh face bounds, or UNKNOWN */
    unsigned int verified;   /* 1=typed marker + class id + bounded indices */
} C4D89_PolyRange;
typedef struct C4D89_PolyRegions {
    unsigned int count;
    unsigned int omitted;
    C4D89_PolyRange ranges[C4D89_MAX_POLY_RANGES];
} C4D89_PolyRegions;
/* TextureTag (5616) -> polygon selection restriction (TEXTURETAG_RESTRICTION,
   parameter 1006). The underlying material pointer (1010) is NOT proven. */
#define C4D89_MAX_TEXTURE_TAGS 128u
typedef struct C4D89_TextureTagLink {
    unsigned long tag_offset, restriction_offset;
    char restriction[C4D89_MAX_NAME];
    unsigned int region_index;
    unsigned int verified_restriction;
} C4D89_TextureTagLink;
typedef struct C4D89_TextureTagLinks {
    unsigned int count, omitted, resolved_restrictions;
    C4D89_TextureTagLink tags[C4D89_MAX_TEXTURE_TAGS];
} C4D89_TextureTagLinks;
int c4d89_scan_texturetag_links(const char *path,const C4D89_PolyRegions *regions,
                               C4D89_TextureTagLinks *out);

/* Phong-like fixed-point normals; Q10 signed output, caller selects crease
   cosine in Q10 (e.g. 587=55 degrees). No runtime heap allocation. */
typedef int (*C4D89_SmoothFaceFn)(void *user,unsigned long face_index,
    const C4D89_Face *face,short normals_q10[4][3]);
int c4d89_emit_smooth_faces(const char *path,unsigned int mesh_number,
    const C4D89_MeshDesc *mesh,const C4D89_PolyRegions *regions,
    int cosine_q10,C4D89_SmoothFaceFn fn,void *user);
int c4d89_export_obj_smooth(const char *path,const char *dest,int cosine_q10);


/* Parses native Tpolygonselection records with the observed AD BB framing.
   Only matches records that pass class, string, and face-bound validation. */
int c4d89_scan_poly_regions(const char *path,
                            const C4D89_MeshTable *mesh_table,
                            C4D89_PolyRegions *out);
/* 0 on unselected face, >0 for number of matching selection ranges. */
unsigned int c4d89_find_face_regions(const C4D89_PolyRegions *regions,
                                   unsigned int mesh_index,
                                   unsigned long face_index,
                                   unsigned int *range_indices,
                                   unsigned int capacity);

/* end original declarations */



/* This block is intentionally outside the generic C4D binary format.
   Callers own the asset catalogue (e.g. files inside original RAR packages).
   An exact bitmap -> file match does NOT prove TextureTag -> material identity. */
#define C4D89_RESOURCE_PATH_MAX 512u
#define C4D89_ASSET_NONE 0u
#define C4D89_ASSET_SCENE_TEX 1u
#define C4D89_ASSET_SCENE_DIR 2u
#define C4D89_ASSET_UNIQUE_PACKAGE 3u
#define C4D89_ASSET_AMBIGUOUS 4u
#define C4D89_ASSET_BAD_PATH 5u
/* Receives paths relative to the SAME package root as scene_relative_path.
   Returns 1 for a valid path, 0 for error/end; count is explicit.
   Path data is ASCII/UTF8, compared bytewise case-insensitively for ASCII. */
typedef int (*C4D89_ResourceGetFn)(void *user,unsigned int index,
                                       char *path,unsigned int capacity);
typedef struct C4D89_ResourceIndex {
    C4D89_ResourceGetFn get;
    void *user;
    unsigned int count;
} C4D89_ResourceIndex;
typedef struct C4D89_ResourceMatch {
    unsigned int status;
    unsigned int matched_index;
    unsigned int legacy_absolute_reference; /* source path ignored: basename only */
    unsigned int same_folder_matches;
    unsigned int package_matches;
    char path[C4D89_RESOURCE_PATH_MAX];
} C4D89_ResourceMatch;
int c4d89_match_bitmap_asset(const char *scene_relative_path,
         const char *bitmap_reference,const C4D89_ResourceIndex *index,
         C4D89_ResourceMatch *result);

/* Optional shortest-arc sampling for HPB Q16.16 radian rotation tracks.
   Keeps legacy scalar interpolation intact; cannot infer Bezier tangents
   or intentional multi-turn motion. */
int c4d89_angle_lerp_q16(long left,long right,long fraction_q16,long *out);
int c4d89_sample_track_angle_file(FILE *file,const C4D89_AnimTrack *track,
                                  long seconds_q16,long *angle_q16);
int c4d89_sample_track_angle(const char *path,const C4D89_AnimTrack *track,
                             long seconds_q16,long *angle_q16);

/* Per-frame bulk animation sampler: a single already-open file handle.
   Only validated scalar tracks with a known fixed key stride are sampled.
   No guesswork about parent joints, bind matrices or weighting.
   Modes: 0=all linear, 1=short-arc only for Rotation channels. */
typedef int (*C4D89_FrameTrackFn)(void *user,unsigned int track_index,
                const C4D89_AnimTrack *track,long value_q16);
typedef struct C4D89_FrameStats {
    unsigned int sampled;
    unsigned int unsupported;
} C4D89_FrameStats;
int c4d89_sample_animation_frame_file(FILE *file,
         const C4D89_AnimIndex *index,long seconds_q16,
         unsigned int rotation_shortarc,C4D89_FrameTrackFn output,
         void *user,C4D89_FrameStats *stats);
int c4d89_sample_animation_frame(const char *path,
         const C4D89_AnimIndex *index,long seconds_q16,
         unsigned int rotation_shortarc,C4D89_FrameTrackFn output,
         void *user,C4D89_FrameStats *stats);

#endif /* C4D89_H */
