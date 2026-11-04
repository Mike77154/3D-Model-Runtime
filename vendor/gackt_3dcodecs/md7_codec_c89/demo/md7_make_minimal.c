#include <stdio.h>

static void w8(FILE *f, unsigned int v) { fputc((int)(v & 255U), f); }
static void w16(FILE *f, unsigned int v) { w8(f, v); w8(f, v >> 8); }
static void w32(FILE *f, unsigned long v) { w8(f, (unsigned int)v); w8(f, (unsigned int)(v >> 8)); w8(f, (unsigned int)(v >> 16)); w8(f, (unsigned int)(v >> 24)); }
static void wf32bits(FILE *f, unsigned long bits) { w32(f, bits); }
static void wname(FILE *f, const char *s, unsigned int n)
{
    unsigned int i;
    unsigned char c;
    for (i = 0U; i < n; ++i) {
        c = 0U;
        if (s != 0 && s[i] != '\0') {
            c = (unsigned char)s[i];
        }
        w8(f, c);
    }
}

static void wmaterial(FILE *f)
{
    unsigned int i;
    unsigned long diffuse[4];
    unsigned long ambient[4];
    unsigned long specular[4];
    unsigned long emissive[4];
    diffuse[0] = 0x3f800000UL; diffuse[1] = 0x3f400000UL; diffuse[2] = 0x3f000000UL; diffuse[3] = 0x3f800000UL;
    ambient[0] = 0x3e99999aUL; ambient[1] = 0x3e99999aUL; ambient[2] = 0x3e99999aUL; ambient[3] = 0x3f800000UL;
    specular[0] = 0x3f000000UL; specular[1] = 0x3f000000UL; specular[2] = 0x3f000000UL; specular[3] = 0x3f800000UL;
    emissive[0] = 0UL; emissive[1] = 0UL; emissive[2] = 0UL; emissive[3] = 0x3f800000UL;
    for (i = 0U; i < 4U; ++i) wf32bits(f, diffuse[i]);
    for (i = 0U; i < 4U; ++i) wf32bits(f, ambient[i]);
    for (i = 0U; i < 4U; ++i) wf32bits(f, specular[i]);
    for (i = 0U; i < 4U; ++i) wf32bits(f, emissive[i]);
    wf32bits(f, 0x41200000UL);
}

static void wvtx(FILE *f, unsigned long x, unsigned long y, unsigned long z)
{
    wf32bits(f, x); wf32bits(f, y); wf32bits(f, z);
    w16(f, 0U);
    w8(f, 0U);
    w8(f, 0U);
}

static void widentity_transform(FILE *f)
{
    unsigned int i;
    for (i = 0U; i < 16U; ++i) {
        if (i == 0U || i == 5U || i == 10U || i == 15U) {
            wf32bits(f, 0x3f800000UL);
        } else {
            wf32bits(f, 0UL);
        }
    }
    w16(f, 0U);
    w16(f, 0U);
}

int main(int argc, char **argv)
{
    FILE *f;
    const char *path;
    unsigned long data_size;
    unsigned long group_size;

    path = "tests/minimal_mdl7.mdl";
    if (argc > 1) {
        path = argv[1];
    }
    f = fopen(path, "wb");
    if (f == 0) {
        return 1;
    }

    group_size = 44UL + 96UL + 24UL + 16UL + 48UL + 92UL + 68UL;
    data_size = 48UL + 36UL + group_size;

    wname(f, "MDL7", 4U);
    w32(f, 0UL);
    w32(f, 1UL);
    w32(f, 1UL);
    w32(f, data_size);
    w32(f, 0UL);
    w32(f, 0UL);
    w16(f, 36U);
    w16(f, 28U);
    w16(f, 16U);
    w16(f, 68U);
    w16(f, 8U);
    w16(f, 16U);
    w16(f, 16U);
    w16(f, 16U);
    w16(f, 68U);
    w16(f, 24U);

    w16(f, 0xffffU);
    w16(f, 0U);
    wf32bits(f, 0UL); wf32bits(f, 0UL); wf32bits(f, 0UL);
    wname(f, "root", 20U);

    w8(f, 1U);
    w8(f, 1U);
    w8(f, 1U);
    w8(f, 0U);
    w32(f, group_size);
    wname(f, "tri_group", 16U);
    w32(f, 1UL);
    w32(f, 3UL);
    w32(f, 1UL);
    w32(f, 3UL);
    w32(f, 1UL);

    w8(f, 0x10U);
    w8(f, 0U); w8(f, 0U); w8(f, 0U);
    w32(f, 64UL);
    w32(f, 64UL);
    wname(f, "skin.png", 16U);
    wmaterial(f);

    wf32bits(f, 0UL); wf32bits(f, 0UL);
    wf32bits(f, 0x3f800000UL); wf32bits(f, 0UL);
    wf32bits(f, 0UL); wf32bits(f, 0x3f800000UL);

    w16(f, 0U); w16(f, 1U); w16(f, 2U);
    w16(f, 0U); w16(f, 1U); w16(f, 2U);
    w32(f, 0UL);

    wvtx(f, 0UL, 0UL, 0UL);
    wvtx(f, 0x3f800000UL, 0UL, 0UL);
    wvtx(f, 0UL, 0x3f800000UL, 0UL);

    wname(f, "idle", 16U);
    w32(f, 0UL);
    w32(f, 1UL);
    widentity_transform(f);

    w8(f, 0U);
    w8(f, 0U);
    w16(f, 0U);
    w32(f, 0UL);
    w32(f, 1UL);
    w32(f, 68UL);

    w32(f, 0UL);
    wname(f, "root", 20U);
    w32(f, 3UL);
    w32(f, 0UL); wf32bits(f, 0x3f800000UL);
    w32(f, 1UL); wf32bits(f, 0x3f800000UL);
    w32(f, 2UL); wf32bits(f, 0x3f800000UL);

    fclose(f);
    printf("wrote %s\n", path);
    return 0;
}
