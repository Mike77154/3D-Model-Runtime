#include "usd3d_volume_metadata.h"

#include <string.h>
#include <ctype.h>

static void vm_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int vm_contains(const unsigned char *data, unsigned long size, const char *needle)
{
    unsigned long i;
    unsigned long n;
    if (data == 0 || needle == 0) return 0;
    n = (unsigned long)strlen(needle);
    if (n == 0UL || size < n) return 0;
    for (i = 0UL; i + n <= size; ++i) {
        if (memcmp(data + i, needle, (size_t)n) == 0) return 1;
    }
    return 0;
}

void usd3d_volume_metadata_init(usd3d_volume_metadata *m)
{
    if (m == 0) return;
    memset(m, 0, sizeof(*m));
    usd3d_volume_probe_init(&m->probe);
}

static int vm_add_grid(usd3d_volume_metadata *m, const char *name, const char *type, unsigned long offset)
{
    usd3d_volume_grid_meta *g;
    if (m == 0 || name == 0) return USD3D_ERR_BAD_ARG;
    if (m->grid_count >= (unsigned long)USD3D_VOLUME_META_MAX_GRIDS) return USD3D_ERR_LIMIT;
    g = &m->grids[m->grid_count++];
    vm_copy(g->name, sizeof(g->name), name);
    vm_copy(g->value_type, sizeof(g->value_type), type);
    g->offset = offset;
    g->active = 1;
    return USD3D_OK;
}

static void vm_scan_grid_names(const unsigned char *data, unsigned long size, usd3d_volume_metadata *m)
{
    static const char *known[] = { "density", "temperature", "flame", "vel", "velocity", "color", "fog", "surface", "levelset", 0 };
    unsigned int k;
    unsigned long i;
    unsigned long n;
    for (k = 0U; known[k] != 0; ++k) {
        n = (unsigned long)strlen(known[k]);
        for (i = 0UL; i + n <= size; ++i) {
            if (memcmp(data + i, known[k], (size_t)n) == 0) {
                vm_add_grid(m, known[k], "unknown/sample-grid-token", i);
                break;
            }
        }
    }
}

int usd3d_volume_metadata_scan_memory(const unsigned char *data, unsigned long size, usd3d_volume_metadata *m)
{
    int r;
    if (data == 0 || m == 0) return USD3D_ERR_BAD_ARG;
    usd3d_volume_metadata_init(m);
    r = usd3d_volume_probe_memory(data, size, &m->probe);
    if (r != USD3D_OK) return r;
    if (size >= 16UL && data[0] == 0x89U && data[1] == 'H' && data[2] == 'D' && data[3] == 'F') {
        m->hdf5_superblock_version = (unsigned long)data[8];
        m->hdf5_offset_size = (unsigned long)data[13];
        m->hdf5_length_size = (unsigned long)data[14];
    }
    if (vm_contains(data, size, "fileVersion") || vm_contains(data, size, "OPENVDB")) m->has_openvdb_file_version_token = 1;
    if (vm_contains(data, size, "partition") || vm_contains(data, size, "Field3D")) m->has_field3d_partition_token = 1;
    vm_scan_grid_names(data, size, m);
    if (m->grid_count == 0UL && m->probe.kind == USD3D_VOLUME_KIND_OPENVDB) vm_add_grid(m, "unknown_openvdb_grid", "unknown", 0UL);
    if (m->grid_count == 0UL && m->probe.kind == USD3D_VOLUME_KIND_FIELD3D) vm_add_grid(m, "unknown_field3d_partition", "unknown", 0UL);
    return USD3D_OK;
}

int usd3d_volume_metadata_emit(const usd3d_volume_metadata *m,
                               int (*grid_cb)(void *user, const usd3d_volume_grid_meta *grid),
                               void *user)
{
    unsigned long i;
    if (m == 0 || grid_cb == 0) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < m->grid_count; ++i) {
        if (m->grids[i].active) {
            if (grid_cb(user, &m->grids[i]) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}
