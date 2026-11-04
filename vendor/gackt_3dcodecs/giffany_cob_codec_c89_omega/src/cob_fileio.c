#include "cob_private.h"

#include <stdio.h>

typedef struct cob_file_writer_s {
    FILE *f;
} cob_file_writer;

static int file_write_cb(void *user, const void *bytes, unsigned long count)
{
    cob_file_writer *w;
    w = (cob_file_writer *)user;
    if (w == 0 || w->f == 0) {
        return 1;
    }
    if (count == 0UL) {
        return 0;
    }
    return fwrite(bytes, 1, (size_t)count, w->f) == (size_t)count ? 0 : 1;
}

int cob_load_file(const char *path, void *io_buffer, unsigned long io_capacity, cob_scene *scene)
{
    FILE *f;
    long len;
    unsigned long got;
    int st;

    if (path == 0 || io_buffer == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "rb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return COB_ERR_ARGUMENT;
    }
    len = ftell(f);
    if (len < 0L) {
        fclose(f);
        return COB_ERR_ARGUMENT;
    }
    if ((unsigned long)len > io_capacity) {
        fclose(f);
        return COB_ERR_CAPACITY;
    }
    if (fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return COB_ERR_ARGUMENT;
    }
    got = (unsigned long)fread(io_buffer, 1, (size_t)len, f);
    fclose(f);
    if (got != (unsigned long)len) {
        return COB_ERR_TRUNCATED;
    }
    st = cob_decode_memory(scene, io_buffer, got);
    return st;
}

int cob_save_ascii_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_encode_ascii(scene, file_write_cb, &w);
    fclose(f);
    return st;
}

int cob_save_binary_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_encode_binary(scene, file_write_cb, &w);
    fclose(f);
    return st;
}

int cob_save_obj_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_export_obj(scene, file_write_cb, &w);
    fclose(f);
    return st;
}

int cob_save_mtl_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_export_mtl(scene, file_write_cb, &w);
    fclose(f);
    return st;
}

int cob_save_obj_mtl_file(const char *path, const cob_scene *scene, const char *mtl_name)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_export_obj_mtl(scene, file_write_cb, &w, mtl_name);
    fclose(f);
    return st;
}

int cob_save_tri_obj_file(const char *path, const cob_scene *scene)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_export_tri_obj(scene, file_write_cb, &w);
    fclose(f);
    return st;
}

int cob_save_tri_obj_mtl_file(const char *path, const cob_scene *scene, const char *mtl_name)
{
    FILE *f;
    cob_file_writer w;
    int st;

    if (path == 0 || scene == 0) {
        return COB_ERR_ARGUMENT;
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return COB_ERR_ARGUMENT;
    }
    w.f = f;
    st = cob_export_tri_obj_mtl(scene, file_write_cb, &w, mtl_name);
    fclose(f);
    return st;
}
