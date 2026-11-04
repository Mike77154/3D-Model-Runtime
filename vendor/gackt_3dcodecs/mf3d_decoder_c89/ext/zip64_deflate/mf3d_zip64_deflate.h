#ifndef MF3D_ZIP64_DEFLATE_H
#define MF3D_ZIP64_DEFLATE_H

/* Optional no-heap ZIP64 + Deflate module for mf3d.
   C89, no malloc/free/realloc, no float/double.
   It is intentionally separate from the core so tiny builds may omit it. */

#include "mf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MF3D_Z64_MAX_NAME
#define MF3D_Z64_MAX_NAME 256
#endif
#ifndef MF3D_Z64_MAX_ENTRIES
#define MF3D_Z64_MAX_ENTRIES MF3D_MAX_ZIP_ENTRIES
#endif

#define MF3D_Z64_GP_DATA_DESCRIPTOR 8U
#define MF3D_Z64_METHOD_STORE 0U
#define MF3D_Z64_METHOD_DEFLATE 8U

typedef struct mf3d_z64_u64_s {
    mf3d_u32 lo;
    mf3d_u32 hi;
} mf3d_z64_u64;

typedef struct mf3d_z64_entry_s {
    char name[MF3D_Z64_MAX_NAME];
    mf3d_u16 flag;
    mf3d_u16 method;
    mf3d_u32 crc32;
    mf3d_z64_u64 comp_size;
    mf3d_z64_u64 uncomp_size;
    mf3d_z64_u64 local_off;
    mf3d_z64_u64 data_off;
    mf3d_u32 data_descriptor_off;
    mf3d_u32 zip64_used;
    mf3d_u32 descriptor_used;
} mf3d_z64_entry;

typedef struct mf3d_z64_archive_s {
    mf3d_z64_entry entries[MF3D_Z64_MAX_ENTRIES];
    mf3d_u32 count;
    mf3d_z64_u64 central_dir_off;
    mf3d_z64_u64 central_dir_size;
    mf3d_u32 zip64_eocd_used;
} mf3d_z64_archive;

/* Raw RFC1951 Deflate inflater callback: supports stored, fixed Huffman, and
   dynamic Huffman blocks. The input must be raw Deflate, as ZIP method 8 stores. */
int mf3d_zip64_deflate_inflate_cb(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                                  mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);

/* ZIP64/data-descriptor aware archive reader. It still requires the complete
   ZIP buffer in memory and rejects offsets/sizes above the addressable buffer. */
int mf3d_zip64_read(const mf3d_u8 *data, mf3d_u32 size, mf3d_z64_archive *zip);
int mf3d_zip64_find(mf3d_z64_archive *zip, const char *name);
int mf3d_zip64_find_first_model(mf3d_z64_archive *zip);
int mf3d_zip64_extract(const mf3d_u8 *data, mf3d_u32 size, mf3d_z64_archive *zip, int idx,
                       mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);
int mf3d_zip64_extract_named(const mf3d_u8 *data, mf3d_u32 size, const char *name,
                             mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);

#ifdef __cplusplus
}
#endif
#endif
