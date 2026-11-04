#ifndef RTCW_PREVIEW_H
#define RTCW_PREVIEW_H
/* CC0-1.0: offline, deterministic, user-directed animation preview state. */
#define RTCW_PREVIEW_STATE 32U
#define RTCW_PREVIEW_MAX_MS 3600000UL
#define RTCW_PREVIEW_MAX_SHOTS 96U
typedef struct rtcw_preview_channel_s {
    unsigned int enabled,clip_id;
    unsigned long start_ms,hold_ms;
} rtcw_preview_channel;
typedef struct rtcw_preview_s {
    char state[RTCW_PREVIEW_STATE];
    unsigned long now_ms;
    unsigned int visible_mask;
    rtcw_preview_channel legs,torso;
    char last_event[48];
    char last_sound[96];
    unsigned long last_command_line;
} rtcw_preview;
typedef struct rtcw_preview_sample_s {
    unsigned long frame_a,frame_b;
    unsigned int lerp_q12;
    char name[65];
} rtcw_preview_sample;
void rtcw_preview_reset(rtcw_preview *p);
int rtcw_preview_state(rtcw_preview *p,const char *name);
int rtcw_preview_route(rtcw_preview *p,const char *section,const char *name,const char *condition);
int rtcw_preview_event(rtcw_preview *p,const char *event,const char *condition);
int rtcw_preview_change(rtcw_preview *p,const char *new_state,const char *condition);
int rtcw_preview_apply_id(rtcw_preview *p,unsigned int cmd_id);
int rtcw_preview_advance(rtcw_preview *p,unsigned long delta);
int rtcw_preview_sample_channel(const rtcw_preview *p,const rtcw_preview_channel *ch,rtcw_preview_sample *out);
int rtcw_preview_render(const rtcw_preview *p);
const char *rtcw_preview_error(void);
#endif
