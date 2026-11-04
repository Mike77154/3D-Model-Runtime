#include "uc3d.h"

static void upkg_zero(void *ptr, unsigned long size)
{
    unsigned char *p;
    unsigned long i;
    p = (unsigned char *)ptr;
    for (i = 0UL; i < size; ++i) {
        p[i] = 0U;
    }
}

static unsigned long upkg_len(const char *s)
{
    unsigned long n;
    if (s == (const char *)0) {
        return 0UL;
    }
    n = 0UL;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static void upkg_copy_n(char *dst, unsigned long cap, const char *src, unsigned long n)
{
    unsigned long i;
    unsigned long max;
    if (dst == (char *)0 || cap == 0UL) {
        return;
    }
    max = cap - 1UL;
    if (n < max) {
        max = n;
    }
    for (i = 0UL; i < max; ++i) {
        dst[i] = src[i];
    }
    dst[max] = '\0';
}

static void upkg_copy(char *dst, unsigned long cap, const char *src)
{
    upkg_copy_n(dst, cap, src, upkg_len(src));
}

static char upkg_upper(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static int upkg_eq_ci(const char *a, const char *b)
{
    unsigned long i;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    i = 0UL;
    while (a[i] != '\0' && b[i] != '\0') {
        if (upkg_upper(a[i]) != upkg_upper(b[i])) {
            return 0;
        }
        ++i;
    }
    return a[i] == b[i];
}

static int upkg_contains_ci(const char *text, const char *needle)
{
    unsigned long i;
    unsigned long j;
    unsigned long tn;
    unsigned long nn;
    if (text == (const char *)0 || needle == (const char *)0) {
        return 0;
    }
    tn = upkg_len(text);
    nn = upkg_len(needle);
    if (nn == 0UL) {
        return 1;
    }
    if (tn < nn) {
        return 0;
    }
    for (i = 0UL; i + nn <= tn; ++i) {
        for (j = 0UL; j < nn; ++j) {
            if (upkg_upper(text[i + j]) != upkg_upper(needle[j])) {
                break;
            }
        }
        if (j == nn) {
            return 1;
        }
    }
    return 0;
}

static void upkg_append(char *dst, unsigned long cap, const char *src)
{
    unsigned long d;
    unsigned long s;
    if (dst == (char *)0 || cap == 0UL || src == (const char *)0) {
        return;
    }
    d = upkg_len(dst);
    s = 0UL;
    while (src[s] != '\0' && d + 1UL < cap) {
        dst[d++] = src[s++];
    }
    dst[d] = '\0';
}

static unsigned long upkg_rd32le(const unsigned char *p)
{
    return ((unsigned long)p[0]) |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static long upkg_rd32s_le(const unsigned char *p)
{
    unsigned long u;
    unsigned long mag;
    u = upkg_rd32le(p);
    if ((u & 0x80000000UL) != 0UL) {
        mag = ((~u) & 0xFFFFFFFFUL) + 1UL;
        return -(long)mag;
    }
    return (long)u;
}

static int upkg_range_ok(unsigned long off, unsigned long need, unsigned long size)
{
    if (off > size) {
        return 0;
    }
    if (need > size - off) {
        return 0;
    }
    return 1;
}

static unsigned long upkg_crc32(const unsigned char *data, unsigned long size)
{
    unsigned long crc;
    unsigned long i;
    unsigned int bit;
    crc = 0xFFFFFFFFUL;
    for (i = 0UL; i < size; ++i) {
        crc ^= (unsigned long)data[i];
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 1UL) != 0UL) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }
    return (crc ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}

void uc3d_upkg_init(uc3d_upkg *pkg)
{
    if (pkg != (uc3d_upkg *)0) {
        upkg_zero(pkg, sizeof(*pkg));
    }
}

int uc3d_upkg_read_compact_index(const unsigned char *data,
                                 unsigned long data_size,
                                 unsigned long *offset,
                                 long *out_value)
{
    unsigned long pos;
    unsigned long value;
    unsigned int shift;
    unsigned char b;
    unsigned int count;
    int neg;

    if (data == (const unsigned char *)0 || offset == (unsigned long *)0 || out_value == (long *)0) {
        return UC3D_ERR_NULL;
    }
    pos = *offset;
    if (pos >= data_size) {
        return UC3D_ERR_TRUNCATED;
    }

    b = data[pos++];
    neg = (b & 0x80U) != 0U;
    value = (unsigned long)(b & 0x3FU);
    shift = 6U;
    count = 1U;

    if ((b & 0x40U) != 0U) {
        for (;;) {
            if (pos >= data_size) {
                return UC3D_ERR_TRUNCATED;
            }
            b = data[pos++];
            value |= ((unsigned long)(b & 0x7FU)) << shift;
            ++count;
            if ((b & 0x80U) == 0U || count >= 5U) {
                break;
            }
            shift = (unsigned int)(shift + 7U);
        }
    }

    *offset = pos;
    if (neg) {
        *out_value = -(long)value;
    } else {
        *out_value = (long)value;
    }
    return UC3D_OK;
}

static const char *upkg_name_at(const uc3d_upkg *pkg, long idx)
{
    if (pkg == (const uc3d_upkg *)0) {
        return "";
    }
    if (idx < 0L || (unsigned long)idx >= pkg->name_count) {
        return "";
    }
    return pkg->names[(unsigned long)idx].name;
}

static void upkg_resolve_ref_inner(const uc3d_upkg *pkg,
                                   long object_ref,
                                   char *out,
                                   unsigned long out_size,
                                   unsigned int depth)
{
    unsigned long idx;
    char parent[UC3D_MAX_REF_PATH];

    if (out == (char *)0 || out_size == 0UL) {
        return;
    }
    out[0] = '\0';
    if (pkg == (const uc3d_upkg *)0) {
        return;
    }
    if (object_ref == 0L) {
        upkg_copy(out, out_size, "None");
        return;
    }
    if (depth > 16U) {
        upkg_copy(out, out_size, "<ref-loop>");
        return;
    }

    if (object_ref < 0L) {
        idx = (unsigned long)(-object_ref - 1L);
        if (idx >= pkg->import_count) {
            upkg_copy(out, out_size, "<bad-import-ref>");
            return;
        }
        if (pkg->imports[idx].package_index != 0L) {
            upkg_resolve_ref_inner(pkg, pkg->imports[idx].package_index, parent, sizeof(parent), depth + 1U);
            if (parent[0] != '\0' && !upkg_eq_ci(parent, "None")) {
                upkg_copy(out, out_size, parent);
                upkg_append(out, out_size, ".");
            }
        }
        upkg_append(out, out_size, pkg->imports[idx].object_name_text);
        return;
    }

    idx = (unsigned long)(object_ref - 1L);
    if (idx >= pkg->export_count) {
        upkg_copy(out, out_size, "<bad-export-ref>");
        return;
    }
    if (pkg->exports[idx].package_index != 0L) {
        upkg_resolve_ref_inner(pkg, pkg->exports[idx].package_index, parent, sizeof(parent), depth + 1U);
        if (parent[0] != '\0' && !upkg_eq_ci(parent, "None")) {
            upkg_copy(out, out_size, parent);
            upkg_append(out, out_size, ".");
        }
    }
    upkg_append(out, out_size, pkg->exports[idx].object_name_text);
}

int uc3d_upkg_resolve_object_ref(const uc3d_upkg *pkg,
                                 long object_ref,
                                 char *out,
                                 unsigned long out_size)
{
    if (pkg == (const uc3d_upkg *)0 || out == (char *)0) {
        return UC3D_ERR_NULL;
    }
    upkg_resolve_ref_inner(pkg, object_ref, out, out_size, 0U);
    return UC3D_OK;
}

static int upkg_parse_name_table(const unsigned char *data,
                                 unsigned long data_size,
                                 uc3d_upkg *pkg)
{
    unsigned long off;
    unsigned long i;
    unsigned long len;
    unsigned long store_count;
    off = pkg->name_offset;
    store_count = pkg->declared_name_count;
    if (store_count > UC3D_MAX_PKG_NAMES) {
        store_count = UC3D_MAX_PKG_NAMES;
        pkg->overflow_count++;
    }
    if (pkg->declared_name_count > 100000UL) {
        return UC3D_ERR_RANGE;
    }
    for (i = 0UL; i < pkg->declared_name_count; ++i) {
        if (!upkg_range_ok(off, 1UL, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        if (i < store_count) {
            pkg->names[i].file_offset = off;
        }
        len = (unsigned long)data[off++];
        if (len == 0UL) {
            return UC3D_ERR_BAD_FORMAT;
        }
        if (!upkg_range_ok(off, len, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        if (i < store_count) {
            if (len > 0UL) {
                upkg_copy_n(pkg->names[i].name, sizeof(pkg->names[i].name), (const char *)(data + off), len - 1UL);
            }
        }
        off += len;
        if (!upkg_range_ok(off, 4UL, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        if (i < store_count) {
            pkg->names[i].flags = upkg_rd32le(data + off);
        }
        off += 4UL;
    }
    pkg->name_count = store_count;
    return UC3D_OK;
}

static int upkg_parse_import_table(const unsigned char *data,
                                   unsigned long data_size,
                                   uc3d_upkg *pkg)
{
    unsigned long off;
    unsigned long i;
    unsigned long store_count;
    int r;
    uc3d_upkg_import *im;

    off = pkg->import_offset;
    store_count = pkg->declared_import_count;
    if (store_count > UC3D_MAX_PKG_IMPORTS) {
        store_count = UC3D_MAX_PKG_IMPORTS;
        pkg->overflow_count++;
    }
    if (pkg->declared_import_count > 100000UL) {
        return UC3D_ERR_RANGE;
    }
    for (i = 0UL; i < pkg->declared_import_count; ++i) {
        long class_package;
        long class_name;
        long object_name;
        long package_index;
        unsigned long item_off;

        item_off = off;
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &class_package);
        if (r != UC3D_OK) {
            return r;
        }
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &class_name);
        if (r != UC3D_OK) {
            return r;
        }
        if (!upkg_range_ok(off, 4UL, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        package_index = upkg_rd32s_le(data + off);
        off += 4UL;
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &object_name);
        if (r != UC3D_OK) {
            return r;
        }
        if (i < store_count) {
            im = &pkg->imports[i];
            im->file_offset = item_off;
            im->class_package = class_package;
            im->class_name = class_name;
            im->package_index = package_index;
            im->object_name = object_name;
            upkg_copy(im->class_package_name, sizeof(im->class_package_name), upkg_name_at(pkg, class_package));
            upkg_copy(im->class_name_text, sizeof(im->class_name_text), upkg_name_at(pkg, class_name));
            upkg_copy(im->object_name_text, sizeof(im->object_name_text), upkg_name_at(pkg, object_name));
        }
    }
    pkg->import_count = store_count;
    return UC3D_OK;
}

static int upkg_parse_export_table(const unsigned char *data,
                                   unsigned long data_size,
                                   uc3d_upkg *pkg)
{
    unsigned long off;
    unsigned long i;
    unsigned long store_count;
    int r;
    uc3d_upkg_export *ex;

    off = pkg->export_offset;
    store_count = pkg->declared_export_count;
    if (store_count > UC3D_MAX_PKG_EXPORTS) {
        store_count = UC3D_MAX_PKG_EXPORTS;
        pkg->overflow_count++;
    }
    if (pkg->declared_export_count > 100000UL) {
        return UC3D_ERR_RANGE;
    }
    for (i = 0UL; i < pkg->declared_export_count; ++i) {
        long class_index;
        long super_index;
        long object_name;
        long serial_size;
        long serial_offset;
        long package_index;
        unsigned long object_flags;
        unsigned long item_off;

        item_off = off;
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &class_index);
        if (r != UC3D_OK) {
            return r;
        }
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &super_index);
        if (r != UC3D_OK) {
            return r;
        }
        if (!upkg_range_ok(off, 4UL, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        package_index = upkg_rd32s_le(data + off);
        off += 4UL;
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &object_name);
        if (r != UC3D_OK) {
            return r;
        }
        if (!upkg_range_ok(off, 4UL, data_size)) {
            return UC3D_ERR_TRUNCATED;
        }
        object_flags = upkg_rd32le(data + off);
        off += 4UL;
        r = uc3d_upkg_read_compact_index(data, data_size, &off, &serial_size);
        if (r != UC3D_OK) {
            return r;
        }
        serial_offset = 0L;
        if (serial_size > 0L) {
            r = uc3d_upkg_read_compact_index(data, data_size, &off, &serial_offset);
            if (r != UC3D_OK) {
                return r;
            }
        }
        if (i < store_count) {
            ex = &pkg->exports[i];
            ex->file_offset = item_off;
            ex->class_index = class_index;
            ex->super_index = super_index;
            ex->package_index = package_index;
            ex->object_name = object_name;
            ex->object_flags = object_flags;
            ex->serial_size = serial_size;
            ex->serial_offset = serial_offset;
            upkg_copy(ex->object_name_text, sizeof(ex->object_name_text), upkg_name_at(pkg, object_name));
            if (serial_size > 0L && serial_offset >= 0L &&
                upkg_range_ok((unsigned long)serial_offset, (unsigned long)serial_size, data_size)) {
                ex->payload_in_range = 1;
                ex->serial_crc32 = upkg_crc32(data + (unsigned long)serial_offset, (unsigned long)serial_size);
            } else if (serial_size > 0L) {
                pkg->warning_count++;
            }
        }
    }
    pkg->export_count = store_count;
    return UC3D_OK;
}

static void upkg_resolve_all_paths(uc3d_upkg *pkg)
{
    unsigned long i;
    for (i = 0UL; i < pkg->import_count; ++i) {
        uc3d_upkg_resolve_object_ref(pkg, pkg->imports[i].package_index,
                                     pkg->imports[i].package_path,
                                     sizeof(pkg->imports[i].package_path));
        uc3d_upkg_resolve_object_ref(pkg, -(long)i - 1L,
                                     pkg->imports[i].full_path,
                                     sizeof(pkg->imports[i].full_path));
    }
    for (i = 0UL; i < pkg->export_count; ++i) {
        uc3d_upkg_resolve_object_ref(pkg, pkg->exports[i].class_index,
                                     pkg->exports[i].class_path,
                                     sizeof(pkg->exports[i].class_path));
        uc3d_upkg_resolve_object_ref(pkg, pkg->exports[i].super_index,
                                     pkg->exports[i].super_path,
                                     sizeof(pkg->exports[i].super_path));
        uc3d_upkg_resolve_object_ref(pkg, pkg->exports[i].package_index,
                                     pkg->exports[i].package_path,
                                     sizeof(pkg->exports[i].package_path));
        uc3d_upkg_resolve_object_ref(pkg, (long)i + 1L,
                                     pkg->exports[i].full_path,
                                     sizeof(pkg->exports[i].full_path));
    }
}

int uc3d_parse_upkg_buffer(const unsigned char *data,
                           unsigned long data_size,
                           uc3d_upkg *pkg)
{
    int r;
    if (data == (const unsigned char *)0 || pkg == (uc3d_upkg *)0) {
        return UC3D_ERR_NULL;
    }
    uc3d_upkg_init(pkg);
    if (data_size < 36UL) {
        return UC3D_ERR_TRUNCATED;
    }
    pkg->magic = upkg_rd32le(data + 0UL);
    if (pkg->magic != UC3D_UPKG_MAGIC) {
        return UC3D_ERR_BAD_FORMAT;
    }
    pkg->version = upkg_rd32le(data + 4UL);
    pkg->package_flags = upkg_rd32le(data + 8UL);
    pkg->declared_name_count = upkg_rd32le(data + 12UL);
    pkg->name_offset = upkg_rd32le(data + 16UL);
    pkg->declared_export_count = upkg_rd32le(data + 20UL);
    pkg->export_offset = upkg_rd32le(data + 24UL);
    pkg->declared_import_count = upkg_rd32le(data + 28UL);
    pkg->import_offset = upkg_rd32le(data + 32UL);
    pkg->header_size_guess = 36UL;
    if (data_size >= 56UL) {
        pkg->guid[0] = upkg_rd32le(data + 36UL);
        pkg->guid[1] = upkg_rd32le(data + 40UL);
        pkg->guid[2] = upkg_rd32le(data + 44UL);
        pkg->guid[3] = upkg_rd32le(data + 48UL);
        pkg->generation_count = upkg_rd32le(data + 52UL);
        pkg->header_size_guess = 56UL;
    }
    if (!upkg_range_ok(pkg->name_offset, 1UL, data_size) ||
        (pkg->declared_import_count > 0UL && !upkg_range_ok(pkg->import_offset, 1UL, data_size)) ||
        (pkg->declared_export_count > 0UL && !upkg_range_ok(pkg->export_offset, 1UL, data_size))) {
        return UC3D_ERR_TRUNCATED;
    }
    r = upkg_parse_name_table(data, data_size, pkg);
    if (r != UC3D_OK) {
        return r;
    }
    r = upkg_parse_import_table(data, data_size, pkg);
    if (r != UC3D_OK) {
        return r;
    }
    r = upkg_parse_export_table(data, data_size, pkg);
    if (r != UC3D_OK) {
        return r;
    }
    upkg_resolve_all_paths(pkg);
    return UC3D_OK;
}

int uc3d_upkg_find_export_by_name(const uc3d_upkg *pkg, const char *name)
{
    unsigned long i;
    if (pkg == (const uc3d_upkg *)0 || name == (const char *)0) {
        return UC3D_ERR_NULL;
    }
    for (i = 0UL; i < pkg->export_count; ++i) {
        if (upkg_eq_ci(pkg->exports[i].object_name_text, name)) {
            return (int)i;
        }
    }
    return UC3D_ERR_NOT_FOUND;
}

int uc3d_upkg_find_export_by_class(const uc3d_upkg *pkg, const char *class_substr)
{
    unsigned long i;
    if (pkg == (const uc3d_upkg *)0 || class_substr == (const char *)0) {
        return UC3D_ERR_NULL;
    }
    for (i = 0UL; i < pkg->export_count; ++i) {
        if (upkg_contains_ci(pkg->exports[i].class_path, class_substr)) {
            return (int)i;
        }
    }
    return UC3D_ERR_NOT_FOUND;
}

int uc3d_upkg_get_export_payload(const unsigned char *data,
                                 unsigned long data_size,
                                 const uc3d_upkg_export *exp,
                                 const unsigned char **payload,
                                 unsigned long *payload_size)
{
    if (data == (const unsigned char *)0 || exp == (const uc3d_upkg_export *)0 ||
        payload == (const unsigned char **)0 || payload_size == (unsigned long *)0) {
        return UC3D_ERR_NULL;
    }
    *payload = (const unsigned char *)0;
    *payload_size = 0UL;
    if (exp->serial_size <= 0L) {
        return UC3D_ERR_NOT_FOUND;
    }
    if (exp->serial_offset < 0L) {
        return UC3D_ERR_RANGE;
    }
    if (!upkg_range_ok((unsigned long)exp->serial_offset, (unsigned long)exp->serial_size, data_size)) {
        return UC3D_ERR_TRUNCATED;
    }
    *payload = data + (unsigned long)exp->serial_offset;
    *payload_size = (unsigned long)exp->serial_size;
    return UC3D_OK;
}
