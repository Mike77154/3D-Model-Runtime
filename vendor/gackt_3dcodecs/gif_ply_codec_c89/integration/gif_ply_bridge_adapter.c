#include "gif_ply_bridge_adapter.h"

void gif_ply_get_codec_binding(GifPlyCodecBinding *out_binding)
{
    if (out_binding == (GifPlyCodecBinding *)0) return;
    out_binding->name = GIF_PLY_CODEC_NAME;
    out_binding->extension = GIF_PLY_CODEC_EXT;
    out_binding->version_major = GIF_PLY_VERSION_MAJOR;
    out_binding->version_minor = GIF_PLY_VERSION_MINOR;
    out_binding->version_patch = GIF_PLY_VERSION_PATCH;
    out_binding->probe_memory = gif_ply_probe_memory;
    out_binding->import_memory = gif_ply_import_memory;
}
