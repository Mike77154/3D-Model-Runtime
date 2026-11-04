#ifndef RTCW_PK3_H
#define RTCW_PK3_H
/* Independent, bounded ZIP/PK3 reader; CC0; ISO C89; no heap. */
#define RTCW_PK3_PATH 240U
#define RTCW_PK3_DATA_MAX (32UL*1024UL*1024UL)
typedef struct rtcw_pk3_entry_s {
    char name[RTCW_PK3_PATH+1U];
    unsigned long uncompressed_size,compressed_size,local_offset,crc32;
    unsigned int method,flags;
    int is_directory;
} rtcw_pk3_entry;
int rtcw_pk3_open(const char *filename);
void rtcw_pk3_close(void);
unsigned long rtcw_pk3_count(void);
/* 1=entry, 0=end, -1=error; error has diagnostic. */
int rtcw_pk3_next(rtcw_pk3_entry *out);
/* Copy a single regular entry into the specified path, verify CRC32. */
int rtcw_pk3_extract(const rtcw_pk3_entry *entry,const char *output);
/* ZIP relative paths must be normalized, without absolute or .. components. */
int rtcw_pk3_safe_path(const char *name);
const char *rtcw_pk3_error(void);
#endif
