#include "c4d89_internal.h"
#include <string.h>
int c4d89_magic(FILE *f)
{
    unsigned char h[8];
    if (fseek(f, 0L, SEEK_SET) != 0 || fread(h, 1u, 8u, f) != 8u) return 0;
    if (memcmp(h,"XC4DC4D6",8u)==0) return 1;
    if (memcmp(h,"QC4DC4D6",8u)==0) return 2;
    return 0;
}
int c4d89_probe(const char *path)
{
    FILE *f; int n;
    if (!path) return 0;
    f=fopen(path,"rb"); if (!f) return 0;
    n=c4d89_magic(f); fclose(f); return n;
}
const char *c4d89_error(int status)
{
    switch (status) {
    case C4D89_OK: return "OK";
    case C4D89_ERR_IO: return "I/O error";
    case C4D89_ERR_SIGNATURE: return "Not an XC4DC4D6/QC4DC4D6 file";
    case C4D89_ERR_LIMIT: return "Input exceeds supported file size limits";
    case C4D89_ERR_UNSUPPORTED: return "No supported native mesh records found (or format variant unknown)";
    case C4D89_ERR_CORRUPT: return "Malformed/truncated native data";
    case C4D89_ERR_SINK: return "Game engine sink rejected data";
    default: return "Unknown error";
    }
}
