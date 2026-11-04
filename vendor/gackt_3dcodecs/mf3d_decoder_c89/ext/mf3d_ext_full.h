#ifndef MF3D_EXT_FULL_H
#define MF3D_EXT_FULL_H

#include "../src/mf3d.h"
#include "mf3d_ext_package.h"
#include "mf3d_ext_materials.h"
#include "mf3d_ext_metadata.h"
#include "mf3d_ext_components.h"
#include "mf3d_ext_production.h"
#include "mf3d_ext_beam_lattice.h"
#include "mf3d_ext_slice.h"
#include "mf3d_ext_validate.h"
#include "mf3d_ext_boolean.h"
#include "mf3d_ext_displacement.h"
#include "mf3d_ext_volumetric.h"
#include "mf3d_ext_secure_content.h"
#include "mf3d_ext_decode_secure.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_full_scene_s {
    mf3d_model core;
    mf3d_package_report package;
    mf3d_materials materials;
    mf3d_metadata metadata;
    mf3d_components components;
    mf3d_production production;
    mf3d_beam_lattice beam_lattices;
    mf3d_slices slices;
    mf3d_validate_report validation;
    mf3d_booleans booleans;
    mf3d_displacement displacement;
    mf3d_volumetric volumetric;
    mf3d_secure_content secure_content;
    mf3d_u32 parse_mask;
} mf3d_full_scene;

#define MF3D_FULL_CORE          0x00000001UL
#define MF3D_FULL_PACKAGE       0x00000002UL
#define MF3D_FULL_MATERIALS     0x00000004UL
#define MF3D_FULL_METADATA      0x00000008UL
#define MF3D_FULL_COMPONENTS    0x00000010UL
#define MF3D_FULL_PRODUCTION    0x00000020UL
#define MF3D_FULL_BEAMLATTICE   0x00000040UL
#define MF3D_FULL_SLICE         0x00000080UL
#define MF3D_FULL_VALIDATE      0x00000100UL
#define MF3D_FULL_BOOLEAN       0x00000200UL
#define MF3D_FULL_DISPLACEMENT  0x00000400UL
#define MF3D_FULL_VOLUMETRIC    0x00000800UL
#define MF3D_FULL_SECURECONTENT 0x00001000UL

void mf3d_full_scene_init(mf3d_full_scene *s);
int mf3d_full_parse_model_xml(char *xml, mf3d_full_scene *out_scene);
int mf3d_full_decode_3mf(const mf3d_u8 *data, mf3d_u32 size,
                         mf3d_full_scene *out_scene,
                         mf3d_secure_decode_options *opt);
int mf3d_full_flatten_build(const mf3d_full_scene *scene, mf3d_model *out_flat);

#ifdef __cplusplus
}
#endif
#endif
