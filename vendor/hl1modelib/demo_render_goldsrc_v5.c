#include "hlfp_softmdl_plus.h"
#include "hlfp_bundle.h"
#include "hlfp_goldsrc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define DEMO_W 768
#define DEMO_H 768
#define DEMO_MDL_CAP (8U * 1024U * 1024U)

static hlfp_u8 g_mdl_bytes[DEMO_MDL_CAP];
static hlfp_u8 g_texture_bytes[DEMO_MDL_CAP];
static hlfp_u8 g_seqgroup_bytes[DEMO_MDL_CAP];
static hlfp_mdl g_mdl;
static hlfp_mdl_quat_pose_set g_qpose;
static hlfp_mat3x4 g_world[HLFP_MDL_MAX_BONES];
static hlfp_mat3x4 g_front_world[HLFP_MDL_MAX_BONES];
static hlfp_soft_scratch g_scratch;
static int g_v4_enable=0;
static int g_v5_enable=0;
static hlfp_p89_camera g_perspective_camera;
static hlfp_u8 g_rgba[DEMO_W * DEMO_H * 4];
static hlfp_i32 g_zbuf[DEMO_W * DEMO_H];


static int demo_error(hlfp_error *err, int code, const char *msg) {
    if (err != 0) {
        hlfp_error_clear(err);
        err->code = code;
        if (msg != 0) {
            strncpy(err->message, msg, sizeof(err->message) - 1U);
            err->message[sizeof(err->message) - 1U] = '\0';
        }
    }
    return code;
}

typedef struct demo_bounds_s {
    hlfp_fixed minx;
    hlfp_fixed maxx;
    hlfp_fixed miny;
    hlfp_fixed maxy;
    int valid;
} demo_bounds;

static int demo_write(void *user, const void *data, size_t size) {
    FILE *f;
    f = (FILE*)user;
    if (size == 0U) return 1;
    return fwrite(data, 1U, size, f) == size;
}

static long demo_fsize(FILE *f) {
    long n;
    if (fseek(f, 0L, SEEK_END) != 0) return -1L;
    n = ftell(f);
    if (fseek(f, 0L, SEEK_SET) != 0) return -1L;
    return n;
}

static int demo_read_disk(void *user, const char *path,
                          hlfp_u8 *dst, size_t capacity, size_t *out_bytes) {
    FILE *f;
    long n;
    size_t actual;
    (void)user;
    f = fopen(path, "rb");
    if (f == 0) return HLFP_ERR_MISSING_DATA;
    n = demo_fsize(f);
    if (n <= 0 || (unsigned long)n > capacity) {
        fclose(f);
        return HLFP_ERR_CAPACITY;
    }
    actual = fread(dst, 1U, (size_t)n, f);
    fclose(f);
    if (actual != (size_t)n) return HLFP_ERR_IO;
    *out_bytes = actual;
    return HLFP_OK;
}

static int demo_load_mdl(const char *path, hlfp_error *err) {
    hlfp_bundle_buffers buffers;
    hlfp_bundle_stats stats;
    int rc;
    buffers.main_bytes = g_mdl_bytes;
    buffers.main_capacity = sizeof(g_mdl_bytes);
    buffers.texture_bytes = g_texture_bytes;
    buffers.texture_capacity = sizeof(g_texture_bytes);
    buffers.sequence_pool = g_seqgroup_bytes;
    buffers.sequence_capacity = sizeof(g_seqgroup_bytes);
    rc = hlfp_bundle_load(path, demo_read_disk, 0, &buffers, &g_mdl, &stats, err);
    if (rc != HLFP_OK) {
        printf("LOAD_FAIL code=%d msg=%s path=%s\n", rc, err->message, path);
        return rc;
    }
    printf("LOAD_OK textures_external=%d seqgroups_external=%d\n",
           stats.has_external_textures, stats.sequence_files_loaded);
    return HLFP_OK;
}

static int demo_decode_pose(int seq, hlfp_fixed frame_position, hlfp_error *err) {
    int rc;
    rc = hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(&g_mdl, seq, frame_position, 0, &g_qpose, err);
    if (rc != HLFP_OK) {
        printf("QUAT_DECODE_FAIL code=%d msg=%s\n", err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return rc;
    }
    rc = hlfp_mdl_quat_pose_to_world_matrices(&g_mdl, &g_qpose, g_world, HLFP_MDL_MAX_BONES, err);
    if (rc != HLFP_OK) {
        printf("QUAT_WORLD_FAIL code=%d msg=%s\n", err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return rc;
    }
    return HLFP_OK;
}

static void demo_front_project_mats(void) {
    int i;
    for (i = 0; i < g_mdl.num_bones && i < HLFP_MDL_MAX_BONES; ++i) {
        hlfp_fixed old_x;
        hlfp_fixed old_y;
        hlfp_fixed old_z;
        g_front_world[i] = g_world[i];
        old_x = g_world[i].m[0][3];
        old_y = g_world[i].m[1][3];
        old_z = g_world[i].m[2][3];
        /* GoldSrc convention: X forward, Y left, Z up.
           Front capture looks down X, so screen=(Y,Z), depth=X. */
        g_front_world[i].m[0][3] = -old_y;
        g_front_world[i].m[1][3] = old_z;
        g_front_world[i].m[2][3] = old_x;
    }
}

static void demo_front_project_mesh(hlfp_mdl_mesh_expanded *mesh) {
    int i;
    if (mesh == 0) return;
    for (i = 0; i < mesh->num_vertices; ++i) {
        hlfp_fixed old_x;
        hlfp_fixed old_y;
        hlfp_fixed old_z;
        old_x = mesh->vertices[i].pos.x;
        old_y = mesh->vertices[i].pos.y;
        old_z = mesh->vertices[i].pos.z;
        mesh->vertices[i].pos.x = -old_y;
        mesh->vertices[i].pos.y = old_z;
        mesh->vertices[i].pos.z = old_x;
        old_x = mesh->vertices[i].normal.x;
        old_y = mesh->vertices[i].normal.y;
        old_z = mesh->vertices[i].normal.z;
        mesh->vertices[i].normal.x = -old_y;
        mesh->vertices[i].normal.y = old_z;
        mesh->vertices[i].normal.z = old_x;
    }
}

static void demo_bounds_add(demo_bounds *b, hlfp_fixed x, hlfp_fixed y) {
    if (b == 0) return;
    if (!b->valid) {
        b->minx = x;
        b->maxx = x;
        b->miny = y;
        b->maxy = y;
        b->valid = 1;
        return;
    }
    if (x < b->minx) b->minx = x;
    if (x > b->maxx) b->maxx = x;
    if (y < b->miny) b->miny = y;
    if (y > b->maxy) b->maxy = y;
}

static int demo_compute_bounds(int body_value, int skin_family, demo_bounds *bounds, hlfp_error *err) {
    int bp;
    int rc;
    if (bounds == 0) return demo_error(err, HLFP_ERR_BAD_ARGUMENT, "null demo bounds");
    memset(bounds, 0, sizeof(*bounds));
    for (bp = 0; bp < g_mdl.num_bodyparts; ++bp) {
        int model_index;
        int mesh_local;
        rc = hlfp_goldsrc_model_for_bodypart(&g_mdl, body_value, bp, &model_index, err);
        if (rc != HLFP_OK) return rc;
        if (model_index < 0 || model_index >= g_mdl.num_models) continue;
        for (mesh_local = 0; mesh_local < g_mdl.models[model_index].nummesh; ++mesh_local) {
            int mesh_index;
            int v;
            mesh_index = g_mdl.models[model_index].first_mesh + mesh_local;
            rc = hlfp_goldsrc_bake_mesh_with_matrices(&g_mdl, model_index, mesh_index, skin_family, g_world, HLFP_MDL_MAX_BONES, &g_scratch.mesh, err);
            if (rc != HLFP_OK) return rc;
            for (v = 0; v < g_scratch.mesh.num_vertices; ++v) {
                demo_bounds_add(bounds, -g_scratch.mesh.vertices[v].pos.y, g_scratch.mesh.vertices[v].pos.z);
            }
        }
    }
    if (!bounds->valid) {
        return demo_error(err, HLFP_ERR_BAD_FORMAT, "no renderable vertices selected");
    }
    return HLFP_OK;
}

static int demo_setup_camera(const demo_bounds *bounds, hlfp_soft_camera *cam, hlfp_error *err) {
    hlfp_fixed spanx;
    hlfp_fixed spany;
    hlfp_fixed sx;
    hlfp_fixed sy;
    int rc;
    if (bounds == 0 || cam == 0) return demo_error(err, HLFP_ERR_BAD_ARGUMENT, "null demo camera input");
    hlfp_soft_camera_init(cam, DEMO_W, DEMO_H);
    cam->origin.x = (bounds->minx + bounds->maxx) / 2;
    cam->origin.y = (bounds->miny + bounds->maxy) / 2;
    cam->origin.z = 0;
    cam->center_x = DEMO_W / 2;
    cam->center_y = DEMO_H / 2;
    cam->flip_y = HLFP_TRUE;
    spanx = bounds->maxx - bounds->minx;
    spany = bounds->maxy - bounds->miny;
    if (spanx <= 0) spanx = HLFP_FIXED_ONE;
    if (spany <= 0) spany = HLFP_FIXED_ONE;
    rc = hlfp_fixed_div((hlfp_fixed)((DEMO_W - 96) << HLFP_FIXED_FRAC_BITS), spanx, &sx);
    if (rc != HLFP_OK) return demo_error(err, rc, "camera scale x failed");
    rc = hlfp_fixed_div((hlfp_fixed)((DEMO_H - 96) << HLFP_FIXED_FRAC_BITS), spany, &sy);
    if (rc != HLFP_OK) return demo_error(err, rc, "camera scale y failed");
    cam->scale = sx < sy ? sx : sy;
    if (cam->scale <= 0) cam->scale = HLFP_FIXED_ONE;
    return HLFP_OK;
}

static int demo_clear(const hlfp_soft_framebuffer *fb, hlfp_error *err) {
    hlfp_soft_options opt;
    hlfp_soft_options_init(&opt);
    opt.clear_rgba[0] = 20;
    opt.clear_rgba[1] = 20;
    opt.clear_rgba[2] = 24;
    opt.clear_rgba[3] = 255;
    return hlfp_soft_clear(fb, &opt, err);
}

static int demo_write_tga(const char *path, const hlfp_soft_framebuffer *fb, hlfp_error *err) {
    FILE *f;
    int rc;
    f = fopen(path, "wb");
    if (f == 0) {
        printf("WRITE_OPEN_FAIL %s\n", path);
        return -200;
    }
    rc = hlfp_soft2_write_tga32(fb, demo_write, f, err);
    fclose(f);
    if (rc != HLFP_OK) {
        printf("WRITE_TGA_FAIL %s code=%d msg=%s\n", path, err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return rc;
    }
    return HLFP_OK;
}

static int demo_render_skeleton(const char *path, const hlfp_soft_framebuffer *fb, const hlfp_soft_camera *cam, hlfp_error *err) {
    hlfp_soft2_options opt;
    int rc;
    hlfp_soft2_options_init(&opt);
    opt.depth_test = HLFP_FALSE;
    opt.skeleton_rgba[0] = 255;
    opt.skeleton_rgba[1] = 255;
    opt.skeleton_rgba[2] = 255;
    opt.skeleton_rgba[3] = 255;
    rc = demo_clear(fb, err);
    if (rc != HLFP_OK) return rc;
    demo_front_project_mats();
    rc = hlfp_soft2_draw_skeleton(&g_mdl, g_front_world, HLFP_MDL_MAX_BONES, fb, cam, &opt, err);
    if (rc != HLFP_OK) return rc;
    return demo_write_tga(path, fb, err);
}

static int demo_render_textured(const char *path, int body_value, int skin_family, int eye_offset, const hlfp_soft_framebuffer *fb, const hlfp_soft_camera *cam, hlfp_error *err) {
    hlfp_soft2_options opt;
    int bp;
    int rc;
    hlfp_soft2_options_init(&opt);
    opt.wire_overlay = HLFP_FALSE;
    opt.skeleton_overlay = HLFP_FALSE;
    opt.shade_flat = HLFP_FALSE;
    opt.depth_test = HLFP_TRUE;
    opt.chrome_view_dependent = HLFP_TRUE;
    opt.enable_directional_light = HLFP_TRUE;
    if(g_v4_enable){
        opt.chrome_per_bone=HLFP_TRUE;
        opt.smooth_vertex_light=HLFP_TRUE;
        demo_front_project_mats();
        opt.bone_matrices=g_front_world;
        opt.bone_matrix_count=g_mdl.num_bones;
        opt.light_lambert=HLFP_FIXED_ONE+HLFP_FIXED_ONE/2;
    }
    if(g_v5_enable){
        hlfp_p89_camera_init(&g_perspective_camera);
        g_perspective_camera.eye.x=cam->origin.x;
        g_perspective_camera.eye.y=cam->origin.y;
        g_perspective_camera.eye.z=256*HLFP_FIXED_ONE;
        g_perspective_camera.forward.z=-HLFP_FIXED_ONE;
        g_perspective_camera.focal=256*HLFP_FIXED_ONE;
        g_perspective_camera.near_z=8*HLFP_FIXED_ONE;
        opt.perspective=&g_perspective_camera;
    }
    opt.chrome_eye.x = cam->origin.x + eye_offset * HLFP_FIXED_ONE;
    opt.chrome_eye.y = cam->origin.y;
    opt.chrome_eye.z = 120 * HLFP_FIXED_ONE;
    opt.light_direction.x = 0;
    opt.light_direction.y = HLFP_FIXED_ONE / 2;
    opt.light_direction.z = HLFP_FIXED_ONE / 2;
    opt.ambient_8 = 96;
    opt.diffuse_8 = 159;
    rc = demo_clear(fb, err);
    if (rc != HLFP_OK) return rc;
    for (bp = 0; bp < g_mdl.num_bodyparts; ++bp) {
        int model_index;
        int mesh_local;
        rc = hlfp_goldsrc_model_for_bodypart(&g_mdl, body_value, bp, &model_index, err);
        if (rc != HLFP_OK) return rc;
        if (model_index < 0 || model_index >= g_mdl.num_models) continue;
        for (mesh_local = 0; mesh_local < g_mdl.models[model_index].nummesh; ++mesh_local) {
            int mesh_index;
            mesh_index = g_mdl.models[model_index].first_mesh + mesh_local;
            rc = hlfp_goldsrc_bake_mesh_with_matrices(&g_mdl, model_index, mesh_index, skin_family, g_world, HLFP_MDL_MAX_BONES, &g_scratch.mesh, err);
            if (rc != HLFP_OK) return rc;
            demo_front_project_mesh(&g_scratch.mesh);
            rc = hlfp_soft2_draw_expanded_mesh_textured(&g_mdl, &g_scratch.mesh, fb, cam, &opt, err);
            if (rc != HLFP_OK) return rc;
        }
    }
    return demo_write_tga(path, fb, err);
}

/* Parse 16.16 positions as decimal strings, without float/double. */
static int demo_parse_frame(const char *arg, hlfp_fixed *out) {
    char *next;
    long w;
    long frac = 0;
    long den = 1;
    if (arg == 0 || out == 0) return 0;
    w = strtol(arg, &next, 10);
    if (next == arg || w < 0 || w > (INT_MAX / HLFP_FIXED_ONE) - 1) return 0;
    if (*next == '.') {
        ++next;
        while (*next >= '0' && *next <= '9' && den < 10000L) {
            frac = frac * 10L + (*next - '0');
            den *= 10L;
            ++next;
        }
    }
    if (*next != '\0') return 0;
    *out = (hlfp_fixed)(w * HLFP_FIXED_ONE + (frac * HLFP_FIXED_ONE) / den);
    return 1;
}

int main(int argc, char **argv) {
    hlfp_error err;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    demo_bounds bounds;
    const char *mdl_path;
    const char *prefix;
    char skeleton_path[256];
    char textured_path[256];
    int seq;
    hlfp_fixed frame_position;
    int body_value;
    int skin_family;
    int rc;

    if (argc < 3) {
        printf("usage: demo_render_goldsrc_v5 <model.mdl> <output_prefix> [sequence] [frame] [v4|v5]\n");
        return 2;
    }
    mdl_path = argv[1];
    prefix = argv[2];
    seq = argc > 3 ? atoi(argv[3]) : 0;
    if (!demo_parse_frame(argc > 4 ? argv[4] : "0", &frame_position)) {
        puts("BAD_FIXED_FRAME use e.g. 0, 0.5, 6.25");
        return 2;
    }
    body_value = 0;
    skin_family = 0;
    g_v5_enable=(argc>5 && strcmp(argv[5],"v5")==0);
    g_v4_enable=g_v5_enable||(argc>5 && strcmp(argv[5],"v4")==0);

    hlfp_error_clear(&err);
    rc = demo_load_mdl(mdl_path, &err);
    if (rc != HLFP_OK) return 1;
    /* Fixed camera per sequence: establish bounds from frame zero so a
       filmstrip does not zoom separately for every fractional sample. */
    rc = demo_decode_pose(seq, 0, &err);
    if (rc != HLFP_OK) return 1;
    rc = demo_compute_bounds(body_value, skin_family, &bounds, &err);
    if (rc != HLFP_OK) {
        printf("BOUNDS_FAIL code=%d msg=%s\n", err.code, err.message);
        return 1;
    }
    rc = demo_setup_camera(&bounds, &cam, &err);
    if (rc != HLFP_OK) {
        printf("CAMERA_FAIL code=%d msg=%s\n", err.code, err.message);
        return 1;
    }

    rc = demo_decode_pose(seq, frame_position, &err);
    if (rc != HLFP_OK) return 1;
    fb.width = DEMO_W;
    fb.height = DEMO_H;
    fb.stride_bytes = DEMO_W * 4;
    fb.rgba = g_rgba;
    fb.zbuffer = g_zbuf;

    if (strlen(prefix) + sizeof("_skeleton.tga") > sizeof(skeleton_path) ||
        strlen(prefix) + sizeof("_textured.tga") > sizeof(textured_path)) {
        puts("OUTPUT_PREFIX_TOO_LONG");
        return 2;
    }
    sprintf(skeleton_path, "%s_skeleton.tga", prefix);
    sprintf(textured_path, "%s_textured.tga", prefix);

    rc = demo_render_skeleton(skeleton_path, &fb, &cam, &err);
    if (rc != HLFP_OK) {
        printf("SKELETON_FAIL code=%d msg=%s\n", err.code, err.message);
        return 1;
    }
    rc = demo_render_textured(textured_path, body_value, skin_family, 0, &fb, &cam, &err);
    if (rc != HLFP_OK) {
        printf("TEXTURED_FAIL code=%d msg=%s\n", err.code, err.message);
        return 1;
    }

    {
        char alternate_path[256];
        if (strlen(prefix) + sizeof("_eye_shift.tga") <= sizeof(alternate_path)) {
            sprintf(alternate_path,"%s_eye_shift.tga",prefix);
            rc = demo_render_textured(alternate_path,body_value,skin_family,96,&fb,&cam,&err);
            if (rc!=HLFP_OK) return 1;
        }
    }
    printf("GOLDSRC_POSE_RENDER_OK model=%s seq=%d frame_fixed=%d body=%d skin=%d bones=%d bodyparts=%d models=%d meshes=%d scale_fixed=%d skeleton=%s textured=%s\n",
           g_mdl.header.name, seq, (int)frame_position, body_value, skin_family, g_mdl.num_bones, g_mdl.num_bodyparts, g_mdl.num_models, g_mdl.num_meshes,
           (int)cam.scale, skeleton_path, textured_path);
    return 0;
}
