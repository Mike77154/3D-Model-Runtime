#ifndef MD3_VALIDATE_H
#define MD3_VALIDATE_H

#include "md3_types.h"

#define MD3_VALIDATE_LAYOUT_GENERIC 0x0001U
#define MD3_VALIDATE_LAYOUT_CANONICAL 0x0002U

int md3_validate_header_info(const md3_header_info_t *header_info);
int md3_validate_surface_info(const md3_surface_info_t *surface_info,
    md3_i32 expected_num_frames);
int md3_validate_model_layout(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count,
    unsigned int flags);
int md3_surface_layout_is_canonical(const md3_surface_info_t *surface_info);
int md3_model_layout_is_canonical(const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos, md3_i32 surface_count);

#endif
