#include "re4c3d/re4_animation.h"
#include "re4c3d/re4_base.h"
#include <string.h>

re4_err re4_mot_ref_from_payload(re4_span payload, re4_mot_ref *out)
{
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (payload.size < 60UL) return RE4_ERR_RANGE;
    out->model_name = re4_strref_fixed(payload.data + 0UL, 12);
    out->motion_name = re4_strref_fixed(payload.data + 12UL, 48);
    return RE4_OK;
}
