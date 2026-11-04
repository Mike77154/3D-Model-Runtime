#ifndef MD2_ERROR_H
#define MD2_ERROR_H

enum
{
    MD2_OK = 0,
    MD2_ERR_BAD_ARG = -1,
    MD2_ERR_IO = -2,
    MD2_ERR_BAD_MAGIC = -3,
    MD2_ERR_BAD_VERSION = -4,
    MD2_ERR_BAD_LAYOUT = -5,
    MD2_ERR_OVER_CAPACITY = -6,
    MD2_ERR_BAD_FRAME_SIZE = -7,
    MD2_ERR_BAD_FLOAT = -8,
    MD2_ERR_OVERFLOW = -9,
    MD2_ERR_BAD_INDEX = -10,
    MD2_ERR_BAD_GLCMD = -11,
    MD2_ERR_BAD_STRIDE = -12
};

const char *md2_strerror(int code);

#endif /* MD2_ERROR_H */
