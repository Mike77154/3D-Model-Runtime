#include <stdio.h>
#include "uc3d.h"

#define DUMP_MAX_FILE (16UL * 1024UL * 1024UL)
static unsigned char g_file[DUMP_MAX_FILE + 1UL];

static unsigned long load_file(const char *path, unsigned char *dst, unsigned long cap)
{
    FILE *f;
    unsigned long n;
    f = fopen(path, "rb");
    if (f == (FILE *)0) {
        return 0UL;
    }
    n = (unsigned long)fread(dst, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

static unsigned long rd32le(const unsigned char *p)
{
    return ((unsigned long)p[0]) |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static void print_fixed(const char *label, uc3d_fixed v)
{
    char buf[32];
    uc3d_fixed_to_string(v, buf, sizeof(buf), 4);
    printf("%s%s", label, buf);
}

static int dump_uc(unsigned long size)
{
    uc3d_scene scene;
    unsigned long i;
    int r;

    g_file[size] = 0U;
    r = uc3d_parse_uc_buffer((const char *)g_file, size, &scene);
    printf("uc3d .uc parse result: %d\n", r);
    printf("class: %s extends %s\n", scene.class_name, scene.parent_name);
    printf("execs=%lu meshes=%lu textures=%lu sequences=%lu materials=%lu defaults=%lu warnings=%lu overflow=%lu\n",
           scene.exec_count, scene.mesh_count, scene.texture_count, scene.sequence_count,
           scene.material_count, scene.default_prop_count, scene.warning_count, scene.overflow_count);

    for (i = 0UL; i < scene.mesh_count; ++i) {
        printf("\nmesh[%lu] %s meshmap=%s\n", i, scene.meshes[i].mesh, scene.meshes[i].meshmap);
        printf("  anivfile=%s\n", scene.meshes[i].anivfile);
        printf("  datafile=%s\n", scene.meshes[i].datafile);
        print_fixed("  origin x=", scene.meshes[i].origin_x);
        print_fixed(" y=", scene.meshes[i].origin_y);
        print_fixed(" z=", scene.meshes[i].origin_z);
        printf(" pitch=%ld yaw=%ld roll=%ld\n", scene.meshes[i].pitch, scene.meshes[i].yaw, scene.meshes[i].roll);
        print_fixed("  scale  x=", scene.meshes[i].scale_x);
        print_fixed(" y=", scene.meshes[i].scale_y);
        print_fixed(" z=", scene.meshes[i].scale_z);
        printf("\n");
    }

    for (i = 0UL; i < scene.sequence_count; ++i) {
        printf("sequence[%lu] mesh=%s seq=%s start=%ld num=%ld\n", i,
               scene.sequences[i].mesh, scene.sequences[i].name,
               scene.sequences[i].start_frame, scene.sequences[i].num_frames);
    }

    for (i = 0UL; i < scene.texture_count; ++i) {
        printf("texture[%lu] name=%s file=%s group=%s flags=%ld\n", i,
               scene.textures[i].name, scene.textures[i].file,
               scene.textures[i].group, scene.textures[i].flags);
    }

    return r == UC3D_OK || r == UC3D_ERR_FULL ? 0 : 1;
}

static int dump_upkg(unsigned long size)
{
    uc3d_upkg pkg;
    unsigned long i;
    int r;

    r = uc3d_parse_upkg_buffer(g_file, size, &pkg);
    printf("uc3d .u package parse result: %d\n", r);
    if (r != UC3D_OK) {
        return 1;
    }
    printf("magic=0x%08lX version=%lu flags=0x%08lX\n", pkg.magic, pkg.version, pkg.package_flags);
    printf("names=%lu/%lu imports=%lu/%lu exports=%lu/%lu warnings=%lu overflow=%lu\n",
           pkg.name_count, pkg.declared_name_count,
           pkg.import_count, pkg.declared_import_count,
           pkg.export_count, pkg.declared_export_count,
           pkg.warning_count, pkg.overflow_count);
    printf("offsets: names=%lu imports=%lu exports=%lu\n", pkg.name_offset, pkg.import_offset, pkg.export_offset);

    for (i = 0UL; i < pkg.import_count && i < 32UL; ++i) {
        printf("import[%lu] class=%s.%s package=%s object=%s full=%s\n", i,
               pkg.imports[i].class_package_name,
               pkg.imports[i].class_name_text,
               pkg.imports[i].package_path,
               pkg.imports[i].object_name_text,
               pkg.imports[i].full_path);
    }
    if (pkg.import_count > 32UL) {
        printf("... %lu more imports\n", pkg.import_count - 32UL);
    }

    for (i = 0UL; i < pkg.export_count && i < 64UL; ++i) {
        printf("export[%lu] name=%s full=%s class=%s super=%s package=%s flags=0x%08lX serial=%ld@%ld crc32=0x%08lX in_range=%d\n",
               i,
               pkg.exports[i].object_name_text,
               pkg.exports[i].full_path,
               pkg.exports[i].class_path,
               pkg.exports[i].super_path,
               pkg.exports[i].package_path,
               pkg.exports[i].object_flags,
               pkg.exports[i].serial_size,
               pkg.exports[i].serial_offset,
               pkg.exports[i].serial_crc32,
               pkg.exports[i].payload_in_range);
    }
    if (pkg.export_count > 64UL) {
        printf("... %lu more exports\n", pkg.export_count - 64UL);
    }
    return 0;
}

static int dump_mesh_pair(const char *d_path, const char *a_path)
{
    static unsigned char d_file[2UL * 1024UL * 1024UL];
    static unsigned char a_file[8UL * 1024UL * 1024UL];
    static uc3d_vertex verts[65536UL];
    static uc3d_face faces[65536UL];
    uc3d_ue1_mesh_out out;
    unsigned long d_size;
    unsigned long a_size;
    unsigned long i;
    int r;

    d_size = load_file(d_path, d_file, sizeof(d_file));
    a_size = load_file(a_path, a_file, sizeof(a_file));
    if (d_size == 0UL || a_size == 0UL) {
        printf("could not read mesh pair\n");
        return 1;
    }
    out.vertices = verts;
    out.vertex_capacity = 65536UL;
    out.faces = faces;
    out.face_capacity = 65536UL;
    r = uc3d_ue1_decode_frame(d_file, d_size, a_file, a_size, 0UL, UC3D_UE1_FORMAT_UNREAL, &out);
    printf("uc3d _d/_a decode result: %d\n", r);
    if (r != UC3D_OK) {
        return 1;
    }
    printf("vertices=%lu faces=%lu frames=%lu frame_size=%lu warnings=%lu\n",
           out.vertex_count, out.face_count, out.frame_count, out.frame_size, out.warning_count);
    for (i = 0UL; i < out.vertex_count && i < 8UL; ++i) {
        char x[32];
        char y[32];
        char z[32];
        uc3d_fixed_to_string(out.vertices[i].x, x, sizeof(x), 3);
        uc3d_fixed_to_string(out.vertices[i].y, y, sizeof(y), 3);
        uc3d_fixed_to_string(out.vertices[i].z, z, sizeof(z), 3);
        printf("v[%lu]=%s,%s,%s\n", i, x, y, z);
    }
    return 0;
}

int main(int argc, char **argv)
{
    unsigned long size;

    if (argc < 2) {
        printf("usage:\n");
        printf("  uc3d_dump file.uc\n");
        printf("  uc3d_dump file.u|file.utx|file.uax|file.unr\n");
        printf("  uc3d_dump mesh_d.3d mesh_a.3d\n");
        return 2;
    }

    if (argc >= 3) {
        return dump_mesh_pair(argv[1], argv[2]);
    }

    size = load_file(argv[1], g_file, DUMP_MAX_FILE);
    if (size == 0UL) {
        printf("could not read file\n");
        return 3;
    }
    if (size >= 4UL && rd32le(g_file) == UC3D_UPKG_MAGIC) {
        return dump_upkg(size);
    }
    return dump_uc(size);
}
