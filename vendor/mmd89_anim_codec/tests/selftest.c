#include "mmd89_anim.h"

#include <stdio.h>
#include <string.h>

static mmd89_pose_t pose_a;
static mmd89_pose_t pose_b;
static mmd89_motion_t motion_a;
static mmd89_motion_t motion_b;
static unsigned char text_buf[8192];
static unsigned char vmd_buf[8192];
static unsigned int text_size;
static unsigned int vmd_size;

static int fail(const char *msg)
{
    printf("FAIL: %s\n", msg);
    return 1;
}

int main(void)
{
    mmd89_result_t r;
    static const unsigned char sample[] =
        "Vocaloid Pose Data file\n"
        "\n"
        "model.osm;\n"
        "\n"
        "2;\n"
        "\n"
        "Bone0{center\n"
        "  1.250000,-2.500000,0.125000;\n"
        "  0.000000,0.000000,0.000000,1.000000;\n"
        "}\n"
        "\n"
        "Bone1{neck\n"
        "  0.000000,0.000000,0.000000;\n"
        "  0.000000,0.250000,0.000000,0.968750;\n"
        "}\n";

    if (mmd89_check_types() != MMD89_OK) {
        return fail("C integer type size mismatch");
    }
    r = mmd89_vpd_read_memory(sample, (mmd89_u32)(sizeof(sample) - 1u), &pose_a);
    if (r != MMD89_OK) {
        return fail("read vpd");
    }
    if (pose_a.bone_count != 2u) {
        return fail("vpd bone count");
    }
    r = mmd89_vpd_write_memory(&pose_a, text_buf, (mmd89_u32)sizeof(text_buf), (mmd89_u32 *)&text_size);
    if (r != MMD89_OK) {
        return fail("write vpd");
    }
    r = mmd89_vpd_read_memory(text_buf, (mmd89_u32)text_size, &pose_b);
    if (r != MMD89_OK || pose_b.bone_count != 2u) {
        return fail("roundtrip vpd");
    }
    r = mmd89_vpd_to_vmd_frame0(&pose_b, "model", &motion_a);
    if (r != MMD89_OK || motion_a.bone_count != 2u) {
        return fail("vpd to vmd");
    }
    r = mmd89_vmd_write_memory(&motion_a, vmd_buf, (mmd89_u32)sizeof(vmd_buf), (mmd89_u32 *)&vmd_size);
    if (r != MMD89_OK) {
        return fail("write vmd");
    }
    r = mmd89_vmd_read_memory(vmd_buf, (mmd89_u32)vmd_size, &motion_b);
    if (r != MMD89_OK || motion_b.bone_count != 2u) {
        return fail("roundtrip vmd");
    }
    if (strcmp(motion_b.bones[0].name, "center") != 0) {
        return fail("bone name");
    }
    printf("OK: selftest passed, vpd=%u bytes, vmd=%u bytes\n", text_size, vmd_size);
    return 0;
}
