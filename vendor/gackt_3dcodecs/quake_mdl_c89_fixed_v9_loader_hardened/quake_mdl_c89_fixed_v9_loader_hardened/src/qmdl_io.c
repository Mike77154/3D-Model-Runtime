
#include "qmdl_internal.h"
#include <stdio.h>

int qmdl_file_read_all(const char *path,
                       void *buffer,
                       size_t buffer_capacity,
                       size_t *file_size)
{
    FILE *fp;
    long length;
    size_t read_count;

    if (path == 0 || buffer == 0)
    {
        return QMDL_ERR_ARGS;
    }

    fp = fopen(path, "rb");
    if (fp == 0)
    {
        return QMDL_ERR_IO;
    }

    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        fclose(fp);
        return QMDL_ERR_IO;
    }

    length = ftell(fp);
    if (length < 0)
    {
        fclose(fp);
        return QMDL_ERR_IO;
    }

    if ((size_t)length > buffer_capacity)
    {
        fclose(fp);
        return QMDL_ERR_SIZE;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0)
    {
        fclose(fp);
        return QMDL_ERR_IO;
    }

    read_count = fread(buffer, 1u, (size_t)length, fp);
    fclose(fp);

    if (read_count != (size_t)length)
    {
        return QMDL_ERR_IO;
    }

    if (file_size != 0)
    {
        *file_size = read_count;
    }

    return QMDL_OK;
}

int qmdl_file_write_all(const char *path,
                        const void *data,
                        size_t data_size)
{
    FILE *fp;
    size_t write_count;

    if (path == 0 || data == 0)
    {
        return QMDL_ERR_ARGS;
    }

    fp = fopen(path, "wb");
    if (fp == 0)
    {
        return QMDL_ERR_IO;
    }

    write_count = fwrite(data, 1u, data_size, fp);
    fclose(fp);

    if (write_count != data_size)
    {
        return QMDL_ERR_IO;
    }

    return QMDL_OK;
}
