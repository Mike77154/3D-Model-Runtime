#ifndef GIFFANY_SCN3D_STDIO_H
#define GIFFANY_SCN3D_STDIO_H

#include <stdio.h>
#include "scn3d.h"

#ifdef __cplusplus
extern "C" {
#endif

void scn3d_io_from_FILE(scn3d_io *io, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
