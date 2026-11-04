#include "chiqui_amf.h"
#include <stdio.h>
#include <string.h>

#define AMF_CONV_INPUT_CAP  (8u * 1024u * 1024u)
#define AMF_CONV_ARENA_CAP  (16u * 1024u * 1024u)
#define AMF_CONV_OUTPUT_CAP (32u * 1024u * 1024u)

static unsigned char input_mem[AMF_CONV_INPUT_CAP];
static unsigned char arena_mem[AMF_CONV_ARENA_CAP];
static unsigned char output_mem[AMF_CONV_OUTPUT_CAP];
static char xml_mem[AMF_CONV_OUTPUT_CAP / 2u];

static int ends_with(const char *s, const char *suffix)
{
    size_t ns;
    size_t nf;
    ns = strlen(s);
    nf = strlen(suffix);
    if (ns < nf) return 0;
    return memcmp(s + ns - nf, suffix, nf) == 0;
}

static int write_file(const char *path, const unsigned char *data, size_t len)
{
    FILE *f;
    size_t w;
    f = fopen(path, "wb");
    if (f == 0) return 0;
    w = fwrite(data, 1u, len, f);
    fclose(f);
    return w == len;
}

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    size_t out_len;
    amf_arena arena;
    amf_document doc;
    amf_diag diag;
    int r;
    if (argc < 3) {
        printf("usage: amf_convert input.amf output.obj|output.stl|output.amf|output.zip.amf\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (f == 0) {
        printf("could not open %s\n", argv[1]);
        return 2;
    }
    n = fread(input_mem, 1u, sizeof(input_mem), f);
    fclose(f);
    if (n == sizeof(input_mem)) {
        printf("input too large for static buffer\n");
        return 2;
    }
    amf_arena_init(&arena, arena_mem, sizeof(arena_mem));
    r = amf_parse_memory(&doc, &arena, input_mem, n, 0, &diag);
    if (r != AMF_OK) {
        printf("parse failed: %s line %lu col %lu: %s\n", amf_error_name(r), (unsigned long)diag.line, (unsigned long)diag.column, diag.message);
        return 1;
    }
    if (ends_with(argv[2], ".obj")) {
        r = amf_write_obj(&doc, (char *)output_mem, sizeof(output_mem), &out_len);
    } else if (ends_with(argv[2], ".stl")) {
        r = amf_write_stl_ascii(&doc, "amf_export", (char *)output_mem, sizeof(output_mem), &out_len);
    } else if (ends_with(argv[2], ".zip.amf") || ends_with(argv[2], ".amf.zip")) {
        size_t xml_len;
        r = amf_write_xml(&doc, xml_mem, sizeof(xml_mem), &xml_len);
        if (r == AMF_OK) {
            r = amf_write_zip_stored_xml("model.amf", (const amf_u8 *)xml_mem, xml_len, output_mem, sizeof(output_mem), &out_len);
        }
    } else {
        r = amf_write_xml(&doc, (char *)output_mem, sizeof(output_mem), &out_len);
    }
    if (r != AMF_OK) {
        printf("write failed: %s\n", amf_error_name(r));
        return 1;
    }
    if (!write_file(argv[2], output_mem, out_len)) {
        printf("could not write %s\n", argv[2]);
        return 2;
    }
    printf("wrote %lu bytes to %s\n", (unsigned long)out_len, argv[2]);
    return 0;
}
