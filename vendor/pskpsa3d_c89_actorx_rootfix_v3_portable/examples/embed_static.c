#include "pskpsa3d.h"

/*
   Example: keep the codec entirely heapless by owning storage in static memory.
   Feed bytes from your engine asset pack, ROM image, or file loader.
*/

static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_report g_report;
static psk3d_pose g_local_pose;
static psk3d_pose g_global_pose;
static psk3d_skin_cache g_skin_cache;
static psk3d_skinned_points g_skinned;

int load_actorx_pair(const unsigned char *psk_data, unsigned long psk_size,
                     const unsigned char *psa_data, unsigned long psa_size)
{
    psk3d_result r;

    r = psk3d_parse_psk(psk_data, (psk3d_u32)psk_size, &g_model, &g_report);
    if (r != PSK3D_OK) {
        return 0;
    }

    r = psk3d_parse_psa(psa_data, (psk3d_u32)psa_size, &g_animset, &g_report);
    if (r != PSK3D_OK) {
        return 0;
    }

    r = psk3d_build_skin_cache(&g_model, &g_skin_cache);
    if (r != PSK3D_OK) {
        return 0;
    }

    r = psk3d_sample_sequence_slerp(&g_animset, 0u, 0u, 0L, &g_local_pose);
    if (r != PSK3D_OK) {
        return 0;
    }

    r = psk3d_globalize_pose(g_model.bones, g_model.bone_count, &g_local_pose, &g_global_pose);
    if (r != PSK3D_OK) {
        return 0;
    }

    r = psk3d_skin_points_lbs_cached(&g_model, &g_skin_cache, &g_global_pose, &g_skinned);
    if (r != PSK3D_OK) {
        return 0;
    }

    return 1;
}

const psk3d_model *actorx_model(void)
{
    return &g_model;
}

const psk3d_skinned_points *actorx_skinned_points(void)
{
    return &g_skinned;
}
