#ifndef P3_EGG_PARSER_H
#define P3_EGG_PARSER_H

#include "p3_egg_ast.h"
#include "p3_types.h"

int p3_egg_parse(p3_egg_ast *ast, const char *src, p3_u32 size, char *err, p3_u32 err_cap);

#endif
