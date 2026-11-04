/* Build a minimal IQM v2 file with a single 90-degree rotating weighted bone.
 * This fixture generator is ISO C89 and uses no dynamic memory or floats. */
#include <stdio.h>
#include <string.h>
static unsigned char bytes[520];
static void put32(unsigned int off,unsigned long v)
{
    bytes[off]=(unsigned char)(v&255u);
    bytes[off+1]=(unsigned char)((v>>8)&255u);
    bytes[off+2]=(unsigned char)((v>>16)&255u);
    bytes[off+3]=(unsigned char)((v>>24)&255u);
}
static void put16(unsigned int off,unsigned int v)
{bytes[off]=(unsigned char)(v&255u);bytes[off+1]=(unsigned char)((v>>8)&255u);}
int main(int argc,char**argv)
{
    FILE*f;unsigned int v,at;
    if(argc!=2 && argc!=3)return 2;
    memset(bytes,0,sizeof(bytes));
    memcpy(bytes,"INTERQUAKEMODEL",15);
    put32(16,2u);put32(20,(unsigned long)sizeof(bytes));
    put32(28,20u);put32(32,124u); /* text */
    put32(36,1u);put32(40,144u); /* meshes */
    put32(44,4u);put32(48,3u);put32(52,168u); /* arrays */
    put32(56,1u);put32(60,248u); /* triangle */
    put32(68,1u);put32(72,260u); /* joint */
    put32(76,1u);put32(80,308u); /* pose */
    put32(84,1u);put32(88,396u); /* animation */
    put32(92,2u);put32(96,2u);put32(100,416u); /* frames */
    memcpy(bytes+124,"mesh\0mat\0idle\0joint",20u);
    /* Mesh name, material, vertex range, triangle range. */
    put32(144,0u);put32(148,5u);put32(152,0u);put32(156,3u);
    put32(160,0u);put32(164,1u);
    /* Four vertex arrays: POSITION, NORMAL, BLENDINDEXES, BLENDWEIGHTS. */
    put32(168,0u);put32(176,7u);put32(180,3u);put32(184,424u);
    put32(188,2u);put32(196,7u);put32(200,3u);put32(204,460u);
    put32(208,4u);put32(216,1u);put32(220,4u);put32(224,496u);
    put32(228,5u);put32(236,1u);put32(240,4u);put32(244,508u);
    put32(248,0u);put32(252,1u);put32(256,2u);
    put32(260,14u);put32(264,0xffffffffu); /* joint */
    put32(260+8+6*4,0x3f800000u); /* identity quaternion W */
    for(v=7u;v<10u;v++)put32(260+8+v*4,0x3f800000u); /* unit scales */
    put32(308,0xffffffffu);put32(312,(1u<<5)|(1u<<6)); /* animate qz and qw */
    put32(308+8+6*4,0x3f800000u); /* offset quaternion W 1 */
    for(v=7u;v<10u;v++)put32(308+8+v*4,0x3f800000u);
    put32(308+48+5*4,0x373505a8u); /* 0.70710678 / 65535 */
    put32(308+48+6*4,0xb695f6b0u); /* (0.70710678 - 1) / 65535 */
    put32(396,9u);put32(400,0u);put32(404,2u);
    put32(408,0x41c00000u); /* 24 fps */
    put16(416,0u);put16(418,0u);put16(420,65535u);put16(422,65535u);
    /* 3 positions: (1,0,0), (0,1,0), (0,0,0). */
    put32(424,0x3f800000u);put32(424+16,0x3f800000u);
    /* 3 normals: (0,0,1). */
    for(v=0u;v<3u;v++)put32(460+v*12u+8u,0x3f800000u);
    /* Each vertex is weighted 100% on bone 0. */
    for(v=0u;v<3u;v++)bytes[508+v*4u]=255u;
    if(argc==3){
        /* Scale-only regression: bind identity; animation pose scale X2.
         * Normal (1,1,0)/sqrt(2) should become (1,2,0)/sqrt(5).
         * The original rotation fixture remains byte-for-byte unchanged. */
        if(strcmp(argv[2],"nonuniform")!=0)return 2;
        put32(312,0u); /* no animated channels */
        put32(308+8+7*4,0x40000000u); /* pose scale X = 2 */
        put32(92,1u);put32(96,0u);put32(404,1u);
        for(v=0u;v<3u;v++){
            put32(460+v*12u,0x3f3504f3u);
            put32(460+v*12u+4u,0x3f3504f3u);
            put32(460+v*12u+8u,0u);
        }
    }
    f=fopen(argv[1],"wb");if(!f)return 3;
    at=(unsigned int)fwrite(bytes,1,sizeof(bytes),f);
    if(fclose(f)!=0||at!=sizeof(bytes))return 4;
    return 0;
}
