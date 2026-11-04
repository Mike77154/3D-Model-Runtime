#ifndef CHARACTER_DRAW_BONE_PART_H
#define CHARACTER_DRAW_BONE_PART_H
#include "../inherited/c89/hod2_extra_parts.h"
/* Strict-C89 binding to CharacterDrawBonePart (generic, not zombie-only).
 * Copies untouched POL IEEE754 position/normal bits or genuine EXE override.
 * Does NOT infer missing bones, bridge holes, or draw custom unsupported subparts.
 */
typedef struct CharacterDrawBonePartResult {
 long bone;
 int original_vertex;
 int supported;
 unsigned long xyz_nxyz[6];
} CharacterDrawBonePartResult;
int CharacterDrawBonePart(const Hod2ExtraExe *exe,const Hod2ExtraPart *part,
                          unsigned long source_vertex_offset,
                          const unsigned char *original_vertex32,
                          CharacterDrawBonePartResult *result);
#endif
