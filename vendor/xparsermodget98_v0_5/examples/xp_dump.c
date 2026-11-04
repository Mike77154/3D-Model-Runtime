/* xp_dump.c - example tool (prints parse events)
   MIT License
*/
#include <stdio.h>
#include <string.h>
#include "xp.h"

static void on_begin(void* u, const char* type, const char* name) {
    (void)u;
    printf("[BEGIN] %s %s\n", type, (name && name[0]) ? name : "");
}
static void on_end(void* u, const char* type, const char* name) {
    (void)u;
    printf("[END]   %s %s\n", type, (name && name[0]) ? name : "");
}
static void on_mesh_begin(void* u, const char* name, int nV, int nF) {
    (void)u;
    printf("  Mesh '%s': nV=%d nF=%d\n", name?name:"", nV, nF);
}
static void on_mesh_face_count(void* u, int nF) {
    (void)u;
    printf("  Mesh face count: nF=%d\n", nF);
}
static void print_fx(xp_fx32 v) {
    long whole;
    long frac;
    if (v < 0) {
        putchar('-');
        if (v == XP_FX_MIN) v = XP_FX_MAX;
        else v = -v;
    }
    whole = (long)(v >> XP_FX_SHIFT);
    frac = (long)((v & (XP_FX_ONE - 1L)) * 10000L / XP_FX_ONE);
    printf("%ld.%04ld", whole, frac);
}

static void on_v(void* u, int i, xp_fx32 x, xp_fx32 y, xp_fx32 z) {
    (void)u;
    if (i < 3) {
        printf("    v[%d]=(", i);
        print_fx(x); printf(","); print_fx(y); printf(","); print_fx(z); printf(")\n");
    }
}
static void on_f(void* u, int i, int n, const int* idx) {
    (void)u;
    if (i < 3) {
        int j;
        printf("    f[%d] deg=%d idx=", i, n);
        for (j=0;j<n;++j) printf("%d%s", idx[j], (j+1<n)?",":"");
        printf("\n");
    }
}
static void on_ref(void* u, const char* name) {
    (void)u;
    printf("  ref -> %s\n", name ? name : "");
}
static void on_anim_target(void* u, const char* name) {
    (void)u;
    printf("  animation target -> %s\n", name ? name : "");
}
static void on_ticks(void* u, unsigned int ticks) {
    (void)u;
    printf("  animation ticks/sec = %u\n", ticks);
}
static void on_skin_header(void* u, unsigned int pv, unsigned int pf, unsigned int bones) {
    (void)u;
    printf("  skin header: perVertex=%u perFace=%u bones=%u\n", pv, pf, bones);
}
static void on_err(void* u, int line, int col, const char* msg) {
    (void)u;
    fprintf(stderr, "ERROR at %d:%d: %s\n", line, col, msg);
}

int main(int argc, char** argv) {
    FILE* f;
    xp_context ctx;
    xp_header hdr;
    xp_result r;
    xp_callbacks cb;

    if (argc < 2) {
        fprintf(stderr, "usage: %s file.x\n", argv[0]);
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 2; }

    r = xp_open_file(&ctx, f, &hdr);
    if (r != XP_OK) {
        fprintf(stderr, "xp_open_file: %s\n", xp_result_str(r));
        fclose(f);
        return 2;
    }

    memset(&cb, 0, sizeof(cb));
    cb.on_begin_object = on_begin;
    cb.on_end_object = on_end;
    cb.on_data_reference = on_ref;
    cb.on_animation_target = on_anim_target;
    cb.on_anim_ticks_per_second = on_ticks;
    cb.on_skin_mesh_header = on_skin_header;
    cb.on_mesh_begin = on_mesh_begin;
    cb.on_mesh_face_count = on_mesh_face_count;
    cb.on_vertex = on_v;
    cb.on_face = on_f;
    cb.on_error = on_err;

    printf("Header: xof %d%d enc=%d num=%d\n", hdr.major, hdr.minor, (int)hdr.enc, (int)hdr.numfmt);

    r = xp_parse(&ctx, &cb, 0);
    if (r != XP_OK) fprintf(stderr, "xp_parse: %s\n", xp_result_str(r));

    xp_close(&ctx);
    fclose(f);
    return (r==XP_OK) ? 0 : 1;
}
