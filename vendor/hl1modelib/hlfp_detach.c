#include "hlfp_internal.h"

static int hlfp__cache_copy_main(const void *data, size_t size,
                                 hlfp_mdl_binary_cache *cache,
                                 hlfp_error *err) {
    if (cache == 0 || data == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null main cache input");
    }
    if (size > (size_t)HLFP_MDL_CACHE_MAIN_MAX) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "main binary exceeds fixed cache capacity");
    }
    hlfp__zero(cache->main, (size_t)HLFP_MDL_CACHE_MAIN_MAX);
    if (size != 0U) {
        memcpy(cache->main, data, size);
    }
    cache->main_size = size;
    return HLFP_OK;
}

static int hlfp__cache_copy_texture(const void *data, size_t size,
                                    hlfp_mdl_binary_cache *cache,
                                    hlfp_error *err) {
    if (cache == 0 || data == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null texture cache input");
    }
    if (size > (size_t)HLFP_MDL_CACHE_TEXTURE_MAX) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "texture binary exceeds fixed cache capacity");
    }
    hlfp__zero(cache->texture, (size_t)HLFP_MDL_CACHE_TEXTURE_MAX);
    if (size != 0U) {
        memcpy(cache->texture, data, size);
    }
    cache->texture_size = size;
    return HLFP_OK;
}

static int hlfp__cache_copy_seqgroup(const void *data, size_t size,
                                     hlfp_mdl_binary_cache *cache,
                                     int seqgroup_index,
                                     hlfp_error *err) {
    size_t off;
    if (cache == 0 || data == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence group cache input");
    }
    if (seqgroup_index <= 0 || seqgroup_index >= HLFP_MDL_MAX_SEQGROUPS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence group index out of range");
    }
    off = cache->seqgroup_pool_used;
    if (size > (size_t)HLFP_MDL_CACHE_SEQGROUP_TOTAL_MAX - off) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "sequence group cache pool exhausted");
    }
    if (size != 0U) {
        memcpy(cache->seqgroup_pool + off, data, size);
    }
    cache->seqgroup_offset[seqgroup_index] = off;
    cache->seqgroup_size[seqgroup_index] = size;
    cache->seqgroup_pool_used = off + size;
    return HLFP_OK;
}

void hlfp_mdl_binary_cache_init(hlfp_mdl_binary_cache *cache) {
    if (cache != 0) {
        hlfp__zero(cache, sizeof(*cache));
    }
}

int hlfp_mdl_parse_main_cached(const void *data,
                               size_t size,
                               hlfp_mdl *out,
                               hlfp_mdl_binary_cache *cache,
                               hlfp_error *err) {
    int rc;
    if (data == 0 || out == 0 || cache == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null cached parse input");
    }
    hlfp_error_clear(err);
    hlfp_mdl_binary_cache_init(cache);
    rc = hlfp__cache_copy_main(data, size, cache, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_parse_main((const void *)cache->main, cache->main_size, out, err);
}

int hlfp_mdl_attach_texture_file_cached(hlfp_mdl *out,
                                        const void *data,
                                        size_t size,
                                        hlfp_mdl_binary_cache *cache,
                                        hlfp_error *err) {
    int rc;
    if (out == 0 || data == 0 || cache == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null cached texture input");
    }
    hlfp_error_clear(err);
    rc = hlfp__cache_copy_texture(data, size, cache, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_attach_texture_file(out, (const void *)cache->texture, cache->texture_size, err);
}

int hlfp_mdl_attach_seqgroup_file_cached(hlfp_mdl *out,
                                         int seqgroup_index,
                                         const void *data,
                                         size_t size,
                                         hlfp_mdl_binary_cache *cache,
                                         hlfp_error *err) {
    int rc;
    if (out == 0 || data == 0 || cache == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null cached seqgroup input");
    }
    hlfp_error_clear(err);
    rc = hlfp__cache_copy_seqgroup(data, size, cache, seqgroup_index, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_attach_seqgroup_file(out,
                                         seqgroup_index,
                                         (const void *)(cache->seqgroup_pool + cache->seqgroup_offset[seqgroup_index]),
                                         cache->seqgroup_size[seqgroup_index],
                                         err);
}

int hlfp_mdl_regenerate_main_binary(const hlfp_mdl *mdl,
                                    void *dst,
                                    size_t dst_capacity,
                                    size_t *out_size,
                                    hlfp_error *err) {
    return hlfp_mdl_regenerate_main_binary_semantic(mdl, dst, dst_capacity, out_size, err);
}

int hlfp_mdl_regenerate_texture_binary(const hlfp_mdl *mdl,
                                       void *dst,
                                       size_t dst_capacity,
                                       size_t *out_size,
                                       hlfp_error *err) {
    return hlfp_mdl_regenerate_texture_binary_semantic(mdl, dst, dst_capacity, out_size, err);
}

int hlfp_mdl_regenerate_seqgroup_binary(const hlfp_mdl *mdl,
                                        int seqgroup_index,
                                        void *dst,
                                        size_t dst_capacity,
                                        size_t *out_size,
                                        hlfp_error *err) {
    return hlfp_mdl_regenerate_seqgroup_binary_semantic(mdl, seqgroup_index, dst, dst_capacity, out_size, err);
}
