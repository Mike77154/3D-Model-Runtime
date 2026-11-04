#include "dts89.h"
#include <stdio.h>

#define DTS89_INSPECT_MAX_INPUT   (32u * 1024u * 1024u)
#define DTS89_INSPECT_WORKSPACE   (16u * 1024u * 1024u)

static unsigned char g_input[DTS89_INSPECT_MAX_INPUT];
static unsigned char g_workspace[DTS89_INSPECT_WORKSPACE];

static long file_size(FILE *f)
{
    long cur;
    long end;
    cur = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

static const char *mesh_type_name(dts89_u32 t)
{
    switch (t) {
        case DTS89_MESH_STANDARD: return "standard";
        case DTS89_MESH_SKIN: return "skin";
        case DTS89_MESH_DECAL: return "decal";
        case DTS89_MESH_SORTED: return "sorted";
        case DTS89_MESH_NULL: return "null";
        default: return "unknown";
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    long sz;
    dts89_arena arena;
    dts89_shape shape;
    dts89_s32 i;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dts\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_INSPECT_MAX_INPUT) {
        printf("input too large\n");
        fclose(f);
        return 3;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 4;
    }
    fclose(f);

    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_decode_shape(g_input, (dts89_u32)sz, &arena, &shape);
    if (rc) {
        printf("decode failed: %d\n", rc);
        return 5;
    }

    printf("read_version=%u exporter=%u\n", (unsigned)shape.header.read_version, (unsigned)shape.header.exporter_version);
    printf("nodes=%d objects=%d meshes=%d details=%d names=%d sequences=%d materials=%d tail=%u\n",
           (int)shape.num_nodes, (int)shape.num_objects, (int)shape.num_meshes,
           (int)shape.num_details, (int)shape.num_names, (int)shape.num_sequences,
           (int)shape.num_materials, (unsigned)shape.sequence_tail_bytes);

    for (i = 0; i < shape.num_materials; ++i) {
        printf("material[%d]=%s flags=0x%08X\n",
               (int)i,
               shape.materials[i].name.ptr ? shape.materials[i].name.ptr : "",
               (unsigned)shape.materials[i].flags);
    }

    for (i = 0; i < shape.num_meshes; ++i) {
        const dts89_mesh_view *m = &shape.meshes[i];
        printf("mesh[%d]: type=%s frames=%d mat_frames=%d verts=%d prim=%d idx=%d flags=0x%08X\n",
               (int)i, mesh_type_name(m->mesh_type), (int)m->num_frames, (int)m->num_mat_frames,
               (int)m->num_verts_editable, (int)m->num_primitives, (int)m->num_indices,
               (unsigned)m->flags);
    }

    for (i = 0; i < shape.num_names; ++i) {
        printf("name[%d]=%s\n", (int)i, shape.names[i].ptr ? shape.names[i].ptr : "");
    }

    return 0;
}
