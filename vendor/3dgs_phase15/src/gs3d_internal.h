#ifndef GS3D_INTERNAL_H
#define GS3D_INTERNAL_H
#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
extern FILE *gs_fp;
extern int gs_is_open;
extern int gs_owned_file;
extern GS3D_Source gs_source;
extern const unsigned char *gs_mem;
extern unsigned long gs_mem_size;
extern unsigned long gs_cursor;
extern unsigned long gs_size;
extern unsigned long gs_legacy_scale[3];
extern long gs_legacy_offset[3];
extern unsigned short gs7_header_stride[10];
extern GS3D_FileSections gs_file_sections;
int gs7_sections_init(unsigned long data_size,unsigned long ent_size,unsigned long med_size);
int gs_seek(unsigned long off,unsigned long n);
int gs_read(void *dst,unsigned long size);
int gs_get(unsigned long at,unsigned char *out,unsigned long n);
unsigned long gs_u32(const unsigned char *p);
unsigned int gs_u16(const unsigned char *p);
long gs_s16(const unsigned char *p);
int gs_addmul(unsigned long base,unsigned long count,unsigned long stride,
              unsigned long *end);
int gs_legacy(void);
int gs_legacy_header(unsigned long *cur);
int gs_legacy_skins(unsigned long *cur);
int gs_legacy_geometry(unsigned long *cur);
int gs_legacy_frames(unsigned long *cur);
int gs7_header(unsigned long *cur);
int gs7_bones(unsigned long *cur);
int gs7_group(unsigned long *cur,unsigned long index);
int gs7_skin(unsigned long *cur,unsigned long end,GS3D_Skin *skin);
int gs7_geometry(GS3D_Group *g,unsigned long *cur,unsigned long end,
                  unsigned long uv_stride,unsigned long tri_stride,
                  unsigned long vert_stride);
int gs7_frames(GS3D_Group *g,unsigned long *cur,unsigned long end);
int gs_mdl7(void);
int gs_decode_vertex_legacy(unsigned long off,unsigned int stride,GS3D_Vec *v);
int gs_decode_vertex7(unsigned long off,unsigned int stride,GS3D_Vec *v);
int gs_skin_payload(unsigned long base,unsigned long width,unsigned long height,
                    unsigned int type,unsigned long *bytes);
long gs_mul_packed(long scale,unsigned long v);
long gs_addsat(long a,long b);
int gs_dds_decode(unsigned long off,unsigned long n,unsigned long *width,unsigned long *height);
extern GS3D_ExternalTextureDecode gs_external_decoder;
extern void *gs_external_userdata;
int gs_skin_read(unsigned long pos,unsigned long width,unsigned long height,
                 unsigned int type,int write_pixels);
int gs_legacy_frame(unsigned int group,unsigned int frame);
int gs3d_legacy_deform(unsigned int group,unsigned int frame);
int gs3d_mdl7_deform(unsigned int group,unsigned int frame);
int gs_mdl7_frame(unsigned int group,unsigned int frame);
long gs_qmul(long a,long b);
void gs_mat_identity(GS3D_Mat *dst);
void gs_mat_mul(GS3D_Mat *dst,const GS3D_Mat *a,const GS3D_Mat *b);
void gs_mat_translate(GS3D_Mat *out,long x,long y,long z);
void gs_mat_point(const GS3D_Mat *m,const GS3D_Vec *v,GS3D_Vec *out);
int gs_pose_load_keys(unsigned int frame);
int gs_pose_sample_keys(unsigned int frame_a,unsigned int frame_b,long alpha);
int gs3d_skeletal_sample_raw_vertices(unsigned int group,unsigned int a,unsigned int b,long alpha);
int gs3d_trs_interpolate(const GS3D_Mat *a,const GS3D_Mat *b,long alpha,GS3D_Mat *out);
long gs3d_fraction_q16(unsigned long rem,unsigned long den);
int gs3d_blend_validate(const GS3D_Crossfade *fade,unsigned int *track);
int gs3d_blend_morph(unsigned int group,const GS3D_Clip *from,
    const GS3D_ClipSample *sample_from,const GS3D_Clip *to,
    const GS3D_ClipSample *sample_to,long alpha);
int gs3d_blend_skeletal(unsigned int group,const GS3D_ClipSample *from,
    const GS3D_ClipSample *to,long alpha);
int gs3d_layer_validate(const GS3D_LayerRequest *request,
    const GS3D_BoneMask *mask);
int gs3d_layer_skeletal(unsigned int group,const GS3D_LayerRequest *request,
    const GS3D_BoneMask *mask,const GS3D_ClipSample *base,
    const GS3D_ClipSample *overlay);
int gs_pose_file_space(void);
int gs_pose_hierarchy(void);
int gs3d_format_deform(unsigned int group,unsigned int frame);
int gs3d_format_weights(unsigned long vertex,GS3D_WeightSet *out);
void gs_weights_reset(void);
int gs_weights_append(unsigned long vertex,unsigned int bone,long weight);
int gs_weights_finish_group(unsigned int group);
int gs_weighted_skin_group(unsigned int group);
int gs_legacy_weight_info(unsigned long vertex,GS3D_WeightSet *out);
int gs_mdl7_weight_info(unsigned long vertex,GS3D_WeightSet *out);
int gs7_read_deformers(GS3D_Group *g,unsigned int group,unsigned long *cursor,unsigned long end,unsigned int count,unsigned int maxweights);
extern GS3D_DeformerInfo gs_deformer_info[GS3D_MAX_GROUPS];
extern GS3D_WeightSet gs_vertex_weights[GS3D_MAX_VERTS];
extern int gs3d_pose_ready;
extern GS3D_Mat gs_pose_local[GS3D_MAX_BONES];
extern GS3D_Mat gs_pose_world[GS3D_MAX_BONES];
typedef struct GS3D_Quat {long x,y,z,w;} GS3D_Quat;
typedef struct GS3D_TRS { GS3D_Quat q; long scale[3],translation[3]; } GS3D_TRS;
int gs3d_trs_read(const GS3D_Mat *m,GS3D_TRS *out);
void gs3d_trs_write(const GS3D_TRS *in,GS3D_Mat *out);
int gs3d_trs_normalize(GS3D_Quat *q);
int gs3d_div_q16(long a,long b,long *out);
int gs3d_additive_matrix(const GS3D_Mat *base,const GS3D_Mat *add,
                         const GS3D_Mat *reference,long weight,GS3D_Mat *out);

int gs3d_a4_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m);
int gs3d_a5_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m);
int gs3d_a6_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m);
int gs3d_a7_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m);
int gs3d_a8_additive(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m);
int gs3d_additive_validate(const GS3D_AdditiveRequest *request,
                           const GS3D_BoneMask *mask);
int gs3d_additive_legacy(unsigned int group,const GS3D_AdditiveRequest *req);
int gs3d_additive_skeletal(unsigned int group,
                          const GS3D_AdditiveRequest *request,
                          const GS3D_BoneMask *mask,
                          const GS3D_ClipSample *base,
                          const GS3D_ClipSample *add,
                          const GS3D_ClipSample *ref);

#endif
