#ifndef GIFFANY_SCN3D_INTERNAL_H
#define GIFFANY_SCN3D_INTERNAL_H

#include "scn3d.h"

long scn3d_read_bytes(scn3d_io *io, void *dst, long bytes);
int scn3d_seek_abs(scn3d_io *io, long pos);
long scn3d_tell_pos(scn3d_io *io);
int scn3d_skip_bytes(scn3d_io *io, long bytes);
int scn3d_read_u8(scn3d_io *io, scn3d_u8 *v);
int scn3d_read_le16(scn3d_io *io, long *v);
int scn3d_read_le32(scn3d_io *io, long *v);
int scn3d_read_u32_bits(scn3d_io *io, scn3d_u32 *v);

void scn3d_emit_warning(scn3d_callbacks *cb, int code, const char *message);

#endif
