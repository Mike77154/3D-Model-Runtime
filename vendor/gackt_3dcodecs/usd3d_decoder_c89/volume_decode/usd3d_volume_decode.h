#ifndef USD3D_VOLUME_DECODE_H
#define USD3D_VOLUME_DECODE_H

#include "usd3d.h"

#define USD3D_VOLUME_KIND_UNKNOWN 0
#define USD3D_VOLUME_KIND_OPENVDB 1
#define USD3D_VOLUME_KIND_FIELD3D 2
#define USD3D_VOLUME_KIND_HDF5 3

typedef struct usd3d_volume_probe_s {
    int kind;
    char format_name[USD3D_MAX_NAME];
    char grid_name[USD3D_MAX_NAME];
    unsigned long header_bytes;
    unsigned long printable_tokens;
    int has_hdf5_signature;
    int has_openvdb_marker;
} usd3d_volume_probe;

void usd3d_volume_probe_init(usd3d_volume_probe *probe);
int usd3d_volume_probe_memory(const unsigned char *data, unsigned long size, usd3d_volume_probe *probe);
int usd3d_volume_probe_file(const char *path, usd3d_volume_probe *probe);
const char *usd3d_volume_kind_name(int kind);

#endif
