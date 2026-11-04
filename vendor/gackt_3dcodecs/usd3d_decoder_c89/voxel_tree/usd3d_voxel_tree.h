#ifndef USD3D_VOXEL_TREE_H
#define USD3D_VOXEL_TREE_H

#include "usd3d.h"
#include "usd3d_volume_metadata.h"

#ifndef USD3D_VOXEL_MAX_GRIDS
#define USD3D_VOXEL_MAX_GRIDS 64
#endif

#ifndef USD3D_VOXEL_MAX_NODES
#define USD3D_VOXEL_MAX_NODES 512
#endif

#define USD3D_VOXEL_FORMAT_UNKNOWN 0
#define USD3D_VOXEL_FORMAT_OPENVDB 1
#define USD3D_VOXEL_FORMAT_FIELD3D_HDF5 2

#define USD3D_VOXEL_NODE_ROOT 1
#define USD3D_VOXEL_NODE_INTERNAL 2
#define USD3D_VOXEL_NODE_LEAF 3
#define USD3D_VOXEL_NODE_TILE 4
#define USD3D_VOXEL_NODE_HDF5_DATASET 5

typedef struct usd3d_voxel_grid_s {
    char name[USD3D_MAX_NAME];
    char value_type[USD3D_MAX_NAME];
    unsigned long index;
    unsigned long offset;
    unsigned long voxel_count_hint;
    int active;
} usd3d_voxel_grid;

typedef struct usd3d_voxel_node_s {
    int kind;
    unsigned long grid_index;
    long origin_x;
    long origin_y;
    long origin_z;
    unsigned long child_mask_lo;
    unsigned long child_mask_hi;
    unsigned long file_offset;
    unsigned long byte_size;
    usd3d_fixed constant_value;
    int active;
} usd3d_voxel_node;

typedef struct usd3d_voxel_tree_s {
    int format;
    unsigned long version_major;
    unsigned long version_minor;
    unsigned long grid_count;
    unsigned long node_count;
    unsigned long hdf5_superblock_version;
    unsigned long hdf5_offset_size;
    unsigned long hdf5_length_size;
    usd3d_voxel_grid grids[USD3D_VOXEL_MAX_GRIDS];
    usd3d_voxel_node nodes[USD3D_VOXEL_MAX_NODES];
} usd3d_voxel_tree;

void usd3d_voxel_tree_init(usd3d_voxel_tree *tree);
const char *usd3d_voxel_format_name(int format);
const char *usd3d_voxel_node_kind_name(int kind);
int usd3d_voxel_tree_scan_memory(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree);
int usd3d_voxel_tree_add_grid(usd3d_voxel_tree *tree, const char *name, const char *value_type, unsigned long offset);
int usd3d_voxel_tree_add_node(usd3d_voxel_tree *tree, int kind, unsigned long grid_index, long ox, long oy, long oz, unsigned long file_offset, unsigned long byte_size);
int usd3d_voxel_tree_emit(const usd3d_voxel_tree *tree,
                          int (*grid_cb)(void *user, const usd3d_voxel_grid *grid),
                          int (*node_cb)(void *user, const usd3d_voxel_node *node),
                          void *user);
int usd3d_voxel_hdf5_read_superblock(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree);
int usd3d_voxel_openvdb_scan_tree_tokens(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree);

#endif
