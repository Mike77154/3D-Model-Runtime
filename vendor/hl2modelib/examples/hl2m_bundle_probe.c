#include "hl2modelib.h"
#include <stdio.h>

#define PROBE_BUFFER_SIZE (8u * 1024u * 1024u)
static unsigned char g_mdl_bytes[PROBE_BUFFER_SIZE];
static unsigned char g_vvd_bytes[PROBE_BUFFER_SIZE];
static unsigned char g_vtx_bytes[PROBE_BUFFER_SIZE];
static hl2m_mdl g_mdl;
static hl2m_vvd g_vvd;
static hl2m_vtx g_vtx;
static hl2m_engine_model g_engine;

static unsigned int load_file(const char *path, unsigned char *buffer)
{
    FILE *f;
    size_t got;
    f = fopen(path, "rb");
    if (!f) return 0u;
    got = fread(buffer, 1u, PROBE_BUFFER_SIZE, f);
    fclose(f);
    return (unsigned int)got;
}

int main(int argc, char **argv)
{
    unsigned int smdl;
    unsigned int svvd;
    unsigned int svtx;
    int r;
    if (argc < 4) {
        printf("usage: hl2m_bundle_probe file.mdl file.vvd file.dx90.vtx\n");
        return 1;
    }
    smdl = load_file(argv[1], g_mdl_bytes);
    svvd = load_file(argv[2], g_vvd_bytes);
    svtx = load_file(argv[3], g_vtx_bytes);
    if (smdl == 0u || svvd == 0u || svtx == 0u) {
        printf("could not read one or more files\n");
        return 2;
    }
    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl);
    if (r != HL2M_OK) { printf("mdl decode=%d\n", r); return 3; }
    r = hl2m_decode_vvd(g_vvd_bytes, svvd, 0, &g_vvd);
    if (r != HL2M_OK) { printf("vvd decode=%d\n", r); return 4; }
    r = hl2m_decode_vtx(g_vtx_bytes, svtx, &g_vtx);
    if (r != HL2M_OK) { printf("vtx decode=%d\n", r); return 5; }
    r = hl2m_build_engine_model(&g_mdl, &g_vvd, &g_vtx, 0, &g_engine);
    printf("engine build=%d vertices=%d indices=%d draws=%d skipped=%d warnings=%d matrepl=%d\n",
           r, g_engine.vertex_count, g_engine.index_count, g_engine.draw_count,
           g_engine.skipped_triangle_count, g_engine.warning_flags,
           g_vtx.material_replacement_count);
    if (g_engine.draw_count > 0) {
        printf("draw0 material=%d name=%s replacement=%s\n",
               g_engine.draws[0].material,
               g_engine.draws[0].material_name,
               g_engine.draws[0].replacement_material);
    }
    if (g_engine.vertex_count > 0) {
        char x[32];
        char y[32];
        char z[32];
        hl2m_fixed_to_string(g_engine.vertices[0].position.x, x, (unsigned int)sizeof(x), 4);
        hl2m_fixed_to_string(g_engine.vertices[0].position.y, y, (unsigned int)sizeof(y), 4);
        hl2m_fixed_to_string(g_engine.vertices[0].position.z, z, (unsigned int)sizeof(z), 4);
        printf("v0 pos=%s,%s,%s material=%d src_vvd=%d src_vtx=%d\n",
               x, y, z, g_engine.vertices[0].material,
               g_engine.vertices[0].source_vvd_vertex,
               g_engine.vertices[0].source_vtx_vertex);
    }
    return r == HL2M_OK ? 0 : 6;
}
