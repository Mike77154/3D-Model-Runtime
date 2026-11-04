#include "chiqui_amf.h"
#include <stdio.h>

#define AMF_DUMP_INPUT_CAP (4u * 1024u * 1024u)
#define AMF_DUMP_ARENA_CAP (8u * 1024u * 1024u)
#define AMF_DUMP_TEMP_CAP  (4u * 1024u * 1024u)

static unsigned char input_mem[AMF_DUMP_INPUT_CAP];
static unsigned char arena_mem[AMF_DUMP_ARENA_CAP];
static unsigned char temp_mem[AMF_DUMP_TEMP_CAP];

static void print_fx(const char *name, amf_fx v)
{
    char buf[48];
    size_t n;
    if (amf_fx_to_text(v, buf, sizeof(buf), &n) == AMF_OK) {
        printf("%s%s", name, buf);
    } else {
        printf("%s?", name);
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    amf_arena arena;
    amf_arena temp;
    amf_document doc;
    amf_diag diag;
    amf_stats stats;
    amf_topology_report report;
    int r;
    if (argc < 2) {
        printf("usage: amf_dump file.amf\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (f == 0) {
        printf("could not open %s\n", argv[1]);
        return 2;
    }
    n = fread(input_mem, 1u, sizeof(input_mem), f);
    fclose(f);
    if (n == sizeof(input_mem)) {
        printf("input too large for static tool buffer\n");
        return 2;
    }
    amf_arena_init(&arena, arena_mem, sizeof(arena_mem));
    r = amf_parse_memory(&doc, &arena, input_mem, n, 0, &diag);
    if (r != AMF_OK) {
        printf("error %s at line %lu col %lu: %s\n", amf_error_name(r), (unsigned long)diag.line, (unsigned long)diag.column, diag.message);
        return 1;
    }
    printf("AMF unit=%s objects=%lu materials=%lu textures=%lu constellations=%lu vertices=%lu volumes=%lu triangles=%lu metadata=%lu composites=%lu\n",
        amf_unit_name(doc.unit),
        (unsigned long)doc.object_count,
        (unsigned long)doc.material_count,
        (unsigned long)doc.texture_count,
        (unsigned long)doc.constellation_count,
        (unsigned long)doc.vertex_count,
        (unsigned long)doc.volume_count,
        (unsigned long)doc.triangle_count,
        (unsigned long)doc.metadata_count,
        (unsigned long)doc.composite_count);
    r = amf_compute_stats(&doc, AMF_ALL_OBJECTS, &stats);
    if (r == AMF_OK) {
        print_fx("bbox_min=", stats.bbox_min.x); print_fx(",", stats.bbox_min.y); print_fx(",", stats.bbox_min.z); printf(" ");
        print_fx("bbox_max=", stats.bbox_max.x); print_fx(",", stats.bbox_max.y); print_fx(",", stats.bbox_max.z); printf("\n");
        print_fx("surface_area=", stats.surface_area); print_fx(" signed_volume=", stats.signed_volume);
        printf(" degenerate=%lu\n", (unsigned long)stats.degenerate_triangle_count);
    }
    amf_arena_init(&temp, temp_mem, sizeof(temp_mem));
    r = amf_validate_deep(&doc, &temp, AMF_VALIDATE_ALL, &report, &diag);
    printf("topology flags=%lu edges=%lu boundary=%lu nonmanifold=%lu duplicate_triangles=%lu missing_material=%lu missing_object=%lu\n",
        (unsigned long)report.flags,
        (unsigned long)report.edge_count,
        (unsigned long)report.boundary_edge_count,
        (unsigned long)report.nonmanifold_edge_count,
        (unsigned long)report.duplicate_triangle_count,
        (unsigned long)report.missing_material_ref_count,
        (unsigned long)report.missing_object_ref_count);
    if (r != AMF_OK) {
        printf("deep validation note: %s: %s\n", amf_error_name(r), diag.message);
    }
    return 0;
}
