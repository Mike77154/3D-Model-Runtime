#include "scn3d.h"
#include "scn3d_stdio.h"

long scn3d_read_bytes(scn3d_io *io, void *dst, long bytes)
{
    if (!io || !io->read || bytes < 0) {
        return 0;
    }
    return io->read(io->user, dst, bytes);
}

int scn3d_seek_abs(scn3d_io *io, long pos)
{
    if (!io || !io->seek || pos < 0) {
        return 0;
    }
    return io->seek(io->user, pos);
}

long scn3d_tell_pos(scn3d_io *io)
{
    if (!io || !io->tell) {
        return -1;
    }
    return io->tell(io->user);
}

int scn3d_skip_bytes(scn3d_io *io, long bytes)
{
    long p;
    if (bytes < 0) {
        return 0;
    }
    p = scn3d_tell_pos(io);
    if (p < 0) {
        return 0;
    }
    return scn3d_seek_abs(io, p + bytes);
}

int scn3d_read_u8(scn3d_io *io, scn3d_u8 *v)
{
    return scn3d_read_bytes(io, v, 1) == 1;
}

int scn3d_read_le16(scn3d_io *io, long *v)
{
    unsigned char b[2];
    if (scn3d_read_bytes(io, b, 2) != 2) return 0;
    *v = (long)(((unsigned long)b[0]) | (((unsigned long)b[1]) << 8));
    if (*v & 0x8000L) *v -= 0x10000L;
    return 1;
}

int scn3d_read_le32(scn3d_io *io, long *v)
{
    unsigned char b[4];
    unsigned long u;
    if (scn3d_read_bytes(io, b, 4) != 4) return 0;
    u = ((unsigned long)b[0]) |
        (((unsigned long)b[1]) << 8) |
        (((unsigned long)b[2]) << 16) |
        (((unsigned long)b[3]) << 24);
    *v = (long)u;
    if (u & 0x80000000UL) {
        *v = -((long)((~u + 1UL) & 0xffffffffUL));
    }
    return 1;
}

int scn3d_read_u32_bits(scn3d_io *io, scn3d_u32 *v)
{
    unsigned char b[4];
    if (scn3d_read_bytes(io, b, 4) != 4) return 0;
    *v = ((unsigned long)b[0]) |
         (((unsigned long)b[1]) << 8) |
         (((unsigned long)b[2]) << 16) |
         (((unsigned long)b[3]) << 24);
    return 1;
}

static long scn3d_file_read(void *user, void *dst, long bytes)
{
    FILE *fp;
    fp = (FILE *)user;
    if (!fp || bytes <= 0) return 0;
    return (long)fread(dst, 1, (size_t)bytes, fp);
}

static int scn3d_file_seek(void *user, long absolute_pos)
{
    FILE *fp;
    fp = (FILE *)user;
    if (!fp) return 0;
    return fseek(fp, absolute_pos, SEEK_SET) == 0;
}

static long scn3d_file_tell(void *user)
{
    FILE *fp;
    long p;
    fp = (FILE *)user;
    if (!fp) return -1;
    p = ftell(fp);
    return p;
}

static long scn3d_file_size(void *user)
{
    FILE *fp;
    long p;
    long e;
    fp = (FILE *)user;
    if (!fp) return -1;
    p = ftell(fp);
    if (p < 0) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) return -1;
    e = ftell(fp);
    fseek(fp, p, SEEK_SET);
    return e;
}

void scn3d_io_from_FILE(scn3d_io *io, FILE *fp)
{
    if (!io) return;
    io->user = fp;
    io->read = scn3d_file_read;
    io->seek = scn3d_file_seek;
    io->tell = scn3d_file_tell;
    io->size = scn3d_file_size;
}
