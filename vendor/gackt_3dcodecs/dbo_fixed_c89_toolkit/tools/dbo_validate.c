#include <stdio.h>
#include "dbo_tool_common.h"

static dbo_u8 g_input[DBO_TOOL_MAX_FILE_BYTES];

int main(int argc, char** argv)
{
    char err[DBO_MAX_ERROR_TEXT];
    dbo_u32 size;
    static dbo_scene scene;
    static dbo_validation_report report;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <input.dbo>\n", argv[0]);
        return 1;
    }

    if (!dbo_tool_load_scene(argv[1], &scene, g_input, (dbo_u32)sizeof(g_input), &size, err, (dbo_u32)sizeof(err)))
    {
        fprintf(stderr, "load failed: %s\n", err);
        return 1;
    }

    dbo_validate_scene(&scene, &report);

    printf("file: %s\n", argv[1]);
    printf("bytes: %lu\n", (unsigned long)size);
    printf("unknown chunks: %lu\n", (unsigned long)dbo_scene_unknown_chunk_count(&scene));
    printf("unknown bytes: %lu\n", (unsigned long)dbo_scene_unknown_bytes(&scene));
    printf("errors: %lu\n", (unsigned long)report.errors);
    printf("warnings: %lu\n", (unsigned long)report.warnings);
    if (report.errors > 0UL)
    {
        printf("first error: %s\n", report.first_error);
    }
    if (report.warnings > 0UL)
    {
        printf("first warning: %s\n", report.first_warning);
    }

    if (report.errors == 0UL)
    {
        puts("validation: ok");
        return 0;
    }

    puts("validation: failed");
    return 2;
}
