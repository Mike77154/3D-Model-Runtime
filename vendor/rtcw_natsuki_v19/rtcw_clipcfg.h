#ifndef RTCW_CLIPCFG_H
#define RTCW_CLIPCFG_H
/* Conservative numeric animation.cfg frame-table reader (not AI script parsing). */
#include <stdio.h>
int rtcw_clipcfg_append(const char *input,const char *source,FILE *table,
                        unsigned long *rows);
#endif
