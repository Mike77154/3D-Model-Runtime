#include "mmd_codec.h"

static unsigned char pmx_minimal[] = {
    'P','M','X',' ',
    0x00,0x00,0x00,0x40,
    8,
    1,0,1,1,1,1,1,1,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0
};

int main(void)
{
    MMD_Model m;
    int rc;
    rc = mmd_load(pmx_minimal, (mmd_u32)sizeof(pmx_minimal), &m);
    if (rc != MMD_OK) return 1;
    if (m.format != MMD_FMT_PMX) return 2;
    if (m.version_q16 != (2 * MMD_FP_ONE)) return 3;
    return 0;
}
