#include "x3d_codec.h"
#include <stdio.h>

#define X3D_TOOL_IN_CAP (4 * 1024 * 1024)
#define X3D_TOOL_OUT_CAP (4 * 1024 * 1024)

static char g_in[X3D_TOOL_IN_CAP + 1];
static char g_out[X3D_TOOL_OUT_CAP + 1];
static x3d_scene g_scene;
static x3d_tri_mesh g_mesh;

static int read_file(const char *path, char *buf, int cap, int *len_out)
{
    FILE *f;
    int n;
    int total;
    if (!path || !buf || !len_out) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    total = 0;
    while (total < cap) {
        n = (int)fread(buf + total, 1, (unsigned int)(cap - total), f);
        if (n <= 0) break;
        total += n;
    }
    fclose(f);
    buf[total] = 0;
    *len_out = total;
    return 1;
}

static int write_file(const char *path, const char *buf, int len)
{
    FILE *f;
    int n;
    if (!path || !buf || len < 0) return 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    n = (int)fwrite(buf, 1, (unsigned int)len, f);
    fclose(f);
    return n == len;
}

static int first_tri_geom(const x3d_scene *s)
{
    int i;
    int t;
    for (i = 0; i < s->node_count; ++i) {
        t = s->nodes[i].type;
        if (t == X3D_NODE_INDEXED_FACE_SET || t == X3D_NODE_INDEXED_TRIANGLE_SET || t == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET || t == X3D_NODE_INDEXED_TRIANGLE_FAN_SET || t == X3D_NODE_TRIANGLE_SET || t == X3D_NODE_TRIANGLE_STRIP_SET || t == X3D_NODE_TRIANGLE_FAN_SET) return i;
    }
    return X3D_NONE;
}

static void print_stats(const x3d_scene *s)
{
    x3d_stats st;
    x3d_v6_summary v6;
    x3d_v7_summary v7;
    x3d_v8_summary v8;
    x3d_draw_list draw;
    x3d_component_audit audit;
    x3d_collect_stats(s, &st);
    x3d_v6_collect_summary(s, &v6);
    x3d_v7_collect_summary(s, &v7);
    x3d_v8_collect_summary(s, &v8);
    x3d_build_draw_list(s, &draw);
    x3d_audit_declared_components(s, &audit);
    printf("X3D version: %s\n", s->version);
    printf("X3D profile: %s\n", s->profile);
    printf("nodes: %d\n", st.nodes);
    printf("shapes: %d\n", st.shapes);
    printf("transforms: %d\n", st.transforms);
    printf("indexed_face_sets: %d\n", st.indexed_face_sets);
    printf("rendering_primitives: %d\n", st.rendering_primitives);
    printf("primitive_geometries: %d\n", st.primitive_geometries);
    printf("coordinates_vec3: %d\n", st.coordinates);
    printf("integer_indices: %d\n", st.coord_indices);
    printf("materials: %d\n", st.materials);
    printf("pbr_materials: %d\n", st.pbr_materials);
    printf("textures: %d\n", st.textures);
    printf("head_components: %d\n", st.head_components);
    printf("head_units: %d\n", st.head_units);
    printf("scripts: %d\n", st.scripts);
    printf("script_fields: %d\n", st.script_fields);
    printf("sensors: %d\n", st.sensors);
    printf("cad_nodes: %d\n", st.cad_nodes);
    printf("nurbs_nodes: %d\n", st.nurbs_nodes);
    printf("hanim_nodes: %d\n", st.hanim_nodes);
    printf("geospatial_nodes: %d\n", st.geospatial_nodes);
    printf("proto_nodes: %d\n", st.proto_nodes);
    printf("shader_nodes: %d\n", st.shader_nodes);
    printf("environmental_nodes: %d\n", st.environmental_nodes);
    printf("navigation_nodes: %d\n", st.navigation_nodes);
    printf("sound_nodes: %d\n", st.sound_nodes);
    printf("physics_nodes: %d\n", st.physics_nodes);
    printf("event_utility_nodes: %d\n", st.event_utility_nodes);
    printf("import_export_nodes: %d\n", st.import_export_nodes);
    printf("particle_nodes: %d\n", st.particle_nodes);
    printf("follower_nodes: %d\n", st.follower_nodes);
    printf("layer_nodes: %d\n", st.layer_nodes);
    printf("layout_nodes: %d\n", st.layout_nodes);
    printf("geometry2d_nodes: %d\n", st.geometry2d_nodes);
    printf("text_nodes: %d\n", st.text_nodes);
    printf("texturing3d_nodes: %d\n", st.texturing3d_nodes);
    printf("cube_map_nodes: %d\n", st.cube_map_nodes);
    printf("volume_nodes: %d\n", st.volume_nodes);
    printf("sound_graph_nodes: %d\n", st.sound_graph_nodes);
    printf("metadata_nodes: %d\n", st.metadata_nodes);
    printf("dis_nodes: %d\n", st.dis_nodes);
    printf("shape_extra_nodes: %d\n", st.shape_extra_nodes);
    printf("routes: %d\n", st.routes);
    printf("unknown_nodes_preserved: %d\n", st.unknown_nodes);
    printf("v6_texture_projectors: %d\n", v6.texture_projector_nodes);
    printf("v6_rendered_textures: %d\n", v6.rendered_texture_nodes);
    printf("v6_texture_properties: %d\n", v6.texture_property_nodes);
    printf("v6_sequencers: %d\n", v6.sequencer_nodes);
    printf("v6_triggers: %d\n", v6.trigger_nodes);
    printf("v6_routes_resolved: %d\n", v6.routes_resolved);
    printf("v6_component_required_nodes: %d\n", audit.required_nodes);
    printf("v6_component_missing_or_low: %d\n", audit.missing_or_low);
    printf("v7_draw_items: %d\n", draw.count);
    printf("v7_raw_attrs: %d\n", v7.raw_attrs);
    printf("v7_max_depth: %d\n", v7.max_depth);
    printf("v8_draw_items: %d\n", v8.draw_items);
    printf("v8_interpolators: %d\n", v8.interpolators);
    printf("v8_field_arity_ok: %d\n", v8.field_arity_ok);
    printf("v8_route_proto_validation_ok: %d\n", v8.route_proto_validation_ok);
}


int main(int argc, char **argv)
{
    int len;
    int rc;
    int out_len;
    int geom;
    x3d_bounds bounds;
    char b0[48];
    char b1[48];
    char b2[48];
    char b3[48];
    char b4[48];
    char b5[48];
    x3d_diag diag;
    if (argc < 2) {
        printf("usage: x3d_dump input.x3d-or-x3dv [roundtrip-out.x3d]\n");
        return 2;
    }
    if (!read_file(argv[1], g_in, X3D_TOOL_IN_CAP, &len)) {
        printf("could not read input file\n");
        return 1;
    }
    rc = x3d_parse_any(&g_scene, (const unsigned char *)g_in, len, &diag);
    if (rc != X3D_STATUS_OK) {
        printf("parse rc=%d line=%d col=%d node=%d msg=%s\n", rc, diag.line, diag.column, diag.node_index, diag.message);
        return 1;
    }
    if (diag.warnings > 0) {
        printf("warnings: %d (%s)\n", diag.warnings, diag.message);
    }
    print_stats(&g_scene);
    rc = x3d_validate_scene(&g_scene, &diag);
    if (rc == X3D_STATUS_OK) {
        printf("validation: ok\n");
    } else {
        printf("validation: warnings=%d node=%d msg=%s\n", diag.warnings, diag.node_index, diag.message);
    }
    rc = x3d_compute_coordinate_bounds(&g_scene, &bounds);
    if (rc == X3D_STATUS_OK) {
        x3d_fx_to_text(bounds.min.x, b0, (int)sizeof(b0));
        x3d_fx_to_text(bounds.min.y, b1, (int)sizeof(b1));
        x3d_fx_to_text(bounds.min.z, b2, (int)sizeof(b2));
        x3d_fx_to_text(bounds.max.x, b3, (int)sizeof(b3));
        x3d_fx_to_text(bounds.max.y, b4, (int)sizeof(b4));
        x3d_fx_to_text(bounds.max.z, b5, (int)sizeof(b5));
        printf("coord_bounds: min(%s %s %s) max(%s %s %s)\n", b0, b1, b2, b3, b4, b5);
    }
    geom = first_tri_geom(&g_scene);
    if (geom != X3D_NONE) {
        rc = x3d_triangulate_node(&g_scene, geom, &g_mesh, &diag);
        if (rc == X3D_STATUS_OK) {
            printf("first_tri_geom: %s triangles=%d\n", x3d_node_type_name(g_scene.nodes[geom].type), g_mesh.tri_count);
        } else {
            printf("first_tri_geom_triangulation_rc: %d\n", rc);
        }
    }
    if (argc >= 3) {
        rc = x3d_write_xml(&g_scene, g_out, X3D_TOOL_OUT_CAP, &out_len);
        if (rc != X3D_STATUS_OK) {
            printf("write rc=%d\n", rc);
            return 1;
        }
        if (!write_file(argv[2], g_out, out_len)) {
            printf("could not write output file\n");
            return 1;
        }
        printf("wrote: %s (%d bytes)\n", argv[2], out_len);
    }
    return 0;
}
