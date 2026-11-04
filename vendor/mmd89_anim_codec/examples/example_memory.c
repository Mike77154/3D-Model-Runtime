#include "mmd89_anim.h"

#include <stdio.h>
#include <string.h>

static mmd89_pose_t pose;
static mmd89_motion_t motion;
static unsigned char out_vmd[4096];
static unsigned int out_size;

int main(void)
{
    mmd89_result_t r;
    static const unsigned char sample_vpd[] =
        "Vocaloid Pose Data file\r\n"
        "\r\n"
        "sample.osm;\r\n"
        "\r\n"
        "1;\r\n"
        "\r\n"
        "Bone0{center\r\n"
        "  0.000000,10.000000,0.000000;\r\n"
        "  0.000000,0.000000,0.000000,1.000000;\r\n"
        "}\r\n";

    r = mmd89_vpd_read_memory(sample_vpd, (mmd89_u32)(sizeof(sample_vpd) - 1u), &pose);
    if (r != MMD89_OK) {
        printf("vpd read failed: %s\n", mmd89_result_string(r));
        return 1;
    }
    r = mmd89_vpd_to_vmd_frame0(&pose, "sample", &motion);
    if (r != MMD89_OK) {
        printf("vpd->vmd failed: %s\n", mmd89_result_string(r));
        return 1;
    }
    r = mmd89_vmd_write_memory(&motion, out_vmd, (mmd89_u32)sizeof(out_vmd), (mmd89_u32 *)&out_size);
    if (r != MMD89_OK) {
        printf("vmd write failed: %s\n", mmd89_result_string(r));
        return 1;
    }
    printf("created frame-0 VMD in memory: %u bytes, bones=%u\n", out_size, (unsigned int)motion.bone_count);
    return 0;
}
