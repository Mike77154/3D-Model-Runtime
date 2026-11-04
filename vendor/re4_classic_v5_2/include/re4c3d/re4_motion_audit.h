#ifndef RE4_MOTION_AUDIT_H
#define RE4_MOTION_AUDIT_H
/* RE4 FCV diagnostic and provenance guards; C89, no heap. */
#include "re4_animation.h"
#include "re4_arena.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RE4_MOTION_NATIVE 1
#define RE4_MOTION_SHARED_IDENTICAL 2
#define RE4_MOTION_UNVERIFIED 0
#define RE4_MOTION_DENIED -1

typedef struct re4_motion_audit_s {
 re4_u16 max_frame;
 re4_u32 blocks;
 re4_u32 keys;
 re4_u32 rotation_blocks;
 re4_u32 root_blocks;
 re4_u32 ik_blocks;
 re4_u32 other_special_blocks;
 re4_u32 unknown_blocks;
 re4_u32 nonrotation_blocks;
} re4_motion_audit;
/* Returns RE4_OK for a structurally valid FCV and counts original channels.
   Rewinds scratch arena to its previous position even on parse failure. */
re4_err re4_motion_audit_fcv(re4_span src,re4_endian en,
                             re4_arena *scratch,re4_motion_audit *out);
/* Equality applies to ALL bytes of two valid FCVs. It does not prove
   character identity, source origin or valid retargeting. */
re4_err re4_motion_exact_fcv(re4_span a,re4_span b,re4_endian en,
                             re4_arena *scratch,int *identical);
/* Source-selection policy: native archive is accepted. Cross-archive
   reuse is allowed ONLY with explicit permission AND verified byte
   identity at that particular FCV index; even then retargeting is
   still a separate problem. Never infer head Plaga assets from em25. */
int re4_motion_source_policy(const char *target_archive,
                             const char *source_archive,
                             int verified_identical,
                             int allow_explicit_shared);
#ifdef __cplusplus
}
#endif
#endif
