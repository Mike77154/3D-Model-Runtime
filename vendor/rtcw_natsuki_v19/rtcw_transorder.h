#ifndef RTCW_TRANSORDER_H
#define RTCW_TRANSORDER_H
/* CC0-1.0 fixed buffer sorting, no allocation. */
typedef struct rtcw_transitem_s {unsigned long triangle;long depth;} rtcw_transitem;
int rtcw_transorder_build(unsigned long count);
unsigned long rtcw_transorder_count(void);
const rtcw_transitem *rtcw_transorder_items(void);
#endif
