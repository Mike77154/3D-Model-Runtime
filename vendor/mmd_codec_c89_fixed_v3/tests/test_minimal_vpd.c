#include "mmd_codec.h"

static unsigned char vpd_minimal[] =
"Vocaloid Pose Data file\r\n"
"model.osm;\r\n"
"1;\r\n"
"Bone0{\r\n"
"center\r\n"
"0.0,1.5,-2.0;\r\n"
"0.0,0.0,0.0,1.0;\r\n"
"}\r\n";

int main(void)
{
    MMD_Pose p;
    int rc;
    rc = mmd_load_vpd(vpd_minimal, (mmd_u32)(sizeof(vpd_minimal) - 1u), &p);
    if (rc != MMD_OK) return 1;
    if (p.bone_count != 1) return 2;
    if (p.bones[0].translation.y != (mmd_fp)(MMD_FP_ONE + (MMD_FP_ONE / 2))) return 3;
    return 0;
}
