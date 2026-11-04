#include "qmdl_internal.h"

static qmdl_u32 qmdl_wad2_load_u32_le(const qmdl_u8 *src)
{
    return ((qmdl_u32)src[0]) |
           ((qmdl_u32)src[1] << 8) |
           ((qmdl_u32)src[2] << 16) |
           ((qmdl_u32)src[3] << 24);
}

static qmdl_u16 qmdl_wad2_load_u16_le(const qmdl_u8 *src)
{
    return (qmdl_u16)(((qmdl_u16)src[0]) | ((qmdl_u16)src[1] << 8));
}

static int qmdl_wad2_name_equals(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

void qmdl_wad2_archive_clear(qmdl_wad2_archive *archive)
{
    if (archive != 0)
    {
        memset(archive, 0, sizeof(*archive));
    }
}

void qmdl_wad2_entry_clear(qmdl_wad2_entry *entry)
{
    if (entry != 0)
    {
        memset(entry, 0, sizeof(*entry));
    }
}

void qmdl_wad2_pic_clear(qmdl_wad2_pic *pic)
{
    if (pic != 0)
    {
        memset(pic, 0, sizeof(*pic));
    }
}

int qmdl_wad2_open(const void *data, size_t size, qmdl_wad2_archive *archive)
{
    const qmdl_u8 *bytes;
    qmdl_u32 count;
    qmdl_u32 dir_offset;
    size_t dir_bytes;

    if (data == 0 || archive == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (size < 12u)
    {
        return QMDL_ERR_SIZE;
    }

    bytes = (const qmdl_u8 *)data;
    if (bytes[0] != 'W' || bytes[1] != 'A' || bytes[2] != 'D' || bytes[3] != '2')
    {
        return QMDL_ERR_FORMAT;
    }

    count = qmdl_wad2_load_u32_le(bytes + 4u);
    dir_offset = qmdl_wad2_load_u32_le(bytes + 8u);
    dir_bytes = (size_t)count * 32u;

    if ((size_t)dir_offset > size || dir_bytes > size - (size_t)dir_offset)
    {
        return QMDL_ERR_SIZE;
    }

    archive->data = bytes;
    archive->size = size;
    archive->dir_offset = dir_offset;
    archive->num_entries = count;
    return QMDL_OK;
}

int qmdl_wad2_read_entry(const qmdl_wad2_archive *archive,
                         qmdl_u32 index,
                         qmdl_wad2_entry *entry)
{
    const qmdl_u8 *ptr;
    qmdl_u32 i;

    if (archive == 0 || entry == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (archive->data == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (index >= archive->num_entries)
    {
        return QMDL_ERR_RANGE;
    }

    ptr = archive->data + archive->dir_offset + ((size_t)index * 32u);
    entry->offset = qmdl_wad2_load_u32_le(ptr + 0u);
    entry->disk_size = qmdl_wad2_load_u32_le(ptr + 4u);
    entry->size = qmdl_wad2_load_u32_le(ptr + 8u);
    entry->type = ptr[12];
    entry->compression = ptr[13];
    entry->dummy = qmdl_wad2_load_u16_le(ptr + 14u);
    for (i = 0u; i < 16u; ++i)
    {
        entry->name[i] = (char)ptr[16u + i];
    }
    entry->name[15] = '\0';

    if ((size_t)entry->offset > archive->size ||
        (size_t)entry->disk_size > archive->size - (size_t)entry->offset)
    {
        return QMDL_ERR_SIZE;
    }

    return QMDL_OK;
}

int qmdl_wad2_find_entry(const qmdl_wad2_archive *archive,
                         const char *name,
                         qmdl_u32 *index_out,
                         qmdl_wad2_entry *entry_out)
{
    qmdl_wad2_entry tmp;
    qmdl_u32 i;
    int rc;

    if (archive == 0 || name == 0)
    {
        return QMDL_ERR_ARGS;
    }

    for (i = 0u; i < archive->num_entries; ++i)
    {
        rc = qmdl_wad2_read_entry(archive, i, &tmp);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        if (qmdl_wad2_name_equals(tmp.name, name))
        {
            if (index_out != 0)
            {
                *index_out = i;
            }
            if (entry_out != 0)
            {
                *entry_out = tmp;
            }
            return QMDL_OK;
        }
    }

    return QMDL_ERR_RANGE;
}

int qmdl_wad2_entry_view(const qmdl_wad2_archive *archive,
                         const qmdl_wad2_entry *entry,
                         const void **data_out,
                         size_t *size_out)
{
    if (archive == 0 || entry == 0 || data_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (archive->data == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)entry->offset > archive->size ||
        (size_t)entry->disk_size > archive->size - (size_t)entry->offset)
    {
        return QMDL_ERR_SIZE;
    }

    *data_out = archive->data + entry->offset;
    if (size_out != 0)
    {
        *size_out = (size_t)entry->disk_size;
    }
    return QMDL_OK;
}

int qmdl_wad2_parse_picture(const void *data,
                            size_t data_size,
                            qmdl_wad2_pic *pic)
{
    const qmdl_u8 *bytes;
    qmdl_u32 width;
    qmdl_u32 height;
    size_t pixel_count;

    if (data == 0 || pic == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (data_size < 8u)
    {
        return QMDL_ERR_SIZE;
    }

    bytes = (const qmdl_u8 *)data;
    width = qmdl_wad2_load_u32_le(bytes + 0u);
    height = qmdl_wad2_load_u32_le(bytes + 4u);
    pixel_count = (size_t)width * (size_t)height;
    if (width != 0u && pixel_count / (size_t)width != (size_t)height)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (pixel_count > data_size - 8u)
    {
        return QMDL_ERR_SIZE;
    }

    pic->width = width;
    pic->height = height;
    pic->pixels = bytes + 8u;
    pic->pixel_count = pixel_count;
    return QMDL_OK;
}
