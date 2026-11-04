#include "m3d89.h"
#include <stdio.h>
#include <string.h>

#ifndef M3D89_FILE_MAX
#define M3D89_FILE_MAX (8UL * 1024UL * 1024UL)
#endif

static unsigned char g_file[M3D89_FILE_MAX];

static void print_q16(m3d89_q16 q)
{
    m3d89_i32 ip;
    m3d89_i32 fp;
    if(q < 0) {
        putchar('-');
        q = -q;
    }
    ip = q / 65536L;
    fp = ((q % 65536L) * 10000L) / 65536L;
    printf("%ld.%04ld", (long)ip, (long)fp);
}

static long read_file(const char *path)
{
    FILE *f;
    long n;
    f = fopen(path, "rb");
    if(!f) return -1;
    if(fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    n = ftell(f);
    if(n < 0 || (unsigned long)n > M3D89_FILE_MAX) { fclose(f); return -1; }
    if(fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    if(fread(g_file, 1, (size_t)n, f) != (size_t)n) { fclose(f); return -1; }
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    m3d89_model model;
    long n;
    int r;
    m3d89_u16 i;
    if(argc < 2) {
        printf("usage: m3d89_dump file.m3d\n");
        return 1;
    }
    n = read_file(argv[1]);
    if(n < 0) {
        printf("read failed or file too large\n");
        return 2;
    }
    r = m3d89_decode(&model, g_file, (m3d89_u32)n);
    if(r != M3D89_OK) {
        printf("decode error: %d\n", r);
        if(r == M3D89_ERR_STREAM_PACKED) printf("note: this file uses the packed M3D stream; feed an unpacked HEAD payload or add an inflater bridge.\n");
        return 3;
    }
    printf("M3D89 decode OK\n");
    printf("name: %s\n", m3d89_string(&model, model.name));
    printf("license: %s\n", m3d89_string(&model, model.license));
    printf("author: %s\n", m3d89_string(&model, model.author));
    printf("scale_q16: "); print_q16(model.scale_q16); printf("\n");
    printf("chunks=%u vertices=%u texcoords=%u faces=%u materials=%u bones=%u skins=%u actions=%u vox_types=%u vox_blocks=%u labels=%u assets=%u warnings=%lu\n",
        (unsigned)model.chunk_count,
        (unsigned)model.vertex_count,
        (unsigned)model.texcoord_count,
        (unsigned)model.face_count,
        (unsigned)model.material_count,
        (unsigned)model.bone_count,
        (unsigned)model.skin_count,
        (unsigned)model.action_count,
        (unsigned)model.vox_type_count,
        (unsigned)model.vox_block_count,
        (unsigned)model.label_count,
        (unsigned)model.asset_count,
        (unsigned long)model.warnings);
    for(i = 0; i < model.vertex_count && i < 8; i++) {
        printf("v[%u] = ", (unsigned)i);
        print_q16(model.vertices[i].x); printf(", ");
        print_q16(model.vertices[i].y); printf(", ");
        print_q16(model.vertices[i].z); printf(" rgba=%08lX\n", (unsigned long)model.vertices[i].rgba);
    }
    for(i = 0; i < model.face_count && i < 8; i++) {
        printf("f[%u] = %lu %lu %lu flags=%u material=%s\n",
            (unsigned)i,
            (unsigned long)model.faces[i].vertex[0],
            (unsigned long)model.faces[i].vertex[1],
            (unsigned long)model.faces[i].vertex[2],
            (unsigned)model.faces[i].flags,
            m3d89_string(&model, model.faces[i].material_name));
    }
    return 0;
}
