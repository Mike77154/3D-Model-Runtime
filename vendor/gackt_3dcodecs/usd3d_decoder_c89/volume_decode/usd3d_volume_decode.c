#include "usd3d_volume_decode.h"

#include <stdio.h>
#include <string.h>

static void vd_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void usd3d_volume_probe_init(usd3d_volume_probe *probe)
{
    if (probe == 0) return;
    memset(probe, 0, sizeof(*probe));
    probe->kind = USD3D_VOLUME_KIND_UNKNOWN;
    vd_copy(probe->format_name, sizeof(probe->format_name), "unknown");
}

const char *usd3d_volume_kind_name(int kind)
{
    switch (kind) {
    case USD3D_VOLUME_KIND_OPENVDB: return "OpenVDB";
    case USD3D_VOLUME_KIND_FIELD3D: return "Field3D";
    case USD3D_VOLUME_KIND_HDF5: return "HDF5";
    default: return "unknown";
    }
}

static int vd_mem_contains(const unsigned char *data, unsigned long size, const char *needle)
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

int usd3d_volume_probe_memory(const unsigned char *data, unsigned long size, usd3d_volume_probe *probe)
{
    unsigned long i;
    unsigned long tokens;
    if (data == 0 || probe == 0) return USD3D_ERR_BAD_ARG;
    usd3d_volume_probe_init(probe);
    probe->header_bytes = size;
    if (size >= 8UL && data[0] == 0x89U && data[1] == 'H' && data[2] == 'D' && data[3] == 'F' && data[4] == 0x0dU && data[5] == 0x0aU && data[6] == 0x1aU && data[7] == 0x0aU) {
        probe->has_hdf5_signature = 1;
        probe->kind = USD3D_VOLUME_KIND_HDF5;
        vd_copy(probe->format_name, sizeof(probe->format_name), "HDF5/Field3D-candidate");
    }
    if (vd_mem_contains(data, size, "OpenVDB") || vd_mem_contains(data, size, "openvdb") || vd_mem_contains(data, size, "VDB")) {
        probe->has_openvdb_marker = 1;
        probe->kind = USD3D_VOLUME_KIND_OPENVDB;
        vd_copy(probe->format_name, sizeof(probe->format_name), "OpenVDB-marker");
    }
    if (vd_mem_contains(data, size, "Field3D") || vd_mem_contains(data, size, "field3d")) {
        probe->kind = USD3D_VOLUME_KIND_FIELD3D;
        vd_copy(probe->format_name, sizeof(probe->format_name), "Field3D/HDF5-marker");
    }
    tokens = 0UL;
    for (i = 0UL; i < size; ++i) {
        if (data[i] >= 32U && data[i] <= 126U) tokens++;
    }
    probe->printable_tokens = tokens;
    return USD3D_OK;
}

int usd3d_volume_probe_file(const char *path, usd3d_volume_probe *probe)
{
    FILE *fp;
    unsigned char buf[4096];
    size_t got;
    int r;
    if (path == 0 || probe == 0) return USD3D_ERR_BAD_ARG;
    fp = fopen(path, "rb");
    if (fp == 0) return USD3D_ERR_IO;
    got = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    r = usd3d_volume_probe_memory(buf, (unsigned long)got, probe);
    return r;
}
