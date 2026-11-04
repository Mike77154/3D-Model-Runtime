#ifndef MF3D_EXT_PACKAGE_H
#define MF3D_EXT_PACKAGE_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_REQUIRED
#define MF3D_EXT_MAX_REQUIRED 32
#endif
#ifndef MF3D_EXT_MAX_WARNINGS
#define MF3D_EXT_MAX_WARNINGS 64
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_CAP_CORE          0x00000001UL
#define MF3D_CAP_MATERIALS     0x00000002UL
#define MF3D_CAP_PRODUCTION    0x00000004UL
#define MF3D_CAP_SLICE         0x00000008UL
#define MF3D_CAP_BEAMLATTICE   0x00000010UL
#define MF3D_CAP_SECURECONTENT 0x00000020UL
#define MF3D_CAP_BOOLEAN       0x00000040UL
#define MF3D_CAP_DISPLACEMENT  0x00000080UL
#define MF3D_CAP_VOLUMETRIC    0x00000100UL
#define MF3D_CAP_IMPLICIT      0x00000200UL

typedef struct mf3d_package_report_s {
    char required[MF3D_EXT_MAX_REQUIRED][24];
    mf3d_u32 required_count;
    mf3d_u32 present_caps;
    mf3d_u32 unsupported_required_count;
    char warnings[MF3D_EXT_MAX_WARNINGS][96];
    mf3d_u32 warning_count;
} mf3d_package_report;

void mf3d_package_report_init(mf3d_package_report *r);
int mf3d_package_scan_model_xml(char *xml, mf3d_package_report *r);
int mf3d_package_add_warning(mf3d_package_report *r, const char *msg);
int mf3d_package_required_supported(const mf3d_package_report *r);
const char *mf3d_package_cap_name(mf3d_u32 cap);

#ifdef __cplusplus
}
#endif
#endif
