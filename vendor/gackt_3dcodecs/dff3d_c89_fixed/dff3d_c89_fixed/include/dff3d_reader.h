#ifndef DFF3D_READER_H
#define DFF3D_READER_H

#include "dff3d_types.h"

typedef struct Dff3dReaderTag {
    const dff_u8 *data;
    dff_u32 size;
    dff_u32 pos;
    int error;
} Dff3dReader;

typedef struct Dff3dChunkTag {
    dff_u32 type;
    dff_u32 size;
    dff_u32 version_stamp;
    dff_u32 body;
    dff_u32 end;
} Dff3dChunk;

void dff3d_reader_init(Dff3dReader *r, const void *data, dff_u32 size);
int dff3d_reader_can_read(const Dff3dReader *r, dff_u32 n);
dff_u8 dff3d_read_u8(Dff3dReader *r);
dff_u16 dff3d_read_u16(Dff3dReader *r);
dff_u32 dff3d_read_u32(Dff3dReader *r);
dff_i32 dff3d_read_i32(Dff3dReader *r);
dff_fixed dff3d_read_f32_fixed(Dff3dReader *r);
int dff3d_read_chunk_header(Dff3dReader *r, Dff3dChunk *c);
void dff3d_reader_skip_to(Dff3dReader *r, dff_u32 pos);
void dff3d_reader_sub(Dff3dReader *dst, const Dff3dReader *src, const Dff3dChunk *c);
dff_u32 dff3d_decode_rw_version(dff_u32 library_id_stamp);

#endif
