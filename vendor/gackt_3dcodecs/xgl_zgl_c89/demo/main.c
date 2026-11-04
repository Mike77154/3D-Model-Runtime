#include <stdio.h>
#include <string.h>
#include "../src/xgl3d.h"

#define DEMO_INPUT_MAX (8UL * 1024UL * 1024UL)
#define DEMO_XML_MAX   (32UL * 1024UL * 1024UL)
#define DEMO_OBJ_MAX   (32UL * 1024UL * 1024UL)
#define DEMO_STATS_MAX (64UL * 1024UL)

static unsigned char input_buf[DEMO_INPUT_MAX];
static char xml_buf[DEMO_XML_MAX];
static char obj_buf[DEMO_OBJ_MAX];
static char stats_buf[DEMO_STATS_MAX];
static xgl3d_context ctx;

static unsigned long read_all(const char *path, unsigned char *buf, unsigned long cap)
{
    FILE *f;
    unsigned long n;
    unsigned long r;
    f = fopen(path, "rb");
    if (f == 0) return 0UL;
    n = 0UL;
    while (n < cap) {
        r = (unsigned long)fread(buf + n, 1, (size_t)(cap - n), f);
        if (r == 0UL) break;
        n += r;
    }
    fclose(f);
    return n;
}

static int write_all(const char *path, const char *buf, unsigned long len)
{
    FILE *f;
    unsigned long w;
    f = fopen(path, "wb");
    if (f == 0) return 0;
    w = (unsigned long)fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return w == len;
}

static void print_fixed3(const char *name, xgl3d_color c)
{
    printf("%s %ld.%03ld %ld.%03ld %ld.%03ld\n", name,
           xgl3d_fixed_to_milli(c.r) / 1000L, xgl3d_fixed_to_milli(c.r) % 1000L,
           xgl3d_fixed_to_milli(c.g) / 1000L, xgl3d_fixed_to_milli(c.g) % 1000L,
           xgl3d_fixed_to_milli(c.b) / 1000L, xgl3d_fixed_to_milli(c.b) % 1000L);
}

int main(int argc, char **argv)
{
    unsigned long in_len;
    unsigned long xml_len;
    unsigned long obj_len;
    int r;
    int i;
    if (argc < 2) {
        printf("usage: xgl3d_demo file.xgl|file.zgl [out.obj] [out.mtl]\n");
        return 1;
    }
    in_len = read_all(argv[1], input_buf, DEMO_INPUT_MAX);
    if (in_len == 0UL) {
        printf("could not read input\n");
        return 1;
    }
    xgl3d_init(&ctx);
    xml_len = 0UL;
    if (in_len >= 2UL && input_buf[0] == 0x1FU && input_buf[1] == 0x8BU) {
        r = xgl3d_load_zgl_buffer(&ctx, input_buf, in_len, xml_buf, DEMO_XML_MAX, &xml_len);
    } else {
        r = xgl3d_load_xgl_text(&ctx, (const char *)input_buf, in_len);
    }
    printf("decode: %s (%d)\n", xgl3d_error_string(r), r);
    printf("positions=%d normals=%d texcoords=%d materials=%d textures=%d line_styles=%d point_styles=%d meshes=%d instances=%d raw_faces=%d raw_lines=%d raw_points=%d\n",
           ctx.position_count, ctx.normal_count, ctx.texcoord_count, ctx.model.material_count, ctx.model.texture_count,
           ctx.model.line_style_count, ctx.model.point_style_count, ctx.mesh_count, ctx.instance_count, ctx.raw_face_count, ctx.raw_line_count, ctx.raw_point_count);
    printf("output vertices=%d triangles=%d lines=%d points=%d\n", ctx.model.vertex_count, ctx.model.triangle_count, ctx.model.line_count, ctx.model.point_count);
    if (ctx.model.has_bounds) {
        printf("bbox min=%ld,%ld,%ld max=%ld,%ld,%ld milli\n",
               xgl3d_fixed_to_milli(ctx.model.bbox_min.x), xgl3d_fixed_to_milli(ctx.model.bbox_min.y), xgl3d_fixed_to_milli(ctx.model.bbox_min.z),
               xgl3d_fixed_to_milli(ctx.model.bbox_max.x), xgl3d_fixed_to_milli(ctx.model.bbox_max.y), xgl3d_fixed_to_milli(ctx.model.bbox_max.z));
    }
    print_fixed3("background", ctx.model.background);
    if (xml_len != 0UL) printf("inflated_xml=%lu bytes\n", xml_len);
    i = 0;
    while (i < ctx.diag_count && i < 12) {
        printf("diag[%d] code=%d off=%lu tag=%s msg=%s\n", i, ctx.diags[i].code, ctx.diags[i].offset, ctx.diags[i].tag, ctx.diags[i].message);
        ++i;
    }
    r = xgl3d_write_stats_to_buffer(&ctx, stats_buf, DEMO_STATS_MAX, &obj_len);
    if (r == XGL3D_OK) printf("%s", stats_buf);
    if (argc >= 3) {
        obj_len = 0UL;
        if (argc >= 4) {
            unsigned long mtl_len;
            mtl_len = 0UL;
            r = xgl3d_write_obj_mtl_to_buffers(&ctx.model, argv[3], obj_buf, DEMO_OBJ_MAX, &obj_len, xml_buf, DEMO_XML_MAX, &mtl_len);
            if (r == XGL3D_OK && write_all(argv[2], obj_buf, obj_len) && write_all(argv[3], xml_buf, mtl_len)) printf("wrote OBJ/MTL: %s + %s\n", argv[2], argv[3]);
            else printf("OBJ/MTL write failed: %s\n", xgl3d_error_string(r));
        } else {
            r = xgl3d_write_obj_to_buffer(&ctx.model, obj_buf, DEMO_OBJ_MAX, &obj_len);
            if (r == XGL3D_OK && write_all(argv[2], obj_buf, obj_len)) printf("wrote OBJ: %s (%lu bytes)\n", argv[2], obj_len);
            else printf("OBJ write failed: %s\n", xgl3d_error_string(r));
        }
    }
    return 0;
}
