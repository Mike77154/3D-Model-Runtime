/* xp_internal.h - internal prototypes (arena/static storage)
   MIT License
*/
#ifndef XP_INTERNAL_H
#define XP_INTERNAL_H

#include <string.h>
#include <ctype.h>
#include "xp.h"
#include "xp_mszip_stream.h"

/* provider/reader */
xp_result xpio_read_exact(xp_io* io, unsigned char* dst, unsigned int count);
xp_result xpr_init(xp_reader* r, xp_io* io);
xp_result xpr_peek(xp_reader* r, unsigned char* out);
xp_result xpr_get(xp_reader* r, unsigned char* out);
xp_result xpr_skip_ws_and_comments(xp_reader* r);

/* text lexer */
xp_result xpl_init(xp_lexer* lx, xp_reader* r);
xp_result xpl_next(xp_lexer* lx, xp_token* out);

/* binary lexer */
xp_result xpb_init(xp_blexer* bx, xp_reader* r, xp_numfmt numfmt);
xp_result xpb_next(xp_blexer* bx, xp_btoken* out);
xp_result xpb_read_u32_list(xp_blexer* bx, unsigned int count, unsigned int* out, unsigned int out_cap);
xp_result xpb_read_real_list(xp_blexer* bx, unsigned int count, xp_fx32* out, unsigned int out_cap);
xp_result xpb_discard_u32_list(xp_blexer* bx, unsigned int count);
xp_result xpb_discard_real_list(xp_blexer* bx, unsigned int count);

/* templates helpers */
int xp_is_mesh_type(const char* s);
int xp_is_mesh_normals_type(const char* s);
int xp_is_mesh_texcoords_type(const char* s);
int xp_is_mesh_colors_type(const char* s);
int xp_is_mesh_material_list_type(const char* s);
int xp_is_material_type(const char* s);
int xp_is_texture_filename_type(const char* s);
int xp_is_material_wrap_type(const char* s);
int xp_is_mesh_face_wraps_type(const char* s);
int xp_is_frame_type(const char* s);
int xp_is_frame_transform_matrix_type(const char* s);
int xp_is_skin_weights_type(const char* s);
int xp_is_skin_mesh_header_type(const char* s);
int xp_is_animation_set_type(const char* s);
int xp_is_animation_type(const char* s);
int xp_is_animation_key_type(const char* s);
int xp_is_anim_ticks_per_second_type(const char* s);
int xp_is_animation_options_type(const char* s);
int xp_is_vertex_duplication_indices_type(const char* s);
int xp_is_decl_data_type(const char* s);
int xp_is_fvf_data_type(const char* s);

int xp_streq_icase(const char* a, const char* b);

/* MSZIP signature helper; stream type/prototypes live in xp_mszip_stream.h. */
xp_result xp_mszip_check_signature(const unsigned char* two);

#endif
