#ifndef P3_ERROR_H
#define P3_ERROR_H

enum {
    P3_OK = 0,
    P3_ERR_INVALID = -1,
    P3_ERR_TRUNCATED = -2,
    P3_ERR_CAPACITY = -3,
    P3_ERR_SYNTAX = -4,
    P3_ERR_IO = -5,
    P3_ERR_RANGE = -6,
    P3_ERR_UNSUPPORTED = -7
};

void p3_error_message(int code, char *dst);

#endif
