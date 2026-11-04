#include "../src/modelrt_internal.h"
#include <stdio.h>

static int probe_yes(const char *p, const modelrt_u8 *h, size_t n)
{
    (void)p; (void)h; (void)n; return MODELRT_PROBE_YES;
}
static int probe_maybe(const char *p, const modelrt_u8 *h, size_t n)
{
    (void)p; (void)h; (void)n; return MODELRT_PROBE_MAYBE;
}
static int open_wrong(modelrt_runtime *rt, modelrt_handle *h, const char *p)
{
    (void)h; (void)p;
    modelrt_set_error(rt, "wrong-provider", MODELRT_ERR_FORMAT, "synthetic false positive");
    return MODELRT_ERR_FORMAT;
}
static int open_right(modelrt_runtime *rt, modelrt_handle *h, const char *p)
{
    (void)rt; (void)p;
    h->info.mesh_count = 7u;
    return MODELRT_OK;
}
static void close_noop(modelrt_handle *h) { (void)h; }

static const modelrt_provider wrong_provider = {
    100u, "wrong-provider", probe_yes, open_wrong, close_noop,
    0,0,0,0,0,0,0,0,0,0
};
static const modelrt_provider right_provider = {
    101u, "right-provider", probe_maybe, open_right, close_noop,
    0,0,0,0,0,0,0,0,0,0
};

static unsigned char workspace[4096];

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    rt.provider_count = 2u;
    rt.providers[0] = &wrong_provider;
    rt.providers[1] = &right_provider;
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("fallback failed: %s\n", modelrt_last_error(&rt));
        return 3;
    }
    if (h.info.codec_id != 101u || h.info.mesh_count != 7u) return 4;
    if (h.provider != &right_provider) return 5;
    puts("Provider fallback regression OK: false-positive FORMAT -> next candidate");
    return 0;
}
