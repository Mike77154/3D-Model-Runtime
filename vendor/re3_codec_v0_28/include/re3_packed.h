#ifndef RE3_PACKED_H
#define RE3_PACKED_H
/* RE3 PC packed player/weapon assets. C89, fixed-size tables, no heap.
 * This loader recognizes five-section PLD and nine-section PLW containers;
 * it does not claim behavioral equivalence to the original game. */
#include "re3_codec.h"
#define RE3P_MAX_SEGMENTS 9u
#define RE3P_KIND_PLD 5u
#define RE3P_KIND_PLW 9u
typedef struct re3p_segment_s { re3c_u32 offset, size; } re3p_segment;
typedef struct re3p_info_s {
 re3c_u32 file_size, directory_offset, kind, segment_count;
 re3p_segment segments[RE3P_MAX_SEGMENTS];
} re3p_info;
int re3p_probe_file(const char *path, re3p_info *out);
/* Loads the MD2 into a standard re3c_model structure, plus normal-bank
 * EDD/EMR when supported; geometry-only fallback is flagged by bank.present.
 * The caller MUST call re3c_model_init before this function, and close when done. */
int re3p_open_model_file(re3c_model *model, const char *path, re3p_info *info);
/* Caller provides scratch to avoid allocating a heap or hidden large buffer.
 * PLD/PLW TIM content is copied into scratch, then decoded into tim. */
int re3p_load_tim(const char *path, const re3p_info *info,
                  re3c_u8 *scratch, size_t scratch_capacity, re3c_tim_vram *tim);
/* Copy two TIM uploads into a single PSX VRAM image (player first, then
 * weapon). Destination must be distinct from both inputs. Memory is supplied
 * by caller; each VRAM object is a fixed-size table. */
int re3p_compose_vram(re3c_tim_vram *dst,
                      const re3c_tim_vram *player,
                      const re3c_tim_vram *weapon);
#endif
