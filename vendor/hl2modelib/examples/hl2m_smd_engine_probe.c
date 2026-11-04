#include "hl2modelib.h"
#include <stdio.h>

#define PROBE_BUFFER_SIZE (16u * 1024u * 1024u)
static unsigned char g_file_buffer[PROBE_BUFFER_SIZE];
static hl2m_smd g_smd;
static hl2m_engine_model g_engine;
static hl2m_qc g_qc;
static char g_qc_text[65536];

static unsigned int load_file(const char *path)
{
    FILE *f;
    size_t got;
    f = fopen(path, "rb");
    if (!f) return 0u;
    got = fread(g_file_buffer, 1u, PROBE_BUFFER_SIZE, f);
    fclose(f);
    return (unsigned int)got;
}

static void print_fixed(hl2m_fixed v)
{
    char text[32];
    hl2m_fixed_to_string(v, text, (unsigned int)sizeof(text), 4);
    printf("%s", text);
}

int main(int argc, char **argv)
{
    unsigned int size;
    unsigned int used;
    int r;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
    if (argc < 2) {
        printf("usage: hl2m_smd_engine_probe file.smd [modelname.mdl]\n");
        return 1;
    }
    size = load_file(argv[1]);
    if (size == 0u) {
        printf("could not read smd\n");
        return 2;
    }
    r = hl2m_decode_smd(g_file_buffer, size, &g_smd);
    if (r != HL2M_OK) {
        printf("smd decode=%d %s\n", r, hl2m_error_name(r));
        return 3;
    }
    r = hl2m_build_engine_model_from_smd(&g_smd, &g_engine);
    if (r != HL2M_OK) {
        printf("engine build=%d %s\n", r, hl2m_error_name(r));
        return 4;
    }
    r = hl2m_engine_model_bounds(&g_engine, &bbmin, &bbmax);
    printf("smd_engine vertices=%d indices=%d draws=%d bounds=%d ", g_engine.vertex_count, g_engine.index_count, g_engine.draw_count, r);
    if (r == HL2M_OK) {
        printf("min=");
        print_fixed(bbmin.x); printf(","); print_fixed(bbmin.y); printf(","); print_fixed(bbmin.z);
        printf(" max=");
        print_fixed(bbmax.x); printf(","); print_fixed(bbmax.y); printf(","); print_fixed(bbmax.z);
    }
    printf("\n");
    r = hl2m_make_basic_qc(&g_smd, (argc > 2) ? argv[2] : "models/hl2modelib/generated.mdl", "body", argv[1], "models/hl2modelib", 1, &g_qc);
    if (r == HL2M_OK) {
        r = hl2m_qc_write(&g_qc, g_qc_text, (unsigned int)sizeof(g_qc_text), &used);
        printf("qc_make=%d bytes=%u\n%s", r, used, g_qc_text);
    }
    return 0;
}
