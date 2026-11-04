#include "3dfbx.h"

#include <stdio.h>
#include <string.h>

static int count_models_of_type(const FbxScene *scene, FbxModelType type)
{
    const FbxModel *m;
    int count;
    count = 0;
    if (!scene) return 0;
    for (m = scene->models; m; m = m->next) {
        if (m->type == type) count += 1;
    }
    return count;
}

int main(void)
{
    static unsigned char out[262144];
    fbx_real pos[9];
    fbx_u32 idx[3];
    FbxSemanticMesh mesh;
    FbxSemanticCamera camera;
    FbxSemanticLight light;
    FbxSemanticBone bones[2];
    FbxSemanticSkinCluster clusters[2];
    fbx_u32 c0_idx[2];
    fbx_u32 c1_idx[2];
    fbx_real c0_w[2];
    fbx_real c1_w[2];
    FbxSemanticAnimKey keys[2];
    FbxSemanticAnimCurve curve;
    FbxSemanticAnimStack stack;
    FbxSemanticScene sem_scene;
    FbxSemanticWriteOptions opt;
    FbxWriteBuffer wb;
    FbxCodecDecodeResult decoded;
    int cameras;
    int lights;
    int ok;

    pos[0] = 0; pos[1] = 0; pos[2] = 0;
    pos[3] = FBX_REAL_ONE; pos[4] = 0; pos[5] = 0;
    pos[6] = 0; pos[7] = FBX_REAL_ONE; pos[8] = 0;
    idx[0] = 0u; idx[1] = 1u; idx[2] = 2u;

    memset(&mesh, 0, sizeof(mesh));
    mesh.name = "RiggedTriangle";
    mesh.positions = pos;
    mesh.vertex_count = 3u;
    mesh.indices = idx;
    mesh.index_count = 3u;
    mesh.material_name = "RigMat";

    memset(&camera, 0, sizeof(camera));
    camera.name = "MainCamera";
    camera.field_of_view = fbx_real_from_int(45);
    camera.near_plane = fbx_real_from_ratio(1, 10);
    camera.far_plane = fbx_real_from_int(1000);

    memset(&light, 0, sizeof(light));
    light.name = "KeyLight";
    light.light_type = 0;
    light.color[0] = FBX_REAL_ONE;
    light.color[1] = FBX_REAL_ONE;
    light.color[2] = FBX_REAL_ONE;
    light.intensity = fbx_real_from_int(100);
    light.cone_angle = fbx_real_from_int(45);

    memset(bones, 0, sizeof(bones));
    bones[0].name = "Root";
    bones[0].parent_index = -1;
    bones[0].scale[0] = FBX_REAL_ONE; bones[0].scale[1] = FBX_REAL_ONE; bones[0].scale[2] = FBX_REAL_ONE;
    bones[1].name = "Child";
    bones[1].parent_index = 0;
    bones[1].translation[1] = FBX_REAL_ONE;
    bones[1].scale[0] = FBX_REAL_ONE; bones[1].scale[1] = FBX_REAL_ONE; bones[1].scale[2] = FBX_REAL_ONE;

    c0_idx[0] = 0u; c0_idx[1] = 1u;
    c1_idx[0] = 1u; c1_idx[1] = 2u;
    c0_w[0] = FBX_REAL_ONE; c0_w[1] = fbx_real_from_ratio(1, 2);
    c1_w[0] = fbx_real_from_ratio(1, 2); c1_w[1] = FBX_REAL_ONE;
    memset(clusters, 0, sizeof(clusters));
    clusters[0].mesh_index = 0;
    clusters[0].bone_index = 0;
    clusters[0].indices = c0_idx;
    clusters[0].weights = c0_w;
    clusters[0].count = 2u;
    clusters[1].mesh_index = 0;
    clusters[1].bone_index = 1;
    clusters[1].indices = c1_idx;
    clusters[1].weights = c1_w;
    clusters[1].count = 2u;

    keys[0].time_ticks = 0;
    keys[0].value = FBX_REAL_ZERO;
    keys[0].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;
    keys[1].time_ticks = (fbx_i64)46186158000;
    keys[1].value = fbx_real_from_int(10);
    keys[1].interpolation = (unsigned char)FBX_ANIM_INTERP_LINEAR;
    memset(&curve, 0, sizeof(curve));
    curve.target_kind = 0;
    curve.target_index = 1000;
    curve.channel = FBX_ANIM_CHANNEL_T_X;
    curve.keys = keys;
    curve.key_count = 2;
    memset(&stack, 0, sizeof(stack));
    stack.name = "Take 001";
    stack.curves = &curve;
    stack.curve_count = 1;

    memset(&sem_scene, 0, sizeof(sem_scene));
    sem_scene.creator = "semantic extended smoke";
    sem_scene.meshes = &mesh;
    sem_scene.mesh_count = 1;
    sem_scene.cameras = &camera;
    sem_scene.camera_count = 1;
    sem_scene.lights = &light;
    sem_scene.light_count = 1;
    sem_scene.bones = bones;
    sem_scene.bone_count = 2;
    sem_scene.skin_clusters = clusters;
    sem_scene.skin_cluster_count = 2;
    sem_scene.anim_stacks = &stack;
    sem_scene.anim_stack_count = 1;

    fbx_semantic_write_options_default(&opt);
    opt.version = 7400;
    opt.binary_compress_arrays = 1;
    fbx_write_buffer_init(&wb, out, sizeof(out));
    if (fbx_semantic_write_binary(&sem_scene, &wb, &opt) != FBX_OK) return 1;

    fbx_codec_decode_result_init(&decoded);
    ok = fbx_codec_decode_memory(out, (fbx_i64)wb.size, 1, &decoded);
    if (!ok || !decoded.scene) return 2;
    cameras = count_models_of_type(decoded.scene, FBX_MODEL_TYPE_CAMERA);
    lights = count_models_of_type(decoded.scene, FBX_MODEL_TYPE_LIGHT);
    if (decoded.scene->model_count < 4) return 3;
    if (cameras != 1 || lights != 1) return 4;
    if (!decoded.scene->skeleton || decoded.scene->skeleton->bone_count != 2) return 5;
    if (decoded.scene->anim_clip_count != 1) return 6;
    if (decoded.scene->anim_clips[0]->track_count < 1) return 7;
    printf("semantic extended OK: bytes=%lu cameras=%d lights=%d bones=%d anims=%d\n",
           (unsigned long)wb.size, cameras, lights,
           decoded.scene->skeleton ? decoded.scene->skeleton->bone_count : 0,
           decoded.scene->anim_clip_count);
    fbx_codec_decode_close(&decoded);
    return 0;
}
