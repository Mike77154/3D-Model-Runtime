#include "qmdl_internal.h"

static qmdl_u32 qmdl_pak_load_u32_le(const qmdl_u8 *src)
{
    return ((qmdl_u32)src[0]) |
           ((qmdl_u32)src[1] << 8) |
           ((qmdl_u32)src[2] << 16) |
           ((qmdl_u32)src[3] << 24);
}

static int qmdl_pak_name_equals(const char *a, const char *b)
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

void qmdl_pak_archive_clear(qmdl_pak_archive *archive)
{
    if (archive != 0)
    {
        memset(archive, 0, sizeof(*archive));
    }
}

void qmdl_pak_entry_clear(qmdl_pak_entry *entry)
{
    if (entry != 0)
    {
        memset(entry, 0, sizeof(*entry));
    }
}

int qmdl_pak_open(const void *data, size_t size, qmdl_pak_archive *archive)
{
    const qmdl_u8 *bytes;
    qmdl_u32 dir_offset;
    qmdl_u32 dir_size;

    if (data == 0 || archive == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (size < 12u)
    {
        return QMDL_ERR_SIZE;
    }

    bytes = (const qmdl_u8 *)data;
    if (bytes[0] != 'P' || bytes[1] != 'A' || bytes[2] != 'C' || bytes[3] != 'K')
    {
        return QMDL_ERR_FORMAT;
    }

    dir_offset = qmdl_pak_load_u32_le(bytes + 4u);
    dir_size = qmdl_pak_load_u32_le(bytes + 8u);

    if ((dir_size % 64u) != 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if ((size_t)dir_offset > size || (size_t)dir_size > size - (size_t)dir_offset)
    {
        return QMDL_ERR_SIZE;
    }

    archive->data = bytes;
    archive->size = size;
    archive->dir_offset = dir_offset;
    archive->num_entries = dir_size / 64u;
    return QMDL_OK;
}

int qmdl_pak_read_entry(const qmdl_pak_archive *archive,
                        qmdl_u32 index,
                        qmdl_pak_entry *entry)
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

    ptr = archive->data + archive->dir_offset + ((size_t)index * 64u);
    for (i = 0u; i < 56u; ++i)
    {
        entry->name[i] = (char)ptr[i];
    }
    entry->name[55] = '\0';
    entry->offset = qmdl_pak_load_u32_le(ptr + 56u);
    entry->size = qmdl_pak_load_u32_le(ptr + 60u);

    if ((size_t)entry->offset > archive->size ||
        (size_t)entry->size > archive->size - (size_t)entry->offset)
    {
        return QMDL_ERR_SIZE;
    }

    return QMDL_OK;
}

int qmdl_pak_find_entry(const qmdl_pak_archive *archive,
                        const char *name,
                        qmdl_u32 *index_out,
                        qmdl_pak_entry *entry_out)
{
    qmdl_pak_entry tmp;
    qmdl_u32 i;
    int rc;

    if (archive == 0 || name == 0)
    {
        return QMDL_ERR_ARGS;
    }

    for (i = 0u; i < archive->num_entries; ++i)
    {
        rc = qmdl_pak_read_entry(archive, i, &tmp);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        if (qmdl_pak_name_equals(tmp.name, name))
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

int qmdl_pak_entry_view(const qmdl_pak_archive *archive,
                        const qmdl_pak_entry *entry,
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
        (size_t)entry->size > archive->size - (size_t)entry->offset)
    {
        return QMDL_ERR_SIZE;
    }

    *data_out = archive->data + entry->offset;
    if (size_out != 0)
    {
        *size_out = (size_t)entry->size;
    }
    return QMDL_OK;
}
