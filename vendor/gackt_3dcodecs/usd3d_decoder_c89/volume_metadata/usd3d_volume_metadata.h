#ifndef USD3D_VOLUME_METADATA_H
#define USD3D_VOLUME_METADATA_H

#include "usd3d.h"
#include "usd3d_volume_decode.h"

#ifndef USD3D_VOLUME_META_MAX_GRIDS
#define USD3D_VOLUME_META_MAX_GRIDS 64
#endif

typedef struct usd3d_volume_grid_meta_s {
    char name[USD3D_MAX_NAME];
    char value_type[USD3D_MAX_NAME];
    unsigned long offset;
    int active;
} usd3d_volume_grid_meta;

typedef struct usd3d_volume_metadata_s {
    usd3d_volume_probe probe;
    usd3d_volume_grid_meta grids[USD3D_VOLUME_META_MAX_GRIDS];
    unsigned long grid_count;
    unsigned long hdf5_superblock_version;
    unsigned long hdf5_offset_size;
    unsigned long hdf5_length_size;
    int has_openvdb_file_version_token;
    int has_field3d_partition_token;
} usd3d_volume_metadata;

void usd3d_volume_metadata_init(usd3d_volume_metadata *m);
int usd3d_volume_metadata_scan_memory(const unsigned char *data, unsigned long size, usd3d_volume_metadata *m);
int usd3d_volume_metadata_emit(const usd3d_volume_metadata *m,
                               int (*grid_cb)(void *user, const usd3d_volume_grid_meta *grid),
                               void *user);

#endif
