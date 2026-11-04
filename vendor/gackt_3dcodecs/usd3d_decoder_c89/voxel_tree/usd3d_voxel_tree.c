#include "usd3d_voxel_tree.h"

#include <string.h>

static void vt_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int vt_contains_at(const unsigned char *data, unsigned long size, unsigned long off, const char *needle)
{
    unsigned long n;
    if (data == 0 || needle == 0) return 0;
    n = (unsigned long)strlen(needle);
    if (off + n > size) return 0;
    return memcmp(data + off, needle, (size_t)n) == 0;
}

static int vt_find(const unsigned char *data, unsigned long size, const char *needle, unsigned long *out_off)
{
    unsigned long i;
    unsigned long n;
    if (data == 0 || needle == 0) return 0;
    n = (unsigned long)strlen(needle);
    if (n == 0UL || size < n) return 0;
    for (i = 0UL; i + n <= size; ++i) {
        if (memcmp(data + i, needle, (size_t)n) == 0) {
            if (out_off != 0) *out_off = i;
            return 1;
        }
    }
    return 0;
}

void usd3d_voxel_tree_init(usd3d_voxel_tree *tree)
{
    if (tree == 0) return;
    memset(tree, 0, sizeof(*tree));
}

const char *usd3d_voxel_format_name(int format)
{
    switch (format) {
    case USD3D_VOXEL_FORMAT_OPENVDB: return "OpenVDB";
    case USD3D_VOXEL_FORMAT_FIELD3D_HDF5: return "Field3D/HDF5";
    default: return "unknown";
    }
}

const char *usd3d_voxel_node_kind_name(int kind)
{
    switch (kind) {
    case USD3D_VOXEL_NODE_ROOT: return "root";
    case USD3D_VOXEL_NODE_INTERNAL: return "internal";
    case USD3D_VOXEL_NODE_LEAF: return "leaf";
    case USD3D_VOXEL_NODE_TILE: return "tile";
    case USD3D_VOXEL_NODE_HDF5_DATASET: return "hdf5_dataset";
    default: return "unknown";
    }
}

int usd3d_voxel_tree_add_grid(usd3d_voxel_tree *tree, const char *name, const char *value_type, unsigned long offset)
{
    usd3d_voxel_grid *g;
    if (tree == 0 || name == 0) return USD3D_ERR_BAD_ARG;
    if (tree->grid_count >= (unsigned long)USD3D_VOXEL_MAX_GRIDS) return USD3D_ERR_LIMIT;
    g = &tree->grids[tree->grid_count];
    memset(g, 0, sizeof(*g));
    g->index = tree->grid_count;
    vt_copy(g->name, sizeof(g->name), name);
    vt_copy(g->value_type, sizeof(g->value_type), value_type);
    g->offset = offset;
    g->active = 1;
    tree->grid_count++;
    return USD3D_OK;
}

int usd3d_voxel_tree_add_node(usd3d_voxel_tree *tree, int kind, unsigned long grid_index, long ox, long oy, long oz, unsigned long file_offset, unsigned long byte_size)
{
    usd3d_voxel_node *n;
    if (tree == 0) return USD3D_ERR_BAD_ARG;
    if (tree->node_count >= (unsigned long)USD3D_VOXEL_MAX_NODES) return USD3D_ERR_LIMIT;
    n = &tree->nodes[tree->node_count++];
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    n->grid_index = grid_index;
    n->origin_x = ox;
    n->origin_y = oy;
    n->origin_z = oz;
    n->file_offset = file_offset;
    n->byte_size = byte_size;
    n->active = 1;
    return USD3D_OK;
}

int usd3d_voxel_hdf5_read_superblock(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree)
{
    if (data == 0 || tree == 0) return USD3D_ERR_BAD_ARG;
    if (size < 16UL) return USD3D_ERR_LIMIT;
    if (!(data[0] == 0x89U && data[1] == 'H' && data[2] == 'D' && data[3] == 'F')) return USD3D_ERR_FORMAT;
    tree->format = USD3D_VOXEL_FORMAT_FIELD3D_HDF5;
    tree->hdf5_superblock_version = (unsigned long)data[8];
    tree->hdf5_offset_size = (unsigned long)data[13];
    tree->hdf5_length_size = (unsigned long)data[14];
    return USD3D_OK;
}

int usd3d_voxel_openvdb_scan_tree_tokens(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree)
{
    static const char *grids[] = { "density", "temperature", "flame", "velocity", "vel", "levelset", "fog", "color", 0 };
    unsigned int i;
    unsigned long off;
    if (data == 0 || tree == 0) return USD3D_ERR_BAD_ARG;
    tree->format = USD3D_VOXEL_FORMAT_OPENVDB;
    if (vt_find(data, size, "fileVersion", &off)) tree->version_major = 1UL;
    for (i = 0U; grids[i] != 0; ++i) {
        if (vt_find(data, size, grids[i], &off)) usd3d_voxel_tree_add_grid(tree, grids[i], "unknown", off);
    }
    if (tree->grid_count == 0UL) usd3d_voxel_tree_add_grid(tree, "unknown_openvdb_grid", "unknown", 0UL);
    if (vt_find(data, size, "RootNode", &off) || vt_find(data, size, "root", &off)) usd3d_voxel_tree_add_node(tree, USD3D_VOXEL_NODE_ROOT, 0UL, 0L, 0L, 0L, off, 0UL);
    if (vt_find(data, size, "InternalNode", &off) || vt_find(data, size, "internal", &off)) usd3d_voxel_tree_add_node(tree, USD3D_VOXEL_NODE_INTERNAL, 0UL, 0L, 0L, 0L, off, 0UL);
    if (vt_find(data, size, "LeafNode", &off) || vt_find(data, size, "leaf", &off)) usd3d_voxel_tree_add_node(tree, USD3D_VOXEL_NODE_LEAF, 0UL, 0L, 0L, 0L, off, 0UL);
    if (tree->node_count == 0UL) usd3d_voxel_tree_add_node(tree, USD3D_VOXEL_NODE_ROOT, 0UL, 0L, 0L, 0L, 0UL, size);
    return USD3D_OK;
}

static void vt_scan_hdf5_datasets(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree)
{
    static const char *names[] = { "density", "temperature", "partition", "Field3D", "voxels", "data", 0 };
    unsigned int i;
    unsigned long off;
    for (i = 0U; names[i] != 0; ++i) {
        if (vt_find(data, size, names[i], &off)) {
            usd3d_voxel_tree_add_grid(tree, names[i], "hdf5-dataset-token", off);
            usd3d_voxel_tree_add_node(tree, USD3D_VOXEL_NODE_HDF5_DATASET, tree->grid_count - 1UL, 0L, 0L, 0L, off, 0UL);
        }
    }
}

int usd3d_voxel_tree_scan_memory(const unsigned char *data, unsigned long size, usd3d_voxel_tree *tree)
{
    if (data == 0 || tree == 0) return USD3D_ERR_BAD_ARG;
    usd3d_voxel_tree_init(tree);
    if (usd3d_voxel_hdf5_read_superblock(data, size, tree) == USD3D_OK) {
        vt_scan_hdf5_datasets(data, size, tree);
        if (tree->grid_count == 0UL) usd3d_voxel_tree_add_grid(tree, "unknown_hdf5_dataset", "unknown", 0UL);
        return USD3D_OK;
    }
    if (vt_find(data, size, "OpenVDB", 0) || vt_find(data, size, "OPENVDB", 0) || vt_find(data, size, "fileVersion", 0)) return usd3d_voxel_openvdb_scan_tree_tokens(data, size, tree);
    if (vt_find(data, size, "Field3D", 0) || vt_find(data, size, "partition", 0)) {
        tree->format = USD3D_VOXEL_FORMAT_FIELD3D_HDF5;
        vt_scan_hdf5_datasets(data, size, tree);
        if (tree->grid_count == 0UL) usd3d_voxel_tree_add_grid(tree, "field3d_partition", "unknown", 0UL);
        return USD3D_OK;
    }
    if (vt_contains_at(data, size, 0UL, "VDB")) return usd3d_voxel_openvdb_scan_tree_tokens(data, size, tree);
    return USD3D_ERR_FORMAT;
}

int usd3d_voxel_tree_emit(const usd3d_voxel_tree *tree,
                          int (*grid_cb)(void *user, const usd3d_voxel_grid *grid),
                          int (*node_cb)(void *user, const usd3d_voxel_node *node),
                          void *user)
{
    unsigned long i;
    if (tree == 0) return USD3D_ERR_BAD_ARG;
    if (grid_cb != 0) {
        for (i = 0UL; i < tree->grid_count; ++i) if (tree->grids[i].active && grid_cb(user, &tree->grids[i]) != 0) return USD3D_ERR_PARSE;
    }
    if (node_cb != 0) {
        for (i = 0UL; i < tree->node_count; ++i) if (tree->nodes[i].active && node_cb(user, &tree->nodes[i]) != 0) return USD3D_ERR_PARSE;
    }
    return USD3D_OK;
}
