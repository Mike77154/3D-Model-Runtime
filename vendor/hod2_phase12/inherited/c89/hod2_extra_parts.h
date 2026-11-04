#ifndef HOD2_EXTRA_PARTS_H
#define HOD2_EXTRA_PARTS_H
/* HOD2 extra-part descriptor reader. Strict C89, no heap and no floating point.
 * Inputs are the original PE executable bytes. Caller owns the input buffer.
 * 1999 profile = Hod2 - y99.exe; 2000 profile = Hod2 - y2000.exe.
 */
#include <stddef.h>
#define HOD2EX_VERSION_1999 0
#define HOD2EX_VERSION_2000 1
#define HOD2EX_MAX_PARTS 31
#define HOD2EX_MAX_GROUPS 4
#define HOD2EX_STATUS_OK 1
#define HOD2EX_STATUS_EMPTY 0
#define HOD2EX_STATUS_ERROR (-1)
#define HOD2EX_DRAW_UNSUPPORTED 0
#define HOD2EX_DRAW_GROUP0 1
#define HOD2EX_DRAW_GROUP2 2
#define HOD2EX_DRAW_ALL 3

typedef struct Hod2ExtraExe {
 const unsigned char *data;
 size_t size;
 int profile;
} Hod2ExtraExe;

typedef struct Hod2ExtraPart {
 unsigned long slot;
 unsigned long mesh_info_va;
 unsigned long drawer_va;
 unsigned long source_vertex_count[4];
 unsigned long source_vertices_va[4];
 unsigned long assignment_va[4];
 long group_bone[4];
 long draw_bone;
 int draw_mode;
 unsigned long row_count;
 unsigned long row_ptr_count;
 unsigned long row_header_bytes;
 unsigned long row_stride;
 unsigned long rows_va;
} Hod2ExtraPart;

typedef struct Hod2ExtraVertex {
 long bone;
 int has_override;
 unsigned long raw_position_normal[6]; /* original IEEE754 bits, no float needed */
} Hod2ExtraVertex;

void hod2ex_init(Hod2ExtraExe *ctx, const unsigned char *data, size_t n, int profile);
int hod2ex_count(const Hod2ExtraExe *ctx, unsigned int type, unsigned int *n);
int hod2ex_read(const Hod2ExtraExe *ctx, unsigned int type, unsigned int part_index, Hod2ExtraPart *out);
/* "source_offset" is byte offset from POL model root to original vertex data.
 * Status 1: a valid bone mapping; has_override=1 if from a deformed group.
 * Status 0: vertex not found in extra-part row table; use stored POL vertex
 * and draw_bone. Negative: malformed data.
 */
int hod2ex_vertex(const Hod2ExtraExe *ctx, const Hod2ExtraPart *part,
                  unsigned long source_offset, Hod2ExtraVertex *out);
#endif
