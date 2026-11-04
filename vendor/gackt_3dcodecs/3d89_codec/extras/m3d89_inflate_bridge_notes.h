#ifndef M3D89_INFLATE_BRIDGE_NOTES_H
#define M3D89_INFLATE_BRIDGE_NOTES_H

/*
   This header is intentionally only a contract sketch.

   M3D89 core detects a packed Model 3D payload and returns
   M3D89_ERR_STREAM_PACKED. A platform layer can unpack into a caller-owned
   static buffer, then call:

       m3d89_decode_payload(&model, unpacked_head_payload, unpacked_size,
                            original_file_size, original_payload_offset);

   Keep any inflater outside the core if your target forbids heap usage.
*/

typedef int (*m3d89_inflate_to_static_fn)(
    const unsigned char *src,
    unsigned long src_size,
    unsigned char *dst,
    unsigned long dst_cap,
    unsigned long *dst_size);

#endif
