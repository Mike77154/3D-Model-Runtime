#include "hlfp_bundle.h"
#include "hlfp_internal.h"

#include <string.h>

#define HLFP_BUNDLE_PATH_CAP 1024

static int bundle_is_ext(const char *ext, const char *expected) {
    int i;
    for (i=0; ext[i] && expected[i]; ++i) {
        int c=(unsigned char)ext[i];
        int b=(unsigned char)expected[i];
        if (c>='A' && c<='Z') c += 'a'-'A';
        if (c != b) return 0;
    }
    return ext[i] == 0 && expected[i] == 0;
}

static int bundle_read(hlfp_bundle_read_fn reader,
                       void *user,
                       const char *path,
                       hlfp_u8 *dst,
                       size_t capacity,
                       size_t *got,
                       hlfp_error *err) {
    int rc;
    *got = 0U;
    if (dst == 0 || capacity == 0U) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "missing external byte storage");
    }
    rc = reader(user, path, dst, capacity, got);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "model bundle file unavailable");
    }
    if (*got == 0U || *got > capacity) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "invalid model bundle file length");
    }
    return HLFP_OK;
}

int hlfp_bundle_load(const char *path,
                     hlfp_bundle_read_fn read_file,
                     void *read_user,
                     const hlfp_bundle_buffers *buffers,
                     hlfp_mdl *out_mdl,
                     hlfp_bundle_stats *stats,
                     hlfp_error *err) {
    char base[HLFP_BUNDLE_PATH_CAP];
    char companion[HLFP_BUNDLE_PATH_CAP];
    const char *dot;
    size_t base_len;
    size_t bytes;
    size_t used;
    int is_dol;
    int i;
    int mesh_needed;
    int rc;
    hlfp_bundle_stats local_stats;

    if (path == 0 || read_file == 0 || buffers == 0 || out_mdl == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "invalid bundle load parameters");
    }
    hlfp_error_clear(err);
    hlfp__zero(&local_stats, sizeof(local_stats));
    dot = strrchr(path, '.');
    if (dot == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "model path needs .mdl or .dol");
    }
    is_dol = bundle_is_ext(dot, ".dol");
    if (!is_dol && !bundle_is_ext(dot, ".mdl")) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "unsupported model path extension");
    }
    base_len = (size_t)(dot - path);
    if (base_len == 0U || base_len >= sizeof(base)) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "model path too long");
    }
    memcpy(base, path, base_len);
    base[base_len] = '\0';

    rc = bundle_read(read_file, read_user, path, buffers->main_bytes,
                     buffers->main_capacity, &bytes, err);
    if (rc != HLFP_OK) return rc;
    local_stats.main_bytes = bytes;
    rc = hlfp_mdl_parse_main(buffers->main_bytes, bytes, out_mdl, err);
    if (rc != HLFP_OK) return rc;

    mesh_needed = 0;
    for (i=0; i<out_mdl->num_meshes; ++i) {
        if (out_mdl->meshes[i].numtris > 0) {mesh_needed=1;break;}
    }
    /* Like GoldSrc/HLMV: the main header's textureindex, not its
       numtextures, signals an external T.mdl. Empty nonrenderable models
       do not require a texture companion. */
    if (out_mdl->header.textureindex == 0 && mesh_needed) {
        rc = hlfp_goldsrc_make_texture_filename(base, is_dol,
                 companion, (int)sizeof(companion), err);
        if (rc != HLFP_OK) return rc;
        rc = bundle_read(read_file, read_user, companion,
                         buffers->texture_bytes,
                         buffers->texture_capacity, &bytes, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_mdl_attach_texture_file(out_mdl, buffers->texture_bytes,
                                          bytes, err);
        if (rc != HLFP_OK) return rc;
        local_stats.texture_bytes = bytes;
        local_stats.has_external_textures = 1;
    }

    used = 0U;
    for (i=1; i<out_mdl->num_seqgroups; ++i) {
        rc = hlfp_goldsrc_make_seqgroup_filename(base, i, is_dol,
                 companion, (int)sizeof(companion), err);
        if (rc != HLFP_OK) return rc;
        if (buffers->sequence_pool == 0 ||
            used >= buffers->sequence_capacity) {
            return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "sequence pool exhausted");
        }
        rc = bundle_read(read_file, read_user, companion,
                         buffers->sequence_pool+used,
                         buffers->sequence_capacity-used, &bytes, err);
        if (rc != HLFP_OK) return rc;
        rc = hlfp_mdl_attach_seqgroup_file(out_mdl, i,
                 buffers->sequence_pool+used, bytes, err);
        if (rc != HLFP_OK) return rc;
        used += bytes;
        ++local_stats.sequence_files_loaded;
    }
    local_stats.sequence_bytes = used;
    if (stats != 0) *stats = local_stats;
    return HLFP_OK;
}
