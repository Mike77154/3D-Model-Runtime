#include "g3d_codec.h"
#include <stdio.h>
#include <string.h>

static void wr32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
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

static int write_file(const char *path, const unsigned char *buf, g3d_u32 size)
{
    FILE *f;
    f = fopen(path, "wb");
    if (f == NULL)
    {
        return 0;
    }
    if (fwrite(buf, 1u, (size_t)size, f) != (size_t)size)
    {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned char bone_sb_buf[16];
    unsigned char mat_sb_buf[20];
    unsigned char motion_name_sb_buf[16];
    unsigned char bitmap_payload[15];
    unsigned char act_buf[1 << 16];
    unsigned char body_buf[1 << 12];
    unsigned char mot_buf[2048];
    unsigned char mot_buf2[2048];
    g3d_body body;
    g3d_body_bone bone;
    g3d_body_material material;
    g3d_body_xskin_vertex vertices[3];
    g3d_body_normal normals[3];
    g3d_body_triangle face;
    g3d_strblock bone_sb;
    g3d_strblock mat_sb;
    g3d_strblock mot_name_sb;
    g3d_motion motion;
    g3d_motion motion2;
    g3d_motion motions_arr[2];
    g3d_path path;
    g3d_path path2;
    g3d_vkframe vk;
    g3d_vkframe vk2;
    g3d_qkframe qk;
    g3d_qkframe qk2;
    g3d_u32 vk_times[2];
    g3d_fvec3 vk_vecs[2];
    g3d_u32 vk2_times[2];
    g3d_fvec3 vk2_vecs[2];
    g3d_u32 qk_times[2];
    g3d_fquat qk_quats[2];
    g3d_actor actor;
    g3d_u32 used_act;
    g3d_u32 used_body;
    g3d_u32 used_mot;
    g3d_u32 used_mot2;
    char act_path[512];
    char body_path[512];
    char mot_path[512];
    char mot2_path[512];
    const char *outdir;

    if (argc != 2)
    {
        printf("usage: mk_sample <outdir>\n");
        return 1;
    }
    outdir = argv[1];

    memset(&body, 0, sizeof(body));
    memset(&bone, 0, sizeof(bone));
    memset(&material, 0, sizeof(material));
    memset(vertices, 0, sizeof(vertices));
    memset(normals, 0, sizeof(normals));
    memset(&face, 0, sizeof(face));
    memset(&motion, 0, sizeof(motion));
    memset(&motion2, 0, sizeof(motion2));
    memset(&path, 0, sizeof(path));
    memset(&path2, 0, sizeof(path2));
    memset(&vk, 0, sizeof(vk));
    memset(&vk2, 0, sizeof(vk2));
    memset(&qk, 0, sizeof(qk));
    memset(&qk2, 0, sizeof(qk2));
    memset(&actor, 0, sizeof(actor));

    make_one_string_block(&bone_sb, bone_sb_buf, "Root");
    make_one_string_block(&mat_sb, mat_sb_buf, "Default");
    make_one_string_block(&mot_name_sb, motion_name_sb_buf, "Root");

    bone.parent_bone_index = -1;
    bone.attachment.f[0] = 0x3f800000u;
    bone.attachment.f[4] = 0x3f800000u;
    bone.attachment.f[8] = 0x3f800000u;

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

    vertices[0].lod_mask = 1u;
    vertices[0].bone_index = 0;
    vertices[1] = vertices[0];
    vertices[2] = vertices[0];
    vertices[1].xpoint.x_bits = 0x3f800000u;
    vertices[1].u_bits = 0x3f800000u;
    vertices[2].xpoint.y_bits = 0x3f800000u;
    vertices[2].v_bits = 0x3f800000u;

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

    actor.has_body = 1u;
    actor.motion_count = 2;
    actor.body = &body;
    actor.motions = motions_arr;

    if (g3d_body_write_file(&body, body_buf, (g3d_u32)sizeof(body_buf), &used_body) != G3D_OK)
    {
        return 2;
    }
    if (g3d_motion_write_binary(&motion, mot_buf, (g3d_u32)sizeof(mot_buf), &used_mot) != G3D_OK)
    {
        return 3;
    }
    if (g3d_motion_write_binary(&motion2, mot_buf2, (g3d_u32)sizeof(mot_buf2), &used_mot2) != G3D_OK)
    {
        return 3;
    }
    if (g3d_actor_write_file(&actor, act_buf, (g3d_u32)sizeof(act_buf), &used_act) != G3D_OK)
    {
        return 4;
    }

    sprintf(act_path, "%s/sample.act", outdir);
    sprintf(body_path, "%s/sample_body.bdy", outdir);
    sprintf(mot_path, "%s/sample_walk.mot", outdir);
    sprintf(mot2_path, "%s/sample_strafe.mot", outdir);

    if (!write_file(act_path, act_buf, used_act)) return 5;
    if (!write_file(body_path, body_buf, used_body)) return 6;
    if (!write_file(mot_path, mot_buf, used_mot)) return 7;
    if (!write_file(mot2_path, mot_buf2, used_mot2)) return 8;

    printf("wrote %s\n", act_path);
    printf("wrote %s\n", body_path);
    printf("wrote %s\n", mot_path);
    printf("wrote %s\n", mot2_path);
    return 0;
}
