#include "tmd_codec3d.h"
#include <stdio.h>

#ifndef TMD_DUMP_MAX_FILE
#define TMD_DUMP_MAX_FILE (4UL * 1024UL * 1024UL)
#endif
#ifndef TMD_DUMP_MAX_OBJECTS
#define TMD_DUMP_MAX_OBJECTS 256UL
#endif
#ifndef TMD_DUMP_MAX_FACES
#define TMD_DUMP_MAX_FACES 65536UL
#endif

static unsigned char g_file[TMD_DUMP_MAX_FILE];
static TMD_Object g_objects[TMD_DUMP_MAX_OBJECTS];
static TMD_Face g_faces[TMD_DUMP_MAX_FACES];

static int load_file(const char *path, unsigned char *buf, unsigned long cap, unsigned long *out_size)
{
    FILE *f;
    unsigned long n;
    int c;

    if (path == 0 || buf == 0 || out_size == 0) return 0;
    f = fopen(path, "rb");
    if (f == 0) return 0;

    n = 0UL;
    while ((c = fgetc(f)) != EOF) {
        if (n >= cap) {
            fclose(f);
            return 0;
        }
        buf[n++] = (unsigned char)c;
    }
    fclose(f);
    *out_size = n;
    return 1;
}

static void print_face(const TMD_Face *f)
{
    unsigned int i;
    printf("  face off=0x%08lx mode=0x%02x flag=0x%02x vc=%u attrs=0x%04x v=",
           (unsigned long)f->packet_offset,
           (unsigned int)f->mode,
           (unsigned int)f->packet_flag,
           (unsigned int)f->vertex_count,
           (unsigned int)f->attrs);
    for (i = 0U; i < (unsigned int)f->vertex_count; ++i) {
        printf("%s%u", (i == 0U) ? "" : ",", (unsigned int)f->v[i]);
    }
    if ((f->attrs & TMD_FACEF_TEXTURED) != 0U) {
        printf(" uv=");
        for (i = 0U; i < (unsigned int)f->vertex_count; ++i) {
            printf("%s%u:%u", (i == 0U) ? "" : ",", (unsigned int)f->uv[i].u, (unsigned int)f->uv[i].v);
        }
        printf(" cba=0x%04x tsb=0x%04x", (unsigned int)f->cba, (unsigned int)f->tsb);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    TMD_Context ctx;
    unsigned long size;
    tmd_u32 i;
    int r;

    if (argc < 2) {
        printf("usage: tmd_dump file.tmd\n");
        return 2;
    }

    if (!load_file(argv[1], g_file, TMD_DUMP_MAX_FILE, &size)) {
        printf("could not load file or file exceeds static buffer (%lu bytes)\n", (unsigned long)TMD_DUMP_MAX_FILE);
        return 1;
    }

    r = tmd_parse(&ctx, g_file, (tmd_u32)size, g_objects, TMD_DUMP_MAX_OBJECTS);
    if (r != TMD_ERR_OK) {
        printf("parse error: %s (%d)\n", tmd_result_string(r), r);
        return 1;
    }

    printf("TMD id=0x%08lx flags=0x%08lx objects=%lu warnings=0x%08lx\n",
           (unsigned long)ctx.header.id,
           (unsigned long)ctx.header.flags,
           (unsigned long)ctx.header.object_count,
           (unsigned long)ctx.warnings);

    for (i = 0UL; i < ctx.parsed_object_count; ++i) {
        tmd_u32 written;
        tmd_u32 needed;
        tmd_u32 unsupported;
        tmd_u32 j;
        TMD_Object *o;
        o = ctx.objects + i;
        printf("object %lu: verts=%lu normals=%lu prims=%lu scale_pow=%ld offs(v/n/p)=0x%lx/0x%lx/0x%lx\n",
               (unsigned long)i,
               (unsigned long)o->n_vert,
               (unsigned long)o->n_normal,
               (unsigned long)o->n_primitive,
               (long)o->scale_power,
               (unsigned long)o->vert_offset,
               (unsigned long)o->normal_offset,
               (unsigned long)o->primitive_offset);
        r = tmd_decode_object_faces(&ctx, i, g_faces, TMD_DUMP_MAX_FACES, &written, &needed, &unsupported);
        if (r != TMD_ERR_OK && r != TMD_ERR_OUTPUT_OVERFLOW) {
            printf(" decode error: %s (%d)\n", tmd_result_string(r), r);
            continue;
        }
        printf(" faces written=%lu needed=%lu unsupported_nonpolygon=%lu\n",
               (unsigned long)written, (unsigned long)needed, (unsigned long)unsupported);
        for (j = 0UL; j < written && j < 16UL; ++j) {
            print_face(g_faces + j);
        }
        if (written > 16UL) printf("  ... %lu more faces omitted\n", (unsigned long)(written - 16UL));
    }

    return 0;
}
