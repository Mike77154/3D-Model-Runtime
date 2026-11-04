#include "g3d_codec.h"
#include "g3d_kib.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { int rc__ = (x); if (rc__ != G3D_OK) { printf("FAIL:%s:%d rc=%d %s\n", __FILE__, __LINE__, rc__, g3d_errstr(rc__)); return 1; } } while(0)
#define ASSERT(x) do { if (!(x)) { printf("ASSERT:%s:%d\n", __FILE__, __LINE__); return 2; } } while(0)

static void wr32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static unsigned int rd32(const unsigned char *p)
{
    return ((unsigned int)p[0]) |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static void make_one_string_block(g3d_strblock *sb, unsigned char *buf, const char *s)
{
    unsigned int len = (unsigned int)strlen(s) + 1u;
    wr32(buf + 0u, 4u);
    memcpy(buf + 4u, s, len);
    sb->count = 1;
    sb->size = (g3d_s32)(4u + len);
    sb->data = buf;
}

static const char ascii_mot_sample[] =
    "MOTN"
    " 0.F0\n"
    "NameID asciiwalk\n"
    "MaintainNames 1\n"
    "PathCount 1\n"
    "NameChecksum 1260\n"
    "Events 1\n"
    "TKEV"
    " 0.F0\n"
    "DataSize 5\n"
    "TimeKeys 1\n"
    "0.500000 0\n"
    "step\n"
    "NameArray 1\n"
    "SBLK"
    " 0.F0\n"
    "Strings 1\n"
    "Root\n"
    "PathArray 1\n"
    "PATH"
    " 0.F2\n"
    "Rotation 1 3\n"
    "Keys 2 0 0 0\n"
    "0.000000 1.000000 0.000000 0.000000 0.000000\n"
    "1.000000 1.000000 0.000000 0.000000 0.000000\n"
    "Translation 1 0\n"
    "Keys 2 0 0 0\n"
    "0.000000 0.000000 0.000000 0.000000\n"
    "1.000000 10.000000 0.000000 0.000000\n";

static const char ascii_mot_hinge_lineartime[] =
    "MOTN"
    " 0.F0\n"
    "NameID ascii_hinge\n"
    "MaintainNames 1\n"
    "PathCount 1\n"
    "NameChecksum 1260\n"
    "Events 0\n"
    "NameArray 1\n"
    "SBLK"
    " 0.F0\n"
    "Strings 1\n"
    "Root\n"
    "PathArray 1\n"
    "PATH"
    " 0.F2\n"
    "Rotation 1 3\n"
    "Keys 2 0 3 0\n"
    "0.000000 1.000000 Start T,Delta T\n"
    "0.000000 0.000000 1.000000 Axis\n"
    "0.000000\n"
    "1.570796\n"
    "Translation 1 0\n"
    "Keys 3 0 2 0\n"
    "0.000000 0.500000 Start T,Delta T\n"
    "0.000000 0.000000 0.000000\n"
    "5.000000 0.000000 0.000000\n"
    "10.000000 0.000000 0.000000\n";


static const char ascii_mot_f0_oldpath[] =
    "MOTN"
    " 0.F0\n"
    "NameID o\n"
    "MaintainNames 1\n"
    "PathCount 1\n"
    "NameChecksum 1260\n"
    "Events 0\n"
    "NameArray 1\n"
    "SBLK"
    " 0.F0\n"
    "Strings 1\n"
    "Root\n"
    "PathArray 1\n"
    "PATH"
    " 0.F0\n"
    "Looped 0\n"
    "Rotation 1\n"
    "CHNL"
    " 0.F0\n"
    "InterpolationType 2\n"
    "StartTime 0\n"
    "EndTime 1\n"
    "KeyList 2\n"
    "0\n"
    "QKFS"
    "1 0 0 0\n"
    "1\n"
    "QKFS"
    "1 0 0 0\n"
    "Translation 1\n"
    "CHNL"
    " 0.F0\n"
    "InterpolationType 1\n"
    "StartTime 0\n"
    "EndTime 1\n"
    "KeyList 2\n"
    "0\n"
    "VKFH"
    "0 0 0 0 0 0 0 0 0\n"
    "1\n"
    "VKFH"
    "8 0 0 0 0 0 0 0 0\n";

int main(void)
{
    unsigned char bone_sb_buf[16];
    unsigned char mat_sb_buf[20];
    unsigned char motion_name_sb_buf[16];
    unsigned char bitmap_payload[15];
    unsigned char pal_bitmap_payload[32];
    unsigned char comp_bitmap_payload[128];
    unsigned char paeth_bitmap_payload[64];
    unsigned char comp_pal_bitmap_payload[160];
    unsigned char kib_payload[64];
    unsigned char kib_wrapped[96];
    unsigned char raw_mip_buf[64];
    unsigned char wavelet_payload[32];
    unsigned char wavelet_pixels[8];
    unsigned char rgba_buf[64];
    unsigned char arena_mem[1 << 16];
    unsigned char file_buf[1 << 16];
    unsigned char masked_file_buf[1 << 16];
    unsigned char body_buf[1 << 12];
    unsigned char geometry_buf[1 << 12];
    unsigned char masked_body_buf[1 << 12];
    unsigned char motion_buf[2048];
    unsigned char motion_buf2[2048];
    unsigned char ascii_out_buf[4096];
    unsigned char sb_official_buf[32];
    unsigned char sb_legacy_buf[32];
    unsigned char qk_order_buf[64];
    g3d_arena arena;
    g3d_body body;
    g3d_body parsed_body;
    g3d_body raw_geometry_body;
    g3d_body masked_body;
    g3d_body_bone bone;
    g3d_body_material material;
    g3d_body_xskin_vertex vertices[3];
    g3d_body_normal normals[3];
    g3d_body_triangle face;
    g3d_strblock bone_sb;
    g3d_strblock mat_sb;
    g3d_strblock mot_name_sb;
    g3d_strblock parsed_sb_official;
    g3d_strblock parsed_sb_legacy;
    g3d_motion motion;
    g3d_motion motion2;
    g3d_motion parsed_motion;
    g3d_motion ascii_motion;
    g3d_motion ascii_motion_hinge;
    g3d_motion ascii_motion_f0;
    g3d_motion ascii_roundtrip;
    g3d_path path;
    g3d_path path2;
    g3d_path path_hinge;
    g3d_path path_lineartime;
    g3d_path path_bad_time;
    g3d_path path_bad_lineartime;
    g3d_vkframe vk;
    g3d_vkframe vk2;
    g3d_vkframe vk_lineartime;
    g3d_vkframe vk_bad_lineartime;
    g3d_qkframe qk;
    g3d_qkframe qk2;
    g3d_qkframe qk_order;
    g3d_qkframe qk_order_parsed;
    g3d_qkframe qk_hinge;
    g3d_qkframe qk_bad_time;
    g3d_u32 vk_times[2];
    g3d_fvec3 vk_vecs[2];
    g3d_u32 vk2_times[2];
    g3d_fvec3 vk2_vecs[2];
    g3d_u32 vk_lineartime_times[2];
    g3d_fvec3 vk_lineartime_vecs[3];
    g3d_u32 qk_times[2];
    g3d_u32 qk_order_time[1];
    g3d_u32 qk_hinge_times[2];
    g3d_u32 qk_hinge_angles[2];
    g3d_u32 bad_times[2];
    g3d_u32 bad_lineartime_times[2];
    g3d_fquat qk_quats[2];
    g3d_fquat qk_order_quat[1];
    g3d_motion motions_arr[2];
    g3d_fixvec3 sample_t;
    g3d_fixvec3 lineartime_t;
    g3d_q15quat sample_q;
    g3d_q15quat hinge_q;
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    g3d_fixmat34 sample_m;
    g3d_actor actor;
    g3d_actor parsed_actor;
    g3d_actor masked_actor;
    g3d_vfs vfs_check;
    g3d_asset_slot slots[4];
    g3d_asset_manager mgr;
    g3d_skinned_mesh mesh;
    g3d_u32 used = 0u;
    g3d_u32 body_used = 0u;
    g3d_u32 geometry_used = 0u;
    g3d_u32 geometry_parse_used = 0u;
    g3d_u32 masked_body_used = 0u;
    g3d_u32 masked_actor_used = 0u;
    g3d_u32 motion_used = 0u;
    g3d_u32 motion_used2 = 0u;
    g3d_u32 sb_used = 0u;
    g3d_u32 qk_order_used = 0u;
    g3d_u32 ascii_used = 0u;
    g3d_u32 event_time_bits = 0u;
    g3d_u32 event_offset = 0u;
    g3d_u32 rgba_w = 0u;
    g3d_u32 rgba_h = 0u;
    g3d_u32 rgba_bytes = 0u;
    g3d_u32 kib_used = 0u;
    g3d_u32 kib_payload_used = 0u;
    g3d_u32 kib_method = 0u;
    g3d_u32 raw_bytes = 0u;
    const char *event_text = 0;
    g3d_s32 event_index = -1;
    g3d_s32 event_count = 0;
    g3d_s32 path_index = -1;
    g3d_s32 bone_index = -1;
    g3d_s32 material_index = -1;
    g3d_s32 motion_index = -1;
    g3d_s32 bad_mat_index = -1;
    g3d_u32 bone_vertex_count = 0u;
    g3d_bitmap_info bitmap_info;
    g3d_fixvec3 computed_bbox_min;
    g3d_fixvec3 computed_bbox_max;
    g3d_u8 cf_mask = 0u;

    g3d_arena_init(&arena, arena_mem, (g3d_u32)sizeof(arena_mem));

    memset(&body, 0, sizeof(body));
    memset(&parsed_body, 0, sizeof(parsed_body));
    memset(&raw_geometry_body, 0, sizeof(raw_geometry_body));
    memset(&masked_body, 0, sizeof(masked_body));
    memset(&bone, 0, sizeof(bone));
    memset(&material, 0, sizeof(material));
    memset(vertices, 0, sizeof(vertices));
    memset(normals, 0, sizeof(normals));
    memset(&face, 0, sizeof(face));
    memset(&motion, 0, sizeof(motion));
    memset(&motion2, 0, sizeof(motion2));
    memset(&parsed_motion, 0, sizeof(parsed_motion));
    memset(&ascii_motion, 0, sizeof(ascii_motion));
    memset(&ascii_motion_hinge, 0, sizeof(ascii_motion_hinge));
    memset(&path, 0, sizeof(path));
    memset(&path2, 0, sizeof(path2));
    memset(&path_hinge, 0, sizeof(path_hinge));
    memset(&path_lineartime, 0, sizeof(path_lineartime));
    memset(&path_bad_time, 0, sizeof(path_bad_time));
    memset(&path_bad_lineartime, 0, sizeof(path_bad_lineartime));
    memset(&vk, 0, sizeof(vk));
    memset(&vk2, 0, sizeof(vk2));
    memset(&vk_lineartime, 0, sizeof(vk_lineartime));
    memset(&vk_bad_lineartime, 0, sizeof(vk_bad_lineartime));
    memset(&qk, 0, sizeof(qk));
    memset(&qk2, 0, sizeof(qk2));
    memset(&qk_order, 0, sizeof(qk_order));
    memset(&qk_order_parsed, 0, sizeof(qk_order_parsed));
    memset(&qk_hinge, 0, sizeof(qk_hinge));
    memset(&qk_bad_time, 0, sizeof(qk_bad_time));
    memset(&actor, 0, sizeof(actor));
    memset(&parsed_actor, 0, sizeof(parsed_actor));
    memset(&masked_actor, 0, sizeof(masked_actor));
    memset(&vfs_check, 0, sizeof(vfs_check));
    memset(&mesh, 0, sizeof(mesh));

    make_one_string_block(&bone_sb, bone_sb_buf, "Root");
    make_one_string_block(&mat_sb, mat_sb_buf, "Default");
    make_one_string_block(&mot_name_sb, motion_name_sb_buf, "Root");
    ASSERT(g3d_strblock_checksum(&bone_sb) == 1260);
    cf_mask = g3d_vfs_context_mask_from_string("Genesis3");
    ASSERT(cf_mask == 0x67u);

    bone.parent_bone_index = -1;
    bone.attachment.f[0] = 0x3f800000u;
    bone.attachment.f[4] = 0x3f800000u;
    bone.attachment.f[8] = 0x3f800000u;

    /* Tiny valid Genesis3D geBitmap payload: GeBm 4.0, 2x2 8bit gray,
       one uncompressed mip, terminator mip 0x0F. */
    bitmap_payload[0] = 'G';
    bitmap_payload[1] = 'e';
    bitmap_payload[2] = 'B';
    bitmap_payload[3] = 'm';
    bitmap_payload[4] = 0x40u;
    bitmap_payload[5] = 0x01u;
    bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_8BIT_GRAY;
    bitmap_payload[7] = 0x01u;
    bitmap_payload[8] = 0x11u;
    bitmap_payload[9] = 0x00u;
    bitmap_payload[10] = 1u;
    bitmap_payload[11] = 2u;
    bitmap_payload[12] = 3u;
    bitmap_payload[13] = 4u;
    bitmap_payload[14] = 0x0Fu;
    material.has_bitmap = 1u;
    material.bitmap_data = bitmap_payload;
    material.bitmap_size = 15u;
    material.red_bits = 0x3f800000u;
    material.green_bits = 0x3f800000u;
    material.blue_bits = 0x3f800000u;

    body.bbox_max.x_bits = 0x3f800000u;
    body.bbox_max.y_bits = 0x3f800000u;
    body.bbox_max.z_bits = 0u;
    body.bone_count = 1;
    body.bones = &bone;
    body.bone_names = &bone_sb;
    body.material_count = 1;
    body.materials = &material;
    body.material_names = &mat_sb;
    body.xskin_vertex_count = 3;
    body.xskin_vertices = vertices;
    body.normal_count = 3;
    body.normals = normals;
    body.lod_count = 1u;
    body.lods[0].face_count = 1u;
    body.lods[0].faces = &face;

    vertices[0].xpoint.x_bits = 0u;
    vertices[0].xpoint.y_bits = 0u;
    vertices[0].xpoint.z_bits = 0u;
    vertices[0].u_bits = 0u;
    vertices[0].v_bits = 0u;
    vertices[0].lod_mask = 1u;
    vertices[0].bone_index = 0;

    vertices[1].xpoint.x_bits = 0x3f800000u;
    vertices[1].xpoint.y_bits = 0u;
    vertices[1].xpoint.z_bits = 0u;
    vertices[1].u_bits = 0x3f800000u;
    vertices[1].v_bits = 0u;
    vertices[1].lod_mask = 1u;
    vertices[1].bone_index = 0;

    vertices[2].xpoint.x_bits = 0u;
    vertices[2].xpoint.y_bits = 0x3f800000u;
    vertices[2].xpoint.z_bits = 0u;
    vertices[2].u_bits = 0u;
    vertices[2].v_bits = 0x3f800000u;
    vertices[2].lod_mask = 1u;
    vertices[2].bone_index = 0;

    normals[0].normal.z_bits = 0x3f800000u;
    normals[0].lod_mask = 1u;
    normals[0].bone_index = 0;
    normals[1] = normals[0];
    normals[2] = normals[0];

    face.vtx_index[0] = 0;
    face.vtx_index[1] = 1;
    face.vtx_index[2] = 2;
    face.normal_index[0] = 0;
    face.normal_index[1] = 1;
    face.normal_index[2] = 2;
    face.material_index = 0;

    vk_times[0] = 0u;
    vk_times[1] = 0x3f800000u;
    vk_vecs[0].x_bits = 0u;
    vk_vecs[0].y_bits = 0u;
    vk_vecs[0].z_bits = 0u;
    vk_vecs[1].x_bits = 0x41200000u;
    vk_vecs[1].y_bits = 0u;
    vk_vecs[1].z_bits = 0u;
    vk.interpolation = G3D_VK_LINEAR;
    vk.compression = 0u;
    vk.looping = 1u;
    vk.count = 2u;
    vk.time_value_count = 2u;
    vk.time_bits = vk_times;
    vk.vectors = vk_vecs;

    vk2_times[0] = 0u;
    vk2_times[1] = 0x3f800000u;
    vk2_vecs[0].x_bits = 0u;
    vk2_vecs[0].y_bits = 0u;
    vk2_vecs[0].z_bits = 0u;
    vk2_vecs[1].x_bits = 0u;
    vk2_vecs[1].y_bits = 0x41000000u;
    vk2_vecs[1].z_bits = 0u;
    vk2.interpolation = G3D_VK_LINEAR;
    vk2.compression = 0u;
    vk2.looping = 1u;
    vk2.count = 2u;
    vk2.time_value_count = 2u;
    vk2.time_bits = vk2_times;
    vk2.vectors = vk2_vecs;

    /* Genesis3D LINEARTIME compression stores start time + delta time,
       matching Actor/vkframe.c.  This guards against the old start/end
       interpretation, which collapses multi-key timing. */
    vk_lineartime_times[0] = 0x3f800000u; /* start = 1.0 */
    vk_lineartime_times[1] = 0x3f000000u; /* delta = 0.5 */
    vk_lineartime_vecs[0].x_bits = 0x3f800000u; /* 1.0 */
    vk_lineartime_vecs[0].y_bits = 0u;
    vk_lineartime_vecs[0].z_bits = 0u;
    vk_lineartime_vecs[1].x_bits = 0x40000000u; /* 2.0 */
    vk_lineartime_vecs[1].y_bits = 0u;
    vk_lineartime_vecs[1].z_bits = 0u;
    vk_lineartime_vecs[2].x_bits = 0x40400000u; /* 3.0 */
    vk_lineartime_vecs[2].y_bits = 0u;
    vk_lineartime_vecs[2].z_bits = 0u;
    vk_lineartime.interpolation = G3D_VK_LINEAR;
    vk_lineartime.compression = G3D_VK_LINEARTIME_COMPRESSION;
    vk_lineartime.looping = 0u;
    vk_lineartime.count = 3u;
    vk_lineartime.time_value_count = 2u;
    vk_lineartime.time_bits = vk_lineartime_times;
    vk_lineartime.vectors = vk_lineartime_vecs;

    qk_times[0] = 0u;
    qk_times[1] = 0x3f800000u;
    qk_quats[0].x_bits = 0u;
    qk_quats[0].y_bits = 0u;
    qk_quats[0].z_bits = 0u;
    qk_quats[0].w_bits = 0x3f800000u;
    qk_quats[1] = qk_quats[0];
    qk.interpolation = G3D_QK_SLERP;
    qk.compression = 0u;
    qk.looping = 1u;
    qk.count = 2u;
    qk.time_value_count = 2u;
    qk.time_bits = qk_times;
    qk.quats = qk_quats;
    qk2 = qk;

    bad_times[0] = 0x3f800000u;
    bad_times[1] = 0u;
    qk_bad_time = qk;
    qk_bad_time.time_bits = bad_times;

    bad_lineartime_times[0] = 0u;
    bad_lineartime_times[1] = 0u;
    vk_bad_lineartime = vk_lineartime;
    vk_bad_lineartime.time_bits = bad_lineartime_times;

    /* Hinge-compressed QKFrames store one axis plus one angle per key. */
    qk_hinge_times[0] = 0u;
    qk_hinge_times[1] = 0x3f800000u;
    qk_hinge_angles[0] = 0u;
    qk_hinge_angles[1] = 0x3fc90fdbu; /* pi/2 radians */
    qk_hinge.interpolation = G3D_QK_SLERP;
    qk_hinge.compression = G3D_QK_HINGE_COMPRESSION;
    qk_hinge.looping = 0u;
    qk_hinge.count = 2u;
    qk_hinge.time_value_count = 2u;
    qk_hinge.time_bits = qk_hinge_times;
    qk_hinge.hinge_axis.x_bits = 0u;
    qk_hinge.hinge_axis.y_bits = 0u;
    qk_hinge.hinge_axis.z_bits = 0x3f800000u;
    qk_hinge.angle_bits = qk_hinge_angles;

    path_hinge.has_rotation = 1u;
    path_hinge.rotation_interp = G3D_PATH_QK_SLERP;
    path_hinge.rotation = qk_hinge;

    path_lineartime.has_translation = 1u;
    path_lineartime.has_rotation = 0u;
    path_lineartime.translation_interp = G3D_PATH_VK_LINEAR;
    path_lineartime.translation = vk_lineartime;

    path_bad_time.has_translation = 0u;
    path_bad_time.has_rotation = 1u;
    path_bad_time.rotation_interp = G3D_PATH_QK_SLERP;
    path_bad_time.rotation = qk_bad_time;

    path_bad_lineartime.has_translation = 1u;
    path_bad_lineartime.has_rotation = 0u;
    path_bad_lineartime.translation_interp = G3D_PATH_VK_LINEAR;
    path_bad_lineartime.translation = vk_bad_lineartime;

    path.has_translation = 1u;
    path.has_rotation = 1u;
    path.translation_interp = G3D_PATH_VK_LINEAR;
    path.rotation_interp = G3D_PATH_QK_SLERP;
    path.looped = 1u;
    path.translation = vk;
    path.rotation = qk;

    path2 = path;
    path2.translation = vk2;
    path2.rotation = qk2;

    /* Regression: original Genesis3D SBKB writes Count,Size; older codec
       snapshots wrote Size,Count.  The parser accepts both, writer emits official. */
    memset(sb_official_buf, 0, sizeof(sb_official_buf));
    wr32(sb_official_buf + 0u, 0x424B4253u);
    wr32(sb_official_buf + 4u, 1u);
    wr32(sb_official_buf + 8u, 9u);
    wr32(sb_official_buf + 12u, 4u);
    memcpy(sb_official_buf + 16u, "Root", 5u);
    g3d_arena_reset(&arena);
    CHECK(g3d_strblock_parse(&parsed_sb_official, sb_official_buf, 21u, &arena, &sb_used));
    ASSERT(sb_used == 21u);
    ASSERT(strcmp(g3d_strblock_get(&parsed_sb_official, 0), "Root") == 0);

    memset(sb_legacy_buf, 0, sizeof(sb_legacy_buf));
    wr32(sb_legacy_buf + 0u, 0x424B4253u);
    wr32(sb_legacy_buf + 4u, 9u);
    wr32(sb_legacy_buf + 8u, 1u);
    wr32(sb_legacy_buf + 12u, 4u);
    memcpy(sb_legacy_buf + 16u, "Root", 5u);
    g3d_arena_reset(&arena);
    CHECK(g3d_strblock_parse(&parsed_sb_legacy, sb_legacy_buf, 21u, &arena, &sb_used));
    ASSERT(sb_used == 21u);
    ASSERT(strcmp(g3d_strblock_get(&parsed_sb_legacy, 0), "Root") == 0);

    /* Regression: Genesis3D serializes geQuaternion in file order W,X,Y,Z;
       the public g3d_fquat remains API order X,Y,Z,W. */
    qk_order_time[0] = 0u;
    qk_order_quat[0].x_bits = 0x11111111u;
    qk_order_quat[0].y_bits = 0x22222222u;
    qk_order_quat[0].z_bits = 0x33333333u;
    qk_order_quat[0].w_bits = 0x44444444u;
    qk_order.interpolation = G3D_QK_SLERP;
    qk_order.compression = 0u;
    qk_order.looping = 0u;
    qk_order.count = 1u;
    qk_order.time_value_count = 1u;
    qk_order.time_bits = qk_order_time;
    qk_order.quats = qk_order_quat;
    CHECK(g3d_qkframe_write_binary(&qk_order, qk_order_buf, (g3d_u32)sizeof(qk_order_buf), &qk_order_used));
    ASSERT(rd32(qk_order_buf + 16u) == 0x44444444u);
    ASSERT(rd32(qk_order_buf + 20u) == 0x11111111u);
    ASSERT(rd32(qk_order_buf + 24u) == 0x22222222u);
    ASSERT(rd32(qk_order_buf + 28u) == 0x33333333u);
    g3d_arena_reset(&arena);
    CHECK(g3d_qkframe_parse_binary(&qk_order_parsed, qk_order_buf, qk_order_used, &arena, 0));
    ASSERT(qk_order_parsed.quats[0].x_bits == 0x11111111u);
    ASSERT(qk_order_parsed.quats[0].y_bits == 0x22222222u);
    ASSERT(qk_order_parsed.quats[0].z_bits == 0x33333333u);
    ASSERT(qk_order_parsed.quats[0].w_bits == 0x44444444u);
    g3d_arena_reset(&arena);

    motion.name = "walk";
    motion.name_len = 0u;
    motion.maintain_names = 1u;
    motion.node_type = G3D_MOTION_NODE_LEAF;
    motion.leaf.path_count = 1;
    motion.leaf.name_checksum = 0;
    motion.leaf.flags = 0x2u;
    motion.leaf.name_array = &mot_name_sb;
    motion.leaf.paths = &path;

    motion2.name = "strafe";
    motion2.name_len = 0u;
    motion2.maintain_names = 1u;
    motion2.node_type = G3D_MOTION_NODE_LEAF;
    motion2.leaf.path_count = 1;
    motion2.leaf.name_checksum = 0;
    motion2.leaf.flags = 0x2u;
    motion2.leaf.name_array = &mot_name_sb;
    motion2.leaf.paths = &path2;

    motions_arr[0] = motion;
    motions_arr[1] = motion2;

    CHECK(g3d_path_validate(&path));
    CHECK(g3d_path_validate(&path_hinge));
    CHECK(g3d_path_sample_channels_fix16(&path_hinge, 0x00008000L, &hinge_q, 0));
    ASSERT(hinge_q.z > 11000 && hinge_q.z < 14000);
    ASSERT(hinge_q.w > 29000 && hinge_q.w < 31500);
    CHECK(g3d_path_validate(&path_lineartime));
    ASSERT(g3d_path_validate(&path_bad_time) == G3D_ERR_CORRUPT);
    ASSERT(g3d_path_validate(&path_bad_lineartime) == G3D_ERR_CORRUPT);
    CHECK(g3d_path_get_time_extents_fix16(&path_lineartime, &start_time, &end_time));
    ASSERT(start_time == 0x00010000L);
    ASSERT(end_time == 0x00020000L);
    CHECK(g3d_path_sample_channels_fix16(&path_lineartime, 0x00014000L, 0, &lineartime_t));
    ASSERT(lineartime_t.x > 0x00017F00L && lineartime_t.x < 0x00018100L);
    CHECK(g3d_path_sample_channels_fix16(&path_lineartime, 0x0001C000L, 0, &lineartime_t));
    ASSERT(lineartime_t.x > 0x00027F00L && lineartime_t.x < 0x00028100L);
    CHECK(g3d_motion_validate(&motion));
    CHECK(g3d_motion_validate(&motion2));
    CHECK(g3d_body_validate(&body));

    CHECK(g3d_motion_write_binary(&motion, motion_buf, (g3d_u32)sizeof(motion_buf), &motion_used));
    CHECK(g3d_motion_write_binary(&motion2, motion_buf2, (g3d_u32)sizeof(motion_buf2), &motion_used2));
    ASSERT(motion_used > 0u);
    ASSERT(motion_used2 > 0u);

    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&parsed_motion, motion_buf, motion_used, &arena));
    CHECK(g3d_motion_validate(&parsed_motion));
    ASSERT(parsed_motion.leaf.name_checksum == 1260);
    CHECK(g3d_motion_find_path_index(&parsed_motion, "Root", &path_index));
    ASSERT(path_index == 0);
    motion_buf[motion_used] = 'X';
    g3d_arena_reset(&arena);
    ASSERT(g3d_motion_parse_binary(&parsed_motion, motion_buf, motion_used + 1u, &arena) == G3D_ERR_CORRUPT);
    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&parsed_motion, motion_buf, motion_used, &arena));
    ASSERT(strcmp(g3d_motion_get_path_name(&parsed_motion, 0), "Root") == 0);
    ASSERT(g3d_motion_name_checksum(&parsed_motion) == 1260);

    CHECK(g3d_motion_get_time_extents_fix16(&parsed_motion, &start_time, &end_time));
    ASSERT(start_time == 0);
    ASSERT(end_time == 0x00010000L);
    CHECK(g3d_motion_sample_channels_named_fix16(&parsed_motion, "Root", 0x00008000L, &sample_q, &sample_t));
    ASSERT(sample_t.x > 0x0004F000L && sample_t.x < 0x00051000L);
    ASSERT(sample_q.w > 32000 && sample_q.w <= 32768);
    CHECK(g3d_motion_sample_channels_fix16(&parsed_motion, 0, 0x00014000L, &sample_q, &sample_t));
    ASSERT(sample_t.x > 0x00027000L && sample_t.x < 0x00029000L);
    g3d_sample_xform_to_matrix(&sample_q, &sample_t, &sample_m);
    ASSERT(sample_m.m[3] == sample_t.x);

    /* Regression: standalone .mot files written by the original Genesis3D tools
       are ASCII MOTN, not MTNB.  Parse MOTN/TKEV/SBLK/PATH F2 and sample it. */
    path_index = -1;
    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&ascii_motion, ascii_mot_sample,
                                   (g3d_u32)(sizeof(ascii_mot_sample) - 1u),
                                   &arena));
    CHECK(g3d_motion_validate(&ascii_motion));
    ASSERT(ascii_motion.node_type == G3D_MOTION_NODE_LEAF);
    ASSERT(ascii_motion.maintain_names == 1u);
    ASSERT(ascii_motion.leaf.flags == 0x3u);
    ASSERT(ascii_motion.leaf.path_count == 1);
    ASSERT(ascii_motion.name != 0);
    ASSERT(strcmp(ascii_motion.name, "asciiwalk") == 0);
    ASSERT(ascii_motion.leaf.events != 0);
    CHECK(g3d_tkevents_get(ascii_motion.leaf.events, 0, &event_time_bits, &event_offset));
    ASSERT(event_time_bits == 0x3f000000u);
    ASSERT(event_offset == 0u);
    ASSERT(strcmp((const char *)ascii_motion.leaf.events->event_data, "step") == 0);
    CHECK(g3d_tkevents_get_text(ascii_motion.leaf.events, 0, &event_time_bits, &event_text));
    ASSERT(strcmp(event_text, "step") == 0);
    event_count = g3d_tkevents_count_between_fix16(ascii_motion.leaf.events, 0, 0x00010000L);
    ASSERT(event_count == 1);
    event_index = -1;
    CHECK(g3d_tkevents_find_next_between_fix16(ascii_motion.leaf.events, 0, 0x00010000L,
                                                event_index, &event_index,
                                                &event_time_bits, &event_text));
    ASSERT(event_index == 0);
    ASSERT(strcmp(event_text, "step") == 0);
    ASSERT(g3d_tkevents_find_next_between_fix16(ascii_motion.leaf.events, 0, 0x00010000L,
                                                event_index, &event_index,
                                                &event_time_bits, &event_text) == G3D_ERR_NOT_FOUND);
    ASSERT(g3d_motion_count_events_between_fix16(&ascii_motion, 0, 0x00010000L) == 1);
    event_index = -1;
    CHECK(g3d_motion_find_next_event_between_fix16(&ascii_motion, 0, 0x00010000L,
                                                   event_index, &event_index,
                                                   &event_time_bits, &event_text));
    ASSERT(event_index == 0);
    ASSERT(strcmp(event_text, "step") == 0);
    ASSERT(strcmp(g3d_strblock_get(ascii_motion.leaf.name_array, 0), "Root") == 0);
    CHECK(g3d_motion_find_path_index(&ascii_motion, "Root", &path_index));
    ASSERT(path_index == 0);
    CHECK(g3d_motion_get_time_extents_fix16(&ascii_motion, &start_time, &end_time));
    ASSERT(start_time == 0);
    ASSERT(end_time == 0x00010000L);
    CHECK(g3d_motion_sample_channels_named_fix16(&ascii_motion, "Root", 0x00008000L, &sample_q, &sample_t));
    ASSERT(sample_t.x > 0x0004F000L && sample_t.x < 0x00051000L);
    ASSERT(sample_q.w > 32000 && sample_q.w <= 32768);

    /* Writer regression: emit official Genesis3D-style MOTN ASCII, then
       reparse it through the same reader. */
    CHECK(g3d_motion_write_ascii(&ascii_motion, ascii_out_buf,
                                  (g3d_u32)sizeof(ascii_out_buf), &ascii_used));
    ASSERT(ascii_used > 0u);
    ASSERT(rd32(ascii_out_buf) == 0x4E544F4Du);
    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&ascii_roundtrip, ascii_out_buf, ascii_used, &arena));
    CHECK(g3d_motion_validate(&ascii_roundtrip));
    CHECK(g3d_motion_sample_channels_named_fix16(&ascii_roundtrip, "Root", 0x00008000L, &sample_q, &sample_t));
    ASSERT(sample_t.x > 0x0004F000L && sample_t.x < 0x00051000L);
    ASSERT(strcmp(g3d_strblock_get(ascii_roundtrip.leaf.name_array, 0), "Root") == 0);

    /* Regression: Genesis3D PATH 0.F0 old channel format with CHNL/QKFS/VKFH
       blocks can be parsed and sampled. */
    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&ascii_motion_f0, ascii_mot_f0_oldpath,
                                   (g3d_u32)(sizeof(ascii_mot_f0_oldpath) - 1u),
                                   &arena));
    CHECK(g3d_motion_validate(&ascii_motion_f0));
    ASSERT(ascii_motion_f0.leaf.paths[0].rotation_interp == G3D_PATH_QK_SLERP);
    ASSERT(ascii_motion_f0.leaf.paths[0].translation_interp == G3D_PATH_VK_HERMITE);
    CHECK(g3d_motion_sample_channels_named_fix16(&ascii_motion_f0, "Root", 0x00008000L, &sample_q, &sample_t));
    ASSERT(sample_t.x > 0x0003E000L && sample_t.x < 0x00042000L);
    ASSERT(sample_q.w > 32000 && sample_q.w <= 32768);

    /* ASCII QK hinge + lineartime and ASCII VK lineartime, matching the
       original writer's trailing comments after start/delta and axis lines. */
    g3d_arena_reset(&arena);
    CHECK(g3d_motion_parse_binary(&ascii_motion_hinge, ascii_mot_hinge_lineartime,
                                   (g3d_u32)(sizeof(ascii_mot_hinge_lineartime) - 1u),
                                   &arena));
    CHECK(g3d_motion_validate(&ascii_motion_hinge));
    CHECK(g3d_motion_get_time_extents_fix16(&ascii_motion_hinge, &start_time, &end_time));
    ASSERT(start_time == 0);
    ASSERT(end_time == 0x00010000L);
    CHECK(g3d_motion_sample_channels_named_fix16(&ascii_motion_hinge, "Root", 0x00008000L, &hinge_q, &sample_t));
    ASSERT(sample_t.x > 0x0004F000L && sample_t.x < 0x00051000L);
    ASSERT(hinge_q.z > 11000 && hinge_q.z < 14000);
    ASSERT(hinge_q.w > 29000 && hinge_q.w < 31500);

    CHECK(g3d_body_write_geometry(&body, geometry_buf, (g3d_u32)sizeof(geometry_buf), &geometry_used));
    ASSERT(geometry_used > 0u);
    ASSERT(rd32(geometry_buf) == 0x5E444F42u);
    g3d_arena_reset(&arena);
    CHECK(g3d_body_parse_geometry(&raw_geometry_body, geometry_buf, geometry_used,
                                  &arena, &geometry_parse_used));
    ASSERT(geometry_parse_used == geometry_used);
    CHECK(g3d_body_validate(&raw_geometry_body));
    ASSERT(strcmp(g3d_strblock_get(raw_geometry_body.bone_names, 0), "Root") == 0);
    ASSERT(raw_geometry_body.xskin_vertex_count == 3);
    ASSERT(raw_geometry_body.lod_count == 1u);
    ASSERT(raw_geometry_body.materials[0].has_bitmap == 1u);
    ASSERT(raw_geometry_body.materials[0].bitmap_size == 0u);

    CHECK(g3d_body_write_file(&body, body_buf, (g3d_u32)sizeof(body_buf), &body_used));
    ASSERT(body_used > 0u);

    g3d_arena_reset(&arena);
    CHECK(g3d_body_parse_file(&parsed_body, body_buf, body_used, &arena));
    CHECK(g3d_body_validate(&parsed_body));
    ASSERT(strcmp(g3d_strblock_get(parsed_body.bone_names, 0), "Root") == 0);
    ASSERT(parsed_body.xskin_vertex_count == 3);
    ASSERT(parsed_body.lod_count == 1u);
    ASSERT(parsed_body.lods[0].face_count == 1u);
    ASSERT(parsed_body.materials[0].has_bitmap == 1u);
    ASSERT(parsed_body.materials[0].bitmap_size == 15u);
    ASSERT(parsed_body.materials[0].bitmap_data != 0);
    ASSERT(parsed_body.materials[0].bitmap_data[0] == 'G');
    ASSERT(parsed_body.materials[0].bitmap_data[1] == 'e');
    ASSERT(parsed_body.materials[0].bitmap_data[2] == 'B');
    ASSERT(parsed_body.materials[0].bitmap_data[3] == 'm');
    CHECK(g3d_body_get_material_bitmap_info(&parsed_body, 0, &bitmap_info));
    ASSERT(bitmap_info.width == 2u);
    ASSERT(bitmap_info.height == 2u);
    ASSERT(bitmap_info.format == G3D_PIXELFORMAT_8BIT_GRAY);
    ASSERT(bitmap_info.mip_count == 1u);
    ASSERT(bitmap_info.mips[0].data_size == 4u);
    ASSERT(g3d_pixel_format_bytes_per_pixel(G3D_PIXELFORMAT_8BIT_GRAY) == 1u);
    ASSERT(strcmp(g3d_pixel_format_name(G3D_PIXELFORMAT_8BIT_GRAY), "8bit gray") == 0);
    CHECK(g3d_bitmap_decode_mip_rgba32_arena(parsed_body.materials[0].bitmap_data,
                                             parsed_body.materials[0].bitmap_size,
                                             0u,
                                             G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
                                             rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                             &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_w == 2u);
    ASSERT(rgba_h == 2u);
    ASSERT(rgba_bytes == 16u);
    ASSERT(rgba_buf[0] == 1u && rgba_buf[1] == 1u && rgba_buf[2] == 1u && rgba_buf[3] == 255u);
    ASSERT(rgba_buf[12] == 4u && rgba_buf[13] == 4u && rgba_buf[14] == 4u && rgba_buf[15] == 255u);
    CHECK(g3d_body_decode_material_rgba32_arena(&parsed_body, 0, 0u,
                                                G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
                                                rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                                &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_w == 2u && rgba_h == 2u && rgba_bytes == 16u);
    ASSERT(g3d_bitmap_decode_mip_rgba32_arena(parsed_body.materials[0].bitmap_data,
                                            parsed_body.materials[0].bitmap_size, 1u, 0u,
                                            rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                            &rgba_w, &rgba_h, &rgba_bytes, &arena) == G3D_ERR_NOT_FOUND);

    pal_bitmap_payload[0] = 'G';
    pal_bitmap_payload[1] = 'e';
    pal_bitmap_payload[2] = 'B';
    pal_bitmap_payload[3] = 'm';
    pal_bitmap_payload[4] = 0x40u;
    pal_bitmap_payload[5] = 0x09u;
    pal_bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_8BIT;
    pal_bitmap_payload[7] = 0x01u;
    pal_bitmap_payload[8] = 0x10u;
    pal_bitmap_payload[9] = (unsigned char)G3D_PIXELFORMAT_24BIT_RGB;
    pal_bitmap_payload[10] = 2u;
    pal_bitmap_payload[11] = 255u;
    pal_bitmap_payload[12] = 0u;
    pal_bitmap_payload[13] = 0u;
    pal_bitmap_payload[14] = 0u;
    pal_bitmap_payload[15] = 255u;
    pal_bitmap_payload[16] = 0u;
    pal_bitmap_payload[17] = 0u;
    pal_bitmap_payload[18] = 0u;
    pal_bitmap_payload[19] = 1u;
    pal_bitmap_payload[20] = 0x0Fu;
    CHECK(g3d_bitmap_decode_mip_rgba32_arena(pal_bitmap_payload, 21u, 0u, 0u,
                                             rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                             &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_w == 2u && rgba_h == 1u && rgba_bytes == 8u);
    ASSERT(rgba_buf[0] == 255u && rgba_buf[1] == 0u && rgba_buf[2] == 0u && rgba_buf[3] == 255u);
    ASSERT(rgba_buf[4] == 0u && rgba_buf[5] == 255u && rgba_buf[6] == 0u && rgba_buf[7] == 255u);

    /* Regression: KIB sub-lib wraps a compressed GeBm mip and reports the
       stored byte count, while public unpack/decode gives the raw mip back. */
    raw_mip_buf[0] = 9u;
    raw_mip_buf[1] = 9u;
    raw_mip_buf[2] = 9u;
    raw_mip_buf[3] = 10u;
    CHECK(g3d_kib_packbits_encode(raw_mip_buf, 4u, kib_payload, (g3d_u32)sizeof(kib_payload), &kib_payload_used));
    CHECK(g3d_kib_wrap_container(G3D_KIB_METHOD_PACKBITS, kib_payload, kib_payload_used,
                                 4u, kib_wrapped, (g3d_u32)sizeof(kib_wrapped), &kib_used));
    comp_bitmap_payload[0] = 'G';
    comp_bitmap_payload[1] = 'e';
    comp_bitmap_payload[2] = 'B';
    comp_bitmap_payload[3] = 'm';
    comp_bitmap_payload[4] = 0x40u;
    comp_bitmap_payload[5] = 0x01u;
    comp_bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_8BIT_GRAY;
    comp_bitmap_payload[7] = 0x01u;
    comp_bitmap_payload[8] = 0x11u;
    comp_bitmap_payload[9] = 0x10u;
    memcpy(comp_bitmap_payload + 10u, kib_wrapped, kib_used);
    comp_bitmap_payload[10u + kib_used] = 0x0Fu;
    CHECK(g3d_bitmap_probe(comp_bitmap_payload, 11u + kib_used, &bitmap_info));
    ASSERT((bitmap_info.flags & G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS) != 0u);
    ASSERT(bitmap_info.mips[0].stored_size == kib_used);
    ASSERT(bitmap_info.mips[0].codec_method == G3D_KIB_METHOD_PACKBITS);
    CHECK(g3d_bitmap_unpack_mip_arena(comp_bitmap_payload, 11u + kib_used, 0u,
                                      raw_mip_buf, (g3d_u32)sizeof(raw_mip_buf),
                                      &rgba_w, &rgba_h, &raw_bytes, &kib_method, &arena));
    ASSERT(rgba_w == 2u && rgba_h == 2u && raw_bytes == 4u);
    ASSERT(kib_method == G3D_KIB_METHOD_PACKBITS);
    ASSERT(raw_mip_buf[0] == 9u && raw_mip_buf[1] == 9u && raw_mip_buf[2] == 9u && raw_mip_buf[3] == 10u);
    CHECK(g3d_bitmap_decode_mip_rgba32_arena(comp_bitmap_payload, 11u + kib_used, 0u, 0u,
                                             rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                             &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_buf[0] == 9u && rgba_buf[12] == 10u);

    /* Regression: Genesis3D's Paeth mip flag is now actually reversed. */
    raw_mip_buf[0] = 10u;
    raw_mip_buf[1] = 20u;
    raw_mip_buf[2] = 30u;
    raw_mip_buf[3] = 40u;
    memcpy(kib_payload, raw_mip_buf, 4u);
    CHECK(g3d_kib_paeth_filter(kib_payload, 2u, 2u, 1u));
    paeth_bitmap_payload[0] = 'G';
    paeth_bitmap_payload[1] = 'e';
    paeth_bitmap_payload[2] = 'B';
    paeth_bitmap_payload[3] = 'm';
    paeth_bitmap_payload[4] = 0x40u;
    paeth_bitmap_payload[5] = 0x01u;
    paeth_bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_8BIT_GRAY;
    paeth_bitmap_payload[7] = 0x01u;
    paeth_bitmap_payload[8] = 0x11u;
    paeth_bitmap_payload[9] = 0x20u;
    memcpy(paeth_bitmap_payload + 10u, kib_payload, 4u);
    paeth_bitmap_payload[14] = 0x0Fu;
    CHECK(g3d_bitmap_probe(paeth_bitmap_payload, 15u, &bitmap_info));
    ASSERT((bitmap_info.flags & G3D_BITMAP_INFO_HAS_PAETH_MIPS) != 0u);
    CHECK(g3d_bitmap_unpack_mip_arena(paeth_bitmap_payload, 15u, 0u,
                                      raw_mip_buf, (g3d_u32)sizeof(raw_mip_buf),
                                      &rgba_w, &rgba_h, &raw_bytes, &kib_method, &arena));
    ASSERT(raw_mip_buf[0] == 10u && raw_mip_buf[1] == 20u && raw_mip_buf[2] == 30u && raw_mip_buf[3] == 40u);

    /* Regression: compressed palette flag can be skipped/probed and decoded. */
    kib_payload[0] = 255u;
    kib_payload[1] = 0u;
    kib_payload[2] = 0u;
    kib_payload[3] = 0u;
    kib_payload[4] = 255u;
    kib_payload[5] = 0u;
    CHECK(g3d_kib_wrap_container(G3D_KIB_METHOD_STORE, kib_payload, 6u,
                                 6u, kib_wrapped, (g3d_u32)sizeof(kib_wrapped), &kib_used));
    comp_pal_bitmap_payload[0] = 'G';
    comp_pal_bitmap_payload[1] = 'e';
    comp_pal_bitmap_payload[2] = 'B';
    comp_pal_bitmap_payload[3] = 'm';
    comp_pal_bitmap_payload[4] = 0x40u;
    comp_pal_bitmap_payload[5] = 0x09u;
    comp_pal_bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_8BIT;
    comp_pal_bitmap_payload[7] = 0x01u;
    comp_pal_bitmap_payload[8] = 0x10u;
    comp_pal_bitmap_payload[9] = (unsigned char)(G3D_PIXELFORMAT_24BIT_RGB | 0x40u);
    comp_pal_bitmap_payload[10] = 2u;
    memcpy(comp_pal_bitmap_payload + 11u, kib_wrapped, kib_used);
    comp_pal_bitmap_payload[11u + kib_used] = 0u;
    comp_pal_bitmap_payload[12u + kib_used] = 0u;
    comp_pal_bitmap_payload[13u + kib_used] = 1u;
    comp_pal_bitmap_payload[14u + kib_used] = 0x0Fu;
    CHECK(g3d_bitmap_probe(comp_pal_bitmap_payload, 15u + kib_used, &bitmap_info));
    ASSERT((bitmap_info.flags & G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE) != 0u);
    ASSERT(bitmap_info.palette_stored_size == kib_used);
    CHECK(g3d_bitmap_decode_mip_rgba32_arena(comp_pal_bitmap_payload, 15u + kib_used, 0u, 0u,
                                             rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                             &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_buf[0] == 255u && rgba_buf[1] == 0u && rgba_buf[2] == 0u);
    ASSERT(rgba_buf[4] == 0u && rgba_buf[5] == 255u && rgba_buf[6] == 0u);

    /* Regression: experimental KIB wavelet8 payloads decode to gray pixels. */
    wavelet_payload[0] = 2u; wavelet_payload[1] = 0u;
    wavelet_payload[2] = 2u; wavelet_payload[3] = 0u;
    wavelet_payload[4] = 0u; wavelet_payload[5] = 0u;
    wavelet_payload[6] = 11u; wavelet_payload[7] = 0u;
    wavelet_payload[8] = 22u; wavelet_payload[9] = 0u;
    wavelet_payload[10] = 33u; wavelet_payload[11] = 0u;
    wavelet_payload[12] = 44u; wavelet_payload[13] = 0u;
    CHECK(g3d_kib_wavelet8_decode_arena(wavelet_payload, 14u, wavelet_pixels, 4u, &rgba_w, &rgba_h, &kib_method, &arena));
    ASSERT(rgba_w == 2u && rgba_h == 2u && kib_method == 0u);
    ASSERT(wavelet_pixels[0] == 11u && wavelet_pixels[3] == 44u);
    CHECK(g3d_kib_wrap_container(G3D_KIB_METHOD_WAVELET8, wavelet_payload, 14u,
                                 4u, kib_wrapped, (g3d_u32)sizeof(kib_wrapped), &kib_used));
    comp_bitmap_payload[6] = (unsigned char)G3D_PIXELFORMAT_WAVELET;
    comp_bitmap_payload[9] = 0x10u;
    memcpy(comp_bitmap_payload + 10u, kib_wrapped, kib_used);
    comp_bitmap_payload[10u + kib_used] = 0x0Fu;
    CHECK(g3d_bitmap_decode_mip_rgba32_arena(comp_bitmap_payload, 11u + kib_used, 0u, 0u,
                                             rgba_buf, (g3d_u32)sizeof(rgba_buf),
                                             &rgba_w, &rgba_h, &rgba_bytes, &arena));
    ASSERT(rgba_w == 2u && rgba_h == 2u && rgba_buf[0] == 11u && rgba_buf[12] == 44u);

    CHECK(g3d_body_validate_bitmaps(&parsed_body, &bad_mat_index));
    ASSERT(bad_mat_index == -1);
    CHECK(g3d_body_find_material_index(&parsed_body, "Default", &material_index));
    ASSERT(material_index == 0);
    CHECK(g3d_body_find_material_index_prefix(&parsed_body, "Def", &material_index));
    ASSERT(material_index == 0);
    ASSERT(g3d_body_find_material_index(&parsed_body, "missing", &material_index) == G3D_ERR_NOT_FOUND);
    ASSERT(strcmp(g3d_body_get_material_name(&parsed_body, 0), "Default") == 0);
    ASSERT(g3d_body_count_textured_materials(&parsed_body) == 1u);
    CHECK(g3d_body_find_bone_index(&parsed_body, "Root", &bone_index));
    ASSERT(bone_index == 0);
    ASSERT(g3d_body_find_bone_index(&parsed_body, "Missing", &bone_index) == G3D_ERR_NOT_FOUND);
    ASSERT(strcmp(g3d_body_get_bone_name(&parsed_body, 0), "Root") == 0);
    ASSERT(g3d_body_bone_name_checksum(&parsed_body) == 1260);
    CHECK(g3d_body_count_vertices_for_bone(&parsed_body, 0, &bone_vertex_count));
    ASSERT(bone_vertex_count == 3u);
    CHECK(g3d_body_compute_bone_bbox_fix16(&parsed_body, 0, &computed_bbox_min, &computed_bbox_max));
    ASSERT(computed_bbox_min.x == 0);
    ASSERT(computed_bbox_min.y == 0);
    ASSERT(computed_bbox_min.z == 0);
    ASSERT(computed_bbox_max.x == 0x00010000L);
    ASSERT(computed_bbox_max.y == 0x00010000L);
    ASSERT(computed_bbox_max.z == 0);
    ASSERT(g3d_body_lod_face_count(&parsed_body, 0u) == 1u);
    ASSERT(g3d_body_lod_face_count(&parsed_body, 1u) == 0u);
    CHECK(g3d_body_compute_bbox_fix16(&parsed_body, &computed_bbox_min, &computed_bbox_max));
    ASSERT(computed_bbox_min.x == 0);
    ASSERT(computed_bbox_min.y == 0);
    ASSERT(computed_bbox_min.z == 0);
    ASSERT(computed_bbox_max.x == 0x00010000L);
    ASSERT(computed_bbox_max.y == 0x00010000L);
    ASSERT(computed_bbox_max.z == 0);

    /* Regression: Genesis3D CF00 VFS stores normal DirTree metadata but XORs
       child payload bytes with a one-byte mask derived from an 8-byte context. */
    CHECK(g3d_body_write_file_with_mask(&body, masked_body_buf,
                                         (g3d_u32)sizeof(masked_body_buf),
                                         &masked_body_used, cf_mask));
    ASSERT(masked_body_used == body_used);
    ASSERT(rd32(masked_body_buf) == 0x30304643u);
    g3d_arena_reset(&arena);
    ASSERT(g3d_body_parse_file(&masked_body, masked_body_buf, masked_body_used, &arena) == G3D_ERR_UNSUPPORTED);
    g3d_arena_reset(&arena);
    CHECK(g3d_body_parse_file_with_mask(&masked_body, masked_body_buf, masked_body_used, &arena, cf_mask));
    CHECK(g3d_body_validate(&masked_body));
    ASSERT(strcmp(g3d_strblock_get(masked_body.bone_names, 0), "Root") == 0);
    ASSERT(masked_body.materials[0].bitmap_size == 15u);
    ASSERT(masked_body.materials[0].bitmap_data[0] == 'G');
    CHECK(g3d_body_get_material_bitmap_info(&masked_body, 0, &bitmap_info));

    actor.has_body = 1u;
    actor.motion_count = 2;
    actor.body = &body;
    actor.motions = motions_arr;

    CHECK(g3d_actor_validate(&actor));
    CHECK(g3d_actor_write_file(&actor, file_buf, (g3d_u32)sizeof(file_buf), &used));
    ASSERT(used > 0u);

    g3d_arena_reset(&arena);
    CHECK(g3d_vfs_parse(&vfs_check, file_buf, used, &arena));
    ASSERT(vfs_check.root != 0);
    ASSERT(vfs_check.root->name != 0);
    ASSERT(vfs_check.root->name[0] == '\0');
    ASSERT(g3d_vfs_find(&vfs_check, "Header") != 0);
    ASSERT(g3d_vfs_find(&vfs_check, "header") != 0);
    ASSERT(g3d_vfs_find(&vfs_check, "Motions") != 0);
    ASSERT(g3d_vfs_find(&vfs_check, "MOTIONS\\0") != 0);
    ASSERT(g3d_vfs_find(&vfs_check, "motions/1") != 0);
    ASSERT(g3d_vfs_count_files(&vfs_check) == 4u);
    ASSERT(g3d_vfs_count_entries(&vfs_check, 1) == 6u);

    g3d_arena_reset(&arena);
    CHECK(g3d_actor_parse_file(&parsed_actor, file_buf, used, &arena));
    CHECK(g3d_actor_validate(&parsed_actor));

    ASSERT(parsed_actor.has_body == 1u);
    ASSERT(parsed_actor.motion_count == 2);
    ASSERT(parsed_actor.body != 0);
    ASSERT(parsed_actor.body->bone_count == 1);
    ASSERT(parsed_actor.body->material_count == 1);
    ASSERT(parsed_actor.body->xskin_vertex_count == 3);
    ASSERT(parsed_actor.body->lod_count == 1u);
    ASSERT(parsed_actor.motions != 0);
    ASSERT(strcmp(g3d_strblock_get(parsed_actor.body->bone_names, 0), "Root") == 0);
    ASSERT(strcmp(g3d_strblock_get(parsed_actor.motions[0].leaf.name_array, 0), "Root") == 0);
    ASSERT(strcmp(g3d_strblock_get(parsed_actor.motions[1].leaf.name_array, 0), "Root") == 0);
    CHECK(g3d_actor_find_motion_index(&parsed_actor, "walk", &motion_index));
    ASSERT(motion_index == 0);
    CHECK(g3d_actor_find_motion_index(&parsed_actor, "strafe", &motion_index));
    ASSERT(motion_index == 1);
    ASSERT(g3d_actor_find_motion_index(&parsed_actor, "missing", &motion_index) == G3D_ERR_NOT_FOUND);
    ASSERT(strcmp(g3d_actor_get_motion_name(&parsed_actor, 1), "strafe") == 0);

    CHECK(g3d_actor_write_file_with_mask(&actor, masked_file_buf,
                                          (g3d_u32)sizeof(masked_file_buf),
                                          &masked_actor_used, cf_mask));
    ASSERT(masked_actor_used == used);
    ASSERT(rd32(masked_file_buf) == 0x30304643u);
    g3d_arena_reset(&arena);
    ASSERT(g3d_actor_parse_file(&masked_actor, masked_file_buf, masked_actor_used, &arena) == G3D_ERR_UNSUPPORTED);
    g3d_arena_reset(&arena);
    CHECK(g3d_vfs_parse_with_mask(&vfs_check, masked_file_buf, masked_actor_used, &arena, cf_mask));
    ASSERT(vfs_check.encrypted == 1u);
    ASSERT(vfs_check.xor_mask == cf_mask);
    ASSERT(g3d_vfs_find(&vfs_check, "HEADER") != 0);
    g3d_arena_reset(&arena);
    CHECK(g3d_actor_parse_file_with_mask(&masked_actor, masked_file_buf, masked_actor_used, &arena, cf_mask));
    CHECK(g3d_actor_validate(&masked_actor));
    ASSERT(masked_actor.motion_count == 2);
    ASSERT(strcmp(g3d_strblock_get(masked_actor.body->bone_names, 0), "Root") == 0);
    ASSERT(masked_actor.body->materials[0].bitmap_size == 15u);
    ASSERT(masked_actor.body->materials[0].bitmap_data[1] == 'e');
    CHECK(g3d_body_get_material_bitmap_info(masked_actor.body, 0, &bitmap_info));

    CHECK(g3d_body_alloc_skinned_mesh(&arena, masked_actor.body, &mesh));
    CHECK(g3d_actor_sample_skinned_fix16(&masked_actor, 0, 0x00008000L, &mesh));
    ASSERT(mesh.vertex_count == 3u);
    ASSERT(mesh.vertices[0].x > 0x0004F000L && mesh.vertices[0].x < 0x00051000L);
    ASSERT(mesh.vertices[0].y == 0);
    ASSERT(mesh.vertices[1].x > 0x0005F000L && mesh.vertices[1].x < 0x00061000L);
    ASSERT(mesh.vertices[2].x > 0x0004F000L && mesh.vertices[2].x < 0x00051000L);
    ASSERT(mesh.vertices[2].y > 0x0000F000L && mesh.vertices[2].y < 0x00011000L);
    ASSERT(mesh.normals[0].z > 0x0000F000L && mesh.normals[0].z <= 0x00010000L);
    ASSERT(mesh.bone_matrices[0].m[3] > 0x0004F000L && mesh.bone_matrices[0].m[3] < 0x00051000L);

    CHECK(g3d_actor_sample_skinned_blend_fix16(&masked_actor, 0, 0x00008000L,
                                               1, 0x00008000L,
                                               16384, G3D_POSE_BLEND_LINEAR,
                                               &mesh));
    ASSERT(mesh.vertices[0].x > 0x00027000L && mesh.vertices[0].x < 0x00029000L);
    ASSERT(mesh.vertices[0].y > 0x0001F000L && mesh.vertices[0].y < 0x00021000L);

    g3d_asset_manager_init(&mgr, slots, 4u);
    CHECK(g3d_asset_manager_add(&mgr, &arena, "player.act", G3D_ASSET_ACTOR, &masked_actor));
    ASSERT(g3d_asset_manager_count_type(&mgr, G3D_ASSET_ACTOR) == 1u);
    ASSERT(g3d_asset_manager_find(&mgr, "player.act", G3D_ASSET_ACTOR) == &masked_actor);
    ASSERT(g3d_asset_manager_add(&mgr, &arena, "player.act", G3D_ASSET_ACTOR, &masked_actor) == G3D_ERR_CORRUPT);
    CHECK(g3d_asset_manager_upsert(&mgr, &arena, "player.act", G3D_ASSET_ACTOR, &actor));
    ASSERT(g3d_asset_manager_find(&mgr, "player.act", G3D_ASSET_ACTOR) == &actor);

    printf("OK actor_bytes=%u body_bytes=%u motion_bytes=%u/%u motions=%d vertices=%u\n",
           (unsigned)used, (unsigned)body_used,
           (unsigned)motion_used, (unsigned)motion_used2,
           (int)parsed_actor.motion_count,
           (unsigned)mesh.vertex_count);
    return 0;
}
