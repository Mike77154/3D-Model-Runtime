#ifndef C3FAR_H
#define C3FAR_H

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C3FAR_KIND_UNKNOWN       0U
#define C3FAR_KIND_ZIP           1U
#define C3FAR_KIND_RAR4          2U
#define C3FAR_KIND_RAR5          3U
#define C3FAR_KIND_FUCHSIA_FAR   4U
#define C3FAR_KIND_UNIX_AR       5U

unsigned int c3far_probe_archive(const unsigned char *data, c3dae_u32 len);
const char *c3far_kind_name(unsigned int kind);

#ifdef __cplusplus
}
#endif

#endif
