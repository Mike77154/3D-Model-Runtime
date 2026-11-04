#include <stdio.h>

static void put_u8(FILE *fp, unsigned int v)
{
    fputc((int)(v & 255U), fp);
}

static void put_u16(FILE *fp, unsigned int v)
{
    put_u8(fp, v);
    put_u8(fp, v >> 8);
}

static void put_u32(FILE *fp, unsigned long v)
{
    put_u8(fp, (unsigned int)v);
    put_u8(fp, (unsigned int)(v >> 8));
    put_u8(fp, (unsigned int)(v >> 16));
    put_u8(fp, (unsigned int)(v >> 24));
}

static void put_ieee_zero(FILE *fp)
{
    put_u32(fp, 0UL);
}

static void put_ieee_one(FILE *fp)
{
    put_u32(fp, 0x3f800000UL);
}

static void put_ieee_24(FILE *fp)
{
    put_u32(fp, 0x41c00000UL);
}

static void put_name(FILE *fp, const char *s, unsigned int size)
{
    unsigned int i;
    unsigned char c;

    for (i = 0U; i < size; ++i) {
        c = 0U;
        if (s[i] != '\0') {
            c = (unsigned char)s[i];
        }
        put_u8(fp, (unsigned int)c);
        if (s[i] == '\0') {
            ++i;
            while (i < size) {
                put_u8(fp, 0U);
                ++i;
            }
            return;
        }
    }
}

static void put_vertex(FILE *fp, int x_one, int y_one, int z_one)
{
    put_u8(fp, 0U);
    if (x_one) { put_ieee_one(fp); } else { put_ieee_zero(fp); }
    if (y_one) { put_ieee_one(fp); } else { put_ieee_zero(fp); }
    if (z_one) { put_ieee_one(fp); } else { put_ieee_zero(fp); }
    put_u8(fp, 255U);
    put_u8(fp, 0U);
}

static void put_normal_z(FILE *fp)
{
    put_ieee_zero(fp);
    put_ieee_zero(fp);
    put_ieee_one(fp);
}

int main(int argc, char **argv)
{
    FILE *fp;
    int i;

    if (argc != 2) {
        fputs("usage: make_tiny_ms3d out.ms3d\n", stderr);
        return 2;
    }

    fp = fopen(argv[1], "wb");
    if (fp == (FILE *)0) {
        fputs("could not create output\n", stderr);
        return 2;
    }

    fwrite("MS3D000000", 1U, 10U, fp);
    put_u32(fp, 4UL);

    put_u16(fp, 3U);
    put_vertex(fp, 0, 0, 0);
    put_vertex(fp, 1, 0, 0);
    put_vertex(fp, 0, 1, 0);

    put_u16(fp, 1U);
    put_u16(fp, 0U);
    put_u16(fp, 0U);
    put_u16(fp, 1U);
    put_u16(fp, 2U);
    for (i = 0; i < 3; ++i) {
        put_normal_z(fp);
    }
    put_ieee_zero(fp);
    put_ieee_one(fp);
    put_ieee_zero(fp);
    put_ieee_zero(fp);
    put_ieee_zero(fp);
    put_ieee_one(fp);
    put_u8(fp, 1U);
    put_u8(fp, 0U);

    put_u16(fp, 1U);
    put_u8(fp, 0U);
    put_name(fp, "triangle", 32U);
    put_u16(fp, 1U);
    put_u16(fp, 0U);
    put_u8(fp, 255U);

    put_u16(fp, 0U);

    put_ieee_24(fp);
    put_ieee_zero(fp);
    put_u32(fp, 1UL);

    put_u16(fp, 0U);

    fclose(fp);
    return 0;
}
