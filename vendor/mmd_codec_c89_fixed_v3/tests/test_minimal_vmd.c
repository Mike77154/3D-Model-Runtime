#include "mmd_codec.h"

static unsigned char vmd_minimal[] = {
    'V','o','c','a','l','o','i','d',' ','M','o','t','i','o','n',' ','D','a','t','a',' ','0','0','0','2',0,0,0,0,0,
    'm','o','d','e','l',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0,
    0,0,0,0
};

int main(void)
{
    MMD_Motion m;
    int rc;
    rc = mmd_load_vmd(vmd_minimal, (mmd_u32)sizeof(vmd_minimal), &m);
    if (rc != MMD_OK) return 1;
    if (m.bone_key_count != 0) return 2;
    return 0;
}
