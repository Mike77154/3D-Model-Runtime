#ifndef MODELRT_PRJ89_H
#define MODELRT_PRJ89_H
#include <stddef.h>
/* Utility for CRS WKT/PRJ, not a 3D model provider.
   Uses only caller supplied workspace; no heap. ISO C89. */
size_t modelrt_prj89_workspace_bytes(void);
int modelrt_prj89_summary(const char *wkt,void *workspace,size_t workspace_bytes,
                         char *out,size_t output_bytes);
#endif
