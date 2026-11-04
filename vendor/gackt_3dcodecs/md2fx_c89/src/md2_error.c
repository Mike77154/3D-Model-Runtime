#include "md2/md2_error.h"

const char *md2_strerror(int code)
{
    switch (code)
    {
    case MD2_OK: return "ok";
    case MD2_ERR_BAD_ARG: return "bad argument";
    case MD2_ERR_IO: return "i/o error";
    case MD2_ERR_BAD_MAGIC: return "bad md2 magic";
    case MD2_ERR_BAD_VERSION: return "bad md2 version";
    case MD2_ERR_BAD_LAYOUT: return "bad md2 file layout";
    case MD2_ERR_OVER_CAPACITY: return "file exceeds static capacity";
    case MD2_ERR_BAD_FRAME_SIZE: return "bad frame size";
    case MD2_ERR_BAD_FLOAT: return "invalid or unsupported f32 bit pattern";
    case MD2_ERR_OVERFLOW: return "integer overflow";
    case MD2_ERR_BAD_INDEX: return "index out of range";
    case MD2_ERR_BAD_GLCMD: return "malformed gl command list";
    case MD2_ERR_BAD_STRIDE: return "bad buffer stride";
    default: return "unknown error";
    }
}
