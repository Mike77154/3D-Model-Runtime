#include "fbx_gackt.h"

#include <string.h>

static fbx_u32 gackt_rd_u32(const unsigned char *p)
{
    return ((fbx_u32)p[0]) | ((fbx_u32)p[1] << 8) | ((fbx_u32)p[2] << 16) | ((fbx_u32)p[3] << 24);
}

static int gackt_magic_ok(const unsigned char *p)
{
    return p[0] == 'G' && p[1] == 'A' && p[2] == 'C' && p[3] == 'K' &&
           p[4] == 'T' && p[5] == '3' && p[6] == 'D' && p[7] == 0;
}

int fbx_gackt_validate_memory(const void *data, fbx_u32 size, FbxGacktInfo *out_info)
{
    const unsigned char *p;
    fbx_u32 version;
    fbx_u32 header_size;
    fbx_u32 file_size;
    fbx_u32 mesh_count;
    fbx_u32 mesh_table_offset;

    if (!data) return FBX_GACKT_ERR_ARGUMENT;
    if (size < FBX_GACKT_HEADER_SIZE) return FBX_GACKT_ERR_FORMAT;

    p = (const unsigned char*)data;
    if (!gackt_magic_ok(p)) return FBX_GACKT_ERR_FORMAT;

    version = gackt_rd_u32(p + 8u);
    header_size = gackt_rd_u32(p + 12u);
    file_size = gackt_rd_u32(p + 44u);
    mesh_count = gackt_rd_u32(p + 20u);
    mesh_table_offset = gackt_rd_u32(p + 32u);

    if (version != FBX_GACKT_VERSION) return FBX_GACKT_ERR_FORMAT;
    if (header_size != FBX_GACKT_HEADER_SIZE) return FBX_GACKT_ERR_FORMAT;
    if (file_size > size || file_size < FBX_GACKT_HEADER_SIZE) return FBX_GACKT_ERR_FORMAT;
    if (mesh_table_offset < FBX_GACKT_HEADER_SIZE) return FBX_GACKT_ERR_FORMAT;
    if (mesh_count > 0u) {
        fbx_u32 bytes;
        if (mesh_count > (fbx_u32)0xffffffffu / FBX_GACKT_MESH_RECORD_SIZE) return FBX_GACKT_ERR_FORMAT;
        bytes = mesh_count * FBX_GACKT_MESH_RECORD_SIZE;
        if (bytes > file_size || mesh_table_offset > file_size - bytes) return FBX_GACKT_ERR_FORMAT;
    }

    if (out_info) {
        out_info->version = version;
        out_info->flags = gackt_rd_u32(p + 16u);
        out_info->mesh_count = mesh_count;
        out_info->total_vertex_count = gackt_rd_u32(p + 24u);
        out_info->total_index_count = gackt_rd_u32(p + 28u);
        out_info->file_size = file_size;
    }

    return FBX_GACKT_OK;
}

const char *fbx_gackt_error_string(int code)
{
    switch (code) {
        case FBX_GACKT_OK: return "OK";
        case FBX_GACKT_ERR_ARGUMENT: return "bad argument";
        case FBX_GACKT_ERR_LIMIT: return "limit/overflow";
        case FBX_GACKT_ERR_CAPACITY: return "destination buffer too small";
        case FBX_GACKT_ERR_FORMAT: return "bad GACKT mesh format";
        default: break;
    }
    return "unknown GACKT error";
}
