#include "mot89.h"

#include <stdio.h>
#include <string.h>

#ifndef MOT89_DEMO_FILE_MAX
#define MOT89_DEMO_FILE_MAX 1048576UL
#endif

#ifndef MOT89_DEMO_ARENA_SIZE
#define MOT89_DEMO_ARENA_SIZE 1048576UL
#endif

static unsigned char file_mem[MOT89_DEMO_FILE_MAX];
static unsigned char arena_mem[MOT89_DEMO_ARENA_SIZE];

static void print_fixed(long v)
{
    char buf[64];
    if (mot89_fixed_to_dec4(v, buf, sizeof(buf)) == MOT89_OK) {
        printf("%s", buf);
    } else {
        printf("?");
    }
}

int main(int argc, char **argv)
{
    FILE *fp;
    size_t n;
    mot89_arena arena;
    mot89_motion *m;
    mot89_report rep;
    long out[MOT89_MAX_CHANNELS];
    int rc;
    int ch;
    long sample_time;

    if (argc < 2) {
        printf("usage: mot89_dump file.mot [frame]\n");
        return 2;
    }

    fp = fopen(argv[1], "rb");
    if (fp == (FILE *)0) {
        printf("could not open %s\n", argv[1]);
        return 2;
    }
    n = fread(file_mem, 1U, sizeof(file_mem), fp);
    fclose(fp);

    mot89_arena_init(&arena, arena_mem, sizeof(arena_mem));
    rc = mot89_parse((const char *)file_mem, n, &arena, &m);
    if (rc != MOT89_OK) {
        printf("parse error: %s\n", mot89_error_string(rc));
        return 1;
    }
    rc = mot89_validate(m, &rep);
    if (rc != MOT89_OK) {
        printf("validate error: %s\n", mot89_error_string(rc));
        return 1;
    }

    printf("type: %s\n", rep.type == MOT89_TYPE_ENV ? "LWEN" : "LWMO");
    printf("version: %d\n", rep.version);
    printf("channels: %d\n", rep.channels);
    printf("keys: %d\n", rep.key_count);
    printf("first_time: "); print_fixed(rep.first_time); printf("\n");
    printf("last_time: "); print_fixed(rep.last_time); printf("\n");
    printf("nonlinear_keys: %d\n", rep.nonlinear_keys);

    sample_time = rep.first_time;
    if (argc >= 3) {
        long f;
        f = 0L;
        {
            char *p;
            p = argv[2];
            while (*p >= '0' && *p <= '9') {
                f = f * 10L + (long)(*p - '0');
                p++;
            }
        }
        sample_time = mot89_fixed_from_int(f);
    }

    rc = mot89_sample_all(m, sample_time, out);
    if (rc != MOT89_OK) {
        printf("sample error: %s\n", mot89_error_string(rc));
        return 1;
    }
    printf("sample @ "); print_fixed(sample_time); printf("\n");
    for (ch = 0; ch < MOT89_MAX_CHANNELS; ch++) {
        printf("  %s = ", mot89_channel_name(ch));
        print_fixed(out[ch]);
        printf("\n");
    }
    return 0;
}
