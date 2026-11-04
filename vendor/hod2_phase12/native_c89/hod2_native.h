#ifndef HOD2_NATIVE_H
#define HOD2_NATIVE_H
/* Portable C89, 32-bit little-endian binary data, bounded buffers, no heap. */
#include <stdio.h>
#include <stddef.h>
#define HOD2_FILE_CAP (64UL*1024UL*1024UL)
#define HOD2_MAX_BANKS 700U
#define HOD2_MAX_NODES 128U
#define HOD2_MAX_MODELS 4096U
#define HOD2_MAX_TEX 4096U
#define HOD2_MAX_PATH 1024U
#define HOD2_MAX_MATERIALS 1024U
#define HOD2_MAX_VERTICES 1000000UL
/* Shared fixed, non-heap I/O workspaces. Callers must not use them concurrently. */
extern unsigned char hod2_workspace_raw[HOD2_FILE_CAP];
extern unsigned char hod2_workspace_decoded[HOD2_FILE_CAP];
#define HOD2_OK 1
#define HOD2_BAD 0
#define HOD2_ERR (-1)

typedef struct Hod2Bytes { const unsigned char *data; size_t size; } Hod2Bytes;
typedef struct Hod2PE { Hod2Bytes file; int profile; } Hod2PE;
typedef struct Hod2Bank { unsigned int index; char name[96]; unsigned int count; unsigned long id_va; } Hod2Bank;
typedef struct Hod2TexDesc { unsigned int width,height; unsigned long fmt,offset,slot; } Hod2TexDesc;
typedef struct Hod2Node { unsigned int id, parent,children,mid; unsigned long pos_bits[3]; unsigned long va; } Hod2Node;
typedef struct Hod2PolyStat { unsigned long materials,strips,vertices,triangles,nonfinite_uv,alt_strips,untextured; unsigned long texrefs; } Hod2PolyStat;

typedef int (*Hod2VertexCb)(const unsigned char *vertex32, unsigned long texslot, void *user);

unsigned long hod2_u16(const unsigned char *b);
unsigned long hod2_u32(const unsigned char *b);
long hod2_s16(const unsigned char *b);
long hod2_s32(const unsigned char *b);
int hod2_span(size_t size, unsigned long offset, unsigned long length);
int hod2_read_file(const char *file, unsigned char *dst, size_t cap, size_t *len);
int hod2_write_file(const char *file,const unsigned char *data,size_t len);
int hod2_prs_decode(const unsigned char *source,size_t src_size,unsigned char *out,size_t capacity,size_t *written);
int hod2_file_decode(const unsigned char *src,size_t in_size,unsigned char *dst,size_t out_cap,size_t *out_size);
int hod2_pe_init(Hod2PE *pe,const unsigned char *src,size_t length,int profile);
/* Standalone data-table archive, with no PE code or EXE dependency. */
int hod2_tablepack_valid(const unsigned char *data,size_t size,int profile);
int hod2_tablepack_data(const unsigned char *data,size_t size,unsigned long va,
                        size_t len,const unsigned char **out);
int hod2_tablepack_export(const Hod2PE *pe,const char *filename);
int hod2_tablepack_dump_csv(const Hod2PE *pe,const char *dir);
int hod2_pe_data(const Hod2PE *pe,unsigned long va,size_t len,const unsigned char **out);
int hod2_pe_u16(const Hod2PE *pe,unsigned long va,unsigned long *out);
int hod2_pe_u32(const Hod2PE *pe,unsigned long va,unsigned long *out);
unsigned long hod2_pe_shift(const Hod2PE *pe,unsigned long original,int data_section);
int hod2_bank(const Hod2PE *pe,unsigned int index,int motion,Hod2Bank *out);
int hod2_bank_id(const Hod2PE *pe,const Hod2Bank *bank,unsigned int i,long *id);
int hod2_find_model(const Hod2PE *pe,unsigned int mid,Hod2Bank *bank,unsigned int *within);
int hod2_skeleton(const Hod2PE *pe,unsigned int type,Hod2Node *nodes,unsigned int maxnodes,unsigned int *count);
int hod2_tex_desc(const Hod2PE *pe,unsigned int bank,unsigned int index,Hod2TexDesc *desc,unsigned int *count);
int hod2_tex_pixel(const unsigned char *src,size_t size,const Hod2TexDesc *d,unsigned int x,unsigned int y,const unsigned long *palette,unsigned char rgba[4]);
int hod2_tex_png(const char *filename,const unsigned char *src,size_t size,const Hod2TexDesc *d,const unsigned long *palette);
int hod2_pol_model(const unsigned char *data,size_t size,unsigned int index,unsigned long *root,unsigned long *end,unsigned int *model_count);
int hod2_pol_scan(const unsigned char *data,size_t size,unsigned long root,unsigned long end,Hod2PolyStat *s,Hod2VertexCb cb,void *cbctx);
int hod2_mot_stride(unsigned int bones,unsigned long ticks,unsigned long blocksize,unsigned long *stride,unsigned long *frames);
int hod2_mot_list(const Hod2PE *pe,const char *mot_dir,unsigned int character_type,const char *csv_path,unsigned long *found);
int hod2_mot_dump(const Hod2PE *pe,const char *mot_dir,unsigned int character_type,unsigned int motion_id,const char *out_path);
int hod2_corpus_audit(const char *root,const char *csv);
int hod2_corpus_diff(const char *root_a,const char *root_b,const char *csv);
int hod2_glb_check(const char *path,unsigned long *json_size,unsigned long *bin_size);
int hod2_export_glb(const char *out,const unsigned char *poly,size_t size,unsigned int index);
/* Build a rigid-node character GLB from the original skeleton, POL files, and
 * optionally one lossless H2MC motion. No extra-part deformation is implied.
 */
int hod2_export_character_glb(const char *out,const Hod2PE *pe,
  unsigned int type,const char *pol_directory,const char *motion_h2mc,
  unsigned int *included_nodes,unsigned int *missing_nodes,const char *tex_directory);
int hod2_export_glb_textured(const char *out,const unsigned char *poly,size_t size,unsigned int index,const Hod2PE *pe,unsigned int bank,const unsigned char *texdata,size_t texlen);
int hod2_skin_audit(const Hod2PE *pe,const char *pol_dir,const char *csv,
                    unsigned long *matched,unsigned long *total,unsigned long *parts);
int hod2_skin_map(const Hod2PE *pe,const char *pol_dir,unsigned int type,
                  unsigned int pi,const char *csv,unsigned long *nref,
                  unsigned long *overrides);
int hod2_tail23_report(const Hod2PE *pe,const char *csv,unsigned long *found);
int hod2_export_skinned_part_glb(const char *out,const Hod2PE *pe,unsigned int type,
 unsigned int part,const char *pol_dir,const char *h2mc,const char *tex_dir);
int hod2_export_skinned_character_glb(const char *out,const Hod2PE *pe,unsigned int type,
 const char *pol_dir,const char *h2mc,const char *tex_dir);
#endif
