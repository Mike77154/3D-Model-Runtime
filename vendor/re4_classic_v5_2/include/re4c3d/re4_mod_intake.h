#ifndef RE4_MOD_INTAKE_H
#define RE4_MOD_INTAKE_H
/* External RE4 mod package manifest. Pure parsing, no IO/heap/floats.
   Declaring a package does NOT make it engine compatible. */
#include "re4_base.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RE4_MOD_MAX_PARTS 16U
#define RE4_MOD_MAX_CLIPS 32U
#define RE4_MOD_MAX_TEXTURES 32U
#define RE4_MOD_PATH_CAP 128U
#define RE4_MOD_PMD2007 1U
#define RE4_MOD_BIN_UHD 2U
#define RE4_MOD_HEADER "R4MOD1"
typedef struct re4_mod_asset_s {
 char path[RE4_MOD_PATH_CAP];
} re4_mod_asset;
typedef struct re4_mod_package_s {
 re4_u32 format;
 re4_mod_asset master;
 re4_mod_asset parts[RE4_MOD_MAX_PARTS];
 re4_mod_asset clips[RE4_MOD_MAX_CLIPS];
 re4_mod_asset textures[RE4_MOD_MAX_TEXTURES];
 re4_u32 part_count,clip_count,texture_count;
} re4_mod_package;
/* A path must be RELATIVE, normalized, and traversal-free.
   Backslash is permitted but normalized to slash by the parser. */
int re4_mod_path_safe(const char *path);
/* Bounded ASCII text manifest; rejects unknown keys, duplicates, paths
   with traversal, missing model, invalid extensions, and mixed formats.
   Return RE4_OK only for a syntactically sound package declaration. */
re4_err re4_mod_parse(re4_span text,re4_mod_package *out);
#ifdef __cplusplus
}
#endif
#endif
