#ifndef RTCW_CODEC_H
#define RTCW_CODEC_H
/* White-room implementation, CC0-1.0. Strict C89, no heap or floating-point. */
#define RTCW_MAX_FILE (32UL * 1024UL * 1024UL)
#define RTCW_MAX_SURFACES 32
#define RTCW_MAX_VERTICES 192000UL
#define RTCW_MAX_TRIANGLES 262144UL
#define RTCW_MAX_BONES 128
#define RTCW_COORD_SCALE 64L
#define RTCW_UV_SCALE 4096L

typedef struct rtcw_vertex_s {
    long x, y, z; /* Q6 world space */
    long u, v;    /* Q12 texture coordinates */
    long sx, sy;  /* projected pixel coords */
} rtcw_vertex;
typedef struct rtcw_triangle_s { unsigned long a, b, c; unsigned int surface; } rtcw_triangle;
typedef struct rtcw_surface_s {
    char name[65], shader[65];
    unsigned long first_vertex, vertices, first_triangle, triangles;
    long min_lod;
} rtcw_surface;
typedef struct rtcw_bone_s { char name[65]; long parent; long distance_q6; unsigned long flags; } rtcw_bone;
typedef struct rtcw_tag_s {
    char name[65];
    long bone_index;
    long origin[3]; /* Q6 */
    long axis[3][3]; /* Q14: 3 local basis vectors, axis[local_axis][world_component] */
} rtcw_tag;
typedef struct rtcw_model_s {
    char format[4];
    char name[65];
    unsigned long frames, frame, bone_count, tag_count;
    unsigned int surface_count;
    unsigned long vertex_count, triangle_count;
    rtcw_surface surfaces[RTCW_MAX_SURFACES];
    rtcw_bone bones[RTCW_MAX_BONES];
    rtcw_tag tags[RTCW_MAX_BONES];
    rtcw_vertex verts[RTCW_MAX_VERTICES];
    rtcw_triangle tris[RTCW_MAX_TRIANGLES];
} rtcw_model;
extern rtcw_model rtcw_current;
/* Load file (<=32 MiB), decode given frame; never retains allocations. */
int rtcw_open(const char *filename);
int rtcw_decode(unsigned long frame);
const char *rtcw_error(void);
int rtcw_save_obj(const char *filename);
int rtcw_save_ppm(const char *filename);
int rtcw_report(const char *filename);
/* MDS: blend independent legs/torso frames BEFORE skinning. torso_yaw=65536 units/revolution. */
int rtcw_decode_mds_pose(unsigned long legs_a,unsigned long legs_b,unsigned int legs_t,
                         unsigned long torso_a,unsigned long torso_b,unsigned int torso_t,
                         unsigned short torso_yaw);
/* Render a blended pose (MDS bone-space; MD3/MDC vertex-space). */
int rtcw_decode_blend(unsigned long a, unsigned long b, unsigned int t_q12);
/* Assemble a two-piece tagged scene into rtcw_current; filenames are local files. */
int rtcw_attach_pair(const char *parent_file, unsigned long parent_frame,
                     const char *parent_skin, const char *tag_name,
                     const char *child_file, unsigned long child_frame,
                     const char *child_skin);
const char *rtcw_attach_error(void);
/* Incremental multi-part assembly. Begin once; add up to 30 child parts. */
int rtcw_attach_begin(const char *parent_file,unsigned long frame,const char *skin);
/* v0.12: animated parent already decoded, child with optional own frame interpolation. */
int rtcw_attach_begin_decoded(void);
int rtcw_attach_add_blend(const char *tag_name,const char *child_file,
                          unsigned long child_frame,unsigned long child_next,
                          unsigned int blend_q12,const char *child_skin);
int rtcw_attach_add(const char *tag_name,const char *child_file,
                    unsigned long frame,const char *skin);
/* Optional RTCW .skin material map. Returns 0 on parse/open errors. */
int rtcw_apply_skin(const char *filename);
#endif
