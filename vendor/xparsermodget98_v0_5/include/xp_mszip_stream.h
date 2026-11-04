/* xp_mszip_stream.h - MSZIP streaming API compatibility header
   Canonical xp_mszip_stream storage is declared once in xp.h.
   MIT License
*/
#ifndef XP_MSZIP_STREAM_H
#define XP_MSZIP_STREAM_H

#include "xp.h"

#if XP_ENABLE_MSZIP
xp_result xpm_init(xp_mszip_stream* s, xp_io* io);
xp_result xpm_read(xp_mszip_stream* s, unsigned char* dst, unsigned int cap, unsigned int* out_n);
#endif

#endif
