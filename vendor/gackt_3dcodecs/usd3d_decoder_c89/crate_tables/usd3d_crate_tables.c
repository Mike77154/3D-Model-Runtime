#include "usd3d_crate_tables.h"

#include <string.h>
#include <ctype.h>

static void ct_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int ct_is_ident_char(int c)
{
    return (isalnum(c) || c == '_' || c == ':' || c == '.' || c == '/' || c == '-' || c == '@');
}

static int ct_string_looks_path(const char *s)
{
    if (s == 0) return 0;
    if (s[0] == '/' || s[0] == '@') return 1;
    if (strstr(s, ".usd") != 0 || strstr(s, ".usda") != 0 || strstr(s, ".usdc") != 0 || strstr(s, ".png") != 0 || strstr(s, ".jpg") != 0 || strstr(s, ".exr") != 0 || strstr(s, ".vdb") != 0 || strstr(s, ".f3d") != 0) return 1;
    return 0;
}

void usd3d_crate_tables_init(usd3d_crate_tables *tables)
{
    if (tables == 0) return;
    memset(tables, 0, sizeof(*tables));
    tables->endian = USD3D_CRATE_ENDIAN_UNKNOWN;
}

int usd3d_crate_tables_read_le32(const unsigned char *data, unsigned long size, unsigned long offset, unsigned long *out_value)
{
    if (data == 0 || out_value == 0) return USD3D_ERR_BAD_ARG;
    if (offset + 4UL > size) return USD3D_ERR_LIMIT;
    *out_value = ((unsigned long)data[offset]) | (((unsigned long)data[offset + 1UL]) << 8) | (((unsigned long)data[offset + 2UL]) << 16) | (((unsigned long)data[offset + 3UL]) << 24);
    return USD3D_OK;
}

int usd3d_crate_tables_read_le64_pair(const unsigned char *data, unsigned long size, unsigned long offset, unsigned long *out_lo, unsigned long *out_hi)
{
    int r;
    if (out_lo == 0 || out_hi == 0) return USD3D_ERR_BAD_ARG;
    r = usd3d_crate_tables_read_le32(data, size, offset, out_lo);
    if (r != USD3D_OK) return r;
    r = usd3d_crate_tables_read_le32(data, size, offset + 4UL, out_hi);
    return r;
}

static int ct_add_section(usd3d_crate_tables *tables, const char *name, unsigned long offset, unsigned long size, int confidence)
{
    usd3d_crate_section *s;
    if (tables == 0 || name == 0) return USD3D_ERR_BAD_ARG;
    if (tables->section_count >= (unsigned long)USD3D_CRATE_TABLES_MAX_SECTIONS) return USD3D_ERR_LIMIT;
    s = &tables->sections[tables->section_count++];
    ct_copy(s->name, sizeof(s->name), name);
    s->offset = offset;
    s->size = size;
    s->confidence = confidence;
    return USD3D_OK;
}

static int ct_add_string(usd3d_crate_tables *tables, const char *text, unsigned long offset, unsigned long size)
{
    usd3d_crate_string *s;
    unsigned int i;
    int ident;
    if (tables == 0 || text == 0) return USD3D_ERR_BAD_ARG;
    if (tables->string_count >= (unsigned long)USD3D_CRATE_TABLES_MAX_STRINGS) return USD3D_ERR_LIMIT;
    s = &tables->strings[tables->string_count++];
    ct_copy(s->text, sizeof(s->text), text);
    s->offset = offset;
    s->size = size;
    s->looks_path = ct_string_looks_path(s->text);
    ident = 1;
    for (i = 0U; s->text[i] != '\0'; ++i) {
        if (!ct_is_ident_char((unsigned char)s->text[i])) {
            ident = 0;
            break;
        }
    }
    s->looks_identifier = ident;
    return USD3D_OK;
}

static void ct_scan_named_sections(const unsigned char *data, unsigned long size, usd3d_crate_tables *tables)
{
    static const char *names[] = {
        "TOKENS", "STRINGS", "FIELDS", "FIELDSETS", "SPECS", "PATHS", "ASSETS", "SUBLAYERS", "RELOCATES", "BOOTSTRAP", "TOC", 0
    };
    unsigned long i;
    unsigned int n;
    unsigned long len;
    for (n = 0U; names[n] != 0; ++n) {
        len = (unsigned long)strlen(names[n]);
        if (len == 0UL || size < len) continue;
        for (i = 0UL; i + len <= size; ++i) {
            if (memcmp(data + i, names[n], (size_t)len) == 0) {
                ct_add_section(tables, names[n], i, 0UL, 50);
                break;
            }
        }
    }
}

int usd3d_crate_tables_scan_memory(const unsigned char *data, unsigned long size, usd3d_crate_tables *tables)
{
    unsigned long i;
    unsigned long start;
    unsigned long n;
    char text[USD3D_CRATE_TABLES_TEXT_MAX];
    int c;
    unsigned int ti;

    if (data == 0 || tables == 0) return USD3D_ERR_BAD_ARG;
    usd3d_crate_tables_init(tables);
    if (size >= 8UL && memcmp(data, "PXR-USDC", 8) == 0) {
        tables->has_magic = 1;
        tables->endian = USD3D_CRATE_ENDIAN_LITTLE;
        tables->version_major = 0UL;
        tables->version_minor = 0UL;
        ct_add_section(tables, "MAGIC", 0UL, 8UL, 100);
    }
    ct_scan_named_sections(data, size, tables);

    i = 0UL;
    while (i < size) {
        c = (int)data[i];
        if (c >= 32 && c <= 126) {
            start = i;
            ti = 0U;
            while (i < size) {
                c = (int)data[i];
                if (!(c >= 32 && c <= 126)) break;
                if (ti + 1U < (unsigned int)sizeof(text)) text[ti++] = (char)c;
                i++;
            }
            text[ti] = '\0';
            n = i - start;
            tables->printable_bytes += n;
            if (n >= 3UL && ti > 0U) {
                ct_add_string(tables, text, start, n);
            }
            if (i < size && data[i] == 0U && n >= 3UL) tables->nul_terminated_strings++;
        } else {
            i++;
        }
    }
    return USD3D_OK;
}

int usd3d_crate_tables_emit(const usd3d_crate_tables *tables,
                            int (*section_cb)(void *user, const usd3d_crate_section *section),
                            int (*string_cb)(void *user, const usd3d_crate_string *string_value),
                            void *user)
{
    unsigned long i;
    if (tables == 0) return USD3D_ERR_BAD_ARG;
    if (section_cb != 0) {
        for (i = 0UL; i < tables->section_count; ++i) {
            if (section_cb(user, &tables->sections[i]) != 0) return USD3D_ERR_PARSE;
        }
    }
    if (string_cb != 0) {
        for (i = 0UL; i < tables->string_count; ++i) {
            if (string_cb(user, &tables->strings[i]) != 0) return USD3D_ERR_PARSE;
        }
    }
    return USD3D_OK;
}
