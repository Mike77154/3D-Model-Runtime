#include "mmd_codec.h"

static unsigned char pmd_minimal[] = {
    'P','m','d',
    0x00,0x00,0x80,0x3f,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
#define Z16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,Z16,
#undef Z16
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,
    0,0,
    0,0,
    0,
    0,
    0,0,0,0
};

int main(void)
{
    MMD_Model m;
    int rc;
    rc = mmd_load(pmd_minimal, (mmd_u32)sizeof(pmd_minimal), &m);
    if (rc != MMD_OK) return 1;
    if (m.format != MMD_FMT_PMD) return 2;
    return 0;
}
