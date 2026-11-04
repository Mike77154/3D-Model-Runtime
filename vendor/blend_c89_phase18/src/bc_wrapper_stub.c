/* Portable fallback for platforms without a statically bundled decoder. */
#include "bc.h"
int bc_unwrap(BC_File *b,int algorithm)
{
  (void)b; (void)algorithm;
  return BC_FAILURE;
}
