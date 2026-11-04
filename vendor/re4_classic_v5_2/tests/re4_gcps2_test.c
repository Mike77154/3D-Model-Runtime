/* Synthetic, redistributable GameCube and PS2 BIN/GX tests. No RE4 assets. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>

#define MUST(expr) do { if (!(expr)) { \
    printf("gcps2 test failed line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)
static void wr16le(re4_u8 *p, re4_u16 v) { p[0]=(re4_u8)v;p[1]=(re4_u8)(v>>8); }
static void wr32le(re4_u8 *p, re4_u32 v) { wr16le(p,(re4_u16)v);wr16le(p+2U,(re4_u16)(v>>16)); }
static void wr16be(re4_u8 *p, re4_u16 v) { p[0]=(re4_u8)(v>>8);p[1]=(re4_u8)v; }
static void wr32be(re4_u8 *p, re4_u32 v) { wr16be(p,(re4_u16)(v>>16));wr16be(p+2U,(re4_u16)v); }

static int gc_test(void)
{
    re4_u8 bytes[0x160];
    re4_span src, fifo;
    re4_gc_bin bin;
    re4_gc_polygon part;
    re4_gc_corner corners[12];
    re4_gc_triangle triangles[8];
    re4_gc_gx_counts count;
    re4_q16 pos[3], normal[3], uv[2];
    re4_u8 rgba[4];
    re4_u32 i, at;
    memset(bytes,0,sizeof(bytes));
    wr32be(bytes+0x3CU,0x20030818U);
    wr32be(bytes+0x0CU,0x70U); /* vertex colors */
    wr32be(bytes+0x10U,0x80U); /* UV data */
    wr32be(bytes+0x1CU,0xC0U); /* polygon data */
    wr32be(bytes+0x20U,0x80000000U); /* signed texcoords frac 8 */
    wr32be(bytes+0x30U,0x90U); /* vertices */
    wr32be(bytes+0x34U,0xB0U); /* normals */
    wr16be(bytes+0x1AU,1U);
    wr16be(bytes+0x38U,4U);
    wr16be(bytes+0x3AU,1U);
    bytes[0x28U]=4U;
    bytes[0x70U]=8U;bytes[0x71U]=16U;bytes[0x72U]=32U;bytes[0x73U]=255U;
    wr16be(bytes+0x80U,256U);wr16be(bytes+0x82U,(re4_u16)0xFF00U);
    wr16be(bytes+0x90U,32U);wr16be(bytes+0x92U,(re4_u16)0xFFF0U);wr16be(bytes+0x94U,16U);
    wr16be(bytes+0xB0U,16384U);
    bytes[0xCCU]=3U; /* material diffuse texture */
    wr32be(bytes+0xD8U,64U); /* padded 64-byte GX list */
    wr32be(bytes+0xDCU,2U);
    at=0xE0U;
    bytes[at++]=0x80U;wr16be(bytes+at,4U);at+=2U; /* GX quad */
    for(i=0;i<4U;++i){
        wr16be(bytes+at,(re4_u16)i);at+=2U;
        wr16be(bytes+at,0U);at+=2U;
        wr16be(bytes+at,0U);at+=2U;
        wr16be(bytes+at,0U);at+=2U;
    }
    src.data=bytes;src.size=sizeof(bytes);
    MUST(re4_gc_bin_read(src,&bin)==RE4_OK);
    MUST(bin.positions_count==4U && bin.texcoords_count==4U);
    MUST(re4_gc_bin_polygon(&bin,0U,&part)==RE4_OK);
    MUST(part.diffuse_texture==3U && part.fifo.size==64U);
    fifo=part.fifo;
    MUST(re4_gc_gx_decode(fifo,RE4_TRUE,bin.positions_count,bin.normals_count,
                          bin.colors_count,bin.texcoords_count,0,0U,0,0U,&count)==RE4_OK);
    MUST(count.commands==1U && count.corners==4U && count.triangles==2U);
    MUST(re4_gc_gx_decode(fifo,RE4_TRUE,bin.positions_count,bin.normals_count,
                          bin.colors_count,bin.texcoords_count,corners,12U,triangles,8U,&count)==RE4_OK);
    MUST(triangles[0].corner[0]==0U && triangles[0].corner[2]==2U);
    MUST(triangles[1].corner[0]==0U && triangles[1].corner[1]==2U && triangles[1].corner[2]==3U);
    MUST(re4_gc_corner_q16(&bin,&corners[0],pos,normal,uv,rgba)==RE4_OK);
    MUST(pos[0]==131072 && pos[1]==-65536 && pos[2]==65536);
    MUST(normal[0]==65536 && uv[0]==65536 && uv[1]==-65536 && rgba[3]==255U);
    MUST(re4_gc_gx_decode(fifo,RE4_TRUE,4U,1U,1U,4U,corners,2U,triangles,8U,&count)==RE4_ERR_NO_SPACE);
    bytes[0xE0U]=0x10U;
    MUST(re4_gc_gx_decode(fifo,RE4_TRUE,4U,1U,1U,4U,0,0U,0,0U,&count)==RE4_ERR_UNSUPPORTED);
    bytes[0xE0U]=0x80U;
    fifo.size=12U;
    MUST(re4_gc_gx_decode(fifo,RE4_TRUE,4U,1U,1U,4U,0,0U,0,0U,&count)==RE4_ERR_RANGE);
    src.size=0x40U;
    MUST(re4_gc_bin_read(src,&bin)==RE4_ERR_RANGE); /* extended header truncated */
    return 0;
}

static int ps2_test(void)
{
    re4_u8 bytes[0x250];
    re4_span src;
    re4_ps2_bin bin;
    re4_ps2_node node;
    re4_ps2_segment seg, seg2;
    re4_ps2_vertex vertex;
    re4_ps2_bone bone;
    re4_ps2_weight wt;
    re4_q16 xyz[3], uv[2];
    re4_gc_triangle faces[4];
    re4_u32 count;
    re4_u32 i;
    memset(bytes,0,sizeof(bytes));
    wr16le(bytes,0x0030U);
    wr16le(bytes+2U,7U);
    wr32le(bytes+4U,0x50U);
    bytes[8U]=6U;bytes[9U]=1U;
    wr16le(bytes+10U,1U);
    wr32le(bytes+12U,0x60U);
    wr32le(bytes+0x18U,0x20030818U);
    wr32le(bytes+0x24U,0x80000000U);
    wr32le(bytes+0x30U,0x3F800000U); /* bound x = 1.0 */
    bytes[0x50U]=1U;bytes[0x51U]=255U;
    wr32le(bytes+0x54U,0x3F800000U);
    wr32le(bytes+0x60U+12U,0x70U); /* node pointer */
    wr16le(bytes+0x70U,0xD0U);
    bytes[0x72U]=1U; /* two segments */
    bytes[0x73U]=1U; /* one bone */
    bytes[0x74U]=1U;
    /* first segment, scenario-colored vertex rows */
    bytes[0x80U+12U]=1U;
    bytes[0x90U]=4U; /* four vertices */
    wr32le(bytes+0x9CU,0x3F800000U);
    bytes[0xA0U]=6U; /* 6*16 bytes */
    for(i=0U;i<4U;++i) {
        re4_u32 at=0xB0U+i*24U;
        wr16le(bytes+at,(re4_u16)(100U+i));
        wr16le(bytes+at+6U,(re4_u16)(300U+i)); /* index mount */
        wr16le(bytes+at+8U,(re4_u16)(255U+i)); /* U */
        wr16le(bytes+at+14U,0U); /* face continuation */
        wr16le(bytes+at+16U,64U); /* red */
    }
    /* second segment with weight map header and three regular vertices */
    bytes[0x110U]=2U;bytes[0x110U+12U]=0U;bytes[0x110U+14U]=2U;
    wr32le(bytes+0x120U,4U); /* weight-map reference */
    wr32le(bytes+0x12CU,1U); /* one bone */
    wr32le(bytes+0x130U,0x3F800000U); /* full weight */
    bytes[0x140U+16U]=3U;
    wr32le(bytes+0x140U+16U+12U,0x40000000U); /* conversion factor 2 */
    bytes[0x140U+32U]=5U; /* 5 * 16 bytes payload */
    for(i=0U;i<3U;++i) {
        re4_u32 at=0x170U+i*24U;
        wr16le(bytes+at,(re4_u16)(10U+i));
        wr16le(bytes+at+6U,6U); /* weight reference */
        wr16le(bytes+at+8U,16384U); /* normal */
        wr16le(bytes+at+14U,0U);
        wr16le(bytes+at+16U,(re4_u16)255U);
        wr16le(bytes+at+22U,1U);
    }
    src.data=bytes;src.size=sizeof(bytes);
    MUST(re4_ps2_bin_read(src,&bin)==RE4_OK);
    MUST(bin.texture_count==7U && bin.bone_count==1U && bin.bounds[0]==65536);
    MUST(re4_ps2_bin_bone(&bin,0U,&bone)==RE4_OK);
    MUST(bone.id==1U && bone.parent==255U && bone.xyz[0]==65536);
    MUST(re4_ps2_bin_node(&bin,0U,&node)==RE4_OK);
    MUST(node.segments==2U && node.bones.size==1U && node.bones.data[0]==1U);
    MUST(re4_ps2_node_segment(&bin,&node,0U,&seg)==RE4_OK);
    MUST(seg.scenario_color && seg.vertex_count==4U);
    MUST(re4_ps2_segment_vertex(&bin,&seg,3U,&vertex)==RE4_OK);
    MUST(vertex.xyz[0]==103 && vertex.index_mount==303U && vertex.texcoord[0]==258);
    MUST(re4_ps2_segment_triangles(&bin,&seg,faces,4U,&count)==RE4_OK);
    MUST(count==2U && faces[0].corner[0]==0U && faces[1].corner[0]==3U);
    MUST(re4_ps2_node_segment(&bin,&node,1U,&seg2)==RE4_OK);
    MUST(!seg2.scenario_color && seg2.weightmap_bytes==32U && seg2.conversion_factor==131072);
    MUST(re4_ps2_segment_vertex(&bin,&seg2,0U,&vertex)==RE4_OK);
    MUST(vertex.normal_or_color[0]==16384 && vertex.weight_or_alpha==6U);
    MUST(re4_ps2_vertex_q16(&seg2,&vertex,xyz,uv)==RE4_OK);
    MUST(xyz[0]==13107 && uv[0]==65536);
    MUST(re4_ps2_segment_weight(&bin,&seg2,0U,&wt)==RE4_OK);
    MUST(wt.bone_slot[0]==4U && wt.amount==1U && wt.weight[0]==65536);
    MUST(re4_ps2_segment_triangles(&bin,&seg2,faces,4U,&count)==RE4_OK && count==1U);
    MUST(re4_ps2_segment_triangles(&bin,&seg2,faces,0U,&count)==RE4_ERR_NO_SPACE);
    bytes[0xA0U]=1U; /* only 16 bytes claimed for four 24-byte vertices */
    MUST(re4_ps2_node_segment(&bin,&node,0U,&seg)==RE4_ERR_BAD_FORMAT);
    bytes[0xA0U]=6U;
    src.size=0x40U;
    MUST(re4_ps2_bin_read(src,&bin)==RE4_ERR_RANGE);
    return 0;
}
int main(void)
{
    int code;
    code=gc_test();if(code) return code;
    code=ps2_test();if(code) return code;
    puts("gcps2 synthetic tests ok");
    return 0;
}
