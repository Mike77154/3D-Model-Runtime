#include <stdio.h>
#include <string.h>
#include "ask3d.h"

static Ask3dScene g_scene;
static Ask3dWork g_work;
static Ask3dValidateReport g_report;
static Ask3dResolveReport g_resolve;
static Ask3dPostReport g_post;
static Ask3dTopologyReport g_topology;
static Ask3dBakedMesh g_baked;
static char g_file_buffer[ASK3D_FILE_MAX_BYTES];

static int file_write(void *user, const char *text)
{
    FILE *f;
    f = (FILE *)user;
    if (f == 0 || text == 0) {
        return 0;
    }
    fputs(text, f);
    return 1;
}

static void print_fixed(ask3d_fixed v)
{
    char buf[48];
    ask3d_fixed_to_ascii(v, buf, (unsigned int)sizeof(buf), 5U);
    printf("%s", buf);
}

static void print_vec3(const Ask3dVec3 *v)
{
    printf("(");
    print_fixed(v->x);
    printf(",");
    print_fixed(v->y);
    printf(",");
    print_fixed(v->z);
    printf(")");
}

static int load_file(const char *path, char *buffer, unsigned long cap, unsigned long *out_size)
{
    FILE *f;
    unsigned long n;

    if (path == 0 || buffer == 0 || out_size == 0 || cap == 0UL) {
        return 0;
    }
    f = fopen(path, "rb");
    if (f == 0) {
        return 0;
    }
    n = (unsigned long)fread(buffer, 1U, (size_t)(cap - 1UL), f);
    fclose(f);
    buffer[n] = '\0';
    *out_size = n;
    return 1;
}

static int save_obj_pair(const char *obj_path, const char *mtl_path)
{
    FILE *f;
    FILE *m;
    const char *mtl_name;

    f = fopen(obj_path, "wb");
    if (f == 0) {
        return 0;
    }
    m = fopen(mtl_path, "wb");
    if (m == 0) {
        fclose(f);
        return 0;
    }
    mtl_name = strrchr(mtl_path, '/');
    if (mtl_name == 0) {
        mtl_name = strrchr(mtl_path, '\\');
    }
    if (mtl_name != 0) {
        mtl_name++;
    } else {
        mtl_name = mtl_path;
    }
    ask3d_export_obj(&g_scene, file_write, f, mtl_name);
    ask3d_export_mtl(&g_scene, file_write, m);
    fclose(f);
    fclose(m);
    return 1;
}


static int save_ask_file(const char *path)
{
    FILE *f;
    f = fopen(path, "wb");
    if (f == 0) {
        return 0;
    }
    ask3d_export_ask(&g_scene, file_write, f);
    fclose(f);
    return 1;
}

static void print_scene_info(void)
{
    if (g_scene.info.has_export_version != 0U) {
        printf("ase_version: %d\n", g_scene.info.export_version);
    }
    if (g_scene.info.has_filename != 0U) {
        printf("scene_filename: %s\n", g_scene.info.filename);
    }
    if (g_scene.info.has_frame_range != 0U) {
        printf("frames: %d..%d\n", g_scene.info.first_frame, g_scene.info.last_frame);
    }
    if (g_scene.info.has_frame_speed != 0U) {
        printf("frame_speed: %d\n", g_scene.info.frame_speed);
    }
    if (g_scene.info.has_ticks_per_frame != 0U) {
        printf("ticks_per_frame: %d\n", g_scene.info.ticks_per_frame);
    }
}

static void print_objects(void)
{
    unsigned int i;
    const Ask3dObject *obj;
    for (i = 0U; i < g_scene.object_count; i++) {
        obj = &g_scene.objects[i];
        printf("object[%lu]: %s v=%lu vt=%lu vc=%lu f=%lu material_ref=%d time=%d",
            (unsigned long)i,
            obj->name,
            (unsigned long)obj->vertex_count,
            (unsigned long)obj->tvertex_count,
            (unsigned long)obj->cvertex_count,
            (unsigned long)obj->face_count,
            obj->material_ref,
            obj->first_timevalue);
        if (obj->has_parent != 0U) {
            printf(" parent=%s", obj->parent_name);
        }
        if (obj->extra_uv_channel_count != 0U) {
            printf(" extra_uv_channels=%lu", (unsigned long)obj->extra_uv_channel_count);
        }
        if (obj->has_transform != 0U) {
            printf(" tm_pos=");
            print_vec3(&obj->tm_row3);
        }
        if (obj->has_bounds != 0U) {
            printf(" bounds=");
            print_vec3(&obj->bounds_min);
            printf("..");
            print_vec3(&obj->bounds_max);
        }
        printf("\n");
    }
}

static void print_materials(void)
{
    unsigned int i;
    const Ask3dMaterial *mat;
    for (i = 0U; i < g_scene.material_count; i++) {
        mat = &g_scene.materials[i];
        if (mat->used == 0) {
            continue;
        }
        printf("material[%lu]: %s", (unsigned long)i, mat->name);
        if (mat->is_submaterial != 0U) {
            printf(" parent=%d sub=%d", mat->parent_index, mat->sub_index);
        }
        if (mat->has_bitmap != 0U) {
            printf(" bitmap=%s", mat->bitmap);
        }
        if (mat->maps[ASK3D_MAP_BUMP].has_bitmap != 0U) {
            printf(" bump=%s", mat->maps[ASK3D_MAP_BUMP].bitmap);
        }
        if (mat->maps[ASK3D_MAP_OPACITY].has_bitmap != 0U) {
            printf(" opacity=%s", mat->maps[ASK3D_MAP_OPACITY].bitmap);
        }
        printf("\n");
    }
}

static void print_cameras_lights(void)
{
    unsigned int i;
    const Ask3dCamera *cam;
    const Ask3dLight *light;
    for (i = 0U; i < g_scene.camera_count; i++) {
        cam = &g_scene.cameras[i];
        printf("camera[%lu]: %s", (unsigned long)i, cam->name);
        if (cam->has_parent != 0U) {
            printf(" parent=%s", cam->parent_name);
        }
        if (cam->has_transform != 0U) {
            printf(" pos=");
            print_vec3(&cam->tm_row3);
        }
        if (cam->has_fov != 0U) {
            printf(" fov=");
            print_fixed(cam->fov);
        }
        printf("\n");
    }
    for (i = 0U; i < g_scene.light_count; i++) {
        light = &g_scene.lights[i];
        printf("light[%lu]: %s type=%s", (unsigned long)i, light->name, light->light_type);
        if (light->has_parent != 0U) {
            printf(" parent=%s", light->parent_name);
        }
        if (light->has_transform != 0U) {
            printf(" pos=");
            print_vec3(&light->tm_row3);
        }
        if (light->has_intensity != 0U) {
            printf(" intensity=");
            print_fixed(light->intensity);
        }
        printf("\n");
    }
}


static void print_helpers_shapes(void)
{
    unsigned int i;
    const Ask3dHelper *helper;
    const Ask3dShape *shape;
    for (i = 0U; i < g_scene.helper_count; i++) {
        helper = &g_scene.helpers[i];
        printf("helper[%lu]: %s", (unsigned long)i, helper->name);
        if (helper->has_parent != 0U) {
            printf(" parent=%s", helper->parent_name);
        }
        if (helper->has_transform != 0U) {
            printf(" pos=");
            print_vec3(&helper->tm_row3);
        }
        printf("\n");
    }
    for (i = 0U; i < g_scene.shape_count; i++) {
        shape = &g_scene.shapes[i];
        printf("shape[%lu]: %s points=%lu", (unsigned long)i, shape->name, (unsigned long)shape->point_count);
        if (shape->has_parent != 0U) {
            printf(" parent=%s", shape->parent_name);
        }
        if (shape->has_bounds != 0U) {
            printf(" bounds=");
            print_vec3(&shape->bounds_min);
            printf("..");
            print_vec3(&shape->bounds_max);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    unsigned long size;
    int r;
    unsigned int i;

    if (argc < 2) {
        printf("usage: ask3d_dump input.ask [out.obj out.mtl [out.ask]]\n");
        printf("also accepts Autodesk ASCII Scene Export text saved as .ase\n");
        return 1;
    }

    if (!load_file(argv[1], g_file_buffer, ASK3D_FILE_MAX_BYTES, &size)) {
        printf("could not read input file\n");
        return 1;
    }

    r = ask3d_parse_memory(&g_scene, &g_work, g_file_buffer, size);
    ask3d_resolve_scene(&g_scene, &g_resolve);
    ask3d_post_fill_missing_normals(&g_scene, &g_post);
    ask3d_topology_analyze(&g_scene, &g_topology);
    ask3d_validate_scene(&g_scene, &g_report);
    ask3d_bake_scene_world(&g_scene, &g_baked);
    printf("parse_result: %d\n", r);
    print_scene_info();
    printf("objects: %lu\n", (unsigned long)g_scene.object_count);
    printf("materials: %lu\n", (unsigned long)g_scene.material_count);
    printf("cameras: %lu\n", (unsigned long)g_scene.camera_count);
    printf("lights: %lu\n", (unsigned long)g_scene.light_count);
    printf("helpers: %lu\n", (unsigned long)g_scene.helper_count);
    printf("shapes: %lu\n", (unsigned long)g_scene.shape_count);
    printf("shape_points: %lu\n", (unsigned long)g_scene.shape_point_count);
    printf("anim_samples: %lu\n", (unsigned long)g_scene.anim_sample_count);
    printf("mesh_snapshots: %lu\n", (unsigned long)g_scene.mesh_snapshot_count);
    printf("vertices: %lu\n", (unsigned long)g_scene.vertex_count);
    printf("uvs: %lu\n", (unsigned long)g_scene.tvertex_count);
    printf("uv_channels: %lu\n", (unsigned long)g_scene.uv_channel_count);
    printf("uv_channel_faces: %lu\n", (unsigned long)g_scene.uv_face_count);
    printf("vertex_colors: %lu\n", (unsigned long)g_scene.cvertex_count);
    printf("faces: %lu\n", (unsigned long)g_scene.face_count);
    printf("baked_vertices: %lu\n", (unsigned long)g_baked.vertex_count);
    printf("baked_triangles: %lu\n", (unsigned long)g_baked.triangle_count);
    printf("baked_material_kinds: %lu\n", (unsigned long)g_baked.material_count);
    printf("baked_overflowed: %lu\n", (unsigned long)g_baked.overflowed);
    if (g_baked.vertex_count != 0U) {
        unsigned int bt;
        bt = 0U;
        for (i = 0U; i < g_baked.vertex_count; i++) {
            if (g_baked.vertices[i].has_tangent != 0U) {
                bt++;
            }
        }
        printf("baked_first_pos: ");
        print_vec3(&g_baked.vertices[0].position);
        printf("\n");
        printf("baked_tangents: %lu\n", (unsigned long)bt);
    }
    printf("post_normals: face_generated=%lu vertex_generated=%lu degenerate=%lu\n",
        (unsigned long)g_post.generated_face_normals,
        (unsigned long)g_post.generated_vertex_normals,
        (unsigned long)g_post.degenerate_faces);
    printf("topology: unique_edges=%lu boundary=%lu shared=%lu nonmanifold=%lu hidden=%lu hard=%lu smoothing_bits=0x%lX\n",
        (unsigned long)g_topology.unique_edges,
        (unsigned long)g_topology.boundary_edges,
        (unsigned long)g_topology.shared_edges,
        (unsigned long)g_topology.nonmanifold_edges,
        (unsigned long)g_topology.hidden_edges,
        (unsigned long)g_topology.hard_edges,
        (unsigned long)g_topology.smoothing_group_bits);
    printf("faces_with_uvs: %lu\n", (unsigned long)g_report.faces_with_uvs);
    printf("faces_with_colors: %lu\n", (unsigned long)g_report.faces_with_colors);
    printf("faces_with_normals: %lu\n", (unsigned long)g_report.faces_with_normals);
    printf("missing_refs: v=%lu vt=%lu vc=%lu uvch_vt=%lu uvch_f=%lu mat=%lu parent=%lu\n",
        (unsigned long)g_report.missing_vertex_refs,
        (unsigned long)g_report.missing_tvertex_refs,
        (unsigned long)g_report.missing_cvertex_refs,
        (unsigned long)g_report.missing_uv_channel_tvertex_refs,
        (unsigned long)g_report.missing_uv_channel_face_refs,
        (unsigned long)g_report.missing_material_refs,
        (unsigned long)g_report.missing_parent_refs);
    printf("resolved: parents=%lu missing_parents=%lu material_faces=%lu material_kinds=%lu\n",
        (unsigned long)g_resolve.parent_links_resolved,
        (unsigned long)g_resolve.missing_parent_links,
        (unsigned long)g_resolve.faces_resolved,
        (unsigned long)g_resolve.material_use_count);
    printf("skipped_blocks: %lu\n", (unsigned long)g_scene.skipped_blocks);

    print_objects();
    print_materials();
    print_helpers_shapes();
    print_cameras_lights();

    for (i = 0U; i < g_scene.warning_count; i++) {
        printf("warning[%lu]: %s\n", (unsigned long)i, g_scene.warnings[i].text);
    }

    if (argc >= 4) {
        if (!save_obj_pair(argv[2], argv[3])) {
            printf("could not write obj/mtl\n");
            return 1;
        }
        printf("wrote: %s and %s\n", argv[2], argv[3]);
    }

    if (argc >= 5) {
        if (!save_ask_file(argv[4])) {
            printf("could not write ask\n");
            return 1;
        }
        printf("wrote: %s\n", argv[4]);
    }

    return r == ASK3D_OK ? 0 : 2;
}
