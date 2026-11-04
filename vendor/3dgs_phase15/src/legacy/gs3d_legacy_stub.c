#include "../gs3d_internal.h"
#if !GS3D_WITH_LEGACY
int gs_legacy_weight_info(unsigned long v,GS3D_WeightSet *w){(void)v;(void)w;return GS3D_ERR_UNSUPPORTED;}
int gs3d_legacy_deform(unsigned int group,unsigned int frame){(void)group;(void)frame;return GS3D_ERR_UNSUPPORTED;}
#endif
