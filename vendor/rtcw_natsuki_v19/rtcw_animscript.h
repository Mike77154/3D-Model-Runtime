#ifndef RTCW_ANIMSCRIPT_H
#define RTCW_ANIMSCRIPT_H
/* Original white-room implementation CC0. File scanner / offline association, NOT gameplay. */
#define RTCW_SCRIPT_MAX_COMMANDS 2048U
#define RTCW_SCRIPT_NAME 95U
#define RTCW_SCRIPT_CONDITION 255U
typedef struct rtcw_script_command_s {
    char section[24];
    char state[32];
    char movement[32];
    char event[48];
    char old_state[32];
    char new_state[32];
    char condition[RTCW_SCRIPT_CONDITION+1U];
    char body[2][8];
    char clip[2][RTCW_SCRIPT_NAME+1U];
    char sound[RTCW_SCRIPT_NAME+1U];
    unsigned long duration[2];
    unsigned long line;
    unsigned int parts, sound_only;
    unsigned int clip_matches[2], playable[2];
    unsigned int showparts, hideparts;
} rtcw_script_command;
int rtcw_animscript_read(const char *script);
const char *rtcw_animscript_error(void);
unsigned int rtcw_animscript_count(void);
unsigned int rtcw_animscript_unresolved(void);
const rtcw_script_command *rtcw_animscript_at(unsigned int i);
int rtcw_animscript_report(const char *path);
#endif
