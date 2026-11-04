#ifndef MOT89_H
#define MOT89_H

/*
   mot89: LightWave MOT/LWEN motion loader and sampler.
   C89, caller-owned arena, fixed point only.
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOT89_VERSION_MAJOR 1
#define MOT89_VERSION_MINOR 0
#define MOT89_VERSION_PATCH 0

#define MOT89_ONE ((long)65536L)
#define MOT89_HALF ((long)32768L)
#define MOT89_MAX_CHANNELS 9

#define MOT89_TYPE_UNKNOWN 0
#define MOT89_TYPE_MOT 1
#define MOT89_TYPE_ENV 2

#define MOT89_OK 0
#define MOT89_E_NULL -1
#define MOT89_E_MAGIC -2
#define MOT89_E_VERSION -3
#define MOT89_E_CHANNELS -4
#define MOT89_E_KEYS -5
#define MOT89_E_NUMBER -6
#define MOT89_E_ARENA -7
#define MOT89_E_RANGE -8
#define MOT89_E_OVERFLOW -9
#define MOT89_E_INTERNAL -10

#define MOT89_FLAG_SORTED_INPUT 1
#define MOT89_FLAG_WAS_SORTED 2
#define MOT89_FLAG_NONLINEAR_KEYS 4
#define MOT89_FLAG_ENV 8
#define MOT89_FLAG_MOT 16

#define MOT89_CHAN_X 0
#define MOT89_CHAN_Y 1
#define MOT89_CHAN_Z 2
#define MOT89_CHAN_HEADING 3
#define MOT89_CHAN_PITCH 4
#define MOT89_CHAN_BANK 5
#define MOT89_CHAN_SCALE_X 6
#define MOT89_CHAN_SCALE_Y 7
#define MOT89_CHAN_SCALE_Z 8

typedef struct mot89_arena {
    unsigned char *base;
    size_t size;
    size_t used;
    int overflowed;
} mot89_arena;

typedef struct mot89_key {
    long values[MOT89_MAX_CHANNELS];
    long time;
    int linear_flag;
    long tension;
    long continuity;
    long bias;
} mot89_key;

typedef struct mot89_motion {
    int type;
    int version;
    int channels;
    int key_count;
    int flags;
    mot89_key *keys;
} mot89_motion;

typedef struct mot89_report {
    int type;
    int version;
    int channels;
    int key_count;
    int sorted;
    int nonlinear_keys;
    long first_time;
    long last_time;
} mot89_report;

void mot89_arena_init(mot89_arena *arena, void *buffer, size_t buffer_size);
void mot89_arena_reset(mot89_arena *arena);
void *mot89_arena_push(mot89_arena *arena, size_t size, size_t align_bytes);

const char *mot89_error_string(int code);
const char *mot89_channel_name(int channel_index);

long mot89_fixed_from_int(long value);
long mot89_fixed_from_ratio(long numerator, long denominator);
int mot89_fixed_to_dec4(long value, char *out, size_t out_cap);

int mot89_parse(const char *text, size_t text_size, mot89_arena *arena, mot89_motion **out_motion);
int mot89_validate(const mot89_motion *motion, mot89_report *out_report);

int mot89_sample_channel(const mot89_motion *motion, int channel_index, long time, long *out_value);
int mot89_sample_all(const mot89_motion *motion, long time, long out_values[MOT89_MAX_CHANNELS]);

int mot89_emit(const mot89_motion *motion, char *out_text, size_t out_cap, size_t *out_written);

#ifdef __cplusplus
}
#endif

#endif
