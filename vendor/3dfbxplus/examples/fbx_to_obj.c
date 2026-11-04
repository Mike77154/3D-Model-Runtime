#include "fbx_importer.h"
#include "fbx_scene.h"

#include <stdio.h>

static void obj_write_real(FILE *fp, fbx_real v)
{
    fbx_i64 sv;
    fbx_u64 av;
    fbx_u64 ip;
    fbx_u64 frac;
    fbx_u64 dec;
    char digits[6];
    int i;
    sv = (fbx_i64)v;
    if (sv < 0) {
        fputc('-', fp);
        av = (fbx_u64)(~(fbx_u64)sv) + 1u;
    } else {
        av = (fbx_u64)sv;
    }
    ip = av >> FBX_REAL_SHIFT;
    frac = av & (((fbx_u64)1u << FBX_REAL_SHIFT) - 1u);
    dec = (frac * (fbx_u64)1000000u + (fbx_u64)FBX_REAL_HALF) >> FBX_REAL_SHIFT;
    if (dec >= (fbx_u64)1000000u) {
        ip += 1u;
        dec -= (fbx_u64)1000000u;
    }
    fprintf(fp, "%lu.", (unsigned long)ip);
    for (i = 5; i >= 0; --i) {
        digits[i] = (char)('0' + (char)(dec % 10u));
        dec /= 10u;
    }
    fwrite(digits, 1u, sizeof(digits), fp);
}

static int export_scene_obj(FbxScene *scene, FILE *out)
{
    FbxModel *m;
    unsigned long base;
    base = 1ul;
    if (!scene || !out) return 0;
    fprintf(out, "# OBJ exported by 3dfbx smoke tool\n");
    for (m = scene->models; m; m = m->next) {
        FbxMesh *mesh;
        fbx_u32 i;
        mesh = m->mesh;
        if (!mesh) continue;
        fprintf(out, "o %s\n", m->name ? m->name : "mesh");
        for (i = 0u; i < mesh->vertex_count; ++i) {
            fputs("v ", out);
            obj_write_real(out, mesh->positions[i * 3u + 0u]); fputc(' ', out);
            obj_write_real(out, mesh->positions[i * 3u + 1u]); fputc(' ', out);
            obj_write_real(out, mesh->positions[i * 3u + 2u]); fputc('\n', out);
        }
        if (mesh->uvs) {
            for (i = 0u; i < mesh->vertex_count; ++i) {
                fputs("vt ", out);
                obj_write_real(out, mesh->uvs[i * 2u + 0u]); fputc(' ', out);
                obj_write_real(out, mesh->uvs[i * 2u + 1u]); fputc('\n', out);
            }
        }
        if (mesh->normals) {
            for (i = 0u; i < mesh->vertex_count; ++i) {
                fputs("vn ", out);
                obj_write_real(out, mesh->normals[i * 3u + 0u]); fputc(' ', out);
                obj_write_real(out, mesh->normals[i * 3u + 1u]); fputc(' ', out);
                obj_write_real(out, mesh->normals[i * 3u + 2u]); fputc('\n', out);
            }
        }
        for (i = 0u; i + 2u < mesh->index_count; i += 3u) {
            unsigned long a;
            unsigned long b;
            unsigned long c;
            a = base + (unsigned long)mesh->indices[i + 0u];
            b = base + (unsigned long)mesh->indices[i + 1u];
            c = base + (unsigned long)mesh->indices[i + 2u];
            if (mesh->uvs && mesh->normals) fprintf(out, "f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n", a,a,a,b,b,b,c,c,c);
            else fprintf(out, "f %lu %lu %lu\n", a,b,c);
        }
        base += (unsigned long)mesh->vertex_count;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FILE *out;

    if (argc < 3) {
        fprintf(stderr, "usage: fbx_to_obj input.fbx output.obj\n");
        return 2;
    }
    ctx = fbx_open(argv[1]);
    if (!ctx) {
        fprintf(stderr, "open failed: %s\n", argv[1]);
        return 1;
    }
    root = fbx_read_scene(ctx);
    if (!root) {
        fprintf(stderr, "read failed: %s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return 1;
    }
    scene = fbx_scene_build(root);
    if (!scene) {
        fprintf(stderr, "scene build failed\n");
        fbx_close(ctx, root);
        return 1;
    }
    out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "output failed: %s\n", argv[2]);
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }
    if (!export_scene_obj(scene, out)) {
        fclose(out);
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }
    fclose(out);
    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return 0;
}
