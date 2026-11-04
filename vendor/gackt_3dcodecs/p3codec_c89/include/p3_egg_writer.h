#ifndef P3_EGG_WRITER_H
#define P3_EGG_WRITER_H

#include "p3_egg_ast.h"
#include "p3_types.h"

int p3_egg_write_canonical(const p3_egg_ast *ast, char *dst, p3_u32 cap, p3_u32 *out_size);

#endif
