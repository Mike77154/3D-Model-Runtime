#ifndef RE4_ARCHIVE_H
#define RE4_ARCHIVE_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum re4_archive_kind_e {
    RE4_ARCHIVE_UNKNOWN = 0,
    RE4_ARCHIVE_DAT_GUESS = 1,
    RE4_ARCHIVE_UDAS_GUESS = 2,
    RE4_ARCHIVE_AFS = 3,
    RE4_ARCHIVE_GCA = 4,
    RE4_ARCHIVE_LFS = 5
} re4_archive_kind;

#define RE4_ARCHIVE_ENTRY_F_INFERRED_OFFSET 1U
#define RE4_ARCHIVE_ENTRY_F_COMPRESSED_HINT 2U
#define RE4_ARCHIVE_ENTRY_F_NAME_REF 4U

typedef struct re4_archive_entry_s {
    re4_strref name;
    re4_u32 offset;
    re4_u32 packed_size;
    re4_u32 unpacked_size;
    re4_u32 flags;
} re4_archive_entry;

typedef struct re4_archive_s {
    re4_archive_kind kind;
    re4_u32 entry_count;
    re4_archive_entry *entries;
    re4_endian endian;
    re4_u32 flags;
} re4_archive;

typedef struct re4_lfs_chunk_s {
    re4_u32 offset;
    re4_u16 packed_size;
    re4_u16 unpacked_size;
    re4_u8 compressed;
} re4_lfs_chunk;

typedef enum re4_lfs_kind_e {
    RE4_LFS_KIND_UNKNOWN = 0,
    RE4_LFS_KIND_CHUNK_TABLE = 1,
    RE4_LFS_KIND_STREAM_XMEM = 2
} re4_lfs_kind;

typedef struct re4_lfs_s {
    re4_lfs_kind kind;
    re4_u32 magic1;
    re4_u32 magic2;
    re4_u32 size_decompressed;
    re4_u32 size_compressed;
    re4_u32 chunk_count;
    re4_lfs_chunk *chunks;
    re4_u32 stream_offset;
    re4_u32 stream_packed_size;
    re4_u32 stream_unpacked_size;
    re4_u32 stream_flags;
    re4_endian endian;
} re4_lfs;

typedef re4_err (*re4_lfs_codec_fn)(void *user,
                                    const re4_u8 *src,
                                    re4_size src_size,
                                    re4_u8 *dst,
                                    re4_size *dst_size);

re4_err re4_lfs_read(re4_span src, re4_arena *arena, re4_lfs *out);
const char *re4_lfs_kind_name(re4_lfs_kind kind);
re4_err re4_lfs_chunk_span(re4_span src, const re4_lfs_chunk *chunk, re4_span *out, re4_size *unpacked_size);
re4_err re4_lfs_extract(re4_span src, const re4_lfs *lfs,
                        re4_u8 *dst, re4_size dst_cap, re4_size *written,
                        re4_lfs_codec_fn codec, void *codec_user);
re4_err re4_lfs_store_size(re4_size raw_size, re4_size *out_size, re4_u32 *out_chunks);
re4_err re4_lfs_write_store(re4_span raw, re4_endian endian, re4_u8 *dst, re4_size dst_cap, re4_size *written);

/* Confirmed RE4 DAT/MAP and UDAS table layout, separate from legacy guesses. */
typedef struct re4_udas_parts_s {
    re4_bool has_dat;
    re4_u32 dat_offset;
    re4_u32 dat_size;
    re4_bool has_sound;
    re4_u32 sound_type;
    re4_u32 sound_offset;
    re4_u32 sound_size;
} re4_udas_parts;

re4_err re4_dat_read_indexed(re4_span src, re4_endian endian, re4_bool e3,
                             re4_arena *arena, re4_archive *out);
re4_err re4_udas_read_parts(re4_span src, re4_endian endian, re4_udas_parts *out);
re4_err re4_udas_read_indexed(re4_span src, re4_endian endian, re4_bool e3,
                              re4_arena *arena, re4_archive *out, re4_udas_parts *parts);

re4_err re4_afs_read(re4_span src, re4_arena *arena, re4_archive *out);
re4_err re4_dat_read_guess(re4_span src, re4_endian endian, re4_arena *arena, re4_archive *out);
re4_err re4_udas_read_guess(re4_span src, re4_endian endian, re4_arena *arena, re4_archive *out);
re4_err re4_gca_probe(re4_span src, re4_archive *out);
re4_err re4_gca_read(re4_span src, re4_arena *arena, re4_archive *out);
/* Parse only the footer of a GCAX archive; offsets are file-relative. */
re4_err re4_gca_read_footer(re4_span footer, re4_u32 archive_size,
                            re4_u32 footer_offset, re4_arena *arena,
                            re4_archive *out);
re4_err re4_archive_entry_span(re4_span src, const re4_archive_entry *entry, re4_span *out);

#ifdef __cplusplus
}
#endif

#endif
