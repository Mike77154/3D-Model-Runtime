#include <stdio.h>
#include "p3_error.h"

void p3_error_message(int code, char *dst) {
    if (dst == 0) {
        return;
    }
    switch (code) {
    case P3_OK:
        sprintf(dst, "ok");
        break;
    case P3_ERR_INVALID:
        sprintf(dst, "invalid data");
        break;
    case P3_ERR_TRUNCATED:
        sprintf(dst, "truncated data");
        break;
    case P3_ERR_CAPACITY:
        sprintf(dst, "capacity exceeded");
        break;
    case P3_ERR_SYNTAX:
        sprintf(dst, "syntax error");
        break;
    case P3_ERR_IO:
        sprintf(dst, "i/o error");
        break;
    case P3_ERR_RANGE:
        sprintf(dst, "numeric range error");
        break;
    case P3_ERR_UNSUPPORTED:
        sprintf(dst, "unsupported feature");
        break;
    default:
        sprintf(dst, "unknown error");
        break;
    }
}
