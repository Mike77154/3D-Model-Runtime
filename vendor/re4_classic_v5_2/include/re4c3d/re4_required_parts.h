#ifndef RE4_REQUIRED_PARTS_H
#define RE4_REQUIRED_PARTS_H
/* Character part preflight; C89, no allocation. Names are PMD BASENAMES.
   This prevents accidentally omitting a separately stored component.
   It DOES NOT establish semantic role or correct bone retargeting. */
#include "re4c3d/re4_types.h"
int re4_required_parts_check(const char * const *attached, re4_u32 attached_count,
                             const char * const *required, re4_u32 required_count,
                             re4_u32 *first_bad);
#endif
