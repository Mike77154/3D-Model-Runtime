/* xp_mszip.c - MSZIP scaffolding (arena/static storage)
   MIT License

   NOTE: MSZIP blocks begin with signature bytes 0x43 0x4B ("CK"),
   followed by DEFLATE data. Implementing RFC1951 inflate without runtime allocation
   is doable using fixed windows and fixed Huffman tables, but is TODO here.
*/
#include "xp_internal.h"

xp_result xp_mszip_check_signature(const unsigned char* two) {
    if (!two) return XP_EFORMAT;
    return (two[0]==0x43 && two[1]==0x4B) ? XP_OK : XP_EFORMAT;
}

/* TODO:
   - streaming inflate with fixed 32KB window buffer
   - support concatenated MSZIP blocks
*/
