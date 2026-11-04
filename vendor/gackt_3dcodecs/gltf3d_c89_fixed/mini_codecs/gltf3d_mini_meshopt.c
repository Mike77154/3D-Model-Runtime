#include "gltf3d_mini_meshopt.h"

int gltf3d_mini_meshopt_validate_job(const gltf3d_mini_meshopt_job *job)
{
    if (!job) return GLTF3D_MINI_ERR_BAD_ARG;
    if (!job->src || !job->dst) return GLTF3D_MINI_ERR_BAD_ARG;
    if (job->count == 0UL || job->stride == 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (job->count > 0xffffffffUL / job->stride) return GLTF3D_MINI_ERR_RANGE;
    if (job->dst_size < job->count * job->stride) return GLTF3D_MINI_ERR_RANGE;
    if (job->mode == GLTF3D_MESHOPT_MODE_ATTRIBUTES) {
        if ((job->stride & 3UL) != 0UL || job->stride > 256UL) return GLTF3D_MINI_ERR_BAD_DATA;
    } else if (job->mode == GLTF3D_MESHOPT_MODE_TRIANGLES) {
        if ((job->count % 3UL) != 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
        if (job->stride != 2UL && job->stride != 4UL) return GLTF3D_MINI_ERR_BAD_DATA;
    } else if (job->mode == GLTF3D_MESHOPT_MODE_INDICES) {
        if (job->stride != 2UL && job->stride != 4UL) return GLTF3D_MINI_ERR_BAD_DATA;
    } else if (job->mode == GLTF3D_MESHOPT_MODE_WEIGHTS) {
        if (job->stride == 0UL || job->stride > 256UL) return GLTF3D_MINI_ERR_BAD_DATA;
    } else {
        return GLTF3D_MINI_ERR_UNSUPPORTED;
    }
    if (job->filter < GLTF3D_MESHOPT_FILTER_NONE || job->filter > GLTF3D_MESHOPT_FILTER_EXPONENTIAL) return GLTF3D_MINI_ERR_UNSUPPORTED;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_meshopt_decode_raw_passthrough(const gltf3d_mini_meshopt_job *job)
{
    unsigned long i;
    if (gltf3d_mini_meshopt_validate_job(job) != GLTF3D_MINI_OK) return gltf3d_mini_meshopt_validate_job(job);
    if (job->src_size > job->dst_size) return GLTF3D_MINI_ERR_RANGE;
    for (i = 0UL; i < job->src_size; ++i) job->dst[i] = job->src[i];
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_meshopt_decode(const gltf3d_mini_meshopt_job *job, const gltf3d_mini_meshopt_callbacks *cb)
{
    int vr;
    vr = gltf3d_mini_meshopt_validate_job(job);
    if (vr != GLTF3D_MINI_OK) return vr;
    if (cb && cb->decode) return cb->decode(cb->user, job);
    return GLTF3D_MINI_ERR_NEED_EXTERNAL;
}

int gltf3d_mini_meshopt_probe_stream(const unsigned char *src, unsigned long src_size, gltf3d_mini_meshopt_probe *probe)
{
    unsigned long i;
    if (!src || !probe) return GLTF3D_MINI_ERR_BAD_ARG;
    for (i = 0UL; i < sizeof(*probe); ++i) ((unsigned char *)probe)[i] = 0U;
    probe->byte_count = src_size;
    if (src_size > 0UL) probe->tag0 = src[0];
    if (src_size > 1UL) probe->tag1 = src[1];
    /* EXT_meshopt_compression payloads are mode-specific entropy streams, not self-describing files.
       This probe only rejects obviously empty/truncated streams and records leading tags for diagnostics. */
    probe->maybe_meshopt = (src_size > 0UL) ? 1 : 0;
    probe->version_guess = 0;
    return GLTF3D_MINI_OK;
}


const char *gltf3d_mini_meshopt_mode_name(int mode)
{
    if (mode == GLTF3D_MESHOPT_MODE_ATTRIBUTES) return "ATTRIBUTES";
    if (mode == GLTF3D_MESHOPT_MODE_TRIANGLES) return "TRIANGLES";
    if (mode == GLTF3D_MESHOPT_MODE_INDICES) return "INDICES";
    if (mode == GLTF3D_MESHOPT_MODE_WEIGHTS) return "WEIGHTS";
    return "UNKNOWN";
}

const char *gltf3d_mini_meshopt_filter_name(int filter)
{
    if (filter == GLTF3D_MESHOPT_FILTER_NONE) return "NONE";
    if (filter == GLTF3D_MESHOPT_FILTER_OCTAHEDRAL) return "OCTAHEDRAL";
    if (filter == GLTF3D_MESHOPT_FILTER_QUATERNION) return "QUATERNION";
    if (filter == GLTF3D_MESHOPT_FILTER_EXPONENTIAL) return "EXPONENTIAL";
    return "UNKNOWN";
}

int gltf3d_mini_meshopt_job_from_bufferview(const gltf3d_asset *asset, int buffer_view_index, unsigned char *dst, unsigned long dst_size, gltf3d_mini_meshopt_job *job)
{
    const gltf3d_bufferview *bv;
    const gltf3d_buffer *b;
    unsigned long off;
    if (!asset || !dst || !job) return GLTF3D_MINI_ERR_BAD_ARG;
    if (buffer_view_index < 0 || buffer_view_index >= asset->buffer_view_count) return GLTF3D_MINI_ERR_RANGE;
    bv = &asset->buffer_views[buffer_view_index];
    if (!bv->has_meshopt) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (bv->meshopt_buffer < 0 || bv->meshopt_buffer >= asset->buffer_count) return GLTF3D_MINI_ERR_RANGE;
    b = &asset->buffers[bv->meshopt_buffer];
    off = bv->meshopt_byte_offset;
    if (!b->data || off > b->data_length || bv->meshopt_byte_length > b->data_length - off) return GLTF3D_MINI_ERR_RANGE;
    job->src = b->data + off;
    job->src_size = bv->meshopt_byte_length;
    job->dst = dst;
    job->dst_size = dst_size;
    job->count = bv->meshopt_count;
    job->stride = bv->meshopt_byte_stride;
    job->mode = bv->meshopt_mode;
    job->filter = bv->meshopt_filter;
    return gltf3d_mini_meshopt_validate_job(job);
}

int gltf3d_mini_meshopt_decode_best_effort(const gltf3d_mini_meshopt_job *job, const gltf3d_mini_meshopt_callbacks *cb)
{
    int r;
    unsigned long expected;
    r = gltf3d_mini_meshopt_validate_job(job);
    if (r != GLTF3D_MINI_OK) return r;
    if (cb && cb->decode) return cb->decode(cb->user, job);
    expected = job->count * job->stride;
    if (job->src_size == expected) return gltf3d_mini_meshopt_decode_raw_passthrough(job);
    return GLTF3D_MINI_ERR_NEED_EXTERNAL;
}
