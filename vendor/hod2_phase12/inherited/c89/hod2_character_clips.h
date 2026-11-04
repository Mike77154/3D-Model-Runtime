#ifndef HOD2_CHARACTER_CLIPS_H
#define HOD2_CHARACTER_CLIPS_H
/* Compiled from the two source executables and frame-stride validation.
 * C89: no malloc/heap/floats; use PROVENANCE to avoid selecting guesses.
 */
#define HOD2_CHARACTER_TYPES 86U
#define HOD2_CLIP_NONE 0U
#define HOD2_CLIP_EXE_DISPATCH 1U
#define HOD2_CLIP_PROVISIONAL 2U
#define HOD2_CLIP_BANK_STRIDE_CANDIDATE 3U
/* versions 0=1999, 1=second supplied EXE (filename says y2000). */
int hod2_character_clip(unsigned int version,unsigned int character_type,
                        unsigned int which,unsigned int *motion_id,
                        unsigned int *provenance);
#endif
