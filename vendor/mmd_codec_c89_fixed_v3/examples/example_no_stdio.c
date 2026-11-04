#include "mmd_codec.h"

/*
   No-stdio example. In your engine, map/read the full asset into a static byte
   buffer, then call the matching loader. The codec itself does not open files,
   allocate memory, or print.
*/

static unsigned char g_file_bytes[1024 * 1024];
static MMD_Model g_model;
static MMD_Motion g_motion;
static MMD_Pose g_pose;

int engine_import_mmd_asset(const unsigned char *bytes, mmd_u32 byte_count)
{
    int fmt;
    fmt = mmd_probe(bytes, byte_count);
    if (fmt == MMD_FMT_PMD || fmt == MMD_FMT_PMX) return mmd_load_model(bytes, byte_count, &g_model);
    if (fmt == MMD_FMT_VMD) return mmd_load_vmd(bytes, byte_count, &g_motion);
    if (fmt == MMD_FMT_VPD) return mmd_load_vpd(bytes, byte_count, &g_pose);
    return MMD_ERR_MAGIC;
}

int dummy_reference(void)
{
    return engine_import_mmd_asset(g_file_bytes, 0u);
}
