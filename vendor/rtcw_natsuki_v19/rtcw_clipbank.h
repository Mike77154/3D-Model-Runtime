#ifndef RTCW_CLIPBANK_H
#define RTCW_CLIPBANK_H
/* CC0-1.0; ISO C89; bounded memory; no heap, no floating point. */
#define RTCW_CLIPS_MAX 512U
#define RTCW_CLIP_NAME 64U
typedef struct rtcw_clip_s {
    char name[RTCW_CLIP_NAME+1U];
    unsigned long line, first, count, loop, fps_milli;
    long move_speed;
    unsigned long blend, priority;
    int valid;
} rtcw_clip;
int rtcw_clipbank_read(const char *cfg,unsigned long frames);
unsigned int rtcw_clipbank_count(void);
const rtcw_clip *rtcw_clipbank_at(unsigned int idx);
const char *rtcw_clipbank_error(void);
int rtcw_clipbank_report(const char *output);
int rtcw_clip_elapsed_ms(unsigned long frame_offset,unsigned long fps_milli,unsigned long *ms);
#endif
