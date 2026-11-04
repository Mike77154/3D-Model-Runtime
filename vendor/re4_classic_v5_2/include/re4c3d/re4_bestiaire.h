#ifndef RE4_BESTIAIRE_H
#define RE4_BESTIAIRE_H
/* Character-family profiles for RE4 PC 2007. C89; no heap; no assumptions
   about unknown assets. Profiles are diagnostic, NOT proof of canonical parts. */
#include "re4_base.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RE4_GROUP_MINOR_BOSS 1
#define RE4_GROUP_HUMANOID 2
#define RE4_GROUP_GANADO 3
#define RE4_GROUP_PARASITE 4
#define RE4_GROUP_ANIMAL 5
#define RE4_GROUP_EXTRA 6
#define RE4_PROFILE_FAMILY_ONLY 1
#define RE4_PROFILE_VARIANT_GUARD 2
#define RE4_PROFILE_UNKNOWN_ASSETS 3
/* check_part: 1 = family + variant accepted, 0 = mismatched,
   -1 = family fits but sub-variant cannot be confirmed automatically. */
typedef struct re4_beast_profile {
    const char *slug;
    const char *name;
    const char *family;
    int group;
    int policy;
    const char *variant;
} re4_beast_profile;
re4_u32 re4_beast_count(void);
const re4_beast_profile *re4_beast_at(re4_u32 index);
const re4_beast_profile *re4_beast_find(const char *slug);
int re4_beast_check_part(const re4_beast_profile *profile, const char *path);
/* Validate a complete named set for confirmed test profiles.
   1 = allowed file-name set; 0 = bad/incomplete; -1 = further review.
   Passing never establishes animation/scene canonical fidelity. */
int re4_beast_validate_set(const re4_beast_profile *profile,
                           const char * const *parts, re4_u32 count);
#ifdef __cplusplus
}
#endif
#endif
