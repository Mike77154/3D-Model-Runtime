#include "ogre_mesh_codec.h"
#include <stdio.h>
#include <string.h>

#ifndef OMC_TOOL_BUFFER_SIZE
#define OMC_TOOL_BUFFER_SIZE (4UL * 1024UL * 1024UL)
#endif

static unsigned char g_file_buffer[OMC_TOOL_BUFFER_SIZE];
static OMC_Vertex g_frame_vertices[OMC_MAX_VERTICES];

static void print_usage(void)
{
    printf("ogre_mesh_codec_c89 tool\n");
    printf("usage:\n");
    printf("  omc_tool info input.mesh.xml|input.mesh\n");
    printf("  omc_tool validate input.mesh.xml|input.mesh\n");
    printf("  omc_tool xml  input.mesh.xml|input.mesh output.mesh.xml\n");
    printf("  omc_tool bin  input.mesh.xml|input.mesh output.mesh\n");
    printf("  omc_tool obj  input.mesh.xml|input.mesh output.obj\n");
    printf("  omc_tool upgrade input.mesh.xml|input.mesh output.mesh.xml\n");
    printf("  omc_tool skinblend input.mesh.xml|input.mesh output.mesh\n");
    printf("  omc_tool triangulate input.mesh.xml|input.mesh output.mesh.xml|output.mesh\n");
    printf("  omc_tool cleanup input.mesh.xml|input.mesh output.mesh.xml|output.mesh\n");
    printf("  omc_tool rewind input.mesh.xml|input.mesh output.mesh.xml|output.mesh\n");
    printf("  omc_tool compact input.mesh.xml|input.mesh output.mesh.xml|output.mesh\n");
    printf("  omc_tool weld input.mesh.xml|input.mesh output.mesh.xml|output.mesh\n");
    printf("  omc_tool mergeuv input.mesh.xml|input.mesh dst src output.mesh.xml\n");
    printf("  omc_tool frame input.mesh.xml|input.mesh animation time output.obj\n");
    printf("  omc_tool framexml input.mesh.xml|input.mesh animation time output.mesh.xml\n");
}

static void print_fixed(OMC_Fixed v)
{
    char b[48];
    omc_fixed_to_string(v, b, sizeof(b), 5);
    printf("%s", b);
}

static void print_info(const OMC_Mesh *m)
{
    unsigned int i;
    unsigned int shown;
    unsigned long blend_weighted;
    unsigned long blend_indexed;
    printf("format: %s\n", m->source_format);
    if (m->binary_version[0]) printf("binary_version: %s\n", m->binary_version);
    if (m->skeleton_name[0]) printf("skeleton: %s\n", m->skeleton_name);
    printf("vertices: %lu\n", (unsigned long)m->vertex_count);
    blend_weighted = 0UL;
    blend_indexed = 0UL;
    i = 0U;
    while ((unsigned long)i < (unsigned long)m->vertex_count) {
        if ((m->vertices[i].flags & OMC_VTX_HAS_BLEND_WEIGHTS) != 0U) blend_weighted++;
        if ((m->vertices[i].flags & OMC_VTX_HAS_BLEND_INDICES) != 0U) blend_indexed++;
        i++;
    }
    if (blend_weighted || blend_indexed) printf("blend_vertices: weights=%lu indices=%lu\n", blend_weighted, blend_indexed);
    printf("shared_vertices: %lu\n", (unsigned long)m->shared_vertex_count);
    printf("indices: %lu\n", (unsigned long)m->index_count);
    printf("triangles: %lu\n", omc_count_triangles(m));
    printf("submeshes: %u\n", (unsigned int)m->submesh_count);
    printf("texture_aliases: %lu\n", (unsigned long)m->texture_alias_count);
    printf("mesh_bone_assignments: %lu\n", (unsigned long)m->bone_assignment_count);
    printf("submesh_bone_assignments: %lu\n", (unsigned long)m->submesh_bone_assignment_count);
    printf("poses: %u pose_offsets: %lu\n", (unsigned int)m->pose_count, (unsigned long)m->pose_offset_count);
    printf("animations: %u tracks: %lu pose_keys: %lu morph_keys: %lu morph_vertices: %lu\n",
           (unsigned int)m->animation_count,
           (unsigned long)m->track_count,
           (unsigned long)m->key_count,
           (unsigned long)m->morph_key_count,
           (unsigned long)m->morph_vertex_count);
    printf("lod: levels=%u faces=%lu strategy='%s' manual=%u declared=%u\n",
           (unsigned int)m->lod_level_count,
           (unsigned long)m->lod_face_count,
           m->lod_strategy,
           (unsigned int)m->lod_manual,
           (unsigned int)m->lod_declared_levels);
    printf("extremes: %lu\n", (unsigned long)m->extreme_count);
    printf("edges: %lu\n", (unsigned long)m->edge_count);
    i = 0U;
    while (i < (unsigned int)m->animation_count) {
        printf("animation[%u]: name='%s' tracks=%lu baseinfo=%u base='%s'\n",
               i,
               m->animations[i].name,
               (unsigned long)m->animations[i].track_count,
               (unsigned int)m->animations[i].has_baseinfo,
               m->animations[i].base_animation_name);
        i++;
    }
    printf("binary_chunks: %u\n", (unsigned int)m->binary_chunk_count);
    shown = (unsigned int)m->binary_chunk_count;
    if (shown > 16U) shown = 16U;
    i = 0U;
    while (i < shown) {
        printf("binary_chunk[%u]: id=0x%04X start=%lu size=%lu depth=%u\n",
               i,
               (unsigned int)m->binary_chunks[i].id,
               (unsigned long)m->binary_chunks[i].start,
               (unsigned long)m->binary_chunks[i].size,
               (unsigned int)m->binary_chunks[i].depth);
        i++;
    }
    if (m->has_bounds) {
        printf("bounds_min: "); print_fixed(m->bounds_min.x); printf(" "); print_fixed(m->bounds_min.y); printf(" "); print_fixed(m->bounds_min.z); printf("\n");
        printf("bounds_max: "); print_fixed(m->bounds_max.x); printf(" "); print_fixed(m->bounds_max.y); printf(" "); print_fixed(m->bounds_max.z); printf("\n");
        printf("bounds_radius: "); print_fixed(m->bounds_radius); printf("\n");
    }
    i = 0U;
    while (i < (unsigned int)m->submesh_count) {
        printf("submesh[%u]: material='%s' name='%s' indices=%lu vertices=%lu shared=%u op=%u\n",
               i,
               m->submeshes[i].material,
               m->submeshes[i].name,
               (unsigned long)m->submeshes[i].index_count,
               (unsigned long)m->submeshes[i].vertex_count,
               (unsigned int)m->submeshes[i].use_shared_vertices,
               (unsigned int)m->submeshes[i].operation_type);
        i++;
    }
    i = 0U;
    while (i < (unsigned int)m->texture_alias_count) {
        printf("texture_alias[%u]: submesh=%u alias='%s' texture='%s'\n",
               i,
               (unsigned int)m->texture_aliases[i].submesh_index,
               m->texture_aliases[i].alias,
               m->texture_aliases[i].texture);
        i++;
    }
    i = 0U;
    while (i < (unsigned int)m->diagnostic_count) {
        printf("diagnostic[%u]: %d %s\n", i, m->diagnostics[i].code, m->diagnostics[i].message);
        i++;
    }
}


static OMC_U8 parse_u8_arg(const char *s)
{
    unsigned long v;
    v = 0UL;
    if (!s) return 0U;
    while (*s >= '0' && *s <= '9') {
        v = v * 10UL + (unsigned long)(*s - '0');
        if (v > 255UL) return 255U;
        s++;
    }
    return (OMC_U8)v;
}


static int has_suffix(const char *s, const char *suffix)
{
    unsigned long sl;
    unsigned long xl;

    if (!s || !suffix) return 0;
    sl = (unsigned long)strlen(s);
    xl = (unsigned long)strlen(suffix);
    if (xl > sl) return 0;
    return strcmp(s + sl - xl, suffix) == 0;
}

static int save_auto_output(const OMC_Mesh *mesh, const char *path)
{
    if (!mesh || !path) return OMC_ERR_NULL;
    if (has_suffix(path, ".mesh") && !has_suffix(path, ".mesh.xml")) return omc_save_mesh_binary_file(mesh, path);
    return omc_save_mesh_xml_file(mesh, path);
}

int main(int argc, char **argv)
{
    OMC_Mesh mesh;
    int r;
    unsigned long frame_count;
    OMC_U32 i;

    if (argc < 3) {
        print_usage();
        return 1;
    }

    r = omc_load_file_auto(&mesh, argv[2], g_file_buffer, OMC_TOOL_BUFFER_SIZE);
    if (r != OMC_OK) {
        printf("load failed: %s (%d)\n", omc_result_string(r), r);
        return 2;
    }

    if (strcmp(argv[1], "info") == 0) {
        print_info(&mesh);
        return 0;
    }
    if (strcmp(argv[1], "validate") == 0) {
        r = omc_validate_mesh(&mesh);
        if (r != OMC_OK) { printf("invalid: %s (%d)\n", omc_result_string(r), r); return 3; }
        printf("valid %s\n", argv[2]);
        return 0;
    }
    if (strcmp(argv[1], "xml") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_save_mesh_xml_file(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save xml failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "bin") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_save_mesh_binary_file(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save binary failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote binary %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "obj") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_export_obj_file(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save obj failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "skinblend") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_build_blend_elements_from_bone_assignments(&mesh, 4U);
        if (r != OMC_OK) { printf("skinblend failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_save_mesh_binary_file(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save skinblend binary failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote skinblend binary %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "triangulate") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_triangulate_primitives(&mesh);
        if (r != OMC_OK) { printf("triangulate failed: %s\n", omc_result_string(r)); return 3; }
        omc_compute_bounds(&mesh);
        r = save_auto_output(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save triangulated failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote triangulated %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "cleanup") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_remove_degenerate_faces(&mesh);
        if (r != OMC_OK) { printf("cleanup failed: %s\n", omc_result_string(r)); return 3; }
        omc_compute_bounds(&mesh);
        r = save_auto_output(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save cleaned mesh failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote cleaned %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "rewind") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_reverse_winding(&mesh);
        if (r != OMC_OK) { printf("rewind failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_flip_normals(&mesh);
        if (r != OMC_OK) { printf("flip normals failed: %s\n", omc_result_string(r)); return 3; }
        omc_compute_bounds(&mesh);
        r = save_auto_output(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save rewound mesh failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote rewound %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "compact") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_compact_vertices(&mesh);
        if (r != OMC_OK) { printf("compact failed: %s\n", omc_result_string(r)); return 3; }
        r = save_auto_output(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save compacted mesh failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote compacted %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "weld") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_weld_vertices_exact(&mesh);
        if (r != OMC_OK) { printf("weld failed: %s\n", omc_result_string(r)); return 3; }
        r = save_auto_output(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save welded mesh failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote welded %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "upgrade") == 0) {
        if (argc < 4) { print_usage(); return 1; }
        r = omc_remove_degenerate_faces(&mesh);
        if (r != OMC_OK) { printf("cleanup failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_compact_vertices(&mesh);
        if (r != OMC_OK && r != OMC_ERR_UNSUPPORTED) { printf("compact failed: %s\n", omc_result_string(r)); return 3; }
        omc_compute_bounds(&mesh);
        r = omc_recalculate_normals(&mesh);
        if (r != OMC_OK) { printf("normal generation failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_generate_tangents(&mesh, 0U, 4U);
        if (r != OMC_OK) { printf("tangent generation failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_generate_extremes(&mesh, 6U);
        if (r != OMC_OK) { printf("extreme generation failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_build_edge_list(&mesh);
        if (r != OMC_OK) { printf("edge build failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_save_mesh_xml_file(&mesh, argv[3]);
        if (r != OMC_OK) { printf("save upgraded xml failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote upgraded %s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "mergeuv") == 0) {
        if (argc < 6) { print_usage(); return 1; }
        r = omc_merge_texcoord_sets(&mesh, parse_u8_arg(argv[3]), parse_u8_arg(argv[4]));
        if (r != OMC_OK) { printf("mergeuv failed: %s\n", omc_result_string(r)); return 3; }
        r = omc_save_mesh_xml_file(&mesh, argv[5]);
        if (r != OMC_OK) { printf("save merged xml failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote merged %s\n", argv[5]);
        return 0;
    }
    if (strcmp(argv[1], "frame") == 0) {
        if (argc < 6) { print_usage(); return 1; }
        frame_count = 0UL;
        r = omc_make_animation_frame(&mesh, argv[3], omc_fixed_from_decimal(argv[4]), g_frame_vertices, OMC_MAX_VERTICES, &frame_count);
        if (r != OMC_OK) { printf("frame bake failed: %s\n", omc_result_string(r)); return 3; }
        i = 0UL;
        while (i < frame_count && i < mesh.vertex_count) {
            mesh.vertices[i] = g_frame_vertices[i];
            i++;
        }
        r = omc_export_obj_file(&mesh, argv[5]);
        if (r != OMC_OK) { printf("save frame obj failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote frame %s\n", argv[5]);
        return 0;
    }
    if (strcmp(argv[1], "framexml") == 0) {
        if (argc < 6) { print_usage(); return 1; }
        frame_count = 0UL;
        r = omc_make_animation_frame(&mesh, argv[3], omc_fixed_from_decimal(argv[4]), g_frame_vertices, OMC_MAX_VERTICES, &frame_count);
        if (r != OMC_OK) { printf("framexml bake failed: %s\n", omc_result_string(r)); return 3; }
        i = 0UL;
        while (i < frame_count && i < mesh.vertex_count) {
            mesh.vertices[i] = g_frame_vertices[i];
            i++;
        }
        mesh.pose_count = 0U;
        mesh.pose_offset_count = 0UL;
        mesh.animation_count = 0U;
        mesh.track_count = 0UL;
        mesh.key_count = 0UL;
        mesh.morph_key_count = 0UL;
        mesh.morph_vertex_count = 0UL;
        omc_compute_bounds(&mesh);
        r = omc_save_mesh_xml_file(&mesh, argv[5]);
        if (r != OMC_OK) { printf("save frame xml failed: %s\n", omc_result_string(r)); return 3; }
        printf("wrote frame xml %s\n", argv[5]);
        return 0;
    }

    print_usage();
    return 1;
}
