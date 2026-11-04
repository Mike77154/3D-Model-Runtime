#include "dff3d.h"
#include "dff3d_reader.h"

static int dff3d_is_container(dff_u32 type)
{
    switch (type) {
    case DFF3D_SEC_CLUMP:
    case DFF3D_SEC_FRAME_LIST:
    case DFF3D_SEC_GEOMETRY_LIST:
    case DFF3D_SEC_GEOMETRY:
    case DFF3D_SEC_MATERIAL_LIST:
    case DFF3D_SEC_MATERIAL:
    case DFF3D_SEC_TEXTURE:
    case DFF3D_SEC_EXTENSION:
    case DFF3D_SEC_ATOMIC:
    case DFF3D_SEC_TEXTURE_DICTIONARY:
    case DFF3D_SEC_TEXTURE_NATIVE:
        return 1;
    default:
        return 0;
    }
}

static void dff3d_dump_indent(FILE *f, dff_u32 depth)
{
    dff_u32 i;
    for (i = 0U; i < depth; ++i) {
        fputs("  ", f);
    }
}

static void dff3d_dump_range(FILE *f, const Dff3dReader *base, dff_u32 start, dff_u32 end, dff_u32 depth)
{
    Dff3dReader r;
    Dff3dChunk c;
    dff_u32 version;

    if (depth > DFF3D_MAX_CHUNK_DEPTH || start > end || end > base->size) {
        return;
    }
    r = *base;
    r.pos = start;
    r.size = end;
    while (r.pos < r.size) {
        if (!dff3d_read_chunk_header(&r, &c)) {
            dff3d_dump_indent(f, depth);
            fprintf(f, "<bad chunk at %lu>\n", (unsigned long)r.pos);
            return;
        }
        version = dff3d_decode_rw_version(c.version_stamp);
        dff3d_dump_indent(f, depth);
        fprintf(f, "%s type=0x%08lx size=%lu stamp=0x%08lx rw=0x%08lx body=%lu\n",
            dff3d_chunk_name(c.type),
            (unsigned long)c.type,
            (unsigned long)c.size,
            (unsigned long)c.version_stamp,
            (unsigned long)version,
            (unsigned long)c.body);
        if (dff3d_is_container(c.type) && c.size >= 12U) {
            dff3d_dump_range(f, base, c.body, c.end, depth + 1U);
        }
        dff3d_reader_skip_to(&r, c.end);
    }
}

int dff3d_dump_chunks(FILE *f, const void *bytes, dff_u32 size)
{
    Dff3dReader r;
    if (f == DFF3D_NULL || bytes == DFF3D_NULL) {
        return DFF3D_ERR_NULL;
    }
    dff3d_reader_init(&r, bytes, size);
    dff3d_dump_range(f, &r, 0U, size, 0U);
    return DFF3D_OK;
}
