#include <stdio.h>
#include <string.h>
#include "p3_config.h"
#include "p3_error.h"
#include "p3_fileio.h"
#include "p3_egg_parser.h"
#include "p3_egg_model.h"
#include "p3_egg_writer.h"

static p3_u8 file_buf[P3_MAX_FILE_BYTES];
static char text_buf[P3_TEXT_IO_CAP];
static char err_buf[256];

int main(int argc, char **argv) {
    p3_u32 size;
    p3_u32 out_size;
    p3_egg_ast ast;
    p3_egg_model model;
    int rc;

    if (argc < 2 || argc > 3) {
        printf("usage: %s input.egg [output.egg]\n", argv[0]);
        return 1;
    }

    rc = p3_file_read_all(argv[1], file_buf, P3_MAX_FILE_BYTES, &size);
    if (rc != P3_OK) {
        p3_error_message(rc, err_buf);
        printf("read error: %s\n", err_buf);
        return 1;
    }

    rc = p3_egg_parse(&ast, (const char *)file_buf, size, err_buf, sizeof(err_buf));
    if (rc != P3_OK) {
        printf("parse error: %s\n", err_buf);
        return 1;
    }

    rc = p3_egg_model_from_ast(&model, &ast, err_buf, sizeof(err_buf));
    if (rc != P3_OK) {
        printf("model error: %s\n", err_buf);
        return 1;
    }

    p3_egg_model_summary(&model, text_buf, P3_TEXT_IO_CAP);
    printf("%s\n", text_buf);

    if (argc == 3) {
        rc = p3_egg_write_canonical(&ast, text_buf, P3_TEXT_IO_CAP, &out_size);
        if (rc != P3_OK) {
            p3_error_message(rc, err_buf);
            printf("write error: %s\n", err_buf);
            return 1;
        }
        rc = p3_file_write_all(argv[2], (const p3_u8 *)text_buf, out_size);
        if (rc != P3_OK) {
            p3_error_message(rc, err_buf);
            printf("save error: %s\n", err_buf);
            return 1;
        }
    }

    return 0;
}
